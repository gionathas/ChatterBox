/**
 *  @file threadpool.c
 *  @brief Implementazione del modulo del threadpool
 *  @author Gionatha Sturba 531274
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 */
#define _POSIX_SOURCE
#include<stdlib.h>
#include<pthread.h>
#include<errno.h>
#include<stdio.h>
#include<unistd.h>
#include<signal.h>
#include"threadpool.h"
#include"utils.h"
#include"queue.h"

/*Definisco lo scheduling FIFO per la coda dei task */
static const q_mode_t task_scheduler = FIFO;

/*
 * Variabile per propagare il codice di errore con cui puo' fallire un thread del pool
 * @note non si usa un mutex per proteggere questa variabile perche' una volta settata,
 * la variabile verra' accessa solo in lettura.
*/
static int threadpool_thread_error = 0;

/* FUNZIONI DI SUPPORTO */

/**
 * @function create_task_queue
 * @brief Crea e inizializza la coda dei task
 * @return puntatore alla coda dei task,altrimenti ritorna NULL e setta errno e dealloca tutto
 */
static queue_task_t *create_task_queue();

/**
 * @function join_threads
 * @brief Fa la join dei threads del pool
 * @param tp threadpool
 * @return 0 tutto andato bene, -1 con errno settato se c'e' stato un errore,
 *            altrimenti codice dell'errore con cui e' fallito un thread
 *
 */
static int join_threads(threadpool_t *tp);

/**
 * @function free_task_queue
 * @brief dealloca l'intera struttura della coda dei task
 *
 * @return 0 on success,altrimenti codice dell'errore.
 * @note il codice dell'errore verra gestito dalla create.
 */
static int free_task_queue(queue_task_t *Q);

/**
 * @function thread_worker
 * @brief Comportamento di un thread del threadpool.
 *
 * @param arg argomento della funzione(in questo caso gli viene passato il threadpool
 * @return EXIT_SUCCESS in caso di successo,altrimenti ritorna il valore di errno con cui e' fallito
 *
 * @note Nel caso fallisca un thread del threadpool,allora si incorre in una terminazione
 * immediata di tutto il threadpool.Il modo in cui e' stata ottenuta questa terminazione
 * e'terminare (SIGINT) tutto il processo.
 */
static void* thread_worker(void* arg);

/* Funzioni Interfaccia */

threadpool_t *threadpool_create(int thread_in_pool)
{
    int err;

    //controllo parametri
    if(thread_in_pool <= 0 || thread_in_pool > MAX_THREAD)
    {
        errno = EINVAL;
        return NULL;
    }

    //alloco struttura threapool, e controllo esito
    threadpool_t *pool = (threadpool_t*)malloc(sizeof(threadpool_t));

    //controllo allocazione
    error_handler_1(pool,NULL,NULL);

    //creazione coda dei task5
    pool->task_queue = create_task_queue(pool);

    if(pool->task_queue == NULL)
    {
        free(pool);
        return NULL;
    }

    //inizializzazione variabili
    pool->threads_in_pool = 0;
    pool->shutdown = 0;

    //allocazione spazio per tutti i thread del poo
    pool->threads= (pthread_t*)malloc(sizeof(pthread_t) * thread_in_pool);

    if(pool->threads == NULL)
    {
        free_task_queue(pool->task_queue);
        free(pool);
        return NULL;
    }

    //inizializzo mutex del threadpool
    err = pthread_mutex_init(&pool->mtx,NULL);

    if(err)
    {
        errno = err;
        free(pool->threads);
        free_task_queue(pool->task_queue);
        free(pool);
        return NULL;
    }

    //faccio partire i thread del pool
    for (size_t i = 0; i < thread_in_pool; i++)
    {
        /*Da qui in poi si attiva la concorrenza tra thread */

        //creo thread
        err = pthread_create(&pool->threads[i],NULL,thread_worker,(void*)pool);

        //controllo errore creazione thread
        TP_ERROR_HANDLER_2(err,NULL,threadpool_destroy(&pool));

        //incremento numero di thread attuali nel pool
        ++pool->threads_in_pool;
    }

    return pool;
}

static queue_task_t *create_task_queue()
{
    int err;
    queue_task_t *q_task;

    //allocazione struttura della coda,e controllo esito
    q_task = (queue_task_t*)malloc(sizeof(queue_task_t));

    if(q_task == NULL)
    {
        return NULL;
    }

    //allocazione coda
    q_task->queue = create_queue(task_scheduler,sizeof(task_t));

    if(q_task->queue == NULL)
    {
        free(q_task);
        return NULL;
    }

    //allocazione del mutex,e controllo esito
    err = pthread_mutex_init(&q_task->mtx,NULL);

    if(err)
    {
        errno = err;
        destroy_queue(&q_task->queue);
        free(q_task);
        return NULL;
    }

    //allocazione della variabile condizione, e controllo esito
    err = pthread_cond_init(&q_task->cond,NULL);

    if(err)
    {
        errno = err;
        //non serve controllare esito, @see man pthread_mutex_destroy il perche'
        pthread_mutex_destroy(&q_task->mtx);
        destroy_queue(&q_task->queue);
        free(q_task);
        return NULL;
    }

    return q_task;
}

int threadpool_destroy(threadpool_t **pool)
{
    #ifdef DEBUG
        fflush(stdout);
        printf("\nDistruzione avviata\n");
    #endif

    //per gestire errori funzioni
    int err = 0;
    int cod_error = 0;

    //threadpool non valido
    if(*pool == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    #ifdef DEBUG
        fflush(stdout);
        printf("DESTROY:lock pool mutex\n");
    #endif

    //lock per andare a controllare info sul threadpool
    err = pthread_mutex_lock( &((*pool)->mtx) );

    //possibile errore nella lock,@note posso ritornare subito perche' non ho fatto la lock
    error_handler_3(err,-1);

    //shutdown gia' partito
    if((*pool)->shutdown == 1)
    {
        errno = EINPROGRESS;
        return -1;
    }

    //setto shutdown
    (*pool)->shutdown = 1;

    //la lock non puo' fallire,se sono arrivato qui possiedo sicuramente il mutex
    pthread_mutex_unlock( (&(*pool)->mtx) );

    #ifdef DEBUG
        fflush(stdout);
        printf("DESTROY: rilascio lock pool mutex\n");
    #endif

    #ifdef DEBUG
        fflush(stdout);
        printf("DESTROY: lock task mutex\n");
    #endif

    //lock sulla coda dei task
    err = pthread_mutex_lock(&((*pool)->task_queue->mtx));

    //possibile errore nella lock della coda dei task,posso ritornare subito
    error_handler_3(err,-1);

    /* sveglio tutti eventuali thread bloccati sulla coda dei task */
    err = pthread_cond_broadcast(&((*pool)->task_queue->cond));

    //unlock coda dei task
    pthread_mutex_unlock(&((*pool)->task_queue->mtx));

    #ifdef DEBUG
        fflush(stdout);
        printf("DESTROY:rilascio lock task mutex\n");
    #endif

    //controllo errore nel broadcast
    error_handler_3(err,-1);

    #ifdef DEBUG
        printf("joining threads \n");
    #endif

    //faccio la join dei thread nel pool
    err = join_threads(*pool);

    #ifdef DEBUG
        fflush(stdout);
        printf("join went well\n");
    #endif

    //controllo errore nella join
    if(err == -1)
    {
        return -1;
    }

    //caso in cui un thread e' fallito ed ha settato codice dell'errore
    if(err > 0)
    {
        cod_error = err;
    }

    //dealloco spazio dei thread del pool
    free((*pool)->threads);

    //dealloco coda dei task
    err = free_task_queue((*pool)->task_queue);

    //controllo errore nella deallocazione della task queue
    error_handler_3(err,-1);

    //distruzione mutex,non serve test errore
    err = pthread_mutex_destroy(&(*pool)->mtx);

    //controllo errore nella destroy del mutex del threadpool
    error_handler_3(err,-1);

    //deallocazione struttura threadpool
    free(*pool);
    *pool = NULL;

    //se vale 0 tutto andato bene,altrimenti ritorniamo codice errore con cui e' fallito il thread
    return cod_error;
}

static int join_threads(threadpool_t *tp)
{
    int rc;

    //prima di fare la join sveglio eventuali thread bloccati sulla coda dei task
    rc = pthread_mutex_lock(&tp->task_queue->mtx);

    //controllo errore lock coda dei task
    error_handler_3(rc,-1);

    //sveglio eventuali thread bloccati sulla coda dei task
    rc = pthread_cond_broadcast(&tp->task_queue->cond);

    pthread_mutex_unlock(&tp->task_queue->mtx);

    //controllo errore nel broadcast
    error_handler_3(rc,-1);

    //faccio il join di tutti i thread,e controllo lo stato con cui terminano
    for (size_t i = 0; i < tp->threads_in_pool; i++)
    {
        rc = pthread_join(tp->threads[i],NULL);

        //controllo errore nella join
        error_handler_3(rc,-1);

    }

    //se thread_error e' uguale a 0,nessun thread e' fallito,altrimenti ritorna il codice dell'errore
    return threadpool_thread_error;
}

static int free_task_queue(queue_task_t *Q)
{
    int err;

    //distruzione mutex,cond,e distruzione coda.
    err = pthread_mutex_destroy(&Q->mtx);

    //controllo errore nella destroy del mutex
    TP_RETURN_ERR_CODE(err);

    err = pthread_cond_destroy(&Q->cond);

    //controllo errore nella destroy condizione
    TP_RETURN_ERR_CODE(err);

    destroy_queue(&Q->queue);

    free(Q);
    Q = NULL;

    return 0;
}

int threadpool_add_task(threadpool_t *tp,int (*function)(void *),void* arg)
{
    int err;
    void *err2;

    //argomenti errati,@note l'argomento puo essere NULL
    if(tp == NULL || function == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    //controllo che lo shutdown non sia gia avviato
    err = pthread_mutex_lock(&tp->mtx);

    //econtrollo rrore nella lock
    error_handler_3(err,-1);

    //shutdown avviato non si inserisce piu nella coda
    if(tp->shutdown != 0)
    {
        pthread_mutex_unlock(&tp->mtx);
        //ritorno questo errno se siamo in fase di chiusura
        errno = EPERM;
        return -1;
    }

    pthread_mutex_unlock(&tp->mtx);

    //se non stiamo terminando,posso inserire nella coda

    //creo il task da inserire
    task_t task;
    task.function = function;
    task.arg = arg;

    //ora posso inserire nella coda

    //prendo lock della coda
    err = pthread_mutex_lock(&tp->task_queue->mtx);

    //controllo errore nella lock
    error_handler_3(err,-1);

    err2 = push_queue(tp->task_queue->queue,(void*)&task);

    //controllo errore nella push
    if(err2 == NULL)
    {
        pthread_mutex_unlock(&tp->task_queue->mtx);
        return -1;
    }

    //inserito correttamente,incremento la size della coda
    ++tp->task_queue->queue->size;

    //sveglio i  worker in attesa
    err = pthread_cond_signal(&tp->task_queue->cond);

    //controllo errore nella signal
    TP_ERROR_HANDLER_2(err,-1,pthread_mutex_unlock(&tp->task_queue->mtx));

    //rilascio mutex coda
    pthread_mutex_unlock(&tp->task_queue->mtx);

    return 0;

}

static void* thread_worker(void* arg)
{
    #ifdef DEBUG
        fflush(stdout);
        printf("Attivato thread %ld\n",pthread_self());
        //id per riconoscere il thread.
        pthread_t id= pthread_self();
    #endif

    //cast dell'argomento
    threadpool_t *tp = (threadpool_t*)arg;
    task_t *mytask = NULL;
    int rc;

    //fin quando non avviene lo shutdown
    while(1)
    {
        //prendo sia la lock della coda dei task
        rc = pthread_mutex_lock(&tp->task_queue->mtx);

        //errore lock
        if(rc)
        {
            goto immediate_termination;
        }

        //prendo la lock del threadpool per controllare se stiamo terminando
        rc = pthread_mutex_lock(&tp->mtx);

        //errore lock
        if(rc)
        {
            pthread_mutex_unlock(&tp->task_queue->mtx);
            goto immediate_termination;
        }

        //fin quando non ci sono task e lo shutdown normale non e' stato avviato
        while (tp->task_queue->queue->size == 0 && tp->shutdown == 0)
        {
            //non stiamo terminano lascio il mutex del threadpool
            pthread_mutex_unlock(&tp->mtx);

            #ifdef DEBUG
                fflush(stdout);
                printf("in attesa %ld\n",id);
            #endif

            //mi metto in attesa sulla coda dei task
            rc = pthread_cond_wait(&tp->task_queue->cond,&tp->task_queue->mtx);

            //errore wait
            if(rc)
            {
                pthread_mutex_unlock(&tp->task_queue->mtx);
                goto immediate_termination;
            }

            //riprendo la lock del pool per la condizione del while
            rc = pthread_mutex_lock(&tp->mtx);

            //errore lock
            if(rc)
            {
                pthread_mutex_unlock(&tp->task_queue->mtx);
                goto immediate_termination;
            }

        }

        //mi sono svegliato,o peche' e' stato avviato lo shutdown o perche' c'e' un nuovo task

        //controllo shutdown
        if(tp->shutdown == 1)
        {
            //rilascio la lock della coda dei task
            pthread_mutex_unlock(&tp->task_queue->mtx);
            break;
        }

        //non e' stato avviato lo shutdown,quindi c'e' un nuovo task per me

        //rilascio lock del threadpool..
        pthread_mutex_unlock(&tp->mtx);

        //recupero il task da eseguire dalla coda
        mytask = pop_queue(tp->task_queue->queue);

        //errore nell'estrazione
        if(mytask == NULL)
        {
            rc = errno;
            pthread_mutex_unlock(&tp->task_queue->mtx);
            goto immediate_termination;
        }

        //nessun errore,rilascio mutex della coda, e decremento la size della coda
        --tp->task_queue->queue->size;
        pthread_mutex_unlock(&tp->task_queue->mtx);

        #ifdef DEBUG
            printf("%ld eseguo task\n",id);
        #endif

        //eseguo il task,rc mi ritorna esito della funzione
        rc = (*(mytask->function))(mytask->arg);

        //libero memoria da mytask
        free(mytask);
        mytask = NULL;

        //errore funzione
        if(rc == -1)
        {
            rc = errno;
            goto immediate_termination;
        }

        #ifdef DEBUG
            printf("%ld finito task\n",id);
        #endif
    }

    //rilascio mutex del threadpool..
    pthread_mutex_unlock(&tp->mtx);

    //..e termino
    pthread_exit((void*)EXIT_SUCCESS);

//error handler
immediate_termination:
    //Mando un segnale di terminazione immediata
    kill(getpid(),SIGTERM);
    //setto la var thread_error per propagare codice di ritorno dell'errore con cui sono fallito
    threadpool_thread_error = rc;
    //termino indicando fallimento
    pthread_exit((void*)EXIT_FAILURE);
}
