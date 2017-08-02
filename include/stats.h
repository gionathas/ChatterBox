/**
 * @file  stats.h
 * @brief Header file delle statistiche relative al server chatty
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef SERVER_STATS_H
#define SERVER_STATS_H

#include<stdio.h>
#include<time.h>
#include<pthread.h>
#include<errno.h>
#include"utils.h"

struct statistics {
    unsigned long nusers;                       // n. di utenti registrati
    unsigned long nonline;                      // n. di utenti connessi
    unsigned long ndelivered;                   // n. di messaggi testuali consegnati
    unsigned long nnotdelivered;                // n. di messaggi testuali non ancora consegnati
    unsigned long nfiledelivered;               // n. di file consegnati
    unsigned long nfilenotdelivered;            // n. di file non ancora consegnati
    unsigned long nerrors;                      // n. di messaggi di errore
};

/**
 * @function printStats
 * @brief Stampa le statistiche nel file passato come argomento
 *
 * @param fout descrittore del file aperto in append.
 *
 * @return 0 in caso di successo, -1 in caso di fallimento
 */
static inline int printStats(char* fout_path)
{
    extern struct statistics chattyStats;
    extern pthread_mutex_t mtx_chatty_stat;
    int rc;

    //creo il file dove andro' scrivere
    FILE *file = fopen(fout_path,"w");

    //errore creazione file stati
    error_handler_1(file,NULL,-1);

    rc = pthread_mutex_lock(&mtx_chatty_stat);

    //errore lock su statistiche
    if(rc)
    {
        fclose(file);
        errno = rc;
        return -1;
    }

    if (fprintf(file, "%ld - %ld %ld %ld %ld %ld %ld %ld\n",
        (unsigned long)time(NULL),
		chattyStats.nusers,
		chattyStats.nonline,
		chattyStats.ndelivered,
		chattyStats.nnotdelivered,
		chattyStats.nfiledelivered,
		chattyStats.nfilenotdelivered,
		chattyStats.nerrors
    ) < 0)
    {//errore scrittura
        fclose(file);
        pthread_mutex_unlock(&mtx_chatty_stat);
        return -1;
    }


    fflush(file);

    pthread_mutex_unlock(&mtx_chatty_stat);

    fclose(file);

    return 0;
}

#endif /* SERVER_STATS_ */
