import os
import files

test_paths = (
    "/home/user/documents/file.txt",
    "/usr/bin/",
    "relative/path/to/file",
    "/"
)

for path in test_paths
    parent = files.parentfolder(path)
    if parent == false
        os.output("Error getting parent of: {path}")
    else
        os.output("Parent of '{path}' -> '{parent}'")