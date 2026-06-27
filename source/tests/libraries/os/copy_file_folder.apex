import os

function copy_file()
    os.create_file("source.txt")
    
    os.copy_file("source.txt", "dest.txt")
    
    e1 = os.exists("source.txt")
    e2 = os.exists("dest.txt")
    
    os.output("source.txt exists: {e1}, expected true")
    os.output("dest.txt exists: {e2}, expected true")
    
    os.delete_file("source.txt")
    os.delete_file("dest.txt")
    
    result = os.copy_file("nonexistent.txt", "output.txt")
    os.output("copy_file nonexistent returned: {result}, expected false")

function copy_folder()
    os.create_folder("srcdir")
    os.create_folder("srcdir/subdir")
    os.create_file("srcdir/file1.txt")
    os.create_file("srcdir/subdir/file2.txt")
    
    result = os.copy_folder("srcdir", "dstdir")
    os.output("copy_folder result: {result}, expected true")
    
    e1 = os.exists("dstdir")
    e2 = os.exists("dstdir/file1.txt")
    e3 = os.exists("dstdir/subdir")
    e4 = os.exists("dstdir/subdir/file2.txt")
    
    os.output("dstdir exists: {e1}, expected true")
    os.output("dstdir/file1.txt exists: {e2}, expected true")
    os.output("dstdir/subdir exists: {e3}, expected true")
    os.output("dstdir/subdir/file2.txt exists: {e4}, expected true")
    
    os.delete_file("srcdir/file1.txt")
    os.delete_file("srcdir/subdir/file2.txt")
    os.delete_folder("srcdir/subdir")
    os.delete_folder("srcdir")
    
    os.delete_file("dstdir/file1.txt")
    os.delete_file("dstdir/subdir/file2.txt")
    os.delete_folder("dstdir/subdir")
    os.delete_folder("dstdir")
    
    result = os.copy_folder("nonexistent", "output")
    os.output("copy_folder nonexistent returned: {result}, expected false")

copy_file()
copy_folder()