/**
 * @file  gruppi.h
 * @brief Header file del modulo che implementa i gruppi di chatty
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
 #ifndef GRUPPI_H_
 #define GRUPPI_H_

 #include<pthread.h>
 #include"utenti.h"
 #include"config.h"

 typedef struct utente utente_t;

 struct gruppo{
     char name[MAX_NAME_LENGTH + 1];
     pthread_mutex_t mtx;
     char admin[MAX_NAME_LENGTH + 1];
     char iscritti[MAX_NAME_LENGTH + 1][MAX_USER_FOR_GROUP];
     unsigned int numero_iscritti; //se 0 il gruppo puo' essere utilizzato per crearne uno nuovo
 };//gruppo_t

typedef struct{
    struct gruppo *elenco;
}gruppi_registrati_t;

gruppi_registrati_t *inizializzaGruppiRegistrati();
struct gruppo *cercaGruppo(char *name,gruppi_registrati_t *gruppi);
int iscrizioneGruppo(utente_t *utente,char *groupname,gruppi_registrati_t *gruppi);
int disiscrizioneGruppo(utente_t *utente,char *groupname,gruppi_registrati_t *gruppi);
int eliminaGruppi(gruppi_registrati_t *gruppi);
int RegistraGruppo(utente_t *utente_creatore,char *groupname,gruppi_registrati_t *gruppi);
void mostraGruppi(gruppi_registrati_t *gruppi);

#endif
