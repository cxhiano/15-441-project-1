/** @file request_handler.c
 *  @brief HTTP Request handlers
 *
 *  @author Chao Xin(cxin)
 */
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "log.h"
#include "config.h"
#include "request_handler.h"
#include "http_client.h"

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
 *  @param uri URI from HTTP request
 *  @param size The pointer to the variable which stores the file size
 *  @param mimetype The pointer to a string buffer which will be used to store
 *                  the mimetype
 *  @param last_modified The pointer to a string buffer that stores the last
 *                       modified date.
 *  @return File descriptor of the openned file if success. Negate of the
 *          corresponding http response code if error occurs.
 */
static int open_file(char *uri, int *size, char *mimetype, char *last_modifiled) {
    struct stat s;
    char *path;
    int fd;

    path = malloc(strlen(www_folder) + strlen(uri) + 20);
    if (realpath(www_folder, path) == NULL) {
        free(path);
        log_error("handle_get error: realpath error");
        return -INTERNAL_SERVER_ERROR;
    }
    strcat(path, uri);

    /* Check if the file exists */
    if (stat(path, &s) == -1) {
        free(path);
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
                free(path);
                log_error("handle_get error: stat error");
                return -NOT_FOUND;
            }
        }
    }

    if ((fd = open(path, O_RDONLY)) == -1) {
        free(path);
        log_error("handle_get error: open error");
        return -INTERNAL_SERVER_ERROR;
    }

    /* get the size of file by lseek */
    if ((*size = lseek(fd, 0, SEEK_END)) == -1) {
        free(path);
        close(fd);
        log_error("handle_get error: lseek error");
        return -INTERNAL_SERVER_ERROR;
    }
    /* restore offset */
    if (lseek(fd, 0, SEEK_SET) == -1) {
        free(path);
        close(fd);
        log_error("handle_get error: lseek error");
        return -INTERNAL_SERVER_ERROR;
    }

    strcpy(mimetype, get_mimetype(path));
    free(path);

    strftime(last_modifiled, 128, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&(s.st_mtime)));

    return fd;
}

/** @brief Internal GET and HEAD handler which only return headers
 *
 *  This process will be called by both handle_get and handle_head. It only
 *  send response line and response header to the client.
 *
 *  @param client A pointer to corresponding client object
 *  @param flag Indicate the request type which will be used to decide whether
 *         or not the message body should be return
 *  @return 0 if OK. Response status code on error
 */
static int internal_handler(http_client_t *client, int flag) {
    char buf[MAXBUF];
    char last_modifiled[128], date[128], mimetype[128];
    int n, size, fd;
    time_t current_time;

    if ((fd = open_file(client->req->uri, &size, mimetype, last_modifiled)) < 0)
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
    client_write_string(client, "\r\n");

    if (flag == F_GET) {
        while ((n = read(fd, buf, MAXBUF)) > 0)
            client_write(client, buf, n);
    }

    close(fd);

    return 0;
}
/** @brief Handle HTTP GET request
 *
 *  @param client A pointer to corresponding client object
 *  @return 0 if OK. Response status code if error.
 */
int handle_get(http_client_t *client) {
    log_msg(L_INFO, "Handler GET request. URI: %s\n", client->req->uri);
    return internal_handler(client, F_GET);
}

/** @brief Handle HTTP HEAD request
 *
 *  @param client A pointer to corresponding client object
 *  @return 0 if OK. Response status code if error.
 */
int handle_head(http_client_t *client) {
    log_msg(L_INFO, "Handle HEAD request. URI: %s\n", client->req->uri);
    return internal_handler(client, F_HEAD);
}

/** @brief Handle HTTP POST request
 *
 *  @param client A pointer to corresponding client object
 *  @return 0 if OK. Response status code if error.
 */
int handle_post(http_client_t *client) {
    char *buf;
    int content_len;
    int i;

    log_msg(L_INFO, "Handle POST request. URI: %s\n", client->req->uri);
    buf = get_request_header(client->req, "Content-Length");
    if (buf == NULL)
        return LENGTH_REQUIRED;

    //validate content-length
    if (strlen(buf) == 0)
        return BAD_REQUEST;
    for (i = 0; i < strlen(buf); ++i)
        if (buf[i] < '0' || buf[i] >'9') //each char in range ['0', '9']
            return BAD_REQUEST;

    content_len =  atoi(buf);

    /*
     * For now, since CGI has not been implemented, the response to a POST
     * request would be 501 Not Implemented
     */
    return NOT_IMPLEMENTED;
}
