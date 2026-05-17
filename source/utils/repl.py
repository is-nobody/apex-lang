# apex_repl.py
import platform
import sys
from source.utils.execute import execute_file

def print_apex_info():
    """Print Apex version and system info"""
    system_info = f"{platform.system()} {platform.release()}"
    print(f"Apex v26.06 (main, June 1) on {system_info}")
    print("Write filename for execute it")

def main():
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
            print("\nExiting...")
            break

if __name__ == "__main__":
    main()