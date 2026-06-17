import os
import files

function copy_file()
    files.create_file("source.txt")
    
    files.copy_file("source.txt", "dest.txt")
    
    e1 = files.exists("source.txt")
    e2 = files.exists("dest.txt")
    
    os.output("source.txt exists: {e1}, expected true")
    os.output("dest.txt exists: {e2}, expected true")
    
    files.delete_file("source.txt")
    files.delete_file("dest.txt")
    
    result = files.copy_file("nonexistent.txt", "output.txt")
    os.output("copy_file nonexistent returned: {result}, expected false")

function copy_folder()
    files.create_folder("srcdir")
    files.create_folder("srcdir/subdir")
    files.create_file("srcdir/file1.txt")
    files.create_file("srcdir/subdir/file2.txt")
    
    result = files.copy_folder("srcdir", "dstdir")
    os.output("copy_folder result: {result}, expected true")
    
    e1 = files.exists("dstdir")
    e2 = files.exists("dstdir/file1.txt")
    e3 = files.exists("dstdir/subdir")
    e4 = files.exists("dstdir/subdir/file2.txt")
    
    os.output("dstdir exists: {e1}, expected true")
    os.output("dstdir/file1.txt exists: {e2}, expected true")
    os.output("dstdir/subdir exists: {e3}, expected true")
    os.output("dstdir/subdir/file2.txt exists: {e4}, expected true")
    
    files.delete_file("srcdir/file1.txt")
    files.delete_file("srcdir/subdir/file2.txt")
    files.delete_folder("srcdir/subdir")
    files.delete_folder("srcdir")
    
    files.delete_file("dstdir/file1.txt")
    files.delete_file("dstdir/subdir/file2.txt")
    files.delete_folder("dstdir/subdir")
    files.delete_folder("dstdir")
    
    result = files.copy_folder("nonexistent", "output")
    os.output("copy_folder nonexistent returned: {result}, expected false")

copy_file()
copy_folder()