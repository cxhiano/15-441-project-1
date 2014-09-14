#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "http_parser.h"
#include "log.h"

static int strcicmp(char* s1, char* s2) {
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

    printf("key: %s\nvalue: %s\n", header->key, header->val);

    return 0;
}

static void bad_request(http_client_t *client) {
    send_response_line(client, BAD_REQUEST);
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
            bad_request(client);
            return BAD_REQUEST;
        }

        /* parse request line and store information in client->req */
        if ((ret = parse_request_line(client->req, line)) > 0) {
            send_response_line(client, ret);
            deinit_request(client->req);
            client->req = NULL;
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

            return ret;
        }
        ret = parse_header(client->req, line);
        if (ret == -1) {
            bad_request(client);
            return BAD_REQUEST;
        }
    }

    return 0;
}
