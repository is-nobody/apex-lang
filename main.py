# main.py
import sys
from pathlib import Path
from source.utils.repl import main as repl_main
from source.utils.execute import execute_file
import source.utils.add_to_path

def main():
    source.utils.add_to_path.main()
    if len(sys.argv) == 1:
        # No arguments - start REPL
        repl_main()
    elif len(sys.argv) == 2:
        # With filename - execute it
        filepath = sys.argv[1]
        success = execute_file(filepath)
        sys.exit(0 if success else 1)
    else:
        print("Usage: python3 main.py [filename.apex]")
        sys.exit(1)

if __name__ == "__main__":
    main()