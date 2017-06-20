#include<pthread.h>
#include"config.h"

typedef struct{
    char nickname[MAX_NAME_LENGTH];
    unsigned short isOnline; //0 offline,1 online
    int fd; //descrittore relativo al client di quell'utente
    pthread_mutex_t mtx; //mutex per accedere ai dati dell'utente
}utente_t;

typedef struct{
    utente_t *elenco[MAX_USERS]; //elenco utenti registrati
    unsigned long *utenti_registrati;
    unsigned long *utenti_online;
    pthread_mutex_t *mtx;
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

utente_t *cercaUtente(char *name,utenti_registrati_t Utenti);
int registraUtente(char *name,int fd,utenti_registrati_t Utenti);
int deregistraUtente(char *name,utenti_registrati_t Utenti);
void mostraUtenti(FILE *fout,utenti_registrati_t Utenti);
void mostraUtentiOnline(FILE *fout,utenti_registrati_t Utenti);
int connectUtente(char *name,int fd,utenti_registrati_t Utenti);
int disconnectUtente(char *name,utenti_registrati_t Utenti);
