# source/libraries/table_module.py
from source.libraries.builtin_module import BuiltinModule
from source.core.interpreter.tables import Table

class TableModule(BuiltinModule):
    def __init__(self):
        super().__init__("table")
        self._funcs = {
            "remove": self.remove,
            "has": self.has,
            "size": self.size,
            "keys": self.keys,
            "values": self.values,
            "clear": self.clear,
            "copy": self.copy,
            "merge": self.merge,
        }
    
    # ========== Helper ==========
    
    def _check_table(self, t):
        """Check if value is a Table"""
        if not isinstance(t, Table):
            # Get the actual type name
            type_name = t.__class__.__name__ if hasattr(t, '__class__') else type(t).__name__
            raise TypeError(f"Expected table, got {type_name}")
        return True
    
    # ========== Remove ==========
    
    def remove(self, t, key):
        """Removes an item from a table by key or index"""
        self._check_table(t)
        
        # Convert string to int if possible for numeric keys
        if isinstance(key, str):
            try:
                key = int(key)
            except ValueError:
                pass
        
        if key not in t.items:
            raise KeyError(f"Key '{key}' does not exist in table")
        
        del t.items[key]
        
        # Reindex ordered items if we deleted an integer key
        if isinstance(key, int):
            # Find all integer keys greater than the deleted key
            to_reindex = {}
            for k in list(t.items.keys()):
                if isinstance(k, int) and k > key:
                    to_reindex[k] = t.items[k]
            
            # Remove old keys and add with new indices
            for old_key in to_reindex:
                del t.items[old_key]
                new_key = old_key - 1
                t.items[new_key] = to_reindex[old_key]
        
        # Update next_index
        max_index = 0
        for k in t.items:
            if isinstance(k, int) and k > max_index:
                max_index = k
        t._next_index = max_index + 1 if max_index > 0 else 1
    
    # ========== Has ==========
    
    def has(self, t, key) -> bool:
        """Returns true if the table has the specified key or index"""
        self._check_table(t)
        
        if isinstance(key, str):
            try:
                key = int(key)
            except ValueError:
                pass
        
        return key in t.items
    
    # ========== Size ==========
    
    def size(self, t) -> int:
        """Returns the number of items in the table"""
        self._check_table(t)
        return len(t.items)
    
    # ========== Keys ==========
    
    def keys(self, t) -> Table:
        """Returns a table of all keys"""
        self._check_table(t)
        
        result = Table()
        index = 1
        for key in t.items.keys():
            if isinstance(key, int):
                result.set(index, str(key))
            else:
                result.set(index, key)
            index += 1
        return result
    
    # ========== Values ==========
    
    def values(self, t) -> Table:
        """Returns a table of all values"""
        self._check_table(t)
        
        result = Table()
        index = 1
        for value in t.items.values():
            result.set(index, value)
            index += 1
        return result
    
    # ========== Clear ==========
    
    def clear(self, t):
        """Removes all items from the table"""
        self._check_table(t)
        t.items.clear()
        t._next_index = 1
        return None
    
    # ========== Copy ==========
    
    def copy(self, t) -> Table:
        """Returns a shallow copy of the table"""
        self._check_table(t)
        
        result = Table()
        result.items = t.items.copy()
        result._next_index = t._next_index
        return result
    
    # ========== Merge ==========
    
    def merge(self, t1, t2) -> Table:
        """Merges two tables into a new table"""
        self._check_table(t1)
        self._check_table(t2)
        
        result = Table()
        
        # Add all items from first table
        for key, value in t1.items.items():
            result.set(key, value)
        
        # Add all items from second table (overwrites conflicts)
        for key, value in t2.items.items():
            result.set(key, value)
        
        return result