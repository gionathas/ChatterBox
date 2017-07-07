#ifndef MESS_UTENTI_H_
#define MESS_UTENTI_H_

#include<stdlib.h>
#include"ops.h"

//per identificare il tipo del messaggio
typedef enum{
    TEXT_ID,
    FILE_ID
}messaggio_id_t;

int send_ok_message(int fd,char *buf,unsigned int len);
int send_fail_message(int fd,op_t op_fail,utenti_registrati_t *utenti);
int inviaMessaggioUtente(char *sender,char *receiver,char *msg,size_t size_msg,messaggio_id_t type,utenti_registrati_t *utenti);
int inviaMessaggioUtentiRegistrati(char *sender_name,char *msg,size_t size_msg,utenti_registrati_t *utenti);
utente_t *checkSender(char *sender_name,utenti_registrati_t *utenti,int *pos);
int sendUserOnline(int fd,utenti_registrati_t *utenti);
int getFile(char *sender_name,char *filename,utenti_registrati_t *utenti);

#endif /*MESS_UTENTI_H_ */
