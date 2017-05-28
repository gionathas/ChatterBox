#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<sys/select.h>
#include<sys/time.h>
#include"server.h"

// lunghezza massima del path in Unix
#define UNIX_PATH_MAX 108
#define DEBUG
#define PROVA

/**
 * @struct worker_args_t
 * @brief Struttura che contiene gli argomenti per un thread_worker
 * @var worker_fun funzione che deve svolgere il thread_worker
 * @var fd_client il descrittore del client con cui deve interagire
 * @var messageSize size del messaggio che riceve da un client
 * @var arg_used per vedere se l'argomento e' stato finito di utilizzare
 * @var mtx mutex per settare arg_used,sia dai thred che dal dispatcher;
 */
typedef struct{
    void (*worker_fun)(void*,int,void*);
    void *worker_fun_arg;
    void *buffer;
    int fd_client;
    size_t messageSize;
    unsigned short int arg_used; //0 non utilizzato, 1 fine utilizzato
    pthread_mutex_t mtx;
}worker_args_t;

static worker_args_t* prepare_arg(worker_args_t *args,size_t MessSize,void (*worker_fun)(void *,int,void*),void *arg,size_t args_size,int fd_client,void *buff)
{
    int err;
    int i;
    int isInit = 0; //per vedere se la posizione e' inizializzata

    //cerco una posizione libera per l'argomento all'interno dell'array degli argomenti
    for (i = 0; i < args_size; i++)
    {
        //se non e' ancora inizializzata,si puo' utilizzare senza fare la lock
        if(args[i].buffer == NULL)
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

    if(i == args_size)
    {
        errno = EAGAIN;
        return NULL;
    }

    if(!isInit)
    {

        args[i].buffer = malloc(MessSize);

        err = pthread_mutex_init(&args[i].mtx,NULL);

        if(err)
        {
            errno = err;
            return NULL;
        }
    }

    else //gia' inizializzato
    {
        //pulisco il buffer
        memset(args[i].buffer,0,MessSize);
    }

    //scrivo sul buffer
    memcpy(args[i].buffer,buff,MessSize);
    //inizializzo altri parametri
    args[i].worker_fun = worker_fun;
    args[i].worker_fun_arg = arg;
    args[i].fd_client = fd_client;
    args[i].messageSize = MessSize;
    args[i].arg_used = 1;


    return (args+i);
}

//funzione che estrapolano i worker dal queue del threadpool,ed eseguono
static void init_worker(void *arg)
{
    int err;
    //cast degli argomenti
    worker_args_t *wa = (worker_args_t*)arg;

    //worker esegue la funzione per gestire il client.Gli passo il buffer del messaggio,l'fd del client e gli argomenti personali
    (*(wa->worker_fun))(wa->buffer,wa->fd_client,wa->worker_fun_arg);

    //finito di eseguire la funzione,setto l'argomento come disponibile per essere riutilizzato
    err = pthread_mutex_lock(&wa->mtx);

    //errore nella lock
    if(err)
    {
        errno = err;
        pthread_exit((void*)EXIT_FAILURE);
    }

    wa->arg_used = 0;

    pthread_mutex_unlock(&wa->mtx);

}

//chiude tutte le connessioni dei client
static void close_all_client(int max_sd,fd_set *set)
{
    for (int i=0; i <= max_sd; ++i)
    {
        if (FD_ISSET(i,set))
            close(i);
    }
}

static void remove_client(int fd,fd_set *active_set,int *actual_client,int *max_fd)
{
    #ifdef DEBUG
        printf("Rimozione client %d\n",fd);
    #endif
    //lo tolgo dal set
    FD_CLR(fd,active_set);

    //decremento numero attuale di client connessi
    --*actual_client;

    //aggiorno l'indice
    if(fd == *max_fd)
    {
        //fin quando trovo bit non settati,decremento l'indice massimo
        while(FD_ISSET(*max_fd,active_set) == 0)
            *max_fd -= 1;
    }

    //chiudo il descrittore
    close(fd);
}

static int dispatcher(server_t *server,size_t messageSize,int max_connection,client_manager_fun_t cl_manager,too_much_client_handler_t cl_outbound)
{
    /*
     * Struttura per mantenere delle info sui client quando si devono rimuovere.
     * Ogni posizione contiene: 0 = client da buttare fuori, 1 altrimenti
     * Ogni posizione corrisponde ad un particolare client.
     * ne alloco FD_SETSIZE perche' e' il numero massimo di descrittori che la select puo' gestire
     */
    int info_clients[FD_SETSIZE];

    /* Altre variabili utili */
    int fd;  //per verificare risultati select
    int fd_client; //socket per l'I/O con un client
    int err = 0,curr_error = 0; //per la gestione degli errori
    fd_set read_set,active_set; //set usati per la select
    int max_fd,actual_client = 0; //numero attuale di client,e massimo descrittore
    int too_much_client = 0; //se settato a 1,dobbiamo buttare fuori client
    worker_args_t *args; //insieme degli argomenti per i worker
    void *buff; //spazio per il messaggio del client

    //alloco spazio per gli argomenti dei worker.Ne alloco esattamente quanti sono le massimo connessioni consentite
    args = (worker_args_t*)calloc(max_connection,sizeof(worker_args_t));

    if(args == NULL)
    {
        curr_error = errno;

        err = threadpool_destroy(&server->threadpool);
        err = unlink(server->sa.sun_path);
        close(server->fd);
        free(server);

        //ritorno errore della mancata allocazione
        if(err != 0)
            errno = curr_error;

        return -1;
    }

    //alloco spazio per il buffere lo inizializzo
    buff = malloc(messageSize);

    //errore malloc
    if(buff == NULL)
    {
        curr_error = errno;

        free(args);
        err = threadpool_destroy(&server->threadpool);
        err = unlink(server->sa.sun_path);
        close(server->fd);
        free(server);

        //ritorno errore della mancata allocazione
        if(err != 0)
            errno = curr_error;

        return -1;
    }

    //inzializzo il buffer che conterra' il messaggio di un client
    memset(buff,0,messageSize);

    //inizializzo l'active_set
    FD_ZERO(&active_set);
    FD_SET(server->fd,&active_set);
    max_fd = server->fd;

    //TODO Inserire timeval

    //qui parte il ciclo di vita del server
    while(1)
    {
        //preparo maschera per select
        read_set = active_set;

        #ifdef DEBUG
            printf("Dispatcher: Select in attesa || client connessi : %d\n",actual_client);
        #endif

        //mi blocco fin quando non posso fare una read da un client,oppure arriva un nuovo client
        int ds = select(max_fd + 1,&read_set,NULL,NULL,NULL);

        //errore nella select
        if(ds == -1)
        {
            err = 1;
            break;
        }

        //TODO caso in cui e' terminato il timeout

        //vado a vedere quali descrittori sono pronti
        for (fd = 0; fd <= max_fd; fd++)
        {
            //trovato un descrittore pronto
            if(FD_ISSET(fd,&read_set))
            {
                //nuovo client
                if(fd == server->fd)
                {
                    //recupero fd client
                    fd_client = accept(server->fd,NULL,NULL);

                    //errore nella accept
                    if(fd_client == -1)
                    {
                        err = 1;
                        break;
                    }

                    //incremento numero di client connessi
                    ++actual_client;

                    //troppi client?
                    if(actual_client > max_connection)
                    {
                        too_much_client = 1;

                        //questo client e' da buttare fuori
                        info_clients[fd_client]= 1;
                    }
                    else //non e' un client di troppo
                    {
                        info_clients[fd_client] = 0;
                    }

                    #ifdef DEBUG
                        printf("Dispatcher: Nuova connessione da parte di: %d\n",fd_client);
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
                        printf("Dispatcher: il client %d e' pronto\n",fd);
                    #endif

                    //leggo il messaggio del client e lo salvo nel buffer
                    int nread = read(fd,buff,messageSize);

                    //errore nella read
                    if(nread == -1)
                    {
                        err = 1;
                        break;
                    }

                    //caso in cui il client ha chiuso la Connessione
                    if(nread == 0)
                    {
                        remove_client(fd,&active_set,&actual_client,&max_fd);
                    }

                    //se ci sono troppi client connessi,ne butto fuori alcuni
                    if(too_much_client)
                    {
                        //client attuale e' da buttare fuori?
                        if(info_clients[fd] == 1)
                        {
                            //procedura per buttare fuori un client
                            (*(cl_outbound.function))(fd,cl_outbound.arg);

                            //rimuovo il client
                            remove_client(fd,&active_set,&actual_client,&max_fd);

                            //vediamo se ci sono ancora troppi client
                            if(actual_client <= max_connection)
                            {
                                too_much_client = 0;
                            }
                        }
                    }

                    //se arrivo qui,allora aggiungo alla coda dei task

                    //preparo gli argomenti per il thread worker
                    worker_args_t *wa = prepare_arg(args,messageSize,cl_manager.function,cl_manager.arg,max_connection,fd,buff);

                    //errore nella prepare arg
                    if(wa == NULL)
                    {
                        err = 1;
                        break;
                    }

                    /*
                     * Qui aggiungo alla coda dei task,sia la funzione che deve svolgere
                     * un thread worker,sia i corrispondenti argomenti
                     */
                    err = threadpool_add_task(server->threadpool,init_worker,(void*)wa);

                    //errore nell'aggiunta del task
                    if(err == -1)
                    {
                        err = 1;
                        break;
                    }

                    //pulisco il buffer,per inserire un nuovo messaggio
                    memset(buff,0,messageSize);

                }//fine delle gestione del client pronto

            }//fine del caso in cui il descrittore e' settato

        }//fine del for

        //se arrivo qui e c'e' stato un errore,dealloco tutto
        //TODO questa e' server destroy
        if(err == 1)
        {
            curr_error = errno;

            close_all_client(max_fd,&active_set);
            free(args);
            err = threadpool_destroy(&server->threadpool);
            err = unlink(server->sa.sun_path);
            close(server->fd);
            free(server);

            //ritorno errore della mancata allocazione
            if(err != 0)
                errno = curr_error;

            return -1;
        }

    }//fine del ciclo di vita del server

    //se arrivo qui il server deve terminare
    return 0;
}

void start_server(server_t *server,size_t messageSize,int max_connection,client_manager_fun_t cl_manager,too_much_client_handler_t cl_outbound)
{
    //controllo parametri di Inizializzazione del server
    if(messageSize <= 0 || server == NULL || max_connection <= 0 || cl_manager.function == NULL || cl_outbound.function == NULL)
    {
        errno = EINVAL;
        return;
    }

    //attivo il dispatcher
    dispatcher(server,messageSize,max_connection,cl_manager,cl_outbound);

    //se arrivo qui c'e' stato un errore nel dispatcher e ritorno
    return;
}


server_t* init_server(char *sockname,int num_pool_thread)
{
    int err,curr_error;
    server_t *server;

    //controllo parametri
    if(sockname == NULL || num_pool_thread <= 0)
    {
        errno = EINVAL;
        return NULL;
    }

    //alloco server e controllo esito
    server = (server_t*)malloc(sizeof(server_t));

    SERVER_ERR_HANDLER(server,NULL,NULL);

    //inizializzazione indirizzo server

    //path del server
    err = snprintf(server->sa.sun_path,UNIX_PATH_MAX,"%s",sockname);

    //controllo errore snprintf
    if(err < 0)
    {
        //@see man snprintf return value
        errno = EIO;
        free(server);
        return NULL;
    }

    //setto il tipo di connessione
    server->sa.sun_family = AF_UNIX;

    //inzializzo il socket
    server->fd = socket(AF_UNIX,SOCK_STREAM,0);

    //controllo errore socket
    if(server->fd == -1)
    {
        free(server);
        return NULL;
    }

    //assegno indirizzo al socket
    err = bind(server->fd,(struct sockaddr *)&server->sa,sizeof(server->sa));

    //controllo errore bind
    if(err == -1)
    {
        close(server->fd);
        free(server);
        return NULL;
    }

    //metto in ascolto
    err = listen(server->fd,SOMAXCONN);

    //controllo errore listen
    if(err == -1)
    {
        //se la unlink fallisce ritorno l'errore della listen
        curr_error = errno;

        //rimuovo indirizzo fisico
        if(unlink(server->sa.sun_path) == -1)
        {
            errno = curr_error;
        }

        close(server->fd);
        free(server);
        return NULL;
    }

    //faccio partire il threadpool
    server->threadpool = threadpool_create(num_pool_thread);

    //errore creazione threadpool
    if(server->threadpool == NULL)
    {
        //se la unlink fallisce ritorno l'errore della listen
        curr_error = errno;

        //rimuovo indirizzo fisico
        if(unlink(server->sa.sun_path) == -1)
        {
            errno = curr_error;
        }

        close(server->fd);
        free(server);
        return NULL;
    }

    //inizializzazione andata a buon fine
    return server;
}
