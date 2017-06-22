/**
 * @file  utenti.h
 * @brief Header file per la gestione degli utenti di chatty
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include<pthread.h>
#include"config.h"

/**
 * @struct utente_t
 * @brief struttura utente di chatty
 * @var nickname nickname utente
 * @var isOnline flag per indicare se l'utente e' online
 * @var fd descrittore associato all'utente
 * @note l'fd vale solo se l'utente e' online
 * @var mtx mutex per sincronizzazioni per accesso dati dell'utente
 * @var isInit flag per verifcare se l'utente e' inizializzato o meno
 */
typedef struct{
    char nickname[MAX_NAME_LENGTH + 1];
    unsigned short isOnline; //0 offline,1 online
    int fd; //-1 quando non e' online
    pthread_mutex_t mtx;
    unsigned short isInit;
}utente_t;

/**
 * @struct utenti_registrati_t
 * @brief struttura per gestire utenti registrati di chatty
 * @var elenco elenco utenti registrati
 * @var utenti_registrati puntatore alla variabile che contiene il numero di utenti registrati
 * @var utenti_online  puntatore alla variabile che contiene il numero di utenti online
 * @var mtx puntatore al mutex per accedere ai dati riguardante gli utenti
 * @var max_utenti numero massimo di utenti registrabili
 */
typedef struct{
    utente_t *elenco; //elenco utenti registrati
    unsigned long *utenti_registrati;
    unsigned long *utenti_online;
    pthread_mutex_t *mtx;
    unsigned long max_utenti;
}utenti_registrati_t;

/**
 * @function inizializzaUtentiRegistrati
 * @brief inizializza struttua elenco degli utenti
 * @param max_utenti numero massimo di utenti registrabili
 * @param registrati puntatore alla variabile che contiene utenti registrati
 * @param online puntatore alla variabile che contiene il numero di utenti online
 * @param mtx puntatore al mutex per accedere ai dati riguardante gli utenti
 * return puntatore alla struttua utenti in caso di successo,altrimenti NULL e setta errno
 */
utenti_registrati_t *inizializzaUtentiRegistrati(long max_utenti,unsigned long *registrati,unsigned long *online,pthread_mutex_t *mtx);

/**
 * @function cercaUtente
 * @brief cerca un utente all'interno dei registrati
 * @param name nome utente
 * @param Utenti elenco utenti registrati
 * @return puntatore all'utente in caso sia presente,altrimenti NULL se non e' stato trovato
 * @note se ritorna NULL ed errno e' settatto c'e' stato un errore
*/
utente_t *cercaUtente(char *name,utenti_registrati_t *Utenti);

/**
 * @function registraUtente
 * @brief Registra un utente che non sia gia' presente
 * @param name nome utente
 * @param Utenti Elenco utenti registrati
 * @param fd descrittore relativo al client dell'utente
 * @return 0 utente registrato correttamente,1 nel caso l'utente sia gia' registrato,
 *          -1 in caso di errore e setta errno.
 * @note errno puo essere settato su ENOMEM se l'elenco e'pieno e non c'e' piu spazio
 */
int registraUtente(char *name,int fd,utenti_registrati_t *Utenti);

/**
 * @function deregistraUtente
 * @brief deregistra un utente presente nell'elenco dei registrati
 * @param name nome utente
 * @param Utenti lista utenti registrati
 * @return 0 utente rimosso correttamente,-1 errore e setta errno,1 utente non trovato
 */
int deregistraUtente(char *name,utenti_registrati_t *Utenti);

/**
 * @function mostraUtenti
 * @brief mostra sullo stdout la lista degli utenti registrati e il loro stato
 * @param Utenti elenco utenti
 */
void mostraUtenti(utenti_registrati_t *Utenti);

/**
 * @function mostraUtentiOnline
 * @brief Scrive sulla stringa puntata da buff,la lista degli utenti online
 * @param buff buffer dove scrivere la lista di utenti
 * @param size_buff size del buffer
 * @param new_size puntatore alla nuova size del buffer,dopo la scrittura
 * @param Utenti elenco utenti
 * @return 0 in caso di successo,altrimenti -1 e setta errno
 * @note se lo spazio del buffer non e' sufficiente viene ritornato ENOBUFS
 * @warning la stringa deve essere gia' allocata e deve essere uguale alla stringa vuota.
 * @warning new_size deve essere inizializzato a 0
 */
int mostraUtentiOnline(char *buff,size_t *size_buff,int *new_size,utenti_registrati_t *Utenti);

/**
 * @function connectUtente
 * @brief Effettua la connessione di un utente registrato
 * @param name nome utente
 * @param fd descrittore relativo al client dell'utente
 * @param Utenti elenco utenti registrati
 * @return 0 utente connesso correttamente, -1 errore e setta errno,
 *         altrimenti 1 utente non trovato
 */
int connectUtente(char *name,int fd,utenti_registrati_t *Utenti);

/**
 * @function disconnectUtente
 * @brief Effettua la disconnessione di un utente registrato
 * @param name nome utente
 * @param Utenti elenco utenti registrati
 * @return 0 utente disconnesso correttamente, -1 errore e setta errno,
 *         altrimenti 1 utente non trovato
 */
int disconnectUtente(char *name,utenti_registrati_t *Utenti);

/**
 * @function eliminaElenco
 * @brief dealloca ed elimina tutta la struttura relativa all'elenco degli utenti
 * @param Utenti elenco utenti
 * @return 0 in caso di successo, -1 errore e setta errno
 */
int eliminaElenco(utenti_registrati_t *Utenti);
