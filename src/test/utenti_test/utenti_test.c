#include<pthread.h>
#include<errno.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include"utenti.h"
#include"stats.h"

#define MAX_USER 5

int main()
{
    utenti_registrati_t *utenti;
    int rc;

    //per stringa utenti
    char buff[1000] ="";
    int buff_size = 0;
    size_t nbuff = 1000;


    //per statistics
    struct statistics Stats = { 0,0,0,0,0,0,0 };
    pthread_mutex_t mtx_stat;


    pthread_mutex_init(&mtx_stat,NULL);

    utenti = inizializzaUtentiRegistrati(100,100,100,&Stats,&mtx_stat,"/tmp/chatty");

    if(utenti == NULL)
    {
        perror("creazione elenco utenti");
        return -1;
    }


    rc = registraUtente("Gio",3,utenti);

    if(rc == -1)
    {
        perror("registraut");
        return -1;
    }

    sleep(1);
    registraUtente("Anna",2,utenti);
    sleep(1);
    registraUtente("Marco",4,utenti);
    registraUtente("Nino",6,utenti);

    sleep(3);

    deregistraUtente("Nino",utenti);
    deregistraUtente("Marco",utenti);

    /*
    rc = mostraUtentiOnline(buff,&nbuff,&buff_size,utenti);

    if(rc == -1)
    {
        perror("show users");
        return -1;
    }

    // Per stampare utenti online
    int nusers = buff_size / (33);

    printf("buff %d nusers %d\n",(buff_size),nusers );

    for(int i=0,p=0;i<nusers; ++i, p+=(33)) {
	    printf("%s\n", &buff[p]);
	}

    */

    printf("Utenti Registrati = %ld\nUtenti online = %ld\n",Stats.nusers,Stats.nonline);
    mostraUtenti(utenti);

    sleep(5);

    eliminaElenco(utenti);

    return 0;

}
