/** @file http_parser.c
 *  @brief Parse http requests and call handler to handle them
 *
 *  @author Chao Xin(cxin)
 */
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "http_parser.h"
#include "request_handler.h"
#include "log.h"

/** @brief Check if string str start with string sub
 *  @return 1 if true, 0 if false
 */
static int startwith(char* str, char* sub) {
    int i = 0;
    if (strlen(sub) > strlen(str))
        return 0;
    for (i = 0; i < strlen(sub); ++i)
        if (str[i] != sub[i])
            return 0;
    return 1;
}

/** @brief Parse a uri
 *
 *  Check if the given uri points to a static file or a cgi script. The
 *  path to the static file or cgi path is stored in buf. When the uri
 *  points to a cgi script, the query string is stored in req->query.
 */
static void parse_uri(http_request_t* req, char* uri) {
    char *cgi_head = "/cgi/";
    char *cgi_start, *path_start, *query_start;
    int n = strlen(uri),
        cgi_head_len = strlen(cgi_head);

    query_start = strchr(uri, '?');
    if (query_start == NULL) {      // No query string
        strcpy(req->uri, uri);
        req->query[0] = '\0';
    } else {                // With query string
        strncpy(req->uri, uri, query_start - uri);
        req->uri[query_start - uri] = '\0';
        strcpy(req->query, query_start + 1);
    }

    if (startwith(uri, cgi_head)) {     // Cgi?
        req->is_cgi = 1;
        cgi_start = uri + cgi_head_len - 1;
        // Further parse into SCRIPT_NAME and PATH_INFO
        path_start = strchr(cgi_start + 1, '/');
        if (path_start == NULL) {
            strcpy(req->script_name, cgi_start);
            req->path[0] = '\0';
        } else {
            strncpy(req->script_name, cgi_start, path_start - cgi_start);
            req->script_name[path_start - cgi_start] = '\0';
            if (query_start == NULL)
                strcpy(req->path, path_start);
            else {
                strncpy(req->path, path_start, query_start - path_start);
                req->path[query_start - path_start] = '\0';
            }
        }
    } else {
        req->is_cgi = 0;
    }
}

/** @brief Parse the first line of a request
 *
 *  @return 0 on success. HTTP status code on error.
 */
static int parse_request_line(http_request_t* req, char *line) {
    char method[MAXBUF], uri[MAXBUF], version[MAXBUF];

    if (sscanf(line, "%s %s %s", method, uri, version) < 3) {
        log_msg(L_ERROR, "Bad request line: %s\n", line);
        return BAD_REQUEST;
    }

    if (strlen(uri) > MAX_URI_LEN) {
        log_msg(L_ERROR, "URI too long\n");
        return BAD_REQUEST;
    }

    req->method = -1;
    if (strcicmp(method, "GET") == 0) req->method = M_GET;
    if (strcicmp(method, "HEAD") == 0) req->method = M_HEAD;
    if (strcicmp(method, "POST") == 0) req->method = M_POST;

    //Method not allowed.
    if (req->method == -1) {
        log_msg(L_ERROR, "Method not allowed: %s\n", method);
        return METHOD_NOT_ALLOWED;
    }

    //Wrong version
    if (strcicmp(version, http_version) != 0) {
        log_msg(L_ERROR, "Version not supported: %s\n", version);
        return HTTP_VERSION_NOT_SUPPORTED;
    }

    parse_uri(req, uri);

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

    //Remove heading and trailing spaces
    while (head <= tail) {
        if (*head != ' ' && *tail != ' ')
            break;
        if (*head == ' ')
            head += 1;
        if (*tail == ' ')
            tail -= 1;
    }
    //After removing heading and trailing spaces, the string become empty
    if (head > tail)
        return NULL;

    buf = malloc(tail - head + 2);
    strncpy(buf, head, tail - head + 1);
    buf[tail - head + 1] = '\0';

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
        return -1;
    }

    ++req->cnt_headers;
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
    int ret, i;
    char line[MAXBUF];
    char* buf;

    if (client->status == C_IDLE) {  /* A new request, parse request line */
        ret = client_readline(client, line);
        if (strlen(line) == 0) return 0;

        log_msg(L_HTTP_DEBUG, "%s\n", line);

        if (ret == 0) return 0;

        /* The length of a line exceed MAXBUF */
        if (ret < 0) {
            log_error("A line in request is too long");
            return end_request(client, BAD_REQUEST);
        }

        if (client->req != NULL) free(client->req);
        client->req = new_request();

        /* parse request line and store information in client->req */
        if ((ret = parse_request_line(client->req, line)) > 0)
            return end_request(client, ret);

        /* Now start parsing header */
        client->status = C_PHEADER;
    }

    /*
     *  Read request headers. When finish reading request headers of a POST
     *  request without error, client->req->content_length will be set
     *  correspondingly. Thus client->req->content_length == -1 means the
     *  request header section has not ended.
     */
    while (client->status == C_PHEADER && client_readline(client, line) > 0) {
        log_msg(L_HTTP_DEBUG, "%s\n", line);

        if (strlen(line) == 0) {    //Request header ends
            if (client->req->method == M_GET) ret = handle_get(client);
            if (client->req->method == M_HEAD) ret = handle_head(client);
            if (client->req->method == M_POST) {
                buf = get_request_header(client->req, "Content-Length");
                if (buf == NULL)
                    return end_request(client, LENGTH_REQUIRED);
                //validate content-length
                if (strlen(buf) == 0) return BAD_REQUEST;
                for (i = 0; i < strlen(buf); ++i)
                    if (buf[i] < '0' || buf[i] >'9') //each char in range ['0', '9']
                        return end_request(client, BAD_REQUEST);

                client->req->content_length = atoi(buf);

                /* Now start receiving body */
                client->status = C_PBODY;
                break;
            }

            if (ret != 0)
                return end_request(client, ret);
            else {
                /* The client signal a "Connection: Close" */
                if (connection_close(client->req))
                    client->alive = 0;

                return ret;
            }
        }
        ret = parse_header(client->req, line);
        if (ret == -1) {
            log_msg(L_ERROR, "Bad request header format: %s\n", line);
            return end_request(client, BAD_REQUEST);
        }
    }

    /*
     * We've finished reading and parsing request header. Now, see if the body
     * of the request is ready. If so, copy data
     */
    if (client->status == C_PBODY) {
        // Reveive complete body?
        if (client->in->datasize - client->in->pos >= client->req->content_length) {
            /* Let body points to corresponding memory in the input buffer */
            client->req->body = client->in->buf + client->in->pos;
            client->in->pos += client->req->content_length;
            ret = handle_post(client);

            if (ret != 0)
                return end_request(client, ret);
            else {
                /* The client signal a "Connection: Close" */
                if (connection_close(client->req))
                    client->alive = 0;

                return ret;
            }
        }

        /* Body not ready, next time then */
        return 0;
    }

    return 0;
}
