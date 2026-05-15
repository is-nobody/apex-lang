# source/core/parser.py
import sys
import json
from pathlib import Path
from typing import Optional

from source.core.parser.token import Token
from source.core.parser.token_type import TokenType
from source.core.parser.ast import ASTNode, ASTNodeType
from source.core.parser.parser_imports import ParserImportsMixin
from source.core.parser.parser_statements import ParserStatementsMixin
from source.core.parser.parser_expressions import ParserExpressionsMixin
from source.core.parser.parser_helpers import parse_file, read_tokens_from_file, fixup_tokens

class Parser(ParserImportsMixin, ParserStatementsMixin, ParserExpressionsMixin):
    def __init__(self, tokens: list[Token], filename: str = "<unknown>"):
        self.tokens = tokens
        self.filename = filename
        self.pos = 0
        self.errors = []

        self.functions = {}
        self.variables = {}
        self.in_loop = 0
        self.current_function = None
        
    def fatal_error(self, message: str, line: int = 0, column: int = 0):
        if line > 0:
            error_msg = f"Syntax Error in {self.filename} on line {line}: {message}"
        else:
            error_msg = f"Syntax Error in {self.filename}: {message}"
        self.errors.append(error_msg)
        raise SyntaxError(error_msg)
    
    def syntax_error(self, message: str, token: Token = None):
        if token is None:
            token = self.peek()
        error_msg = f"Syntax Error in {self.filename} on line {token.line}: {message}"
        self.errors.append(error_msg)
        raise SyntaxError(error_msg)
    
    def peek(self, offset: int = 0) -> Token:
        index = self.pos + offset
        if index >= len(self.tokens):
            return self.tokens[-1]
        return self.tokens[index]
    
    def advance(self) -> Token:
        token = self.peek()
        self.pos += 1
        return token
    
    def skip_newlines(self):
        while self.peek().type in (TokenType.NEWLINE, TokenType.COMMENT):
            self.advance()
    
    def expect(self, token_type: TokenType, message: str = "") -> Token:
        self.skip_newlines()
        token = self.peek()
        if token.type != token_type:
            error_msg = message or f"Expected {token_type.name}"
            self.syntax_error(error_msg, token)
        return self.advance()
    
    def match(self, *token_types: TokenType) -> Optional[Token]:
        self.skip_newlines()
        if self.peek().type in token_types:
            return self.advance()
        return None
    
    def check(self, *token_types: TokenType) -> bool:
        self.skip_newlines()
        return self.peek().type in token_types
    
    def _validate_top_level_expression(self, node: ASTNode):
        """Проверяет, что выражение на верхнем уровне имеет смысл"""
        if node.type == ASTNodeType.IDENTIFIER:
            self.fatal_error(
                f"Name '{node.value}' is not defined",
                node.line, node.column
            )
        elif node.type == ASTNodeType.ASSIGNMENT_EXPR:
            # Присваивание ок (x = 5), но проверим левую часть
            target = node.children[0]
            if target.type == ASTNodeType.IDENTIFIER:
                # Ок, создание переменной
                pass
            elif target.type == ASTNodeType.MEMBER_EXPR:
                # Ок, table.field = value
                pass
            else:
                self.fatal_error(
                    "Cannot assign to this expression",
                    target.line, target.column
                )
        elif node.type == ASTNodeType.CALL_EXPR:
            # Вызов функции ок, но проверим существование
            callee = node.children[0]
            if callee.type == ASTNodeType.IDENTIFIER:
                if callee.value not in self.functions:
                    self.fatal_error(
                        f"Function '{callee.value}' is not defined",
                        callee.line, callee.column
                    )
        elif node.type in (ASTNodeType.BINARY_EXPR, ASTNodeType.UNARY_EXPR):
            # Арифметика без присваивания бесполезна
            self.fatal_error(
                "Expression result is not used (did you mean to assign it?)",
                node.line, node.column
            )
    
    # ========== Entry Point ==========
        
    def parse(self) -> Optional[ASTNode]:
        program = ASTNode(ASTNodeType.PROGRAM, line=1, column=1)
        
        while self.peek().type != TokenType.EOF:
            self.skip_newlines()
            if self.peek().type == TokenType.EOF:
                break
            
            try:
                statement = self.parse_statement()
                if statement:
                    program.children.append(statement)
            except SyntaxError:
                self.synchronize()
        
        # show all errors at once
        if self.errors:
            for error in self.errors:
                print(error)
            return None
        
        return program
    
    def synchronize(self):
        """Skip tokens until we find a safe recovery point"""
        max_iterations = 1000
        iterations = 0
        
        while self.peek().type != TokenType.EOF:
            iterations += 1
            if iterations > max_iterations:
                # Принудительно пропускаем всё до конца
                while self.peek().type != TokenType.EOF:
                    self.advance()
                return
            
            token = self.peek()
            
            if token.type == TokenType.NEWLINE:
                self.advance()
                next_tok = self.peek()
                if next_tok.type == TokenType.EOF:
                    return
                if next_tok.type in (TokenType.FUNCTION, TokenType.IF,
                                    TokenType.WHILE, TokenType.FOR,
                                    TokenType.IMPORT, TokenType.TRY,
                                    TokenType.RETURN, TokenType.IDENTIFIER):
                    return
                if next_tok.column == 1:
                    return
                continue
            
            self.advance()


# ============= Main function for standalone execution =============

def main():
    cache_dir = Path('.cache')
    
    if not cache_dir.exists():
        print("Error: .cache directory not found. Run tokenizer first.")
        sys.exit(1)
    
    if len(sys.argv) > 1:
        filename = sys.argv[1]
        base_name = Path(filename).stem
        token_file = cache_dir / f"{base_name}.apexc"
        
        if not token_file.exists():
            print(f"Error: {token_file} not found. Run tokenizer first.")
            sys.exit(1)
        
        files = [(token_file, filename)]  # (technical_path, display_name)
    else:
        apex_files = list(Path('.').glob("*.apex"))
        files = []
        for apex_file in apex_files:
            token_file = cache_dir / f"{apex_file.stem}.apexc"
            if token_file.exists():
                files.append((token_file, str(apex_file)))
        
        if not files:
            print("No .apex files with cached tokens found.")
            sys.exit(1)
    
    success = 0
    errors = 0

    for filepath, display_name in files:
        ast = parse_file(str(filepath), display_name)
        
        if ast is not None:
            with open(filepath, 'w', encoding='utf-8') as f:
                json.dump(ast, f, indent=2, ensure_ascii=False)
            success += 1
        else:
            errors += 1

    if errors > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()