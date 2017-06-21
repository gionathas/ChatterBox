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
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
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
#include"utenti.h"

/* Strutture Dati utili */

/* Struttura per manternere statistiche sul server */
struct statistics chattyStats = { 0,0,0,0,0,0,0 };
pthread_mutex_t mtx_chatty_stat;//mutex per proteggere chattyStats

/* Per mantenere le info sugli utenti di chatty */
utenti_registrati_t *chattyUtenti;

/* Struttura argomenti per i thread del pool */
typedef struct{
    struct statistics *stat;
    pthread_mutex_t *mtx_stat;
    utenti_registrati_t *utenti;
}server_thread_argument_t

/* Funzioni di utilita' */

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

/* Ridenominazione delle funzioni vitali del server,per l'interfaccia di server.h */
static int server_read_message(int fd,void *mess)
{
    return readMsg((long)fd,(message_t*)mess);
}

static int server_signalusr1_handler(void* file)
{
    return printStats((FILE*)file);
}

static int server_client_overflow(int fd,void *arg)
{
    return chatty_clients_overflow(fd);
}


int main(int argc, char *argv[])
{
    server_t *server; //istanza del server
    server_config_t config; //variabile per salvare la configurazione attuale del server
    server_function_t funs; //funzioni utili per il server
    server_thread_argument_t thread_argument; //argomenti per i thread del pool

    int rc; //gestione ritorno funzioni

    //TODO parse argomenti

    //TODO Implementare file parser per la configurazione server

    //inizializzo mutex per statistiche
    rc = pthread_mutex_init(&mtx_chatty_stat,NULL);

    th_error_main(rc,"on init mutex");

    //inizializzo struttura utenti di chatty
    chattyUtenti = inizializzaUtentiRegistrati(MAX_USERS,&chattyStats.nusers,&chatty.nonline,&mtx_chatty_stat);

    if(chattyUtenti == NULL)
    {
        exit(EXIT_FAILURE);
    }

    //inizializzo threda argument
    thread_argument.mtx_stat = &mtx_chatty_stat;
    thread_argument.stat = &chattyStats;
    thread_argument.utenti &chattyUtenti;

    //inizializzo e configuro il server
    server = init_server("tmp/server",sizeof(message_t),2);

    //controllo esito di init server
    err_null_ex(server,"MAIN: on init server");

    //inizializzo funzioni per il server

    //funzione per lettura messaggio
    funs.read_message_fun = server_read_message;
    //funzione per gestire troppi client,quest'ultima viene utilizzata dal listener
    funs.client_out_of_bound = server_client_overflow;
    //argomennti per la precedente funzione
    funs.arg_cob = NULL;
    //funzione per gestire l'arrivo del segnale USR1
    funs.signal_usr_handler = server_signalusr1_handler;
    //argomenti per la funzione precedente
    funs.arg_suh = NULL;
    //funzione per gestire comunicazione con il client
    funs.client_manager_fun = //TODO;
    funs.arg_cmf = (void*)&thread_argument;

    //faccio partire il server,da ora in poi posso terminarlo solo con un segnale
    rc = start_server(server,2,funs);

    //TODO fare la destroy dei mutex degli utenti e delle statistiche.Si puo fare con cleanup di exit.

    //controllo esito terminazione server.
    if(rc == -1 || rc == 1)
    {
        //caso di errore non riguardante il fallimento di uno o piu thread
        if(rc == -1)
        {
            perror("Server Fail");
        }

        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
