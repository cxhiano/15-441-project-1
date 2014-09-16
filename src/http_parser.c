#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "http_parser.h"
#include "request_handler.h"
#include "log.h"

/** @brief Parse the first line of a request
 *
 *  @return 0 on success. HTTP status code on error.
 */
static int parse_request_line(http_request_t* req, char *line) {
    char version[MAXBUF];

    if (sscanf(line, "%s %s %s", req->method, req->uri, version) < 3) {
        log_msg(L_ERROR, "Bad request line: %s\n", line);
        return BAD_REQUEST;
    }

    //Method not allowed.
    if (strcicmp(req->method, "GET") != 0 &&
            strcicmp(req->method, "HEAD") != 0 &&
            strcicmp(req->method, "POST") != 0) {
        log_msg(L_ERROR, "Method not allowed: %s\n", req->method);
        return METHOD_NOT_ALLOWED;
    }

    //Wrong version
    if (strcicmp(version, http_version) != 0) {
        log_msg(L_ERROR, "Version not supported: %s\n", version);
        return HTTP_VERSION_NOT_SUPPORTED;
    }

    return 0;
}

/** @brief Return a trimmed version of a string which starts at head and ends
 *         at tail.
 *
 *  Before copying, remove heading and trailing while spaces. If after
 *  trimming, the string becomes empty, return NULL.
 *
 *  @param head A pointer to the first character in the string
 *  @param tail A pointer to the last character in the string
 *  @return A copy of the trimmed version. NULL if the string becomes empty
 *          after trimming.
 */
static char* copy_trimmed_string(char *head, char* tail) {
    char *buf;

    while (head <= tail) {
        if (*head != ' ' && *tail != ' ')
            break;
        if (*head == ' ')
            head += 1;
        if (*tail == ' ')
            tail -= 1;
    }
    if (head > tail)
        return NULL;

    buf = malloc(tail - head + 2);
    strncpy(buf, head, tail - head + 1);

    return buf;
}

/** @brief Parse a line into key/val pair and add them into header collection
 *         in the request object.
 *
 *  @return 0 on success. -1 if parse error.
 */
static int parse_header(http_request_t* req, char* line) {
    char *val;
    http_header_t *header;

    val = strchr(line, ':');
    /* Seperator not found or is the first or last character */
    if (val == NULL || val[1] == '\0' || val == line)
        return -1;

    header = malloc(sizeof(http_header_t));
    header->key = copy_trimmed_string(line, val - 1);
    if (header->key == NULL) {
        free(header);
        return -1;
    }
    header->val = copy_trimmed_string(val + 1, line + strlen(line) - 1);
    if (header->val == NULL) {
        deinit_header(header);
        free(header);
        return -1;
    }

    /* Insert new header into header list in request object */
    header->next = req->headers;
    req->headers = header;

    return 0;
}

/** @brief Parse and response to request from a client
 *
 *  @return 0 if the connection should be kept alive. -1 if the connection
 *          should be closed.
 */
int http_parse(http_client_t *client) {
    int ret;
    char line[MAXBUF];

    if (client->req == NULL) {  /* A new request, parse request line */
        ret = client_readline(client, line);
        if (strlen(line) == 0) return 0;

        client->req = new_request();

        log_msg(L_HTTP_DEBUG, "%s\n", line);

        if (ret == 0) return 0;

        /* The length of a line exceed MAXBUF */
        if (ret < 0) {
            log_error("A line in request is too long");
            return end_request(client, BAD_REQUEST);
        }

        /* parse request line and store information in client->req */
        if ((ret = parse_request_line(client->req, line)) > 0)
            return end_request(client, ret);
    }

    /*  Read request headers */
    while (client_readline(client, line) > 0) {
        log_msg(L_HTTP_DEBUG, "%s\n", line);
        if (strlen(line) == 0) {    //Request header ends
            if (strcicmp(client->req->method, "GET") == 0)
                ret = handle_get(client);
            if (strcicmp(client->req->method, "POST") == 0)
                ret = handle_post(client);
            if (strcicmp(client->req->method, "HEAD") == 0)
                ret = handle_head(client);

            if (ret != 0)
                return end_request(client, ret);
            else {
                /* The client signal a "Connection: Close" */
                if (connection_close(client->req))
                    ret = -1;
                else
                    ret = 0;

                deinit_request(client->req);
                client->req = NULL;

                return ret;
            }
        }
        ret = parse_header(client->req, line);
        if (ret == -1) {
            log_msg(L_ERROR, "Bad request header format: %s\n", line);
            return end_request(client, BAD_REQUEST);
        }
    }

    return 0;
}
