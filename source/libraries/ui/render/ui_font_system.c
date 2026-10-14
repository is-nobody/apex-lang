// source/libraries/ui/render/ui_font_system.c
// Implementation of system font discovery for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_text.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __linux__
#  include <fontconfig/fontconfig.h>
#elif defined(_WIN32)
#  include <windows.h>
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

// resolves the path of the system segoe ui font via the windows fonts directory
static char* system_font_path_windows(void) {
    // segoe ui is the default ui font on vista and later
    static const char* const candidates[] = {
        "segoeui.ttf",       // vista, 7, 8, 10, 11 default ui font
        "tahoma.ttf",        // 2000 / xp default ui font
        "arial.ttf",         // universal fallback on every install
        "verdana.ttf",       // rarely missing
    };

    char windir[MAX_PATH];                                   // %windir% buffer
    UINT n = GetWindowsDirectoryA(windir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return NULL;                // no windows directory: bail

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        char path[MAX_PATH];                                 // candidate path buffer
        int written = snprintf(path, MAX_PATH, "%s\\Fonts\\%s", windir, candidates[i]);
        if (written <= 0 || written >= MAX_PATH) continue;    // overflow: skip

        DWORD attrs = GetFileAttributesA(path);
        if (attrs == INVALID_FILE_ATTRIBUTES) continue;       // missing: try next
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) continue;       // directory: skip

        return strdup(path);                                 // found: hand back the path
    }
    return NULL;                                             // nothing matched
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
    path = system_font_path_windows();                       // windows fonts directory lookup
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