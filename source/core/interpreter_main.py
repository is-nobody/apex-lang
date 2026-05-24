# source/core/interpreter/interpreter.py
from typing import Any, Optional

from source.core.interpreter.environment import Environment
from source.core.interpreter.evaluator import EvaluatorMixin
from source.core.interpreter.statements import StatementsMixin
from source.core.interpreter.expressions import ExpressionsMixin
from source.core.interpreter.literals import LiteralsMixin
from source.core.interpreter.modules import ModulesMixin
from source.core.interpreter.functions import FunctionsMixin

class Interpreter(
    EvaluatorMixin,
    StatementsMixin,
    ExpressionsMixin,
    LiteralsMixin,
    ModulesMixin,
    FunctionsMixin
):
    def __init__(self, filename: str = "<unknown>"):
        self.filename = filename
        self.global_env = Environment()
        self.functions: dict[str, dict] = {}
        self.current_line = 0
        self.loaded_modules: dict[str, Any] = {}
        self._loading_modules: set = set()
        self._register_native_functions()
    
    def _register_native_functions(self):
        """Register native type conversion functions in global environment"""
        native_funcs = self._get_native_functions()
        for name, func in native_funcs.items():
            # Store as a callable wrapper that the interpreter can recognize
            self.global_env.define(name, {
                'native': True,
                'func': func,
                'name': name
            })

    def error(self, message: str, node: dict):
        line = node.get('line', self.current_line)
        raise RuntimeError(f"in {self.filename} on line {line}: {message}")