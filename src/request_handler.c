/** @file request_handler.c
 *  @brief HTTP Request handlers
 *
 *  @author Chao Xin(cxin)
 */
#include <limits.h>
#include <stdarg.h>
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

/** @brief */
static char* create_string(char* format, ...) {
    char buf[MAXBUF];
    char *str;

    va_list arguments;
    va_start(arguments, format);
    vsprintf(buf, format, arguments);
    va_end(arguments);

    str = malloc(sizeof(char) * (strlen(buf) + 1));
    strcpy(str, buf);
    str[strlen(buf) + 1] = '\0';
    return str;
}

/** @brief Translate a http request header to a cgi environment variable
 *
 *  '-' to '_'. Lowcase to uppercase
 */
static void translate_header(char* http_header, char* cgi_var) {
    int i;
    char c;

    for (i = 0; i < strlen(http_header); ++i) {
        c = http_header[i];
        cgi_var[i] = c;
        if (c == '-') cgi_var[i] = '_';
        if (c >= 'a' && c <= 'z')
            cgi_var[i] = c - 'a' + 'A';
    }
    cgi_var[strlen(http_header)] = '\0';
}

/** @brief Setup and return environment variables for cgi script */
static char** setup_envp(http_client_t* client) {
    int RFC_VARS = 17;
    char** envp;
    char* tmp;
    char buf[MAXBUF];
    http_header_t *h;
    int i;
    http_request_t *req = client->req;

    envp = malloc(sizeof(char*) * (RFC_VARS + req->cnt_headers + 1));
    /* AUTH_TYPE */
    envp[0] = create_string("AUTH_TYPE=");
    /* CONTENT_LENGTH */
    if (req->method == M_POST)
        envp[1] = create_string("CONTENT_LENGTH=%d", req->content_length);
    else
        envp[1] = create_string("CONTENT_LENGTH=");
    /* CONTENT_TYPE */
    tmp = get_request_header(req, "content-type");
    if (tmp == NULL)
        envp[2] = create_string("CONTENT_TYPE=");
    else
        envp[2] = create_string("CONTENT_TYPE=%s", tmp);
    /* GATEWAY_INTERFACE */
    envp[3] = create_string("GATEWAY_INTERFACE=CGI/1.1");
    /* PATH_INFO */
    envp[4] = create_string("PATH_INFO=");
    /* PATH_TRANSLATED */
    envp[5] = create_string("PATH_TRANSLATED=");
    /* QUERY_STRING */
    envp[6] = create_string("QUERY_STRING=%s", req->query);
    /* REMOTE_ADDR */
    envp[7] = create_string("REMOTE_ADDR=%s", client->remote_ip);
    /* REMOTE_HOST */
    if (client->remote_host == NULL)
        envp[8] = create_string("REMOTE_HOST=");
    else
        envp[8] = create_string("REMOTE_HOST=%s", client->remote_host);
    /* REMOTE_IDENT */
    envp[9] = create_string("REMOTE_IDENT=");
    /* REMOTE_USER */
    envp[10] = create_string("REMOTE_USER=");
    /* REQUEST_METHOD */
    tmp = NULL;
    if (req->method == M_HEAD) tmp = "HEAD";
    if (req->method == M_GET) tmp = "GET";
    if (req->method == M_POST) tmp = "POST";
    if (tmp == NULL)  {
        log_msg(L_ERROR, "Unexpected request method: %d\n", req->method);
        tmp = "";
    }
    envp[11] = create_string("REQUEST_METHOD=%s", tmp);
    /* SCRIPT_NAME */
    envp[12] = create_string("SCRIPT_NAME=%s", req->path);
    /* SERVER_NAME */
    envp[13] = create_string("SERVER_NAME=Liso/1.0");
    /* SERVER_PORT */
    envp[14] = create_string("SERVER_PORT=%d", http_port);
    /* SERVER_PROTOCOL */
    envp[15] = create_string("SERVER_PROTOCOL=%s", "HTTP/1.1");
    /* SERVER_SOFTWARE */
    envp[16] = create_string("SERVER_SOFTWARE=Liso/1.0");
    /* HTTP request headers */
    i = RFC_VARS; // Index in envp
    for (h = req->headers; h != NULL; h = h->next) {
        translate_header(h->key, buf);
        envp[i++] = create_string("HTTP_%s=%s", buf, h->val);
    }
    // Terminate the array by NULL
    envp[i] = NULL;

    return envp;
}

/** @brief Handle a CGI request
 *
 *  Use fork() to create a new process to run cgi script. Use pipe to feed
 *  request body to stdin of the cgi script. Setup pipe for stdout of the
 *  cgi script.
 *
 *  @return 0 if ok. HTTP status code if something goes wrong
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
        log_msg(L_INFO, "Start child process %d\n", pid);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        /* Write request body to subprocess */
        if (client->req->method == M_POST) {
            bytes_left = client->req->content_length;
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
    } else if (client->req->method != M_POST) {
        return server_static_file(client);
    }

    // POST to a static file is not allowed
    return SERVICE_UNAVAILABLE;
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
