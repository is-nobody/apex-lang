# source/core/tokenizer.py
import sys
import os
from pathlib import Path
from enum import Enum, auto
from dataclasses import dataclass

class TokenType(Enum):
    # Keywords
    FUNCTION = auto()
    IF = auto()
    ELIF = auto()
    ELSE = auto()
    WHILE = auto()
    FOR = auto()
    IN = auto()
    TO = auto()
    RANGE = auto()
    BREAK = auto()
    CONTINUE = auto()
    RETURN = auto()
    IMPORT = auto()
    TRY = auto()
    FAILURE = auto()
    ALWAYS = auto()
    AND = auto()
    OR = auto()
    NOT = auto()
    
    # Literals
    NONE = auto()
    TRUE = auto()
    FALSE = auto()
    NUMBER = auto()
    STRING = auto()
    IDENTIFIER = auto()
    
    # Operators
    PLUS = auto()
    MINUS = auto()
    STAR = auto()
    SLASH = auto()
    PERCENT = auto()
    EQUAL = auto()
    EQUAL_EQUAL = auto()
    NOT_EQUAL = auto()
    LESS = auto()
    GREATER = auto()
    LESS_EQUAL = auto()
    GREATER_EQUAL = auto()
    
    # Delimiters
    LPAREN = auto()
    RPAREN = auto()
    COMMA = auto()
    DOT = auto()
    COLON = auto()
    NEWLINE = auto()
    
    # Special
    EOF = auto()

@dataclass
class Token:
    type: TokenType
    value: str
    line: int
    column: int
    
    def __repr__(self):
        return f"Token({self.type.name}, '{self.value}', line={self.line}, col={self.column})"

KEYWORDS = {
    'function': TokenType.FUNCTION,
    'if': TokenType.IF,
    'elif': TokenType.ELIF,
    'else': TokenType.ELSE,
    'while': TokenType.WHILE,
    'for': TokenType.FOR,
    'in': TokenType.IN,
    'to': TokenType.TO,
    'range': TokenType.RANGE,
    'break': TokenType.BREAK,
    'continue': TokenType.CONTINUE,
    'return': TokenType.RETURN,
    'import': TokenType.IMPORT,
    'try': TokenType.TRY,
    'failure': TokenType.FAILURE,
    'always': TokenType.ALWAYS,
    'and': TokenType.AND,
    'or': TokenType.OR,
    'not': TokenType.NOT,
    'none': TokenType.NONE,
    'true': TokenType.TRUE,
    'false': TokenType.FALSE,
}

class Tokenizer:
    def __init__(self, source: str, filename: str = "<unknown>"):
        self.source = source
        self.filename = filename
        self.pos = 0
        self.line = 1
        self.column = 1
        self.tokens = []
    
    def error(self, message: str):
        raise SyntaxError(f"{self.filename}:{self.line}:{self.column}: {message}")
    
    def peek(self, offset: int = 0) -> str:
        index = self.pos + offset
        return '' if index >= len(self.source) else self.source[index]
    
    def advance(self) -> str:
        if self.pos >= len(self.source):
            return ''
        char = self.source[self.pos]
        self.pos += 1
        if char == '\n':
            self.line += 1
            self.column = 1
        else:
            self.column += 1
        return char
    
    def add_token(self, token_type: TokenType, value: str, line: int, column: int):
        self.tokens.append(Token(token_type, value, line, column))
    
    def skip_whitespace(self):
        while self.peek() in (' ', '\t', '\r'):
            self.advance()
    
    def skip_comment(self):
        self.advance()  # first /
        self.advance()  # second /
        
        while self.peek() and self.peek() != '\n':
            self.advance()
    
    def read_string(self) -> str:
        start_line = self.line
        start_col = self.column
        self.advance()  # Skip opening quote
        
        string_content = ""
        while self.peek():
            if self.peek() == '"':
                self.advance()  # Skip this quote
                next_char = self.peek()
                
                # Конец строки только если после кавычки:
                # - конец файла
                # - перевод строки  
                # - оператор сравнения
                # - закрывающая скобка или запятая
                if next_char == '' or next_char == '\n' or next_char == ')' or next_char == ',':
                    break
                
                # В любом другом случае кавычка внутри строки
                string_content += '"'
            else:
                string_content += self.advance()
        
        return string_content
    
    def read_number(self) -> str:
        number = ""
        while self.peek().isdigit():
            number += self.advance()
        
        if self.peek() == '.' and self.peek(1).isdigit():
            number += self.advance()  # dot
            while self.peek().isdigit():
                number += self.advance()
        
        return number
    
    def read_identifier(self) -> str:
        identifier = ""
        while self.peek() and (self.peek().isalnum() or self.peek() == '_'):
            identifier += self.advance()
        return identifier
    
    def tokenize(self) -> list[Token]:
        while self.pos < len(self.source):
            self.skip_whitespace()
            
            if self.pos >= len(self.source):
                break
            
            char = self.peek()
            line = self.line
            col = self.column
            
            if char == '\n':
                self.advance()
                self.add_token(TokenType.NEWLINE, '\\n', line, col)
                continue
            
            if char == '/' and self.peek(1) == '/':
                self.skip_comment()
                continue
            
            if char == '"':
                string_value = self.read_string()
                self.add_token(TokenType.STRING, string_value, line, col)
                continue
            
            if char.isdigit():
                number_value = self.read_number()
                self.add_token(TokenType.NUMBER, number_value, line, col)
                continue
            
            if char.isalpha() or char == '_':
                identifier = self.read_identifier()
                keyword_type = KEYWORDS.get(
                    identifier.lower() if identifier.lower() in ('none', 'true', 'false') 
                    else identifier
                )
                if keyword_type:
                    self.add_token(keyword_type, identifier, line, col)
                else:
                    self.add_token(TokenType.IDENTIFIER, identifier, line, col)
                continue
            
            if char == '=' and self.peek(1) == '=':
                self.advance()
                self.advance()
                self.add_token(TokenType.EQUAL_EQUAL, '==', line, col)
                continue
            
            if char == '!' and self.peek(1) == '=':
                self.advance()
                self.advance()
                self.add_token(TokenType.NOT_EQUAL, '!=', line, col)
                continue
            
            if char == '<' and self.peek(1) == '=':
                self.advance()
                self.advance()
                self.add_token(TokenType.LESS_EQUAL, '<=', line, col)
                continue
            
            if char == '>' and self.peek(1) == '=':
                self.advance()
                self.advance()
                self.add_token(TokenType.GREATER_EQUAL, '>=', line, col)
                continue
            
            single_char_tokens = {
                '+': TokenType.PLUS,
                '-': TokenType.MINUS,
                '*': TokenType.STAR,
                '/': TokenType.SLASH,
                '%': TokenType.PERCENT,
                '=': TokenType.EQUAL,
                '<': TokenType.LESS,
                '>': TokenType.GREATER,
                '(': TokenType.LPAREN,
                ')': TokenType.RPAREN,
                ',': TokenType.COMMA,
                '.': TokenType.DOT,
                ':': TokenType.COLON,
            }
            
            if char in single_char_tokens:
                self.advance()
                self.add_token(single_char_tokens[char], char, line, col)
                continue
            
            self.advance()
        
        self.add_token(TokenType.EOF, '', self.line, self.column)
        return self.tokens

def format_token(token: Token) -> str:
    value_display = f"'{token.value}'" if token.value else "''"
    return f"{token.type.name:<20} {value_display:<25} {token.line:<8} {token.column:<8}"

def main():
    if len(sys.argv) != 2:
        print("Usage: python apex_tokenizer.py <filename.apex>")
        sys.exit(1)
    
    filepath = sys.argv[1]
    
    if not os.path.exists(filepath):
        print(f"Error: File {filepath} not found")
        sys.exit(1)
    
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            source = f.read()
        
        tokenizer = Tokenizer(source, filepath)
        tokens = tokenizer.tokenize()
        
        # Write to .cache folder
        cache_dir = Path('.cache')
        cache_dir.mkdir(exist_ok=True)
        
        base_name = Path(filepath).stem
        output_path = cache_dir / f"{base_name}.apexc"
        
        with open(output_path, 'w', encoding='utf-8') as f:
            for token in tokens:
                f.write(format_token(token) + '\n')
            
    except SyntaxError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()