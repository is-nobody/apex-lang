# source/core/parser_helpers.py
from pathlib import Path
from typing import Optional
from source.core.parser.token import Token
from source.core.parser.token_type import TokenType

def read_tokens_from_file(filepath: str) -> list[Token]:
    """Read tokens from .apexc file"""
    tokens = []
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    if '---AST---' in content:
        content = content.split('---AST---')[0]
    
    lines = content.split('\n')
    
    for line in lines:
        if not line.strip():
            continue
        
        first_space = line.find(' ')
        if first_space == -1:
            continue
        
        type_name = line[:first_space].strip()
        rest = line[first_space:].strip()
        
        if type_name == 'EOF':
            token_line = 0
            token_col = 0
            parts = rest.split()
            if len(parts) >= 2:
                try:
                    token_line = int(parts[0])
                    token_col = int(parts[1])
                except (ValueError, IndexError):
                    pass
            tokens.append(Token(TokenType.EOF, '', token_line, token_col))
            continue
        
        try:
            token_type = TokenType[type_name]
        except KeyError:
            continue
        
        value = ""
        token_line = 0
        token_col = 0
        
        if token_type == TokenType.COMMENT:
            quote_start = rest.find("'")
            if quote_start != -1:
                quote_end = rest.rfind("'")
                if quote_end > quote_start:
                    value = rest[quote_start + 1:quote_end]
                after = rest[quote_end + 1:].strip().split()
                if len(after) >= 2:
                    try:
                        token_line = int(after[0])
                        token_col = int(after[1])
                    except ValueError:
                        pass
        elif token_type == TokenType.NEWLINE:
            value = '\n'
            parts = rest.split()
            if len(parts) >= 2:
                try:
                    token_line = int(parts[0])
                    token_col = int(parts[1])
                except ValueError:
                    pass
        else:
            quote_start = rest.find("'")
            if quote_start != -1:
                quote_end = rest.rfind("'")
                if quote_end > quote_start:
                    value = rest[quote_start + 1:quote_end]
                
                after = rest[quote_end + 1:].strip().split()
                if len(after) >= 2:
                    try:
                        token_line = int(after[0])
                        token_col = int(after[1])
                    except ValueError:
                        pass
            else:
                parts = rest.split(None, 1)
                if parts:
                    value = parts[0]
                    if len(parts) > 1:
                        nums = parts[1].split()
                        if len(nums) >= 2:
                            try:
                                token_line = int(nums[0])
                                token_col = int(nums[1])
                            except ValueError:
                                pass
        
        tokens.append(Token(token_type, value, token_line, token_col))
    
    return tokens

def fixup_tokens(tokens: list[Token]) -> list[Token]:
    """Remove duplicate newlines"""
    fixed = []
    prev_was_newline = False
    
    for token in tokens:
        if token.type == TokenType.NEWLINE:
            if not prev_was_newline:
                fixed.append(token)
            prev_was_newline = True
        else:
            fixed.append(token)
            prev_was_newline = False
    
    return fixed

def parse_file(filepath: str, source_name: str = None) -> Optional[dict]:
    """Parse a .apexc file and return AST as dict"""
    from source.core.parser_main import Parser
    
    # Use source filename for error messages, not .apexc
    display_name = source_name or filepath
    
    tokens = read_tokens_from_file(filepath)
    
    if not tokens:
        print(f"Error in {display_name}: No tokens found")
        return None
    
    tokens = fixup_tokens(tokens)
    
    if tokens[-1].type != TokenType.EOF:
        tokens.append(Token(TokenType.EOF, '', 0, 0))
    
    # Pass display_name to parser for error messages
    parser = Parser(tokens, display_name)
    
    try:
        ast = parser.parse()
    except SyntaxError:
        return None

    return ast.to_dict()