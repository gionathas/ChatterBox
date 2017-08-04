/**
 * @file  utils.h
 * @brief File che contiene alcune macro e funzioni utili: error handling,ecc..
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef UTILS_H
#define UTILS_H

#include<errno.h>
#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include<math.h>
#include"threadpool.h"

#define NO_ERROR_THREADS 0
#define SYS_ERROR -1

/* Gestione errori generiche */
#define error_handler_1(err,val,ret)            \
    do{if( (err) == (val) ){return (ret);} }while(0)

#define error_handler_2(err,val,ret,err_no)         \
    do{if( (err) == (val) ){errno = err_no;return (ret);} }while(0)

#define error_handler_3(err,ret)         \
    do{if(err){errno = err;return (ret);} }while(0)

/* Gestione degli errori per chiamate di sistema */
#define err_meno1_ex(s,m) \
    if ( (s) == SYS_ERROR ) {perror(m); exit(EXIT_FAILURE);}

#define th_error_main(err,m)   \
    if((err) != NO_ERROR_THREADS) {errno=(err);perror(m);exit(EXIT_FAILURE);}

/**
 * @function hash
 * @brief Algoritmo di hashing per stringhe
 * @author Dan Bernstein
 * @return codice hash per accedere ad un particolare elemento dell'elenco degli utenti
 */
static inline unsigned int hash(char *str,int max)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash % max;
}

/**
 * @function numOfDigits
 * @brief Restituisce il numero di cifre che compongono un numero
 */
static inline int numOfDigits(int x)
{
    return (floor(log10 (abs (x))) + 1);
}


#endif
