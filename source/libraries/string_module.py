# source/libraries/string_module.py
from .builtin_module import BuiltinModule


class String(BuiltinModule):
    def __init__(self):
        super().__init__("string")
        self._funcs = {
            "len": self.len,
            "lower": self.lower,
            "upper": self.upper,
            "sub": self.sub,
            "split": self.split,
            "join": self.join,
            "trim": self.trim,
            "find": self.find,
            "replace": self.replace,
        }
    
    @staticmethod
    def len(s: str) -> int:
        """Returns the length of the string"""
        try:
            return len(s)
        except Exception:
            return 0
    
    @staticmethod
    def lower(s: str) -> str:
        """Converts string to lowercase"""
        try:
            return s.lower()
        except Exception:
            return ""
    
    @staticmethod
    def upper(s: str) -> str:
        """Converts string to uppercase"""
        try:
            return s.upper()
        except Exception:
            return ""
    
    @staticmethod
    def sub(s: str, start: int, end: int) -> str:
        """Extracts substring from start to end (end exclusive)"""
        try:
            if start < 0:
                start = 0
            if end > len(s):
                end = len(s)
            return s[start:end]
        except Exception:
            return ""
    
    @staticmethod
    def split(s: str, sep: str = None) -> list:
        """Splits string by separator"""
        try:
            if sep is None:
                return s.split()
            return s.split(sep)
        except Exception:
            return [s] if s else []
    
    @staticmethod
    def join(parts: list, sep: str = "") -> str:
        """Joins list of strings with separator"""
        try:
            # Convert all parts to strings
            str_parts = [str(p) for p in parts]
            return sep.join(str_parts)
        except Exception:
            return ""
    
    @staticmethod
    def trim(s: str) -> str:
        """Removes leading and trailing whitespace"""
        try:
            return s.strip()
        except Exception:
            return ""
    
    @staticmethod
    def find(s: str, sub: str) -> int:
        """Returns index of first occurrence of sub, or -1 if not found"""
        try:
            return s.find(sub)
        except Exception:
            return -1
    
    @staticmethod
    def replace(s: str, old: str, new: str) -> str:
        """Replaces all occurrences of old with new"""
        try:
            return s.replace(old, new)
        except Exception:
            return s