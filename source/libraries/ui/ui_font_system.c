// source/libraries/ui/ui_font_system.c
// Implementation of system font discovery for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_internal.h"
#include "ui_text.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __linux__
#  include <fontconfig/fontconfig.h>
#elif defined(_WIN32)
#  include <initguid.h>
#  include <windows.h>
#  include <dwrite.h>
#  include <shlwapi.h>
#elif defined(__APPLE__)
#  include <CoreText/CoreText.h>
#  include <CoreFoundation/CoreFoundation.h>
#endif

// reads an entire file into a freshly allocated buffer, returns null on any failure
static uint8_t* read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");                 // open in binary mode so no newline translation
    if (!f) return NULL;                         // file missing or unreadable
    fseek(f, 0, SEEK_END);                       // seek to end to measure size
    long sz = ftell(f);                          // size in bytes
    fseek(f, 0, SEEK_SET);                       // rewind to start
    if (sz <= 0) { fclose(f); return NULL; }     // empty or unseekable: reject
    uint8_t* buf = (uint8_t*)malloc((size_t)sz); // allocate exact-size buffer
    if (!buf) { fclose(f); return NULL; }        // allocation failed
    size_t got = fread(buf, 1, (size_t)sz, f);   // read the whole file
    fclose(f);                                   // close regardless of read result
    if (got != (size_t)sz) { free(buf); return NULL; }  // short read: discard partial data
    *out_len = (size_t)sz;                       // report the byte count
    return buf;                                  // caller owns the buffer
}

#ifdef __linux__

// resolves the path of the system sans-serif font via fontconfig
static char* system_font_path_linux(void) {
    if (!FcInit()) return NULL;                  // fontconfig init failed

    FcPattern* pat = FcPatternBuild(NULL,          // build a query pattern
        FC_FAMILY, FcTypeString, "sans-serif",     // ask for the generic sans-serif family
        NULL);
    if (!pat) { FcFini(); return NULL; }         // pattern allocation failed

    FcConfigSubstitute(NULL, pat, FcMatchPattern);  // apply user config substitutions
    FcDefaultSubstitute(pat);                       // fill in defaults for unspecified fields

    FcResult result;
    FcPattern* match = FcFontMatch(NULL, pat, &result);  // find best match
    FcPatternDestroy(pat);                           // query pattern no longer needed
    if (!match) { FcFini(); return NULL; }           // no match found

    FcChar8* file = NULL;
    if (FcPatternGetString(match, FC_FILE, 0, &file) != FcResultMatch) {  // extract file path
        FcPatternDestroy(match);
        FcFini();
        return NULL;
    }

    char* out = strdup((const char*)file);       // copy before destroying the match

    FcPatternDestroy(match);                     // release match pattern
    FcFini();                                    // release fontconfig globals

    return out;                                  // caller owns the path
}

#elif defined(_WIN32)

// walks a directwrite font file object to obtain its utf-8 filesystem path
static char* dwrite_file_to_path(IDWriteFontFile* file) {
    IDWriteFontFileLoader* loader = NULL;
    if (FAILED(file->lpVtbl->GetLoader(file, &loader))) return NULL;  // loader lookup failed
    if (!loader) return NULL;                                         // no loader attached

    IDWriteLocalFontFileLoader* local = NULL;                         // try to specialise the loader
    HRESULT hr = loader->lpVtbl->QueryInterface(loader, &IID_IDWriteLocalFontFileLoader, (void**)&local);
    loader->lpVtbl->Release(loader);                                  // interface acquired, drop base
    if (FAILED(hr) || !local) return NULL;                            // not a local file loader

    const void* key = NULL;                                           // opaque reference key
    UINT32 key_size = 0;
    if (FAILED(file->lpVtbl->GetReferenceKey(file, &key, &key_size))) {  // fetch key
        local->lpVtbl->Release(local);
        return NULL;
    }

    UINT32 path_len = 0;                                              // length of the path in wchars
    if (FAILED(local->lpVtbl->GetFilePathLengthFromKey(local, key, key_size, &path_len))) {
        local->lpVtbl->Release(local);
        return NULL;
    }

    wchar_t* wpath = (wchar_t*)malloc(sizeof(wchar_t) * (path_len + 1));  // wide path buffer
    if (!wpath) { local->lpVtbl->Release(local); return NULL; }           // allocation failed

    if (FAILED(local->lpVtbl->GetFilePathFromKey(local, key, key_size, wpath, path_len + 1))) {
        free(wpath);                                                  // path fetch failed
        local->lpVtbl->Release(local);
        return NULL;
    }
    local->lpVtbl->Release(local);                                    // done with the loader

    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, NULL, 0, NULL, NULL);  // measure utf-8
    char* utf8 = (char*)malloc((size_t)utf8_len);                     // allocate utf-8 buffer
    if (!utf8) { free(wpath); return NULL; }                          // allocation failed
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, utf8, utf8_len, NULL, NULL);  // convert
    free(wpath);                                                      // release wide buffer
    return utf8;                                                      // caller owns the utf-8 path
}

// resolves the path of the system segoe ui font via directwrite
static char* system_font_path_windows(void) {
    IDWriteFactory* factory = NULL;                                   // directwrite factory
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                     &IID_IDWriteFactory,
                                     (IUnknown**)&factory);
    if (FAILED(hr) || !factory) return NULL;                          // factory creation failed

    IDWriteFontCollection* collection = NULL;                         // system font collection
    hr = factory->lpVtbl->GetSystemFontCollection(factory, &collection, FALSE);
    if (FAILED(hr) || !collection) {
        factory->lpVtbl->Release(factory);
        return NULL;
    }

    UINT32 index = 0;                                                 // family index for segoe ui
    BOOL exists = FALSE;
    hr = collection->lpVtbl->FindFamilyName(collection, L"Segoe UI", &index, &exists);
    if (FAILED(hr) || !exists) {                                      // family not present
        collection->lpVtbl->Release(collection);
        factory->lpVtbl->Release(factory);
        return NULL;
    }

    IDWriteFontFamily* family = NULL;                                 // family object
    hr = collection->lpVtbl->GetFontFamily(collection, index, &family);
    if (FAILED(hr) || !family) {
        collection->lpVtbl->Release(collection);
        factory->lpVtbl->Release(factory);
        return NULL;
    }

    IDWriteFont* font = NULL;                                         // regular weight style
    hr = family->lpVtbl->GetFirstMatchingFont(family,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
    if (FAILED(hr) || !font) {
        family->lpVtbl->Release(family);
        collection->lpVtbl->Release(collection);
        factory->lpVtbl->Release(factory);
        return NULL;
    }

    IDWriteFontFace* face = NULL;                                     // concrete font face
    hr = font->lpVtbl->CreateFontFace(font, &face);
    if (FAILED(hr) || !face) {
        font->lpVtbl->Release(font);
        family->lpVtbl->Release(family);
        collection->lpVtbl->Release(collection);
        factory->lpVtbl->Release(factory);
        return NULL;
    }

    UINT32 file_count = 0;                                            // first probe: just count files
    hr = face->lpVtbl->GetFiles(face, &file_count, NULL);
    if (FAILED(hr) || file_count == 0) {                              // no backing files
        face->lpVtbl->Release(face);
        font->lpVtbl->Release(font);
        family->lpVtbl->Release(family);
        collection->lpVtbl->Release(collection);
        factory->lpVtbl->Release(factory);
        return NULL;
    }

    IDWriteFontFile* file = NULL;                                     // second probe: fetch first file
    hr = face->lpVtbl->GetFiles(face, &file_count, &file);
    if (FAILED(hr) || !file) {
        face->lpVtbl->Release(face);
        font->lpVtbl->Release(font);
        family->lpVtbl->Release(family);
        collection->lpVtbl->Release(collection);
        factory->lpVtbl->Release(factory);
        return NULL;
    }

    char* path = dwrite_file_to_path(file);                           // convert file object to path
    file->lpVtbl->Release(file);                                      // release file
    face->lpVtbl->Release(face);                                      // release face
    font->lpVtbl->Release(font);                                      // release font
    family->lpVtbl->Release(family);                                  // release family
    collection->lpVtbl->Release(collection);                          // release collection
    factory->lpVtbl->Release(factory);                                // release factory
    return path;                                                      // caller owns the path
}

#elif defined(__APPLE__)

// resolves the path of the system ui font via coretext
static char* system_font_path_macos(void) {
    CTFontRef font = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 13.0, NULL);  // system ui font
    if (!font) return NULL;                                  // font lookup failed

    CFURLRef url = (CFURLRef)CTFontCopyAttribute(font, kCTFontURLAttribute);  // file url attribute
    CFRelease(font);                                         // font object no longer needed
    if (!url) return NULL;                                   // attribute missing

    char path[4096];                                         // filesystem path buffer
    Boolean ok = CFURLGetFileSystemRepresentation(url, true, (UInt8*)path, sizeof(path));
    CFRelease(url);                                          // release the url
    if (!ok) return NULL;                                    // conversion failed

    return strdup(path);                                     // caller owns the path
}

#endif

// platform-dispatch: locate a system font, read it, and decode it into a UiFont
UiFont* ui_font_load_system(void) {
    char* path = NULL;                                       // resolved font file path

#if defined(__linux__)
    path = system_font_path_linux();                         // fontconfig lookup
#elif defined(_WIN32)
    path = system_font_path_windows();                       // directwrite lookup
#elif defined(__APPLE__)
    path = system_font_path_macos();                         // coretext lookup
#endif

    if (!path) {
        fprintf(stderr, "[ui] system font not found\n");     // no platform could resolve a font
        return NULL;
    }

    size_t len = 0;                                          // file size in bytes
    uint8_t* data = read_file(path, &len);                   // slurp the font file
    if (!data) {
        fprintf(stderr, "[ui] cannot read system font: %s\n", path);  // read failed
        free(path);
        return NULL;
    }

    UiFont* f = ui_font_load_memory(data, len);              // hand bytes to the ttf decoder
    free(data);                                              // decoder has copied what it needs
    if (!f) {
        fprintf(stderr, "[ui] system font at %s is not a valid TTF\n", path);  // decode failed
    }
    free(path);                                              // release the path in all cases
    return f;                                                // null on any failure, else the font
}