#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: vfs_packer <binary_path> <base_dir> <script1> [script2...]\n");
        return 1;
    }
    const char* bin_path = argv[1];
    const char* base_dir = argv[2];
    int num_files = argc - 3;

    typedef struct {
        char* vpath;
        uint32_t vpath_len;
        char* data;
        uint32_t data_len;
    } FileEntry;
    
    FileEntry* entries = malloc(sizeof(FileEntry) * num_files);
    uint32_t total_payload_size = 4; // для записи num_files
    
    // 1. Читаем все файлы в память и считаем размер
    for (int i = 0; i < num_files; i++) {
        const char* real_path = argv[3 + i];
        
        // Вычисляем относительный путь от base_dir
        const char* rel_path = real_path;
        size_t base_len = strlen(base_dir);
        if (strncmp(real_path, base_dir, base_len) == 0) {
            rel_path = real_path + base_len;
            while (*rel_path == '/' || *rel_path == '\\') rel_path++;
        }
        
        // Формируем виртуальный путь
        char virtual_path[4096];
        snprintf(virtual_path, sizeof(virtual_path), ".EMBEDDED/%s", rel_path);
        // Нормализуем слеши
        for (char* p = virtual_path; *p; p++) {
            if (*p == '\\') *p = '/';
        }
        
        entries[i].vpath = strdup(virtual_path);
        entries[i].vpath_len = (uint32_t)strlen(virtual_path);
        
        FILE* f = fopen(real_path, "rb");
        if (!f) {
            fprintf(stderr, "Cannot open script: %s\n", real_path);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        long script_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        entries[i].data = malloc(script_size);
        entries[i].data_len = (uint32_t)script_size;
        fread(entries[i].data, 1, script_size, f);
        fclose(f);
        
        total_payload_size += 4 + entries[i].vpath_len + 4 + entries[i].data_len;
    }
    
    // 2. Дописываем в бинарник
    FILE* out = fopen(bin_path, "ab");
    if (!out) {
        fprintf(stderr, "Cannot open binary: %s\n", bin_path);
        return 1;
    }
    
    uint32_t num_files_32 = (uint32_t)num_files;
    fwrite(&num_files_32, 4, 1, out);
    
    for (int i = 0; i < num_files; i++) {
        fwrite(&entries[i].vpath_len, 4, 1, out);
        fwrite(entries[i].vpath, 1, entries[i].vpath_len, out);
        fwrite(&entries[i].data_len, 4, 1, out);
        fwrite(entries[i].data, 1, entries[i].data_len, out);
        
        free(entries[i].vpath);
        free(entries[i].data);
    }
    free(entries);
    
    // Пишем размер и маркер в самом конце
    fwrite(&total_payload_size, 4, 1, out);
    fwrite("__APEX_INT_PAYLOAD__", 1, 20, out);
    
    fclose(out);
    printf("[vfs_packer] Embedded %d built-in script(s) into %s\n", num_files, bin_path);
    return 0;
}