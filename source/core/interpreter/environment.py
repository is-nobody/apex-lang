# source/core/interpreter/environment.py
from typing import Any, Optional

class Return(Exception):
    """Исключение для выхода из функции с возвратом значения"""
    def __init__(self, value):
        self.value = value

class Break(Exception):
    """Исключение для выхода из цикла"""
    pass

class Continue(Exception):
    """Исключение для перехода к следующей итерации цикла"""
    pass

class Environment:
    """Область видимости с поддержкой вложенности"""
    def __init__(self, parent: Optional['Environment'] = None):
        self.variables: dict[str, Any] = {}
        self.parent = parent
    
    def define(self, name: str, value: Any):
        self.variables[name] = value
    
    def assign(self, name: str, value: Any):
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