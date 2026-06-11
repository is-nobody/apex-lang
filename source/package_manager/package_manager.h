#ifndef PACKAGE_MANAGER_H
#define PACKAGE_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
    // Windows networking
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    // POSIX networking
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

// Helper to send HTTP GET request and return status code
static inline int pm_http_get_status(const char* host, const char* path) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return -1;
#endif

    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent* server;
    char buffer[4096];
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
        WSACleanup();
#endif
        return -1;
    }
    
    server = gethostbyname(host);
    if (server == NULL) { 
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
        return -1; 
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(3000);
    
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
        return -1;
    }
    
    char request[1024];
    snprintf(request, sizeof(request), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
#ifdef _WIN32
    send(sockfd, request, strlen(request), 0);
#else
    write(sockfd, request, strlen(request));
#endif
    
    int total = 0;
    int n;
    while ((n = recv(sockfd, buffer + total, sizeof(buffer) - total - 1, 0)) > 0) {
        total += n;
    }
    buffer[total] = '\0';
    
#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
    
    // Parse HTTP Status Code (e.g., "HTTP/1.1 200 OK")
    int status = 0;
    if (sscanf(buffer, "HTTP/%*d.%*d %d", &status) == 1) {
        return status;
    }
    return -1;
}

// Helper to download file via HTTP
static inline bool pm_http_download(const char* host, const char* path, const char* dest_path) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
#endif

    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent* server;
    char buffer[8192];
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }
    
    server = gethostbyname(host);
    if (server == NULL) { 
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
        return false; 
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(3000);
    
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
        return false;
    }
    
    char request[1024];
    snprintf(request, sizeof(request), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
#ifdef _WIN32
    send(sockfd, request, strlen(request), 0);
#else
    write(sockfd, request, strlen(request));
#endif
    
    FILE* fp = fopen(dest_path, "wb");
    if (!fp) { 
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
        return false; 
    }
    
    bool header_skipped = false;
    int n;
    while ((n = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        if (!header_skipped) {
            char* body_start = strstr(buffer, "\r\n\r\n");
            if (body_start) {
                body_start += 4;
                int header_len = body_start - buffer;
                fwrite(body_start, 1, n - header_len, fp);
                header_skipped = true;
            }
        } else {
            fwrite(buffer, 1, n, fp);
        }
    }
    
    fclose(fp);
#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
    return true;
}

int install_package(const char* package_name);
int publish_package();

#endif // PACKAGE_MANAGER_H