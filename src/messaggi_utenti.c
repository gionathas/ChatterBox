#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include"utenti.h"
#include"config.h"
#include"message.h"
#include"connections.h"
#include"messaggi_utenti.h"

/* Numero di byte che servono per memorizzare in una stringa il tipo di un messaggio, in un file */
#define MSG_TYPE_SPACE 15
/* Numero di byte che servono per memorizzare in una stringa la size di un messaggio, in un file */
#define MSG_SIZE_SPACE 6
/* size del buffer iniziale per la stringa degli utenti online */
#define INITAL_BUFFER 1000
/* byte per incrementare la size del buffer della stringa degli utenti online */
#define BUFFER_INCREMENT 500

//per questo tipo di messaggio sender e receiver non ci interessano
int send_ok_message(int fd,char *buf,unsigned int len)
{
    message_t response;
    int rc;

    //setto header, sender e receiver non ci interessano
    setHeader(&response.hdr,OP_OK,"");

    //mando header
    rc = sendHeader(fd,&response.hdr);

    //errore sendHeader
    USER_ERR_HANDLER(rc,-1,-1);

    //se il buffer non e' vuoto,allora inviamo anche il la data del messaggio
    if(len != 0 || strcmp(buf,"") != 0)
    {
        setData(&response.data,"",buf,len);
        //mando data
        rc = sendData(fd,&response.data);

        //errore sendData
        USER_ERR_HANDLER(rc,-1,-1);
    }

    return rc;
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

utente_t *checkSender(char *sender_name,utenti_registrati_t *utenti,int *pos)
{
    //controllo sender sia registrato
    utente_t *sender = cercaUtente(sender_name,utenti,pos);

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

int sendUserOnline(int fd,utenti_registrati_t *utenti)
{
    char user_online[MAX_NAME_LENGTH * MAX_USERS]; //stringa dove salvare i nick degli utenti online
    size_t size = MAX_NAME_LENGTH * MAX_USERS;
    //int  count = -(BUFFER_INCREMENT); //byte in aggiunta alla stringa degli utenti online,si incrementa ogni volta di 100 byte
    int new_size = 0; //nuova size della stringa dopo aver scritto i nick al suo interno
    int rc;

    rc = mostraUtentiOnline(user_online,&size,&new_size,utenti);

    //se sono fallito per altro
    USER_ERR_HANDLER(rc,-1,-1);

    //mando messaggio di ok,con la stringa degli utenti online
    rc = send_ok_message(fd,user_online,new_size);

    return rc;

}

//ritorna file aperto per sciverci,altrimenti NULL e setta errno
static FILE *create_message_file(int id,message_t *msg,char *dir_path)
{
    int rc;
    char msg_id[MAX_ID_LENGTH + 1]; //per memorizzare id sottoforma di stringa

    //trasformo l'id in stringa
    rc = snprintf(msg_id,MAX_ID_LENGTH + 1,"%d",id);

    //controllo errore scrittura
    if(rc < 0)
    {
        errno = EIO;
        return NULL;
    }

    char path_file[UNIX_PATH_MAX]; //path del file

    //creo il path del file
    rc = snprintf(path_file,UNIX_PATH_MAX,"%s/%s",dir_path,msg_id);

    //controllo errore scrittura
    if(rc < 0)
    {
        errno = EIO;
        return NULL;
    }

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

static int write_on_file(FILE *file,message_t *msg)
{
    int rc;
    char type[MSG_TYPE_SPACE]; //per memorizzare in una stringa il tipo del messaggio
    char size_buf[MSG_SIZE_SPACE]; //per memorizzare in una stringa la size del messaggio

    //scrivo il tipo del messaggio
    if(msg->hdr.op == TXT_MESSAGE)
        rc = snprintf(type,MSG_TYPE_SPACE,"Text");
    else
        rc = snprintf(type,MSG_TYPE_SPACE,"File");

    //controllo errore scrittura
    if(rc < 0)
    {
        errno = EIO;
        return -1;
    }

    //scrivo la size del messaggio
    rc = snprintf(size_buf,MSG_SIZE_SPACE,"%d",msg->data.hdr.len);

    //controllo errore scrittura
    if(rc < 0)
    {
        errno = EIO;
        return -1;
    }

    //scrivo sul file
    rc = fprintf(file,"%s\n%s\n%s\n%s\n",msg->hdr.sender,type,size_buf,msg->data.buf);

    //controllo errore scrittura
    if(rc < 0)
    {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int send_message(utente_t *receiver,message_t *text_message,utenti_registrati_t *utenti)
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

        //se il tipo di messaggio e' testuale allora bisogna incrementare ndelivered
        if(text_message->hdr.op == TXT_MESSAGE)
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
        USER_ERR_HANDLER(file_msg,NULL,-1);

        //scrivo il messaggio sul file
        rc = write_on_file(file_msg,text_message);

        //errore scrittura sul file
        if(rc == -1)
        {
            fclose(file_msg);
            return -1;
        }

        fclose(file_msg);

        //nessun errore riscontrato,incremento numero di messaggi non ancora consegnati
        rc = pthread_mutex_lock(utenti->mtx_stat);

        //errore lock
        if(rc)
        {
            errno = rc;
            return -1;
        }

        //se il tipo di messaggio e' testuale allora bisogna decremetare nnotdelivered
        if(text_message->hdr.op == TXT_MESSAGE)
            ++(utenti->stat->nnotdelivered);

        //rilascio lock
        pthread_mutex_unlock(utenti->mtx_stat);
    }

    return 0;
}

//1 messaggio troppo grande, -1 errore, 0 ok
static int uploadFile(int fd,char *filename,utenti_registrati_t *utenti)
{
    int rc;
    message_data_t file_data;
    FILE *file;
    char pathfile[UNIX_PATH_MAX];

    rc = readData(fd,&file_data);

    //errore lettura data file
    USER_ERR_HANDLER(rc,-1,-1);

    //controllo dimensione messaggio
    if(rc > utenti->max_file_size)
    {
        free(file_data.buf);
        return 1;
    }


    //path del file
    rc = snprintf(pathfile,UNIX_PATH_MAX,"%s/%s",utenti->media_dir,filename);

    //errore snpritnf
    if(rc < 0)
    {
        free(file_data.buf);
        errno = EIO;
        return -1;
    }

    //creo il file
    file = fopen(pathfile,"w");

    //errore creazione file
    if(file == NULL)
    {
        free(file_data.buf);
        return -1;
    }

    fwrite(file_data.buf,1,file_data.hdr.len,file);


    //libero memoria dal buffer
    free(file_data.buf);

    //errore scrittura
    if(rc < 0)
    {
        fclose(file);
        errno = EIO;
        return -1;
    }

    //incremento numeri di file non ancora consegnati. Verrano marcati solo con una GETFILE_OP
    rc = pthread_mutex_lock(utenti->mtx_stat);

    //errore lock
    if(rc)
    {
        fclose(file);
        errno = rc;
        return -1;
    }

    ++(utenti->stat->nfilenotdelivered);

    pthread_mutex_unlock(utenti->mtx_stat);

    //chiudo il file e termino
    fclose(file);

    return 0;
}

//, -1 errore,,0 ok
int inviaMessaggioUtente(char *sender_name,char *receiver_name,char *msg,size_t size_msg,messaggio_id_t type,utenti_registrati_t *utenti)
{
    int rc;

    //controllo sender sia registrato ed online
    utente_t *sender = checkSender(sender_name,utenti,NULL);

    if(sender == NULL)
    {
        /* se il nome del sender non e' valido,oppure il sender non e' online,oppure non e' registrato,
           oppure si sta cercando di autoinviarsi un messaggio */
        if(errno == EPERM || errno == ENETDOWN || errno == 0 || strcmp(sender_name,receiver_name) == 0)
        {
            rc = send_fail_message(sender->fd,OP_FAIL,utenti);
        }
        //errore checkSender
        else{
            return -1;
        }
    }
    //controllo dimensione messaggio,sia esso un file o un testuale
    else if(size_msg > utenti->max_msg_size)
    {
        //invio errore di messaggio testuale troppo grande
        rc = send_fail_message(sender->fd,OP_MSG_TOOLONG,utenti);
    }
    //altrimenti sender valido e messaggio valido
    else{

        //controllo stato del receiver: Deve essere solo registrato
        utente_t *receiver = cercaUtente(receiver_name,utenti,NULL);

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

            //in base al tipo di messaggio setto l'header
            if(type == TEXT_ID)
            {
                setHeader(&txt_message.hdr,TXT_MESSAGE,sender->nickname);
            }
            else{
                setHeader(&txt_message.hdr,FILE_MESSAGE,sender->nickname);
            }

            setData(&txt_message.data,receiver->nickname,msg,size_msg);

            rc = send_message(receiver,&txt_message,utenti);

            //rilascio lock receiver
            pthread_mutex_unlock(&receiver->mtx);

            //errore in send_text_message
            if(rc == -1)
            {
                //rilasciare lock del sender
                pthread_mutex_unlock(&sender->mtx);

                return -1;
            }

            //se si tratta di un file,devo fare anche l'upload sulla directory del server
            if(type == FILE_ID)
            {
                rc = uploadFile(sender->fd,msg,utenti);

                //file troppo grande
                if(rc == 1)
                {
                    //invio messaggio errore file size troppo grande
                    rc = send_fail_message(sender->fd,OP_MSG_TOOLONG,utenti);

                    pthread_mutex_unlock(&sender->mtx);

                    //errore invio  messaggio
                    USER_ERR_HANDLER(rc,-1,-1);

                    return 0;

                }
                //errrore nell'uploadFile
                if(rc == -1)
                {
                    pthread_mutex_unlock(&sender->mtx);
                    return -1;
                }
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

//solo testuali
int inviaMessaggioUtentiRegistrati(char *sender_name,char *msg,size_t size_msg,utenti_registrati_t *utenti)
{
    int rc;
    int sender_pos;

    //controllo sender sia registrato ed online
    utente_t *sender = checkSender(sender_name,utenti,&sender_pos);

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
    else
    {
        //preparo messaggio
        message_t txt_message;

        setHeader(&txt_message.hdr,TXT_MESSAGE,sender->nickname);
        setData(&txt_message.data,"",msg,size_msg);

        //scorro tutti gli utenti registrati
        for (size_t i = 0; i < MAX_USERS; i++)
        {
            //caso in cui analizzo il sender.Vado alla prossima iterazione.
            if(sender_pos == i)
                continue;

            utente_t *receiver = &utenti->elenco[i];

            rc = pthread_mutex_lock(&receiver->mtx);

            if(rc)
            {
                errno = rc;
                return -1;
            }

            //se e' un utente registrato,gli mando il messaggio
            if(receiver->isInit)
            {
                rc = send_message(receiver,&txt_message,utenti);
            }

            pthread_mutex_unlock(&receiver->mtx);

            //errore in send_text_message
            if(rc == -1)
            {
                //rilascio lock del sender
                pthread_mutex_unlock(&sender->mtx);

                return -1;
            }

        }

        //se non ci sono stati errori mando risposta al sender di OP_OK
        rc = send_ok_message(sender->fd,"",0);

        //solo ora posso rilasciare lock del sender
        pthread_mutex_unlock(&sender->mtx);
    }

    //controllo esito del messaggio di risposta al sender
    USER_ERR_HANDLER(rc,-1,-1);

    return 0;
}
