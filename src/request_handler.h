/** @file request_handler.h
 *  @brief Define HTTP Request handlers
 *
 *  @author Chao Xin(cxin)
 */
#ifndef __REQUEST_HANDLER__
#define __REQUEST_HANDLER__

#include "http_client.h"

/* Request handlers */
int handle_get(http_client_t *client);
int handle_post(http_client_t *client);
int handle_head(http_client_t *client);

/* helper functions */
int strcicmp(char* s1, char* s2);

#endif