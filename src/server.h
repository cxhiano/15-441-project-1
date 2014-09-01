#ifndef __SERVER_H__
#define __SERVER_H__

#include "config.h"

#define DEFAULT_BACKLOG 1024    //The second argument passed into listen()

void serve(unsigned short port);

#endif