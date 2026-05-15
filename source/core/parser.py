# source/core/parser.py
import sys
import json
from pathlib import Path
from enum import Enum, auto
from dataclasses import dataclass, field
from typing import Optional

class ASTNodeType(Enum):
    PROGRAM = auto()
    IMPORT = auto()
    IMPORT_SPECIFIC = auto()
    VARIABLE_DECL = auto()
    FUNCTION_DECL = auto()
    EXPRESSION_STMT = auto()
    BLOCK = auto()
    IF_STMT = auto()
    WHILE_STMT = auto()
    FOR_STMT = auto()
    FOR_IN_STMT = auto()
    BREAK_STMT = auto()
    CONTINUE_STMT = auto()
    RETURN_STMT = auto()
    TRY_STMT = auto()
    BINARY_EXPR = auto()
    UNARY_EXPR = auto()
    CALL_EXPR = auto()
    MEMBER_EXPR = auto()
    ASSIGNMENT_EXPR = auto()
    LOGICAL_EXPR = auto()
    NUMBER_LITERAL = auto()
    STRING_LITERAL = auto()
    BOOLEAN_LITERAL = auto()
    NONE_LITERAL = auto()
    IDENTIFIER = auto()
    TABLE_LITERAL = auto()
    TABLE_ENTRY = auto()

@dataclass
class ASTNode:
    type: ASTNodeType
    value: any = None
    children: list = field(default_factory=list)
    properties: dict = field(default_factory=dict)
    line: int = 0
    column: int = 0
    
    def to_dict(self):
        result = {
            "type": self.type.name,
            "line": self.line,
            "column": self.column
        }
        if self.value is not None:
            result["value"] = self.value
        if self.properties:
            result["properties"] = {}
            for key, val in self.properties.items():
                if isinstance(val, ASTNode):
                    result["properties"][key] = val.to_dict()
                elif isinstance(val, list):
                    result["properties"][key] = [
                        item.to_dict() if isinstance(item, ASTNode) else item 
                        for item in val
                    ]
                else:
                    result["properties"][key] = val
        if self.children:
            result["children"] = [child.to_dict() if isinstance(child, ASTNode) else child for child in self.children]
        return result

class TokenType(Enum):
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
    NONE = auto()
    TRUE = auto()
    FALSE = auto()
    NUMBER = auto()
    STRING = auto()
    IDENTIFIER = auto()
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
    LPAREN = auto()
    RPAREN = auto()
    COMMA = auto()
    DOT = auto()
    COLON = auto()
    NEWLINE = auto()
    COMMENT = auto()
    EOF = auto()

@dataclass
class Token:
    type: TokenType
    value: str
    line: int
    column: int

class Parser:
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
        
    # ========== Statements ==========
        
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
                        TokenType.WHILE, TokenType.FOR, TokenType.IN, TokenType.TO,
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
        token = self.advance()
        var_name = self.expect(TokenType.IDENTIFIER, "Expected variable name").value
        
        if self.check(TokenType.IN):
            self.advance()
            collection = self.parse_expression()
            
            node = ASTNode(ASTNodeType.FOR_IN_STMT, line=token.line, column=token.column)
            node.properties['variable'] = var_name
            node.children.append(collection)
            
            body = self.parse_block(parent_column=token.column)
            node.children.append(body)
            return node
        
        if self.check(TokenType.EQUAL):
            self.advance()
            start = self.parse_expression()
            
            self.expect(TokenType.TO, "Expected 'to' in for loop")
            end = self.parse_expression()
            
            node = ASTNode(ASTNodeType.FOR_STMT, line=token.line, column=token.column)
            node.properties['variable'] = var_name
            node.children.append(start)
            node.children.append(end)
            
            if self.check(TokenType.RANGE):
                self.advance()
                step = self.parse_expression()
                node.children.append(step)
            
            body = self.parse_block(parent_column=token.column)
            node.children.append(body)
            return node
        
        self.syntax_error("Expected 'in' or '=' after for variable", token)
    
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
    
    # ========== Expressions ==========
    
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
                
                if self.check(TokenType.IDENTIFIER):
                    member = self.advance()
                    node = ASTNode(ASTNodeType.MEMBER_EXPR, 
                                  value=member.value,
                                  line=token.line, 
                                  column=token.column)
                    node.children.append(expr)
                    expr = node
                elif self.check(TokenType.NUMBER):
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
        
        if token.type == TokenType.IDENTIFIER:
            self.advance()
            return ASTNode(ASTNodeType.IDENTIFIER, 
                        value=token.value,
                        line=token.line, 
                        column=token.column)
        
        # Ключевые слова как идентификаторы (имена модулей, переменных)
        if token.type in (TokenType.IF, TokenType.ELIF, TokenType.ELSE,
                        TokenType.WHILE, TokenType.FOR, TokenType.IN, TokenType.TO,
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


# ============= Token File Reader =============

def read_tokens_from_file(filepath: str) -> list[Token]:
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