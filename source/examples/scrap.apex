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

// shared mutable state; tables are reference types, so updating
// state["output_name"] from any function is visible everywhere
state = ["output_name" = ""]

// case-insensitive membership test against a table of strings
function contains_ci(haystack, needle)
    needle_lower = string.lower(needle)
    for item in haystack
        if string.lower(item) == needle_lower
            return true
    return false

// case-sensitive membership test against a table of strings
function contains(haystack, needle)
    for item in haystack
        if item == needle
            return true
    return false

// true when the directory name should be skipped
function should_ignore_dir(name)
    n = string.length(name)
    if n > 0
        if string.slice(name, 1, 1) == "."
            return true
    return contains(IGNORE_DIRS, name)

// extract the extension (text after the last dot), or "" if none
function get_ext(filename)
    n = string.length(filename)
    for i = n, 1, -1
        if string.slice(filename, i, i) == "."
            return string.slice(filename, i + 1, n)
    return ""

// true when the extension is in the ignore list
function should_ignore_ext(ext)
    if string.length(ext) == 0
        return false
    return contains_ci(IGNORE_EXTS, ext)

// true when the filename is in the ignore list (or is the output file)
function should_ignore_file(name)
    if name == state["output_name"]
        return true
    return contains(IGNORE_FILES, name)

// return the final component of a path (after the last slash)
function basename(path)
    n = string.length(path)
    for i = n, 1, -1
        if string.slice(path, i, i) == "/"
            return string.slice(path, i + 1, n)
    return path

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

    header = "`{rel_path}`\n\n```{ext}\n"
    os.append(state["output_name"], header)
    os.append(state["output_name"], content)

    // ensure file body ends with a newline before the closing fence
    n = string.length(content)
    if n > 0
        last = string.slice(content, n, n)
        if last != "\n"
            os.append(state["output_name"], "\n")

    os.append(state["output_name"], "```\n\n")
    return none

// recursively walk a directory tree
function walk_dir(base, rel)
    entries = os.list_folder(base)
    if entries == none
        return none

    count = table.size(entries)

    for i = 1, count
        name = entries[i]
        if string.length(name) == 0
            continue

        // declare paths before the branch so both arms can see them
        rel_path = name
        if string.length(rel) > 0
            rel_path = "{rel}/{name}"
        full_path = "{base}/{name}"

        is_dir = os.is_folder(full_path)
        is_reg = os.is_file(full_path)

        if is_dir == true
            if should_ignore_dir(name) != true
                walk_dir(full_path, rel_path)
        else if is_reg == true
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
    os.write(state["output_name"], "# {base}\n\n")

    // walk the tree starting at the current directory
    walk_dir(cwd, "")

    out = state["output_name"]
    os.output("Done -> {out}")
    return none

main()