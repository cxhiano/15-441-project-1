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

/**
 * In the serving loop, everytime before calling select(), this variable will
 * be checked to determine whether the serving loop should continue or not.
 * This variable is initially 0. By setting it to a non-zero value, the server
 * will be terminated.
 */
int terminate;

void serve(unsigned short port);

void finalize();

#endif