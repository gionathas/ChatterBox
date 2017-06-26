#include<stdlib.h>
#include"ops.h"

int send_ok_message(int fd,char *buf,unsigned int len);
int send_fail_message(int fd,op_t op_fail);
int inviaMessaggioUtente(char *sender,char *receiver,char *msg,size_t size_msg,utenti_registrati_t *utenti);
