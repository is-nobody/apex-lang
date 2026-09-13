// source/libraries/zip_module.c
// Implementation of ZIP Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "zip_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

#ifdef _WIN32
#include <sys/utime.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#endif

// zip file structure constants
#define ZIP_LOCAL_FILE_HEADER_SIG  0x04034b50          // local file header signature
#define ZIP_CENTRAL_FILE_HEADER_SIG 0x02014b50         // central file header signature
#define ZIP_END_OF_CENTRAL_DIR_SIG  0x06054b50         // end of central dir signature
#define ZIP_VERSION_NEEDED          0x0014             // version needed to extract
#define ZIP_VERSION_MADE_BY         0x0014             // version made by
#define ZIP_GENERAL_PURPOSE_BIT     0x0000             // general purpose bit flag
#define ZIP_COMPRESSION_METHOD      0x0000             // store (no compression)
#define ZIP_DISK_NUMBER             0x0000             // disk number start
#define ZIP_INTERNAL_ATTR           0x0000             // internal file attributes
#define ZIP_COMMENT_LENGTH          0x0000             // file comment length
#define ZIP_MAX_PATH                4096               // maximum path length

// crc32 table
static uint32_t crc32_table[256];                      // crc32 lookup table

// initialize crc32 table
static void init_crc32_table(void) {
    static int initialized = 0;                        // initialization flag
    if (initialized) return;                           // already initialized
    
    for (uint32_t i = 0; i < 256; i++) {               // generate table entries
        uint32_t crc = i;                              // current crc value
        for (int j = 0; j < 8; j++) {                  // process 8 bits
            if (crc & 1) {                             // if lsb is set
                crc = (crc >> 1) ^ 0xEDB88320;         // xor with polynomial
            } else {                                   // lsb not set
                crc >>= 1;                             // shift right
            }
        }
        crc32_table[i] = crc;                          // store in table
    }
    initialized = 1;                                   // mark as initialized
}

// calculate crc32
static uint32_t crc32_compute(const uint8_t* data, size_t length) {
    init_crc32_table();                                // ensure table is ready
    uint32_t crc = 0xFFFFFFFF;                         // initial crc value
    
    for (size_t i = 0; i < length; i++) {                        // iterate over data
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);  // update crc
    }
    
    return crc ^ 0xFFFFFFFF;                           // finalize crc
}

// structure for local file header
typedef struct {
    uint32_t signature;                                // local header signature
    uint16_t version_needed;                           // version needed
    uint16_t general_purpose_bit;                      // general purpose bits
    uint16_t compression_method;                       // compression method
    uint16_t last_mod_time;                            // last mod time (dos)
    uint16_t last_mod_date;                            // last mod date (dos)
    uint32_t crc32;                                    // crc32 of data
    uint32_t compressed_size;                          // compressed size
    uint32_t uncompressed_size;                        // uncompressed size
    uint16_t filename_length;                          // filename length
    uint16_t extra_field_length;                       // extra field length
} ZipLocalFileHeader;

// structure for central directory file header
typedef struct {
    uint32_t signature;                                // central header signature
    uint16_t version_made_by;                          // version made by
    uint16_t version_needed;                           // version needed
    uint16_t general_purpose_bit;                      // general purpose bits
    uint16_t compression_method;                       // compression method
    uint16_t last_mod_time;                            // last mod time (dos)
    uint16_t last_mod_date;                            // last mod date (dos)
    uint32_t crc32;                                    // crc32 of data
    uint32_t compressed_size;                          // compressed size
    uint32_t uncompressed_size;                        // uncompressed size
    uint16_t filename_length;                          // filename length
    uint16_t extra_field_length;                       // extra field length
    uint16_t file_comment_length;                      // file comment length
    uint16_t disk_number_start;                        // disk number start
    uint16_t internal_file_attr;                       // internal file attributes
    uint32_t external_file_attr;                       // external file attributes
    uint32_t relative_offset_local_header;             // offset to local header
} ZipCentralFileHeader;

// structure for end of central directory
typedef struct {
    uint32_t signature;                                // end of central dir sig
    uint16_t disk_number;                              // number of this disk
    uint16_t disk_number_cd;                           // disk with central dir
    uint16_t entries_on_disk;                          // entries on this disk
    uint16_t total_entries;                            // total entries
    uint32_t cd_size;                                  // central dir size
    uint32_t cd_offset;                                // central dir offset
    uint16_t comment_length;                           // zip comment length
} ZipEndOfCentralDir;

// structure for file entry info when building zip
typedef struct ZipEntry {
    char* filename;                                    // original filename
    char* archive_name;                                // name in archive
    uint32_t external_attr;                            // external attributes
    uint32_t crc32;                                    // crc32 of data
    uint32_t file_size;                                // file size
    uint16_t dos_time;                                 // dos time
    uint16_t dos_date;                                 // dos date
    long local_header_offset;                          // offset to local header
    struct ZipEntry* next;                             // next entry in list
} ZipEntry;

// write 16-bit little-endian
static void write_le16(uint8_t* buffer, uint16_t value) {
    buffer[0] = value & 0xFF;                          // low byte
    buffer[1] = (value >> 8) & 0xFF;                   // high byte
}

// write 32-bit little-endian
static void write_le32(uint8_t* buffer, uint32_t value) {
    buffer[0] = value & 0xFF;                          // byte 0
    buffer[1] = (value >> 8) & 0xFF;                   // byte 1
    buffer[2] = (value >> 16) & 0xFF;                  // byte 2
    buffer[3] = (value >> 24) & 0xFF;                  // byte 3
}

// read 16-bit little-endian
static uint16_t read_le16(const uint8_t* buffer) {
    return buffer[0] | (buffer[1] << 8);               // combine bytes
}

// read 32-bit little-endian
static uint32_t read_le32(const uint8_t* buffer) {
    return buffer[0] | (buffer[1] << 8) |              // combine bytes
           (buffer[2] << 16) | (buffer[3] << 24);
}

// get filename without extension
static char* get_filename_without_ext(const char* filename) {
    const char* ext = strrchr(filename, '.');          // find last dot
    if (ext) {                                          // has extension
        size_t len = ext - filename;                    // length without ext
        char* result = (char*)malloc(len + 1);          // allocate buffer
        if (result) {                                   // allocation ok
            strncpy(result, filename, len);             // copy filename
            result[len] = '\0';                         // null terminate
        }
        return result;                                  // return result
    }
    return strdup(filename);                            // no extension
}

// get file size
static long get_file_size(FILE* file) {
    fseek(file, 0, SEEK_END);                           // seek to end
    long size = ftell(file);                            // get position
    fseek(file, 0, SEEK_SET);                           // seek back to start
    return size;                                        // return size
}

// read entire file into memory
static uint8_t* read_file(const char* filename, long* size) {
    FILE* file = fopen(filename, "rb");                 // open file
    if (!file) return NULL;                             // open failed
    
    *size = get_file_size(file);                        // get file size
    if (*size <= 0) {                                   // empty file
        fclose(file);                                   // close file
        return NULL;
    }
    
    uint8_t* data = (uint8_t*)malloc(*size);            // allocate buffer
    if (!data) {                                        // allocation failed
        fclose(file);                                   // close file
        return NULL;
    }
    
    size_t read = fread(data, 1, *size, file);          // read file
    fclose(file);                                       // close file
    
    if (read != (size_t)*size) {                        // read error
        free(data);                                     // free buffer
        return NULL;
    }
    
    return data;                                        // return data
}

// write file from memory with permissions
static bool write_file_with_perms(const char* filename, const uint8_t* data, long size, uint32_t external_attr) {
    FILE* file = fopen(filename, "wb");                 // open for writing
    if (!file) return false;                            // open failed
    
    size_t written = fwrite(data, 1, size, file);       // write data
    fclose(file);                                       // close file
    
    if (written != (size_t)size) return false;          // write error
    
#ifdef _WIN32
    (void)external_attr;                                // not used on windows
#else
    mode_t mode = (external_attr >> 16) & 0xFFFF;       // extract mode
    if (mode != 0) {                                    // mode set
        chmod(filename, mode);                          // set permissions
    }
#endif
    return true;                                        // success
}

// create zip filename from input filename
static char* get_zip_filename(const char* input) {
    char* base = get_filename_without_ext(input);       // remove extension
    if (!base) return NULL;                             // allocation failed
    
    char* zipname = (char*)malloc(strlen(base) + 5);    // allocate for .zip
    if (!zipname) {                                     // allocation failed
        free(base);                                     // free base
        return NULL;
    }
    
    sprintf(zipname, "%s.zip", base);                   // build zip name
    free(base);                                         // free base
    return zipname;                                     // return zip name
}

// get file modification time and convert to dos format
static void get_dos_time(const char* filename, uint16_t* dos_date, uint16_t* dos_time) {
#ifdef _WIN32
    struct _stat st;
    if (_stat(filename, &st) == 0) {                    // stat succeeded
#else
    struct stat st;
    if (stat(filename, &st) == 0) {                     // stat succeeded
#endif
        struct tm* tm = localtime(&st.st_mtime);        // convert to local time
        if (tm) {                                       // conversion ok
            *dos_date = ((tm->tm_year + 1900 - 1980) << 9) |  // year (since 1980)
                        ((tm->tm_mon + 1) << 5) |       // month (1-12)
                        tm->tm_mday;                    // day (1-31)
            *dos_time = (tm->tm_hour << 11) |           // hour (0-23)
                        (tm->tm_min << 5) |             // minute (0-59)
                        (tm->tm_sec >> 1);              // second/2 (0-29)
            return;
        }
    }
    *dos_date = 0x4A21;                                 // default date (2024-01-01)
    *dos_time = 0x0000;                                 // default time (00:00:00)
}

// get file permissions
static uint32_t get_file_attrs(const char* filename) {
    (void)filename;                                     // attrs are not read from the path on windows
#ifdef _WIN32
    return 0;                                           // no unix perms on windows
#else
    struct stat st;
    if (stat(filename, &st) == 0) {                     // stat succeeded
        return (st.st_mode << 16) | 0x0000;             // store in high 16 bits
    }
    return (0644 << 16) | 0x0000;                       // default perms
#endif
}

// check if path is a directory
static bool is_directory(const char* path) {
#ifdef _WIN32
    struct _stat st;
    if (_stat(path, &st) == 0) {                        // stat succeeded
        return (st.st_mode & _S_IFDIR) != 0;            // check directory flag
    }
#else
    struct stat st;
    if (stat(path, &st) == 0) {                         // stat succeeded
        return S_ISDIR(st.st_mode);                     // check directory flag
    }
#endif
    return false;                                       // not a directory
}

// create directory recursively
static bool create_directory(const char* path) {
#ifdef _WIN32
    return mkdir(path) == 0 || errno == EEXIST;         // create or exists
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;   // create with perms
#endif
}

// create parent directories for a file
static bool create_parent_dirs(const char* path) {
    char dir[ZIP_MAX_PATH];                             // directory buffer
    strncpy(dir, path, sizeof(dir) - 1);                // copy path
    dir[sizeof(dir) - 1] = '\0';                        // null terminate
    
    char* p = dir;                                      // pointer to char
    while (*p) {                                        // iterate through path
        if (*p == '/' || *p == '\\') {                  // path separator
            *p = '\0';                                  // terminate at separator
            if (strlen(dir) > 0) {                      // not empty
                if (!create_directory(dir)) {           // create directory
                    return false;
                }
            }
            *p = '/';                                   // restore separator
        }
        p++;                                            // next character
    }
    return true;                                        // success
}

// recursively collect all files in a directory
static bool collect_files(const char* base_path, const char* archive_base, ZipEntry** head) {
    DIR* dir = opendir(base_path);                      // open directory
    if (!dir) return false;                             // open failed
    
    struct dirent* entry;                               // directory entry
    char full_path[ZIP_MAX_PATH];                       // full system path
    char archive_path[ZIP_MAX_PATH];                    // path in archive
    
    while ((entry = readdir(dir)) != NULL) {            // iterate entries
        if (strcmp(entry->d_name, ".") == 0 ||          // skip current dir
            strcmp(entry->d_name, "..") == 0) {         // skip parent dir
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);  // build path
        
        if (strlen(archive_base) > 0) {                 // has archive base
            snprintf(archive_path, sizeof(archive_path), "%s/%s", archive_base, entry->d_name);
        } else {                                         // no archive base
            strncpy(archive_path, entry->d_name, sizeof(archive_path) - 1);
            archive_path[sizeof(archive_path) - 1] = '\0';
        }
        
        if (is_directory(full_path)) {                  // is directory
            // Add directory entry
            ZipEntry* dir_entry = (ZipEntry*)calloc(1, sizeof(ZipEntry));  // allocate entry
            if (!dir_entry) {                           // allocation failed
                closedir(dir);                          // close directory
                return false;
            }
            dir_entry->filename = strdup(full_path);                            // copy filename
            dir_entry->archive_name = (char*)malloc(strlen(archive_path) + 2);  // allocate
            sprintf(dir_entry->archive_name, "%s/", archive_path);              // add trailing slash
            dir_entry->external_attr = get_file_attrs(full_path);               // get attributes
            dir_entry->crc32 = 0;                        // no crc for directory
            dir_entry->file_size = 0;                    // no size for directory
            get_dos_time(full_path, &dir_entry->dos_date, &dir_entry->dos_time);  // get time
            dir_entry->local_header_offset = 0;          // no local header
            dir_entry->next = *head;                     // add to list
            *head = dir_entry;                           // update head
            
            // Recursively process subdirectory
            if (!collect_files(full_path, archive_path, head)) {  // recurse
                closedir(dir);                          // close directory
                return false;
            }
        } else {                                        // is file
            // Add file entry
            long file_size;                             // file size
            uint8_t* file_data = read_file(full_path, &file_size);  // read file
            if (!file_data) {                           // read failed
                closedir(dir);                          // close directory
                return false;
            }
            
            ZipEntry* file_entry = (ZipEntry*)calloc(1, sizeof(ZipEntry));  // allocate
            if (!file_entry) {                          // allocation failed
                free(file_data);                        // free data
                closedir(dir);                          // close directory
                return false;
            }
            file_entry->filename = strdup(full_path);                 // copy filename
            file_entry->archive_name = strdup(archive_path);          // copy archive name
            file_entry->external_attr = get_file_attrs(full_path);    // get attributes
            file_entry->crc32 = crc32_compute(file_data, file_size);  // compute crc
            file_entry->file_size = file_size;          // store size
            get_dos_time(full_path, &file_entry->dos_date, &file_entry->dos_time);  // get time
            file_entry->local_header_offset = 0;        // will be set later
            file_entry->next = *head;                   // add to list
            *head = file_entry;                         // update head
            
            free(file_data);                            // free file data
        }
    }
    
    closedir(dir);                                      // close directory
    return true;                                        // success
}

// pack a single file into a zip archive
static bool pack_single_file(const char* input_filename) {
    long file_size;                                              // file size
    uint8_t* file_data = read_file(input_filename, &file_size);  // read file
    if (!file_data) return false;                                // read failed
    
    uint32_t crc32 = crc32_compute(file_data, file_size);     // compute crc
    
    uint32_t external_attr = get_file_attrs(input_filename);  // get attributes
    uint16_t dos_date, dos_time;                              // dos time/date
    get_dos_time(input_filename, &dos_date, &dos_time);       // get time
    
    char* zip_filename = get_zip_filename(input_filename);  // build zip name
    if (!zip_filename) {                                    // allocation failed
        free(file_data);                                    // free data
        return false;
    }
    
    FILE* zip_file = fopen(zip_filename, "wb");             // open zip file
    if (!zip_file) {                                        // open failed
        free(file_data);                                    // free data
        free(zip_filename);                                 // free zip name
        return false;
    }
    
    uint32_t local_header_offset = 0;                            // offset to local header
    
    ZipLocalFileHeader local_header;                             // local header struct
    memset(&local_header, 0, sizeof(local_header));              // zero it
    local_header.signature = ZIP_LOCAL_FILE_HEADER_SIG;          // set signature
    local_header.version_needed = ZIP_VERSION_NEEDED;            // version needed
    local_header.general_purpose_bit = ZIP_GENERAL_PURPOSE_BIT;  // purpose bits
    local_header.compression_method = ZIP_COMPRESSION_METHOD;    // no compression
    local_header.last_mod_time = dos_time;                       // last mod time
    local_header.last_mod_date = dos_date;                       // last mod date
    local_header.crc32 = crc32;                                  // crc32
    local_header.compressed_size = file_size;                    // compressed size
    local_header.uncompressed_size = file_size;                  // uncompressed size
    local_header.filename_length = strlen(input_filename);       // filename length
    local_header.extra_field_length = 0;                         // no extra field
    
    uint8_t header_buffer[30];                                        // header buffer
    write_le32(header_buffer + 0, local_header.signature);            // write signature
    write_le16(header_buffer + 4, local_header.version_needed);       // version needed
    write_le16(header_buffer + 6, local_header.general_purpose_bit);  // purpose bits
    write_le16(header_buffer + 8, local_header.compression_method);   // compression
    write_le16(header_buffer + 10, local_header.last_mod_time);       // time
    write_le16(header_buffer + 12, local_header.last_mod_date);       // date
    write_le32(header_buffer + 14, local_header.crc32);               // crc32
    write_le32(header_buffer + 18, local_header.compressed_size);     // compressed size
    write_le32(header_buffer + 22, local_header.uncompressed_size);   // uncompressed size
    write_le16(header_buffer + 26, local_header.filename_length);     // filename length
    write_le16(header_buffer + 28, local_header.extra_field_length);  // extra length
    
    if (fwrite(header_buffer, 1, 30, zip_file) != 30) {  // write header
        fclose(zip_file);                                // close file
        free(file_data);                                 // free data
        free(zip_filename);                              // free zip name
        return false;
    }
    
    if (fwrite(input_filename, 1, local_header.filename_length, zip_file) != local_header.filename_length) {  // write name
        fclose(zip_file);                                // close file
        free(file_data);                                 // free data
        free(zip_filename);                              // free zip name
        return false;
    }
    
    if (fwrite(file_data, 1, file_size, zip_file) != (size_t)file_size) {  // write data
        fclose(zip_file);                                // close file
        free(file_data);                                 // free data
        free(zip_filename);                              // free zip name
        return false;
    }
    
    ZipCentralFileHeader central_header;                           // central header struct
    memset(&central_header, 0, sizeof(central_header));            // zero it
    central_header.signature = ZIP_CENTRAL_FILE_HEADER_SIG;        // signature
    central_header.version_made_by = ZIP_VERSION_MADE_BY;          // version made by
    central_header.version_needed = ZIP_VERSION_NEEDED;            // version needed
    central_header.general_purpose_bit = ZIP_GENERAL_PURPOSE_BIT;  // purpose bits
    central_header.compression_method = ZIP_COMPRESSION_METHOD;    // no compression
    central_header.last_mod_time = dos_time;                       // time
    central_header.last_mod_date = dos_date;                       // date
    central_header.crc32 = crc32;                                  // crc32
    central_header.compressed_size = file_size;                    // compressed size
    central_header.uncompressed_size = file_size;                  // uncompressed size
    central_header.filename_length = strlen(input_filename);       // filename length
    central_header.extra_field_length = 0;                         // no extra field
    central_header.file_comment_length = 0;                        // no comment
    central_header.disk_number_start = 0;                          // disk number
    central_header.internal_file_attr = ZIP_INTERNAL_ATTR;         // internal attributes
    central_header.external_file_attr = external_attr;             // external attributes
    central_header.relative_offset_local_header = local_header_offset;  // offset
    
    long cd_offset = ftell(zip_file);                              // central dir offset
    
    uint8_t cd_buffer[46];                                           // central header buffer
    write_le32(cd_buffer + 0, central_header.signature);             // signature
    write_le16(cd_buffer + 4, central_header.version_made_by);       // version made by
    write_le16(cd_buffer + 6, central_header.version_needed);        // version needed
    write_le16(cd_buffer + 8, central_header.general_purpose_bit);   // purpose bits
    write_le16(cd_buffer + 10, central_header.compression_method);   // compression
    write_le16(cd_buffer + 12, central_header.last_mod_time);        // time
    write_le16(cd_buffer + 14, central_header.last_mod_date);        // date
    write_le32(cd_buffer + 16, central_header.crc32);                // crc32
    write_le32(cd_buffer + 20, central_header.compressed_size);      // compressed
    write_le32(cd_buffer + 24, central_header.uncompressed_size);    // uncompressed
    write_le16(cd_buffer + 28, central_header.filename_length);      // filename length
    write_le16(cd_buffer + 30, central_header.extra_field_length);   // extra length
    write_le16(cd_buffer + 32, central_header.file_comment_length);  // comment length
    write_le16(cd_buffer + 34, central_header.disk_number_start);    // disk number
    write_le16(cd_buffer + 36, central_header.internal_file_attr);   // internal attr
    write_le32(cd_buffer + 38, central_header.external_file_attr);   // external attr
    write_le32(cd_buffer + 42, central_header.relative_offset_local_header);  // offset
    
    if (fwrite(cd_buffer, 1, 46, zip_file) != 46) {      // write central header
        fclose(zip_file);                                // close file
        free(file_data);                                 // free data
        free(zip_filename);                              // free zip name
        return false;
    }
    
    if (fwrite(input_filename, 1, central_header.filename_length, zip_file) != central_header.filename_length) {  // write name
        fclose(zip_file);                                // close file
        free(file_data);                                 // free data
        free(zip_filename);                              // free zip name
        return false;
    }
    
    ZipEndOfCentralDir end_central;                      // end central struct
    memset(&end_central, 0, sizeof(end_central));        // zero it
    end_central.signature = ZIP_END_OF_CENTRAL_DIR_SIG;  // signature
    end_central.disk_number = ZIP_DISK_NUMBER;           // disk number
    end_central.disk_number_cd = ZIP_DISK_NUMBER;        // disk with central dir
    end_central.entries_on_disk = 1;                     // entries on disk
    end_central.total_entries = 1;                       // total entries
    end_central.cd_size = ftell(zip_file) - cd_offset;   // central dir size
    end_central.cd_offset = cd_offset;                   // central dir offset
    end_central.comment_length = ZIP_COMMENT_LENGTH;     // comment length
    
    uint8_t eocd_buffer[22];                                   // end central buffer
    write_le32(eocd_buffer + 0, end_central.signature);        // signature
    write_le16(eocd_buffer + 4, end_central.disk_number);      // disk number
    write_le16(eocd_buffer + 6, end_central.disk_number_cd);   // disk with cd
    write_le16(eocd_buffer + 8, end_central.entries_on_disk);  // entries on disk
    write_le16(eocd_buffer + 10, end_central.total_entries);   // total entries
    write_le32(eocd_buffer + 12, end_central.cd_size);         // cd size
    write_le32(eocd_buffer + 16, end_central.cd_offset);       // cd offset
    write_le16(eocd_buffer + 20, end_central.comment_length);  // comment length
    
    if (fwrite(eocd_buffer, 1, 22, zip_file) != 22) {   // write end central
        fclose(zip_file);                               // close file
        free(file_data);                                // free data
        free(zip_filename);                             // free zip name
        return false;
    }
    
    fclose(zip_file);                                   // close file
    free(file_data);                                    // free data
    free(zip_filename);                                 // free zip name
    
    return true;                                        // success
}

// pack a directory recursively into a zip archive
static bool pack_directory(const char* input_path) {
    char* zip_filename = get_zip_filename(input_path);  // build zip name
    if (!zip_filename) return false;                    // allocation failed
    
    ZipEntry* entries = NULL;                           // entry list head
    if (!collect_files(input_path, "", &entries)) {     // collect files
        free(zip_filename);                             // free zip name
        return false;
    }
    
    if (!entries) {                                     // no entries
        free(zip_filename);                             // free zip name
        return false;
    }
    
    FILE* zip_file = fopen(zip_filename, "wb");         // open zip file
    if (!zip_file) {                                    // open failed
        free(zip_filename);                             // free zip name
        while (entries) {                               // free entry list
            ZipEntry* next = entries->next;             // get next
            free(entries->filename);                    // free filename
            free(entries->archive_name);                // free archive name
            free(entries);                              // free entry
            entries = next;                             // move to next
        }
        return false;
    }
    
    ZipEntry* entry = entries;                          // start at head
    while (entry) {                                     // iterate entries
        if (entry->file_size > 0) {                     // is a file
            long file_size = entry->file_size;          // file size
            uint8_t* file_data = read_file(entry->filename, &file_size);  // read file
            if (!file_data) {                           // read failed
                fclose(zip_file);                       // close file
                free(zip_filename);                     // free zip name
                return false;
            }
            
            entry->local_header_offset = ftell(zip_file);  // store offset
            
            ZipLocalFileHeader local_header;                             // local header struct
            memset(&local_header, 0, sizeof(local_header));              // zero it
            local_header.signature = ZIP_LOCAL_FILE_HEADER_SIG;          // signature
            local_header.version_needed = ZIP_VERSION_NEEDED;            // version needed
            local_header.general_purpose_bit = ZIP_GENERAL_PURPOSE_BIT;  // purpose bits
            local_header.compression_method = ZIP_COMPRESSION_METHOD;    // no compression
            local_header.last_mod_time = entry->dos_time;                // time
            local_header.last_mod_date = entry->dos_date;                // date
            local_header.crc32 = entry->crc32;                           // crc32
            local_header.compressed_size = file_size;                    // compressed size
            local_header.uncompressed_size = file_size;                  // uncompressed size
            local_header.filename_length = strlen(entry->archive_name);  // filename length
            local_header.extra_field_length = 0;                         // no extra field
            
            uint8_t header_buffer[30];                                        // header buffer
            write_le32(header_buffer + 0, local_header.signature);            // signature
            write_le16(header_buffer + 4, local_header.version_needed);       // version needed
            write_le16(header_buffer + 6, local_header.general_purpose_bit);  // purpose bits
            write_le16(header_buffer + 8, local_header.compression_method);   // compression
            write_le16(header_buffer + 10, local_header.last_mod_time);       // time
            write_le16(header_buffer + 12, local_header.last_mod_date);       // date
            write_le32(header_buffer + 14, local_header.crc32);               // crc32
            write_le32(header_buffer + 18, local_header.compressed_size);     // compressed
            write_le32(header_buffer + 22, local_header.uncompressed_size);   // uncompressed
            write_le16(header_buffer + 26, local_header.filename_length);     // filename length
            write_le16(header_buffer + 28, local_header.extra_field_length);  // extra length
            
            if (fwrite(header_buffer, 1, 30, zip_file) != 30) {  // write header
                free(file_data);                        // free data
                fclose(zip_file);                       // close file
                free(zip_filename);                     // free zip name
                return false;
            }
            
            // Write filename
            if (fwrite(entry->archive_name, 1, local_header.filename_length, zip_file) != local_header.filename_length) {  // write name
                free(file_data);                        // free data
                fclose(zip_file);                       // close file
                free(zip_filename);                     // free zip name
                return false;
            }
            
            // Write file data
            if (fwrite(file_data, 1, file_size, zip_file) != (size_t)file_size) {  // write data
                free(file_data);                        // free data
                fclose(zip_file);                       // close file
                free(zip_filename);                     // free zip name
                return false;
            }
            
            free(file_data);                            // free file data
        }
        entry = entry->next;                            // next entry
    }
    
    long cd_offset = ftell(zip_file);                                  // central dir offset
    entry = entries;                                                   // start at head
    while (entry) {                                                    // iterate entries
        ZipCentralFileHeader central_header;                           // central header struct
        memset(&central_header, 0, sizeof(central_header));            // zero it
        central_header.signature = ZIP_CENTRAL_FILE_HEADER_SIG;        // signature
        central_header.version_made_by = ZIP_VERSION_MADE_BY;          // version made by
        central_header.version_needed = ZIP_VERSION_NEEDED;            // version needed
        central_header.general_purpose_bit = ZIP_GENERAL_PURPOSE_BIT;  // purpose bits
        central_header.compression_method = ZIP_COMPRESSION_METHOD;    // no compression
        central_header.last_mod_time = entry->dos_time;                // time
        central_header.last_mod_date = entry->dos_date;                // date
        central_header.crc32 = entry->crc32;                           // crc32
        central_header.compressed_size = entry->file_size;             // compressed size
        central_header.uncompressed_size = entry->file_size;           // uncompressed size
        central_header.filename_length = strlen(entry->archive_name);  // filename length
        central_header.extra_field_length = 0;                         // no extra field
        central_header.file_comment_length = 0;                        // no comment
        central_header.disk_number_start = 0;                          // disk number
        central_header.internal_file_attr = ZIP_INTERNAL_ATTR;         // internal attributes
        central_header.external_file_attr = entry->external_attr;      // external attributes
        central_header.relative_offset_local_header = entry->local_header_offset;  // offset
        
        uint8_t cd_buffer[46];                                           // central header buffer
        write_le32(cd_buffer + 0, central_header.signature);             // signature
        write_le16(cd_buffer + 4, central_header.version_made_by);       // version made by
        write_le16(cd_buffer + 6, central_header.version_needed);        // version needed
        write_le16(cd_buffer + 8, central_header.general_purpose_bit);   // purpose bits
        write_le16(cd_buffer + 10, central_header.compression_method);   // compression
        write_le16(cd_buffer + 12, central_header.last_mod_time);        // time
        write_le16(cd_buffer + 14, central_header.last_mod_date);        // date
        write_le32(cd_buffer + 16, central_header.crc32);                // crc32
        write_le32(cd_buffer + 20, central_header.compressed_size);      // compressed
        write_le32(cd_buffer + 24, central_header.uncompressed_size);    // uncompressed
        write_le16(cd_buffer + 28, central_header.filename_length);      // filename length
        write_le16(cd_buffer + 30, central_header.extra_field_length);   // extra length
        write_le16(cd_buffer + 32, central_header.file_comment_length);  // comment length
        write_le16(cd_buffer + 34, central_header.disk_number_start);    // disk number
        write_le16(cd_buffer + 36, central_header.internal_file_attr);   // internal attr
        write_le32(cd_buffer + 38, central_header.external_file_attr);   // external attr
        write_le32(cd_buffer + 42, central_header.relative_offset_local_header);  // offset
        
        if (fwrite(cd_buffer, 1, 46, zip_file) != 46) {  // write central header
            fclose(zip_file);                            // close file
            free(zip_filename);                          // free zip name
            return false;
        }
        
        if (fwrite(entry->archive_name, 1, central_header.filename_length, zip_file) != central_header.filename_length) {  // write name
            fclose(zip_file);                           // close file
            free(zip_filename);                         // free zip name
            return false;
        }
        
        entry = entry->next;                            // next entry
    }
    
    long cd_size = ftell(zip_file) - cd_offset;         // central dir size
    int entry_count = 0;                                // entry counter
    entry = entries;                                    // start at head
    while (entry) {                                     // count entries
        entry_count++;                                  // increment count
        entry = entry->next;                            // next entry
    }
    
    ZipEndOfCentralDir end_central;                     // end central struct
    memset(&end_central, 0, sizeof(end_central));       // zero it
    end_central.signature = ZIP_END_OF_CENTRAL_DIR_SIG;  // signature
    end_central.disk_number = ZIP_DISK_NUMBER;          // disk number
    end_central.disk_number_cd = ZIP_DISK_NUMBER;       // disk with central dir
    end_central.entries_on_disk = entry_count;          // entries on disk
    end_central.total_entries = entry_count;            // total entries
    end_central.cd_size = cd_size;                      // central dir size
    end_central.cd_offset = cd_offset;                  // central dir offset
    end_central.comment_length = ZIP_COMMENT_LENGTH;    // comment length
    
    uint8_t eocd_buffer[22];                            // end central buffer
    write_le32(eocd_buffer + 0, end_central.signature);  // signature
    write_le16(eocd_buffer + 4, end_central.disk_number);      // disk number
    write_le16(eocd_buffer + 6, end_central.disk_number_cd);   // disk with cd
    write_le16(eocd_buffer + 8, end_central.entries_on_disk);  // entries on disk
    write_le16(eocd_buffer + 10, end_central.total_entries);   // total entries
    write_le32(eocd_buffer + 12, end_central.cd_size);         // cd size
    write_le32(eocd_buffer + 16, end_central.cd_offset);       // cd offset
    write_le16(eocd_buffer + 20, end_central.comment_length);  // comment length
    
    if (fwrite(eocd_buffer, 1, 22, zip_file) != 22) {   // write end central
        fclose(zip_file);                               // close file
        free(zip_filename);                             // free zip name
        return false;
    }
    
    fclose(zip_file);                                   // close file
    free(zip_filename);                                 // free zip name
    
    while (entries) {                                   // free entry list
        ZipEntry* next = entries->next;                 // get next
        free(entries->filename);                        // free filename
        free(entries->archive_name);                    // free archive name
        free(entries);                                  // free entry
        entries = next;                                 // move to next
    }
    
    return true;                                        // success
}

// pack a file or directory into a zip archive
static bool pack_file_or_dir(const char* input_path) {
    if (!is_directory(input_path)) {                    // is file
        return pack_single_file(input_path);            // pack single file
    } else {                                            // is directory
        return pack_directory(input_path);              // pack directory
    }
}

// unpack a zip archive
static bool unpack_file(const char* zip_filename) {
    long zip_size;                                           // zip file size
    uint8_t* zip_data = read_file(zip_filename, &zip_size);  // read zip
    if (!zip_data) return false;                             // read failed
    
    bool found_eocd = false;                            // eocd found flag
    ZipEndOfCentralDir eocd;                            // eocd struct
    size_t offset = zip_size - 22;                      // start search at end
    
    while (offset > 0 && offset >= 22) {                // search backward
        if (read_le32(zip_data + offset) == ZIP_END_OF_CENTRAL_DIR_SIG) {  // found signature
            found_eocd = true;                          // mark found
            break;                                      // exit loop
        }
        offset--;                                       // move backward
    }
    
    if (!found_eocd) {                                  // not found
        free(zip_data);                                 // free data
        return false;
    }
    
    eocd.signature = read_le32(zip_data + offset);            // read signature
    eocd.disk_number = read_le16(zip_data + offset + 4);      // disk number
    eocd.disk_number_cd = read_le16(zip_data + offset + 6);   // disk with cd
    eocd.entries_on_disk = read_le16(zip_data + offset + 8);  // entries on disk
    eocd.total_entries = read_le16(zip_data + offset + 10);   // total entries
    eocd.cd_size = read_le32(zip_data + offset + 12);         // cd size
    eocd.cd_offset = read_le32(zip_data + offset + 16);       // cd offset
    eocd.comment_length = read_le16(zip_data + offset + 20);  // comment length
    
    if (eocd.total_entries == 0) {                      // no entries
        free(zip_data);                                 // free data
        return false;
    }
    
    if (eocd.cd_offset > (size_t)zip_size) {          // cd offset invalid
        free(zip_data);                               // free data
        return false;
    }
    
    size_t cd_pos = eocd.cd_offset;                   // central dir position
    for (int i = 0; i < eocd.total_entries; i++) {    // iterate entries
        if (cd_pos + 46 > (size_t)zip_size) {         // out of bounds
            free(zip_data);                           // free data
            return false;
        }
        
        uint32_t sig = read_le32(zip_data + cd_pos);  // read signature
        if (sig != ZIP_CENTRAL_FILE_HEADER_SIG) {     // invalid signature
            free(zip_data);                           // free data
            return false;
        }
        
        ZipCentralFileHeader cd_header;                                     // central header
        cd_header.signature = sig;                                          // signature
        cd_header.version_made_by = read_le16(zip_data + cd_pos + 4);       // version made by
        cd_header.version_needed = read_le16(zip_data + cd_pos + 6);        // version needed
        cd_header.general_purpose_bit = read_le16(zip_data + cd_pos + 8);   // purpose bits
        cd_header.compression_method = read_le16(zip_data + cd_pos + 10);   // compression
        cd_header.last_mod_time = read_le16(zip_data + cd_pos + 12);        // time
        cd_header.last_mod_date = read_le16(zip_data + cd_pos + 14);        // date
        cd_header.crc32 = read_le32(zip_data + cd_pos + 16);                // crc32
        cd_header.compressed_size = read_le32(zip_data + cd_pos + 20);      // compressed
        cd_header.uncompressed_size = read_le32(zip_data + cd_pos + 24);    // uncompressed
        cd_header.filename_length = read_le16(zip_data + cd_pos + 28);      // filename length
        cd_header.extra_field_length = read_le16(zip_data + cd_pos + 30);   // extra length
        cd_header.file_comment_length = read_le16(zip_data + cd_pos + 32);  // comment length
        cd_header.disk_number_start = read_le16(zip_data + cd_pos + 34);    // disk number
        cd_header.internal_file_attr = read_le16(zip_data + cd_pos + 36);   // internal attr
        cd_header.external_file_attr = read_le32(zip_data + cd_pos + 38);   // external attr
        cd_header.relative_offset_local_header = read_le32(zip_data + cd_pos + 42);  // offset
        
        if (cd_pos + 46 + cd_header.filename_length > (size_t)zip_size) {  // out of bounds
            free(zip_data);                             // free data
            return false;
        }
        
        char* filename = (char*)malloc(cd_header.filename_length + 1);  // allocate filename
        if (!filename) {                                // allocation failed
            free(zip_data);                             // free data
            return false;
        }
        memcpy(filename, zip_data + cd_pos + 46, cd_header.filename_length);  // copy name
        filename[cd_header.filename_length] = '\0';     // null terminate
        
        bool is_dir = (cd_header.filename_length > 0 && filename[cd_header.filename_length - 1] == '/');  // check trailing slash
        
        if (is_dir) {                                   // is directory
            filename[cd_header.filename_length - 1] = '\0';  // remove slash
            
            if (!create_directory(filename)) {          // create directory
                free(filename);                         // free filename
                free(zip_data);                         // free data
                return false;
            }
            
#ifdef _WIN32
            // windows doesn't support unix permissions
#else
            mode_t mode = (cd_header.external_file_attr >> 16) & 0xFFFF;  // extract mode
            if (mode != 0) {                             // mode set
                chmod(filename, mode);                   // set permissions
            }
#endif
            free(filename);                              // free filename
        } else {                                         // is file
            // File entry
            if (cd_header.compression_method != 0) {     // compressed
                free(filename);                          // free filename
                free(zip_data);                          // free data
                return false;
            }
            
            size_t local_pos = cd_header.relative_offset_local_header;  // local header pos
            if (local_pos + 30 > (size_t)zip_size) {     // out of bounds
                free(filename);                          // free filename
                free(zip_data);                          // free data
                return false;
            }
            
            uint32_t local_sig = read_le32(zip_data + local_pos);  // local signature
            if (local_sig != ZIP_LOCAL_FILE_HEADER_SIG) {  // invalid
                free(filename);                            // free filename
                free(zip_data);                            // free data
                return false;
            }
            
            uint16_t local_filename_len = read_le16(zip_data + local_pos + 26);  // filename length
            uint16_t local_extra_len = read_le16(zip_data + local_pos + 28);     // extra length
            
            size_t data_pos = local_pos + 30 + local_filename_len + local_extra_len;  // data position
            if (data_pos + cd_header.compressed_size > (size_t)zip_size) {            // out of bounds
                free(filename);                         // free filename
                free(zip_data);                         // free data
                return false;
            }
            
            uint32_t computed_crc = crc32_compute(zip_data + data_pos, cd_header.compressed_size);  // compute crc
            if (computed_crc != cd_header.crc32) {      // crc mismatch
                free(filename);                         // free filename
                free(zip_data);                         // free data
                return false;
            }
            
            if (!create_parent_dirs(filename)) {        // create parent dirs
                free(filename);                         // free filename
                free(zip_data);                         // free data
                return false;
            }
            
            if (!write_file_with_perms(filename, zip_data + data_pos, 
                                       cd_header.compressed_size, 
                                       cd_header.external_file_attr)) {  // write file
                free(filename);                          // free filename
                free(zip_data);                          // free data
                return false;
            }
            
            free(filename);                              // free filename
        }
        
        cd_pos += 46 + cd_header.filename_length + cd_header.extra_field_length + cd_header.file_comment_length;  // next entry
    }
    
    free(zip_data);                                     // free data
    return true;                                        // success
}

// main dispatcher for zip module built-in functions
bool zip_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;                                           // suppress unused warning

    if (strcmp(name, "zip.pack") == 0) {                // pack function
        if (arg_count != 1 || !IS_STRING(args[0])) {    // validate args
            *result = MAKE_NONE();                      // return none
            return true;                                // builtin handled
        }
        
        const char* path = AS_STRING(args[0])->chars;   // get path
        bool success = pack_file_or_dir(path);          // pack file or dir
        
        *result = success ? MAKE_BOOL(true) : MAKE_NONE();  // return result
        return true;                                        // builtin handled
    }
    
    if (strcmp(name, "zip.unpack") == 0) {              // unpack function
        if (arg_count != 1 || !IS_STRING(args[0])) {    // validate args
            *result = MAKE_NONE();                      // return none
            return true;                                // builtin handled
        }
        
        const char* filename = AS_STRING(args[0])->chars;  // get filename
        bool success = unpack_file(filename);              // unpack file
        
        *result = success ? MAKE_BOOL(true) : MAKE_NONE();  // return result
        return true;                                        // builtin handled
    }
    
    return false;                                           // not a recognized builtin
}