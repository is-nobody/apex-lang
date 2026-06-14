import os
import files

function mvfile()
    files.mkfile("old.txt")
    files.mkdir("dir")
    
    result = files.mvfile("old.txt", "dir/old.txt")
    
    e1 = files.exists("old.txt")
    e2 = files.exists("dir/old.txt")
    
    os.output("old.txt exists: {e1}, expected false")
    os.output("dir/old.txt exists: {e2}, expected true")
    
    if e2 == true
        files.rmfile("dir/old.txt")

    files.rmdir("dir")

function mvdir()
    files.mkdir("src_dir")
    files.mkfile("src_dir/inner.txt")
    files.write("src_dir/inner.txt", "data")

    result = files.mvdir("src_dir", "dst_dir")

    e1 = files.exists("src_dir")
    e2 = files.exists("dst_dir")
    e3 = files.exists("dst_dir/inner.txt")
    
    content = files.read("dst_dir/inner.txt")

    os.output("src_dir exists: {e1}, expected false")
    os.output("dst_dir exists: {e2}, expected true")

    if e2 == true
        files.rmfile("dst_dir/inner.txt")
    
    files.rmdir("dst_dir")

mvfile()
mvdir()