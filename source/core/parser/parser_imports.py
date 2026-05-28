# source/core/parser_imports.py
from source.core.parser.ast import ASTNode, ASTNodeType
from source.core.parser.token_type import TokenType

class ParserImportsMixin:
    def parse_import(self) -> ASTNode:
        token = self.advance()
        
        module_parts = []
        
        if self.check(TokenType.IDENTIFIER, 
                    TokenType.IF, TokenType.ELIF, TokenType.ELSE,
                    TokenType.WHILE, TokenType.FOR, TokenType.IN,
                    TokenType.RANGE, TokenType.BREAK, TokenType.CONTINUE, 
                    TokenType.RETURN, TokenType.IMPORT, TokenType.TRY, 
                    TokenType.FAILURE, TokenType.ALWAYS,
                    TokenType.AND, TokenType.OR, TokenType.NOT, 
                    TokenType.TRUE, TokenType.FALSE,
                    TokenType.FUNCTION):
            module_parts.append(self.advance().value)
        else:
            self.syntax_error("Expected module name", self.peek())
        
        while self.check(TokenType.DOT):
            self.advance()
            if self.check(TokenType.IDENTIFIER,
                        TokenType.IF, TokenType.ELIF, TokenType.ELSE,
                        TokenType.WHILE, TokenType.FOR, TokenType.IN,
                        TokenType.RANGE, TokenType.BREAK, TokenType.CONTINUE,
                        TokenType.RETURN, TokenType.IMPORT, TokenType.TRY,
                        TokenType.FAILURE, TokenType.ALWAYS,
                        TokenType.AND, TokenType.OR, TokenType.NOT,
                        TokenType.TRUE, TokenType.FALSE,
                        TokenType.FUNCTION):
                module_parts.append(self.advance().value)
            else:
                self.syntax_error("Expected module name after '.'", self.peek())
    
        module_path = '.'.join(module_parts)
        
        return ASTNode(ASTNodeType.IMPORT,
                    value=module_path,
                    line=token.line,
                    column=token.column)