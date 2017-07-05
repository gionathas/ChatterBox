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

#define PARSER_ERR_HANDLER(err,val,ret)         \
    do{if( (err) == (val)){return (ret);} }while(0)

//bisogna passargli l'indirizzo del puntatore,altrimenti non si aggiorna per il prossimo read field
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
        PARSER_ERR_HANDLER(check,-1,-1);

        config->max_connection = check;
    }
    else if(strcmp(sx_field,"ThreadsInPool") == 0)
    {
        check = check_value(dx_field);

        //errore nel controllo del valore
        PARSER_ERR_HANDLER(check,-1,-1);

        config->threads = check;
    }
    else if(strcmp(sx_field,"MaxMsgSize") == 0)
    {
        check = check_value(dx_field);

        //errore nel controllo del valore
        PARSER_ERR_HANDLER(check,-1,-1);

        //numero di byte
        config->max_msg_size = check * sizeof(char);
    }
    else if(strcmp(sx_field,"MaxFileSize") == 0)
    {
        check = check_value(dx_field);

        //errore nel controllo del valore
        PARSER_ERR_HANDLER(check,-1,-1);

        size_t kb = 1024 * sizeof(char);

        //trasformo in kilobyte
        config->max_file_size = check * kb;
    }
    else if(strcmp(sx_field,"MaxHistMsgs") == 0)
    {
        check = check_value(dx_field);

        //errore nel controllo del valore
        PARSER_ERR_HANDLER(check,-1,-1);

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
     char buffer[MAX_SIZE_LINE];//buffer per salvare le righe lette nel file
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
     PARSER_ERR_HANDLER(config_file,NULL,-1);

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
            PARSER_ERR_HANDLER(rc,-1,-1);

            //finito di leggere il campo sinistro,vado avanti fin quando non arrivo al campo destro

            //stavolta oltre ai white space,salto anche '='
            while(isspace(*buff) || *buff == '=')
                ++buff;

            //leggo il campo destro
            rc = read_field(dx,&buff);

            //errore lettura campo
            PARSER_ERR_HANDLER(rc,-1,-1);

            //a questo punto analizzo i due campi
            rc = analyze_field(sx,dx,config);

            //errore nell'analisi dei campi
            PARSER_ERR_HANDLER(rc,-1,-1);

        }
     }

     //chiudo file
     fclose(config_file);

     return 0;
}
