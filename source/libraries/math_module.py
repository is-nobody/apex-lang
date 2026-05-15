# source/libraries/math_module.py
import math as _math
from source.libraries.builtin_module import BuiltinModule

class Math(BuiltinModule):
    def __init__(self):
        super().__init__("math")
        self._funcs = {
            # Основные функции
            "abs": self.abs,
            "floor": self.floor,
            "ceil": self.ceil,
            "round": self.round,
            "sqrt": self.sqrt,
            "exp": self.exp,
            "log": self.log,
            
            # Тригонометрия
            "sin": self.sin,
            "cos": self.cos,
            "tan": self.tan,
            "asin": self.asin,
            "acos": self.acos,
            "atan": self.atan,
        }
    
    @staticmethod
    def abs(x):
        try:
            return abs(x)
        except Exception:
            return None
    
    @staticmethod
    def floor(x):
        try:
            return _math.floor(x)
        except Exception:
            return None
    
    @staticmethod
    def ceil(x):
        try:
            return _math.ceil(x)
        except Exception:
            return None
    
    @staticmethod
    def round(x, ndigits=None):
        try:
            if ndigits is None:
                return round(x)
            return round(x, ndigits)
        except Exception:
            return None
    
    @staticmethod
    def sqrt(x):
        try:
            if x < 0:
                return None
            return _math.sqrt(x)
        except Exception:
            return None
    
    @staticmethod
    def exp(x):
        try:
            return _math.exp(x)
        except Exception:
            return None
    
    @staticmethod
    def log(x, base=None):
        try:
            if x <= 0:
                return None
            if base is None:
                return _math.log(x)
            return _math.log(x, base)
        except Exception:
            return None
    
    @staticmethod
    def sin(x):
        try:
            return _math.sin(x)
        except Exception:
            return None
    
    @staticmethod
    def cos(x):
        try:
            return _math.cos(x)
        except Exception:
            return None
    
    @staticmethod
    def tan(x):
        try:
            return _math.tan(x)
        except Exception:
            return None
    
    @staticmethod
    def asin(x):
        try:
            if x < -1 or x > 1:
                return None
            return _math.asin(x)
        except Exception:
            return None
    
    @staticmethod
    def acos(x):
        try:
            if x < -1 or x > 1:
                return None
            return _math.acos(x)
        except Exception:
            return None
    
    @staticmethod
    def atan(x):
        try:
            return _math.atan(x)
        except Exception:
            return None