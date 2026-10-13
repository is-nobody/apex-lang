// source/libraries/ui/platform/windows/ui_window_windows_clipboard.c
// Win32 clipboard access for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifdef _WIN32

#include "ui_window_windows.h"
#include <stdlib.h>

// stores a utf-8 string and claims ownership of the win32 clipboard
void ui_window_clipboard_set(UiWindow* w, const char* text, int len) {
    (void)w;                                             // window not required by the api
    if (!OpenClipboard(NULL)) return;                    // clipboard busy
    EmptyClipboard();                                    // drop previous contents
    // convert utf-8 to utf-16 for CF_UNICODETEXT
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text ? text : "", len, NULL, 0);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)(wlen + 1) * sizeof(WCHAR));
    if (h) {
        WCHAR* dst = (WCHAR*)GlobalLock(h);
        MultiByteToWideChar(CP_UTF8, 0, text ? text : "", len, dst, wlen);
        dst[wlen] = 0;
        GlobalUnlock(h);
        SetClipboardData(CF_UNICODETEXT, h);             // system owns the handle now
    }
    CloseClipboard();
}

// fetches the win32 clipboard contents as utf-8
bool ui_window_clipboard_get(UiWindow* w, char** out_text, int* out_len) {
    (void)w;                                             // window not required by the api
    if (out_text) *out_text = NULL;
    if (out_len)  *out_len = 0;
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return false;
    if (!OpenClipboard(NULL)) return false;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (!h) { CloseClipboard(); return false; }
    const WCHAR* src = (const WCHAR*)GlobalLock(h);
    if (!src) { CloseClipboard(); return false; }
    int bytes = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
    if (bytes <= 0) { GlobalUnlock(h); CloseClipboard(); return false; }
    char* buf = (char*)malloc((size_t)bytes);
    WideCharToMultiByte(CP_UTF8, 0, src, -1, buf, bytes, NULL, NULL);
    GlobalUnlock(h);
    CloseClipboard();
    int n = bytes - 1;                                   // drop the terminating null
    if (out_text) *out_text = buf; else free(buf);
    if (out_len)  *out_len = n;
    return true;
}

#endif