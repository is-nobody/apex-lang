# source/core/interpreter/literals.py
from typing import Any
from source.core.interpreter.tables import Table
from source.core.interpreter.helpers import interpolate_string

class LiteralsMixin:
    """Mixin for evaluating literals and identifiers"""
    
    def eval_number_literal(self, node: dict, env) -> int | float:
        return node.get('value', 0)
    
    def eval_string_literal(self, node: dict, env) -> str:
        raw_value = node.get('value', '')
        return interpolate_string(raw_value, env, self)
    
    def eval_boolean_literal(self, node: dict, env) -> bool:
        return node.get('value', False)
        
    def eval_identifier(self, node: dict, env) -> Any:
        name = node.get('value', '')
        try:
            return env.get(name)
        except NameError:
            if name in self.functions:
                return self.functions[name]
            self.error(f"Name '{name}' is not defined", node)
    
    def eval_table_literal(self, node: dict, env) -> Table:
        table = Table()
        
        for i, entry_node in enumerate(node.get('children', [])):
            entry_type = entry_node.get('type', '')
            
            if entry_type == 'TABLE_ENTRY':
                key = entry_node.get('properties', {}).get('key', None)
                value = self.evaluate(entry_node['children'][0], env)
                
                if key is not None:
                    table.set(key, value)
                else:
                    table.set(i + 1, value)
        
        return table
    
    def eval_table_entry(self, node: dict, env) -> Any:
        return self.evaluate(node['children'][0], env)