#ifndef __SERVER_H__
#define __SERVER_H__

#include "config.h"
#include "io.h"

#define DEFAULT_BACKLOG 1024    //The second argument passed into listen()

typedef struct client_list {
    int fd;
    buf_t *buf;
    struct client_list *next;
} client_list_t;

client_list_t *client_head;

void serve(unsigned short port);

#endif