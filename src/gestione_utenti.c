/**
 * @file  utenti.c
 * @brief Implementazione funzioni  per la gestione degli utenti di chatty
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include<string.h>
#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include<errno.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<dirent.h>
#include<math.h>
#include<string.h>
#include"utenti.h"
#include"config.h"

#define DEBUG
/**
 * @function hash
 * @brief Algoritmo di hashing per stringhe
 * @author Dan Bernstein
 * @return codice hash per accedere ad un particolare elemento dell'elenco degli utenti
 */
static unsigned int hash(char *str,int max)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash % max;
}

/**
 * @function remove_directory
 * @brief Rimuove una directory e tutti i file al suo interno.
 * @param path path della directory da eiminare
 * @return 0 in caso di successo, -1 in caso di errore settando errno
 */
static int remove_directory(char *path)
{
    DIR *d = opendir(path);
    size_t path_len = strlen(path);
    int r = 0; //variabile per esito ricorsione
    int rc;

    //errore apertura directory
    if (d == NULL)
        return -1;

    struct dirent *p;

    while (r == 0 && (p=readdir(d)) != NULL)
    {
        int r2 = 0; //altra variabile per ricorsione
        char *buf; //buffer dove salvo i path per le sottodirectory
        size_t len; //lunghezza buffer

        //salto le cartelle . e ..
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
            continue;

        len = path_len + strlen(p->d_name) + 2;
        buf = malloc(len);

        if(buf == NULL)
        {
            closedir(d);
            return -1;
        }

        struct stat statbuf;

        //scrivo dentro al buffer il path della sottodirectory che sto per analizzare
        snprintf(buf, len, "%s/%s", path, p->d_name);

        rc = stat(buf, &statbuf);

        if(rc == -1)
        {
            free(buf);
            closedir(d);
            return -1;
        }

        //se si tratta di una directory,ricorsivamente la elimino
        if (S_ISDIR(statbuf.st_mode))
            r2 = remove_directory(buf);
        else //se si tratta di un semplice file,lo elimino
            r2 = unlink(buf);

        free(buf);

        //aggiorno r con l'esito della ricorsione
        r = r2;
    }

    //casi di errore
    if(r == -1 || (p == NULL && errno != 0))
    {
        closedir(d);
        return -1;
    }

    //chiudo directory attuale
    closedir(d);

    //caso di directory vuota,posso ora eliminarla con rmdir
   if (r == 0)
   {
      r = rmdir(path);
   }

   return r;
}

//per vedere quante cifre ci sono in un numero
static inline int numOfDigits(int x)
{
    return (floor(log10 (abs (x))) + 1);
}

utenti_registrati_t *inizializzaUtentiRegistrati(int msg_size,int file_size,int hist_size,struct statistics *statistics,pthread_mutex_t *mtx_stat,char *dirpath)
{
    int rc;
    struct stat st;

    //controllo parametri
    if(msg_size <= 0 || file_size <= 0 || hist_size <= 0 || statistics == NULL || mtx_stat == NULL ||
        dirpath == NULL || strlen(dirpath) > MAX_SERVER_DIR_LENGTH || numOfDigits(hist_size) > MAX_ID_LENGTH )
    {
        errno = EINVAL;
        return NULL;
    }

    utenti_registrati_t *utenti = malloc(sizeof(utenti_registrati_t));

    //allocazione andata male
    if(utenti == NULL)
        return NULL;

    //alloco spazio per elenco,iniziaizzando valori statici all'interno
    utenti->elenco = calloc(MAX_USERS,sizeof(utente_t));

    //esito allocazione
    if(utenti->elenco == NULL)
    {
        free(utenti);
        return NULL;
    }

    //inizializzo tutti i mutex
    for (size_t i = 0; i < MAX_USERS; i++)
    {
        rc = pthread_mutex_init(&utenti->elenco[i].mtx,NULL);

        //errore init mutex
        if(rc)
        {
            //distruggo mutex creati fin ora
            for (size_t j = 0; j < i; j++)
            {
                pthread_mutex_destroy(&utenti->elenco[j].mtx);
            }

            free(utenti->elenco);
            free(utenti);

            errno = rc;
            return NULL;
        }
    }

    utenti->stat = statistics;
    utenti->mtx_stat = mtx_stat;
    utenti->max_msg_size = msg_size;
    utenti->max_file_size = file_size;
    utenti->max_hist_msgs = hist_size;
    strncpy(utenti->media_dir,dirpath,MAX_SERVER_DIR_LENGTH);

    //creo la directory del server,se non e' gia' presente
    if (stat(dirpath, &st) == -1)
    {
        //risetto errno a 0,perche' la directory non deve esistere per funzionare
        errno = 0;
        rc = mkdir(dirpath, 0777);

        //errore creazione directory
        if(rc == -1)
        {
            int curr_error = errno;

            //dealloco tutto
            for (size_t i = 0; i < MAX_USERS; i++)
            {
                pthread_mutex_destroy(&utenti->elenco[i].mtx);
            }

            free(utenti->elenco);
            free(utenti);
            errno = curr_error;

            return NULL;
        }
    }

    return utenti;
}


utente_t *cercaUtente(char *name,utenti_registrati_t *Utenti)
{
    int rc;

    //controllo parametri
    if(name == NULL)
    {
        errno = EPERM;
        return NULL;
    }

    if(Utenti == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    //ottengo posizione tramite hash
    int hashIndex = hash(name,MAX_USERS);
    int count = 0; //contatore di supporto

    //prendo lock sull'utente che analizzo
    rc = pthread_mutex_lock(&Utenti->elenco[hashIndex].mtx);

    //errore lock
    if(rc)
    {
        errno = rc;
        return NULL;
    }

    //fin quando trovo utenti inizializzati
    while(Utenti->elenco[hashIndex].isInit)
    {
        //se i nick sono uguali,abbiamo trovato l'utente
        if(strcmp(Utenti->elenco[hashIndex].nickname,name) == 0)
        {
            return &Utenti->elenco[hashIndex];
        }

        //rilascio la lock dell'utente attuale per andare ad analizzare il prossimo utente
        pthread_mutex_unlock(&Utenti->elenco[hashIndex].mtx);

        //altrimenti proseguo nella ricerca,aggiornando l'indice
        ++hashIndex;
        //per non traboccare
        hashIndex %= MAX_USERS;
        count++;

        //caso in cui ho controllato tutti gli utenti e non ho trovato quello che mi interessava
        if(count == MAX_USERS)
        {
            break;
        }

        //prendo lock per prossimo utente
        rc = pthread_mutex_lock(&Utenti->elenco[hashIndex].mtx);

        //errore lock
        if(rc)
        {
            errno = rc;
            return NULL;
        }
    }

    //rilascio lock sull'utente su cui mi trovo
    pthread_mutex_unlock(&Utenti->elenco[hashIndex].mtx);

    //utente non registrato
    return NULL;
}

int registraUtente(char *name,unsigned int fd,utenti_registrati_t *Utenti)
{
    int rc;

    //controllo parametri
    if(name == NULL || strlen(name) > MAX_NAME_LENGTH)
    {
        errno = EPERM;
        return -1;
    }

    if(Utenti == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    #ifdef DEBUG
        printf("#DEBUG# media dir:%s\n",Utenti->media_dir);
    #endif

    //lock statistiche utenti
    rc = pthread_mutex_lock(Utenti->mtx_stat);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //controllo che non abbiamo raggiunto il massimo degli utenti registrabili
    if(Utenti->stat->nusers == MAX_USERS)
    {
        pthread_mutex_unlock(Utenti->mtx_stat);
        errno = ENOMEM;
        return -1;
    }

    //rilascio lock statistiche
    pthread_mutex_unlock(Utenti->mtx_stat);

    //Controllo se il nick non sia gia' registrato
    utente_t *already_reg = cercaUtente(name,Utenti);

    //utente gia' registrato
    if(already_reg != NULL)
    {
        //rilascio lock utente trovato
        pthread_mutex_unlock(&already_reg->mtx);
        return 1;
    }
    else if(already_reg == NULL && errno != 0)
    {
        //c'e' stato un errore nella ricerca
        return -1;
    }

    //altrimenti nick non presente

    //hash per la ricerca
    int hashIndex = hash(name,MAX_USERS);

    //prendo lock utente che vado a controlare
    rc = pthread_mutex_lock(&Utenti->elenco[hashIndex].mtx);

    if(rc)
    {
        errno = rc;
        return -1;
    }

    //finche trovo posizioni occupate,avanzo con l'indice
    while(Utenti->elenco[hashIndex].isInit)
    {
        //rilascio lock utente
        pthread_mutex_unlock(&Utenti->elenco[hashIndex].mtx);

        //aggiorno indice
        ++hashIndex;
        hashIndex %= MAX_USERS;

        //prendo lock prossimo utente
        rc = pthread_mutex_lock(&Utenti->elenco[hashIndex].mtx);

        //errore lock
        if(rc)
        {
            errno = rc;
            return -1;
        }
    }

    //posizione trovata, inserisco info relativo all'utente
    strncpy(Utenti->elenco[hashIndex].nickname,name,MAX_NAME_LENGTH);
    Utenti->elenco[hashIndex].isInit = 1;
    Utenti->elenco[hashIndex].isOnline = 1;
    Utenti->elenco[hashIndex].fd = fd;
    Utenti->elenco[hashIndex].n_element_in_dir = 0;

    //creo la directory personale dell'utente:

    //creo il path..
    snprintf(Utenti->elenco[hashIndex].personal_dir,MAX_CLIENT_DIR_LENGHT,"%s/%s",Utenti->media_dir,name);

    //..e la cartella
    rc = mkdir(Utenti->elenco[hashIndex].personal_dir,0777);

    if(rc == -1)
    {
        //rilascio lock utente inserito
        pthread_mutex_unlock(&Utenti->elenco[hashIndex].mtx);
        return -1;
    }

    //rilascio lock utente inserito
    pthread_mutex_unlock(&Utenti->elenco[hashIndex].mtx);

    //errore creazione cartella utente
    if(rc == -1)
        return -1;

    //lock statistiche utenti
    rc = pthread_mutex_lock(Utenti->mtx_stat);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //incremento numero utenti registrati e utenti online
    ++(Utenti->stat->nusers);
    ++(Utenti->stat->nonline);

    //rilascio lock statistiche
    pthread_mutex_unlock(Utenti->mtx_stat);

    return 0;
}

int deregistraUtente(char *name,utenti_registrati_t *Utenti)
{
    int rc;

    utente_t *utente = cercaUtente(name,Utenti);

    if(utente == NULL)
    {
        //errore ricerca utente
        if(errno != 0)
            return -1;
        else//utente non registrato
            return 1;
    }

    //elimino la cartella personale dell'utente
    rc = remove_directory(utente->personal_dir);

    //setto flag per segnalare che la posizione da ora in poi e' libera
    utente->isInit = 0;

    //rilascio lock utente
    pthread_mutex_unlock(&utente->mtx);

    //errore eliminazione directory
    if(rc == -1)
    {
        return -1;
    }

    //lock statistiche utenti
    rc = pthread_mutex_lock(Utenti->mtx_stat);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //decremento utenti registrati e utenti online
    --(Utenti->stat->nusers);
    --(Utenti->stat->nonline);

    //rilascio lock statistiche
    pthread_mutex_unlock(Utenti->mtx_stat);

    return 0;
}

int connectUtente(char *name,unsigned int fd,utenti_registrati_t *Utenti)
{
    int rc;

    utente_t *utente;

    utente = cercaUtente(name,Utenti);

    if(utente == NULL)
    {
        //utente non registrato
        if(errno == 0)
            return 1;
        else //errore ricerca utente
            return -1;
    }

    //se l'utente e' gia online
    if(utente->isOnline == 1)
    {
        pthread_mutex_unlock(&utente->mtx);
        errno = EPERM;
        return -1;
    }

    utente->isOnline = 1;
    utente->fd = fd;

    //rilascio lock utente
    pthread_mutex_unlock(&utente->mtx);

    //lock statistiche utenti
    rc = pthread_mutex_lock(Utenti->mtx_stat);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //incremento numero utenti online
    ++(Utenti->stat->nonline);

    //rilascio lock statistiche
    pthread_mutex_unlock(Utenti->mtx_stat);

    return 0;
}

int disconnectUtente(char *name,utenti_registrati_t *Utenti)
{
    int rc;

    utente_t *utente;

    utente = cercaUtente(name,Utenti);

    if(utente == NULL)
    {
        //utente non registrato
        if(errno == 0)
            return 1;
        else //errore ricerca utente
            return -1;
    }

    //se l'utente non risulta essere online
    if(utente->isOnline == 0)
    {
        pthread_mutex_unlock(&utente->mtx);
        errno = EPERM;
        return -1;
    }

    //disconnetto..
    utente->isOnline = 0;

    //rilascio lock utente
    pthread_mutex_unlock(&utente->mtx);

    //lock statistiche utenti
    rc = pthread_mutex_lock(Utenti->mtx_stat);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    --(Utenti->stat->nonline);

    //rilascio lock statistiche
    pthread_mutex_unlock(Utenti->mtx_stat);

    return 0;
}

void mostraUtenti(utenti_registrati_t *Utenti)
{
    int rc;

    if(Utenti == NULL)
    {
        return;
    }

    for (size_t i = 0; i < MAX_USERS; i++)
    {
        //lock su utente
        rc = pthread_mutex_lock(&Utenti->elenco[i].mtx);

        //errore lock
        if(rc)
        {
            return;
        }

        //posizione non  vuota
        if(Utenti->elenco[i].isInit)
        {
            printf("%s ",Utenti->elenco[i].nickname);

            //online?
            if(Utenti->elenco[i].isOnline)
                printf("online\n");
            else
                printf("offline\n");

        }

        //unlock su utente
        pthread_mutex_unlock(&Utenti->elenco[i].mtx);
    }
}

/**
 * @function add_name
 * @brief Aggiunge il nome dell'utente all'interno del buffer
 * @param buffer puntatore al buffer
 * @param buffer_size puntatore alla size attuale allocata dal buffer
 * @param new_size puntatore alla nuova size del buffer,dopo aver inserito il nome
 * @param name nickname da inserire nel buffer
 * @return puntatore alla prossima posizione libera per inserire nel buffer,altrimenti NULL.
 */
static char *add_name(char *buffer,size_t *buffer_size,int *new_size,char *name)
{
    size_t count = 0;//per contare numero di byte scritti

    //finche non arrivo all'ultima lettera del nickname, ed ho spazio nel buffer
    while(*name != '\0' && *buffer_size > 0)
    {
        //inserisco lettera
        *buffer = *name;

        //aggiorno indici per scorrere il nome
        ++buffer;
        ++name;
        ++count;

        //decremento dimensione del buffer
        --*buffer_size;
    }

    //se non ho piu spazio nel buffer
    if(*buffer_size == 0)
        return NULL;
    else
    {
        //metto carattere terminatore
        *buffer = '\0';

        //decremento dimensione del buffer
        --*buffer_size;

        //aggiorno indici
        ++count;
        ++buffer;
    }

    //aggiungo spazi dopo il nick,per occupare tutti i byte
    while( ( (MAX_NAME_LENGTH + 1 - (count)) > 0 ) && *buffer_size > 0)
    {
        *buffer = ' ';

        ++count;
        ++buffer;

        --*buffer_size;
    }

    //se non ho piu' spazio nel buffer,e non ho finito di mettere gli spazi
    if(*buffer_size == 0 && (MAX_NAME_LENGTH + 1 - (count)) != 0 )
        return NULL;

    //altrimenti aggiorno la nuova size del buffer..
    *new_size += count;

    //ritorno la posizione attuale nel buffer,per poter inserire nuovamente
    return (buffer);
}

int mostraUtentiOnline(char *buff,size_t *size_buff,int *new_size,utenti_registrati_t *Utenti)
{
    int rc;
    char *parser; //puntatore di supporto

    if(buff == NULL || Utenti == NULL || size_buff == NULL || new_size == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    parser = buff;

    //scorro gli utenti registrati
    for (int i = 0; i < MAX_USERS; i++)
    {
        //lock su utente attuale
        rc = pthread_mutex_lock(&Utenti->elenco[i].mtx);

        //errore lock
        if(rc)
        {
            errno = rc;
            return -1;
        }

        //posizione non vuota
        if(Utenti->elenco[i].isInit)
        {
            //e' online
            if(Utenti->elenco[i].isOnline)
            {
                //aggiungo il nome di questo utente alla stringa degli utenti online,cioe' il buffer
                parser = add_name(parser,size_buff,new_size,Utenti->elenco[i].nickname);

                //caso in cui lo spazio nel buffer e' terminato
                if(parser == NULL)
                {
                    //unlock su utente
                    pthread_mutex_unlock(&Utenti->elenco[i].mtx);
                    buff = NULL;
                    errno = ENOBUFS;
                    return -1;
                }
            }
        }

        //unlock su utente
        pthread_mutex_unlock(&Utenti->elenco[i].mtx);
    }

    return 0;
}


int eliminaElenco(utenti_registrati_t *Utenti)
{
    int rc;

    if(Utenti == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    //distrutto tutti i mutex relativi agli utente
    for (size_t i = 0; i < MAX_USERS; i++)
    {
        rc = pthread_mutex_destroy(&Utenti->elenco[i].mtx);

        //esito init
        if(rc)
        {
            free(Utenti->elenco);
            free(Utenti);
            errno = rc;
            return -1;
        }
    }

    //elimino tutta la directory del server
    rc = remove_directory(Utenti->media_dir);

    if(rc == -1)
    {
        free(Utenti->elenco);
        free(Utenti);
        return -1;
    }

    //libero memoria
    free(Utenti->elenco);
    free(Utenti);

    return 0;
}
