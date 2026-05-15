# source/libraries/builtin_module.py
class BuiltinModule:
    """Базовый класс для встроенных модулей"""
    def __init__(self, name: str):
        self.name = name
        self._funcs: dict[str, callable] = {}
    
    def get(self, name: str):
        if name in self._funcs:
            return self._funcs[name]
        raise AttributeError(f"Module '{self.name}' has no member '{name}'")
    
    def __repr__(self):
        return f"<module '{self.name}'>"