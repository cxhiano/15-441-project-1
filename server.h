#ifndef __SERVER_H__
#define __SERVER_H__

#include "config.h"

#define DEFAULT_BACKLOG 1024
#define MAXBUFSIZE 4096

void serve(unsigned short port);

#endif