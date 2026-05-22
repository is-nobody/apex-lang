# source/core/parser_statements.py
from typing import Optional
from source.core.parser.ast import ASTNode, ASTNodeType
from source.core.parser.token_type import TokenType

class ParserStatementsMixin:
    """Mixin for parsing statements"""
    
    def parse_statement(self) -> Optional[ASTNode]:
        self.skip_newlines()
        token = self.peek()
        
        if token.type == TokenType.EOF:
            return None
        
        # Проверяем ключевые слова как потенциальные идентификаторы
        # (могут быть именами модулей после import)
        next_tok = self.peek(1)
        
        # IMPORT should always be handled as a statement, not as an identifier
        if token.type == TokenType.IMPORT:
            return self.parse_import()
        
        if token.type in (TokenType.IF, TokenType.ELIF, TokenType.ELSE,
                        TokenType.WHILE, TokenType.FOR, TokenType.IN,
                        TokenType.RANGE, TokenType.BREAK, TokenType.CONTINUE,
                        TokenType.RETURN, TokenType.TRY,
                        TokenType.FAILURE, TokenType.ALWAYS,
                        TokenType.AND, TokenType.OR, TokenType.NOT,
                        TokenType.FUNCTION):
            # Если после ключевого слова идёт точка или присваивание — это идентификатор
            if next_tok.type in (TokenType.DOT, TokenType.EQUAL, TokenType.LPAREN):
                expr = self.parse_expression()
                self._validate_top_level_expression(expr)
                return expr
        
        if token.type == TokenType.FUNCTION:
            return self.parse_function()
        
        if token.type == TokenType.IF:
            return self.parse_if()
        
        if token.type == TokenType.WHILE:
            self.in_loop += 1
            result = self.parse_while()
            self.in_loop -= 1
            return result
        
        if token.type == TokenType.FOR:
            self.in_loop += 1
            result = self.parse_for()
            self.in_loop -= 1
            return result
        
        if token.type == TokenType.BREAK:
            token = self.advance()
            if self.in_loop == 0:
                self.fatal_error("'break' outside of loop", token.line, token.column)
            return ASTNode(ASTNodeType.BREAK_STMT, line=token.line, column=token.column)
        
        if token.type == TokenType.CONTINUE:
            token = self.advance()
            if self.in_loop == 0:
                self.fatal_error("'continue' outside of loop", token.line, token.column)
            return ASTNode(ASTNodeType.CONTINUE_STMT, line=token.line, column=token.column)
        
        if token.type == TokenType.RETURN:
            return self.parse_return()
        
        if token.type == TokenType.TRY:
            return self.parse_try()
        
        expr = self.parse_expression()
        self._validate_top_level_expression(expr)
        return expr
    
    def parse_block(self, parent_column: int = 0) -> ASTNode:
        block = ASTNode(ASTNodeType.BLOCK)
        
        while True:
            self.skip_newlines()
            
            if self.peek().type == TokenType.EOF:
                break
            
            if self.peek().type in (TokenType.ELIF, TokenType.ELSE, 
                                    TokenType.FAILURE, TokenType.ALWAYS):
                break
            
            if parent_column > 0:
                tok = self.peek()
                if tok.column <= parent_column:
                    if tok.type in (TokenType.IF, TokenType.WHILE, TokenType.FOR,
                                TokenType.FUNCTION, TokenType.IMPORT, TokenType.TRY,
                                TokenType.BREAK, TokenType.CONTINUE, TokenType.RETURN):
                        break
                    if tok.type == TokenType.IDENTIFIER:
                        next_tok = self.peek(1)
                        if next_tok.type in (TokenType.EQUAL, TokenType.LPAREN, TokenType.DOT):
                            break
            
            try:
                stmt = self.parse_statement()
                if stmt:
                    block.children.append(stmt)
                else:
                    break
            except SyntaxError:
                self.synchronize()
        
        return block
    
    def parse_if(self) -> ASTNode:
        token = self.advance()
        condition = self.parse_expression()
        
        node = ASTNode(ASTNodeType.IF_STMT, line=token.line, column=token.column)
        node.children.append(condition)
        
        then_block = self.parse_block(parent_column=token.column)
        node.children.append(then_block)
        
        while self.check(TokenType.ELIF):
            elif_token = self.advance()
            elif_condition = self.parse_expression()
            elif_block = self.parse_block(parent_column=elif_token.column)
            
            if 'elif_conditions' not in node.properties:
                node.properties['elif_conditions'] = []
                node.properties['elif_blocks'] = []
            node.properties['elif_conditions'].append(elif_condition)
            node.properties['elif_blocks'].append(elif_block)
        
        if self.check(TokenType.ELSE):
            else_token = self.advance()
            else_block = self.parse_block(parent_column=else_token.column)
            node.children.append(else_block)
        
        return node
    
    def parse_while(self) -> ASTNode:
        token = self.advance()
        condition = self.parse_expression()
        
        node = ASTNode(ASTNodeType.WHILE_STMT, line=token.line, column=token.column)
        node.children.append(condition)
        
        body = self.parse_block(parent_column=token.column)
        node.children.append(body)
        
        return node

    def parse_for(self) -> ASTNode:
        """Parse for statement: for i in range(1, 10)"""
        token = self.advance()
        var_name = self.expect(TokenType.IDENTIFIER, "Expected variable name").value
        
        self.expect(TokenType.IN, "Expected 'in' after for variable")
        
        # Parse collection - could be range expression or table
        collection = self.parse_expression()
        
        node = ASTNode(ASTNodeType.FOR_IN_STMT, line=token.line, column=token.column)
        node.properties['variable'] = var_name
        node.children.append(collection)
        
        body = self.parse_block(parent_column=token.column)
        node.children.append(body)
        
        return node

    def parse_return(self) -> ASTNode:
        token = self.advance()
        node = ASTNode(ASTNodeType.RETURN_STMT, line=token.line, column=token.column)
        
        if self.current_function is None:
            self.fatal_error("'return' outside of function", token.line, token.column)
        
        if not self.check(TokenType.NEWLINE) and not self.check(TokenType.EOF):
            value = self.parse_expression()
            node.children.append(value)
        
        return node
    
    def parse_try(self) -> ASTNode:
        token = self.advance()
        
        node = ASTNode(ASTNodeType.TRY_STMT, line=token.line, column=token.column)
        
        try_block = self.parse_block(parent_column=token.column)
        node.children.append(try_block)
        
        if self.check(TokenType.FAILURE):
            failure_token = self.advance()
            failure_block = self.parse_block(parent_column=failure_token.column)
            node.children.append(failure_block)
        
        if self.check(TokenType.ALWAYS):
            always_token = self.advance()
            always_block = self.parse_block(parent_column=always_token.column)
            node.children.append(always_block)
        
        return node
    
    def parse_function(self) -> ASTNode:
        token = self.advance()
        name = self.expect(TokenType.IDENTIFIER, "Expected function name").value
        
        # Check duplicate function definition
        if name in self.functions:
            self.fatal_error(
                f"Function '{name}' already defined on line {self.functions[name]['line']}",
                token.line, token.column
            )
        
        self.expect(TokenType.LPAREN, "Expected '(' after function name")
        
        params = []
        param_names = set()
        if not self.check(TokenType.RPAREN):
            param = self.expect(TokenType.IDENTIFIER, "Expected parameter name").value
            
            # Check duplicate parameter
            if param in param_names:
                self.fatal_error(
                    f"Duplicate parameter '{param}' in function '{name}'",
                    self.peek().line, self.peek().column
                )
            param_names.add(param)
            params.append(param)
            
            while self.check(TokenType.COMMA):
                self.advance()
                param = self.expect(TokenType.IDENTIFIER, "Expected parameter name").value
                
                if param in param_names:
                    self.fatal_error(
                        f"Duplicate parameter '{param}' in function '{name}'",
                        self.peek().line, self.peek().column
                    )
                param_names.add(param)
                params.append(param)
        
        self.expect(TokenType.RPAREN, "Expected ')' after parameters")
        
        self.functions[name] = {
            'params': params,
            'param_count': len(params),
            'line': token.line
        }
        
        node = ASTNode(ASTNodeType.FUNCTION_DECL, 
                    value=name,
                    line=token.line, 
                    column=token.column)
        node.properties['params'] = params
        
        # Save context
        old_function = self.current_function
        self.current_function = name
        
        body = self.parse_block(parent_column=token.column)
        node.children.append(body)
        
        # Restore context
        self.current_function = old_function
        
        return node