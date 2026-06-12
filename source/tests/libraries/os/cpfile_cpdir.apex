import os

function test_cpfile()
    os.mkfile("source.txt")
    
    os.cpfile("source.txt", "dest.txt")
    
    e1 = os.exists("source.txt")
    e2 = os.exists("dest.txt")
    
    os.output("source.txt exists: {e1}, expected true")
    os.output("dest.txt exists: {e2}, expected true")
    
    os.rmfile("source.txt")
    os.rmfile("dest.txt")
    
    result = os.cpfile("nonexistent.txt", "output.txt")
    os.output("cpfile nonexistent returned: {result}, expected false")

function test_cpdir()
    os.mkdir("srcdir")
    os.mkdir("srcdir/subdir")
    os.mkfile("srcdir/file1.txt")
    os.mkfile("srcdir/subdir/file2.txt")
    
    result = os.cpdir("srcdir", "dstdir")
    os.output("cpdir result: {result}, expected true")
    
    e1 = os.exists("dstdir")
    e2 = os.exists("dstdir/file1.txt")
    e3 = os.exists("dstdir/subdir")
    e4 = os.exists("dstdir/subdir/file2.txt")
    
    os.output("dstdir exists: {e1}, expected true")
    os.output("dstdir/file1.txt exists: {e2}, expected true")
    os.output("dstdir/subdir exists: {e3}, expected true")
    os.output("dstdir/subdir/file2.txt exists: {e4}, expected true")
    
    os.rmfile("srcdir/file1.txt")
    os.rmfile("srcdir/subdir/file2.txt")
    os.rmdir("srcdir/subdir")
    os.rmdir("srcdir")
    
    os.rmfile("dstdir/file1.txt")
    os.rmfile("dstdir/subdir/file2.txt")
    os.rmdir("dstdir/subdir")
    os.rmdir("dstdir")
    
    result = os.cpdir("nonexistent", "output")
    os.output("cpdir nonexistent returned: {result}, expected false")

test_cpfile()
test_cpdir()