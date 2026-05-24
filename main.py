# main.py
import sys
from source.utils.repl import repl_main
from source.utils.execute import execute_file
from source.utils.add_to_path import path

def main():
    path()
    try:
        if len(sys.argv) == 1:
            # no arguments - start repl
            repl_main()
        elif len(sys.argv) == 2:
            # with filename - execute it
            filepath = sys.argv[1]
            success = execute_file(filepath)
            sys.exit(0 if success else 1)
        else:
            sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)

if __name__ == "__main__":
    main()