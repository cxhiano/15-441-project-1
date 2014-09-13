#include <string.h>
#include "config.h"
#include "http_parser.h"
#include "log.h"

static int parse_request_line(http_request_t* req, char *line) {
    char version[MAXBUF];

    if (sscanf(line, "%s %s %s", req->method, req->uri, version) < 3) {
        log_error("bad request line");
        return BAD_REQUEST;
    }

    if (strcmp(req->method, "GET") != 0 &&
            strcmp(req->method, "HEAD") != 0 &&
            strcmp(req->method, "POST") != 0) {
        return METHOD_NOT_ALLOWED;
    }

    if (strcmp(version, http_version) != 0) {
        return HTTP_VERSION_NOT_SUPPORTED;
    }

    return 0;
}

static int get_line(http_client_t *client, char *line) {
    int len = client_readline(client, line);

    if (len == -1) {
        log_error("line too long");
        return -1;
    }

    return 0;
}

int http_parse(http_client_t *client) {
    int ret;

    char line[MAXBUF];

    if (client->req == NULL) {
        client->req = new_request();

        ret = get_line(client, line);
        if (ret < 0) {
            send_response_line(client, BAD_REQUEST);
            return BAD_REQUEST;
        }

        ret = parse_request_line(client->req, line);
        if (ret > 0) {
            send_response_line(client, ret);
            return ret;
        }
    }

    while (client_readline(client, line) > 0) {
        puts(line);
    }

    return 0;

}