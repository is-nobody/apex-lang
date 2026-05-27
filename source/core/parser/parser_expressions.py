# source/core/parser_expressions.py
from source.core.parser.ast import ASTNode, ASTNodeType
from source.core.parser.token_type import TokenType

class ParserExpressionsMixin:
    """Mixin for parsing expressions"""
    
    def parse_expression(self) -> ASTNode:
        return self.parse_assignment()
    
    def parse_assignment(self) -> ASTNode:
        left = self.parse_logical_or()
        
        if self.check(TokenType.EQUAL):
            token = self.advance()
            right = self.parse_assignment()
            
            node = ASTNode(ASTNodeType.ASSIGNMENT_EXPR, 
                          value='=',
                          line=token.line, 
                          column=token.column)
            node.children.append(left)
            node.children.append(right)
            return node
        
        return left
    
    def parse_logical_or(self) -> ASTNode:
        left = self.parse_logical_and()
        
        while self.check(TokenType.OR):
            token = self.advance()
            right = self.parse_logical_and()
            
            node = ASTNode(ASTNodeType.LOGICAL_EXPR, 
                          value='or',
                          line=token.line, 
                          column=token.column)
            node.children.append(left)
            node.children.append(right)
            left = node
        
        return left
    
    def parse_logical_and(self) -> ASTNode:
        left = self.parse_equality()
        
        while self.check(TokenType.AND):
            token = self.advance()
            right = self.parse_equality()
            
            node = ASTNode(ASTNodeType.LOGICAL_EXPR, 
                          value='and',
                          line=token.line, 
                          column=token.column)
            node.children.append(left)
            node.children.append(right)
            left = node
        
        return left
    
    def parse_equality(self) -> ASTNode:
        left = self.parse_comparison()
        
        while self.check(TokenType.EQUAL_EQUAL, TokenType.NOT_EQUAL):
            token = self.advance()
            right = self.parse_comparison()
            
            node = ASTNode(ASTNodeType.BINARY_EXPR, 
                          value=token.value,
                          line=token.line, 
                          column=token.column)
            node.children.append(left)
            node.children.append(right)
            left = node
        
        return left
    
    def parse_comparison(self) -> ASTNode:
        left = self.parse_addition()
        
        while self.check(TokenType.LESS, TokenType.GREATER, 
                        TokenType.LESS_EQUAL, TokenType.GREATER_EQUAL):
            token = self.advance()
            right = self.parse_addition()
            
            node = ASTNode(ASTNodeType.BINARY_EXPR, 
                          value=token.value,
                          line=token.line, 
                          column=token.column)
            node.children.append(left)
            node.children.append(right)
            left = node
        
        return left
    
    def parse_addition(self) -> ASTNode:
        left = self.parse_multiplication()
        
        while self.check(TokenType.PLUS, TokenType.MINUS):
            token = self.advance()
            right = self.parse_multiplication()
            
            # Check for string + number (will fail at runtime)
            if token.value == '+':
                left_type = self._get_expression_type(left)
                right_type = self._get_expression_type(right)
                if (left_type == 'string' and right_type == 'number') or \
                   (left_type == 'number' and right_type == 'string'):
                    self.fatal_error(
                        "Cannot add string and number directly",
                        token.line, token.column
                    )
            
            node = ASTNode(ASTNodeType.BINARY_EXPR, 
                          value=token.value,
                          line=token.line, 
                          column=token.column)
            node.children.append(left)
            node.children.append(right)
            left = node
        
        return left
    
    def parse_multiplication(self) -> ASTNode:
        left = self.parse_unary()
        
        while self.check(TokenType.STAR, TokenType.SLASH, TokenType.PERCENT):
            token = self.advance()
            right = self.parse_unary()
            
            node = ASTNode(ASTNodeType.BINARY_EXPR, 
                          value=token.value,
                          line=token.line, 
                          column=token.column)
            node.children.append(left)
            node.children.append(right)
            left = node
        
        return left
    
    def parse_unary(self) -> ASTNode:
        if self.check(TokenType.MINUS, TokenType.NOT):
            token = self.advance()
            operand = self.parse_unary()
            
            node = ASTNode(ASTNodeType.UNARY_EXPR, 
                          value=token.value,
                          line=token.line, 
                          column=token.column)
            node.children.append(operand)
            return node
        
        return self.parse_call()
    
    def parse_call(self) -> ASTNode:
        expr = self.parse_primary()
        
        while True:
            if self.check(TokenType.DOT):
                token = self.advance()
                
                # Все что может быть идентификатором после точки
                if self.check(TokenType.IDENTIFIER, TokenType.NUMBER,
                            TokenType.NONE, TokenType.TRUE, TokenType.FALSE,
                            TokenType.IF, TokenType.ELIF, TokenType.ELSE,
                            TokenType.WHILE, TokenType.FOR, TokenType.IN,
                            TokenType.RANGE, TokenType.BREAK, TokenType.CONTINUE,
                            TokenType.RETURN, TokenType.IMPORT, TokenType.TRY,
                            TokenType.FAILURE, TokenType.ALWAYS,
                            TokenType.AND, TokenType.OR, TokenType.NOT,
                            TokenType.FUNCTION):
                    member = self.advance()
                    node = ASTNode(ASTNodeType.MEMBER_EXPR, 
                                value=member.value,
                                line=token.line, 
                                column=token.column)
                    node.children.append(expr)
                    expr = node
                else:
                    self.syntax_error("Expected identifier or number after '.'", token)
            
            elif self.check(TokenType.LPAREN):
                token = self.advance()
                
                node = ASTNode(ASTNodeType.CALL_EXPR, 
                              line=token.line, 
                              column=token.column)
                node.children.append(expr)
                
                if not self.check(TokenType.RPAREN):
                    args = self.parse_arguments()
                    node.children.extend(args)
                
                self.expect(TokenType.RPAREN, "Expected ')' after arguments")
                
                # Check function call arity
                if expr.type == ASTNodeType.IDENTIFIER and expr.value in self.functions:
                    func_info = self.functions[expr.value]
                    arg_count = len(node.children) - 1  # minus callee
                    if arg_count != func_info['param_count']:
                        self.fatal_error(
                            f"Function '{expr.value}' expects {func_info['param_count']} arguments, got {arg_count}",
                            node.line, node.column
                        )
                
                expr = node
            
            else:
                break
        
        return expr
    
    def parse_primary(self) -> ASTNode:
        self.skip_newlines()
        token = self.peek()
        
        if token.type == TokenType.NUMBER:
            self.advance()
            return ASTNode(ASTNodeType.NUMBER_LITERAL, 
                        value=float(token.value) if '.' in token.value else int(token.value),
                        line=token.line, 
                        column=token.column)
        
        if token.type == TokenType.STRING:
            self.advance()
            return ASTNode(ASTNodeType.STRING_LITERAL, 
                        value=token.value,
                        line=token.line, 
                        column=token.column)
        
        if token.type == TokenType.TRUE:
            self.advance()
            return ASTNode(ASTNodeType.BOOLEAN_LITERAL, 
                        value=True,
                        line=token.line, 
                        column=token.column)
        
        if token.type == TokenType.FALSE:
            self.advance()
            return ASTNode(ASTNodeType.BOOLEAN_LITERAL, 
                        value=False,
                        line=token.line, 
                        column=token.column)
        
        if token.type == TokenType.NONE:
            self.advance()
            return ASTNode(ASTNodeType.NONE_LITERAL, 
                        line=token.line, 
                        column=token.column)
            
        if token.type == TokenType.RANGE:  # ADD THIS BLOCK
            return self.parse_range_expr()

        if token.type == TokenType.IDENTIFIER:
            self.advance()
            return ASTNode(ASTNodeType.IDENTIFIER, 
                        value=token.value,
                        line=token.line, 
                        column=token.column)
        
        # Ключевые слова как идентификаторы (имена модулей, переменных)
        if token.type in (TokenType.IF, TokenType.ELIF, TokenType.ELSE,
                        TokenType.WHILE, TokenType.FOR, TokenType.IN,
                        TokenType.RANGE, TokenType.BREAK, TokenType.CONTINUE,
                        TokenType.RETURN, TokenType.IMPORT, TokenType.TRY,
                        TokenType.FAILURE, TokenType.ALWAYS,
                        TokenType.AND, TokenType.OR, TokenType.NOT,
                        TokenType.FUNCTION):
            self.advance()
            return ASTNode(ASTNodeType.IDENTIFIER, 
                        value=token.value,
                        line=token.line, 
                        column=token.column)
        
        if token.type == TokenType.LPAREN:
            return self.parse_table_or_group()
        
        self.syntax_error(f"Unexpected token '{token.value}'", token)
    
    def parse_table_or_group(self) -> ASTNode:
        token = self.advance()
        
        is_table = False
        
        if self.check(TokenType.RPAREN):
            self.advance()
            return ASTNode(ASTNodeType.TABLE_LITERAL, line=token.line, column=token.column)
        
        peek_pos = self.pos
        paren_depth = 1
        while paren_depth > 0 and peek_pos < len(self.tokens):
            t = self.tokens[peek_pos]
            peek_pos += 1
            if t.type == TokenType.LPAREN:
                paren_depth += 1
            elif t.type == TokenType.RPAREN:
                paren_depth -= 1
            elif t.type == TokenType.COMMA and paren_depth == 1:
                is_table = True
                break
            elif t.type == TokenType.EQUAL and paren_depth == 1:
                prev = self.tokens[peek_pos - 2]
                if prev.type == TokenType.IDENTIFIER:
                    is_table = True
                    break
        
        if is_table:
            return self.parse_table_contents(token)
        else:
            expr = self.parse_expression()
            self.expect(TokenType.RPAREN, "Expected ')' after expression")
            return expr
    
    def parse_table_contents(self, open_token) -> ASTNode:
        node = ASTNode(ASTNodeType.TABLE_LITERAL, line=open_token.line, column=open_token.column)
        
        if self.check(TokenType.RPAREN):
            self.advance()
            return node
        
        while True:
            self.skip_newlines()
            
            if self.check(TokenType.RPAREN):
                break
            
            if self.check(TokenType.IDENTIFIER):
                saved = self.pos
                self.advance()
                self.skip_newlines()
                if self.check(TokenType.EQUAL):
                    key = self.tokens[saved].value
                    self.advance()
                    value = self.parse_expression()
                    
                    entry = ASTNode(ASTNodeType.TABLE_ENTRY)
                    entry.properties['key'] = key
                    entry.children.append(value)
                    node.children.append(entry)
                    
                    if self.check(TokenType.COMMA):
                        self.advance()
                        continue
                    if self.check(TokenType.RPAREN):
                        break
                    continue
                else:
                    self.pos = saved
            
            value = self.parse_expression()
            
            entry = ASTNode(ASTNodeType.TABLE_ENTRY)
            entry.children.append(value)
            node.children.append(entry)
            
            if self.check(TokenType.RPAREN):
                break
            
            if self.check(TokenType.COMMA):
                self.advance()
                continue
            
            break
        
        self.expect(TokenType.RPAREN, "Expected ')' after table")
        return node
    
    def parse_arguments(self) -> list[ASTNode]:
        args = []
        args.append(self.parse_expression())
        
        while self.check(TokenType.COMMA):
            self.advance()
            args.append(self.parse_expression())
        
        return args
    
    def _get_expression_type(self, node: ASTNode) -> str:
        if node.type == ASTNodeType.STRING_LITERAL:
            return 'string'
        elif node.type == ASTNodeType.NUMBER_LITERAL:
            return 'number'
        elif node.type == ASTNodeType.BOOLEAN_LITERAL:
            return 'boolean'
        elif node.type == ASTNodeType.NONE_LITERAL:
            return 'none'
        elif node.type == ASTNodeType.IDENTIFIER:
            return 'unknown'
        elif node.type == ASTNodeType.BINARY_EXPR:
            if node.value in ('+', '-', '*', '/', '%'):
                return 'number'
            elif node.value in ('==', '!=', '<', '>', '<=', '>='):
                return 'boolean'
        elif node.type == ASTNodeType.CALL_EXPR:
            return 'unknown'
        return 'unknown'
    
    def parse_range_expr(self) -> ASTNode:
        """Parse range(stop) or range(start, stop) or range(start, stop, step)"""
        token = self.advance()  # consume 'range'
        self.expect(TokenType.LPAREN, "Expected '(' after range")
        
        # Parse first argument
        first_arg = self.parse_expression()
        
        # Check if there's a comma (2 or 3 arguments)
        if self.check(TokenType.COMMA):
            self.advance()  # consume comma
            second_arg = self.parse_expression()
            
            # Check for step (3 arguments)
            step = None
            if self.check(TokenType.COMMA):
                self.advance()  # consume comma
                step = self.parse_expression()
            
            self.expect(TokenType.RPAREN, "Expected ')' after range arguments")
            
            node = ASTNode(ASTNodeType.RANGE_EXPR, line=token.line, column=token.column)
            node.children.append(first_arg)  # start
            node.children.append(second_arg)  # end
            if step:
                node.children.append(step)  # step
            
            return node
        else:
            # Single argument: range(stop) -> 0 to stop-1
            self.expect(TokenType.RPAREN, "Expected ')' after range argument")
            
            node = ASTNode(ASTNodeType.RANGE_EXPR, line=token.line, column=token.column)
            # Special marker for single argument
            node.properties['single_arg'] = True
            node.children.append(first_arg)  # stop
            return node