# source/libraries/__init__.py
from .os_module import OS
from .math_module import Math
from .string_module import String
from .network_module import Network
from .ui_module import UI

__all__ = ['OS', 'Math', 'String', 'Network', 'UI']

BUILTIN_MODULES = {
    "os": OS(),
    "math": Math(),
    "string": String(),
    "network": Network(),
    "ui": UI(),
}