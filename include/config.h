/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */
/**
 * @file config.h
 * @brief File contenente alcune define con valori massimi utilizzabili
 */

#if !defined(CONFIG_H_)
#define CONFIG_H_

#define MAX_NAME_LENGTH 32
#define UNIX_PATH_MAX 64

// to avoid warnings like "ISO C forbids an empty translation unit"x
typedef int make_iso_compilers_happy;

#endif /* CONFIG_H_ */
