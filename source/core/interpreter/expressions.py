# source/core/interpreter/expressions.py
from source.core.interpreter.tables import Table
from source.libraries.builtin_module import BuiltinModule
from source.core.interpreter.environment import Environment
from typing import Any

class ExpressionsMixin:
    """Mixin for evaluating expressions"""
    
    def eval_binary_expr(self, node: dict, env) -> Any:
        left = self.evaluate(node['children'][0], env)
        right = self.evaluate(node['children'][1], env)
        op = node.get('value', '')
        
        if op == '+':
            if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                return left + right
            self.error(f"Cannot add {type(left).__name__} and {type(right).__name__}", node)
        
        if op == '-': return left - right
        if op == '*':
            if isinstance(left, str) and isinstance(right, int):
                return left * right
            return left * right
        if op == '/': return left / right
        if op == '%': return left % right
        if op == '==': return left == right
        if op == '!=': return left != right
        if op == '<': return left < right
        if op == '>': return left > right
        if op == '<=': return left <= right
        if op == '>=': return left >= right
        
        self.error(f"Unknown operator: {op}", node)
    
    def eval_unary_expr(self, node: dict, env) -> Any:
        from source.core.interpreter.helpers import is_truthy
        operand = self.evaluate(node['children'][0], env)
        op = node.get('value', '')
        
        if op == '-': return -operand
        if op == 'not': return not is_truthy(operand)
        
        self.error(f"Unknown unary operator: {op}", node)
    
    def eval_logical_expr(self, node: dict, env) -> Any:
        from source.core.interpreter.helpers import is_truthy
        left = self.evaluate(node['children'][0], env)
        op = node.get('value', '')
        
        if op == 'and':
            if not is_truthy(left):
                return left
            return self.evaluate(node['children'][1], env)
        
        if op == 'or':
            if is_truthy(left):
                return left
            return self.evaluate(node['children'][1], env)
        
        self.error(f"Unknown logical operator: {op}", node)
    
    def eval_assignment_expr(self, node: dict, env) -> Any:
        target = node['children'][0]
        value = self.evaluate(node['children'][1], env)
        
        if target.get('type') == 'IDENTIFIER':
            name = target.get('value', '')
            try:
                env.assign(name, value)
            except NameError:
                env.define(name, value)
            return value
        
        if target.get('type') == 'MEMBER_EXPR':
            obj = self.evaluate(target['children'][0], env)
            member = target.get('value', '')
            
            if isinstance(obj, Table):
                try:
                    index = int(member)
                    obj.set(index, value)
                    return value
                except ValueError:
                    pass
                obj.set(member, value)
                return value
            
            self.error(f"Cannot set property on {type(obj).__name__}", node)
        
        self.error("Invalid assignment target", node)
    
    def eval_member_expr(self, node: dict, env) -> Any:
        obj = self.evaluate(node['children'][0], env)
        member = node.get('value', '')
        
        if isinstance(obj, BuiltinModule):
            try:
                return obj.get(member)
            except AttributeError:
                self.error(f"Module '{obj.name}' has no member '{member}'", node)
        
        if isinstance(obj, Environment):
            try:
                return obj.get(member)
            except NameError:
                self.error(f"Module does not export '{member}'", node)
        
        if isinstance(obj, Table):
            try:
                index = int(member)
                return obj.get(index)
            except ValueError:
                pass
            return obj.get(member)
        
        if isinstance(obj, str):
            if member == 'length':
                return len(obj)
        
        if isinstance(obj, dict):
            if member in obj:
                return obj[member]
            try:
                index = int(member)
                if index in obj:
                    return obj[index]
            except ValueError:
                pass
        
        self.error(f"Cannot access member '{member}' of {type(obj).__name__}", node)

    def eval_range_expr(self, node: dict, env) -> Any:
        """Evaluate range(stop) or range(start, stop) or range(start, stop, step)"""
        from source.core.interpreter.tables import Table
        
        children = node.get('children', [])
        is_single_arg = node.get('properties', {}).get('single_arg', False)
        
        if is_single_arg:
            # range(stop) -> 0 to stop-1
            stop = self.evaluate(children[0], env)
            
            try:
                stop = int(stop)
            except (TypeError, ValueError):
                self.error("range argument must be a number", node)
            
            result = Table()
            index = 1
            current = 0
            while current < stop:
                result.set(index, current)
                current += 1
                index += 1
            
            return result
        
        # Two or three arguments
        if len(children) < 2:
            self.error("range requires at least 1 argument", node)
        
        start = self.evaluate(children[0], env)
        end = self.evaluate(children[1], env)
        
        step = 1
        if len(children) >= 3:
            step = self.evaluate(children[2], env)
        
        # Convert to numbers
        try:
            start = int(start)
            end = int(end)
            step = int(step)
        except (TypeError, ValueError):
            self.error("range arguments must be numbers", node)
        
        if step == 0:
            self.error("range step cannot be zero", node)
        
        result = Table()
        index = 1
        
        if step > 0:
            current = start
            while current < end:
                result.set(index, current)
                current += step
                index += 1
        else:
            current = start
            while current > end:
                result.set(index, current)
                current += step
                index += 1
        
        return result