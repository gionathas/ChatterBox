#include"threadpool.h"
#include<stdio.h>
#include<unistd.h>

static int dummy_fun(void* arg)
{
    printf("ciao\n" );
    errno = EINVAL;
    return 0;
}

int main()
{
    threadpool_t *tp;

    tp = threadpool_create(2);

    if(tp == NULL)
    {
        perror("on create");
        return -1;
    }

    int rc = threadpool_add_task(tp,dummy_fun,NULL);

    if(rc == -1)
    {
        perror("on add task");
        return -1;
    }

    rc = threadpool_destroy(&tp);

    if(rc == -1)
    {
        perror("on destroy");
        return -1;
    }

    return 0 ;
}
