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
    Supports nested {} expressions.
    Supports: +, -, *, /, %, parentheses, and variable/field access.
    """
    import re
    
    expr = expr.strip()
    
    # First, evaluate nested {} expressions recursively
    def evaluate_nested(expression: str) -> str:
        result_parts = []
        i = 0
        length = len(expression)
        
        while i < length:
            if expression[i] == '{':
                # Find matching closing brace
                brace_count = 1
                j = i + 1
                while j < length and brace_count > 0:
                    if expression[j] == '{':
                        brace_count += 1
                    elif expression[j] == '}':
                        brace_count -= 1
                    j += 1
                
                if brace_count == 0:
                    nested = expression[i+1:j-1]
                    nested_value = evaluate_interpolation_expr(nested, env, interpreter)
                    result_parts.append(str(nested_value) if nested_value is not None else 'none')
                    i = j
                    continue
            
            result_parts.append(expression[i])
            i += 1
        
        return ''.join(result_parts)
    
    # Evaluate all nested interpolations first
    expr_without_nested = evaluate_nested(expr)
    
    # Now extract variables and field accesses and replace them with placeholders
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
    processed_expr = re.sub(var_pattern, replace_var, expr_without_nested)
    
    # Now evaluate the arithmetic expression with placeholders
    eval_expr = processed_expr
    for placeholder, value in variables.items():
        if value is None:
            eval_expr = eval_expr.replace(placeholder, 'None')
        elif isinstance(value, str):
            escaped = value.replace('\\', '\\\\').replace('"', '\\"')
            eval_expr = eval_expr.replace(placeholder, f'"{escaped}"')
        elif isinstance(value, bool):
            eval_expr = eval_expr.replace(placeholder, str(value))
        else:
            eval_expr = eval_expr.replace(placeholder, str(value))
    
    # Safely evaluate the expression
    try:
        result = eval(eval_expr, {"__builtins__": {}}, {})
        return result
    except Exception as e:
        interpreter.error(f"Error evaluating interpolation expression '{expr}': {e}", {"line": interpreter.current_line})

def is_valid_interpolation(expr: str) -> bool:
    """
    Check if the expression inside {} is valid for interpolation.
    Supports nested braces like {table.{field}}.
    """
    if not expr or expr.isspace():
        return False
    
    # First, extract nested interpolations and replace them with placeholders
    # This allows checking the outer structure while ignoring inner braces
    
    def extract_nested(expression: str) -> tuple[str, list[str]]:
        """Replace nested {} with placeholders and return list of nested expressions"""
        placeholders = []
        result_parts = []
        i = 0
        length = len(expression)
        placeholder_counter = 0
        
        while i < length:
            if expression[i] == '{':
                # Find matching closing brace
                brace_count = 1
                j = i + 1
                while j < length and brace_count > 0:
                    if expression[j] == '{':
                        brace_count += 1
                    elif expression[j] == '}':
                        brace_count -= 1
                    j += 1
                
                if brace_count == 0:
                    # Found nested interpolation
                    nested = expression[i+1:j-1]
                    placeholder = f"__NESTED_{placeholder_counter}__"
                    placeholders.append(nested)
                    result_parts.append(placeholder)
                    placeholder_counter += 1
                    i = j
                    continue
            
            result_parts.append(expression[i])
            i += 1
        
        return ''.join(result_parts), placeholders
    
    # Extract nested braces
    processed_expr, nested_exprs = extract_nested(expr)
    
    # Recursively validate nested expressions
    for nested in nested_exprs:
        if not is_valid_interpolation(nested):
            return False
    
    # Allowed tokens in arithmetic expressions (without braces now)
    import re
    
    # Pattern for valid arithmetic expression with variable/field access
    var_pattern = r'[a-zA-Z_][a-zA-Z0-9_]*(?:\.[a-zA-Z0-9_]+)*'
    number_pattern = r'\d+(?:\.\d+)?'
    operator_pattern = r'[+\-*/%()]'
    whitespace_pattern = r'\s+'
    placeholder_pattern = r'__NESTED_\d+__'
    
    # Combined pattern to match valid tokens including placeholders
    token_pattern = re.compile(
        f'({var_pattern}|{number_pattern}|{operator_pattern}|{whitespace_pattern}|{placeholder_pattern})'
    )
    
    # Check if the entire expression consists of allowed tokens
    pos = 0
    while pos < len(processed_expr):
        match = token_pattern.match(processed_expr, pos)
        if not match:
            return False
        pos = match.end()
    
    # Check for balanced parentheses (ignoring nested braces which are replaced)
    paren_count = 0
    for ch in processed_expr:
        if ch == '(':
            paren_count += 1
        elif ch == ')':
            paren_count -= 1
            if paren_count < 0:
                return False
    
    if paren_count != 0:
        return False
    
    # Check for forbidden patterns (excluding placeholders)
    forbidden = {'=', '!', '<', '>', '&', '|', '^', '~', '[', ']', ',', ';', ':'}
    # Remove placeholders for forbidden check
    check_expr = re.sub(r'__NESTED_\d+__', 'X', processed_expr)
    for ch in check_expr:
        if ch in forbidden:
            return False
    
    # Check for keywords
    keywords = {'and', 'or', 'not', 'true', 'false', 'none', 
                'if', 'else', 'elif', 'while', 'for', 'in', 'to', 'range',
                'function', 'return', 'break', 'continue', 'import', 'try',
                'failure', 'always'}
    
    # Extract variable names (not including field access parts)
    for match in re.finditer(var_pattern, processed_expr):
        var_name = match.group(0).split('.')[0]
        if var_name in keywords:
            return False
    
    return True

def interpolate_string(s: str, env: Environment, interpreter: 'Interpreter') -> str:
    result = ""
    i = 0
    length = len(s)
    
    while i < length:
        if s[i] == '{' and (i == 0 or s[i-1] != '\\'):
            # find matching closing brace considering nesting
            brace_count = 1
            j = i + 1
            while j < length and brace_count > 0:
                if s[j] == '{' and (j == 0 or s[j-1] != '\\'):
                    brace_count += 1
                elif s[j] == '}' and (j == 0 or s[j-1] != '\\'):
                    brace_count -= 1
                j += 1
            
            if brace_count == 0:
                # found matching closing brace
                expr = s[i+1:j-1].strip()
                
                if not expr:
                    result += '{}'
                    i = j
                    continue
                
                if not is_valid_interpolation(expr):
                    interpreter.error(
                        f"Invalid interpolation '{{{expr}}}'. "
                        "Only arithmetic expressions with variables and field access are allowed inside {{}}.",
                        {"line": interpreter.current_line}
                    )
                
                # evaluate the expression
                try:
                    value = evaluate_interpolation_expr(expr, env, interpreter)
                    result += str(value) if value is not None else 'none'
                except Exception as e:
                    result += '{' + expr + '}'
                
                i = j
                continue
            else:
                # unmatched braces, treat as literal
                result += s[i]
                i += 1
        else:
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