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
#include"messaggi_utenti.h"

#define CHATTY_THREAD_ERR_HANDLER(err,val,ret)         \
    do{if( (err) == (val)){return (ret);} }while(0)

/* size del buffer iniziale per la stringa degli utenti online */
#define INITAL_BUFFER 1000
/* byte per incrementare la size del buffer della stringa degli utenti online */
#define BUFFER_INCREMENT 500

/* FUNZIONI INTERFACCIA */
static int sendUserOnline(int fd,utenti_registrati_t *utenti)
{
    char *user_online = NULL; //stringa dove salvare i nick degli utenti online
    size_t size = INITAL_BUFFER;
    int  count = -(BUFFER_INCREMENT); //byte in aggiunta alla stringa degli utenti online,si incrementa ogni volta di 100 byte
    int new_size; //nuova size della stringa dopo aver scritto i nick al suo interno
    int rc;

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

    } while(rc == -1 && errno == ENOBUFS);//fin quando non trovo una size adatta per la stringa

    //se sono fallito per altro
    CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

    //mando messaggio di ok,con la stringa degli utenti online
    return send_ok_message(fd,user_online,new_size);
}


int chatty_client_manager(message_t *message,int fd,utenti_registrati_t *utenti)
{
    int rc;
    char *sender_name = message->hdr.sender;
    char *receiver_name = message->data.hdr.receiver;
    op_t op = message->hdr.op;
    utente_t *sender;

    //analizzo richiesta del client
    switch (op)
    {
        case REGISTER_OP:

            rc = registraUtente(sender_name,fd,utenti);

            //se l'utente risulta GIA' essere registrato con quel nick
            if(rc == 1)
            {
                rc = send_fail_message(fd,OP_NICK_ALREADY);
            }
            //registrazione utente fallita..
            else if(rc == -1)
            {
                //..nome non valido o elenco utenti pieno
                if(errno == EPERM || errno == ENOMEM)
                {
                    rc = send_fail_message(fd,OP_FAIL);
                }
                //..errore interno registrazione
                else{
                    return -1;
                }
            }
            //tutto andato bene,mando messaggio di ok al client e lista di utenti connessi
            else{
                rc = sendUserOnline(fd,utenti);
            }

            //errore operazione precedente
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            break;

        case UNREGISTER_OP:

            rc = deregistraUtente(sender_name,utenti);

            //se l'utente risulta NON essere registrato con quel nick
            if(rc == 1)
            {
                rc = send_fail_message(fd,OP_NICK_UNKNOWN);
            }
            //registrazione utente fallita
            else if(rc == -1)
            {
                //..nome non valido
                if(errno == EPERM)
                {
                    rc = send_fail_message(fd,OP_FAIL);
                }
                //..errore interno deregistrazione
                else{
                    return -1;
                }
            }
            //operazione corretta, mando messaggio di OP_OK
            else{
                rc = send_ok_message(fd,"",0);
            }

            //errore operazione precedente
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            break;

        case CONNECT_OP:

            rc = connectUtente(sender_name,fd,utenti);

            //utente non trovato
            if(rc == 1)
            {
                rc = send_fail_message(fd,OP_NICK_UNKNOWN);
            }
            //connessione utente fallita
            else if(rc == -1)
            {
                //..nome non valido o utente gia' connesso
                if(errno == EPERM)
                {
                    rc = send_fail_message(fd,OP_FAIL);
                }
                //..errore interno connessione
                else{
                    return -1;
                }
            }
            //invio OK,e la lista utenti
            else{
                rc = sendUserOnline(fd,utenti);
            }

            //errore operazione precedente
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            break;

        case DISCONNECT_OP :

            rc = disconnectUtente(sender_name,utenti);

            //utente non registrato
            if(rc == 1)
            {
                rc = send_fail_message(fd,OP_NICK_UNKNOWN);
            }
            //disconnessione utente fallita
            else if(rc == -1)
            {
                //..nome non valido o utente gia disconesso
                if(errno == EPERM)
                {
                    rc = send_fail_message(fd,OP_FAIL);
                }
                //..errore interno disconnessione
                else{
                    return -1;
                }
            }
            else{
                //mando messaggio di OP_OK,qui il data Message non mi interessa
                rc = send_ok_message(fd,"", 0);
            }

            //errore operazione precedente
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            break;

        case USRLIST_OP:

            //controllo sender sia registrato ed online
            sender = checkSender(sender_name,utenti);

            if(sender == NULL)
            {
                //se il nome del sender non e' valido,oppure il sender non e' online,oppure non e' registrato
                if(errno == EPERM || errno == ENETDOWN || errno == 0)
                {
                    rc = send_fail_message(fd,OP_FAIL);
                }
                //errore checkSender
                else{
                    return -1;
                }
            }
            //altrimenti sender valido,invio risposta e rilascio lock dell'utente
            else{
                pthread_mutex_unlock(&sender->mtx);
                rc = sendUserOnline(fd,utenti);
            }

            //controllo esito messaggio inviato
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

            break;

        case POSTTXT_OP:

            rc = inviaMessaggioUtente(sender_name,receiver_name,message->data.buf,message->data.hdr.len,utenti);

            //in caso di errore
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);

        default:

            rc = send_fail_message(fd,OP_FAIL);

            //controllo esito messaggio inviato
            CHATTY_THREAD_ERR_HANDLER(rc,-1,-1);
    }

    //client gestito correttamente
    return 0;
}


int chatty_clients_overflow(int fd)
{
    //mando OP_FAIL per questo genere di errore
    return send_fail_message(fd,OP_FAIL);
}
