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

#define STRING_ERROR_SIZE 100

/* Strutture Dati utili */

/* Struttura per manternere statistiche sul server */
struct statistics chattyStats = { 0,0,0,0,0,0,0 };
pthread_mutex_t mtx_chatty_stat;//mutex per proteggere chattyStats

/* Utenti registrati di chatty */
utenti_registrati_t *chattyUtenti;

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

static int server_client_manager(void *buff,int fd,void* arg)
{
    return chatty_client_manager((message_t*)msg,int fd,(server_thread_argument_t*)arg);
}

/* CHATTY MAIN */

int main(int argc, char *argv[])
{
    server_t *server; //istanza del server
    server_config_t config; //variabile per salvare la configurazione attuale del server
    server_function_t funs; //funzioni utili per il server
    server_thread_argument_t thread_argument; //argomenti per i thread del pool

    int rc; //gestione ritorno funzioni
    int curr_error; //gestione errno
    char string_error[STRING_ERROR_SIZE] = NULL; //stringa errore da stampare

    //TODO parse argomenti

    //TODO Implementare file parser per la configurazione server

    //inizializzo mutex per statistiche
    rc = pthread_mutex_init(&mtx_chatty_stat,NULL);

    th_error_main(rc,"on init mutex");

    //inizializzo struttura utenti di chatty
    chattyUtenti = inizializzaUtentiRegistrati(&chattyStats,&mtx_chatty_stat,"/tmp/chatty");

    if(chattyUtenti == NULL)
    {
        string_error = "Chatty: fallita inizializzazione utenti";
        curr_error = errno;
        goto main_error1;
    }

    //inizializzo threda argument
    thread_argument.mtx_stat = &mtx_chatty_stat;
    thread_argument.stat = &chattyStats;
    thread_argument.utenti &chattyUtenti;

    //inizializzo e configuro il server
    server = init_server("tmp/server",sizeof(message_t),2);

    if(server == NULL)
    {
        string_error = "Chatty: fallita inizializzazione server";
        curr_error = errno;
        goto main_error2;
    }

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
    funs.arg_cmf = (void*)&chattyUtenti;

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

main error2:
    //elimino elenco utenti
    eliminaElenco(chattyUtenti);
    goto main_error1;

//errore handler
main_error1:
    //distruggo mutex statistiche
    pthread_mutex_destroy(&mtx_chatty_stat);

    //stampo errore
    errno = curr_error;
    perror(string_error);
    exit(EXIT_FAILURE);
}
