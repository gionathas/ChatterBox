/**
 * @file chatty_clients.c
 * @brief Implementazione delle funzioni per la gestione dei client da parte del server chatty.
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */

#include<stdlib.h>
#include<errno.h>
#include<pthread.h>
#include"message.h"
#include"chatty_task.h"
#include"config.h"
#include"ops.h"
#include"utenti.h"

#define CHATTY_THREAD_ERR_HANDLER(err,val,ret)         \
    do{if( (err) == (val)){return (ret);} }while(0)

/* size del buffer iniziale per la stringa degli utenti online */
#define INITAL_BUFFER 1000
/* byte per incrementare la size del buffer della stringa degli utenti online */
#define BUFFER_INCREMENT 500

/* FUNZIONI INTERFACCIA */

static int send_ok_message(int fd,char *buf,unsigned int len)
{
    message_t response;
    int rc;

    //setto header e data risposta, sender e receiver non ci interessano
    setHeader(&response.hdr,OP_OK,"");
    setData(&responde.data,"",buf,len);

    //mando header
    rc = sendHeader(fd,&response.hdr);

    //errore sendHeader
    CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

    //mando data
    rc = sendData(fd,&responde.data);

    //errore sendData
    CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

    return 0;
}

static int send_fail_message(int fd,op_t op_fail)
{
    message_t err_response; //risposta di errore al client
    int rc;

    //preparo risposta con op_fail
    setHeader(&err_response.hdr,op_fail,"");

    //invio risposta,solo header perche' e' un errore
    rc = sendHeader(fd,&err_response.hdr);

    //errore invio messaggio
    CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

    //incremento il numero di messaggi di errori inviati dal server

    //lock su stat
    rc = pthread_mutex_lock(chatty->mtx_stat);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //incremento contatore errori
    ++(chatty->stat.nerrors);

    //unlock stat
    pthread_mutex_unlock(chatty->mtx_stat);

    return 0;
}

static int sendUserOnline(int fd,utenti_registrati_t *utenti)
{
    char *user_online = NULL; //stringa dove salvare i nick degli utenti online
    size_t size = INITAL_BUFFER;
    int  count = -(BUFFER_INCREMENT); //byte in aggiunta alla stringa degli utenti online,si incrementa ogni volta di 100 byte
    unsigned int new_size; //nuova size della stringa dopo aver scritto i nick al suo interno
    int rc;
    message_t response; //risposta per il client

    do {
        new_size = 0;
        count += BUFFER_INCREMENT;

        //se la stringa e' stata gia' allocata,la libero per allocare piu' memoria
        if(user_online != NULL)
            free(user_online);

        //incremento il buffer
        size += count;
        //alloco spazio per stringa
        user_online = malloc(size * sizeof(char));

        //esito allocazione
        CHATTY_THREAD_ERR_HANDLER(user_online,NULL,-1);

        //inizializzo a stringa vuota
        user_online = "";

        rc = mostraUtentiOnline(user_online,&size,&new_size,utenti);

    } while(rc == -1 && errno == ENOBUFS)//fin quando non trovo una size adatta per la stringa

    //se sono fallito per altro
    CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

    //mando messaggio di ok,con la stringa degli utenti online
    return send_ok_message(fd,user_online,new_size);
}

int chatty_client_manager(message_t *message,int fd,server_thread_argument_t *chatty)
{
    int rc;
    char *sender = message->hdr.sender;
    char *receiver = message->data.hdr.receiver;
    op_t op = message->hdr.op;

    //analizzo richiesta del client
    switch (op)
    {
        case REGISTER_OP:

            rc = registraUtente(sender,fd,chatty->utenti);

            //se l'utente risulta GIA' essere registrato con quel nick
            if(rc == 1)
            {
                rc = send_fail_messagge(fd,OP_NICK_ALREADY);
            }

            //registrazione utente fallita o errore invio risposta
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            //tutto andato bene,mando messaggio di ok al client e lista di utenti connessi
            rc = sendUserOnline(fd,chatty->utenti);

            //errore operazione
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            break;

        case UNREGISTER_OP:

            rc = deregistraUtente(sender,chatty->utenti);

            //se l'utente risulta NON essere registrato con quel nick
            if(rc == 1)
            {
                rc = send_fail_messagge(fd,OP_NICK_UNKNOWN);
            }

            //deregistrazione utente fallita o errore invio risposta
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            //mando messaggio di OP_OK
            rc = send_ok_message(fd,"",0);

            //errore operazione
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            break;

        case CONNECT_OP:

            rc = connectUtente(sender,fd,chatty->utenti);

            //utente non trovato
            if(rc == 1)
            {
                rc = send_fail_messagge(fd,OP_NICK_UNKNOWN);
            }

            //connessione utente fallita o errore invio risposta
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            //invio OK,e la lista utenti
            rc = sendUserOnline(fd,chatty->utenti);

            //errore operazione
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            break;

        case DISCONNECT_OP :

            rc = disconnectUtente(sender,chatty->utenti);

            //utente non trovato
            if(rc == 1)
            {
                rc = send_fail_messagge(fd,OP_NICK_UNKNOWN);
            }

            //connessione utente fallita o errore invio risposta
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            //mando messaggio di OP_OK,qui il data Message non mi interessa
            rc = send_ok_message(fd,"", 0);

            //erore invio messaggio
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            break;

        case USRLIST_OP:

            //invio OK,e la lista utenti
            rc = sendUserOnline(fd,chatty->utenti);

            //errore operazione
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            break;

        case POSTTXT_OP:



        default:
            //TODO
    }

    //client gestito correttamente
    return 0;
}


int chatty_clients_overflow(int fd)
{
    //mando OP_FAIL per questo genere di errore
    return send_fail_messagge(fd,OP_FAIL);
}
