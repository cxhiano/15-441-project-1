#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "http_parser.h"
#include "request_handler.h"
#include "log.h"

static int parse_request_line(http_request_t* req, char *line) {
    char version[MAXBUF];

    if (sscanf(line, "%s %s %s", req->method, req->uri, version) < 3) {
        return BAD_REQUEST;
    }

    if (strcicmp(req->method, "GET") != 0 &&
            strcicmp(req->method, "HEAD") != 0 &&
            strcicmp(req->method, "POST") != 0) {
        return METHOD_NOT_ALLOWED;
    }

    if (strcicmp(version, http_version) != 0) {
        return HTTP_VERSION_NOT_SUPPORTED;
    }

    return 0;
}

static char* string_copy(char *start, int n) {
    char *buf = malloc(n);

    strncpy(buf, start, n);

    return buf;
}

static int parse_header(http_request_t* req, char* line) {
    char *val;
    http_header_t *header;

    val = strchr(line, ':');
    /* Seperator not found or is the first or last character */
    if (val == NULL || val[1] == '\0' || val == line)
        return -1;

    header = malloc(sizeof(http_header_t));
    header->key = string_copy(line, val - line);
    header->val = string_copy(val + 1, strlen(val + 1));

    /* Insert new header into header list in request object */
    header->next = req->headers;
    req->headers = header;

    return 0;
}

static void end_request(http_client_t *client, int code) {
    send_response_line(client, code);
    deinit_request(client->req);
    client->req = NULL;
}

int http_parse(http_client_t *client) {
    int ret;

    char line[MAXBUF];

    if (client->req == NULL) {  /* A new request, parse request line */
        client->req = new_request();

        ret = client_readline(client, line);

        if (ret == 0) return 0;

        /* The length of a line exceed MAXBUF */
        if (ret < 0) {
            end_request(client, BAD_REQUEST);
            return BAD_REQUEST;
        }

        /* parse request line and store information in client->req */
        if ((ret = parse_request_line(client->req, line)) > 0) {
            end_request(client, ret);
            return ret;
        }
    }

    while (client_readline(client, line) > 0) {
        if (strlen(line) == 0) {
            if (strcicmp(client->req->method, "GET") == 0)
                ret = handle_get(client);
            if (strcicmp(client->req->method, "POST") == 0)
                ret = handle_post(client);
            if (strcicmp(client->req->method, "HEAD") == 0)
                ret = handle_head(client);

            if (ret != 0)
                end_request(client, ret);
            else {
                deinit_request(client->req);
                client->req = NULL;
            }

            return ret;
        }
        ret = parse_header(client->req, line);
        if (ret == -1) {
            end_request(client, BAD_REQUEST);
            return BAD_REQUEST;
        }
    }

    return 0;
}
