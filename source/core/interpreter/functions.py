# source/core/interpreter/functions.py
from source.core.interpreter.environment import Environment, Return
from source.core.interpreter.helpers import add_filename_to_functions
from typing import Any

class FunctionsMixin:
    """Mixin for evaluating function declarations and calls"""
    
    def eval_function_decl(self, node: dict, env) -> None:
        name = node.get('value', '')
        func_info = {
            'node': node,
            'closure': env
        }
        self.functions[name] = func_info
        env.define(name, func_info)
    
    def eval_return_stmt(self, node: dict, env) -> None:
        value = None
        if node.get('children'):
            value = self.evaluate(node['children'][0], env)
        raise Return(value)
    
    def _call_function(self, func_info: dict, args: list, env) -> Any:
        """
        Calls a function, handling both regular and module-scoped functions.
        func_info can be:
        - A dict with 'node' and 'closure' keys (new format with closures)
        - A dict that is the function node itself (old format)
        """
        if 'closure' in func_info and 'node' in func_info:
            func_node = func_info['node']
            closure_env = func_info['closure']
        else:
            func_node = func_info
            closure_env = env
        
        params = func_node.get('properties', {}).get('params', [])
        
        call_env = Environment(closure_env)
        
        for i, param_name in enumerate(params):
            value = args[i] if i < len(args) else None
            call_env.define(param_name, value)
        
        old_filename = self.filename
        
        func_filename = func_node.get('filename', self.filename)
        
        if func_filename:
            self.filename = func_filename
        
        try:
            body = func_node['children'][0]
            return self.evaluate(body, call_env)
        except Return as ret:
            return ret.value
        finally:
            self.filename = old_filename
    
    def eval_call_expr(self, node: dict, env) -> Any:
        callee = self.evaluate(node['children'][0], env)
        
        args = []
        for i in range(1, len(node.get('children', []))):
            args.append(self.evaluate(node['children'][i], env))
        
        if isinstance(callee, str):
            func_info = self.functions.get(callee)
            if func_info:
                return self._call_function(func_info, args, env)
            self.error(f"Function '{callee}' is not defined", node)
        
        if isinstance(callee, dict):
            if 'closure' in callee and 'node' in callee:
                return self._call_function(callee, args, env)
            if callee.get('type') == 'FUNCTION_DECL':
                func_name = callee.get('value', '')
                if func_name and func_name in self.functions:
                    return self._call_function(self.functions[func_name], args, env)
                return self._call_function({'node': callee, 'closure': env}, args, env)

        if callable(callee):
            return callee(*args)

        self.error(f"'{callee}' is not callable", node)