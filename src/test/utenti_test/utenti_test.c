#include<pthread.h>
#include<errno.h>
#include<stdio.h>
#include<string.h>
#include"utenti.h"

#define MAX_USER 5

int main()
{
    utenti_registrati_t *utenti;
    int rc;
    unsigned long reg = 0,onl = 0;
    pthread_mutex_t mtx;

    //per stringa utenti
    char buff[1000] ="";
    int buff_size = 0;
    size_t nbuff = 1000;

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
    connectUtente("Marco",3,utenti);

    rc = mostraUtentiOnline(buff,&nbuff,&buff_size,utenti);

    if(rc == -1)
    {
        perror("show users");
        return -1;
    }

    /* Per stampare utenti online */
    int nusers = buff_size / (33);

    printf("buff %d nusers %d\n",(buff_size),nusers );

    for(int i=0,p=0;i<nusers; ++i, p+=(33)) {
	    printf("%s\n", &buff[p]);
	}

    printf("Utenti Registrati = %ld\nUtenti online = %ld\n",reg,onl);

    return eliminaElenco(utenti);

}
