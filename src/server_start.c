/**
 * @file server_start.c
 * @brief Implementazione della funzione server_start
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
 /*macro per warning di funzioni */
#define _BSD_SOURCE
#define _POSIX_C_SOURCE 200112L
#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<signal.h>
#include<sys/select.h>
#include<sys/time.h>
#include"server.h"
#include"queue.h"

#define DEBUG

/*
 *    Variabile per propagare l'errore con cui fallisce un thread del pool del server
 *    @note non ha niente a che fare con la variabile del thread_error del threadpool,
 *    anche se hanno lo stesso ruolo
 */
static int thread_error = 0;

/**
 * @enum op_queue_descriptor_t
 * @brief operazione da fare sul descrittore
 */
typedef enum{
    SET,
    REMOVE,
}op_queue_descriptor_t;

/**
 * @struct queue_descr_elem_t
 * @brief elemento della coda dei descrittori da aggiornare
 * @var fd descrittore
 * @var op operazione da effetuare sul descrittore
 */
 typedef struct{
    int fd;
    op_queue_descriptor_t op;
}queue_descr_elem_t;

/**
 * @struct descriptor_queue_t
 * @brief struttura della coda dei descrittori da aggiornare
 * @var queue coda
 * @var mtx mutex per la sincronizzazione
 */
typedef struct{
    queue_t *queue;
    pthread_mutex_t mtx;
}descriptor_queue_t;

/* Variabili globali utili*/
static server_t *server;

/* Coda dei descrittori da aggiornare: Viene utilizzata per aggiornare i descrittori
   del set utilizzati dalla select */
static descriptor_queue_t descriptors;

/* Funzioni globali */
static int (*client_manager_fun)(void *,int,void*);
static void *arg_cmf;

static int (*signal_usr_handler)(void*);//argomenti
static void *arg_suh;

static int (*client_out_of_bound)(int,void*);
static void *arg_cob;

static int (*disconnect_client)(int,void*);
static void *arg_dc;

static int (*read_message_fun)(int,void*);

/*Flag segnali */
static volatile sig_atomic_t run = 0; //stato del server
static volatile sig_atomic_t updateSet = 0; //per segnalare di aggiornare il set dei descrittori
static volatile sig_atomic_t sigusr = 0; //per segnalare di eseguire il segnale sigusr1

/**
 * @struct worker_args_t
 * @brief Struttura che contiene gli argomenti per un thread_worker
 * @var fd_client il descrittore del client con cui deve interagire
 * @var arg_used per vedere se l'argomento e' stato finito di utilizzare
 * @var mtx mutex per sincronizzazione
 */
typedef struct{
    int fd_client;
    unsigned short int arg_used; //0 non utilizzato, 1 fine utilizzato
    pthread_mutex_t mtx;
}worker_args_t;

/**
 * @function prepare_arg
 * @param args insieme degli argomenti per i thread del pool
 * @param fd_client fd da passare al thread del pool
 * @return puntatore all'elemento per il thread del pool,altrimenti NULL e setta errno
 */
static worker_args_t* prepare_arg(worker_args_t *args,int fd_client)
{
    int err;
    int i;
    int isInit = 0; //per vedere se la posizione e' inizializzata

    //cerco una posizione libera per l'argomento all'interno dell'array degli argomenti
    for (i = 0; i < server->max_connection; i++)
    {
        //se non e' ancora inizializzata,si puo' utilizzare senza fare la lock
        if(args[i].fd_client == 0)
            break;

        //altrimenti devo fare la lock
        err = pthread_mutex_lock(&args[i].mtx);

        if(err)
        {
            errno = err;
            return NULL;
        }

        //posizione attualmente non utilizzata
        if(args[i].arg_used == 0)
        {
            isInit = 1;
            pthread_mutex_unlock(&args[i].mtx);
            break;
        }

        pthread_mutex_unlock(&args[i].mtx);

    }

    //overflow delgli argomenti
    if(i == server->max_connection)
    {
        errno = EAGAIN;
        return NULL;
    }

    //posizione non ancora inizializzata,devo inizializzarla
    if(!isInit)
    {
        //inizializzo mutex per la posizione
        err = pthread_mutex_init(&args[i].mtx,NULL);

        //errore nell'inizializzazione del mutex
        if(err)
        {
            errno = err;
            return NULL;
        }
    }

    //inizializzo altri parametri
    args[i].fd_client = fd_client;
    args[i].arg_used = 1;

    //ritorno la posizione,sottoforma di puntatore
    return (args+i);
}

/**
 * @function close_all_client
 * @brief chiude tutti i client attualmente connessi al server
 * @param max_sd numero di descrittori controllati
 * @param set insieme dei descrittori controllati
 */
static void close_all_client(int max_sd,fd_set *set)
{
    for (int i=0; i <= max_sd; ++i)
    {
        if (FD_ISSET(i,set))
            close(i);
    }
}

/**
 * @function remove_client
 * @brief RImuove un client dal server
 * @param fd descrittore da rimuovere
 * @param active_set set dei descrittori controllati
 * @param actual_client numero di client connessi
 * @max_fd indice massimo di descrittori controllati
 */
static void remove_client(int fd,fd_set *active_set,int *actual_client,int *max_fd)
{
    #ifdef DEBUG
        printf("Rimozione client %d\n",fd);
    #endif

    //decremento numero attuale di client connessi
    --*actual_client;

    //aggiorno l'indice
    if(fd == *max_fd)
    {
        //aggiorno l'indice massimo
        while(FD_ISSET(*max_fd,active_set) == 0)
            *max_fd -= 1;
    }

    //chiudo il descrittore
    close(fd);
}

/* Funzione per i segnali */
static inline void termination()
{
    #ifdef DEBUG
        printf("arrivata terminazione\n");
    #endif
    run = 0;
}

/**
 * @function sigusr1
 * @brief Handler del segnale SIGUSR1
 */
static inline void sigusr1()
{
    #ifdef DEBUG
        printf("Arrivato sigusr\n");
    #endif
    sigusr = 1;
}

/**
 * @function sigdescript
 * @brief Handler del segnale SIGUSR2
 */
static inline void sigdescript()
{
    #ifdef DEBUG
        printf("Arrivato SIGUSR2\n");
    #endif
    updateSet = 1;
}

/**
 * @function set_listener_signal_handler
 * @brief Gestione dei segnali per il listener
 * @return on success 0,altrimenti -1 e setta errno
 */
static int set_listener_signal_handler()
{
    int err = 0;
    struct sigaction sa;
    sigset_t signal_mask;

    //metto una maschera ad i segnali interessati,per installare i gestori
    err = sigemptyset(&signal_mask);
    err = sigaddset(&signal_mask,SIGPIPE);
    err = sigaddset(&signal_mask,SIGQUIT);
    err = sigaddset(&signal_mask,SIGTERM);
    err = sigaddset(&signal_mask,SIGINT);
    err = sigaddset(&signal_mask,SIGUSR2);
    err = sigaddset(&signal_mask,SIGUSR1);

    //errore nel set dei segnali
    if(err == -1)
        return -1;

    err = pthread_sigmask(SIG_SETMASK,&signal_mask,NULL);

    //errore nel set della maschera
    if(err)
    {
        errno = err;
        return -1;
    }

    //inizializzo la struttura per gestire i segnali
    memset(&sa,0,sizeof(sa));

    sa.sa_flags = 0; //non voglio il restart del SC

    //ingnoriamo SIGPIPE
    sa.sa_handler = SIG_IGN;
    err = sigaction(SIGPIPE,&sa,NULL);

    //terminazione
    sa.sa_handler = termination;
    err = sigaction(SIGTERM,&sa,NULL);
    err = sigaction(SIGQUIT,&sa,NULL);
    err = sigaction(SIGINT,&sa,NULL);

    //arrivo di SIGUSR1
    sa.sa_handler = sigusr1;
    err = sigaction(SIGUSR1,&sa,NULL);

    //arrivo di SIGUSR2
    sa.sa_handler = sigdescript;
    err = sigaction(SIGUSR2,&sa,NULL);

    //errore nei sigaction
    if(err == -1)
    {
        return -1;
    }

    //rimetto la maschera predefinita
    err = sigemptyset(&signal_mask);

    //errore emptyset
    if(err == -1)
        return -1;

    err = pthread_sigmask(SIG_SETMASK,&signal_mask,NULL);

    //errore sigmask
    if(err)
    {
        errno = err;
        return -1;
    }

    return 0;
}

/**
 * @function update_active_set
 * @brief aggiorna il set dei descrittori controllati dal server
 * @param active_set insieme dei descrittori controllati
 * @param actual_client numero attuali di client connessi
 * @param max_fd indice massimo dei descrittori
 * @return 0 on success,altrimenti -1 e setta errno
 */
static int update_active_set(fd_set *active_set,int *actual_client,int *max_fd)
{
    #ifdef DEBUG
        printf("Aggiorno set descrittori\n");
    #endif

    int fd,err;
    queue_descr_elem_t *descr; //elemento della coda dei descittori

    //risetto a 0 il flag del segnale
    updateSet = 0;

    //lock sulla coda dei descrittori da aggiornare
    err = pthread_mutex_lock(&descriptors.mtx);

    //errore lock
    if(err)
    {
        errno = err;
        return -1;
    }

    //fin quando ci sono descrittori da aggiornare
    while(descriptors.queue->size != 0)
    {
        //estraggo l'elemento
        descr = pop_queue(descriptors.queue);

        //errore nell'estrazione
        if(descr == NULL)
        {
            pthread_mutex_unlock(&descriptors.mtx);
            return -1;
        }

        pthread_mutex_unlock(&descriptors.mtx);

        fd = descr->fd; //rinominazione

        //se bisogna risettare il descrittore
        if(descr->op == SET)
        {
            FD_SET(fd,active_set);
        }
        else{
            //bisogna rimuovere
            remove_client(fd,active_set,actual_client,max_fd);
        }

        //lock se torniamo ad inizio del ciclo while
        err = pthread_mutex_lock(&descriptors.mtx);

        //errore lock
        if(err)
        {
            errno = err;
            return -1;
        }
    }

    //unlock finale
    pthread_mutex_unlock(&descriptors.mtx);

    return 0;
}

/**
 * @function server_close
 * @brief Termina il server,deallocando tutto
 *
 * @param max_fd indice massimo dei descrittori attivi
 * @param active_set insieme dei descrittori controllati
 * @param wa_args insieme degli argomenti per i thread del pool
 *
 * @return 0 in caso di successo, -1 in caso di errore e setta errno,altrimenti
 *          contiene il codice dell'errore con cui e' fallito un thread del pool del server
 */
static int server_close(int max_fd,fd_set *active_set,worker_args_t *wa_args)
{
    #ifdef DEBUG
        printf("Avviata terminazione server\n");
    #endif

    int err;

    close_all_client(max_fd,active_set);
    free(wa_args);

    err = pthread_mutex_destroy(&descriptors.mtx);

    if(err)
    {
        errno = err;
        return -1;
    }

    destroy_queue(&descriptors.queue);

    err = threadpool_destroy(&server->threadpool);

    //errore destroy
    if(err == -1)
    {
        return -1;
    }
    else{//continuo chiusura

        int curr_error = 0;

        //controllo se un thread sia fallito
        if(err > 0)
            curr_error = err;

        //rimuovo indirizzo fisico
        err = unlink(server->sa.sun_path);

        //errore unlink
        if(err == -1)
        {
            return -1;
        }

        close(server->fd);
        free(server);

        //curr_error puo contenere il codice dell'errore con cui e' fallito il thread
        return curr_error;
    }
}

/**
  * @function init_worker
  * @brief Startup di un thread worker del pool
  * @param arg argomenti per il thread worker
  * @note in caso di errore stampa a video l'errore, e manda un segnale di terminazione al server
  */
static int init_worker(void *arg)
{
    int err;
    void *buff,*err2; //buffer e gestione errori
    int rc;

    //cast degli argomenti
    worker_args_t *wa = (worker_args_t*)arg;

    //elemento da inserire nella coda dei descrittori da aggiornare
    queue_descr_elem_t elem;
    elem.fd = wa->fd_client;

    //alloco spazio per il buffer
    buff = malloc(server->messageSize);

    //allocazione andata male
    if(buff == NULL)
    {
        return -1;
    }
    else
    {
        //inzializzo il buffer che conterra' il messaggio di un client
        memset(buff,0,server->messageSize);

        //funzione per fare la read del messaggio del client
        rc = (*(read_message_fun))(wa->fd_client,buff);

        //errore nella read
        if(rc == -1)
        {
            goto work_error;
        }
        //connessione con il client chiusa
        else if(rc == 0)
        {
            //disconetto il client
            rc = disconnect_client(wa->fd_client,arg_dc);

            //errore nella disconnessione
            if(rc == -1)
            {
                goto work_error;
            }

            //setto questo fd come da rimuovere,nella coda dei descrittori da aggiornare
            elem.op = REMOVE;
        }
        else
        {
            //funzione per la gestione del client
            rc = (*(client_manager_fun))(buff,wa->fd_client,arg_cmf);

            //errore funzione
            if(rc == -1)
            {
                goto work_error;
            }

            //faccio risettare questo fd,perche' potrebbe inviare nuove richieste.
            elem.op = SET;
        }
    }
    //aggiungo alla coda dei descrittori da aggiornare
    err = pthread_mutex_lock(&descriptors.mtx);
    //errore nella lock
    if(err)
    {
        errno = err;
        goto work_error;
    }

    //inserisco nella coda
    err2 = push_queue(descriptors.queue,(void*)&elem);

    //errore psuh
    if(err2 == NULL)
    {
        pthread_mutex_unlock(&descriptors.mtx);
        goto work_error;
    }

    pthread_mutex_unlock(&descriptors.mtx);

    /* Avverto con il segnale SIGUSR2, il listener sull'aggiornamento della coda dei descrittori.
       Qui bisogna mandare un segnale direttamente al processo,perche' il segnale verra' gestito
       comunque dal listener. L'utilizzo di pthread_kill non puo' essere implementato perche' buggato */
    kill(getpid(),SIGUSR2);

    //finito di eseguire la funzione,setto l'argomento come disponibile per essere riutilizzato
    err = pthread_mutex_lock(&wa->mtx);

    //errore nella lock
    if(err)
    {
        errno = err;
        goto work_error;
    }

    wa->arg_used = 0;

    pthread_mutex_unlock(&wa->mtx);

    free(buff);

    //la funzione termina qui
    return 0;

//handler errori:
work_error:
    free(buff);
    return -1;
}

 /**
  * @function listener
  * @brief Funzione che deve eseguire il listener_thread
  * @param arg argomenti per il listener
  *
  * @return EXIT_SUCCESS in caso di successo,altrimenti EXIT_FAILURE
  *         in questo caso se e' fallito un thread del pool del server
  *         setta thread_error per propagare il codice dell'errore
  *         del thread che e' fallito
  *
  */
static void* listener(void *arg)
{
     /* variabili utili */
     int fd;  //per verificare risultati select
     int fd_client; //socket per l'I/O con un client
     int err = 0,curr_error = 0; //per la gestione degli errori
     fd_set read_set,active_set; //set usati per la select
     int max_fd,actual_client = 0; //numero attuale di client,e massimo descrittore
     worker_args_t *wa_args; //insieme degli argomenti per i worker

     //installo i signal handler
     err = set_listener_signal_handler();

     //errore set handler
     if(err == -1)
     {
         curr_error = errno;
         goto lst_error;
     }

     //server partito
     run = 1;

     //inizializzo coda dei descrittori da aggiornare
     descriptors.queue = create_queue(FIFO,sizeof(queue_descr_elem_t));

     //errore creazione coda dei descrittori da aggiornare
     if(descriptors.queue == NULL)
     {
         curr_error = errno;
         goto lst_error;
     }

     //inizializzo mutex per la coda dei descrittori da aggiornare
     err = pthread_mutex_init(&descriptors.mtx,NULL);

     if(err)
     {
         errno = err;
         goto lst_error2;
     }

     //alloco spazio per gli argomenti dei worker.Ne alloco esattamente quanti sono le massimo connessioni consentite
     wa_args = (worker_args_t*)calloc(server->max_connection,sizeof(worker_args_t));

     //errore creazione argomenti per thread
     if(wa_args == NULL)
     {
         curr_error = errno;
         goto lst_error3;
     }

     //inizializzo l'active_set
     FD_ZERO(&active_set);
     FD_SET(server->fd,&active_set);
     max_fd = server->fd;

     //blocco tutti i segnali per riceverli nella select
     sigset_t curr_mask,signal_handler_mask;
     sigfillset(&curr_mask);
     pthread_sigmask(SIG_SETMASK,&curr_mask,&signal_handler_mask);

     //qui parte il ciclo di vita del server
     while(run)
     {
        //preparo maschera per select
         read_set = active_set;

         #ifdef DEBUG
             printf("Listener: Select in attesa || client connessi : %d\n",actual_client);
         #endif

         //mi blocco fin quando non posso fare una read da un client,oppure arriva un nuovo client,oppure arriva un segnale
         int ds = pselect(max_fd + 1,&read_set,NULL,NULL,NULL,&signal_handler_mask);

         //ho ricevuto un segnale oppure e' fallita la select
         if(ds == -1)
         {
             //arrivato segnale
             if(errno == EINTR)
             {
                 //Controllo i flag dei segnali

                 //controllo la coda dei descrittori da aggiornare
                 if(updateSet)
                 {
                     err = update_active_set(&active_set,&actual_client,&max_fd);

                     //errore update del active set
                     if(err == -1)
                     {
                         curr_error = errno;
                         goto lst_error4;
                     }
                 }

                 //se devo eseguire la funzione definita con sigusr
                 if(sigusr)
                 {
                     sigusr = 0; //risetto a 0 il segnale di sigusr

                     err = threadpool_add_task(server->threadpool,signal_usr_handler,(void*)arg_suh);

                     //errore add task
                     if(err == -1)
                     {
                         curr_error = errno;
                         goto lst_error4;
                     }

                 }

                 //ritorno nella select
                 continue;
             }

             else{ //errore nella pselet
                 curr_error = errno;
                 goto lst_error4;
             }
         }

         //se arrivo qui e' arrivato un nuovo client oppure si puo fare una read

         //vado a vedere quali descrittori sono pronti
         for (fd = 0; fd <= max_fd; fd++)
         {
             //trovato un descrittore pronto
             if(FD_ISSET(fd,&read_set))
             {
                 //nuovo client
                 if(fd == server->fd)
                 {
                     //recupero fd client arrivato
                     fd_client = accept(server->fd,NULL,NULL);

                     //controllo esito della accept
                     if(fd_client == -1)
                     {
                         //arrivato segnale
                         if(errno == EINTR)
                         {
                             //mi fermo e torno all'inizio per controllare il segnale arrivato
                             break;
                         }
                         else{
                             curr_error = errno;
                             goto lst_error4;
                         }
                     }

                     //incremento numero di client connessi
                     ++actual_client;

                     //troppi client?
                     if(actual_client > server->max_connection)
                     {
                         //procedura per buttare fuori un client
                         err = (*(client_out_of_bound))(fd,arg_cob);

                         //errore nella funzione
                         if(err == -1)
                         {
                             curr_error = errno;
                             goto lst_error4;
                         }

                         //lo tolgo dal set
                         FD_CLR(fd_client,&active_set);

                         //rimuovo il client
                         remove_client(fd_client,&active_set,&actual_client,&max_fd);

                         continue;
                     }

                     #ifdef DEBUG
                         printf("Listener: Nuova connessione da parte di: %d\n",fd_client);
                     #endif

                     //setto client come attivo
                     FD_SET(fd_client,&active_set);

                     //aggiorno indice
                     if(fd_client > max_fd)
                         max_fd = fd_client;
                 }
                 //un client e' pronto per la comunicazione
                 else
                 {
                     #ifdef DEBUG
                         printf("Listener: il client %d e' pronto\n",fd);
                     #endif

                     //se arrivo qui,allora aggiungo alla coda dei task del threadpool

                     //lo tolgo dal set,perche' ora verra' gestito da un thread del pool
                     FD_CLR(fd,&active_set);

                     //preparo gli argomenti per il thread worker
                     worker_args_t *wa = prepare_arg(wa_args,fd);

                     //errore nella prepare arg
                     if(wa == NULL)
                     {
                         curr_error = errno;
                         goto lst_error4;
                     }

                     /*
                      * Qui aggiungo alla coda dei task,sia la funzione che deve svolgere
                      * un thread worker,sia i corrispondenti argomenti
                      */

                     err = threadpool_add_task(server->threadpool,init_worker,(void*)wa);

                     //errore nell'aggiunta del task
                     if(err == -1)
                     {
                         curr_error = errno;
                         goto lst_error4;
                     }

                 }//fine delle gestione del client pronto

             }//fine del if

         }//fine del for

     }//fine del while

     //se sono arrivato qui allora il listener deve terminare,deallocando tutto

    //termino normalmente
    err = server_close(max_fd,&active_set,wa_args);

    //controllo esito terminazione server
    if(err == -1 || err > 0)
    {
        //caso in cui e' fallito un thread
        if(err > 0)
        {
            //setto thread error per poter propagare il codice di ritorno dell'errore
            thread_error = err;
        }

        pthread_exit((void*)EXIT_FAILURE);
    }
    //tutto andato bene
    else{
        pthread_exit((void*)EXIT_SUCCESS);
    }

//handler errori listener
 lst_error4:
     close_all_client(max_fd,&active_set);
     free(wa_args);
     goto lst_error3;

 lst_error3:
     pthread_mutex_destroy(&descriptors.mtx);
     goto lst_error2;

 lst_error2:
     destroy_queue(&descriptors.queue);
     goto lst_error;

 lst_error:

     threadpool_destroy(&server->threadpool);
     //rimuovo indirizzo fisico
     unlink(server->sa.sun_path);

     close(server->fd);
     free(server);

     //risetto errno per sicurezza,cioe' che una funzione appena chiamata non fallisca e mi cambi errno
     errno = curr_error;
     pthread_exit((void*)EXIT_FAILURE);
 }

/* Funzione dell'interfaccia */

int start_server(server_t *srv,int num_pool_thread,server_function_t funs)
{
    int err = 0,curr_error;
    sigset_t signal_mask,old_mask;

    //controllo parametri di inizializzazione del server
    if(srv == NULL || funs.client_manager_fun == NULL || funs.read_message_fun == NULL || funs.client_out_of_bound == NULL)
    {
        errno = EINVAL;
        curr_error = errno;
        goto st_error;
    }

    //inizializzo il server dichiarato globalmente
    server = srv;

    //blocco tutti i segnali per i thread del pool,e per questo thread che fa la join
    sigfillset(&signal_mask);
    err = pthread_sigmask(SIG_SETMASK,&signal_mask,&old_mask);

    if(err)
    {
        errno = err;
        curr_error = errno;
        goto st_error;
    }

    //faccio partire il threadpool
    server->threadpool = threadpool_create(num_pool_thread);

    //errore creazione threadpool
    if(server->threadpool == NULL)
    {
        curr_error = errno;
        goto st_error;
    }

    //Inizializzo le funzioni globali
    client_manager_fun = funs.client_manager_fun;
    arg_cmf = funs.arg_cmf;

    signal_usr_handler = funs.signal_usr_handler;
    arg_suh = funs.arg_suh;

    client_out_of_bound = funs.client_out_of_bound;
    arg_cob = funs.arg_cob;

    disconnect_client = funs.disconnect_client;
    arg_dc = funs.arg_dc;

    read_message_fun = funs.read_message_fun;


    //avvio listener
    err = pthread_create(&server->listener_thread,NULL,listener,NULL);

    //errore create
    if(err)
    {

        errno = err;
        curr_error = errno;
        goto st_error2;
    }

    int status;

    err = pthread_join(server->listener_thread,(void*)&status);

    //errorejoin
    if(err)
    {
        errno = err;
        curr_error = errno;
        goto st_error2;
    }

    //server fallito
    if(status == EXIT_FAILURE)
    {
        //se e' fallito un thread,ritorno il codice dell'errore con cui e' fallito
        if(thread_error > 0)
            return thread_error;
        //errore interno del server ritorno solo -1,errno gia settato
        else
            return -1;
    }
    //tutto ok
    else
        return 0;

//definizione degli error_handling
st_error2:
    threadpool_destroy(&server->threadpool);
    goto st_error;
st_error:

    //rimuovo indirizzo fisico
    unlink(server->sa.sun_path);

    close(server->fd);
    free(server);

    //risetto errno per sicurezza,cioe' che una funzione appena chiamata non fallisca e mi cambi errno
    errno = curr_error;
    return -1;
}
