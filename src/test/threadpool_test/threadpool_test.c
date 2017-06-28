#define _POSIX_C_SOURCE 200112L
#include"threadpool.h"
#include<stdio.h>
#include<unistd.h>
#include<signal.h>
#include<string.h>

static int res;
threadpool_t *tp;


static void signal_hdr(int sig)
{
    res = threadpool_destroy(&tp);
}

static int dummy_fun(void* arg)
{
    errno = EINVAL;
    return 0;
}

int main()
{
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));

    sa.sa_handler = signal_hdr;
    sigaction(SIGTERM,&sa,NULL);

    tp = threadpool_create(2);

    if(tp == NULL)
    {
        perror("on create");
        return -1;
    }

    int rc = threadpool_add_task(tp,dummy_fun,NULL);

    sleep(2);

    if(rc == -1)
    {
        perror("on add task");
        return -1;
    }

    rc = threadpool_destroy(&tp);

    return res;
}
