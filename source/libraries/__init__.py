# source/libraries/__init__.py
from .os_module import OS
from .math_module import Math
from .string_module import String
from .table_module import TableModule

__all__ = ['OS', 'Math', 'String', 'Table']

BUILTIN_MODULES = {
    "os": OS(),
    "math": Math(),
    "string": String(),
    "table": TableModule(),
}