#include"server.h"
#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<unistd.h>
#include<string.h>

#define STRING_LEN 256
#define DEBUG

int real_read_message(int fd,char* mess);

static int gestioneClient(void *buff,int fd,void *arg)
{
    char string[STRING_LEN];
    int i = 0;
    int fd_client = fd;

    strncpy(string,buff,STRING_LEN);

//inserisco il terminatore di stringa
    string[strlen(string)] = '\0';


    //toupper
    while(string[i] != '\0')
    {
        string[i] = toupper(string[i]);
        i++;
    }

    write(fd,string,STRING_LEN * sizeof(char));
    errno = EINVAL;

    return -1;
}

static int readMessage(int fd,void *message)
{
    return real_read_message(fd,(char*)message);
}

int real_read_message(int fd,char* mess)
{
    int nread = read(fd,mess,STRING_LEN * sizeof(char));

    if(nread == 0)
        return 0;
    else if(nread == -1)
        return -1;
    else
        return nread;
}

static int gestioneTroppiClient(int fd,void *arg)
{
    char err_mess[] = "Server pieno";

    write(fd,err_mess,STRING_LEN * sizeof(char));
    return 0;
}

static int sigusr1(void *arg)
{
    errno = EINPROGRESS;
    return -1;
}

int main()
{
    server_t *server = init_server("/tmp/server",STRING_LEN * sizeof(char),3);

    if(server == NULL)
    {
        perror("on init server");
        return -1;
    }

    //creo funzioni per il server
    server_function_t funs;

    //funzione gestione client e argomenti
    funs.client_manager_fun = gestioneClient;
    funs.arg_cmf = NULL;

    funs.read_message_fun = readMessage;

    funs.client_out_of_bound = gestioneTroppiClient;
    funs.arg_cob = NULL;

    funs.signal_usr_handler = sigusr1;
    funs.arg_suh = NULL;

    int rc = start_server(server,2,funs);

    //gestisco tutti i casi di errore
    if(rc == -1 || rc > 0)
    {
        //gestione errore non riguardante il fallimento dei thread
        if(rc == -1)
        {
            perror("server fail");
        }
        else{
            errno = rc;
            perror("thread failed");
        }
    }

    //tutto andato a buon fine
    exit(EXIT_SUCCESS);
}
