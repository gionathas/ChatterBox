#include"server.h"
#include<stdio.h>
#include<ctype.h>
#include<unistd.h>
#include<string.h>

#define STRING_LEN 256
#define DEBUG

static void gestioneClient(void *buff,int fd,void *arg)
{
    char string[STRING_LEN];
    int i = 0;
    int fd_client = fd;
    strncpy(string,buff,STRING_LEN);

    string[strlen(string)] = '\0';

    //toupper
    while(string[i] != '\0')
    {
        string[i] = toupper(string[i]);
        i++;
    }

    write(fd_client,string,STRING_LEN * sizeof(char));
}

static void gestioneTroppiClient(int fd,void *arg)
{
    char err_mess[] = "Server pieno";

    write(fd,err_mess,STRING_LEN * sizeof(char));
}

int main()
{
    server_t *server = init_server("/tmp/server",2);

    if(server == NULL)
    {
        perror("init server:");
        return -1;
    }

    client_manager_fun_t cl_manager;
    cl_manager.function = gestioneClient;
    cl_manager.arg = NULL;

    too_much_client_handler_t cl_outbound;
    cl_outbound.function = gestioneTroppiClient;
    cl_outbound.arg = NULL;

    start_server(server,STRING_LEN * sizeof(char),3,cl_manager,cl_outbound);

    //se ritorna c'e' un errore
    perror("server:");
    return -1;

}
