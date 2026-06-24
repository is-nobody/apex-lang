#include "package_manager.h"
#include "http_client.h"
#include "zip_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <io.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

#define PM_MAX_INPUT 256
#define SESSION_FILE ".apex_session"

static bool file_exists(const char* path) { return access(path, F_OK) == 0; }

// Helper to send POST request with JSON body and get status code
static int http_post_json_status(const char* host, int port, const char* path, const char* json_body) {
    int sockfd = create_local_socket(port);
    if (sockfd < 0) return -1;
    
    size_t body_len = strlen(json_body);
    
    char header[1024];
    int header_len = snprintf(header, sizeof(header),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %llu\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, (unsigned long long)body_len);
    
    if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
        cleanup_socket(sockfd);
        return -1;
    }
    
#ifdef _WIN32
    if (send(sockfd, header, header_len, 0) < 0) {
#else
    if (write(sockfd, header, header_len) < 0) {
#endif
        cleanup_socket(sockfd);
        return -1;
    }
    
#ifdef _WIN32
    if (send(sockfd, json_body, body_len, 0) < 0) {
#else
    if (write(sockfd, json_body, body_len) < 0) {
#endif
        cleanup_socket(sockfd);
        return -1;
    }
    
    char resp_buf[4096];
    int total = 0;
    int n;
    while (total < (int)(sizeof(resp_buf) - 1) && (n = recv(sockfd, resp_buf + total, sizeof(resp_buf) - total - 1, 0)) > 0) {
        total += n;
    }
    resp_buf[total] = '\0';
    cleanup_socket(sockfd);
    
    if (total > 0) {
        int status = 0;
        if (sscanf(resp_buf, "HTTP/%*d.%*d %d", &status) == 1) {
            return status;
        }
    }
    
    return -1;
}

// Helper to send POST request with multipart form data
static bool http_post_multipart(const char* host, int port, const char* path, 
                                const char* username, const char* pkg_name, 
                                const char* file_path) {
    int sockfd = create_local_socket(port);
    if (sockfd < 0) return false;

    FILE* fp = fopen(file_path, "rb");
    if (!fp) { cleanup_socket(sockfd); return false; }
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char* file_content = malloc(file_size);
    if (!file_content) { fclose(fp); cleanup_socket(sockfd); return false; }
    
    if (fread(file_content, 1, file_size, fp) != (size_t)file_size) {
        free(file_content);
        fclose(fp);
        cleanup_socket(sockfd);
        return false;
    }
    fclose(fp);

    char boundary[] = "----ApexBoundary123456";
    char header_part[2048];
    snprintf(header_part, sizeof(header_part),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"username\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"pkg_name\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
        "Content-Type: application/zip\r\n\r\n",
        boundary, username, boundary, pkg_name, boundary, pkg_name);

    char footer_part[128];
    snprintf(footer_part, sizeof(footer_part), "\r\n--%s--\r\n", boundary);

    long total_size = strlen(header_part) + file_size + strlen(footer_part);

    char request_header[2048];
    snprintf(request_header, sizeof(request_header),
        "POST %s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "Content-Type: multipart/form-data; boundary=%s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n\r\n",
        path, host, boundary, total_size);

#ifdef _WIN32
    send(sockfd, request_header, strlen(request_header), 0);
    send(sockfd, header_part, strlen(header_part), 0);
    send(sockfd, file_content, file_size, 0);
    send(sockfd, footer_part, strlen(footer_part), 0);
#else
    if (write(sockfd, request_header, strlen(request_header)) < 0 ||
        write(sockfd, header_part, strlen(header_part)) < 0 ||
        write(sockfd, file_content, file_size) < 0 ||
        write(sockfd, footer_part, strlen(footer_part)) < 0) {
        free(file_content);
        cleanup_socket(sockfd);
        return false;
    }
#endif

    free(file_content);

    char buffer[4096];
    int n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    cleanup_socket(sockfd);
    
    if (n > 0) {
        buffer[n] = '\0';
        return (strstr(buffer, "201") != NULL || strstr(buffer, "200") != NULL);
    }
    return false;
}

static char* get_package_name_from_manifest() {
    if (!file_exists("manifest.apex")) return NULL;
    FILE* f = fopen("manifest.apex", "r");
    if (!f) return NULL;
    
    static char pkg_name[256];
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "name") && strstr(line, "=")) {
            char* start = strchr(line, '"');
            char* end = strrchr(line, '"');
            if (start && end && start != end) {
                start++; *end = '\0';
                strncpy(pkg_name, start, sizeof(pkg_name) - 1);
                fclose(f);
                return pkg_name;
            }
        }
    }
    fclose(f);
    return NULL;
}

static void create_default_readme(const char* pkg_name, const char* username, const char* license) {
    FILE* f = fopen("readme.md", "w");
    if (!f) return;
    time_t t = time(NULL); struct tm tm = *localtime(&t);
    fprintf(f, "## %s\nThis is description about `%s` project.\nCreated %04d-%02d-%02d\n\n## License\nThis project is under `%s`\n", 
            pkg_name, username, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, license);
    fclose(f);
    printf("\033[32mCreated default readme.md\033[0m\n");
}

static void create_mit_license(const char* username) {
    FILE* f = fopen("license.txt", "w");
    if (!f) return;
    time_t t = time(NULL); struct tm tm = *localtime(&t);
    fprintf(f, "MIT License\n\nCopyright (c) %d %s\n\nPermission is hereby granted, free of charge...\n", tm.tm_year + 1900, username);
    fclose(f);
    printf("\033[32mCreated MIT license.txt\033[0m\n");
}

static const char* detect_license() {
    const char* files[] = {"license", "license.txt", "license.md", NULL};
    for (int i = 0; files[i]; i++) {
        if (file_exists(files[i])) {
            FILE* f = fopen(files[i], "r"); if (!f) continue;
            char buf[2048]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[n] = '\0';
            for (size_t j = 0; j < n; j++) buf[j] = tolower(buf[j]);
            if (strstr(buf, "mit license")) return "MIT";
            if (strstr(buf, "apache license") && strstr(buf, "2.0")) return "Apache-2.0";
            if (strstr(buf, "gnu general public license") && strstr(buf, "3")) return "GPL-3.0";
            return "Unknown";
        }
    }
    return NULL;
}

static bool prompt_yes_no(const char* msg) {
    printf("%s [Y/n] ", msg);
    char input[16]; if (!fgets(input, sizeof(input), stdin)) return false;
    return (input[0] == 'Y' || input[0] == 'y' || input[0] == '\n');
}

int publish_package() {
    // 0. Check prerequisites
    HttpResponse resp = http_get("localhost", 3000, "/api/packages");
    if (resp.status_code != 200) {
        fprintf(stderr, "\033[31mError: Cannot connect to the package repository at localhost:3000.\nIs the server running? (cd server && npm start)\033[0m\n");
        if (resp.body) free(resp.body);
        return 1;
    }
    if (resp.body) free(resp.body);

    char username[PM_MAX_INPUT], password[PM_MAX_INPUT];
    bool is_logged_in = false;

    // 1. Check for existing session
    FILE* session = fopen(SESSION_FILE, "r");
    if (session) {
        if (fscanf(session, "%255s %255s", username, password) == 2) {
            printf("\033[36mFound saved session for '%s'. Verifying...\033[0m\n", username);
            
            // Verify session with server
            char login_body[1024];
            snprintf(login_body, sizeof(login_body), "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
            int status = http_post_json_status("localhost", 3000, "/api/auth/login", login_body);
            
            if (status == 200) {
                printf("\033[32mSession verified. Logged in as '%s'.\033[0m\n", username);
                is_logged_in = true;
            } else {
                printf("\033[33mSession expired or invalid. Please log in again.\033[0m\n");
                remove(SESSION_FILE);
            }
        }
        fclose(session);
    }

    // 2. Login / Register Flow if not logged in
    if (!is_logged_in) {
        while (true) {
            printf("Your login: ");
            if (!fgets(username, sizeof(username), stdin)) return 1;
            username[strcspn(username, "\n")] = 0;
            if (strlen(username) == 0) continue;
            
            char url[1024];
            snprintf(url, sizeof(url), "/api/users/%s", username);
            HttpResponse user_resp = http_get("localhost", 3000, url);
            
            if (user_resp.status_code == 200) {
                // User exists -> Login
                printf("\033[36mYou're trying to log in\033[0m\n");
                printf("Password: ");
                if (!fgets(password, sizeof(password), stdin)) return 1;
                password[strcspn(password, "\n")] = 0;
                
                char login_body[1024];
                snprintf(login_body, sizeof(login_body), "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
                int status = http_post_json_status("localhost", 3000, "/api/auth/login", login_body);
                
                if (status == 200) {
                    printf("\033[32mLogged in successfully.\033[0m\n");
                    is_logged_in = true;
                    break;
                } else {
                    fprintf(stderr, "\033[31mError: Invalid password.\033[0m\n");
                }
            } else if (user_resp.status_code == 404) {
                // User doesn't exist -> Register
                printf("\033[36mYou're creating new account\033[0m\n");
                printf("Password: ");
                if (!fgets(password, sizeof(password), stdin)) return 1;
                password[strcspn(password, "\n")] = 0;
                
                char reg_body[1024];
                snprintf(reg_body, sizeof(reg_body), "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
                int status = http_post_json_status("localhost", 3000, "/api/auth/register", reg_body);
                
                if (status == 201) {
                    printf("\033[32mAccount created and logged in.\033[0m\n");
                    is_logged_in = true;
                    break;
                } else {
                    fprintf(stderr, "\033[31mError: Registration failed (HTTP %d).\033[0m\n", status);
                }
            } else {
                 fprintf(stderr, "\033[31mError: Server unavailable (HTTP %d).\033[0m\n", user_resp.status_code);
            }
            
            if (user_resp.body) free(user_resp.body);
        }
        
        // Save session
        if (is_logged_in) {
            FILE* s = fopen(SESSION_FILE, "w");
            if (s) {
                fprintf(s, "%s %s", username, password);
                fclose(s);
                chmod(SESSION_FILE, 0600); // Secure permissions
            }
        }
    }
    
    // 3. Check Required Files
    const char* license_type = detect_license();
    if (!license_type) {
        if (prompt_yes_no("'license' file doesn't exists, under the MIT it?")) {
            create_mit_license(username);
            license_type = "MIT";
        } else license_type = "None";
    } else printf("\033[36mDetected license: %s\033[0m\n", license_type);
    
    char* pkg_name = get_package_name_from_manifest();
    if (!pkg_name) {
        fprintf(stderr, "\033[31mError: manifest.apex is missing or invalid!\nCreate a manifest.apex file with: name = \"your-package-name\"\033[0m\n");
        return 1;
    }
    
    bool has_readme = file_exists("readme") || file_exists("readme.txt") || file_exists("readme.md");
    if (!has_readme) {
        if (prompt_yes_no("'readme' file doesn't exists, create it with default content?")) {
            create_default_readme(pkg_name, username, license_type);
        }
    }
    
    // 4. Pack files to zip
    char zip_name[256]; snprintf(zip_name, sizeof(zip_name), "%s.zip", pkg_name);
    printf("\033[36mCreating package archive...\033[0m\n");
    if (!create_zip(zip_name, ".")) {
        fprintf(stderr, "\033[31mError: Failed to create package archive\033[0m\n");
        return 1;
    }
    
    // 5. Upload to server
    printf("\033[36mPublishing package '%s'...\033[0m\n", pkg_name);
    if (http_post_multipart("localhost", 3000, "/api/packages/publish", username, pkg_name, zip_name)) {
        printf("\033[32mPackage published successfully!\033[0m\n");
    } else {
        fprintf(stderr, "\033[31mError: Failed to publish package\033[0m\n");
    }
    
    remove(zip_name);
    return 0;
}