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

int chatty_parser(char *pathfile,server_config_t *config);
int chatty_client_manager(int fd);
//gestione del caso troppi client,e' un caso che gestisce il listener
int chatty_clients_overflow(int fd);

#endif /* CHATTY_TASK_H_ */
