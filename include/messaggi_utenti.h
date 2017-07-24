/**
 * @file  messaggi_utenti.h
 * @brief Header file modulo scambio messaggi tra utenti di chatty.
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef MESS_UTENTI_H_
#define MESS_UTENTI_H_

#include<stdlib.h>
#include"ops.h"

/* Numero di byte che servono per memorizzare in una stringa il tipo di un messaggio, in un file */
#define MSG_TYPE_SPACE 15
/* Numero di byte che servono per memorizzare in una stringa la size di un messaggio, in un file */
#define MSG_SIZE_SPACE 6

/**
 * @struct messaggio_id_t
 * @brief identifica il tipo del messaggio
 */
typedef enum{
    TEXT_ID,
    FILE_ID
}messaggio_id_t;

/**
 * @function checkSender
 * @brief Controllo che il sender di un messaggio sia valido,ovvero sia
 *        registrato ed online.
 * @param sender_name nome del sender
 * @param utenti elenco utenti registrati
 * @param pos posizione all'interno dell'elenco utenti registrati
 * @return se il sender e' valido ritorna un puntatore all'utente sender,
 *         altrimenti ritorna NULL e setta errno.
 * @note se il sender non e' online errno viene settato su ENETDOWN.
 * @warning nel caso venisse ritornato il puntatore all'utente sender,la
 *          la lock su di esso e' gia' acquisita.RILASCIARE LOCK SENDER NON
 *          APPENA FINITO DI UTILIZZARE IL SENDER.
 */
utente_t *checkSender(char *sender_name,utenti_registrati_t *utenti,int *pos);

/**
 * @function send_ok_message
 * @brief Invia un messagio di operazione andata a buon fine al client.
 * @param fd fd del client
 * @param buf buffer del messaggio di risposta,puo' essere anche vuoto.
 * @param len lunghezza del messaggio.
 * @return 0 in caso di successo,altrimenti -1 e setta errno.
 */
int send_ok_message(int fd,char *buf,unsigned int len);

/**
 * @function send_fail_message
 * @brief Invia un messaggio di operazioe fallita al client.
 * @param fd fd del client
 * @param op_fail tipo di errore
 * @param utenti elenco utenti registrati
 * @return 0 in caso di successo,altrimenti -1 e setta errno.
 */
int send_fail_message(int fd,op_t op_fail,utenti_registrati_t *utenti);

/**
 * @function sendUserOnline
 * @brief Invia al client la lista degli utenti attualmente online
 * @param fd fd del client
 * @param utenti elenco utenti registrati
 * @return 0 in caso di successo,altrimenti -1 e setta errno.
 */
int sendUserOnline(int fd,utenti_registrati_t *utenti);

/**
 * @function inviaMessaggioUtente
 * @brief Effettua l'operazione di invio di un messaggio di qualsiasi tipo,
 *        da un utente mittente ad un utente destinatario
 * @param sender nome utente mittente
 * @param receiver nome utente destinatario
 * @param msg data del messaggio
 * @param size_msg dimensione messaggio
 * @param type tipo del messaggio
 * @param utenti elenco utenti registrati
 * @return 0 messaggio inviato correttamente,altrimenti -1 e setta errno.
 * @note per messaggio inviato correttamente si intende quando viene mandato un messaggio
 *       con send_ok_message o send_fail_message.
 */
int inviaMessaggioUtente(char *sender,char *receiver,char *msg,size_t size_msg,messaggio_id_t type,utenti_registrati_t *utenti);

/**
 * @function inviaMessaggioUtentiRegistrati
 * @brief Invia un messaggio testuale a tutti gli utenti registrati.
 * @param sender_name nome utente mittente
 * @param msg data del messaggio
 * @param size_msg dimensione messaggio
 * @utenti elenco utenti registrati
 * @return 0 messaggi inviati tutti correttamente,altrimenti -1 e setta errno.
 */
int inviaMessaggioUtentiRegistrati(char *sender_name,char *msg,size_t size_msg,utenti_registrati_t *utenti);

/**
 * @function getFile
 * @brief Restituisce un file richiesto da un utente.
 * @param sender_name nome dell'utente che ha effettuato la richiesta
 * @param filename noem del file richiesto
 * @param utenti elenco utenti registrati
 * @return 0 file restituito con successo,altrimenti -1 e setta errno.
 */
int getFile(char *sender_name,char *filename,utenti_registrati_t *utenti);

/**
 * @function inviaMessaggiRemoti
 * @brief Invia tutti i messaggi salvati in remoto dal server, relativi ad un utente.
 * @param sender_name nome utente he effettua la richiesta
 * @param utenti elenco utenti registrati
 * @return 0 messaggi inviati con successo,altrimenti -1 e setta errno.
 */
int inviaMessaggiRemoti(char *sender_name,utenti_registrati_t *utenti);

#endif /*MESS_UTENTI_H_ */
