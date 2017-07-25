/**
 * @file queue.h
 * @brief Header file di una coda generica.
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */

#ifndef QUEUE_H
#define QUEUE_H

#include<stdlib.h>
#include<stdio.h>
#include<errno.h>

/*
 *  Macro che controlla se una lista esiste
 */
#define CHECK_QUEUE(L)                              \
    if((L) == NULL)                                 \
    {               								\
    	errno = EINVAL;                             \
        return NULL;                                \
    }

/**
 * @enum q_mode_t
 * @brief Modalita' di transito degli oggetti della coda
 */
typedef enum queue_mode{
    FIFO,
    LIFO
}q_mode_t;

/**
 * @struct node_t
 * @brief Nodo di una coda
 * @var info informazione ch<e contiene il Nodo
 * @var next puntatore all'elemento successivo della coda
 */
typedef struct node{
    void* info;
    struct node *next;
}node_t;

/**
 * @struct queue_t
 * @brief Struttura della coda
 * @var head puntatore al primo nodo della coda
 * @var size dimensione attuale della coda
 * @var memSize sizeof degli elementi della coda
 * @var mode modalita' di funzionamento della coda
 */
typedef struct queue{
    node_t *head;
    size_t size;
    size_t memSize;
    q_mode_t mode;
}queue_t;


/**
 * @function create_queue
 * @brief Crea una nuova coda,con size uguale a 0
 * @param mode Modalita' funzionamento coda
 * @param memSize sizeof dell'info di ogni nodo
 * @return puntatore alla coda,altrimenti NULL  e setta errno.
 */
queue_t* create_queue(q_mode_t mode,size_t memSize);

/**
 * @function push_queue
 * @brief Inserisce il valore val,nella coda Q
 * @param Q puntatore alla coda
 * @param val valore da inserire
 * @return il valore appena inserito,altrimenti NULL e setta errno.
 */
void* push_queue(queue_t *Q,void* val);

/**
 * @function destroy_queue
 * @brief Dealloca la coda.
 * @param Q puntatore alla coda
 */
void destroy_queue(queue_t **Q);

/**
 * @function pop_queue
 * @brief Estrare un elemento dalla coda,in base alla sua modalita'
 * @param Q puntatore alla coda
 * @return valore dell'info del nodo estratto,altrimenti NULL e setta errno.
 * @note EINVAL e' settato se la coda e' vuota
 */
void* pop_queue(queue_t *Q);

#endif
