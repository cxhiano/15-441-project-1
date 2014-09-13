#include "http_parser.h"
#include "log.h"

static int parse_request_line(client_list_t *client, char *line) {
    if (sscanf(line, "%s %s %s", method, uri, version) < 3) {
        log_error("bad request line");
        return -1;
    }

    req->method =

}

static int get_line(client_list_t *client, char *line) {
    len = client_readline(client, line);
    if (len == -1) {
        log_error("line too long");
        return -1;
    }
    if (len == 0) return 0;
    if (len == 1 || line[len - 1] != '\r') {
        log_error("line does not end with CRLF");
        return 0;
    }

}

int http_parse(client_list_t *client) {
    int len;
    char line[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];

    if (client->req->method == NULL) {

        if (parse_request_line(client, line) < 0) {
            return -1;
        }
    }

}