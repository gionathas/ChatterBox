/**
 * @file  utenti.c
 * @brief Implementazione funzioni  per la gestione degli utenti di chatty
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
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
    if(name == NULL || Utenti == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    //ottengo posizione tramite hash
    int hashIndex = hash(name,Utenti->max_utenti);
    int count = 0; //contatore di supporto

    //Non appena incontro una posizione vuota termino,perche' non ho trovato l'utente
    while(Utenti->elenco[hashIndex].isInit)
    {
        //se i nick sono uguali,abbiamo trovato l'utente
        if(strcmp(Utenti->elenco[hashIndex].nickname,name) == 0)
        {
            return &Utenti->elenco[hashIndex];
        }

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
    }

    //utente non trovato
    return NULL;
}

int registraUtente(char *name,int fd,utenti_registrati_t *Utenti)
{
    int rc;

    errno = 0;

    //controllo parametri
    if(name == NULL || fd <= 0 || Utenti == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    //controllo che non abbiamo raggiunto il massimo degli utenti registrabili
    if(*Utenti->utenti_registrati == Utenti->max_utenti)
    {
        errno = ENOMEM;
        return -1;
    }

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

    //finche trovo posizioni occupate,avanzo con l'indice
    while(Utenti->elenco[hashIndex].isInit)
    {
        //aggiorno indice
        ++hashIndex;
        hashIndex %= Utenti->max_utenti;
    }

    //posizione trovata, inserisco info relativo all'utente
    strncpy(Utenti->elenco[hashIndex].nickname,name,MAX_NAME_LENGTH);
    Utenti->elenco[hashIndex].isOnline = 1;
    Utenti->elenco[hashIndex].isInit = 1;
    Utenti->elenco[hashIndex].fd = fd;
    rc = pthread_mutex_init(&Utenti->elenco[hashIndex].mtx,NULL);

    //errore creazione mutex
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //incremento numero utenti registrati
    ++(*(Utenti->utenti_registrati));
    ++(*(Utenti->utenti_online));


    return 0;
}

//-1 errore,0 rimosso,1 non trovato
int deregistraUtente(char *name,utenti_registrati_t *Utenti)
{
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
    pthread_mutex_destroy(&utente->mtx);

    //setto flag per segnalare che la posizione da ora in poi e' libera
    utente->isInit = 0;

    //decremento utenti registrati
    --(*(Utenti->utenti_registrati));
    --(*(Utenti->utenti_online));


    //utente non trovato
    return 0;
}

int connectUtente(char *name,int fd,utenti_registrati_t *Utenti)
{
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

    utente->isOnline = 1;
    utente->fd = fd;
    ++(*(Utenti->utenti_online));

    return 0;
}

//0 disconessione avvenuta,1 utente non trovato,-1 errore
int disconnectUtente(char *name,utenti_registrati_t *Utenti)
{
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

    utente->isOnline = 0;
    utente->fd = -1;
    --(*(Utenti->utenti_online));


    return 0;
}

void mostraUtenti(FILE *fout,utenti_registrati_t *Utenti)
{
    for (size_t i = 0; i < Utenti->max_utenti; i++)
    {
        //posizione non  vuota
        if(Utenti->elenco[i].isInit)
        {
            fprintf(fout,"%s ",Utenti->elenco[i].nickname);

            //online?
            if(Utenti->elenco[i].isOnline)
                fprintf(fout,"online\n");
            else
                fprintf(fout,"offline\n");

        }
    }
}

void mostraUtentiOnline(FILE *fout,utenti_registrati_t *Utenti)
{
    for (size_t i = 0; i < Utenti->max_utenti; i++)
    {
        //posizione non  vuota
        if(Utenti->elenco[i].isInit)
        {
            if(Utenti->elenco[i].isOnline)
                fprintf(fout, "%s\n",Utenti->elenco[i].nickname);
        }
    }
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
