/**
 *  @file threadpool.h
 *  @brief Hader file del threadpool
 *  @author Gionatha Sturba 531274
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 */
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include<pthread.h>
#include"queue.h"

/* numero massimo di thread creabili,non eccedere con questo valore. */
#define MAX_THREAD 64

/* GESTIONE ERRORI THREADPOOL */

/* Per indicare l'errore di ritono quando un thread del pool e' fallito */
#define THREAD_FAILED 2

//handler per la gestione degli errori nel threadpool
#define TP_ERROR_HANDLER_1(err,ret)   \
    do{ if(err){errno = err;return ret;}}while(0)

//handler che esegue anche una funzione
#define TP_ERROR_HANDLER_2(err,ret,fun)   \
    do{ if(err){errno = err;fun;return ret;}}while(0)

//ritorna il codice dell'errore al chiamante.@note viene usato nelle funzioni di utilita'
#define TP_RETURN_ERR_CODE(err)     \
    do{if(err){return err;}}while(0)

/* FINE GESTIONE ERRORI */

/*
 * @struct queue_task
 * @brief Struttura della coda dei task
 *
 * @var queue coda
 * @var mtx mutex per l'accesso in mutua esclusione
 * @var cond variabile condizione per notificare i thread in attesa
 *
 * @see task_t
 */
typedef struct queue_task{
    queue_t *queue;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
}queue_task_t;

/**
 * @struct task
 * @brief struttura di un task. Un task e' una funzione che dovra'
 *        essere eseguita da un thread del threadpool.
 *
 * @var function funzione che deve essere eseguita dal thread
 * @var arg argomento della funzione.
 * @note la funzione ritorna un int per segnalare l'esito.
 */
typedef struct task{
    int (*function)(void*);
    void *arg;
}task_t;


/**
 * @struct threadpool
 * @brief struttura di un threadpool
 *
 * @var task_queue coda dei task
 * @var threads threads del pool
 * @var thread_in_pool numero di thread attuali nel pool
 * @var mtx mutex per sincronizzare i thread nell'accesso alla variabile shutdown
 * @var shutdown indica se il threadpool e' in fase di chiusura
 */
typedef struct threadpool{
    queue_task_t *task_queue;
    pthread_t *threads;
    size_t threads_in_pool;
    pthread_mutex_t mtx;
    unsigned short shutdown;
}threadpool_t;

/**
 * @function threadpool_create
 * @brief Crea un threadpool,e lo inizializza
 *
 * @param thread_in_pool numero di thread da inserire nel pool
 *
 * @return Puntatore alla struttura del threadpool.On error setta errno,e ritorna NULL.
 */
threadpool_t *threadpool_create(int thread_in_pool);

/**
 * @function threadpool_destroy
 * @brief Termina e dealloca l'intera struttura del threadpool
 *
 * @param pool puntatore alla locazione di memoria dove risiede il puntatore al threadpool
 *
 * @note @param pool,viene passato questo puntatore per una corretta deallocazione
 *
 * @return On success ritorna 0. Oppure  -1 se c'e' un errore nella destroy e setta errno.
 * @note Caso particolare: RItorna THREAD_FAILED se un thread del pool fallisce,ma la deallocazione
 *       e' avvenuta corretamente.
 */
int threadpool_destroy(threadpool_t **pool);

/**
 * @function threadpool_add_task
 * @brief Inserisce un nuovo task nella coda dei task
 *
 * @param tp threadpool
 * @param function funzione da eseguire
 * @param arg argomento della funzione
 *
 * @return 0 on success,altrimenti -1 e setta errno
 *
 * @note la funzione e l'argomento diverrano poi un task.
 */
int threadpool_add_task(threadpool_t *tp,int (*function)(void *),void* arg);

#endif /* THREAD_POOL_H */
