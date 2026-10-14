// source/libraries/ui/platform/macos/ui_window_macos_clipboard.c
// NSPasteboard clipboard access for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifdef __APPLE__

#include "ui_window_macos.h"
#include <stdlib.h>
#include <string.h>

// lookup a class by name
#define OBJC_CLS(n) ((id)objc_getClass(n))

// intern a selector
#define OBJC_SEL(n) sel_registerName(n)

// stores a utf-8 string and claims ownership of the pasteboard
void ui_window_clipboard_set(UiWindow* w, const char* text, int len) {
    (void)w;                                                 // pasteboard is per-process, not per-window
    if (!text) text = "";                                    // null guard
    if (len < 0) len = (int)strlen(text);                    // auto-length when negative

    id pb = ((id (*)(id, SEL))objc_msgSend)(OBJC_CLS("NSPasteboard"),
                                            OBJC_SEL("generalPasteboard"));
    if (!pb) return;                                         // no pasteboard: bail

    ((NSInteger (*)(id, SEL))objc_msgSend)(pb, OBJC_SEL("clearContents"));  // drop previous contents

    // build an nsstring from a possibly non-null-terminated range
    id alloc = ((id (*)(id, SEL))objc_msgSend)(OBJC_CLS("NSString"),
                                               OBJC_SEL("alloc"));
    id str = ((id (*)(id, SEL, const char*, NSInteger, NSInteger))objc_msgSend)(
        alloc, OBJC_SEL("initWithBytes:length:encoding:"),
        text, (NSInteger)len, (NSInteger)4 /* nsutf8stringencoding */);
    if (!str) return;                                        // utf-8 decode failed

    // nspasteboardtype string = "public.utf8-plain-text"
    id type = ((id (*)(id, SEL, const char*))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)(OBJC_CLS("NSString"),
                                        OBJC_SEL("alloc")),
        OBJC_SEL("initWithUTF8String:"),
        "public.utf8-plain-text");

    ((BOOL (*)(id, SEL, id, id))objc_msgSend)(
        pb, OBJC_SEL("setString:forType:"), str, type);      // publish the string

    ((void (*)(id, SEL))objc_msgSend)(type, OBJC_SEL("release"));  // release the type string
    ((void (*)(id, SEL))objc_msgSend)(str,  OBJC_SEL("release"));  // release the content string
}

// fetches the pasteboard contents as utf-8
bool ui_window_clipboard_get(UiWindow* w, char** out_text, int* out_len) {
    (void)w;                                                 // pasteboard is per-process, not per-window
    if (out_text) *out_text = NULL;                          // clear the outputs
    if (out_len)  *out_len = 0;

    id pb = ((id (*)(id, SEL))objc_msgSend)(OBJC_CLS("NSPasteboard"),
                                            OBJC_SEL("generalPasteboard"));
    if (!pb) return false;                                   // no pasteboard: bail

    id type = ((id (*)(id, SEL, const char*))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)(OBJC_CLS("NSString"),
                                        OBJC_SEL("alloc")),
        OBJC_SEL("initWithUTF8String:"),
        "public.utf8-plain-text");                           // nspasteboardtype string

    id str = ((id (*)(id, SEL, id))objc_msgSend)(
        pb, OBJC_SEL("stringForType:"), type);               // fetch the text
    ((void (*)(id, SEL))objc_msgSend)(type, OBJC_SEL("release"));
    if (!str) return false;                                  // nothing to read

    const char* c = ((const char* (*)(id, SEL))objc_msgSend)(
        str, OBJC_SEL("UTF8String"));                        // borrowed utf-8 buffer
    if (!c) return false;                                    // conversion failed

    size_t n = strlen(c);
    char* buf = (char*)malloc(n + 1);                        // caller owns the copy
    if (!buf) return false;
    memcpy(buf, c, n + 1);

    if (out_text) *out_text = buf; else free(buf);           // hand off or release
    if (out_len)  *out_len  = (int)n;                        // publish the length
    return true;
}

#endif