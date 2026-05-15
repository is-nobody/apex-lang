# main.py
import sys
import os
import json
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from source.core.tokenizer import Tokenizer
from source.core.parser_main import Parser, parse_file
from source.core.parser.token_type import TokenType
from source.core.parser.token import Token as ParserToken
from source.core.interpreter import Interpreter

def find_apex_file(filename: str) -> Path | None:
    filename = filename.replace('\\', '/')
    
    if filename.endswith('.apex'):
        path = Path(filename)
        if path.exists():
            return path
        return None
    
    path = Path(filename + '.apex')
    if path.exists():
        return path
    
    return None

def main():
    if len(sys.argv) < 2:
        print("Usage: python main.py <filename>")
        print("Example: python main.py test")
        sys.exit(1)
    
    filename = sys.argv[1]
    filepath = find_apex_file(filename)
    
    if filepath is None:
        print(f"Error: File '{filename}.apex' not found")
        sys.exit(1)
    
    # Читаем исходник
    with open(filepath, 'r', encoding='utf-8') as f:
        source = f.read()
    
    # Токенизация
    from source.core.tokenizer import Tokenizer
    tokenizer = Tokenizer(source, str(filepath))
    tokens = tokenizer.tokenize()
    
    # Конвертируем токены в формат парсера
    parser_tokens = [
        ParserToken(
            type=TokenType[t.type.name],
            value=t.value,
            line=t.line,
            column=t.column
        )
        for t in tokens
    ]
    
    # Парсинг
    parser = Parser(parser_tokens, str(filepath))
    ast = parser.parse()
    
    if ast is None:
        sys.exit(1)
    
    ast_dict = ast.to_dict()
    
    # Сохраняем AST в .cache
    cache_dir = Path('.cache')
    cache_dir.mkdir(exist_ok=True)
    cache_file = cache_dir / f"{filepath.stem}.apexc"
    
    with open(cache_file, 'w', encoding='utf-8') as f:
        json.dump(ast_dict, f, indent=2, ensure_ascii=False)
    
    # Интерпретация
    interpreter = Interpreter(str(filepath))
    try:
        interpreter.evaluate(ast_dict)
    except KeyboardInterrupt:
        sys.exit(1)
    except RuntimeError as e:
        print(e)
        sys.exit(1)
    except Exception as e:
        print(e)
        sys.exit(1)

if __name__ == "__main__":
    main()