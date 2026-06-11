#include "package_manager.h"
#include "http_client.h"
#include "zip_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

int install_package(const char* package_name) {
    printf("\033[36mRequesting package '%s' from repository...\033[0m\n", package_name);
    
    // 1. Check if package exists
    char check_path[1024];
    snprintf(check_path, sizeof(check_path), "/api/packages/%s", package_name);
    HttpResponse resp = http_get("localhost", 3000, check_path);
    
    if (resp.status_code != 200) {
        fprintf(stderr, "\033[31mError: package doesn't exists\033[0m\n");
        if (resp.body) free(resp.body);
        return 1;
    }
    if (resp.body) free(resp.body);
    
    // 2. Create .packages directory structure
    mkdir(".packages", 0755);
    
    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), ".packages/%s", package_name);
    mkdir(pkg_dir, 0755);
    
    // 3. Download the zip file
    char zip_path[1024];
    snprintf(zip_path, sizeof(zip_path), "%s/%s.zip", pkg_dir, package_name);
    
    char download_path[1024];
    snprintf(download_path, sizeof(download_path), "/api/packages/%s/download", package_name);
    
    printf("\033[36mDownloading...\033[0m\n");
    if (!http_download_file("localhost", 3000, download_path, zip_path)) {
        fprintf(stderr, "\033[31mError: Failed to download package\033[0m\n");
        return 1;
    }
    
    // 4. Extract the zip file
    printf("\033[36mExtracting...\033[0m\n");
    if (!extract_zip(zip_path, pkg_dir)) {
        fprintf(stderr, "\033[31mError: Failed to extract package\033[0m\n");
        remove(zip_path);
        return 1;
    }
    
    // 5. Cleanup
    remove(zip_path);
    
    printf("\033[32mPackage '%s' installed successfully in .packages/%s\033[0m\n", package_name, package_name);
    return 0;
}