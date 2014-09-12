/** @file server.h
 *  @brief Header file for server.c
 *
 *  @author Chao Xin(cxin)
 */
#ifndef __SERVER_H__
#define __SERVER_H__

#include "config.h"
#include "io.h"

#define DEFAULT_BACKLOG 1024    //The second argument passed into listen()

void serve(unsigned short port);

#endif