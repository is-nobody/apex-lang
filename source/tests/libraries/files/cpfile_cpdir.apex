import os
import files

function test_cpfile()
    files.mkfile("source.txt")
    
    files.cpfile("source.txt", "dest.txt")
    
    e1 = files.exists("source.txt")
    e2 = files.exists("dest.txt")
    
    os.output("source.txt exists: {e1}, expected true")
    os.output("dest.txt exists: {e2}, expected true")
    
    files.rmfile("source.txt")
    files.rmfile("dest.txt")
    
    result = files.cpfile("nonexistent.txt", "output.txt")
    os.output("cpfile nonexistent returned: {result}, expected false")

function test_cpdir()
    files.mkdir("srcdir")
    files.mkdir("srcdir/subdir")
    files.mkfile("srcdir/file1.txt")
    files.mkfile("srcdir/subdir/file2.txt")
    
    result = files.cpdir("srcdir", "dstdir")
    os.output("cpdir result: {result}, expected true")
    
    e1 = files.exists("dstdir")
    e2 = files.exists("dstdir/file1.txt")
    e3 = files.exists("dstdir/subdir")
    e4 = files.exists("dstdir/subdir/file2.txt")
    
    os.output("dstdir exists: {e1}, expected true")
    os.output("dstdir/file1.txt exists: {e2}, expected true")
    os.output("dstdir/subdir exists: {e3}, expected true")
    os.output("dstdir/subdir/file2.txt exists: {e4}, expected true")
    
    files.rmfile("srcdir/file1.txt")
    files.rmfile("srcdir/subdir/file2.txt")
    files.rmdir("srcdir/subdir")
    files.rmdir("srcdir")
    
    files.rmfile("dstdir/file1.txt")
    files.rmfile("dstdir/subdir/file2.txt")
    files.rmdir("dstdir/subdir")
    files.rmdir("dstdir")
    
    result = files.cpdir("nonexistent", "output")
    os.output("cpdir nonexistent returned: {result}, expected false")

test_cpfile()
test_cpdir()