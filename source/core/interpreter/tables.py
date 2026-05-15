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
            return None  # как в вашем языке — индекс 0 возвращает none
        return self.items.get(key, None)
    
    def __len__(self):
        return len(self.items)
    
    def __repr__(self):
        if not self.items:
            return '()'
        items_str = ', '.join(f'{k}: {v}' for k, v in self.items.items())
        return f'{{{items_str}}}'