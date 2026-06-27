import os

function copy_file()
    os.create_file("source.txt")
    
    os.copy("source.txt", "dest.txt")
    
    e1 = os.exists("source.txt")
    e2 = os.exists("dest.txt")
    
    os.output("source.txt exists: {e1}, expected true")
    os.output("dest.txt exists: {e2}, expected true")
    
    os.delete("source.txt")
    os.delete("dest.txt")
    
    result = os.copy("nonexistent.txt", "output.txt")
    os.output("copy nonexistent returned: {result}, expected false")

function copy_folder()
    os.create_folder("srcdir")
    os.create_folder("srcdir/subdir")
    os.create_file("srcdir/file1.txt")
    os.create_file("srcdir/subdir/file2.txt")
    
    result = os.copy("srcdir", "dstdir")
    os.output("copy result: {result}, expected true")
    
    e1 = os.exists("dstdir")
    e2 = os.exists("dstdir/file1.txt")
    e3 = os.exists("dstdir/subdir")
    e4 = os.exists("dstdir/subdir/file2.txt")
    
    os.output("dstdir exists: {e1}, expected true")
    os.output("dstdir/file1.txt exists: {e2}, expected true")
    os.output("dstdir/subdir exists: {e3}, expected true")
    os.output("dstdir/subdir/file2.txt exists: {e4}, expected true")
    
    os.delete("srcdir/file1.txt")
    os.delete("srcdir/subdir/file2.txt")
    os.delete("srcdir/subdir")
    os.delete("srcdir")
    
    os.delete("dstdir/file1.txt")
    os.delete("dstdir/subdir/file2.txt")
    os.delete("dstdir/subdir")
    os.delete("dstdir")
    
    result = os.copy("nonexistent", "output")
    os.output("copy nonexistent returned: {result}, expected false")

copy_file()
copy_folder()