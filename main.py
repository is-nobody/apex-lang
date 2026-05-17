# main.py
import sys
from pathlib import Path

def print_error(error: Exception, filename: str = None):
    if isinstance(error, SyntaxError):
        print(f"Error: {error}")
        return
    
    if isinstance(error, RuntimeError):
        print(f"Error: {error}")
        return
    
    if filename:
        print(f"Error in {filename}: {error}")
    else:
        print(f"Error: {error}")

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 main.py <filename.apex>")
        sys.exit(1)
    
    filepath = sys.argv[1]
    
    if not Path(filepath).exists():
        print(f"Error: File {filepath} not found")
        sys.exit(1)
    
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            source = f.read()
        
        from source.core.tokenizer import Tokenizer
        tokenizer = Tokenizer(source, filepath)
        tokens = tokenizer.tokenize()
        
        from source.core.parser_main import Parser, read_tokens_from_file, fixup_tokens
        from source.core.parser.token import Token as ParserToken
        from source.core.parser.token_type import TokenType
        
        parser_tokens = []
        for t in tokens:
            parser_tokens.append(ParserToken(
                type=TokenType[t.type.name],
                value=t.value,
                line=t.line,
                column=t.column
            ))
        
        parser = Parser(parser_tokens, filepath)
        ast = parser.parse()
        
        if ast is None:
            sys.exit(1)
        
        from source.core.interpreter_main import Interpreter
        interpreter = Interpreter(filepath)
        
        from source.libraries import BUILTIN_MODULES
        for name, module in BUILTIN_MODULES.items():
            interpreter.global_env.define(name, module)
        
        interpreter.evaluate(ast.to_dict())
        
    except SyntaxError as e:
        print(f"Syntax Error {e}")
        sys.exit(1)
    except RuntimeError as e:
        print(f"Runtime Error {e}")
        sys.exit(1)
    except Exception as e:
        error_msg = str(e)
        if error_msg:
            print(f"Error {error_msg}")
        else:
            print(f"Error {type(e).__name__}")
        sys.exit(1)

if __name__ == "__main__":
    main()