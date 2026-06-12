import os

function mvfile()
    os.mkfile("old.txt")
    os.mkdir("dir")
    
    result = os.mvfile("old.txt", "dir/old.txt")
    
    e1 = os.exists("old.txt")
    e2 = os.exists("dir/old.txt")
    
    os.output("old.txt exists: {e1}, expected false")
    os.output("dir/old.txt exists: {e2}, expected true")
    
    if e2 == true
        os.rmfile("dir/old.txt")

    os.rmdir("dir")

function mvdir()
    os.mkdir("src_dir")
    os.mkfile("src_dir/inner.txt")
    os.write("src_dir/inner.txt", "data")

    result = os.mvdir("src_dir", "dst_dir")

    e1 = os.exists("src_dir")
    e2 = os.exists("dst_dir")
    e3 = os.exists("dst_dir/inner.txt")
    
    content = os.read("dst_dir/inner.txt")

    os.output("src_dir exists: {e1}, expected false")
    os.output("dst_dir exists: {e2}, expected true")

    if e2 == true
        os.rmfile("dst_dir/inner.txt")
    
    os.rmdir("dst_dir")

mvfile()
mvdir()