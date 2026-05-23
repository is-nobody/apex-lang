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

def evaluate_interpolation_expr(expr: str, env: Environment, interpreter: 'Interpreter') -> Any:
    """
    Evaluate arithmetic expressions in interpolation.
    Supports: +, -, *, /, %, parentheses, and variable/field access.
    """
    import re
    
    expr = expr.strip()
    
    # First, extract variables and field accesses and replace them with placeholders
    var_pattern = r'[a-zA-Z_][a-zA-Z0-9_]*(?:\.[a-zA-Z0-9_]+)*'
    
    variables = {}
    var_counter = 0
    
    def replace_var(match):
        nonlocal var_counter
        var_path = match.group(0)
        placeholder = f"__VAR_{var_counter}__"
        
        # Evaluate the variable/field access
        try:
            parts = var_path.split('.')
            value = env.get(parts[0])
            
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
            
            variables[placeholder] = value
        except NameError:
            variables[placeholder] = None
        
        var_counter += 1
        return placeholder
    
    # Replace variables with placeholders
    processed_expr = re.sub(var_pattern, replace_var, expr)
    
    # Now evaluate the arithmetic expression with placeholders
    # First, replace placeholders with their actual values (as Python literals)
    eval_expr = processed_expr
    for placeholder, value in variables.items():
        if value is None:
            eval_expr = eval_expr.replace(placeholder, 'None')
        elif isinstance(value, str):
            # Escape special characters in string
            escaped = value.replace('\\', '\\\\').replace('"', '\\"')
            eval_expr = eval_expr.replace(placeholder, f'"{escaped}"')
        elif isinstance(value, bool):
            eval_expr = eval_expr.replace(placeholder, str(value))
        else:
            eval_expr = eval_expr.replace(placeholder, str(value))
    
    # Safely evaluate the expression
    try:
        # Use eval with restricted globals/locals
        result = eval(eval_expr, {"__builtins__": {}}, {})
        return result
    except Exception as e:
        interpreter.error(f"Error evaluating interpolation expression '{expr}': {e}", {"line": interpreter.current_line})


def is_valid_interpolation(expr: str) -> bool:
    """
    Check if the expression inside {} is valid for interpolation.
    Now supports arithmetic expressions.
    """
    if not expr or expr.isspace():
        return False
    
    # Allowed tokens in arithmetic expressions
    import re
    
    # Pattern for valid arithmetic expression with variable/field access
    # Allows: numbers, operators + - * / % ( ), spaces, variable names with optional field access
    var_pattern = r'[a-zA-Z_][a-zA-Z0-9_]*(?:\.[a-zA-Z0-9_]+)*'
    number_pattern = r'\d+(?:\.\d+)?'
    operator_pattern = r'[+\-*/%()]'
    whitespace_pattern = r'\s+'
    
    # Combined pattern to match valid tokens
    token_pattern = re.compile(
        f'({var_pattern}|{number_pattern}|{operator_pattern}|{whitespace_pattern})'
    )
    
    # Check if the entire expression consists of allowed tokens
    pos = 0
    while pos < len(expr):
        match = token_pattern.match(expr, pos)
        if not match:
            return False
        pos = match.end()
    
    # Check for balanced parentheses
    paren_count = 0
    for ch in expr:
        if ch == '(':
            paren_count += 1
        elif ch == ')':
            paren_count -= 1
            if paren_count < 0:
                return False
    
    if paren_count != 0:
        return False
    
    # Check for forbidden patterns
    forbidden = {'=', '!', '<', '>', '&', '|', '^', '~', '[', ']', '{', '}', ',', ';', ':'}
    for ch in expr:
        if ch in forbidden:
            return False
    
    # Check for keywords
    keywords = {'and', 'or', 'not', 'true', 'false', 'none', 
                'if', 'else', 'elif', 'while', 'for', 'in', 'to', 'range',
                'function', 'return', 'break', 'continue', 'import', 'try',
                'failure', 'always'}
    
    # Extract variable names (not including field access parts)
    for match in re.finditer(var_pattern, expr):
        var_name = match.group(0).split('.')[0]
        if var_name in keywords:
            return False
    
    return True

def interpolate_string(s: str, env: Environment, interpreter: 'Interpreter') -> str:
    """
    Replace {variable}, {table.field}, and {arithmetic expressions} with evaluated values.
    """
    result = ""
    i = 0
    while i < len(s):
        if s[i] == '{' and (i == 0 or s[i-1] != '\\'):
            end = s.find('}', i)
            if end != -1:
                expr = s[i+1:end].strip()
                
                if not expr:
                    result += '{}'
                    i = end + 1
                    continue
                
                if not is_valid_interpolation(expr):
                    interpreter.error(
                        f"Invalid interpolation '{{{expr}}}'. "
                        "Only arithmetic expressions with variables and field access are allowed inside {{}}.",
                        {"line": interpreter.current_line}
                    )
                
                # Evaluate the expression
                try:
                    value = evaluate_interpolation_expr(expr, env, interpreter)
                    result += str(value) if value is not None else 'none'
                except Exception as e:
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