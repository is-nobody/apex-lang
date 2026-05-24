import os

os.output("=== OS TESTS ===")
os.output("")

// 1. output
os.output("1. output:")
os.output("   Hello")
os.output("")

// 2. time
os.output("2. time:")
t = os.time()
os.output("   {t}")
os.output("")

// 3. wait
os.output("3. wait:")
start = os.time()
os.wait(0.5)
elapsed = os.time() - start
os.output("   Elapsed: {elapsed}")
os.output("")

// 4. getcwd
os.output("4. getcwd:")
cwd = os.getcwd()
os.output("   {cwd}")
os.output("")

// 5. exists
os.output("5. exists:")
e = os.exists(".")
os.output("   '.' exists: {e}")
os.output("")

// 6. isfile
os.output("6. isfile:")
f = os.isfile("main.py")
os.output("   main.py is file: {f}")
os.output("")

// 7. isdir
os.output("7. isdir:")
d = os.isdir("source")
os.output("   source is dir: {d}")
os.output("")

// 8. mkdir/rmdir
os.output("8. mkdir/rmdir:")
os.mkdir("test_dir")
e1 = os.exists("test_dir")
os.rmdir("test_dir")
e2 = os.exists("test_dir")
os.output("   Created: {e1}")
os.output("   Removed: {e2}")
os.output("")

// 9. mkfile/rmfile
os.output("9. mkfile/rmfile:")
os.mkfile("test.txt")
e1 = os.exists("test.txt")
os.rmfile("test.txt")
e2 = os.exists("test.txt")
os.output("   Created: {e1}")
os.output("   Removed: {e2}")
os.output("")

// 10. write/read
os.output("10. write/read:")
os.mkfile("data.txt")
os.write("data.txt", "Hello Apex")
content = os.read("data.txt")
os.output("   Content: {content}")
os.rmfile("data.txt")
os.output("")

// 11. rename
os.output("11. rename:")
os.mkfile("old.txt")
os.rename("old.txt", "new.txt")
e1 = os.exists("old.txt")
e2 = os.exists("new.txt")
os.output("   Old exists: {e1}")
os.output("   New exists: {e2}")
os.rmfile("new.txt")
os.output("")

// 12. system
os.output("12. system:")
code = os.system("echo test")
os.output("   Exit code: {code}")
os.output("")

// 13. platform
os.output("13. platform:")
p = os.platform()
os.output("   OS: {p}")
os.output("")

os.output("=== DONE ===")