# source/core/interpreter/environment.py
from typing import Any, Optional

class Return(Exception):
    def __init__(self, value):
        self.value = value

class Break(Exception):
    pass

class Continue(Exception):
    pass

class Environment:
    def __init__(self, parent: Optional['Environment'] = None):
        self.variables: dict[str, Any] = {}
        self.constants: set[str] = set()
        self.parent = parent
    
    def define(self, name: str, value: Any, is_constant: bool = False):
        self.variables[name] = value
        if is_constant:
            self.constants.add(name)
    
    def assign(self, name: str, value: Any):
        # check if it's a constant
        if name in self.constants:
            raise RuntimeError(f"Cannot assign to read-only variable '{name}'")
        
        if name in self.variables:
            self.variables[name] = value
        elif self.parent:
            self.parent.assign(name, value)
        else:
            raise NameError(f"Cannot assign to undefined variable '{name}'")
    
    def get(self, name: str) -> Any:
        if name in self.variables:
            return self.variables[name]
        if self.parent:
            return self.parent.get(name)
        raise NameError(f"Name '{name}' is not defined")