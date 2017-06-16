/**
 * @file chatty_task.h
 * @brief Header delle funzioni lato server utilizzate dal server chatty
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef CHATTY_TASK_H_
#define CHATTY_TASK_H_

void chatty_parser();
void chatty_client_manager();
//gestione del caso troppi client,e' un caso che gestisce il listener
void chatty_clients_overflow();

#endif /* CHATTY_TASK_H_ */
