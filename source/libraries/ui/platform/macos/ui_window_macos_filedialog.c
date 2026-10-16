// source/libraries/ui/platform/macos/ui_window_macos_filedialog.c
// NSOpenPanel file / folder picker for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifdef __APPLE__

#include "ui_window_macos.h"
#include <stdlib.h>
#include <string.h>

#define OBJC_CLS(n) ((id)objc_getClass(n))
#define OBJC_SEL(n) sel_registerName(n)

#define MSG_id0(r, s)       ((id      (*)(id, SEL))objc_msgSend)((id)(r), (SEL)(s))
#define MSG_id1(r, s, a)    ((id      (*)(id, SEL, id))objc_msgSend)((id)(r), (SEL)(s), (id)(a))
#define MSG_int0(r, s)      ((NSInteger (*)(id, SEL))objc_msgSend)((id)(r), (SEL)(s))
#define MSG_void0(r, s)     ((void    (*)(id, SEL))objc_msgSend)((id)(r), (SEL)(s))
#define MSG_void1(r, s, a)  ((void    (*)(id, SEL, id))objc_msgSend)((id)(r), (SEL)(s), (id)(a))
#define MSG_void_b(r, s, a) ((void    (*)(id, SEL, BOOL))objc_msgSend)((id)(r), (SEL)(s), (BOOL)(a))

// reads the picked url out of a panel and turns it into a fresh utf-8 path
static char* panel_result_path(id panel) {
    id url = MSG_id0(panel, OBJC_SEL("URL"));             // the panel's result url
    if (!url) return NULL;                                // no url: user cancelled
    char buf[4096];                                       // filesystem path buffer
    Boolean ok = CFURLGetFileSystemRepresentation((CFURLRef)url, true,
                                                  (UInt8*)buf, sizeof(buf));
    if (!ok) return NULL;                                 // conversion failed
    return strdup(buf);                                   // caller owns the utf-8 copy
}

// builds an NSArray of extension strings from every filter entry
static id make_allowed_types_from_filters(const UiFileFilter* filters) {
    if (!filters || !filters[0].name) return NULL;        // no filters: nothing to build
    id array = MSG_id0(OBJC_CLS("NSMutableArray"), OBJC_SEL("array"));
    if (!array) return NULL;                              // oom: bail out

    for (int i = 0; filters[i].name; i++) {               // one filter entry per iteration
        const char* p = filters[i].pattern;               // semicolon-separated glob list
        if (!p) continue;                                 // no pattern: skip this entry
        while (*p) {                                      // walk each glob
            const char* end = strchr(p, ';');             // find the next separator
            int len = end ? (int)(end - p) : (int)strlen(p);
            if (len > 2 && p[0] == '*' && p[1] == '.') {  // only "*.ext" globs are usable
                char ext[64];                             // bare extension, no "*."
                int n = len - 2;                          // length after stripping "*."
                if (n > (int)sizeof(ext) - 1) n = (int)sizeof(ext) - 1;
                memcpy(ext, p + 2, (size_t)n);            // copy the extension body
                ext[n] = 0;                               // null terminate
                id s = macos_nsstring_from_utf8(ext);     // wrap as nsstring
                if (s) {                                  // wrap succeeded
                    MSG_void1(array, OBJC_SEL("addObject:"), s);  // append to the array
                    MSG_void0(s, OBJC_SEL("release"));            // release our ref
                }
            }
            p = end ? end + 1 : p + len;                  // advance past this glob
        }
    }
    return array;                                         // caller releases the array
}

// points a panel at the caller's default path, if any
static void apply_default_path(id panel, const char* default_path) {
    if (!default_path || !*default_path) return;          // nothing to apply
    id s = macos_nsstring_from_utf8(default_path);        // wrap the path
    if (!s) return;                                       // oom: bail out
    id url = MSG_id1(OBJC_CLS("NSURL"), OBJC_SEL("fileURLWithPath:"), s);
    if (url) MSG_void1(panel, OBJC_SEL("setDirectoryURL:"), url);  // point the panel at it
    MSG_void0(s, OBJC_SEL("release"));                    // release the path string
}

// applies an optional title to a panel
static void apply_title(id panel, const char* title) {
    if (!title || !*title) return;                        // nothing to apply
    id t = macos_nsstring_from_utf8(title);               // wrap the title
    if (!t) return;                                       // oom: bail out
    MSG_void1(panel, OBJC_SEL("setTitle:"), t);           // publish the title
    MSG_void0(t, OBJC_SEL("release"));                    // release our ref
}

// shows the native "open file" panel and returns the picked utf-8 path
char* ui_window_open_file(UiWindow* w, const char* title,
                          const char* default_path,
                          const UiFileFilter* filters) {
    (void)w;                                              // panel is not window-modal on this backend
    id panel = MSG_id0(OBJC_CLS("NSOpenPanel"), OBJC_SEL("openPanel"));
    if (!panel) return NULL;                              // no shared panel: bail out

    MSG_void_b(panel, OBJC_SEL("setCanChooseFiles:"), YES);           // files are selectable
    MSG_void_b(panel, OBJC_SEL("setCanChooseDirectories:"), NO);      // directories are not
    MSG_void_b(panel, OBJC_SEL("setAllowsMultipleSelection:"), NO);   // single selection only
    MSG_void_b(panel, OBJC_SEL("setResolvesAliases:"), YES);          // follow aliases/symlinks

    apply_title(panel, title);                            // optional dialog caption
    apply_default_path(panel, default_path);              // optional starting directory

    id types = make_allowed_types_from_filters(filters);  // optional extension filter
    if (types) MSG_void1(panel, OBJC_SEL("setAllowedFileTypes:"), types);

    NSInteger resp = MSG_int0(panel, OBJC_SEL("runModal"));  // run the modal picker
    if (resp != 1) return NULL;                           // NSModalResponseOK == 1
    return panel_result_path(panel);                      // extract the picked path
}

// shows the native "open folder" panel and returns the picked utf-8 path
char* ui_window_open_folder(UiWindow* w, const char* title,
                            const char* default_path) {
    (void)w;                                              // panel is not window-modal on this backend
    id panel = MSG_id0(OBJC_CLS("NSOpenPanel"), OBJC_SEL("openPanel"));
    if (!panel) return NULL;                              // no shared panel: bail out

    MSG_void_b(panel, OBJC_SEL("setCanChooseFiles:"), NO);            // files are not selectable
    MSG_void_b(panel, OBJC_SEL("setCanChooseDirectories:"), YES);     // directories are
    MSG_void_b(panel, OBJC_SEL("setAllowsMultipleSelection:"), NO);   // single selection only
    MSG_void_b(panel, OBJC_SEL("setResolvesAliases:"), YES);          // follow aliases/symlinks

    apply_title(panel, title);                            // optional dialog caption
    apply_default_path(panel, default_path);              // optional starting directory

    NSInteger resp = MSG_int0(panel, OBJC_SEL("runModal"));  // run the modal picker
    if (resp != 1) return NULL;                           // NSModalResponseOK == 1
    return panel_result_path(panel);                      // extract the picked path
}

#endif