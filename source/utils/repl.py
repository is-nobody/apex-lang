# apex_repl.py
import platform
from source.utils.execute import execute_file

def print_apex_info():
    system_info = f"{platform.system()} {platform.release()}"
    print(f"Apex v26.06 on {system_info}")
    print("Write filename for execute it")

def repl():
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
            print("")
            break

if __name__ == "__main__":
    repl()