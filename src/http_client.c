#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "config.h"
#include "log.h"
#include "http_client.h"

/** brief Compare two string(case insensitive) */
int strcicmp(char* s1, char* s2) {
    int len = strlen(s1),
        i;
    char c1, c2;

    if (len != strlen(s2)) return 1;

    for (i = 0; i < len; ++i) {
        c1 = s1[i]; c2 = s2[i];
        if (c1 >= 'A' && c1 <= 'Z')
            c1 = c1 - 'A' + 'a';
        if (c2 >= 'A' && c2 <= 'Z')
            c2 = c2 - 'A' + 'a';
        if (c1 != c2) return 1;
    }

    return 0;
}

/** @brief Destroy a http_header struct */
void deinit_header(http_header_t *header) {
    if (header->key != NULL)
        free(header->key);
    if (header->val != NULL)
        free(header->val);
}

/** @brief Destroy a http_request struct */
void deinit_request(http_request_t *req) {
    http_header_t *ptr, *tmp;

    if (req == NULL) return;

    ptr = req->headers;

    while (ptr != NULL) {
        tmp = ptr->next;
        deinit_header(ptr);
        ptr = tmp;
    }
}

/** @brief Create a new http_request */
http_request_t* new_request() {
    http_request_t *req;

    req = malloc(sizeof(http_request_t));
    req->headers = NULL;

    return req;
}

/** @brief Create a new http client associated with socket fd */
http_client_t* new_client(int fd) {
    http_client_t *client = malloc(sizeof(http_client_t));

    client->fd = fd;

    client->in = io_init();
    client->out = io_init();

    client->req = NULL;

    client->next = NULL;

    return client;
}

/** @brief Destroy a client struct, free all its resource */
void deinit_client(http_client_t *client) {
    close(client->fd);
    log_msg(L_INFO, "Closed fd %d\n", client->fd);
    io_deinit(client->in);
    free(client->in);
    io_deinit(client->out);
    free(client->out);
    deinit_request(client->req);
    free(client->req);
    free(client);
}

/** @brief Write a buffer to client
 *
 *  Copy buf_len bytes from buf to client's output buffer
 *
 *  @param client A pointer to a client struct
 *  @param buf The buffer to be written to client
 *  @param buf_len The length of the buffer
 *  @return Void
 */
void client_write(http_client_t *client, char* buf, int buf_len) {
    buf_t *c_buf = client->out;     //Internal output buffer of client

    //The space is not enough, realloc !
    if (c_buf->datasize + buf_len > c_buf->bufsize) {
        /*
         * An additional INIt_BUFFERSIZE is added to the bufsize to prevent
         * frequent realloc
         */
        c_buf->bufsize = c_buf->datasize + buf_len + INIT_BUFFERSIZE;
        c_buf->buf = realloc(c_buf->buf, c_buf->bufsize);
    }

    memmove(c_buf->buf + c_buf->datasize, buf, buf_len);
    c_buf->datasize += buf_len;
}

/** @brief Write a string to client */
void client_write_string(http_client_t *client, char* str) {
    client_write(client, str, strlen(str));
}

/** @brief Read a line ends in \n from client's input buffer
 *
 *  Find \n started from the internal pointer pos of the client's input buffer
 *  and copy the content between pos pointer and \n(include) to the passed in
 *  buffer. Also update the pointer pos to the character next to '\n'.
 *
 *  @param client A pointer to a client struct
 *  @param line The buffer which stores the line.
 *  @return 1 on success. If no \n is found, return 0. If the length of line
 *  exceed MAX_LINE_LEN, return -1.
 */
int client_readline(http_client_t *client, char *line) {
    buf_t *bp = client->in;
    int end = bp->pos,
        n;

    while (end < bp->datasize) {
        if (end - bp->pos >= MAXBUF)
            return -1;
        if (bp->buf[end] == '\n') {
            n = end - bp->pos;

            /* Deal with \r\n */
            if (n > 0 && bp->buf[end - 1] == '\r')
                n -= 1;

            strncpy(line, bp->buf + bp->pos, n);

            line[n] = '\0';     //End the string

            bp->pos = end + 1;
            return 1;
        }
        end += 1;
    }

    return 0;
}

/** @brief Send the response line to client with status code
 *
 *  @param client The corresponding client
 *  @param code Status code
 */
void send_response_line(http_client_t *client, int code) {
    char* line;

    client_write_string(client, http_version);

    if (code == OK)
        line = " 200 OK";
    if (code == BAD_REQUEST)
        line = " 400 Bad Request";
    if (code == NOT_FOUND)
        line = " 404 Not Found";
    if (code == METHOD_NOT_ALLOWED)
        line = " 405 Method Not Allowed";
    if (code == LENGTH_REQUIRED)
        line = " 411 Length Required";
    if (code == INTERNAL_SERVER_ERROR)
        line = " 500 Internal Server Error";
    if (code == NOT_IMPLEMENTED)
        line = " 501 Not Implemeneted";
    if (code == SERVICE_UNAVAILABLE)
        line = " 503 Service Unavailable";
    if (code == HTTP_VERSION_NOT_SUPPORTED)
        line = " 505 HTTP Version Not Supported";

    client_write_string(client, line);
    client_write_string(client, "\r\n");
}


/**********************************Section Handler****************************/
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

static void send_header(http_client_t *client, char* key, char* val) {
    client_write_string(client, key);
    client_write_string(client, val);
    client_write_string(client, "\r\n");
}

/** @brief Handle HTTP GET request
 *
 *  @param client A pointer to corresponding client object
 *  @return
 */
int handle_get(http_client_t *client) {
    struct stat s;
    char *path, *mimetype;
    char buf[MAXBUF];
    char last_modifiled[128], date[128];
    int fd, n, size;
    time_t current_time;

    path = malloc(strlen(www_folder) + strlen(client->req->uri) + 20);
    if (realpath(www_folder, path) == NULL) {
        free(path);
        log_error("handle_get error: realpath error");
        return INTERNAL_SERVER_ERROR;
    }
    strcat(path, client->req->uri);

    /* Check if the file exists */
    if (stat(path, &s) == -1) {
        free(path);
        log_error("handle_get error: stat error");
        return NOT_FOUND;
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
                free(path);
                log_error("handle_get error: stat error");
                return NOT_FOUND;
            }
        }
    }

    if ((fd = open(path, O_RDONLY)) == -1) {
        free(path);
        log_error("handle_get error: open error");
        return INTERNAL_SERVER_ERROR;
    }

    /* get the size of file by lseek */
    if ((size = lseek(fd, 0, SEEK_END)) == -1) {
        free(path);
        close(fd);
        log_error("handle_get error: lseek error");
        return INTERNAL_SERVER_ERROR;
    }
    /* restore offset */
    if (lseek(fd, 0, SEEK_SET) == -1) {
        free(path);
        close(fd);
        log_error("handle_get error: lseek error");
        return INTERNAL_SERVER_ERROR;
    }

    mimetype = get_mimetype(path);
    free(path); // path is no longer useful
    strftime(last_modifiled, 128, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&(s.st_mtime)));
    current_time = time(NULL);
    strftime(date, 128, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&current_time));

    sprintf(buf, "%d", size);

    send_response_line(client, OK);
    send_header(client, "Content-Type: ", mimetype);
    send_header(client, "Content-Length: ", buf);
    send_header(client, "Date: ", date);
    send_header(client, "Last-Modified: ", last_modifiled);
    send_header(client, "Server: ", "Liso/1.0");
    client_write_string(client, "\r\n");

    while ((n = read(fd, buf, MAXBUF)) > 0)
        client_write(client, buf, n);

    return 0;
}

/** @brief Handle HTTP POST request
 *
 *  @param client A pointer to corresponding client object
 *  @return
 */
int handle_post(http_client_t *client) {
    return -1;
}

/** @brief Handle HTTP HEAD request
 *
 *  @param client A pointer to corresponding client object
 *  @return
 */
int handle_head(http_client_t *client) {
    return -1;
}