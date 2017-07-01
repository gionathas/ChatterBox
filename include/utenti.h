/**
 * @file  utenti.h
 * @brief Header file per la gestione degli utenti di chatty
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef UTENTI_H_
#define UTENTI_H_
#include<pthread.h>
#include"config.h"

#define USER_ERR_HANDLER(err,val,ret)         \
    do{if( (err) == (val)){return (ret);} }while(0)

/**
 * @struct utente_t
 * @brief struttura utente di chatty
 * @var nickname nickname utente
 * @var status flag per indicare se lo stato attuale dell'utente
 * @var isInit flag per controllare se un utente e' stato inizializzato
 * @var fd descrittore associato all'utente
 * @note l'fd vale solo se l'utente e' online
 * @var mtx mutex per sincronizzazioni per accesso dati dell'utente
 * @var personal_dir path della directory personale dell'utente
 * @var n_element_in_dir numero di elementi che si trovano nella mia cartella personale
 */
typedef struct{
    char nickname[MAX_NAME_LENGTH + 1];
    unsigned short isOnline; //0 offline,1 online
    unsigned short isInit; //0 unitialized, 1 inizializzata
    unsigned int fd; //0 quando e' offline
    pthread_mutex_t mtx;
    char personal_dir[MAX_CLIENT_DIR_LENGHT];
    unsigned int n_element_in_dir;
}utente_t;

/**
 * @struct utenti_registrati_t
 * @brief struttura per gestire utenti registrati di chatty
 * @var elenco elenco utenti registrati
 * @var utenti_registrati puntatore alla variabile che contiene il numero di utenti registrati
 * @var utenti_online  puntatore alla variabile che contiene il numero di utenti online
 * @var mtx puntatore al mutex per accedere ai dati riguardante gli utenti
 * @var max_msg_size dimensione massima dei messaggi testuali che possono inviarsi gli utenti
 * @var max_file_size dimensione massima dei file che possono inviarsi gli utenti
 * @var max_hist_msgs numero massimo di messaggi e file che il server ricorda per ogni utente
 * @var media_dir directory che contiene file e messaggi per gli utenti
 * @warning il puntatore a stat,va passato con i valori all'interno inizializzati
 */
typedef struct{
    utente_t *elenco;
    struct statistics *stat;
    pthread_mutex_t *mtx_stat;
    size_t max_msg_size;
    size_t max_file_size;
    unsigned int max_hist_msgs;
    char media_dir[MAX_SERVER_DIR_LENGTH];
}utenti_registrati_t;

/**
 * @function inizializzaUtentiRegistrati
 * @brief inizializza struttua elenco degli utenti
 * @param max_utenti numero massimo di utenti registrabili
 * @param registrati puntatore alla variabile che contiene utenti registrati
 * @param online puntatore alla variabile che contiene il numero di utenti online
 * @param mtx puntatore al mutex per accedere ai dati riguardante gli utenti
 * return puntatore alla struttura utenti in caso di successo,altrimenti NULL e setta errno
 */
 utenti_registrati_t *inizializzaUtentiRegistrati(int size_msg,int file_size,int hist_size,struct statistics *stat,pthread_mutex_t *mtx_stat,char *dirpath);

/**
 * @function cercaUtente
 * @brief cerca un utente all'interno dei registrati
 * @param name nome utente
 * @param Utenti elenco utenti registrati
 * @return puntatore all'utente in caso sia registrato,altrimenti NULL se non e' stato ancora registrato
 *
 * @note se ritorna NULL ed errno e' settato c'e' stato un errore.In particolare se errno vale
 *       EPERM,il nome dell'utente non e' valido.
 *
 * @warning se l'utente e' stato trovato viene ritornato con la lock su di esso.
 */
utente_t *cercaUtente(char *name,utenti_registrati_t *Utenti);

/**
 * @function registraUtente
 * @brief Registra un utente che non sia gia' registrato
 * @param name nome utente
 * @param Utenti Elenco utenti registrati
 * @param fd descrittore relativo al client dell'utente
 * @return 0 utente registrato correttamente,1 nel caso l'utente sia gia' registrato,
 *          -1 in caso di errore e setta errno.
 *
 * @note errno puo essere settato su ENOMEM se l'elenco utenti e'pieno e non c'e' piu spazio,
 *             oppure su EPERM se il nome utente non e' valido.
 */
int registraUtente(char *name,unsigned int fd,utenti_registrati_t *Utenti);

/**
 * @function deregistraUtente
 * @brief deregistra un utente registrato nell'elenco dei registrati
 * @param name nome utente
 * @param Utenti lista utenti registrati
 * @return 0 utente rimosso correttamente,-1 errore e setta errno,1 utente non registrato
 *
 * @note se errno vale EPERM il nome utente non e' valido
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
 * @note se lo spazio del buffer non e' sufficiente viene ritornato ENOBUFS
 * @warning la stringa deve essere gia' allocata e deve essere uguale alla stringa vuota.
 * @warning new_size deve essere inizializzato a 0
 * @return 0 in caso di successo,altrimenti -1 e setta errno
 */
int mostraUtentiOnline(char *buff,size_t *size_buff,int *new_size,utenti_registrati_t *Utenti);

/**
 * @function connectUtente
 * @brief Effettua la connessione di un utente registrato
 * @param name nome utente
 * @param fd descrittore relativo al client dell'utente
 * @param Utenti elenco utenti registrati
 * @return 0 utente connesso correttamente, -1 errore e setta errno,
 *         altrimenti 1 utente non registrato
 *
 * @note errno = EPERM se il nome non e' valido.
 */
int connectUtente(char *name,unsigned int fd,utenti_registrati_t *Utenti);

/**
 * @function disconnectUtente
 * @brief Effettua la disconnessione di un utente registrato
 * @param name nome utente
 * @param Utenti elenco utenti registrati
 * @return 0 utente disconnesso correttamente, -1 errore e setta errno,
 *         altrimenti 1 utente non registrato
 * @note errno = EPERM se il nome non e' valido oppure l'utente non e' online
 */
 int disconnectUtente(unsigned int fd,utenti_registrati_t *Utenti);

/**
 * @function eliminaElenco
 * @brief dealloca ed elimina tutta la struttura relativa all'elenco degli utenti
 * @param Utenti elenco utenti
 * @return 0 in caso di successo, -1 errore e setta errno
 */
int eliminaElenco(utenti_registrati_t *Utenti);

#endif /*UTENTI_H_ */
