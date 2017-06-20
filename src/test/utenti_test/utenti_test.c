#include<pthread.h>
#include<errno.h>
#include<stdio.h>
#include"utenti.h"

int main()
{
    utenti_registrati_t utenti;
    int rc;

    //init
    *(utenti.utenti_registrati) = 0;
    *utenti.utenti_online = 0;
    rc = pthread_mutex_init(&(*(utenti.mtx)),NULL);

    if(rc)
    {
        errno = rc;
        perror("on init mutex");
        return -1;
    }

    registraUtente("Gio",utenti);
    registraUtente("Anna",utenti);

    mostraUtenti(utenti);

    return 0;

}
