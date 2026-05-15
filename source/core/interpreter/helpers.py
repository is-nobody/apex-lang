# source/core/interpreter/helpers.py
from typing import Any, TYPE_CHECKING
from source.core.interpreter.tables import Table
from source.libraries.builtin_module import BuiltinModule
from source.core.interpreter.environment import Environment

# Для избежания циклического импорта
if TYPE_CHECKING:
    from source.core.interpreter_main import Interpreter

def is_truthy(value: Any) -> bool:
    if value is None:
        return False
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        return value != ''
    if isinstance(value, (list, dict, Table)):
        return len(value) > 0
    return True

def is_valid_interpolation(expr: str) -> bool:
    """
    Проверяет, что строка внутри {} — это допустимое выражение для интерполяции.
    Разрешено:
    - простое имя: myVar
    - доступ к полям: myVar.field.subfield
    - числовые индексы таблиц: myTable.1.2
    Запрещено:
    - операторы: a + b, a > b, a and b
    - вызовы функций: func()
    - пробелы
    - строковые литералы
    - арифметика
    """
    if not expr or ' ' in expr:
        return False
    
    # Запрещённые символы/операторы
    forbidden = {'+', '-', '*', '/', '%', '=', '<', '>', '!', 
                '(', ')', '[', ']', '"', "'", '&', '|', '^', '~', ':', ',', ';'}
    
    for ch in expr:
        if ch in forbidden:
            return False
    
    # Ключевые слова запрещены
    keywords = {'and', 'or', 'not', 'true', 'false', 'none', 
                'if', 'else', 'elif', 'while', 'for', 'in', 'to', 'range',
                'function', 'return', 'break', 'continue', 'import', 'try',
                'failure', 'always'}
    
    parts = expr.split('.')
    for part in parts:
        if part in keywords:
            return False
        if not part.isdigit():
            if not part or not (part[0].isalpha() or part[0] == '_'):
                return False
            for ch in part:
                if not (ch.isalnum() or ch == '_'):
                    return False
    
    return True

def interpolate_string(s: str, env: Environment, interpreter: 'Interpreter') -> str:
    """Заменяет {variable} и {table.field} на значения из окружения"""
    result = ""
    i = 0
    while i < len(s):
        if s[i] == '{':
            end = s.find('}', i)
            if end != -1:
                expr = s[i+1:end]
                
                if not is_valid_interpolation(expr):
                    interpreter.error(
                        f"Invalid interpolation '{{{expr}}}'. "
                        "Only variable names and field access (a.b.c) are allowed inside {{}}.",
                        {"line": interpreter.current_line}
                    )
                
                try:
                    parts = expr.split('.')
                    value = env.get(parts[0])
                    
                    if not parts[0].replace('_', '').isalnum():
                        interpreter.error(
                            f"Invalid expression in interpolation: '{{{expr}}}'. "
                            "Only variables and field access are allowed.",
                            {"line": interpreter.current_line}
                        )
                    
                    for part in parts[1:]:
                        if isinstance(value, Environment):
                            try:
                                value = value.get(part)
                            except NameError:
                                value = None
                                break
                        elif isinstance(value, Table):
                            try:
                                idx = int(part)
                                value = value.get(idx)
                            except ValueError:
                                value = value.get(part)
                        elif isinstance(value, BuiltinModule):
                            try:
                                value = value.get(part)
                            except AttributeError:
                                value = None
                                break
                        else:
                            value = None
                            break
                    
                    result += str(value) if value is not None else 'none'
                except NameError:
                    result += '{' + expr + '}'
                except Exception:
                    result += '{' + expr + '}'
                i = end + 1
                continue
        result += s[i]
        i += 1
    return result

def add_filename_to_functions(node: dict, filename: str):
    """Recursively add filename to all function declarations in AST"""
    if not isinstance(node, dict):
        return
    
    if node.get('type') == 'FUNCTION_DECL':
        node['filename'] = filename
    
    for child in node.get('children', []):
        if isinstance(child, dict):
            add_filename_to_functions(child, filename)
    
    for key, value in node.get('properties', {}).items():
        if isinstance(value, dict):
            add_filename_to_functions(value, filename)
        elif isinstance(value, list):
            for item in value:
                if isinstance(item, dict):
                    add_filename_to_functions(item, filename)