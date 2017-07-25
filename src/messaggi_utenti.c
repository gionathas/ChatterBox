/**
 * @file  messaggi_utenti.c
 * @brief Implementazione modulo per lo scambio di messaggi tra utenti di chatty.
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<dirent.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include"utenti.h"
#include"config.h"
#include"message.h"
#include"connections.h"
#include"messaggi_utenti.h"

/*FUNZIONI DI SUPPORTO*/

//per far funzionare una chiamata di funzione in inviaMessaggiRemoti
//@see send_ok_message
static int _send_ok_message(int fd,size_t *buf,unsigned int len)
{
    return send_ok_message(fd,(char*)buf,len);
}

/**
 * @function create_message_file
 * @brief Crea e apre un file per la scrittura,su una directory.
 *        Il file viene rinominato con un id.
 * @param id id con cui rinominare il file da creare
 * @param dir_path path della directory
 * @return file aperto per la scrittura,altrimenti NULL e setta errno.
 */
static FILE *create_message_file(int id,char *dir_path)
{
    int rc;
    char msg_id[MAX_ID_LENGTH + 1]; //per memorizzare id del msg sottoforma di stringa

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

/**
 * @function generate_id_message
 * @brief Genera l'id del messaggio da salvare in remoto sul server.
 * @param receiver utente a cui arrivera' il messaggio
 * @param utenti elenco utenti registrati
 * @return id del messaggio.
 */
static unsigned int generate_id_message(utente_t *receiver,utenti_registrati_t *utenti)
{
    unsigned int id;

    //se la directory del'utente e' piena
    if(receiver->n_remote_message == utenti->max_hist_msgs)
    {
        //dato che tutti i messaggi hanno la stessa importanza,ne scegliamo una a caso da sovrascrivere
        srand(time(NULL));
        id = (rand() % utenti->max_hist_msgs) + 1;
    }
    else{
        id = receiver->n_remote_message + 1;
        //incremento numero di messaggi nella directory dell' utente
        ++(receiver->n_remote_message);
    }

    return id;
}

/**
 * @function removenl
 * @brief Rimuove newline da una stringa
 * @param string stringa su cui operare
 */
static void removenl(char *string)
{
    char *tmp;

    if((tmp = strchr(string,'\n')) != NULL)
        *tmp = '\0';
}

/**
 * @function write_on_file
 * @brief Scrive sul file il messaggio dal salvare in remoto sul server.
 * @param file file su cui scrivere
 * @param msg messaggio da salvare in remoto sul server.
 * @return 0 scrittura su file andata a buon fine,altrimenti -1 e setta errno.
 */
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

/**
 * @function search_file
 * @brief Cerca un file dentro un cartella, e lo restituisce aperto in lettura binaria.
 * @param filename nome del file
 * @param dir_path path della cartella
 * @param file_size puntatore alla dimensione del file
 * @return file aperto in lettura binaria e dimensione del file.Altrimenti ritorna NULL,
 *         se il file non e' stato trovato. In caso di altri errori,errno viene settato.
 */
static FILE *search_file(char *filename,char *dir_path,size_t *file_size)
{
    int rc;
    char *buf;
    size_t buf_len;
    size_t dir_path_len = strlen(dir_path);
    FILE *searched_file = NULL;
    int i = 0;

    //elimino eventuale ./
    while(filename[i] == '.' || filename[i] == '/')
        ++filename;

    //apro directory
    DIR *dir = opendir(dir_path);

    //errore apertura directory
    USER_ERR_HANDLER(dir,NULL,NULL);

    struct dirent *p;

    int find = 0;
    //inizio a scorrere i file dentro la directory
    while((p = readdir(dir)) != NULL && !find)
    {
         struct stat info;

         //inizializzo spazio per il buffer che conterra' il path dell'elemento che analizzo
         buf_len = dir_path_len + strlen(p->d_name) + 2;
         buf = malloc(buf_len);

         if(!buf)
         {
             break;
         }

         //scrivo dentro al buffer il path dell'elemento che sto per analizzare
         snprintf(buf,buf_len, "%s/%s",dir_path, p->d_name);

         //analizzo il file corrente
         rc =  stat(buf,&info);

         //errore stat
         if(rc == -1)
         {
             free(buf);
             break;
         }

         //analizzo il tipo del file,se non e' un file regolare vado avanti
         if(S_ISREG(info.st_mode))
         {
             //se ho trovato il file che cercavo
             if(strcmp(p->d_name,filename) == 0)
             {
                 *file_size = info.st_size;
                 find = 1;
                 free(buf);
                break;
             }
         }

         free(buf);
    }

    //apro il file se l'ho trovato,in modalita binaria
    if(find)
        searched_file = fopen(p->d_name,"rb");

    closedir(dir);

    return searched_file;
}

/**
 * @function uploadFile
 * @brief Carica un file sulla directory del server.
 * @param fd fd del client che invia il file
 * @param filename nome del file
 * @param utenti elenco utenti registrati
 * @return 0 file caricato correttamente sul server. 1 file troppo grande.
 *         altrimenti -1 e setta errno.
 */
static int uploadFile(int fd,char *filename,utenti_registrati_t *utenti)
{
    int rc;
    message_data_t file_data;
    FILE *file;
    char pathfile[UNIX_PATH_MAX];

    rc = readData(fd,&file_data);

    //errore lettura data file
    USER_ERR_HANDLER(rc,-1,-1);

    //controllo dimensione file
    if(rc > utenti->max_file_size)
    {
        free(file_data.buf);
        return 1;
    }

    //creo path del file sulla directory del server
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

    rc = fwrite(file_data.buf,1,file_data.hdr.len,file);


    //libero memoria dal buffer
    free(file_data.buf);

    //errore scrittura
    if(rc < 0)
    {
        fclose(file);
        errno = EIO;
        return -1;
    }

    //incremento numeri di file non ancora consegnati
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

/**
 * @function send_message
 * @brief Invia un messaggio al receiver sia esso online o offline
 * @param receiver destinatario messaggio
 * @param messaggio da inviare
 * @param utenti elenco utenti registrati
 * @return 0 messaggio inviato correttamente,altrimenti -1 e setta errno.
 */
static int send_message(utente_t *receiver,message_t *message,utenti_registrati_t *utenti)
{
    int rc;

    //receiver online,posso mandargli direttamente il messaggio
    if(receiver->isOnline)
    {
        rc = sendHeader(receiver->fd,&message->hdr);

        //errore invio header
        USER_ERR_HANDLER(rc,-1,-1);

        rc = sendData(receiver->fd,&message->data);

        //errore invio data
        USER_ERR_HANDLER(rc,-1,-1);

        //nessun errore riscontrato,aggiorno statistiche server
        rc = pthread_mutex_lock(utenti->mtx_stat);

        //errore lock
        if(rc)
        {
            errno = rc;
            return -1;
        }

        //se il tipo di messaggio e' testuale allora bisogna incrementare ndelivered
        if(message->hdr.op == TXT_MESSAGE)
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
        FILE *file_msg = create_message_file(id,receiver->personal_dir);

        //errore creazione file
        USER_ERR_HANDLER(file_msg,NULL,-1);

        //scrivo il messaggio sul file
        rc = write_on_file(file_msg,message);

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
        if(message->hdr.op == TXT_MESSAGE)
            ++(utenti->stat->nnotdelivered);

        //rilascio lock
        pthread_mutex_unlock(utenti->mtx_stat);
    }

    return 0;
}

/**
 * @function sendDataRemoteFile
 * @brief invia tutti i file presenti nella directory personale di un utente
 * @param utente utente che effettua la rihiesta
 * @param utenti elenco utenti registrati
 * @return 0 operazione effettuata con successo,altrimenti -1 e setta errno.
 * @note i messaggi vengono mandati dal piu recente al meno recente
 */
static int sendDataRemoteFile(utente_t *utente,utenti_registrati_t *utenti)
{
    char *file_path;
    size_t file_path_size;
    int rc;

    //apro la directory dell'utente
    DIR *dir = opendir(utente->personal_dir);

    USER_ERR_HANDLER(dir,NULL,-1);

    //lunghezza path della directory del server
    size_t dir_path_len = strlen(utente->personal_dir);

    struct dirent *file_iterator; // per iterare tra i file

    //fin quando non ci sono piu file
    while((file_iterator = readdir(dir)) != NULL)
    {
        //evito le cartelle . e ..
        if(strcmp(file_iterator->d_name,".") == 0 || strcmp(file_iterator->d_name,"..") == 0 )
            continue;

        //alloco spazio per il buffer che conterra il path del file attuale che stiamo analizzando
        file_path_size = dir_path_len + strlen(file_iterator->d_name) + 2;
        file_path = malloc(file_path_size);

        if(file_path == NULL)
            break;

        rc = snprintf(file_path,file_path_size,"%s/%s",utente->personal_dir,file_iterator->d_name);

        if(rc < 0)
        {
            free(file_path);
            break;
        }

        //apro il file attuale in lettura
        FILE *file = fopen(file_path,"r");

        //errore apertura
        if(!file)
        {
            free(file_path);
            return -1;
        }

        //alloco un buffer per contenere la data del messaggio da leggere
        char *buf = malloc(utenti->max_msg_size);

        if(buf == NULL)
        {
            free(file_path);
            fclose(file);
            break;
        }

        //variabili per leggere il messaggio
        message_t remoteMessage;
        op_t op = 0;
        char *sender,*data;
        size_t size_data = 0;

        //analizzo il file e preparo messaggio di risposta
        //il file si compone di 4 righe ognuna contiene un info per il messaggio
        int n_line = 4,curr_line = 0;
        while(fgets(buf,utenti->max_msg_size,file) !=  NULL && curr_line < n_line )
        {
            //rimuoviamo il newline dalla riga appena letta
            removenl(buf);

            //leggiamo il sender
            if(curr_line == 0)
            {
                sender = malloc(MAX_NAME_LENGTH + 1);

                if(!sender)
                    break;

                strncpy(sender,buf,MAX_NAME_LENGTH);
            }
            //leggiamo il tipo di messaggio
            else if(curr_line == 1)
            {
                if(strcmp(buf,"Text") == 0)
                    op = TXT_MESSAGE;
                else
                    op = FILE_MESSAGE;
            }
            //leggiamo size messaggio
            else if(curr_line == 2)
            {
                size_data = atoi(buf);
            }
            //leggiamo data del messaggio
            else{
                data = malloc(size_data);

                if(!data)
                    break;

                strncpy(data,buf,size_data);
            }

            curr_line++;
        }

        free(buf);
        fclose(file);

        //errore nella lettura dei campi del file
        if(errno != 0)
        {
            free(file_path);
            break;
        }

        //preparo il messaggio letto in remoto per il client
        setHeader(&remoteMessage.hdr,op,sender);
        setData(&remoteMessage.data,"",data,size_data);

        rc = sendHeader(utente->fd,&remoteMessage.hdr);

        //errore invio header
        if(rc == -1)
        {
            free(file_path);
            break;
        }

        rc = sendData(utente->fd,&remoteMessage.data);

        //errore invio data
        if(rc == -1)
        {
            free(file_path);
            break;
        }

        //elimino il file in quando ora non serve piu'
        unlink(file_path);

        free(file_path);

        //decremento numero messaggi in remoto per l'utente attulae
        --utente->n_remote_message;

        //Aggiorno statistiche del server
        rc = pthread_mutex_lock(utenti->mtx_stat);

        //errore lock
        if(rc)
        {
            errno = rc;
            break;
        }

        if(op == FILE_MESSAGE)
        {
            --utenti->stat->nfilenotdelivered;
            ++utenti->stat->nfiledelivered;
        }
        //si tratta di un messaggio testuale
        else{
            --utenti->stat->nfilenotdelivered;
            ++utenti->stat->ndelivered;
        }

        //rilascio lock sulle statistiche
        pthread_mutex_unlock(utenti->mtx_stat);

    }

    //chiudo cartella dell'utente
    closedir(dir);

    //se si sono riscontrati errori
    if(errno != 0)
        return -1;
    else
        return 0;
}

/*FUNZIONI INTERFACCIA */
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

    //se il buffer non e' vuoto,allora inviamo anche data del messaggio
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

    //incremento numero di messaggi di errori inviati
    ++(utenti->stat->nerrors);

    //unlock stat
    pthread_mutex_unlock(utenti->mtx_stat);

    return 0;
}

int sendUserOnline(int fd,utenti_registrati_t *utenti)
{
    char user_online[MAX_NAME_LENGTH * MAX_USERS]; //stringa dove salvare i nick degli utenti online
    size_t size = MAX_NAME_LENGTH * MAX_USERS;
    int new_size = 0; //nuova size della stringa dopo aver scritto i nick al suo interno
    int rc;

    rc = mostraUtentiOnline(user_online,&size,&new_size,utenti);

    //se sono fallito per altro
    USER_ERR_HANDLER(rc,-1,-1);

    //mando messaggio di ok,con la stringa degli utenti online
    rc = send_ok_message(fd,user_online,new_size);

    return rc;
}

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
    //controllo dimensione messaggio,se esso e' un messaggio testuale
    else if(type == TEXT_ID && size_msg > utenti->max_msg_size)
    {
        //invio errore di messaggio testuale troppo grande
        rc = send_fail_message(sender->fd,OP_MSG_TOOLONG,utenti);
        pthread_mutex_unlock(&sender->mtx);
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

int getFile(char *sender_name,char *filename,utenti_registrati_t *utenti)
{
    int rc;
    FILE *file_request;
    size_t file_size = 0;
    char *tmp_file_data = NULL;

    //controllo sender sia registrato ed online
    utente_t *sender = checkSender(sender_name,utenti,NULL);

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
    //cerco il file ricercato dal sender e glielo invio se esiste
    else
    {
        //cerco il file nella directory del server
        file_request = search_file(filename,utenti->media_dir,&file_size);

        if(file_request == NULL)
        {
            //errore nella ricerca del file
            if(errno != 0)
            {
                //rilascio lock sul sender
                pthread_mutex_unlock(&sender->mtx);
                return -1;
            }

            //file non esistente
            else
                rc = send_fail_message(sender->fd,OP_NO_SUCH_FILE,utenti);
        }
        //file trovato,lo leggo e invio la risposta al client
        else{

            size_t byte_read = 0;

            tmp_file_data = malloc(file_size);

            //leggo la data del file e lo salvo nel buffer del messaggio di risposta
            byte_read = fread(tmp_file_data,1,file_size,file_request);

            //errore read nel file
            if(byte_read != file_size)
            {
                fclose(file_request);
                pthread_mutex_unlock(&sender->mtx);
                return -1;
            }

            fclose(file_request);

            //invio il file attraverso un messaggio di OP_OK
            rc = send_ok_message(sender->fd,tmp_file_data,file_size);

            //incremento numero di file spediti dal server
            rc = pthread_mutex_lock(utenti->mtx_stat);

            if(rc)
            {
                pthread_mutex_unlock(&sender->mtx);
                return -1;
            }

            ++(utenti->stat->nfiledelivered);

            pthread_mutex_unlock(utenti->mtx_stat);

        }
    }

    //rilascio lock del sender
    pthread_mutex_unlock(&sender->mtx);

    //controllo esito invio risposta
    USER_ERR_HANDLER(rc,-1,-1);

    return 0;

}

int inviaMessaggiRemoti(char *sender_name,utenti_registrati_t *utenti)
{
    int rc;
    //controllo sender sia registrato ed online
    utente_t *sender = checkSender(sender_name,utenti,NULL);

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
    //cerco il file ricercato dal sender e glielo invio se esiste
    else
    {
        //recupero il numero di messaggi da leggere
        size_t n_msgs = sender->n_remote_message;

        /* Pezzo particolare,abbiamo passato il puntatore alla variabile di n_msgs per rispettare il client */

        //mando un OP_OK con il numero di messaggi da leggere
        rc = _send_ok_message(sender->fd,&n_msgs,sizeof(n_msgs));

        //errore invio messaggio
        if(rc == -1)
        {
            pthread_mutex_unlock(&sender->mtx);
            return -1;
        }

        rc = sendDataRemoteFile(sender,utenti);

        //rilascio lock sul sender
        pthread_mutex_unlock(&sender->mtx);
    }

    //controllo esito delle ultime operazionio
    USER_ERR_HANDLER(rc,-1,-1);

    return 0;
}
