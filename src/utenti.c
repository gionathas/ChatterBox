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
#include<string.h>
#include"utenti.h"
#include"config.h"

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

utenti_registrati_t *inizializzaUtentiRegistrati(long max_utenti,unsigned long *registrati,unsigned long *online,pthread_mutex_t *mtx)
{
    int rc;

    //contrllo parametri
    if(registrati == NULL || online == NULL || mtx == NULL || max_utenti <= 0)
    {
        errno = EINVAL;
        return NULL;
    }

    utenti_registrati_t *utenti = malloc(sizeof(utenti_registrati_t));

    //allocazione andata male
    if(utenti == NULL)
        return NULL;

    //alloco spazio per elenco,iniziaizzando valori statici all'interno
    utenti->elenco = calloc(max_utenti,sizeof(utente_t));

    //inizializzo tutti i mutex
    for (size_t i = 0; i < max_utenti; i++)
    {
        rc = pthread_mutex_init(&utenti->elenco[i].mtx,NULL);

        //esito init
        if(rc)
        {
            errno = rc;
            return NULL;
        }
    }

    //esito allocazione
    if(utenti->elenco == NULL)
        return NULL;

    utenti->utenti_online = online;
    utenti->utenti_registrati = registrati;
    utenti->mtx = mtx;
    utenti->max_utenti = max_utenti;

    return utenti;
}


utente_t *cercaUtente(char *name,utenti_registrati_t *Utenti)
{
    int rc;

    if(name == NULL || Utenti == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    //ottengo posizione tramite hash
    int hashIndex = hash(name,Utenti->max_utenti);
    int count = 0; //contatore di supporto

    //prendo lock sull'utente che analizzo
    rc = pthread_mutex_lock(&Utenti->elenco[hashIndex].mtx);

    //errore lock
    if(rc)
    {
        errno = rc;
        return NULL;
    }

    //Non appena incontro una posizione vuota termino,perche' non ho trovato l'utente
    while(Utenti->elenco[hashIndex].isInit)
    {
        //se i nick sono uguali,abbiamo trovato l'utente
        if(strcmp(Utenti->elenco[hashIndex].nickname,name) == 0)
        {
            pthread_mutex_unlock(&Utenti->elenco[hashIndex].mtx);
            return &Utenti->elenco[hashIndex];
        }

        //rilascio la lock dell'utente attuale per andare ad analizzare il prossimo utente
        pthread_mutex_unlock(&Utenti->elenco[hashIndex].mtx);

        //altrimenti proseguo nella ricerca,aggiornando l'indice
        ++hashIndex;
        //per non traboccare
        hashIndex %= Utenti->max_utenti;
        count++;

        //caso in cui ho controllato tutti gli utenti e non ho trovato quello che mi interessava
        if(count == Utenti->max_utenti)
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

    //utente non trovato
    return NULL;
}

int registraUtente(char *name,int fd,utenti_registrati_t *Utenti)
{
    int rc;

    //controllo parametri
    if(name == NULL || fd <= 0 || Utenti == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    errno = 0;

    //lock statistiche utenti
    rc = pthread_mutex_lock(Utenti->mtx);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //controllo che non abbiamo raggiunto il massimo degli utenti registrabili
    if(*Utenti->utenti_registrati == Utenti->max_utenti)
    {
        pthread_mutex_unlock(Utenti->mtx);
        errno = ENOMEM;
        return -1;
    }

    //rilascio lock statistiche
    pthread_mutex_unlock(Utenti->mtx);

    //Controllo se il nick non sia gia' presente
    utente_t *already_reg = cercaUtente(name,Utenti);

    //utente gia; presente
    if(already_reg != NULL)
    {
        return 1;
    }
    else if(already_reg == NULL && errno != 0)
    {
        //c'e' stato un errore nella ricerca
        return -1;
    }

    //hash per la ricerca
    int hashIndex = hash(name,Utenti->max_utenti);

    //prendo lock sull'utente che analizzo
    rc = pthread_mutex_lock(&Utenti->elenco[hashIndex].mtx);

    //errore lock
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
        hashIndex %= Utenti->max_utenti;

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
    Utenti->elenco[hashIndex].isOnline = 1;
    Utenti->elenco[hashIndex].isInit = 1;
    Utenti->elenco[hashIndex].fd = fd;

    //rilascio lock utente inseritp
    pthread_mutex_unlock(&Utenti->elenco[hashIndex].mtx);

    //lock statistiche utenti
    rc = pthread_mutex_lock(Utenti->mtx);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //incremento numero utenti registrati e utenit online
    ++(*(Utenti->utenti_registrati));
    ++(*(Utenti->utenti_online));

    //rilascio lock statistiche
    pthread_mutex_unlock(Utenti->mtx);

    return 0;
}

//-1 errore,0 rimosso,1 non trovato
int deregistraUtente(char *name,utenti_registrati_t *Utenti)
{
    int rc;

    if(name == NULL || Utenti == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    errno = 0;

    utente_t *utente = cercaUtente(name,Utenti);

    //errore ricerca utente
    if(utente == NULL && errno != 0)
    {
        return -1;
    }
    else if(utente == NULL)
    {
        //utente non trovato
        return 1;
    }

    //altrimenti rimuovo l'utente

    //lock utente
    rc = pthread_mutex_lock(&utente->mtx);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //setto flag per segnalare che la posizione da ora in poi e' libera
    utente->isInit = 0;

    //rilascio lock utente
    pthread_mutex_unlock(&utente->mtx);

    //lock statistiche utenti
    rc = pthread_mutex_lock(Utenti->mtx);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //decremento utenti registrati e utenti online
    --(*(Utenti->utenti_registrati));
    --(*(Utenti->utenti_online));

    //rilascio lock statistiche
    pthread_mutex_unlock(Utenti->mtx);

    return 0;
}

int connectUtente(char *name,int fd,utenti_registrati_t *Utenti)
{
    int rc;

    //controllo parametri
    if(name == NULL || Utenti == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    utente_t *utente;

    errno = 0;

    utente = cercaUtente(name,Utenti);

    //utente non presente
    if(utente == NULL && errno == 0)
        return 1;
    //c'e' stato un errore nella ricerca
    else if(utente == NULL && errno != 0)
        return -1;

    //lock utente
    rc = pthread_mutex_lock(&utente->mtx);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    utente->isOnline = 1;
    utente->fd = fd;

    //rilascio lock utente
    pthread_mutex_unlock(&utente->mtx);

    //lock statistiche utenti
    rc = pthread_mutex_lock(Utenti->mtx);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //incremento numero utenti online
    ++(*(Utenti->utenti_online));

    //rilascio lock statistiche
    pthread_mutex_unlock(Utenti->mtx);

    return 0;
}

//0 disconessione avvenuta,1 utente non trovato,-1 errore
int disconnectUtente(char *name,utenti_registrati_t *Utenti)
{
    int rc;

    //controllo parametri
    if(name == NULL || Utenti == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    utente_t *utente;

    errno = 0;

    utente = cercaUtente(name,Utenti);

    //utente non presente
    if(utente == NULL && errno == 0)
        return 1;
    //c'e' stato un errore nella ricerca
    else if(utente == NULL && errno != 0)
        return -1;

    //lock utente
    rc = pthread_mutex_lock(&utente->mtx);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    utente->isOnline = 0;
    utente->fd = -1;

    //rilascio lock utente
    pthread_mutex_unlock(&utente->mtx);

    //lock statistiche utenti
    rc = pthread_mutex_lock(Utenti->mtx);

    //errore lock
    if(rc)
    {
        errno = rc;
        return -1;
    }

    --(*(Utenti->utenti_online));

    //rilascio lock statistiche
    pthread_mutex_unlock(Utenti->mtx);

    return 0;
}

void mostraUtenti(utenti_registrati_t *Utenti)
{
    int rc;

    if(Utenti == NULL)
    {
        return;
    }

    for (size_t i = 0; i < Utenti->max_utenti; i++)
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

static char *add_name(char *buffer,size_t *buffer_size,int *new_size,char *name)
{
    size_t count = 0;//per contare numero di byte scritti

    //inche non arrivo alla fine del nickname, ed ho spazio nel buffer
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

    //aggiungo spazi per occupare tutti i byte del nickname
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

    //ritorno la posizione attuale nel buffer
    return (buffer);
}

int mostraUtentiOnline(char *buff,size_t *size_buff,int *new_size,utenti_registrati_t *Utenti)
{
    int rc;
    char *parser; //puntatore di supporto

    if(buff == NULL || Utenti == NULL || size_buff <= 0 || size_buff == NULL || new_size == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    parser = buff;

    //scorro gli utenti registrati
    for (int i = 0; i < Utenti->max_utenti; i++)
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
                //aggiungo il nome di questo utente alla stringa degli utenti online,cioe il buffer
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
    if(Utenti == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    free(Utenti->elenco);
    free(Utenti);

    return 0;
}
