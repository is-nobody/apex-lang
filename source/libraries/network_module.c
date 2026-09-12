// source/libraries/network_module.c
// Implementation of Network Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "network_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define HTTP_MAX_RESPONSE (16 * 1024 * 1024)  // hard cap on response size to avoid runaway allocation

// growable byte buffer for building requests and holding responses
typedef struct {
    char* buffer;  // heap-allocated byte storage
    int length;    // number of valid bytes in buffer
    int capacity;  // total allocated bytes
} NetBuffer;

// initialize a buffer with the given capacity (min 64 bytes)
static void nb_init(NetBuffer* nb, int initial_capacity) {
    nb->capacity = initial_capacity > 64 ? initial_capacity : 64;  // enforce minimum size
    nb->buffer = (char*)malloc(nb->capacity);                      // allocate storage
    if (!nb->buffer) { nb->length = 0; return; }                   // allocation failed, leave empty
    nb->length = 0;                                                // no bytes yet
    nb->buffer[0] = '\0';                                          // null terminate for safety
}

// append len bytes from str to the buffer, growing if needed
static void nb_append(NetBuffer* nb, const char* str, int len) {
    if (!nb->buffer) return;                                       // buffer not initialized
    if (nb->length + len + 1 > nb->capacity) {                     // need more space (plus null)
        int new_cap = (nb->length + len + 1) * 2;                  // double to fit new data
        char* new_buf = (char*)realloc(nb->buffer, new_cap);       // grow buffer
        if (!new_buf) return;                                      // realloc failed, keep old
        nb->buffer = new_buf;                                      // update pointer
        nb->capacity = new_cap;                                    // update capacity
    }
    memcpy(nb->buffer + nb->length, str, len);                     // copy new bytes after existing
    nb->length += len;                                             // advance length
    nb->buffer[nb->length] = '\0';                                 // null terminate
}

// free the buffer's storage and reset fields
static void nb_free(NetBuffer* nb) {
    if (nb->buffer) free(nb->buffer);  // release heap storage if any
    nb->buffer = NULL;                 // clear dangling pointer
    nb->length = 0;                    // reset length
}

// parse "http://host[:port]/path" into host, port, path
static bool parse_url(const char* url, char* host, int host_size, int* port, char* path, int path_size) {
    const char* p = url;                                          // cursor into url
    if (strncmp(p, "http://", 7) == 0) p += 7;                    // strip http:// scheme
    else if (strncmp(p, "https://", 8) == 0) return false;        // no tls support
    else return false;                                            // require an explicit scheme

    const char* host_start = p;                                   // host begins after scheme
    while (*p && *p != ':' && *p != '/') p++;                     // scan until port or path
    size_t host_len = (size_t)(p - host_start);                   // host length
    if (host_len >= (size_t)host_size) return false;              // host too long for buffer
    memcpy(host, host_start, host_len);                           // copy host bytes
    host[host_len] = '\0';                                        // null terminate host

    *port = 80;                                                   // default http port
    if (*p == ':') {                                              // explicit port present
        p++;                                                      // skip colon
        *port = atoi(p);                                          // parse port number
        while (*p && *p != '/') p++;                              // skip to path start
    }

    if (*p == '/') {                                              // path present
        size_t path_len = strlen(p);                              // path length including null
        if (path_len >= (size_t)path_size) return false;          // path too long for buffer
        memcpy(path, p, path_len + 1);                            // copy path with terminator
    } else {
        strcpy(path, "/");                                        // no path, default to root
    }
    return true;                                                  // url parsed successfully
}

// open a tcp connection to host:port, return fd or -1 on failure
static int net_connect(const char* host, int port) {
    struct hostent* he = gethostbyname(host);                     // resolve hostname to ipv4
    if (!he) return -1;                                           // dns failure

    int sock = socket(AF_INET, SOCK_STREAM, 0);                   // create tcp socket
    if (sock < 0) return -1;                                      // socket creation failed

    struct sockaddr_in addr;                                      // destination address
    memset(&addr, 0, sizeof(addr));                               // zero for safety
    addr.sin_family = AF_INET;                                    // ipv4
    addr.sin_port = htons((uint16_t)port);                        // port in network byte order
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);     // copy first resolved ip

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {  // attempt connection
        close(sock);                                              // close on failure
        return -1;
    }
    return sock;                                                  // return connected socket
}

// send all bytes in data, return false on any error
static bool net_send_all(int sock, const char* data, int len) {
    int sent = 0;                                                 // bytes successfully sent
    while (sent < len) {                                          // loop until fully sent
        int n = (int)send(sock, data + sent, len - sent, 0);      // send remaining bytes
        if (n <= 0) return false;                                 // error or connection closed
        sent += n;                                                // advance progress
    }
    return true;                                                  // all bytes sent
}

// read everything from the socket until close, appending to the buffer
static bool net_recv_all(int sock, NetBuffer* nb) {
    char chunk[8192];                                             // receive scratch buffer
    int n;                                                        // bytes received per call
    while ((n = (int)recv(sock, chunk, sizeof(chunk), 0)) > 0) {  // loop until peer closes
        if (nb->length + n > HTTP_MAX_RESPONSE) return false;     // refuse oversized response
        nb_append(nb, chunk, n);                                  // append received bytes
    }
    return true;                                                  // reached eof or error absorbed
}

// extract the numeric status code from an HTTP response line
static int parse_status(const char* response) {
    if (strncmp(response, "HTTP/", 5) != 0) return 0;             // not an http response
    const char* p = strchr(response, ' ');                        // find space after status line
    if (!p) return 0;                                             // malformed status line
    return atoi(p + 1);                                           // parse status code
}

// locate the body in a raw http response, writes length to body_len
static const char* parse_body(const char* response, int response_len, int* body_len) {
    const char* p = strstr(response, "\r\n\r\n");                 // find end of headers
    if (!p) { *body_len = 0; return NULL; }                       // no header terminator
    p += 4;                                                       // skip past \r\n\r\n
    *body_len = response_len - (int)(p - response);               // remaining bytes are body
    return p;                                                     // return body pointer
}

// perform a full http request, returning a table with status and body
static Value http_request(VM* vm, const char* url, const char* method,
                          const char* body, const char* content_type) {
    char host[256];                                               // hostname buffer
    char path[2048];                                              // request path buffer
    int port;                                                     // target port

    if (!parse_url(url, host, sizeof(host), &port, path, sizeof(path)))  // parse target url
        return MAKE_NONE();                                       // invalid url, return none

    int sock = net_connect(host, port);                           // open tcp connection
    if (sock < 0) return MAKE_NONE();                             // connection failed, return none

    NetBuffer req;                                                // request buffer
    nb_init(&req, 512);                                           // start with 512 bytes

    char header[1024];                                            // header formatting buffer
    int n = snprintf(header, sizeof(header),
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Apex/26.09\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n",
        method, path, host);                                      // build request line and headers
    nb_append(&req, header, n);                                   // append formatted headers

    if (body) {                                                   // request has a body
        n = snprintf(header, sizeof(header),
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n",
            content_type ? content_type : "application/x-www-form-urlencoded",
            (int)strlen(body));                                   // build content headers
        nb_append(&req, header, n);                               // append content headers
    }

    nb_append(&req, "\r\n", 2);                                   // blank line ends headers
    if (body) nb_append(&req, body, (int)strlen(body));           // append body if present

    if (!net_send_all(sock, req.buffer, req.length)) {            // send full request
        nb_free(&req);                                            // free request buffer
        close(sock);                                              // close socket
        return MAKE_NONE();                                       // send failed
    }
    nb_free(&req);                                                // free request buffer

    NetBuffer resp;                                               // response buffer
    nb_init(&resp, 4096);                                         // start with 4k
    if (!net_recv_all(sock, &resp)) {                             // read full response
        nb_free(&resp);                                           // free response buffer
        close(sock);                                              // close socket
        return MAKE_NONE();                                       // receive failed
    }
    close(sock);                                                  // done with socket

    int status = parse_status(resp.buffer);                       // extract status code
    int body_len = 0;                                             // response body length
    const char* body_ptr = parse_body(resp.buffer, resp.length, &body_len);  // locate body

    Table* t = table_create(8);                                   // result table
    Value k_status = MAKE_STRING(string_intern(&vm->intern_table, "status", 6));  // intern "status"
    Value v_status = MAKE_NUMBER(status);                         // box status as number
    table_set(t, k_status, v_status);                             // store status
    value_decref(k_status);                                       // release local reference
    value_decref(v_status);                                       // release local reference

    Value k_body = MAKE_STRING(string_intern(&vm->intern_table, "body", 4));      // intern "body"
    Value v_body = MAKE_STRING(string_intern(&vm->intern_table,
        body_ptr ? body_ptr : "", body_ptr ? body_len : 0));      // intern response body
    table_set(t, k_body, v_body);                                 // store body
    value_decref(k_body);                                         // release local reference
    value_decref(v_body);                                         // release local reference

    nb_free(&resp);                                               // free response buffer
    return MAKE_TABLE(t);                                         // return result table
}

// dispatch network builtin calls by name
bool network_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "network.get") == 0) {                  // http get
        if (arg_count < 1 || !IS_STRING(args[0])) {               // require url string
            *result = MAKE_NONE();                                // invalid args, return none
            return true;
        }
        *result = http_request(vm, AS_STRING(args[0])->chars, "GET", NULL, NULL);  // perform get
        return true;
    }

    if (strcmp(name, "network.post") == 0) {                 // http post
        if (arg_count < 1 || !IS_STRING(args[0])) {               // require url string
            *result = MAKE_NONE();                                // invalid args, return none
            return true;
        }
        const char* url = AS_STRING(args[0])->chars;              // extract url
        const char* body = NULL;                                  // optional body
        const char* ctype = NULL;                                 // optional content type
        if (arg_count >= 2 && IS_STRING(args[1])) body = AS_STRING(args[1])->chars;   // body arg
        if (arg_count >= 3 && IS_STRING(args[2])) ctype = AS_STRING(args[2])->chars;  // content type arg
        *result = http_request(vm, url, "POST", body, ctype);     // perform post
        return true;
    }

    return false;                                                 // unknown builtin
}