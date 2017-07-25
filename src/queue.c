/**
 * @file queue.c
 * @brief Implementazione di una coda generica.
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include"queue.h"
#include"utils.h"

/* FUNZIONI DI SUPPORTO */

/**
 * @function remove_head
 * @brief Elimina il nodo testa della coda
 * @param Q puntatore alla coda
 * @return valore dell'info del nodo eliminato,altrimenti NULL e setta errno.
 */
static void* remove_head(queue_t *Q);
/**
 * @function fifo_insert
 * @brief inserisce il nodo,secondo la regola FIFO
 * @param Q puntatore alla coda
 * @param node nodo da inserire
 * @return il valore del info del nodo appena inserito
 */
static void* fifo_insert(queue_t *Q,node_t *node);
/**
 * @function fifo_insert
 * @brief inserisce il nodo,secondo la regola LIFO
 * @param Q puntatore alla coda
 * @param node nodo da inserire
 * @return il valore del info del nodo appena inserito
 */
static void* lifo_insert(queue_t *Q,node_t *node);
/**
 * @function get_last_element
 * @brief Ritorna l'ultimo elemento della coda
 * @param Q puntatore alla coda
 * @return puntatore al ultimo nodo della coda.
 */
static node_t* get_last_element(queue_t *Q);

/* IMPLEMENTAZIONE FUNZIONI */

queue_t* create_queue(q_mode_t mode,size_t memSize)
{
    queue_t *new = (queue_t*)malloc(sizeof(queue_t));

    //controllo allocazione
    error_handler_1(new,NULL,NULL);

	new->memSize = memSize;
    new->head = NULL;
    new->size = 0;
    new->mode = mode;

    return new;
}

void destroy_queue(queue_t **Q)
{
    //controllo parametri
    if(*Q == NULL)
        return;

    //puntatori di supporto
    node_t *curr = (*Q)->head;
    node_t *succ = NULL;

    //scorro la coda, e dealloco ogni nodo
    while(curr != NULL)
    {
        succ = curr->next;
        free(curr->info);
        free(curr);
        curr = succ;
    }

    free(*Q);
    *Q=NULL;
}

void* push_queue(queue_t *Q,void* val)
{
    CHECK_QUEUE(Q);

    //creazione nuovo nodo
    node_t *newnode = (node_t*)malloc(sizeof(node_t));

    //controllo allocazione
    error_handler_1(newnode,NULL,NULL);

    newnode->info = malloc(Q->memSize);

    //controllo allocazione
    if(newnode->info == NULL)
    {
        free(newnode);
        return NULL;
    }

    //trasferisco i byte di val nell'info del nodo,a seconda della size del nodo.
    memcpy(newnode->info,val,Q->memSize);

    if(Q->mode == LIFO || Q->size == 0)
        return lifo_insert(Q,newnode);
    else
        return fifo_insert(Q,newnode);
}

static void* fifo_insert(queue_t *Q,node_t *node)
{
    node_t *last = get_last_element(Q);
    last->next = node;
    node->next = NULL;
    (Q->size)++;

    return node->info;
}

static node_t* get_last_element(queue_t *Q)
{
    node_t *curr = Q->head;

    while(curr->next != NULL)
    {
        curr = curr->next;
    }

    return curr;
}

static void* lifo_insert(queue_t *Q,node_t *node)
{
	node->next = Q->head;
    Q->head = node;
    (Q->size)++;

    return node->info;
}

void* pop_queue(queue_t *Q)
{
    CHECK_QUEUE(Q);

    //caso coda vuota
    error_handler_2(Q->size,0,NULL,EINVAL);

    //rimuove sempre il primo elemento
    return remove_head(Q);
}

static void* remove_head(queue_t *Q)
{
    //per salvare info del nodo che sto per eliminare
    void *data = malloc(Q->memSize);

    //controllo allocazione
    error_handler_1(data,NULL,NULL);

    //trasferisco info del nodo head in data,prima di elimarlo
    node_t *tmp = Q->head;
    memcpy(data,tmp->info,Q->memSize);

    Q->head = Q->head->next;

    //elimino nodo head
    free(tmp->info);
    free(tmp);

    (Q->size)--;

    return data;
}
