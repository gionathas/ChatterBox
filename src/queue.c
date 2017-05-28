#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include"queue.h"

static void* remove_head(queue_t *Q);
static void* fifo_insert(queue_t *Q,node_t *node);
static void* lifo_insert(queue_t *Q,node_t *node);
static node_t* get_last_element(queue_t *Q);

queue_t* create_queue(q_mode_t mode,size_t memSize)
{
    queue_t *new = (queue_t*)malloc(sizeof(queue_t));

    if(new == NULL)
    {
    	return NULL;
	}

	new->memSize = memSize;
    new->head = NULL;
    new->size = 0;
    new->mode = mode;

    return new;
}

void destroy_queue(queue_t **Q)
{
    if(*Q == NULL)
        return;

    node_t *curr = (*Q)->head;
    node_t *succ = NULL;

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

    if(newnode == NULL)
    {
    	return NULL;
    }

    newnode->info = malloc(Q->memSize);

    if(newnode->info == NULL)
    {
        free(newnode);
        return NULL;
    }

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

    //coda vuota
    if(Q->size == 0)
    {
    	errno = EINVAL;
        return NULL;
	}
    //rimuove sempre il primo elemento
    return remove_head(Q);
}

static void* remove_head(queue_t *Q)
{
    void *data = malloc(Q->memSize);

    if(data == NULL)
        return NULL;

    node_t *tmp = Q->head;
    memcpy(data,tmp->info,Q->memSize);

    Q->head = Q->head->next;

    free(tmp->info);
    free(tmp);

    (Q->size)--;

    return data;
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
