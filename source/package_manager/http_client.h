#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
#endif

typedef struct {
    int status_code;
    long content_length;
    char* body;
} HttpResponse;

// Portable replacement for memmem
static inline char* http_memmem(const void* haystack, size_t haystack_len, const void* needle, size_t needle_len) {
    if (needle_len == 0) return (char*)haystack;
    if (needle_len > haystack_len) return NULL;
    
    const unsigned char* h = (const unsigned char*)haystack;
    const unsigned char* n = (const unsigned char*)needle;
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(h + i, n, needle_len) == 0) {
            return (char*)(h + i);
        }
    }
    return NULL;
}

// Helper to create a connected socket to localhost:port (IPv4 only)
static inline int create_local_socket(int port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return -1;
#endif

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
#ifdef _WIN32
        WSACleanup();
#endif
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    // Force IPv4 loopback
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
        return -1;
    }
    return sockfd;
}

static inline void cleanup_socket(int sockfd) {
#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
}

// Send a GET request and return the response
static inline HttpResponse http_get(const char* host, int port, const char* path) {
    HttpResponse resp = {0, 0, NULL};
    int sockfd = create_local_socket(port);
    if (sockfd < 0) {
        fprintf(stderr, "Error: Cannot connect to %s:%d\n", host, port);
        return resp;
    }

    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
             path, host);
    
#ifdef _WIN32
    send(sockfd, request, strlen(request), 0);
#else
    if (write(sockfd, request, strlen(request)) < 0) {
        fprintf(stderr, "Error: Failed to send request to %s:%d\n", host, port);
        cleanup_socket(sockfd);
        resp.status_code = -1;
        return resp;
    }
#endif

    char buffer[8192];
    int total_read = 0;
    int n;
    char* full_response = malloc(1); 
    if (!full_response) { cleanup_socket(sockfd); return resp; }
    full_response[0] = '\0';

    while ((n = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        char* temp = realloc(full_response, total_read + n + 1);
        if (!temp) { free(full_response); cleanup_socket(sockfd); return resp; }
        full_response = temp;
        memcpy(full_response + total_read, buffer, n);
        total_read += n;
    }
    full_response[total_read] = '\0';
    cleanup_socket(sockfd);

    sscanf(full_response, "HTTP/%*d.%*d %d", &resp.status_code);

    char* body_start = strstr(full_response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        resp.body = strdup(body_start);
        resp.content_length = strlen(resp.body);
    } else {
        resp.body = strdup("");
    }

    free(full_response);
    return resp;
}

// Download a file directly to disk
static inline bool http_download_file(const char* host, int port, const char* path, const char* dest_path) {
    int sockfd = create_local_socket(port);
    if (sockfd < 0) return false;

    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
             path, host);
#ifdef _WIN32
    send(sockfd, request, strlen(request), 0);
#else
    if (write(sockfd, request, strlen(request)) < 0) {
        cleanup_socket(sockfd);
        return false;
    }
#endif

    FILE* fp = fopen(dest_path, "wb");
    if (!fp) { cleanup_socket(sockfd); return false; }

    char buffer[8192];
    bool header_skipped = false;
    int n;
    while ((n = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        if (!header_skipped) {
            // Use portable http_memmem instead of GNU memmem
            char* body_start = http_memmem(buffer, n, "\r\n\r\n", 4);
            if (body_start) {
                int header_len = (body_start - buffer) + 4;
                fwrite(body_start, 1, n - header_len, fp);
                header_skipped = true;
            }
        } else {
            fwrite(buffer, 1, n, fp);
        }
    }

    fclose(fp);
    cleanup_socket(sockfd);
    return true;
}

#endif // HTTP_CLIENT_H