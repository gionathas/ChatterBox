/*
 * Implentazione di una coda con valori generici
 */

#ifndef QUEUE_H
#define QUEUE_H

#include<stdlib.h>
#include<stdio.h>
#include<errno.h>

typedef enum queue_mode{
    FIFO,
    LIFO
}q_mode_t;

/*
 *  Macro che controlla se una lista esiste
 */
#define CHECK_QUEUE(L)                              \
    if((L) == NULL)                                 \
    {               								\
    	errno = EINVAL;                             \
        return NULL;                                \
    }


typedef struct node{
    void* info;
    struct node *next;
}node_t;

typedef struct queue{
    node_t *head;
    size_t size;
    size_t memSize;
    q_mode_t mode;
}queue_t;


/*
 *  Crea una coda vuota,ovvero una coda con
 *  size uguale a 0, la testa uguale a NULL,
 *  con gestione della coda di tipo mode,e il tipo di
 *  dato memSize.
 *  In caso di errore ritorna NULL, e setta errno.
 */
queue_t* create_queue(q_mode_t mode,size_t memSize);

/*
 * Inserisce val all'interno della coda L,
 * in base alla sua modalita'.
 * incrementa la size della coda.
 * Se la coda non esiste,ritorna NULL e setta errno.
 * In caso non ci siano errori ritorna il valore appena inserito.
 */
void* push_queue(queue_t *Q,void* val);

/*
 * Elimina tutta la coda.
 */
void destroy_queue(queue_t **Q);

/*
 * Rimuove un elemento dalla coda,in base
 * alla modalita' della coda.
 * On success,ritorna il valore (con puntatore a void*)
 * che e' stato estratto. Dovra' poi essere castato.
 * Ritorna NULL se la coda non esiste oppure se e' vuota,
 * e setta errno.
 */
void* pop_queue(queue_t *Q);

#endif
