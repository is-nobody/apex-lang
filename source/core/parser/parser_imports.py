# source/core/parser_imports.py
from typing import Optional
from source.core.parser.ast import ASTNode, ASTNodeType
from source.core.parser.token_type import TokenType

class ParserImportsMixin:
    """Mixin for parsing import statements"""
    
    def parse_import(self) -> ASTNode:
        token = self.advance()
        
        module_parts = []
        
        # Разрешаем ключевые слова как имена модулей и файлов
        if self.check(TokenType.IDENTIFIER, 
                    TokenType.IF, TokenType.ELIF, TokenType.ELSE,
                    TokenType.WHILE, TokenType.FOR, TokenType.IN, TokenType.TO,
                    TokenType.RANGE, TokenType.BREAK, TokenType.CONTINUE, 
                    TokenType.RETURN, TokenType.IMPORT, TokenType.TRY, 
                    TokenType.FAILURE, TokenType.ALWAYS,
                    TokenType.AND, TokenType.OR, TokenType.NOT, 
                    TokenType.NONE, TokenType.TRUE, TokenType.FALSE,
                    TokenType.FUNCTION):
            module_parts.append(self.advance().value)
        else:
            self.syntax_error("Expected module name", self.peek())
        
        while self.check(TokenType.DOT):
            self.advance()
            if self.check(TokenType.IDENTIFIER,
                        TokenType.IF, TokenType.ELIF, TokenType.ELSE,
                        TokenType.WHILE, TokenType.FOR, TokenType.IN, TokenType.TO,
                        TokenType.RANGE, TokenType.BREAK, TokenType.CONTINUE,
                        TokenType.RETURN, TokenType.IMPORT, TokenType.TRY,
                        TokenType.FAILURE, TokenType.ALWAYS,
                        TokenType.AND, TokenType.OR, TokenType.NOT,
                        TokenType.NONE, TokenType.TRUE, TokenType.FALSE,
                        TokenType.FUNCTION):
                module_parts.append(self.advance().value)
            else:
                self.syntax_error("Expected module name after '.'", self.peek())
        
        module_path = '.'.join(module_parts)
        
        if self.check(TokenType.COLON):
            self.advance()
            node = ASTNode(ASTNodeType.IMPORT_SPECIFIC,
                        value=module_path,
                        line=token.line,
                        column=token.column)
            
            imports = []
            if self.check(TokenType.IDENTIFIER,
                        TokenType.IF, TokenType.ELIF, TokenType.ELSE,
                        TokenType.WHILE, TokenType.FOR, TokenType.IN, TokenType.TO,
                        TokenType.RANGE, TokenType.BREAK, TokenType.CONTINUE,
                        TokenType.RETURN, TokenType.IMPORT, TokenType.TRY,
                        TokenType.FAILURE, TokenType.ALWAYS,
                        TokenType.AND, TokenType.OR, TokenType.NOT,
                        TokenType.NONE, TokenType.TRUE, TokenType.FALSE,
                        TokenType.FUNCTION):
                imports.append(self.advance().value)
            else:
                self.syntax_error("Expected item name", self.peek())
            
            while self.check(TokenType.COMMA):
                self.advance()
                if self.check(TokenType.IDENTIFIER,
                            TokenType.IF, TokenType.ELIF, TokenType.ELSE,
                            TokenType.WHILE, TokenType.FOR, TokenType.IN, TokenType.TO,
                            TokenType.RANGE, TokenType.BREAK, TokenType.CONTINUE,
                            TokenType.RETURN, TokenType.IMPORT, TokenType.TRY,
                            TokenType.FAILURE, TokenType.ALWAYS,
                            TokenType.AND, TokenType.OR, TokenType.NOT,
                            TokenType.NONE, TokenType.TRUE, TokenType.FALSE,
                            TokenType.FUNCTION):
                    imports.append(self.advance().value)
                else:
                    self.syntax_error("Expected item name after ','", self.peek())
            
            node.properties['imports'] = imports
            return node
        
        return ASTNode(ASTNodeType.IMPORT,
                    value=module_path,
                    line=token.line,
                    column=token.column)