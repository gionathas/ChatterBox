#include"config.h"

typedef struct{
    char nickname[MAX_NAME_LENGTH];
    unsigned short isOnline; //0 offline,1 online
}info_utente_t;

typedef struct{
    char *key;//chiave per hash,corrisponde al nickname
    info_utente_t info;
}utente_t;

typedef struct{
    utente_t *elenco[MAX_USERS]; //elenco utenti registrati
}utenti_registrati_t;


//algoritmo di hashing djb2 by Dan Bernstein
static unsigned int hash(char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash % MAX_USERS;
}

utente_t *cercaUtente(char *key,utenti_registrati_t Utenti);
int inserisciUtente(char *name,utenti_registrati_t Utenti);
int rimuoviUtente(char *key,utenti_registrati_t Utenti);
void mostraUtenti(utenti_registrati_t Utenti);
