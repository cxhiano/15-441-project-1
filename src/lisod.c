/*
 * lisod.c - A web server call Liso
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "config.h"
#include "server.h"

void usage() {
	fprintf(stderr, "Usage: ./lisod <HTTP port> <HTTPS port> <log file> <lock file> <www folder>");
	fprintf(stderr, "<CGI script path> <private key file> <certificate file>\n");
	fprintf(stderr, "	HTTP port – the port for the HTTP (or echo) server to listen on\n");
	fprintf(stderr, "	HTTPS port – the port for the HTTPS server to listen on\n");
	fprintf(stderr, "	log file – file to send log messages to (debug, info, error)\n");
	fprintf(stderr, "	lock file – file to lock on when becoming a daemon process\n");
	fprintf(stderr, "	www folder – folder containing a tree to serve as the root of a website\n");
	fprintf(stderr, "	CGI script path – this is a file that should be a script where you ");
	fprintf(stderr, "redirect all /cgi/* URIs. In the real world, this would likely be a directory of executable programs.\n");
	fprintf(stderr, "	private key file – private key file path\n");
	fprintf(stderr, "	certificate file – certificate file path\n");
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		usage();
		return -1;
	}

	http_port = atoi(argv[1]);

	serve(http_port);

	return 0;
}