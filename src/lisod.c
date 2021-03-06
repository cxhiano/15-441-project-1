/** @brief A web server call Liso
 *
 *  @author Chao Xin(cxin)
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include "config.h"
#include "server.h"
#include "log.h"

char* http_version = "HTTP/1.1";

/**
 * SIGHUP indicates that the config file should be reloaded.
 */
static void sighup_handler(int sig) {
}

/**
 * SIGTERM informs the server to terminate. Finalization work here
 */
static void sigterm_handler(int sig) {
	terminate = 1;
	finalize();
	log_msg(L_INFO, "Server terminated. Bye~");
	exit(EXIT_SUCCESS);
}

/**
 * Reap terminated child process
 */
static void sigchld_handler(int sig) {
	pid_t pid;
	int status;

	while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0)
		log_msg(L_INFO, "Reap child process %d\n", pid);
}

static void usage() {
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

/** @brief Set up log system */
static void config_log() {
	log_mask = L_ERROR | L_HTTP_DEBUG | L_INFO;
	set_log_file(log_file_name);
}

/** @brief daemonize the server */
static void daemonize(char* lock_file) {
	int pid, lfp, i;

	pid = fork();
	if (pid < 0) exit(EXIT_FAILURE); // fork error
	if (pid > 0) exit(EXIT_SUCCESS); // parent exit;
	// Daemon continues

	signal(SIGHUP, SIG_IGN); // Ignore SIGHUP for now. Prevent terminating

	if (setsid() == -1) { // get a new sid
		perror("setsid error");
		exit(EXIT_FAILURE);
	}

	umask(027); // Deny others from writing the file created by this daemon

	lfp = open(lock_file, O_RDWR|O_CREAT, 0640); // open lock file
	if (lfp < 0) {
		perror("open lock_file error");
		exit(EXIT_FAILURE);
	}

	if (lockf(lfp, F_TLOCK, 0) < 0) {
		perror("lockf error");
		exit(EXIT_FAILURE);
	}

	// Only first instance continues
	dprintf(lfp, "%d\n", getpid());

	// Close file descriptor
	for (i = getdtablesize(); i >= 0; --i)
		if (i != lfp) close(i);

	//redirect stdin stdout stderr to /dev/null
	open("/dev/null", O_RDWR);	// Allocate fd 0(stdin)
	open("/dev/null", O_RDWR);  // 1(stdout)
	open("/dev/null", O_RDWR);  // 2(stderr)

    // Server must run in known directory
    /*if ((chdir("/")) < 0) {
    	log_error("chdir error");
    	exit(EXIT_FAILURE);
    }*/

    // Signal handling
    signal(SIGHUP, sighup_handler);
   	signal(SIGTERM, sigterm_handler);
   	signal(SIGCHLD, sigchld_handler);
	signal(SIGPIPE, SIG_IGN);

	config_log();

    log_msg(L_INFO, "Successfully daemonized lisod process, pid %d.\n",
    	getpid());
}

int main(int argc, char* argv[])
{
	if (argc < 9) {
		usage();
		return -1;
	}

	http_port = atoi(argv[1]);
	https_port = atoi(argv[2]);
	log_file_name = argv[3];
	lock_file = argv[4];
	www_folder = argv[5];
	cgi_path = argv[6];
	private_key_file = argv[7];
	certificate_file = argv[8];

	daemonize(lock_file);

	serve();

	return 0;
}
