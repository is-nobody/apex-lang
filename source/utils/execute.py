# execute.py
import sys
from pathlib import Path
from source.core.tokenizer import Tokenizer
from source.core.parser_main import Parser
from source.core.parser.token import Token as ParserToken
from source.core.parser.token_type import TokenType
from source.core.interpreter_main import Interpreter
from source.libraries import BUILTIN_MODULES

def execute_file(filepath):
    if not Path(filepath).exists():
        print(f"Error: File {filepath} not found")
        return False
    
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            source = f.read()
        
        tokenizer = Tokenizer(source, filepath)
        tokens = tokenizer.tokenize()

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
            return False
        
        interpreter = Interpreter(filepath)
        
        for name, module in BUILTIN_MODULES.items():
            interpreter.global_env.define(name, module)
        
        interpreter.evaluate(ast.to_dict())
        return True
        
    except Exception as e:
        print(f"Error: {e}")
        return False

def main():
    if len(sys.argv) != 2:
        sys.exit(1)
    
    filepath = sys.argv[1]
    success = execute_file(filepath)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()