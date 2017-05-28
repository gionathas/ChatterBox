#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<stdio.h>
#include<sys/un.h>
#include<string.h>
#include<ctype.h>
#include"macro_error.h"

#define UNIX_PATH_MAX 108
#define SOCKNAME "/tmp/server"
#define SIZE_STRING 256
#define DEBUG

int main(int argc, char const *argv[])
{
    int fd_client;
    struct sockaddr_un sa;
    char string[SIZE_STRING];
    char res[SIZE_STRING];

    //init address
    strncpy(sa.sun_path,SOCKNAME,UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    err_meno1_ex(fd_client=socket(AF_UNIX,SOCK_STREAM,0),"on create socket");

    #ifdef DEBUG
        printf("%d\n",fd_client);
    #endif

    printf("Connecting server..\n");
    while(connect(fd_client,(struct sockaddr *)&sa,sizeof(sa)) == -1)
    {
        //server offline
        if(errno == ENOENT)
            sleep(1);
        //problema di connessine
        else
        {
            perror("on connect");
            exit(EXIT_FAILURE);
        }
    }

    printf("Client: Connessione al server stabilita..\n" );

    if(strcmp(res,"Server pieno") == 0)
    {
        close(fd_client);
        return 0;
    }

    while (1)
    {
        printf("Inserisci la stringa (quit per uscire):\n");
        fflush(stdin);
        err_null_ex(fgets(string,SIZE_STRING,stdin),"invalid input string");
        //comunicazione con server:

        //rimuovo newline dalla stringa
        string[strcspn(string, "\n")] = 0;

        //se voglio quittare
        if(strcmp(string,"quit") == 0)
            break;

        //invio stringa
        err_meno1_ex(write(fd_client,string,SIZE_STRING * sizeof(char)),"on write");

        //leggo risposta
        err_meno1_ex(read(fd_client,res,SIZE_STRING * sizeof(char)),"on read");

        //stampo risposta
        printf("%s\n",res);
        //azzero
        memset(res,0,sizeof(res));
    }

    printf("Disconnecting..\n");

    close(fd_client);

    return 0;
}
