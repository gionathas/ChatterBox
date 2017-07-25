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

/* Struttura per mantenere statistiche sul server */
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

static int server_signalusr1_handler(void* path_file)
{
    return printStats((char*)path_file);
}

static int server_client_overflow(int fd,void *utenti)
{
    return chatty_clients_overflow(fd,(utenti_registrati_t*)utenti);
}

static int server_client_manager(void *msg,int fd,void* chatty_utenti)
{
    return chatty_client_manager((message_t*)msg,fd,(utenti_registrati_t*)chatty_utenti);
}

static int server_disconnect_client(int fd,void *utenti)
{
    return chatty_disconnect_client(fd,(utenti_registrati_t*)utenti);
}

/* Distruzione strutture utilizzate da chatty */
static int chatty_close()
{
    int rc;

    //elimino tutto elenco degli utenti registrati
    rc = eliminaElenco(chattyUtenti);

    rc = pthread_mutex_destroy(&mtx_chatty_stat);

    //ritorno esito operazioni
    return rc;
}

/* CHATTY MAIN */

int main(int argc, char *argv[])
{
    server_t *server; //istanza del server
    server_config_t config; //variabile per salvare la configurazione attuale del server
    server_function_t funs; //funzioni utili per il server
    int c;// di supporto al parsing degli argomenti
    char *config_path = NULL;//path del file che contiene la configurazione del server

    int rc; //gestione ritorno funzioni
    int curr_error; //gestione errno
    char string_error[STRING_ERROR_SIZE] = ""; //stringa errore da stampare

    //parsing argomenti per chatty,dobbiamo parsare solo un argomento: -f
    c = getopt(argc,argv,"f:");

    //controllo valore parsato
    switch(c)
    {
        case 'f':
            config_path = optarg;
            break;

        case '?':
        case -1:
            usage(argv[0]);
            exit(EXIT_FAILURE);

    }

    rc = chatty_parser(config_path,&config);

    //errore nel settaggio della configurazione
    err_meno1_ex(rc,"Chatty: Errore nel file di configurazione");

    //inizializzo mutex per statistiche
    rc = pthread_mutex_init(&mtx_chatty_stat,NULL);

    th_error_main(rc,"Chatty: Errore nell'inizializzazione del mutex statistiche");

    //inizializzo struttura utenti di chatty
    chattyUtenti = inizializzaUtentiRegistrati(config.max_msg_size,config.max_file_size,config.max_hist_msgs,&chattyStats,&mtx_chatty_stat,config.dirname);

    if(chattyUtenti == NULL)
    {
        snprintf(string_error,STRING_ERROR_SIZE,"Chatty: Errore inizializzazione utenti");
        curr_error = errno;
        goto main_error1;
    }

    //inizializzo e configuro il server
    server = init_server(config.serverpath,sizeof(message_t),config.max_connection);

    if(server == NULL)
    {
        snprintf(string_error,STRING_ERROR_SIZE,"Chatty: Errore inizializzazione server");
        curr_error = errno;
        goto main_error2;
    }

    //inizializzo funzioni per il server

    //funzione per lettura messaggio
    funs.read_message_fun = server_read_message;
    //funzione per gestire troppi client,quest'ultima viene utilizzata dal listener
    funs.client_out_of_bound = server_client_overflow;
    //argomennti per la precedente funzione
    funs.arg_cob = (void*)chattyUtenti;
    //funzione per gestire l'arrivo del segnale USR1
    funs.signal_usr_handler = server_signalusr1_handler;
    //argomenti per la funzione precedente
    funs.arg_suh = config.stat_file_name;
    //funzione per gestire comunicazione con il client
    funs.client_manager_fun = server_client_manager;
    funs.arg_cmf = (void*)chattyUtenti;
    //funzione per la disconnessione di un client
    funs.disconnect_client = server_disconnect_client;
    funs.arg_dc = (void*)chattyUtenti;

    //faccio partire il server,da ora in poi posso terminarlo solo con un segnale
    rc = start_server(server,1,funs);

    //controllo esito terminazione server.
    if(rc == -1 || rc > 0)
    {
        //caso di errore NON riguardante il fallimento di uno o piu thread
        if(rc == -1)
        {
            //setto messaggio errore
            snprintf(string_error,STRING_ERROR_SIZE,"Chatty: Server terminato con errore");
            curr_error = errno;
            goto main_error2;
        }
        //un thread del pool del server e' terminato erroneamente
        else{
            snprintf(string_error,STRING_ERROR_SIZE,"Chatty: Un thread del server e' terminato con errore");
            curr_error = rc;
            goto main_error2;
        }
    }

    //Se il server e' terminato correttamente,termino normalmente
    rc = chatty_close();

    //chiusura fallita
    if(rc == -1)
    {
        perror("Chatty: Errore in chiusura");
        exit(EXIT_FAILURE);
    }

    //terminato correttamente
    exit(EXIT_SUCCESS);

/* A seguire error handler di chatty,se non sono stati riscontrati errori,non si arriva mai qui */
main_error2:
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
