#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include"utenti.h"
#include"config.h"

utente_t *cercaUtente(char *name,utenti_registrati_t Utenti)
{
    if(name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    //ottengo posizione tramite hash
    int hashIndex = hash(name);
    int count = 0; //contatore di supporto

    //fin quando non incontro una poszione libera
    while(Utenti.elenco[hashIndex] != NULL)
    {
        //se i nick sono uguali,abbiamo trovato l'utente
        if(strcmp(Utenti.elenco[hashIndex]->nickname,name) == 0)
        {
            return Utenti.elenco[hashIndex];
        }

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
    }

    //utente non trovato
    return NULL;
}

//-1 errore,0 ok, 1 utente gia' presente
int registraUtente(char *name,int fd,utenti_registrati_t Utenti)
{
    int rc;

    errno = 0;

    //controllo parametri
    if(name == NULL || fd <= 0)
    {
        errno = EINVAL;
        return -1;
    }

    //controllo che non abbiamo raggiunto il massimo degli utenti registrabili
    if(*Utenti.utenti_registrati == MAX_USERS)
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

    //se arrivo qui posso registrare il nuovo utente

    //creo istanza utente
    utente_t *utente = malloc(sizeof(utente_t));

    //allocazione andata male
    if(utente == NULL)
        return -1;

    strncpy(utente->nickname ,name,MAX_NAME_LENGTH);
    utente->isOnline = 1;
    utente->fd = fd;
    rc = pthread_mutex_init(&utente->mtx,NULL);

    //errore mutex init
    if(rc)
    {
        errno = rc;
        return -1;
    }

    //ora lo memorizzo nell'elenco

    //hash per la ricerca
    int hashIndex = hash(name);

    //finche trovo posizioni occupate,avanzo con l'indice
    while(Utenti.elenco[hashIndex] != NULL)
    {
        //aggiorno indice
        ++hashIndex;
        hashIndex %= MAX_USERS;
    }

    //posizione trovata, inserisco
    Utenti.elenco[hashIndex] = utente;

    //incremento numero utenti registrati
    ++(*(Utenti.utenti_registrati));

    return 0;
}

//-1 errore,0 rimosso,1 non trovato
int deregistraUtente(char *name,utenti_registrati_t Utenti)
{
    if(name == NULL)
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
    free(utente);
    --(*(Utenti.utenti_registrati));

    //utente non trovato
    return 0;
}

int connectUtente(char *name,int fd,utenti_registrati_t Utenti)
{
    //controllo parametri
    if(name == NULL)
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
    ++(*(Utenti.utenti_online));

    return 0;
}

//0 disconessione avvenuta,1 utente non trovato,-1 errore
int disconnectUtente(char *name,utenti_registrati_t Utenti)
{
    //controllo parametri
    if(name == NULL)
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
    --(*(Utenti.utenti_online));


    return 0;
}

void mostraUtenti(FILE *fout,utenti_registrati_t Utenti)
{
    for (size_t i = 0; i < MAX_USERS; i++)
    {
        //posizione non  vuota
        if(Utenti.elenco[i] != NULL)
        {
            fprintf(fout,"%s ",Utenti.elenco[i]->nickname);

            //online?
            if(Utenti.elenco[i]->isOnline)
                fprintf(fout,"online\n");
            else
                fprintf(fout,"offline\n");

        }
    }
}

void mostraUtentiOnline(FILE *fout,utenti_registrati_t Utenti)
{
    for (size_t i = 0; i < MAX_USERS; i++)
    {
        //posizione non  vuota
        if(Utenti.elenco[i] != NULL)
        {
            if(Utenti.elenco[i]->isOnline)
                fprintf(fout, "%s\n",Utenti.elenco[i]->nickname);
        }
    }
}
