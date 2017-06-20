#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include"utenti.h"
#include"config.h"

utente_t *cercaUtente(char *key,utenti_registrati_t Utenti)
{
    //otteniamo l'hash per la ricerca
    int hashIndex = hash(key);

    //cerchiamo nell'array fino a quando non troviamo uno spazio vuoto
    while(Utenti.elenco[hashIndex] != NULL)
    {
        //se troviamo la chiave corrispondente allora abbiamo trovato l'utente
        if(strcmp(Utenti.elenco[hashIndex]->key,key) == 0)
        {
            return Utenti.elenco[hashIndex];
        }

        //altrimenti andiamo avanti con le posizioni
        ++hashIndex;

        //facciamo il modulo per non traboccare fuori
        hashIndex %= MAX_USERS;
    }

    //utente non trovato
    return NULL;
}

int inserisciUtente(char *name,utenti_registrati_t Utenti)
{
    //creo utente
    utente_t *utente = malloc(sizeof(utente_t));

    if(utente == NULL)
        return -1;

    utente->key = name;
    strncpy(utente->info.nickname ,name,MAX_NAME_LENGTH);
    utente->info.isOnline = 1;


    //hash per la ricerca
    int hashIndex = hash(utente->key);
    int count = 0; //contatore per vedere se la elenco utenti e' piena

    //finche trovo posizioni non libere e o che sono state cancellate,vado avanti
    while( (Utenti.elenco[hashIndex] != NULL) && (strcmp(Utenti.elenco[hashIndex]->key,"") != 0) )
    {
        ++hashIndex;
        hashIndex %= MAX_USERS;
        count++;

        if(count > MAX_USERS)
        {
            errno = ENOMEM;
            return -1;
        }
    }

    //posizione trovata, inserisco
    Utenti.elenco[hashIndex] = utente;

    return 0;
}

int rimuoviUtente(char *key,utenti_registrati_t Utenti)
{
    //ottengo posizione tramite hash
    int hashIndex = hash(key);

    //cerco elemento
    while(Utenti.elenco[hashIndex] != NULL)
    {
        //utenet trovato?
        if(strcmp(Utenti.elenco[hashIndex]->key,key) == 0)
        {
            //per eliminare setto la chiave con un stringa vuota
            Utenti.elenco[hashIndex]->key = "";
            return 0;
        }

        //altrimenti proseguo nella ricerca
        ++hashIndex;
        hashIndex %= MAX_USERS;
    }

    //utente non trovato
    return -1;
}

void mostraUtenti(utenti_registrati_t Utenti)
{
    for (size_t i = 0; i < MAX_USERS; i++)
    {
        //posizione vuota
        if(Utenti.elenco[i] != NULL)
        {
            //poszione valida
            if(strcmp(Utenti.elenco[i]->key,"") != 0 )
            {
                printf("%s ",Utenti.elenco[i]->info.nickname);

                //online?
                if(Utenti.elenco[i]->info.isOnline)
                    printf("online\n");
                else
                    printf("offline\n");
            }
        }
    }
}
