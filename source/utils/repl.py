import platform
import sys
import os
from source.utils.execute import execute_file

def print_apex_info():
    system_info = f"{platform.system()} {platform.release()}"
    print(f"Apex v26.06 on {system_info}")
    print("Write filename for execute it")

def repl_main():
    print_apex_info()
    
    while True:
        try:
            filename = input(">>> ").strip()
            if filename:
                execute_file(filename)
        except EOFError:
            print()
            break
        except KeyboardInterrupt:
            os._exit(0)

if __name__ == "__main__":
    try:
        repl_main()
    except KeyboardInterrupt:
        os._exit(0)