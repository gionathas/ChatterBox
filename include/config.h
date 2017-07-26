/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */

/**
 * @file config.h
 * @brief File contenente alcune define con valori massimi utilizzabili
 *        e strutture dati utili per la configurazione del server.
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */

#if !defined(CONFIG_H_)
#define CONFIG_H_

#include"stats.h"

/* Numero caratteri massimo dei nickname */
#define MAX_NAME_LENGTH 32
/* Numero caratteri massimo dei path  */
#define UNIX_PATH_MAX 80
/* Numero caratteri massimo di una riga del file di conf */
#define MAX_SIZE_LINE 160
/* Numero caratteri massimo di un campo all'interno di unra riga del file di config */
#define MAX_SIZE_FIELD 65
/* Numero massimo di utenti registrabili  */
#define MAX_USERS 20
/* Numero massimo di caratteri per l'identificatore di un file o messaggio */
#define MAX_ID_LENGTH 2
/* Numero massimo di caratteri per il path della directory degli utenti */
#define MAX_SERVER_DIR_LENGTH (UNIX_PATH_MAX - (MAX_NAME_LENGTH  + 1) - MAX_ID_LENGTH)
/* Numero massimo di caratteri per il path della directory personale di un utente */
#define MAX_CLIENT_DIR_LENGHT (MAX_SERVER_DIR_LENGTH + (MAX_NAME_LENGTH + 1) )
/* Numero massimo di volte in cui il client puo' ritentare la connessione al server */
#define MAX_RETRIES 10
/* Secondi da attendere tra un retry e il successivo */
#define MAX_SLEEPING 3

/**
 * @struct server_config_t
 * @brief Struttura per la configurazione del server chatty
 * @var serverpath path socket di chatty
 * @var max_connection numero max connessioni ammesse
 * @var thread numero di thread del pool del server
 * @var max_msg_size dimensione massima di un messaggio testuale
 * @var max_file_size dimensione massima di un file
 * @var max_hist_msgs numero massimo di messaggi che vengono salvati per ogni utente
 * @var dirname path della directory del server chatty
 * @var stat_file_name path del file delle statistiche del server
 */
typedef struct{
    char serverpath[UNIX_PATH_MAX + 1];
    unsigned int max_connection;
    unsigned int threads;
    size_t max_msg_size;
    size_t max_file_size;
    unsigned int max_hist_msgs;
    char dirname[MAX_SERVER_DIR_LENGTH + 1];
    char stat_file_name[UNIX_PATH_MAX + 1];
}server_config_t;

// to avoid warnings like "ISO C forbids an empty translation unit"x
typedef int make_iso_compilers_happy;

/* Stampa la configurazione attuale del server */
static inline void print_config(server_config_t conf)
{
    printf("ServerPath = %s\n",conf.serverpath);
    printf("MaxConnections = %d\n",conf.max_connection);
    printf("threads = %d\n",conf.threads);
    printf("MaxMsgSize = %ld\n",conf.max_msg_size);
    printf("MaxFileSize = %ld\n",conf.max_file_size);
    printf("MaxHistMsgs = %d\n",conf.max_hist_msgs);
    printf("DirName = %s\n",conf.dirname);
    printf("StatFileName = %s\n",conf.stat_file_name);
}


#endif /* CONFIG_H_ */
