import os
import files

function move_file()
    files.create_file("old.txt")
    files.create_folder("dir")
    
    result = files.move_file("old.txt", "dir/old.txt")
    
    e1 = files.exists("old.txt")
    e2 = files.exists("dir/old.txt")
    
    os.output("old.txt exists: {e1}, expected false")
    os.output("dir/old.txt exists: {e2}, expected true")
    
    if e2 == true
        files.delete_file("dir/old.txt")

    files.delete_folder("dir")

function move_folder()
    files.create_folder("src_dir")
    files.create_file("src_dir/inner.txt")
    files.write("src_dir/inner.txt", "data")

    result = files.move_folder("src_dir", "dst_dir")

    e1 = files.exists("src_dir")
    e2 = files.exists("dst_dir")
    e3 = files.exists("dst_dir/inner.txt")
    
    content = files.read("dst_dir/inner.txt")

    os.output("src_dir exists: {e1}, expected false")
    os.output("dst_dir exists: {e2}, expected true")

    if e2 == true
        files.delete_file("dst_dir/inner.txt")
    
    files.delete_folder("dst_dir")

move_file()
move_folder()