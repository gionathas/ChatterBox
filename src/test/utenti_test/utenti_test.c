#include<pthread.h>
#include<errno.h>
#include<stdio.h>
#include"utenti.h"

#define MAX_USER 5

int main()
{
    utenti_registrati_t *utenti;
    int rc;
    unsigned long reg = 0,onl = 0;
    pthread_mutex_t mtx;

    pthread_mutex_init(&mtx,NULL);

    utenti = inizializzaUtentiRegistrati(MAX_USER,&reg,&onl,&mtx);

    if(utenti == NULL)
    {
        perror("creazione elenco utenti");
        return -1;
    }


    registraUtente("Gio",3,utenti);
    registraUtente("Anna",2,utenti);
    registraUtente("Marco",4,utenti);
    registraUtente("Nino",6,utenti);

    disconnectUtente("Nino",utenti);
    disconnectUtente("Marco",utenti);

    mostraUtenti(stdout,utenti);

    printf("\n");
    mostraUtentiOnline(stdout,utenti);

    printf("Utenti Registrati = %ld\nUtenti online = %ld\n",reg,onl);

    return eliminaElenco(utenti);

}
