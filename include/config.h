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

/* Numero caratteri massimo dei nickname */
#define MAX_NAME_LENGTH 32
/* Numero caratteri massimo dei path  */
#define UNIX_PATH_MAX 65
/* Numero caratteri massimo di una riga del file di conf */
#define MAX_SIZE_LINE 160
/* Numero caratteri massimo di un campo all'interno di unra riga del file di config */
#define MAX_SIZE_FIELD 65


//Salva la configurazione attuale del server
typedef struct{
    char serverpath[UNIX_PATH_MAX];
    unsigned int max_connection;
    unsigned int threads;
    size_t max_msg_size;
    size_t max_file_size;
    unsigned int max_hist_msgs;
    char dirname[UNIX_PATH_MAX];
    char stat_file_name[UNIX_PATH_MAX];
}server_config_t;

// to avoid warnings like "ISO C forbids an empty translation unit"x
typedef int make_iso_compilers_happy;

void print_config(server_config_t conf)
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
