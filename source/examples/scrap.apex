// source/examples/scrap.apex
// Implementation of Scrap Tool on Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import string
import table

// directories to skip during traversal (any dot-directory is also skipped)
constant IGNORE_DIRS = [
    ".git", "__pycache__", ".venv", "venv", "node_modules", ".idea", ".vscode",
    "target", "build", "dist", "out", ".next", ".nuxt", ".cache", ".parcel-cache",
    ".turbo", ".gradle", ".mvn", ".settings", ".classpath", ".project", ".factorypath",
    "cmake-build-debug", "cmake-build-release", "CMakeFiles", "vendor", "bower_components",
    "coverage", ".pytest_cache", ".mypy_cache", ".ruff_cache", ".tox", ".nox",
    "htmlcov", ".coverage", ".sass-cache", ".dart_tool", ".flutter-plugins",
    ".packages", ".pub-cache", "Pods", "DerivedData", ".build", "obj",
    "Debug", "Release", "x64", "x86", "Generated", "generated", ".terraform",
    ".serverless", ".cdk", ".aws-sam", ".stack-work", ".elixir_ls", "_build",
    "deps", ".mix", ".hex", ".dub", ".deps", ".libs", "autom4te.cache",
    ".eggs", ".ipynb_checkpoints", ".jupyter", ".spyproject",
    ".vs", ".angular", ".nx", ".test", ".tmp", "tmp", "temp", ".temp",
    "artifacts", "packages", ".yarn", ".pnp", ".pnp.js", ".svn", ".hg"
]

// file extensions to skip (binaries / media / archives / etc.)
constant IGNORE_EXTS = [
    "exe", "dll", "so", "dylib", "bin", "o", "obj", "a", "lib", "class", "jar",
    "war", "ear", "nupkg", "snupkg", "msi", "deb", "rpm", "apk", "ipa", "aab",
    "jpg", "jpeg", "png", "gif", "ico", "bmp", "tiff", "webp", "psd", "ai",
    "pdf", "zip", "tar", "gz", "bz2", "xz", "7z", "rar", "zst", "lz4", "lzma",
    "mp3", "mp4", "avi", "mov", "mkv", "wav", "flac", "ogg", "webm", "m4a", "m4v",
    "ttf", "otf", "woff", "woff2", "eot", "db", "sqlite", "sqlite3", "db3",
    "doc", "docx", "xls", "xlsx", "ppt", "pptx", "odt", "ods", "odp",
    "pyc", "pyo", "pyd", "pyw", "pyz", "pyzw",
    "map", "wasm", "bc", "ll", "pdb", "ilk", "exp", "lo", "la",
    "bundle", "framework", "xcframework", "car", "storyboardc", "xib", "nib",
    "momd", "mom", "app", "plugin", "theme", "un~", "swp", "swo", "swn",
    "tmp", "temp", "log", "bak", "backup", "old", "orig", "rej",
    "rpmnew", "rpmsave", "rpmorig"
]

// specific filenames to skip (lock files, editor metadata, etc.)
constant IGNORE_FILES = [
    "package-lock.json", "yarn.lock", "pnpm-lock.yaml", "npm-shrinkwrap.json",
    "Cargo.lock", "Gemfile.lock", "Pipfile.lock", "poetry.lock",
    "composer.lock", "go.sum", "pubspec.lock", "mix.lock", "Podfile.lock",
    "flake.lock", "package-lock.yaml", "bun.lockb", "bun.lock",
    "deno.lock", "packages.lock.json", "project.assets.json",
    ".DS_Store", "Thumbs.db", "desktop.ini", ".gitignore", ".gitattributes",
    ".gitmodules", ".editorconfig", ".prettierrc", ".eslintrc", ".babelrc"
]

// precomputed O(1) lookup sets; built once so per-file checks skip the linear scan
IGNORE_DIR_SET = []
IGNORE_EXT_SET = []
IGNORE_FILE_SET = []

for d in IGNORE_DIRS
    IGNORE_DIR_SET[d] = true

for e in IGNORE_EXTS
    IGNORE_EXT_SET[string.lower(e)] = true

for f in IGNORE_FILES
    IGNORE_FILE_SET[f] = true

// shared mutable state; chunk_count lives here so reassignments inside functions stick
state = ["output_name" = "", "chunk_count" = 0]

// buffered output; flushed in batches, tracked by state["chunk_count"]
chunks = []

// flush the output buffer to the output file and clear it
function flush()
    if state["chunk_count"] == 0
        return none
    os.append(state["output_name"], string.join(chunks, ""))
    table.clear(chunks)
    state["chunk_count"] = 0
    return none

// append a string to the output buffer; flushes when the batch is full
function emit(s)
    n = state["chunk_count"] + 1
    chunks[n] = s
    state["chunk_count"] = n
    if n >= 500
        flush()
    return none

// true when the directory name should be skipped
function should_ignore_dir(name)
    n = string.length(name)
    if n > 0
        if string.slice(name, 1, 1) == "."
            return true
    return table.has(IGNORE_DIR_SET, name)

// extract the extension (text after the last dot), or "" if none
function get_ext(filename)
    parts = string.split(filename, ".")
    n = table.size(parts)
    if n <= 1
        return ""
    return parts[n]

// true when the extension is in the ignore list (case-insensitive)
function should_ignore_ext(ext)
    if string.length(ext) == 0
        return false
    return table.has(IGNORE_EXT_SET, string.lower(ext))

// true when the filename is in the ignore list (or is the output file)
function should_ignore_file(name)
    if name == state["output_name"]
        return true
    return table.has(IGNORE_FILE_SET, name)

// return the final component of a path (after the last slash)
function basename(path)
    parts = string.split(path, "/")
    n = table.size(parts)
    if n == 0
        return path
    return parts[n]

// read a file, apply filters, and append it to the output markdown
function process_file(full_path, rel_path)
    filename = basename(rel_path)
    ext = get_ext(filename)

    if should_ignore_ext(ext) == true
        return none
    if should_ignore_file(filename) == true
        return none

    content = os.read(full_path)
    if content == none
        return none

    emit("`{rel_path}`\n\n```{ext}\n")
    emit(content)

    // ensure file body ends with a newline before the closing fence
    n = string.length(content)
    if n > 0
        last = string.slice(content, n, n)
        if last != "\n"
            emit("\n")

    emit("```\n\n")
    return none

// recursively walk a directory tree
function walk_dir(base, rel)
    entries = os.list_folder(base)
    if entries == none
        return none

    count = table.size(entries)

    for i = 1, count
        name = entries[i]

        // declare paths before the branch so both arms can see them
        rel_path = name
        if string.length(rel) > 0
            rel_path = "{rel}/{name}"
        full_path = "{base}/{name}"

        // one stat per entry; process_file bails out on anything os.read cannot read
        if os.is_folder(full_path) == true
            if should_ignore_dir(name) != true
                walk_dir(full_path, rel_path)
        else
            process_file(full_path, rel_path)
    return none

// entry point
function main()
    cwd = os.current_folder()
    if cwd == none
        os.output("Error: cannot get current working directory")
        return none

    base = basename(cwd)
    state["output_name"] = "{base}.md"

    // create the output file with its title (truncates any existing content)
    os.write(state["output_name"], "")
    emit("# {base}\n\n")

    // walk the tree starting at the current directory
    walk_dir(cwd, "")

    // flush whatever is left in the buffer
    flush()

    out = state["output_name"]
    os.output("Done -> {out}")
    return none

main()