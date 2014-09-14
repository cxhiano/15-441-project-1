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
