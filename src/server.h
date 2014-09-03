#ifndef __SERVER_H__
#define __SERVER_H__

#include "config.h"
#include "io.h"

#define DEFAULT_BACKLOG 1024    //The second argument passed into listen()

typedef struct client_list {
    int fd;         //client's file descriptor
    buf_t *buf;     //buffer assigned to this client
    struct client_list *next;   //next client in the linked list
} client_list_t;

client_list_t *client_head;     //first client in the linked list

void serve(unsigned short port);

#endif