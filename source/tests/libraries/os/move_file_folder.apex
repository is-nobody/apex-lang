import os

function move_file()
    os.create_file("old.txt")
    os.create_folder("dir")
    
    result = os.move_file("old.txt", "dir/old.txt")
    
    e1 = os.exists("old.txt")
    e2 = os.exists("dir/old.txt")
    
    os.output("old.txt exists: {e1}, expected false")
    os.output("dir/old.txt exists: {e2}, expected true")
    
    if e2 == true
        os.delete_file("dir/old.txt")

    os.delete_folder("dir")

function move_folder()
    os.create_folder("src_dir")
    os.create_file("src_dir/inner.txt")
    os.write("src_dir/inner.txt", "data")

    result = os.move_folder("src_dir", "dst_dir")

    e1 = os.exists("src_dir")
    e2 = os.exists("dst_dir")
    e3 = os.exists("dst_dir/inner.txt")
    
    content = os.read("dst_dir/inner.txt")

    os.output("src_dir exists: {e1}, expected false")
    os.output("dst_dir exists: {e2}, expected true")

    if e2 == true
        os.delete_file("dst_dir/inner.txt")
    
    os.delete_folder("dst_dir")

move_file()
move_folder()