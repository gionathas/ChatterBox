/**
 * @file listener.c
 * @brief Implementazione del listener thread del server
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
#include<time.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<signal.h>
#include<sys/select.h>
#include<sys/time.h>
#include"utils.h"
#include"server.h"
#include"queue.h"

#define DEBUG

//istanza del server
static server_t *server;

//@see server_thread_error in server.h
int server_thread_error;

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

/* Coda dei descrittori da aggiornare: Viene utilizzata per aggiornare i descrittori
   del set utilizzati dalla select */
static descriptor_queue_t descriptors;

/*Flag per gestione segnali */
static volatile sig_atomic_t run = 0; //stato del server
static volatile sig_atomic_t updateSet = 0; //per segnalare di aggiornare il set dei descrittori
static volatile sig_atomic_t sigusr = 0; //per segnalare di eseguire il segnale sigusr1

/*FUNZIONI DI SUPPORTO*/

/**
  * @function init_worker
  * @brief Startup di un thread worker del pool
  * @param arg argomenti per il thread worker
  * @note in caso di errore stampa a video l'errore, e manda un segnale di terminazione al server
  */
static int init_worker(void* arg);

/**
 * @function set_listener_signal_handler
 * @brief Gestione dei segnali per il listener
 * @return on success 0,altrimenti -1 e setta errno
 */
static int set_listener_signal_handler();

/**
 * @function update_active_set
 * @brief aggiorna il set dei descrittori controllati dal server
 * @param active_set insieme dei descrittori controllati
 * @param actual_client numero attuali di client connessi
 * @param max_fd indice massimo dei descrittori
 * @return 0 on success,altrimenti -1 e setta errno
 */
static int update_active_set(fd_set *active_set,int *actual_client);

/**
 * @function remove_client
 * @brief RImuove un client dal server
 * @param fd descrittore da rimuovere
 * @param actual_client numero di client connessi
 */
static inline void remove_client(int fd,int *actual_client);

/**
 * @function close_all_client
 * @brief chiude tutti i client attualmente connessi al server
 * @param max_sd numero di descrittori controllati
 * @param set insieme dei descrittori controllati
 */
static inline void close_all_client(fd_set *set);

/**
 * @function server_close
 * @brief Termina il server,deallocando tutto
 *
 * @param max_fd indice massimo dei descrittori attivi
 * @param active_set insieme dei descrittori controllati
 * @param fd_clients struttura per contenere gli fd dei clients da passare ai thread
 *
 * @return 0 in caso di successo, -1 in caso di errore e setta errno,altrimenti
 *          contiene il codice dell'errore con cui e' fallito un thread del pool del server
 */
static int server_close(fd_set *active_set,unsigned int *fd_clients);

/* Signal Handler */

/**
 * @function termination
 * @brief Handler del segnale SIGTERM,SIGQUIT,SIGINT
 */
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

void* listener(void* srv)
{
     /* variabili utili */
     int fd;  //per verificare risultati select
     int fd_client; //socket per l'I/O con un client
     int err = 0,curr_error = 0; //per la gestione degli errori
     fd_set read_set,active_set; //set usati per la select
     int actual_client = 0; //numero attuale di client,e massimo descrittore
     unsigned int *fd_clients; //struttura per contenere gli fd dei client da passare ai thread

     //inizializzo istanza del server dichiarata globalmente
     server = srv;

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

     //alloco spazio per struttura che contiene gli fd dei client,da passare ai thread worker.
     fd_clients = malloc((server->max_connection + 3) * sizeof(unsigned int));

     //errore creazione argomenti per thread
     if(fd_clients == NULL)
     {
         curr_error = errno;
         goto lst_error3;
     }

     //inizializzo l'active_set
     FD_ZERO(&active_set);
     FD_SET(server->fd,&active_set);

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

         //uso FD_SETSIZE come valore massimo degli fd
         //mi blocco fin quando non posso fare una read da un client,oppure arriva un nuovo client,oppure arriva un segnale
         int ds = pselect(FD_SETSIZE,&read_set,NULL,NULL,NULL,&signal_handler_mask);

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
                     err = update_active_set(&active_set,&actual_client);

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

                     //delego un thread alla gestione del segnale USR1
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
         for (fd = 0; fd <= FD_SETSIZE; fd++)
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
                         remove_client(fd_client,&actual_client);

                         continue;
                     }

                     #ifdef DEBUG
                         printf("Listener: Nuova connessione da parte di: %d\n",fd_client);
                     #endif

                     //setto client come attivo
                     FD_SET(fd_client,&active_set);
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
                     fd_clients[fd] = fd;

                     /*
                      * Qui aggiungo alla coda dei task,sia la funzione che deve svolgere
                      * un thread worker,sia i corrispondenti argomenti
                      */

                     err = threadpool_add_task(server->threadpool,init_worker,(void*)&fd_clients[fd]);

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
    err = server_close(&active_set,fd_clients);

    //controllo esito terminazione server
    if(err == -1 || err > 0)
    {
        //caso in cui e' fallito un thread
        if(err > 0)
        {
            //setto thread error per poter propagare il codice di ritorno dell'errore
            server_thread_error = err;
        }

        pthread_exit((void*)EXIT_FAILURE);
    }
    //tutto andato bene
    else{
        pthread_exit((void*)EXIT_SUCCESS);
    }

//handler errori listener
 lst_error4:
     close_all_client(&active_set);
     free(fd_clients);
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

static int init_worker(void* arg)
{
     int err;
     void *buff,*err2; //buffer e gestione errori
     int rc;

     //cast argomenti
     unsigned int fd = *(unsigned int*)arg;

     //elemento da inserire nella coda dei descrittori da aggiornare
     queue_descr_elem_t elem;
     elem.fd = fd;

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
         rc = (*(read_message_fun))(fd,buff);

         //errore nella read
         if(rc == -1)
         {
             goto work_error;
         }
         //connessione con il client chiusa
         else if(rc == 0)
         {
             //disconetto il client
             rc = disconnect_client(fd,arg_dc);

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
             rc = (*(client_manager_fun))(buff,fd,arg_cmf);

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

     free(buff);

     //la funzione termina qui
     return 0;

 //handler errori:
 work_error:
     free(buff);
     return -1;
}

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
     error_handler_1(err,-1,-1);

     //setto la maschera
     err = pthread_sigmask(SIG_SETMASK,&signal_mask,NULL);

     //errore nel set della maschera
     error_handler_3(err,-1);

     //inizializzo la struttura per gestire i segnali
     memset(&sa,0,sizeof(sa));

     sa.sa_flags = 0; //non voglio il restart delle SC

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
     error_handler_1(err,-1,-1);

     //rimetto la maschera predefinita
     err = sigemptyset(&signal_mask);

     //errore emptyset
     error_handler_1(err,-1,-1);

     err = pthread_sigmask(SIG_SETMASK,&signal_mask,NULL);

     //errore sigmask
    error_handler_3(err,-1);

     return 0;
 }

static int update_active_set(fd_set *active_set,int *actual_client)
{
     #ifdef DEBUG
         printf("Aggiorno set descrittori\n");
     #endif

     int fd,err;
     queue_descr_elem_t *descr = NULL; //elemento della coda dei descittori

     //risetto a 0 il flag del segnale
     updateSet = 0;

     //lock sulla coda dei descrittori da aggiornare
     err = pthread_mutex_lock(&descriptors.mtx);

     //errore lock
     error_handler_3(err,-1);

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
             FD_CLR(fd,active_set);
             remove_client(fd,actual_client);
         }

         free(descr);
         descr = NULL;

         //lock se torniamo ad inizio del ciclo while
         err = pthread_mutex_lock(&descriptors.mtx);

         //errore lock
         error_handler_3(err,-1);
     }

     //unlock finale
     pthread_mutex_unlock(&descriptors.mtx);

     return 0;
 }

static inline void remove_client(int fd,int *actual_client)
{
     #ifdef DEBUG
         printf("Rimozione client %d\n",fd);
     #endif

     //decremento numero attuale di client connessi
     --*actual_client;

     //chiudo il descrittore
     close(fd);
}

static int server_close(fd_set *active_set,unsigned int *fd_clients)
{
    #ifdef DEBUG
        printf("Avviata terminazione server\n");
    #endif

    int err;

    close_all_client(active_set);
    free(fd_clients);

    err = threadpool_destroy(&server->threadpool);

    //dealloco struttura per contenere i descrittori
    pthread_mutex_destroy(&descriptors.mtx);
    destroy_queue(&descriptors.queue);

    //errore chiusura threadpool
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
        error_handler_1(err,-1,-1);

        close(server->fd);
        free(server);

        //curr_error puo contenere il codice dell'errore con cui e' fallito il thread
        return curr_error;
    }
}

static inline void close_all_client(fd_set *set)
{
    for (int i=0; i <= FD_SETSIZE; ++i)
    {
        if (FD_ISSET(i,set))
            close(i);
    }
}
