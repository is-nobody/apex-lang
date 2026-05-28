# source/core/interpreter/tables.py
from typing import Any

class Table:
    """Таблица Apex (индексация с 1, ключи — строки или числа)"""
    def __init__(self):
        self.items: dict[int | str, Any] = {}
        self._next_index = 1
    
    def set(self, key: int | str, value: Any):
        self.items[key] = value
        if isinstance(key, int) and key >= self._next_index:
            self._next_index = key + 1
    
    def get(self, key: int | str) -> Any:
        if key == 0:
            raise KeyError(f"Table index 0 is invalid (indices start from 1)")
        
        if key not in self.items:
            raise KeyError(f"Table key '{key}' does not exist")
        
        return self.items[key]
    
    def __len__(self):
        return len(self.items)
    
    def __repr__(self):
        if not self.items:
            return '()'
        
        # Separate ordered items (int keys in sequence from 1)
        ordered = []
        key_values = []
        
        # Find ordered items (consecutive int keys starting from 1)
        i = 1
        while i in self.items:
            ordered.append(self.items[i])
            i += 1
        
        # Collect key-value pairs (non-int keys or int keys outside sequence)
        for key, value in self.items.items():
            if isinstance(key, int) and key < i:
                continue  # Already in ordered
            key_values.append(f"{key} = {value}")
        
        # Build result
        result_parts = []
        if ordered:
            result_parts.append(', '.join(str(v) for v in ordered))
        if key_values:
            result_parts.append(', '.join(key_values))
        
        return f"({', '.join(result_parts)})"