/** @file http_parser.h
 *  @brief Provide function to parse http request
 *
 *  @author Chao Xin(cxin)
 */
#ifndef __HTTP_PARSER__
#define __HTTP_PARSER__

#include "http_client.h"

void http_parse(client_list_t *client);

#endif
