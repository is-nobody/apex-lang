// source/libraries/ui/platform/windows/ui_window_windows_filedialog.c
// Win32 file / folder picker via the common item dialog for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifdef _WIN32

#include "ui_window_windows.h"
#include <shobjidl.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <string.h>

// converts a utf-8 string to a fresh utf-16 buffer; caller frees with free()
static WCHAR* utf8_to_wide_dup(const char* s) {
    if (!s) return NULL;                                  // null guard
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;                              // conversion failed
    WCHAR* w = (WCHAR*)malloc((size_t)n * sizeof(WCHAR)); // allocate wide buffer
    if (!w) return NULL;                                  // oom
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);         // second pass: fill it
    return w;                                             // caller owns the wide copy
}

// converts a utf-16 string to a fresh utf-8 buffer; caller frees with free()
static char* wide_to_utf8_dup(const WCHAR* s) {
    if (!s) return NULL;                                  // null guard
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;                              // conversion failed
    char* u = (char*)malloc((size_t)n);                   // allocate utf-8 buffer
    if (!u) return NULL;                                  // oom
    WideCharToMultiByte(CP_UTF8, 0, s, -1, u, n, NULL, NULL);
    return u;                                             // caller owns the utf-8 copy
}

// builds a COMDLG_FILTERSPEC array from the caller's filter list
static COMDLG_FILTERSPEC* build_specs(const UiFileFilter* filters, int* out_count) {
    *out_count = 0;                                       // default: no filters
    if (!filters) return NULL;                            // nothing to translate
    int n = 0;
    while (filters[n].name) n++;                          // count until the null-name sentinel
    if (n == 0) return NULL;                              // empty list

    COMDLG_FILTERSPEC* specs = (COMDLG_FILTERSPEC*)calloc((size_t)n, sizeof(*specs));
    if (!specs) return NULL;                              // oom
    for (int i = 0; i < n; i++) {
        specs[i].pszName = utf8_to_wide_dup(filters[i].name);     // display name in utf-16
        specs[i].pszSpec = utf8_to_wide_dup(filters[i].pattern);  // glob pattern in utf-16
    }
    *out_count = n;                                       // report the live count
    return specs;                                         // caller frees with free_specs
}

// releases a COMDLG_FILTERSPEC array built by build_specs
static void free_specs(COMDLG_FILTERSPEC* specs, int count) {
    if (!specs) return;                                   // null guard
    for (int i = 0; i < count; i++) {                     // one entry per filter
        free((void*)specs[i].pszName);                    // release the display name
        free((void*)specs[i].pszSpec);                    // release the glob pattern
    }
    free(specs);                                          // release the array itself
}

// shared picker. `folder` selects FOS_PICKFOLDERS instead of FOS_FILEMUSTEXIST
static char* show_picker(UiWindow* w, const char* title, const char* default_path,
                         const UiFileFilter* filters, bool folder) {
    // the common item dialog requires an sta thread
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (hr == RPC_E_CHANGED_MODE) return NULL;            // wrong apartment: bail out
    bool did_init = SUCCEEDED(hr);                        // only uninit if we initialized

    IFileOpenDialog* dlg = NULL;
    hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IFileOpenDialog, (void**)&dlg);
    if (FAILED(hr)) {                                     // creation failed
        if (did_init) CoUninitialize();                   // undo our com init
        return NULL;
    }

    DWORD opts = 0;
    dlg->lpVtbl->GetOptions(dlg, &opts);                  // start from the current flags
    opts |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR;
    if (folder) opts |= FOS_PICKFOLDERS;                  // folder mode: pick directories
    else        opts |= FOS_FILEMUSTEXIST;                // file mode: file must exist
    dlg->lpVtbl->SetOptions(dlg, opts);                   // publish the merged flags

    if (title && *title) {                                // optional dialog caption
        WCHAR* wt = utf8_to_wide_dup(title);
        if (wt) {
            dlg->lpVtbl->SetTitle(dlg, wt);               // hand it to the shell
            free(wt);                                     // shell copied it
        }
    }

    if (default_path && *default_path) {
        // default_path can be a file or a folder
        WCHAR* wd = utf8_to_wide_dup(default_path);
        if (wd) {
            DWORD attrs = GetFileAttributesW(wd);         // probe the filesystem
            if (attrs != INVALID_FILE_ATTRIBUTES &&
                !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                // existing file: split into parent folder and filename
                WCHAR* slash = wcsrchr(wd, L'\\');
                if (!slash) slash = wcsrchr(wd, L'/');    // tolerate forward slashes
                if (slash) {
                    const WCHAR* fname = slash + 1;       // filename after the separator
                    *slash = 0;                           // truncate to the parent folder
                    IShellItem* folder_item = NULL;
                    if (SUCCEEDED(SHCreateItemFromParsingName(wd, NULL,
                                    &IID_IShellItem, (void**)&folder_item))) {
                        dlg->lpVtbl->SetFolder(dlg, folder_item);  // point at the parent
                        folder_item->lpVtbl->Release(folder_item); // release the shell item
                    }
                    dlg->lpVtbl->SetFileName(dlg, fname); // pre-fill the filename box
                }
            } else {
                // treat as a folder: open the picker there
                IShellItem* folder_item = NULL;
                if (SUCCEEDED(SHCreateItemFromParsingName(wd, NULL,
                                &IID_IShellItem, (void**)&folder_item))) {
                    dlg->lpVtbl->SetFolder(dlg, folder_item);  // point at that folder
                    folder_item->lpVtbl->Release(folder_item); // release the shell item
                }
            }
            free(wd);                                     // release the wide path copy
        }
    }

    int spec_count = 0;
    COMDLG_FILTERSPEC* specs = NULL;
    if (!folder) {                                        // filters only make sense for files
        specs = build_specs(filters, &spec_count);
        if (specs && spec_count > 0)                      // only if any were built
            dlg->lpVtbl->SetFileTypes(dlg, (UINT)spec_count, specs);
    }

    char* result = NULL;                                  // default: user cancelled
    HWND owner = (w && w->hwnd) ? w->hwnd : NULL;         // anchor to our window if we have one
    hr = dlg->lpVtbl->Show(dlg, owner);                   // run the modal dialog
    if (SUCCEEDED(hr)) {                                  // user picked something
        IShellItem* item = NULL;
        if (SUCCEEDED(dlg->lpVtbl->GetResult(dlg, &item)) && item) {
            PWSTR path = NULL;
            if (SUCCEEDED(item->lpVtbl->GetDisplayName(item,
                            SIGDN_FILESYSPATH, &path)) && path) {
                result = wide_to_utf8_dup(path);          // convert to utf-8
                CoTaskMemFree(path);                      // release the shell string
            }
            item->lpVtbl->Release(item);                  // release the shell item
        }
    }

    free_specs(specs, spec_count);                        // release the filter specs
    dlg->lpVtbl->Release(dlg);                            // release the dialog
    if (did_init) CoUninitialize();                       // undo our com init
    return result;                                        // caller owns the utf-8 path
}

// shows the native "open file" dialog and returns the picked utf-8 path
char* ui_window_open_file(UiWindow* w, const char* title,
                          const char* default_path,
                          const UiFileFilter* filters) {
    return show_picker(w, title, default_path, filters, false);  // file mode
}

// shows the native "open folder" dialog and returns the picked utf-8 path
char* ui_window_open_folder(UiWindow* w, const char* title,
                            const char* default_path) {
    return show_picker(w, title, default_path, NULL, true);      // folder mode, no filters
}

#endif