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

#define err_null_ex(s,m) \
    if((s) == NULL) {perror(m); exit(EXIT_FAILURE);}

#define print_err_ex(s)\
    {fprintf(stderr,"%s\n",(s));exit(EXIT_FAILURE);}

/* Gestione degli errori per i threads*/
#define th_error(err,m)     \
    if((err) != NO_ERROR_THREADS) {errno=(err);perror(m);pthread_exit((void*)EXIT_FAILURE);}

#define th_null(s,m)    \
    if((s) == NULL) {perror(m);pthread_exit((void*)EXIT_FAILURE);}

#define th_error_main(err,m)   \
    if((err) != NO_ERROR_THREADS) {errno=(err);perror(m);exit(EXIT_FAILURE);}


#endif
