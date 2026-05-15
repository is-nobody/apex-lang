# source/core/interpreter/evaluator.py
from typing import Any, Optional

class EvaluatorMixin:
    """Main evaluate method with dispatch"""
    
    def evaluate(self, node: dict, env: Optional[Any] = None) -> Any:
        if env is None:
            env = self.global_env
        
        if 'line' in node:
            self.current_line = node['line']
        
        node_type = node.get('type', '')
        
        handlers = {
            'PROGRAM': self.eval_program,
            'IMPORT': self.eval_import,
            'IMPORT_SPECIFIC': self.eval_import_specific,
            'VARIABLE_DECL': self.eval_variable_decl,
            'FUNCTION_DECL': self.eval_function_decl,
            'EXPRESSION_STMT': self.eval_expression_stmt,
            'BLOCK': self.eval_block,
            'IF_STMT': self.eval_if_stmt,
            'WHILE_STMT': self.eval_while_stmt,
            'FOR_STMT': self.eval_for_stmt,
            'FOR_IN_STMT': self.eval_for_in_stmt,
            'BREAK_STMT': self.eval_break_stmt,
            'CONTINUE_STMT': self.eval_continue_stmt,
            'RETURN_STMT': self.eval_return_stmt,
            'TRY_STMT': self.eval_try_stmt,
            'BINARY_EXPR': self.eval_binary_expr,
            'UNARY_EXPR': self.eval_unary_expr,
            'CALL_EXPR': self.eval_call_expr,
            'MEMBER_EXPR': self.eval_member_expr,
            'ASSIGNMENT_EXPR': self.eval_assignment_expr,
            'LOGICAL_EXPR': self.eval_logical_expr,
            'NUMBER_LITERAL': self.eval_number_literal,
            'STRING_LITERAL': self.eval_string_literal,
            'BOOLEAN_LITERAL': self.eval_boolean_literal,
            'NONE_LITERAL': self.eval_none_literal,
            'IDENTIFIER': self.eval_identifier,
            'TABLE_LITERAL': self.eval_table_literal,
            'TABLE_ENTRY': self.eval_table_entry,
        }
        
        handler = handlers.get(node_type)
        if handler:
            return handler(node, env)
        
        self.error(f"Unknown node type: {node_type}", node)