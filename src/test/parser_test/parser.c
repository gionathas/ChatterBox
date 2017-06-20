#include<stdio.h>
#include"chatty_task.h"
#include"config.h"

int main(int argc,char **argv)
{
    server_config_t conf;
    int rc;

    rc = chatty_parser(argv[1],&conf);

    if(rc == -1)
    {
        perror("parser");
        return -1;
    }

    //stampo config
    print_config(conf);

    return 0;
}
