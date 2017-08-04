/**
 * @file  gestione_gruppi.c
 * @brief Implementazione modulo per la gestione dei gruppi di chatty
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include<pthread.h>
#include"utils.h"
#include"gruppi.h"

gruppi_registrati_t *inizializzaGruppiRegistrati()
{
     int rc;

     gruppi_registrati_t *gruppi = malloc(sizeof(gruppi_registrati_t));

     //allocazione andata male
     error_handler_1(gruppi,NULL,NULL);

     //alloco spazio per memorizzare l'elenco dei gruppi registrati
     gruppi->elenco = calloc(MAX_GROUPS,sizeof(gruppo_t));

     //controllo esito allocazione
     if(gruppi->elenco == NULL)
     {
         free(gruppi);
         return NULL;
     }

     //inizializzo tutti i mutex dei gruppi
     for (size_t i = 0; i < MAX_GROUPS; i++)
     {
         rc = pthread_mutex_init(&gruppi->elenco[i].mtx,NULL);

         //errore init mutex
         if(rc)
         {
             //distruggo mutex creati fin ora
             for (size_t j = 0; j < i; j++)
             {
                 pthread_mutex_destroy(&gruppi->elenco[j].mtx);
             }

             free(gruppi->elenco);
             free(gruppi);

             errno = rc;
             return NULL;
         }
     }

     return gruppi;
 }

//puntatore al gruppo con lock su di esso,NULL gruppo non trovato, NULL + errno errore
struct gruppo *cercaGruppo(char *name,gruppi_registrati_t *gruppi)
{
    int rc;

    //controllo parametri
    if (name == NULL || gruppi == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    int hashIndex = hash(name,MAX_GROUPS);
    int count = 0; //contatore di supporto

    //prendo lock sul primo gruppo che analizzo con l'indice trovato
    rc = pthread_mutex_lock(&gruppi->elenco[hashIndex].mtx);

    error_handler_3(rc,NULL);

    //scorro la lista di gruppi,fin quando non ne trovo uno vuoto
    while(gruppi->elenco[hashIndex].numero_iscritti != 0)
    {
        //i nomi corrispondono, ho trovato il gruppo
        if(strcmp(gruppi->elenco[hashIndex].name,name) == 0)
        {
            //ritorno il gruppo cercato con la lock su di esso
            return &gruppi->elenco[hashIndex];
        }

        //altrimenti lascio la lock
        pthread_mutex_unlock(&gruppi->elenco[hashIndex].mtx);

        //incremento gli indici per scorrere
        ++hashIndex;
        hashIndex %= MAX_GROUPS;
        count++;

        //caso in cui ho controllato tutti i gruppi e non ho trovato quello che cercavo
        if(count == MAX_GROUPS)
            break;

        //riprendo la lock per andare a controllare il prossimo gruppo
        rc = pthread_mutex_lock(&gruppi->elenco[hashIndex].mtx);

        error_handler_3(rc,NULL);
    }

    //rilascio la lock sull'ultimo gruppo controllato
    pthread_mutex_unlock(&gruppi->elenco[hashIndex].mtx);

    //gruppo non trovato
    return NULL;
}

//1 gruppo gia' registrato,0 gruppo registrato correttamente, -1 errore,-1 + EPERM troppi gruppi
int RegistraGruppo(utente_t *utente_creatore,char *groupname,gruppi_registrati_t *gruppi)
{
    int rc;

    if(utente_creatore == NULL || groupname == NULL || gruppi == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    //se l'utente risulta aver superato i gruppi a cui iscritto allora,non puo' creare il gruppo
    if(utente_creatore->n_gruppi_iscritto == MAX_GROUPS_FOR_USER)
    {
        errno = EPERM;
        return -1;
    }

    //controllo che se esiste gia un gruppo con quel nome
    gruppo_t *already_reg_gr = cercaGruppo(groupname,gruppi);

    //gruppo gia' registrato
    if(already_reg_gr != NULL)
    {
        //rilascio lock sul gruppo trovato
        pthread_mutex_unlock(&already_reg_gr->mtx);
        return 1;
    }
    else if(already_reg_gr == NULL && errno != 0)
    {
        //errore nella ricerca del gruppo
        return -1;
    }
    //gruppo non ancora registrato,procedo con la registrazione
    else
    {
        int hashIndex = hash(groupname,MAX_GROUPS);
        int count = 0;//contatore di supporto

        //prendo lock sulla posizione ottenuta con l'hash
        rc = pthread_mutex_lock(&gruppi->elenco[hashIndex].mtx);

        error_handler_3(rc,-1);

        //finche trovo posizioni occupate da latri gruppi,continuo a scorrere
        while(gruppi->elenco[hashIndex].numero_iscritti != 0)
        {
            //rilascio lock gruppo attuale controllato
            pthread_mutex_unlock(&gruppi->elenco[hashIndex].mtx);

            //incremento indici
            ++hashIndex;
            hashIndex %= MAX_GROUPS;
            ++count;

            //non ci sono piu spazi per registrare gruppi
            if(count == MAX_GROUPS)
            {
                errno = ENOMEM;
                return -1;
            }

            //riprendo lock per controllare prossimo gruppo
            rc = pthread_mutex_lock(&gruppi->elenco[hashIndex].mtx);

            error_handler_3(rc,-1);
        }

        //posizione trovata,procedo con la registrazione del gruppo

        //registro nome gruppo
        strncpy(gruppi->elenco[hashIndex].name,groupname,MAX_NAME_LENGTH);
        //registro nome admin
        strncpy(gruppi->elenco[hashIndex].admin,utente_creatore->nickname,MAX_NAME_LENGTH);
        //registro admin nella lista degli iscritti al gruppo, in prima posizione
        strncpy(gruppi->elenco[hashIndex].iscritti[0],utente_creatore->nickname,MAX_NAME_LENGTH);
        gruppi->elenco[hashIndex].numero_iscritti = 1;

        //aggiorno i gruppi a cui e' iscritto l'utente admin
        utente_creatore->gruppi_iscritto[utente_creatore->n_gruppi_iscritto] = &gruppi->elenco[hashIndex];
        ++utente_creatore->n_gruppi_iscritto;

        //rilascio lock sul gruppo appena creato
        pthread_mutex_unlock(&gruppi->elenco[hashIndex].mtx);
    }

    return 0;
}

//1 gruppo non trovato,0 op ok,EPERM troppri gruppi iscritto,EALREADY gia iscritto
int iscrizioneGruppo(utente_t *utente,char *groupname,gruppi_registrati_t *gruppi)
{
    int rc;

    if(utente == NULL || groupname == NULL || gruppi == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    //controllo che l'utente non risulti gia' essere registrato a quel gruppo
    for (size_t i = 0; i < utente->n_gruppi_iscritto; i++)
    {
        rc  = pthread_mutex_lock(&utente->gruppi_iscritto[i]->mtx);

        error_handler_3(rc,-1);

        //se trovo il gruppo nella lista dei gruppi a cui sono iscritto
        if( strcmp(utente->gruppi_iscritto[i]->name,groupname) == 0)
            errno = EALREADY;

        pthread_mutex_unlock(&utente->gruppi_iscritto[i]->mtx);

        if(errno)
            return -1;
    }

    //controllo che l'utente risulta non aver superato i gruppi a cui e' attualmente iscritto
    if(utente->n_gruppi_iscritto == MAX_GROUPS_FOR_USER)
    {
        errno = EPERM;
        return -1;
    }

    //controllo che il gruppo a cui mi voglio iscrivere esista
    gruppo_t *gruppo = cercaGruppo(groupname,gruppi);

    if(gruppo == NULL)
    {
        //errore ricerca gruppo
        if(errno != 0)
        {
            return -1;
        }
        //gruppo non esistente
        else
            return 1;
    }

    //il gruppo esiste,procedo all'iscrizione

    //registro utente attuale negli iscritti al gruppo
    strncpy(gruppo->iscritti[gruppo->numero_iscritti],utente->nickname,MAX_NAME_LENGTH);
    //incremento numero iscritti del gruppo
    ++gruppo->numero_iscritti;
    //salvo questo gruppo nella lista dei gruppi a cui e' iscritto l'utente
    utente->gruppi_iscritto[utente->n_gruppi_iscritto] = gruppo;
    //incremento numero di gruppi a cui sono iscritto
    ++utente->n_gruppi_iscritto;

    //rilascio lock gruppo
    pthread_mutex_unlock(&gruppo->mtx);

    return 0;

}

//1 non registrato al gruppo,ENOENT gruppo non trovato
int disiscrizioneGruppo(utente_t *utente,char *groupname,gruppi_registrati_t *gruppi)
{
    int rc;
    gruppo_t *gruppo = NULL;

    if(utente == NULL || groupname == NULL || gruppi == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    //cerco il gruppo tra quelli a cui sono registrato
    for (size_t i = 0; i < utente->n_gruppi_iscritto; i++)
    {
        gruppo = utente->gruppi_iscritto[i];

        rc = pthread_mutex_lock(&gruppo->mtx);

        error_handler_3(rc,-1);

        //se ho trovato il gruppo da cui mi voglio deregistrare, mi fermo
        if(strcmp(groupname,gruppo->name) == 0)
        {
            //rimuovo il gruppo da quelli di cui l'utente risulta iscritto
            for (size_t j = i+1; j < utente->n_gruppi_iscritto ; j++)
            {
                utente->gruppi_iscritto[j-1] = utente->gruppi_iscritto[j];
            }

            //decremento il numero dei gruppi a cui risulta iscritto l'utente
            --utente->n_gruppi_iscritto;

            break;
        }


        pthread_mutex_unlock(&gruppo->mtx);
    }

    //se non ho trovato il gruppo
    if(gruppo == NULL)
    {
        errno = ENOENT;
        return -1;
    }

    //gruppo trovato,procedo alla disiscrizione

    //rimuovo dalla lista degli iscritti al gruppo
    for (size_t i = 0; i < gruppo->numero_iscritti; i++)
    {
        //trovato utente,tra gli iscritti
        if(strcmp(utente->nickname,gruppo->iscritti[i]) == 0)
        {
            //decremento tutti i restanti iscritti di una posizione
            for (size_t j = i+1; j < gruppo->numero_iscritti; j++)
            {
                //decremento la posizione dell'iscritto attuale nella lista iscritti
                strcpy(gruppo->iscritti[j-1],gruppo->iscritti[j]);
            }

            //decremento numero di iscritti al gruppo
            --gruppo->numero_iscritti;
        }
    }

    //se il gruppo non e' rimasto vuoto,aggiorno anche l'admin
    if(gruppo->numero_iscritti != 0)
    {
        strcpy(gruppo->admin,gruppo->iscritti[0]);
    }

    //rilascio lock sul gruppo
    pthread_mutex_unlock(&gruppo->mtx);

    return 0;

}

void mostraGruppi(gruppi_registrati_t *gruppi)
{
    printf("######################\n");

    for (size_t i = 0; i < MAX_GROUPS; i++)
    {
        gruppo_t *gruppo = &gruppi->elenco[i];

        pthread_mutex_lock(&gruppo->mtx);

        if(gruppi->elenco[i].numero_iscritti != 0)
        {
            printf("[%s]: admin:%s ",gruppo->name,gruppo->admin);

            printf("iscritti: ");
            for (size_t j = 0; j < gruppo->numero_iscritti; j++)
            {
                printf("%s ",gruppo->iscritti[j]);
            }
            printf("\n");
        }

        pthread_mutex_unlock(&gruppo->mtx);
    }

    printf("######################\n");

}

int eliminaGruppi(gruppi_registrati_t *gruppi)
{
     int rc;

     if(gruppi == NULL)
     {
         errno = EINVAL;
         return -1;
     }

     //distruggo tutti i mutx relativi ad ogni gruppo
     for (size_t i = 0; i < MAX_GROUPS; i++)
     {
         rc = pthread_mutex_destroy(&gruppi->elenco[i].mtx);

         //esito init
         if(rc)
         {
             free(gruppi->elenco);
             free(gruppi);
             errno = rc;
             return -1;
         }
     }

     //libero memoria
     free(gruppi->elenco);
     free(gruppi);

     return 0;
 }
