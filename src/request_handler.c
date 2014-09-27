/** @file request_handler.c
 *  @brief HTTP Request handlers
 *
 *  @author Chao Xin(cxin)
 */
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "log.h"
#include "config.h"
#include "request_handler.h"
#include "http_client.h"
#include "io.h"

static char* get_mimetype(char* path) {
    char* ext = path + strlen(path) - 1;

    while (ext != path && ext[0] != '.' && ext[0] != '/')
        ext -= 1;
    if (ext[0] == '.') {
        ext += 1;
        if (strcicmp(ext, "html") == 0)
            return "text/html";
        if (strcicmp(ext, "css") == 0)
            return "text/css";
        if (strcicmp(ext, "png") == 0)
            return "image/png";
        if (strcicmp(ext, "jpg") == 0)
            return "image/jpg";
        if (strcicmp(ext, "gif") == 0)
            return "image/gif";
    }

    return "application/octet-stream";
}

/** @brief Try to open file and retrieve its information
 *
 *  The passed-in uri will be concatenated with www_folder to make the full
 *  path. The size, type and last modified date will be retrieve.
 *
 *  @param file_path URI from HTTP request
 *  @param size The pointer to the variable which stores the file size
 *  @param mimetype The pointer to a string buffer which will be used to store
 *                  the mimetype
 *  @param last_modified The pointer to a string buffer that stores the last
 *                       modified date.
 *  @return File descriptor of the openned file if success. Negate of the
 *          corresponding http response code if error occurs.
 */
static int open_file(char *file_path, int *size, char *mimetype, char *last_modifiled) {
    struct stat s;
    char path[2 * PATH_MAX];
    int fd;

    /* Get the absolute path of www_folder */
    if (realpath(www_folder, path) == NULL) {
        log_error("handle_get error: realpath error");
        return -INTERNAL_SERVER_ERROR;
    }
    strcat(path, file_path);

    /* Check if the file exists */
    if (stat(path, &s) == -1) {
        log_error("handle_get error: stat error");
        return -NOT_FOUND;
    }
    else {
        //is a directory?
        if (S_ISDIR(s.st_mode)) {
            //try to get index.html
            if (path[strlen(path) - 1] == '/')
                strcat(path, "index.html");
            else
                strcat(path, "/index.html");
            if (stat(path, &s) == -1) {
                log_error("handle_get error: stat error");
                return -NOT_FOUND;
            }
        }
    }

    if ((fd = open(path, O_RDONLY)) == -1) {
        log_error("handle_get error: open error");
        return -INTERNAL_SERVER_ERROR;
    }

    /* get the size of file by lseek to the end of the file */
    if ((*size = lseek(fd, 0, SEEK_END)) == -1) {
        close(fd);
        log_error("handle_get error: lseek error");
        return -INTERNAL_SERVER_ERROR;
    }
    /* restore offset */
    if (lseek(fd, 0, SEEK_SET) == -1) {
        close(fd);
        log_error("handle_get error: lseek error");
        return -INTERNAL_SERVER_ERROR;
    }

    strcpy(mimetype, get_mimetype(path));

    strftime(last_modifiled, 128, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&(s.st_mtime)));

    return fd;
}

/** @brief Handler for serving static file
 *
 *  Send required response line and response headers to client and if the method
 *  is GET, a pipe between the open file and client socket will be setup
 *
 *  @param client A pointer to corresponding client object
 *  @return 0 if OK. Return response status code on error
 */
static int server_static_file(http_client_t *client) {
    char buf[MAXBUF];
    char last_modifiled[128], date[128], mimetype[128];
    int size, fd;
    time_t current_time;

    if ((fd = open_file(client->req->path, &size, mimetype, last_modifiled)) < 0)
        return -fd;

    current_time = time(NULL);
    strftime(date, 128, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&current_time));

    sprintf(buf, "%d", size);

    send_response_line(client, OK);
    send_header(client, "Content-Type", mimetype);
    send_header(client, "Content-Length", buf);
    send_header(client, "Date", date);
    send_header(client, "Last-Modified", last_modifiled);
    send_header(client, "Server", "Liso/1.0");
    if (connection_close(client->req))
        send_header(client, "Connection", "close");
    else
        send_header(client, "Connection", "keep-alive");
    client_write_string(client, "\r\n");

    /**
     * A GET request should send the file content back to the client. Here, we
     * just pipe the file directly to the client socket. See io_pipe() in io.c
     * for more information
     */
    if (client->req->method == M_GET) {
        client->pipe = init_pipe();
        client->pipe->from_fd = fd;
        add_read_fd(fd);
    }
    else
        close(fd);

    return 0;
}

/** @brief
 *
 */
static char** setup_envp(http_client_t *client) {
    char** envp;

    envp = NULL;

    return envp;
}

/** @brief
 *
 *  @return
 */
static int cgi_handler(http_client_t *client) {
    pid_t pid;
    char path[PATH_MAX * 2];
    int stdin_pipe[2], stdout_pipe[2];
    int bytes_left, n;
    char **envp;
    char* argv[] = { NULL, NULL };

    /* Get the absolute path of cgi_path */
    if (realpath(cgi_path, path) == NULL) {
        log_error("cgi_handler error: realpath error");
        return -INTERNAL_SERVER_ERROR;
    }
    strcat(path, client->req->path);
    /* Check whether the script exists. Check permission */
    if (access(path, X_OK) == -1) {
        log_error("cgi_handler error");
        if (errno == ENOENT)
            return NOT_FOUND;
        else
            return INTERNAL_SERVER_ERROR;
    }

    argv[0] = path;
    /* Setup pipe */
    /* 0 can be read from, 1 can be written to */
    if (pipe(stdin_pipe) < 0) {
        log_error("launch_cgi setup stdin_pipe error");
        return INTERNAL_SERVER_ERROR;
    }
    if (pipe(stdout_pipe) < 0) {
        log_error("launch_cgi setup stdin_pipe error");
        return INTERNAL_SERVER_ERROR;
    }

    /* Create subprocess */
    if ((pid = fork()) < 0) {
        log_error("launch_cgi fork() error");
        return INTERNAL_SERVER_ERROR;
    }

    /* Subprocess invokes cgi script */
    if (pid == 0) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);

        if (dup2(stdin_pipe[0], fileno(stdin)) == -1) {
            log_error("dup2 stdin_pipe error");
            exit(EXIT_FAILURE);
        }
        if (dup2(stdout_pipe[1], fileno(stdout)) == -1) {
            log_error("dup2 stdout_pipe error");
            exit(EXIT_FAILURE);
        }

        envp = setup_envp(client);
        if (execve(argv[0], argv, envp)) {
            log_error("exceve error");
            exit(EXIT_FAILURE);
        }
    }

    /* Main routine continues */
    if (pid > 0) {
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        /* Write request body to subprocess */
        if (client->req->method == M_POST) {
            bytes_left = client->req->content_len;
            /* We don't want to be block when passing data to subprocess */
            fcntl(stdin_pipe[1], F_SETFL, O_NONBLOCK);
            while ((n = write(stdin_pipe[1], client->req->body,
                    bytes_left)) > 0)
                bytes_left -= n;

            // Error writing bytes to subprocess
            if (bytes_left > 0 || n == -1) {
                log_msg(L_ERROR, "fail to send request body to subprocess\n");
                kill(pid, SIGKILL);
                close(stdin_pipe[1]);
                return INTERNAL_SERVER_ERROR;
            }
        }
        close(stdin_pipe[1]);

        /* setup pipe from subprocess output */
        client->pipe = init_pipe();
        client->pipe->from_fd = stdout_pipe[0];
        add_read_fd(stdout_pipe[0]);

         return 0;
    }


    log_msg(L_ERROR, "Control should never reach here!!!");
    return -1;
}

/** @brief Internal handler
 *
 *  This process will be called by both handle_get and handle_head. It only
 *  send response line and response header to the client. And if the request
 *  method is GET, a pipe between the file and client socket will be setup.
 *
 *  @param client A pointer to corresponding client object
 *  @return 0 if OK. Response status code on error
 */
static int internal_handler(http_client_t *client) {
    if (client->req->is_cgi) {
        return cgi_handler(client);
    }
    return server_static_file(client);
}

/** @brief Handle HTTP GET request
 *
 *  @param client A pointer to corresponding client object
 *  @return 0 if OK. Response status code if error.
 */
int handle_get(http_client_t *client) {
    int ret = internal_handler(client);

    log_msg(L_INFO, "Handle GET request. PATH: %s\n", client->req->path);

    /*
     * If internal_handler processes without error, the content of the static
     * file will be pipe to client.
     */
    if (ret == 0)
        client->status = C_PIPING;
    else
        client->status = C_IDLE;
    return ret;
}

/** @brief Handle HTTP HEAD request
 *
 *  @param client A pointer to corresponding client object
 *  @return 0 if OK. Response status code if error.
 */
int handle_head(http_client_t *client) {
    int ret = internal_handler(client);

    log_msg(L_INFO, "Handle HEAD request. PATH: %s\n", client->req->path);
    client->status = C_IDLE;
    return ret;
}

/** @brief Handle HTTP POST request
 *
 *  @param client A pointer to corresponding client object
 *  @return 0 if OK. Response status code if error.
 */
int handle_post(http_client_t *client) {
    int ret = internal_handler(client);
    log_msg(L_INFO, "Handle POST request. PATH: %s\n", client->req->path);

    /*
     * If successfully running cgi script, the output will be piped to client.
     */
    if (ret == 0)
        client->status = C_PIPING;
    else
        client->status = C_IDLE;

    return ret;
}
