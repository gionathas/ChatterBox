#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include"utenti.h"
#include"config.h"
#include"message.h"
#include"connections.h"

/* Numero di byte che servono per memorizzare in una stringa il tipo di un messaggio, in un file */
#define MSG_TYPE_SPACE 15
/* Numero di byte che servono per memorizzare in una stringa la size di un messaggio, in un file */
#define MSG_SIZE_SPACE 6

int send_ok_message(int fd,char *buf,unsigned int len)
{
    message_t response;
    int rc;

    //setto header e data risposta, sender e receiver non ci interessano
    setHeader(&response.hdr,OP_OK,"");
    setData(&response.data,"",buf,len);

    //mando header
    rc = sendHeader(fd,&response.hdr);

    //errore sendHeader
    USER_ERR_HANDLER(rc,-1,-1);

    //mando data
    rc = sendData(fd,&response.data);

    //errore sendData
    USER_ERR_HANDLER(rc,-1,-1);

    return 0;
}

int send_fail_message(int fd,op_t op_fail,utenti_registrati_t *utenti)
{
    message_t err_response; //risposta di errore al client
    int rc;

    //preparo risposta con op_fail
    setHeader(&err_response.hdr,op_fail,"");

    //invio risposta,solo header perche' e' un errore
    rc = sendHeader(fd,&err_response.hdr);

    //errore invio messaggio
    USER_ERR_HANDLER(rc,-1,-1);

    //incremento il numero di messaggi di errori inviati dal server

    //lock su stat
    rc = pthread_mutex_lock(utenti->mtx_stat);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //incremento contatore errori
    ++(utenti->stat->nerrors);

    //unlock stat
    pthread_mutex_unlock(utenti->mtx_stat);

    return 0;
}

utente_t *checkSender(char *sender_name,utenti_registrati_t *utenti)
{
    //controllo sender sia registrato
    utente_t *sender = cercaUtente(sender_name,utenti);

    //sender non valido o non registrato o errore
    USER_ERR_HANDLER(sender,NULL,NULL);

    //controllo sender sia online
    if(sender->isOnline == 0)
    {
        //rilascio lock sender
        pthread_mutex_unlock(&sender->mtx);

        errno = ENETDOWN;
        return NULL;
    }

    return sender;
}

//ritorna file aperto per sciverci,altrimenti NULL e setta errno
static FILE *create_message_file(int id,message_t *msg,char *dir_path)
{
    char msg_id[MAX_ID_LENGTH + 1] = ""; //per memorizzare id sottoforma di stringa

    //trasformo l'id in stringa
    snprintf(msg_id,MAX_ID_LENGTH + 1,"%d",id);

    char path_file[UNIX_PATH_MAX] = ""; //path del file

    //creo il path del file
    snprintf(path_file,UNIX_PATH_MAX,"%s/%s",dir_path,msg_id);

    //creo il file,se esiste gia' verra' sovrascritto
    FILE *file = fopen(path_file,"w");

    return file;
}

static unsigned int generate_id_message(utente_t *utente,utenti_registrati_t *utenti)
{
    unsigned int id;

    //se la directory receiver piena
    if(utente->n_element_in_dir == utenti->max_hist_msgs)
    {
        //dato che tutti i messaggi hanno la stessa importanza,ne scegliamo una a caso da sovrascrivere
        srand(time(NULL));
        id = (rand() % utenti->max_hist_msgs) + 1;
    }
    else{
        id = utente->n_element_in_dir + 1;
        //incremento numero di messaggi nella directory del receiver
        ++(utente->n_element_in_dir);
    }

    return id;
}

static void write_on_file(FILE *file,message_t *msg)
{
    char type[MSG_TYPE_SPACE] = ""; //per memorizzare in una stringa il tipo del messaggio
    char size_buf[MSG_SIZE_SPACE] = ""; //per memorizzare in una stringa la size del messaggio

    //scrivo il tipo del messaggio
    if(msg->hdr.op == TXT_MESSAGE)
        snprintf(type,MSG_TYPE_SPACE,"Text");
    else
        snprintf(type,MSG_TYPE_SPACE,"File");

    //scrivo la size del messaggio
    snprintf(size_buf,MSG_SIZE_SPACE,"%d",msg->data.hdr.len);

    //scrivo sul file
    fprintf(file,"%s\n%s\n%s\n%s\n",msg->hdr.sender,type,size_buf,msg->data.buf);
}

static int send_text_message(utente_t *receiver,message_t *text_message,utenti_registrati_t *utenti)
{
    int rc;

    //receiver online,posso mandargli direttamente il messaggio
    if(receiver->isOnline)
    {
        rc = sendHeader(receiver->fd,&text_message->hdr);

        //errore invio header
        USER_ERR_HANDLER(rc,-1,-1);

        rc = sendData(receiver->fd,&text_message->data);

        //errore invio data
        USER_ERR_HANDLER(rc,-1,-1);

        //nessun errore riscontrato,incremento numero di messaggi inviati
        rc = pthread_mutex_lock(utenti->mtx_stat);

        //errore lock
        if(rc)
        {
            errno = rc;
            return -1;
        }

        ++(utenti->stat->ndelivered);

        //rilascio lock
        pthread_mutex_unlock(utenti->mtx_stat);

    }
    //se non e' online,dobbiamo salvarlo nella directory personale del receiver,sottoforma di file
    else
    {
        int id;//identificativo del messaggio

        //creo identificatore messaggio
        id = generate_id_message(receiver,utenti);

        //creo il file messaggio da inviare
        FILE *file_msg = create_message_file(id,text_message,receiver->personal_dir);

        //errore creazione file
        if(file_msg == NULL)
            return -1;

        //scrivo il messaggio sul file
        write_on_file(file_msg,text_message);

        fclose(file_msg);

        //nessun errore riscontrato,incremento numero di messaggi non ancora consegnati
        rc = pthread_mutex_lock(utenti->mtx_stat);

        //errore lock
        if(rc)
        {
            errno = rc;
            return -1;
        }

        ++(utenti->stat->nnotdelivered);

        //rilascio lock
        pthread_mutex_unlock(utenti->mtx_stat);
    }

    return 0;
}

//, -1 errore,,0 ok
int inviaMessaggioUtente(char *sender_name,char *receiver_name,char *msg,size_t size_msg,utenti_registrati_t *utenti)
{
    int rc;

    //controllo sender sia registrato ed online
    utente_t *sender = checkSender(sender_name,utenti);

    if(sender == NULL)
    {
        //se il nome del sender non e' valido,oppure il sender non e' online,oppure non e' registrato
        if(errno == EPERM || errno == ENETDOWN || errno == 0)
        {
            rc = send_fail_message(sender->fd,OP_FAIL,utenti);
        }
        //errore checkSender
        else{
            return -1;
        }
    }
    //controllo dimensione messaggio
    else if(size_msg > utenti->max_msg_size)
    {
        //invio errore di messaggio troppo grande
        rc = send_fail_message(sender->fd,OP_MSG_TOOLONG,utenti);
    }
    //altrimenti sender valido e messaggio valido
    else{

        //controllo stato del receiver: Deve essere solo registrato
        utente_t *receiver = cercaUtente(receiver_name,utenti);

        //receiver non valido
        if(receiver == NULL)
        {
            //receiver non registrato oppure nome receiver invalido
            if(errno == 0 || errno == EPERM)
            {
                rc = send_fail_message(sender->fd,OP_FAIL,utenti);
            }
            //errore ricerca receiver
            else{
                return -1;
            }
        }
        //receiver valido
        else
        {
            //preparo messaggio
            message_t txt_message;

            setHeader(&txt_message.hdr,TXT_MESSAGE,sender->nickname);
            setData(&txt_message.data,receiver->nickname,msg,size_msg);

            rc = send_text_message(receiver,&txt_message,utenti);

            //rilascio lock receiver
            pthread_mutex_unlock(&receiver->mtx);

            //errore in send_text_message
            if(rc == -1)
            {
                //rilasciare lock del sender
                pthread_mutex_unlock(&sender->mtx);

                return -1;
            }

            //se non ci sono stati errori mando risposta al sender di OP_OK
            rc = send_ok_message(sender->fd,"",0);

            //solo ora posso rilasciare lock del sender
            pthread_mutex_unlock(&sender->mtx);

        }
    }

    //controllo esito invio messaggio
    USER_ERR_HANDLER(rc,-1,-1);

    return 0;
}
