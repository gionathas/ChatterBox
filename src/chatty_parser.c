/**
 * @file chatty_parser.c
 * @brief Implementazione del parser del file per configurare il server chatty
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */

#include<string.h>
#include<stdlib.h>
#include<ctype.h>
#include<stdio.h>
#include<errno.h>
#include"chatty_task.h"
#include"config.h"
#include"utils.h"

/**
 * @function read_field
 * @brief legge un campo all'interno della riga e lo salva dentro field
 * @param field buffer in cui salvare il campo letto
 * @param buffer locazione di memoria del puntatore alla riga letta
 * @note si passa **buffer perche' bisogna aggiornare la riga che stiamo leggendo
 * @return 0 campo letto con successo,altrimenti -1 e setta errno
 */
static int read_field(char *field,char **buffer)
{
    int count = 0;

    //Vado avanti fin quando non incontro di nuovo un white space,oppure arrivo alla fine
    while(!isspace(**buffer) && **buffer != '\0')
    {
        //assegno valore carattere
        *field = **buffer;
        //incremento puntatori e contatore
        ++field;
        ++(*buffer);
        ++count;

        //controllo di non eccedere il buffer
        if(count > MAX_SIZE_FIELD)
        {
            errno = ENOBUFS;
            return -1;
        }
    }

    return 0;
}

/**
 * @function check_value
 * @brief Legge un valore numerico da una campo
 * @param field stringa del campo da cui si Legge
 * @return valore letto,altrimenti -1 e setta errno.
 */
static int check_value(char *field)
{
    int val;

    //trasformo in numero
    val = atoi(field);

    //errore atoi
    if( (val == 0 && *field != '0') || (val < 0))
    {
        errno = EINVAL;
        return -1;
    }

    return val;
}

/**
 * @function analyze_field
 * @brief Analizza i campi relativi ad un assegnamento e aggiorna la configurazioe del server
 * @param sx_field campo sinistro assegnamento
 * @param dx_field campo destro assegnamento
 * @param config puntatore alla struttura relativa alla configurazioe del server
 * @return 0 assegnamento corretto,altrimenti -1 e setta errno.
 */
static int analyze_field(char *sx_field,char *dx_field,server_config_t *config)
{
    int check; //per controllare valori aritmetici

    if(strcmp(sx_field,"UnixPath") == 0)
    {
        //memorizzo il server path
        strncpy(config->serverpath,dx_field,UNIX_PATH_MAX);
    }
    else if(strcmp(sx_field,"MaxConnections") == 0)
    {
        check = check_value(dx_field);

        //errore nel controllo del valore
        error_handler_1(check,-1,-1);

        config->max_connection = check;
    }
    else if(strcmp(sx_field,"ThreadsInPool") == 0)
    {
        check = check_value(dx_field);

        //errore nel controllo del valore
        error_handler_1(check,-1,-1);

        config->threads = check;
    }
    else if(strcmp(sx_field,"MaxMsgSize") == 0)
    {
        check = check_value(dx_field);

        //errore nel controllo del valore
        error_handler_1(check,-1,-1);

        //numero di byte
        config->max_msg_size = check * sizeof(char);
    }
    else if(strcmp(sx_field,"MaxFileSize") == 0)
    {
        check = check_value(dx_field);

        //errore nel controllo del valore
        error_handler_1(check,-1,-1);

        size_t kb = 1024 * sizeof(char);

        //trasformo in kilobyte
        config->max_file_size = check * kb;
    }
    else if(strcmp(sx_field,"MaxHistMsgs") == 0)
    {
        check = check_value(dx_field);

        //errore nel controllo del valore
        error_handler_1(check,-1,-1);

        config->max_hist_msgs = check;
    }
    else if(strcmp(sx_field,"DirName") == 0)
    {
        //memorizzo il path della directory del server
        strncpy(config->dirname,dx_field,MAX_SERVER_DIR_LENGTH + 1);
    }
    else if(strcmp(sx_field,"StatFileName") == 0)
    {
        //memorizzo il path della directory del server
        strncpy(config->stat_file_name,dx_field,UNIX_PATH_MAX);
    }

    return 0;
}

 int chatty_parser(char *pathfile,server_config_t *config)
 {
     FILE *config_file; //file di configurazione
     char buffer[MAX_SIZE_LINE + 1];//buffer per salvare le righe lette nel file
     char *buff,*sx,*dx; //puntatori di supporto all'analisi delle stringhe
     int rc; //gestione errori

     //controllo parametri
     if(pathfile == NULL || config == NULL)
     {
         errno = EINVAL;
         return -1;
     }

     //apro il file in sola lettura
     config_file = fopen(pathfile,"r");

     //errore apertura
     error_handler_1(config_file,NULL,-1);

     //leggo fin quando non arrivo alla fine del file
     while(fgets(buffer,MAX_SIZE_LINE,config_file) != NULL)
     {
        //analizzo la stringa

        buff = buffer;

        //fin quando trovo spazi,vado avanti
        while(isspace(*buff))
        {
            ++buff;
        }

        //se si tratta di un commento,oppure di una riga vuota vado alla prossima riga
        if(*buff == '#' || *buff == '\0' )
            continue;

        else
        {
            char sx_field[MAX_SIZE_FIELD] = ""; //campo sinistro assegnamento
            char dx_field[MAX_SIZE_FIELD] = ""; //campo destro assegnamento

            sx = sx_field;
            dx = dx_field;

            //a questo punto ho trovato una lettera.

            //Quello che sto per leggere lo salvo dentro al campo sinistro
            rc = read_field(sx,&buff);

            //errore lettura campo
            error_handler_1(rc,-1,-1);

            //finito di leggere il campo sinistro,vado avanti fin quando non arrivo al campo destro

            //stavolta oltre ai white space,salto anche '='
            while(isspace(*buff) || *buff == '=')
                ++buff;

            //leggo il campo destro
            rc = read_field(dx,&buff);

            //errore lettura campo
            error_handler_1(rc,-1,-1);

            //a questo punto analizzo i due campi
            rc = analyze_field(sx,dx,config);

            //errore nell'analisi dei campi
            error_handler_1(rc,-1,-1);

        }
     }

     //chiudo file
     fclose(config_file);

     return 0;
}
