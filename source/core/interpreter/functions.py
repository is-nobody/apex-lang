# source/core/interpreter/functions.py
from source.core.interpreter.environment import Environment, Return
from source.core.interpreter.tables import Table
from typing import Any

class FunctionsMixin:
    """Mixin for evaluating function declarations and calls"""
    
    def _get_native_functions(self) -> dict[str, callable]:
        """Return dictionary of native functions"""
        return {
            'number': self._native_number,
            'string': self._native_string,
        }
    
    def _native_number(self, value: Any) -> int | float:
        """Convert value to number"""
        if isinstance(value, bool):
            return 1 if value else 0
        if isinstance(value, (int, float)):
            return value
        if isinstance(value, str):
            try:
                if '.' in value:
                    return float(value)
                return int(value)
            except ValueError:
                raise TypeError(f"Cannot convert string '{value}' to number")
        if isinstance(value, Table):
            return len(value)
        return None
    
    def _native_string(self, value: Any) -> str:
        """Convert value to string"""
        if isinstance(value, bool):
            return "true" if value else "false"
        if isinstance(value, (int, float)):
            # Format numbers nicely
            if isinstance(value, float) and value.is_integer():
                return str(int(value))
            return str(value)
        if isinstance(value, str):
            return value
        if isinstance(value, Table):
            if len(value) == 0:
                return "()"
            items = []
            for k, v in value.items.items():
                items.append(f"{k}: {v}")
            return f"({', '.join(items)})"
        return str(value)
    
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
    
    def _is_native_function(self, value: Any) -> bool:
        """Check if a value is a native function wrapper"""
        return isinstance(value, dict) and value.get('native', False)
    
    def _call_native_function(self, native_info: dict, args: list) -> Any:
        """Call a native function"""
        func = native_info.get('func')
        if func:
            return func(*args)
        self.error(f"Invalid native function: {native_info.get('name', 'unknown')}", {})
    
    def _call_function(self, func_info: dict, args: list, env) -> Any:
        """
        Calls a function, handling both regular and module-scoped functions.
        func_info can be:
        - A dict with 'native' flag (native functions)
        - A dict with 'node' and 'closure' keys (new format with closures)
        - A dict that is the function node itself (old format)
        """
        # Check for native function FIRST
        if self._is_native_function(func_info):
            return self._call_native_function(func_info, args)
        
        if 'closure' in func_info and 'node' in func_info:
            func_node = func_info['node']
            closure_env = func_info['closure']
        else:
            func_node = func_info
            closure_env = env
        
        params = func_node.get('properties', {}).get('params', [])
        param_types = func_node.get('properties', {}).get('param_types', {})
        
        # Type checking
        for i, param_name in enumerate(params):
            if i < len(args):
                arg_value = args[i]
                expected_type = param_types.get(param_name)
                
                if expected_type:
                    if not self._check_type(arg_value, expected_type):
                        func_name = func_node.get('value', 'anonymous')
                        actual_type = self._get_type_name(arg_value)
                        self.error(
                            f"Parameter '{param_name}' expected {expected_type}, got {actual_type}",
                            func_node
                        )
        
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

    def _check_type(self, value: Any, expected_type: str) -> bool:
        """Check if value matches expected type"""
        if expected_type == 'number':
            return isinstance(value, (int, float))
        elif expected_type == 'string':
            return isinstance(value, str)
        elif expected_type == 'boolean':
            return isinstance(value, bool)
        elif expected_type == 'table':
            from source.core.interpreter.tables import Table
            return isinstance(value, Table)
        return True

    def _get_type_name(self, value: Any) -> str:
        """Get type name as string for error messages"""
        if isinstance(value, bool):
            return 'boolean'
        elif isinstance(value, (int, float)):
            return 'number'
        elif isinstance(value, str):
            return 'string'
        elif isinstance(value, Table):
            return 'table'
        else:
            return type(value).__name__.lower()
    
    def eval_call_expr(self, node: dict, env) -> Any:
        callee_node = node['children'][0]
        
        # check if the native function is called by name
        if callee_node.get('type') == 'IDENTIFIER':
            func_name = callee_node.get('value', '')
            native_funcs = self._get_native_functions()
            
            if func_name in native_funcs:
                # сollecting arguments
                args = []
                for i in range(1, len(node.get('children', []))):
                    args.append(self.evaluate(node['children'][i], env))
                # сall the native function directly
                return native_funcs[func_name](*args)
        
        # standard resolving for everything else
        callee = self.evaluate(callee_node, env)
        
        args = []
        for i in range(1, len(node.get('children', []))):
            args.append(self.evaluate(node['children'][i], env))
        
        args = []
        for i in range(1, len(node.get('children', []))):
            args.append(self.evaluate(node['children'][i], env))
        
        # Handle native function (dict with 'native' flag)
        if isinstance(callee, dict) and callee.get('native', False):
            return callee['func'](*args)
        
        # Handle string function name
        if isinstance(callee, str):
            # First check if it's a native function by name
            native_funcs = self._get_native_functions()
            if callee in native_funcs:
                return native_funcs[callee](*args)
            
            # Then check user functions
            func_info = self.functions.get(callee)
            if func_info:
                return self._call_function(func_info, args, env)
            self.error(f"Function '{callee}' is not defined", node)
        
        # Handle function object (dict with node)
        if isinstance(callee, dict):
            if self._is_native_function(callee):
                return self._call_native_function(callee, args)
            if 'closure' in callee and 'node' in callee:
                return self._call_function(callee, args, env)
            if callee.get('type') == 'FUNCTION_DECL':
                func_name = callee.get('value', '')
                if func_name and func_name in self.functions:
                    return self._call_function(self.functions[func_name], args, env)
                return self._call_function({'node': callee, 'closure': env}, args, env)

        # Handle regular Python callable
        if callable(callee):
            return callee(*args)

        self.error(f"'{callee}' is not callable", node)