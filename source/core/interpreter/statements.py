# source/core/interpreter/statements.py
from source.core.interpreter.environment import Return, Break, Continue
from source.core.interpreter.helpers import is_truthy
from source.core.interpreter.tables import Table
from typing import Any

class StatementsMixin:
    """Mixin for evaluating statements"""
    
    def eval_program(self, node: dict, env) -> Any:
        result = None
        for child in node.get('children', []):
            result = self.evaluate(child, env)
        return result

    def eval_block(self, node: dict, env) -> Any:
        result = None
        for child in node.get('children', []):
            result = self.evaluate(child, env)
        return result
    
    def eval_variable_decl(self, node: dict, env) -> Any:
        name = node.get('value', '')
        value = None
        if node.get('children'):
            value = self.evaluate(node['children'][0], env)
        env.define(name, value)
        return value
    
    def eval_expression_stmt(self, node: dict, env) -> Any:
        if node.get('children'):
            return self.evaluate(node['children'][0], env)
        return None
    
    def eval_if_stmt(self, node: dict, env) -> Any:
        condition = self.evaluate(node['children'][0], env)
        
        if is_truthy(condition):
            return self.evaluate(node['children'][1], env)
        
        elif_conditions = node.get('properties', {}).get('elif_conditions', [])
        elif_blocks = node.get('properties', {}).get('elif_blocks', [])
        for i, elif_cond in enumerate(elif_conditions):
            if is_truthy(self.evaluate(elif_cond, env)):
                return self.evaluate(elif_blocks[i], env)
        
        if len(node.get('children', [])) > 2:
            return self.evaluate(node['children'][2], env)
        
        return None
        
    def eval_while_stmt(self, node: dict, env) -> Any:
        result = None
        while is_truthy(self.evaluate(node['children'][0], env)):
            try:
                result = self.evaluate(node['children'][1], env)
            except Break:
                break
            except Continue:
                continue
        return result
    
    def eval_for_stmt(self, node: dict, env) -> Any:
        var_name = node.get('properties', {}).get('variable', '')
        children = node.get('children', [])
        
        if len(children) < 2:
            self.error("Invalid for statement", node)
        
        start = self.evaluate(children[0], env)
        end = self.evaluate(children[1], env)
        step = 1
        body_idx = 2
        
        if len(children) > 3:
            step = self.evaluate(children[2], env)
            body_idx = 3
        
        body = children[body_idx] if body_idx < len(children) else None
        
        if body is None:
            self.error("For loop has no body", node)
        
        result = None
        current = start
        while current <= end:
            env.define(var_name, current)
            try:
                result = self.evaluate(body, env)
            except Break:
                break
            except Continue:
                pass
            current += step
        
        return result
    
    def eval_for_in_stmt(self, node: dict, env) -> Any:
        var_name = node.get('properties', {}).get('variable', '')
        collection = self.evaluate(node['children'][0], env)
        
        result = None
        if isinstance(collection, Table):
            items = list(collection.items.values())
        elif isinstance(collection, list):
            items = collection
        else:
            self.error("Can only iterate over tables or lists", node)
        
        for item in items:
            env.define(var_name, item)
            try:
                result = self.evaluate(node['children'][1], env)
            except Break:
                break
            except Continue:
                pass
        
        return result
    
    def eval_break_stmt(self, node: dict, env) -> None:
        raise Break()
    
    def eval_continue_stmt(self, node: dict, env) -> None:
        raise Continue()
    
    def eval_try_stmt(self, node: dict, env) -> Any:
        children = node.get('children', [])
        
        try_body = children[0] if len(children) > 0 else None
        failure_body = children[1] if len(children) > 1 else None
        always_body = children[2] if len(children) > 2 else None
        
        result = None
        try:
            if try_body:
                result = self.evaluate(try_body, env)
        except Exception as e:
            if failure_body:
                env.define("error", str(e))
                result = self.evaluate(failure_body, env)
            else:
                raise
        finally:
            if always_body:
                try:
                    self.evaluate(always_body, env)
                except Exception:
                    pass
        
        return result