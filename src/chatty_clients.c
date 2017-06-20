/**
 * @file chatty_clients.c
 * @brief Implementazione delle funzioni per la gestione dei client da parte del server chatty.
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */

 #include"message.h"
 #include"chatty_task.h"

/* FUNZIONI INTERFACCIA */

int chatty_client_manager(int fd)
{
    //TODO
    return 0;
}


int chatty_clients_overflow(int fd)
{
     message_hdr_t msg;
     int rc;

     //essendo un caso di fallimento mando solo l'header

     //sender non importante per questo tipo di messaggio.
     setHeader(&msg,OP_FAIL,"");

     rc = sendHeader(fd,&msg);

     return rc;
 }
