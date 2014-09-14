/** @file request_handler.h
 *  @brief Define HTTP Request handlers
 *
 *  @author Chao Xin(cxin)
 */
#ifndef __REQUEST_HANDLER__
#define __REQUEST_HANDLER__

#include "http_client.h"

/*
 * One of these flags will be passed into internal handler which handles GET
 * and HEAD request together and help the internal handler to decide whether
 * the meesgae body should be send to the client.
 */
#define F_GET 0
#define F_HEAD 1

/* Request handlers */
int handle_get(http_client_t *client);
int handle_post(http_client_t *client);
int handle_head(http_client_t *client);

/* helper functions */
int strcicmp(char* s1, char* s2);

#endif