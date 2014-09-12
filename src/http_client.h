/** @file http_client.h
 *  @brief Define client struct
 *
 *  @author Chao Xin(cxin)
 */
#ifndef __HTTP_CLIENT__
#define __HTTP_CLIENT__

#include "io.h"

 /** @brief Store information of a single client.
 *
 * Clients are organized using linked list.
 */

typedef struct client_list {
    int fd;         //<!client's file descriptor
    buf_t *in, *out;     //<!input and output buffer assigned to this client
    struct client_list *next;   //<!next client in the linked list
} client_list_t;

client_list_t *client_head;     //first client in the linked list

void deinit_client(client_list_t *client);
client_list_t* new_client(int fd);

void write_client(client_list_t *client, char* buf, int buf_len);

#endif