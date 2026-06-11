#ifndef ZIP_UTILS_H
#define ZIP_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>

// Conditional zlib inclusion
#ifdef _WIN32
    #define HAVE_ZLIB 0 // Disable zlib for MinGW cross-compile for now
#else
    #include <zlib.h>
    #define HAVE_ZLIB 1
#endif

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

// ZIP Structures
#pragma pack(push, 1)
typedef struct {
    uint32_t signature;       // 0x04034b50
    uint16_t version;
    uint16_t flags;
    uint16_t compression;     // 0 = Store, 8 = Deflate
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_length;
    uint16_t extra_field_length;
} ZipLocalFileHeader;

typedef struct {
    uint32_t signature;       // 0x02014b50
    uint16_t version_made;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_length;
    uint16_t extra_field_length;
    uint16_t comment_length;
    uint16_t disk_start;
    uint16_t internal_attr;
    uint32_t external_attr;
    uint32_t local_header_offset;
} ZipCentralDirEntry;
#pragma pack(pop)

static void mkdirs(const char* path) {
    char tmp[1024];
    char* p = NULL;
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static bool extract_zip(const char* zip_path, const char* dest_dir) {
#if !HAVE_ZLIB
    fprintf(stderr, "ZIP extraction requires zlib, which is not available in this build.\n");
    return false;
#else
    FILE* fp = fopen(zip_path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open zip file '%s'\n", zip_path);
        return false;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    while (ftell(fp) < file_size) {
        ZipLocalFileHeader header;
        if (fread(&header, sizeof(header), 1, fp) != 1) break;

        if (header.signature != 0x04034b50) {
            break;
        }

        char* filename = (char*)malloc(header.filename_length + 1);
        if (!filename) break;
        
        if (fread(filename, 1, header.filename_length, fp) != header.filename_length) {
            free(filename);
            break;
        }
        filename[header.filename_length] = '\0';

        // Skip extra field
        fseek(fp, header.extra_field_length, SEEK_CUR);

        // Construct full path
        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", dest_dir, filename);

        // Create directories if needed
        if (header.filename_length > 0 && filename[header.filename_length - 1] == '/') {
            mkdirs(full_path);
            free(filename);
            continue;
        }
        
        // Ensure parent directory exists
        char* dir_part = strdup(full_path);
        if (dir_part) {
            char* last_slash = strrchr(dir_part, '/');
            if (last_slash) {
                *last_slash = '\0';
                mkdirs(dir_part);
            }
            free(dir_part);
        }

        // Read compressed data
        uint8_t* compressed_data = (uint8_t*)malloc(header.compressed_size);
        if (!compressed_data) {
            free(filename);
            break;
        }
        
        if (fread(compressed_data, 1, header.compressed_size, fp) != header.compressed_size) {
            free(compressed_data);
            free(filename);
            break;
        }

        // Decompress
        FILE* out_fp = fopen(full_path, "wb");
        if (!out_fp) {
            fprintf(stderr, "Warning: Could not create file '%s'\n", full_path);
            free(filename);
            free(compressed_data);
            continue;
        }

        if (header.compression == 0) {
            // Stored (no compression)
            fwrite(compressed_data, 1, header.compressed_size, out_fp);
        } else if (header.compression == 8) {
            // Deflated
            z_stream strm;
            memset(&strm, 0, sizeof(strm));
            strm.next_in = compressed_data;
            strm.avail_in = header.compressed_size;
            
            if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
                fprintf(stderr, "Error: Inflate init failed for '%s'\n", filename);
                free(compressed_data);
                fclose(out_fp);
                free(filename);
                continue;
            }

            uint8_t out_buffer[8192];
            int ret;
            do {
                strm.next_out = out_buffer;
                strm.avail_out = sizeof(out_buffer);
                ret = inflate(&strm, Z_NO_FLUSH);
                if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                    fprintf(stderr, "Error: Decompression failed for '%s'\n", filename);
                    break;
                }
                int have = sizeof(out_buffer) - strm.avail_out;
                if (have > 0) {
                    fwrite(out_buffer, 1, have, out_fp);
                }
            } while (ret != Z_STREAM_END);
            
            inflateEnd(&strm);
        } else {
            fprintf(stderr, "Warning: Unsupported compression method %d for '%s'\n", header.compression, filename);
        }

        fclose(out_fp);
        free(compressed_data);
        free(filename);
    }

    fclose(fp);
    return true;
#endif
}

#endif // ZIP_UTILS_H