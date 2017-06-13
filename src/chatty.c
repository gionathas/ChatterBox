/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 */
#define _POSIX_C_SOURCE 200809L
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<signal.h>
#include<pthread.h>
#include"connections.h"
#include"server.h"
#include"message.h"
#include"macro_error.h"
#include"chatty_task.h"
#include"stats.h"


/* struttura che memorizza le statistiche del server, struct statistics
 * e' definita in stats.h.
 *
 */
struct statistics  chattyStats = { 0,0,0,0,0,0,0 };


static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

/* Ridenominazione delle funzioni vitali del server,per l'interfaccia di server.h */
static int server_read_message(int fd,void *mess)
{
    return readMsg((long)fd,(message_t*)msg);
}

static server_signalusr1_handler(void* file)
{
    return printStats((FILE*)file);
}

int main(int argc, char *argv[])
{
    server_t *server; //istanza del server
    int rc;

    //TODO parse argomenti

    //TODO Implementare file parser per la configurazione server

    //inizializzo e configuro il server
    server = init_server("tmp/server",sizeof(message_t),2);

    err_null_ex(server,"MAIN: on init server");

    //faccio partire il server,da ora in poi posso terminarlo solo con un segnale
    rc = start_server(server,2,funs);

    err_meno1_ex(rc,"MAIN: server");

    return EXIT_SUCCESS;
}
