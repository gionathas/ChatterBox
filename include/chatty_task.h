/**
 * @file chatty_task.h
 * @brief Header delle funzioni lato server utilizzate dal server chatty
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef CHATTY_TASK_H_
#define CHATTY_TASK_H_

#include"config.h"
#include"utenti.h"
#include "gruppi.h"
#include"message.h"

//argomenti da passare alle funzioni del server di chatty
typedef struct{
    utenti_registrati_t *utenti;
    gruppi_registrati_t *gruppi;
}chatty_arg_t;

/**
 * @function chatty_parser
 * @brief Parser del file di configurazione del server
 * @param pathfile path del file di config
 * @param config puntatore alla struttura dove salvare la configurazione
 * @return 0 configurazione accettata,altrimenti -1 e setta errno
 */
int chatty_parser(char *pathfile,server_config_t *config);

/**
 * @function chatty_client_manager
 * @brief Gestione di una richiesta di un client
 * @param message messaggio di richiesta del client
 * @param fd fd del client
 * @param utenti elenco utenti registrati
 * @return 0 richiesta gestita correttamente,altrimenti -1 e setta errno.
 */
int chatty_client_manager(message_t *message,int fd,chatty_arg_t *chatty);

/**
 * @function chatty_clients_overflow
 * @brief Gestione del caso in cui il numero massimo di client viene suerato
 * @param fd del client in eccesso
 * @param utenti utenti elenco registrati
 * @return 0 caso gestito correttamente,altrimenti -1 e setta errno.
 */
int chatty_clients_overflow(int fd,utenti_registrati_t *utenti);

/**
 * @function chatty_disconnect_client
 * @brief Effettua la disconnessione di un client del server chatty
 * @param fd fd del client
 * @param utenti elenco utenti registrati a chatty
 * @return 0 client disconnesso correttamente,altrimenti -1 e setta errno
 */
int chatty_disconnect_client(int fd,utenti_registrati_t *utenti);
#endif /* CHATTY_TASK_H_ */
