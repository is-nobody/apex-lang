# source/core/interpreter.py
import sys
import json
from pathlib import Path
from typing import Any, Optional

from source.libraries import BUILTIN_MODULES
from source.libraries.builtin_module import BuiltinModule
# ========== Встроенные модули ==========

class Return(Exception):
    """Исключение для выхода из функции с возвратом значения"""
    def __init__(self, value):
        self.value = value

class Break(Exception):
    """Исключение для выхода из цикла"""
    pass

class Continue(Exception):
    """Исключение для перехода к следующей итерации цикла"""
    pass

class Environment:
    """Область видимости с поддержкой вложенности"""
    def __init__(self, parent: Optional['Environment'] = None):
        self.variables: dict[str, Any] = {}
        self.parent = parent
    
    def define(self, name: str, value: Any):
        self.variables[name] = value
    
    def assign(self, name: str, value: Any):
        if name in self.variables:
            self.variables[name] = value
        elif self.parent:
            self.parent.assign(name, value)
        else:
            raise NameError(f"Cannot assign to undefined variable '{name}'")
    
    def get(self, name: str) -> Any:
        if name in self.variables:
            return self.variables[name]
        if self.parent:
            return self.parent.get(name)
        raise NameError(f"Name '{name}' is not defined")

class Interpreter:
    def __init__(self, filename: str = "<unknown>"):
        self.filename = filename
        self.global_env = Environment()
        self.functions: dict[str, dict] = {}
        self.current_line = 0
        self.loaded_modules: dict[str, Any] = {}
    
    def error(self, message: str, node: dict):
        line = node.get('line', self.current_line)
        # Use self.filename which should be the actual file where the error occurred
        raise RuntimeError(f"Runtime Error in {self.filename} on line {line}: {message}")

    # ========== Главный eval ==========
    
    def evaluate(self, node: dict, env: Optional[Environment] = None) -> Any:
        if env is None:
            env = self.global_env
        
        if 'line' in node:
            self.current_line = node['line']
        
        node_type = node.get('type', '')
        
        handlers = {
            'PROGRAM': self.eval_program,
            'IMPORT': self.eval_import,
            'IMPORT_SPECIFIC': self.eval_import_specific,
            'VARIABLE_DECL': self.eval_variable_decl,
            'FUNCTION_DECL': self.eval_function_decl,
            'EXPRESSION_STMT': self.eval_expression_stmt,
            'BLOCK': self.eval_block,
            'IF_STMT': self.eval_if_stmt,
            'WHILE_STMT': self.eval_while_stmt,
            'FOR_STMT': self.eval_for_stmt,
            'FOR_IN_STMT': self.eval_for_in_stmt,
            'BREAK_STMT': self.eval_break_stmt,
            'CONTINUE_STMT': self.eval_continue_stmt,
            'RETURN_STMT': self.eval_return_stmt,
            'TRY_STMT': self.eval_try_stmt,
            'BINARY_EXPR': self.eval_binary_expr,
            'UNARY_EXPR': self.eval_unary_expr,
            'CALL_EXPR': self.eval_call_expr,
            'MEMBER_EXPR': self.eval_member_expr,
            'ASSIGNMENT_EXPR': self.eval_assignment_expr,
            'LOGICAL_EXPR': self.eval_logical_expr,
            'NUMBER_LITERAL': self.eval_number_literal,
            'STRING_LITERAL': self.eval_string_literal,
            'BOOLEAN_LITERAL': self.eval_boolean_literal,
            'NONE_LITERAL': self.eval_none_literal,
            'IDENTIFIER': self.eval_identifier,
            'TABLE_LITERAL': self.eval_table_literal,
            'TABLE_ENTRY': self.eval_table_entry,
        }
        
        handler = handlers.get(node_type)
        if handler:
            return handler(node, env)
        
        self.error(f"Unknown node type: {node_type}", node)
    
    # ========== Программа и блоки ==========
    
    def eval_program(self, node: dict, env: Environment) -> Any:
        result = None
        for i, child in enumerate(node.get('children', [])):
            result = self.evaluate(child, env)
        return result

    def eval_block(self, node: dict, env: Environment) -> Any:
        result = None
        for i, child in enumerate(node.get('children', [])):
            result = self.evaluate(child, env)
        return result
    
    # ========== Импорты (заглушки) ==========
    
    def eval_import(self, node: dict, env: Environment) -> None:
        module_path = node.get('value', '')
        self._load_module(module_path, None, env, node)

    def eval_import_specific(self, node: dict, env: Environment) -> None:
        module_path = node.get('value', '')
        imports = node.get('properties', {}).get('imports', [])
        self._load_module(module_path, imports, env, node)

    def _load_module(self, module_path: str, specific_imports: Optional[list], 
                    env: Environment, node: dict):
        """Загружает модуль — встроенный или пользовательский"""
        
        # Защита от рекурсивных импортов
        if not hasattr(self, '_loading_modules'):
            self._loading_modules = set()
        
        if module_path in self._loading_modules:
            self.error(f"Circular import detected: '{module_path}'", node)
        
        self._loading_modules.add(module_path)
        
        ast_dict = None  # Initialize ast_dict here
        
        try:
            # 1. Проверяем встроенные модули
            if module_path in BUILTIN_MODULES:
                module = BUILTIN_MODULES[module_path]
                
                if specific_imports:
                    for name in specific_imports:
                        try:
                            env.define(name, module.get(name))
                        except AttributeError:
                            self.error(
                                f"Module '{module_path}' has no member '{name}'", 
                                node
                            )
                else:
                    env.define(module_path, module)
                return
            
            # 2. Пытаемся загрузить пользовательский модуль из файла
            module_file = Path(module_path.replace('.', '/') + '.apex')
            
            # Ищем относительно текущей директории и относительно исходного файла
            search_paths = [
                module_file,
                Path(self.filename).parent / module_file,
            ]
            
            found_file = None
            for path in search_paths:
                if path.exists():
                    found_file = path
                    break
            
            if found_file is None:
                self.error(
                    f"Module '{module_path}' not found.",
                    node
                )
            
            # Проверяем кэш загруженных модулей
            cache_key = str(found_file.resolve())
            if cache_key in self.loaded_modules:
                module_env = self.loaded_modules[cache_key]
            else:
                # Парсим и выполняем модуль
                try:
                    with open(found_file, 'r') as f:
                        source = f.read()
                    
                    # Токенизируем
                    from source.core.tokenizer import Tokenizer
                    tokenizer = Tokenizer(source, str(found_file))
                    raw_tokens = tokenizer.tokenize()

                    # Конвертируем токены в формат парсера (как в main.py)
                    from source.core.parser import Token as ParserToken, TokenType as ParserTokenType
                    parser_tokens = [
                        ParserToken(
                            type=ParserTokenType[t.type.name],
                            value=t.value,
                            line=t.line,
                            column=t.column
                        )
                        for t in raw_tokens
                    ]

                    # Парсим
                    from source.core.parser import Parser
                    parser = Parser(parser_tokens, str(found_file))
                    ast = parser.parse()
                    
                    if ast is None:
                        self.error(f"Failed to parse module '{module_path}'", node)
                    
                    # Выполняем в отдельном окружении
                    module_env = Environment(self.global_env)
                    ast_dict = ast.to_dict() if hasattr(ast, 'to_dict') else ast
                    
                    # After parsing the module's AST, add filename info to function nodes
                    self._add_filename_to_functions(ast_dict, str(found_file))
                    
                    # Создаём временный интерпретатор для модуля
                    module_interp = Interpreter(str(found_file))
                    module_interp.global_env = module_env
                    module_interp.functions = self.functions
                    module_interp.loaded_modules = self.loaded_modules
                    module_interp._loading_modules = self._loading_modules
                    
                    # FIX: Execute module and catch errors with proper filename
                    try:
                        module_interp.evaluate(ast_dict, module_env)
                    except RuntimeError as e:
                        # Re-raise with the module's filename preserved
                        raise
                    except Exception as e:
                        # Wrap other exceptions with proper error info
                        raise RuntimeError(
                            f"Runtime Error in {str(found_file)}: {str(e)}"
                        ) from e
                    
                    self.loaded_modules[cache_key] = module_env
                    
                except RuntimeError:
                    # Re-raise RuntimeError as-is (it already has correct filename)
                    raise
                except Exception as e:
                    self.error(f"Error loading module '{module_path}': {e}", node)
            
            # Экспортируем имена в текущее окружение
            if specific_imports:
                for name in specific_imports:
                    try:
                        value = module_env.get(name)
                        env.define(name, value)
                    except NameError:
                        self.error(
                            f"Module '{module_path}' does not export '{name}'",
                            node
                        )
            else:
                env.define(module_path.split('.')[0], module_env)
        
        finally:
            self._loading_modules.discard(module_path)
    
    # ========== Объявления ==========
    
    def eval_variable_decl(self, node: dict, env: Environment) -> Any:
        name = node.get('value', '')
        value = None
        if node.get('children'):
            value = self.evaluate(node['children'][0], env)
        env.define(name, value)
        return value
    
    def eval_function_decl(self, node: dict, env: Environment) -> None:
        name = node.get('value', '')
        # Store the function node AND the environment where it was defined (closure)
        func_info = {
            'node': node,
            'closure': env  # Capture the defining environment!
        }
        self.functions[name] = func_info
        # Store the full func_info in the environment, not just the node
        env.define(name, func_info)

    def _add_filename_to_functions(self, node: dict, filename: str):
        """Recursively add filename to all function declarations in AST"""
        if not isinstance(node, dict):
            return
        
        if node.get('type') == 'FUNCTION_DECL':
            node['filename'] = filename
        
        for child in node.get('children', []):
            if isinstance(child, dict):
                self._add_filename_to_functions(child, filename)
        
        for key, value in node.get('properties', {}).items():
            if isinstance(value, dict):
                self._add_filename_to_functions(value, filename)
            elif isinstance(value, list):
                for item in value:
                    if isinstance(item, dict):
                        self._add_filename_to_functions(item, filename)

    # ========== Управляющие конструкции ==========
        
    def eval_if_stmt(self, node: dict, env: Environment) -> Any:
        condition = self.evaluate(node['children'][0], env)
        
        if self._is_truthy(condition):
            return self.evaluate(node['children'][1], env)
        
        elif_conditions = node.get('properties', {}).get('elif_conditions', [])
        elif_blocks = node.get('properties', {}).get('elif_blocks', [])
        for i, elif_cond in enumerate(elif_conditions):
            if self._is_truthy(self.evaluate(elif_cond, env)):
                return self.evaluate(elif_blocks[i], env)
        
        # else блок — это последний child (третий)
        if len(node.get('children', [])) > 2:
            return self.evaluate(node['children'][2], env)
        
        return None
        
    def eval_while_stmt(self, node: dict, env: Environment) -> Any:
        result = None
        while self._is_truthy(self.evaluate(node['children'][0], env)):
            try:
                result = self.evaluate(node['children'][1], env)
            except Break:
                break
            except Continue:
                continue
        return result
    
    def eval_for_stmt(self, node: dict, env: Environment) -> Any:
        var_name = node.get('properties', {}).get('variable', '')
        children = node.get('children', [])
        
        if len(children) < 2:
            self.error("Invalid for statement", node)
        
        start = self.evaluate(children[0], env)
        end = self.evaluate(children[1], env)
        step = 1
        body_idx = 2
        
        if len(children) > 3:
            step = self.evaluate(children[2], env)
            body_idx = 3
        
        body = children[body_idx] if body_idx < len(children) else None
        
        if body is None:
            self.error("For loop has no body", node)
        
        result = None
        current = start
        while current <= end:
            env.define(var_name, current)
            try:
                result = self.evaluate(body, env)
            except Break:
                break  # <-- тут был просто pass, нужен break!
            except Continue:
                pass
            current += step
        
        return result
    
    def eval_for_in_stmt(self, node: dict, env: Environment) -> Any:
        var_name = node.get('properties', {}).get('variable', '')
        collection = self.evaluate(node['children'][0], env)
        
        result = None
        if isinstance(collection, Table):
            items = list(collection.items.values())
        elif isinstance(collection, list):
            items = collection
        else:
            self.error("Can only iterate over tables or lists", node)
        
        for item in items:
            env.define(var_name, item)  # define для каждой итерации
            try:
                result = self.evaluate(node['children'][1], env)
            except Break:
                break
            except Continue:
                pass
        
        return result
    
    def eval_break_stmt(self, node: dict, env: Environment) -> None:
        raise Break()
    
    def eval_continue_stmt(self, node: dict, env: Environment) -> None:
        raise Continue()
    
    def eval_return_stmt(self, node: dict, env: Environment) -> None:
        value = None
        if node.get('children'):
            value = self.evaluate(node['children'][0], env)
        raise Return(value)
        
    def eval_try_stmt(self, node: dict, env: Environment) -> Any:
        children = node.get('children', [])
        
        try_body = children[0] if len(children) > 0 else None
        failure_body = children[1] if len(children) > 1 else None
        always_body = children[2] if len(children) > 2 else None
        
        result = None
        try:
            if try_body:
                result = self.evaluate(try_body, env)
        except Exception as e:
            if failure_body:
                # Создаём переменную с ошибкой в скоупе failure-блока
                env.define("error", str(e))
                result = self.evaluate(failure_body, env)
            else:
                raise  # Перепробрасываем, если нет failure
        finally:
            if always_body:
                try:
                    self.evaluate(always_body, env)
                except Exception:
                    pass  # always не должен крашить программу
        
        return result
            
    # ========== Выражения ==========
    
    def eval_expression_stmt(self, node: dict, env: Environment) -> Any:
        if node.get('children'):
            return self.evaluate(node['children'][0], env)
        return None
    
    def eval_binary_expr(self, node: dict, env: Environment) -> Any:
        left = self.evaluate(node['children'][0], env)
        right = self.evaluate(node['children'][1], env)
        op = node.get('value', '')
        
        if op == '+':
            # Только числа!
            if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                return left + right
            self.error(f"Cannot add {type(left).__name__} and {type(right).__name__}", node)
        
        if op == '-': return left - right
        if op == '*':
            if isinstance(left, str) and isinstance(right, int):
                return left * right
            return left * right
        if op == '/': return left / right
        if op == '%': return left % right
        if op == '==': return left == right
        if op == '!=': return left != right
        if op == '<': return left < right
        if op == '>': return left > right
        if op == '<=': return left <= right
        if op == '>=': return left >= right
        
        self.error(f"Unknown operator: {op}", node)
    
    def eval_unary_expr(self, node: dict, env: Environment) -> Any:
        operand = self.evaluate(node['children'][0], env)
        op = node.get('value', '')
        
        if op == '-': return -operand
        if op == 'not': return not self._is_truthy(operand)
        
        self.error(f"Unknown unary operator: {op}", node)
    
    def eval_logical_expr(self, node: dict, env: Environment) -> Any:
        left = self.evaluate(node['children'][0], env)
        op = node.get('value', '')
        
        if op == 'and':
            if not self._is_truthy(left):
                return left
            return self.evaluate(node['children'][1], env)
        
        if op == 'or':
            if self._is_truthy(left):
                return left
            return self.evaluate(node['children'][1], env)
        
        self.error(f"Unknown logical operator: {op}", node)
    
    def eval_assignment_expr(self, node: dict, env: Environment) -> Any:
        target = node['children'][0]
        value = self.evaluate(node['children'][1], env)
        
        if target.get('type') == 'IDENTIFIER':
            name = target.get('value', '')
            # Проверяем, существует ли переменная
            try:
                env.assign(name, value)
            except NameError:
                env.define(name, value)
            return value
        
        if target.get('type') == 'MEMBER_EXPR':
            # table.field = value или table[index] = value
            obj = self.evaluate(target['children'][0], env)
            member = target.get('value', '')
            
            if isinstance(obj, Table):
                # Пробуем как числовой индекс
                try:
                    index = int(member)
                    obj.set(index, value)
                    return value
                except ValueError:
                    pass
                # Как строковый ключ
                obj.set(member, value)
                return value
            
            self.error(f"Cannot set property on {type(obj).__name__}", node)
        
        self.error("Invalid assignment target", node)
    
    def eval_call_expr(self, node: dict, env: Environment) -> Any:
        callee = self.evaluate(node['children'][0], env)
        
        # Collect arguments
        args = []
        for i in range(1, len(node.get('children', []))):
            args.append(self.evaluate(node['children'][i], env))
        
        # If callee is a string (function name), look it up
        if isinstance(callee, str):
            func_info = self.functions.get(callee)
            if func_info:
                return self._call_function(func_info, args, env)
            self.error(f"Function '{callee}' is not defined", node)
        
        # If callee is a dict (function info with closure or function node)
        if isinstance(callee, dict):
            if 'closure' in callee and 'node' in callee:
                # New format with closure
                return self._call_function(callee, args, env)
            if callee.get('type') == 'FUNCTION_DECL':
                # Function node - lookup in functions dict for closure
                func_name = callee.get('value', '')
                if func_name and func_name in self.functions:
                    return self._call_function(self.functions[func_name], args, env)
                # Fallback
                return self._call_function({'node': callee, 'closure': env}, args, env)
        
        # If callee is a dict (function info with closure or function node)
        if isinstance(callee, dict):
            if 'closure' in callee and 'node' in callee:
                # New format with closure
                return self._call_function(callee, args, env)
            if callee.get('type') == 'FUNCTION_DECL':
                # Function node - lookup in functions dict for closure
                func_name = callee.get('value', '')
                if func_name and func_name in self.functions:
                    return self._call_function(self.functions[func_name], args, env)
                # Fallback
                return self._call_function({'node': callee, 'closure': env}, args, env)

        if callable(callee):
            return callee(*args)

        self.error(f"'{callee}' is not callable", node)
    
    def _call_function(self, func_info: dict, args: list, env: Environment) -> Any:
        """
        Calls a function, handling both regular and module-scoped functions.
        func_info can be:
        - A dict with 'node' and 'closure' keys (new format with closures)
        - A dict that is the function node itself (old format)
        """
        # Extract function node and closure
        if 'closure' in func_info and 'node' in func_info:
            func_node = func_info['node']
            closure_env = func_info['closure']
        else:
            func_node = func_info
            closure_env = env
        
        params = func_node.get('properties', {}).get('params', [])
        
        # Create new scope with closure as parent (NOT the call site env)
        call_env = Environment(closure_env)
        
        # Bind arguments to parameters
        for i, param_name in enumerate(params):
            value = args[i] if i < len(args) else None
            call_env.define(param_name, value)
        
        # Save the current filename for error reporting
        old_filename = self.filename
        
        # Get the function's source file from the node
        func_filename = func_node.get('filename', self.filename)
        
        # Temporarily set the interpreter's filename to the function's file
        if func_filename:
            self.filename = func_filename
        
        try:
            # Execute function body
            body = func_node['children'][0]
            return self.evaluate(body, call_env)
        except Return as ret:
            return ret.value
        finally:
            # Restore the original filename
            self.filename = old_filename
        
    def eval_member_expr(self, node: dict, env: Environment) -> Any:
        obj = self.evaluate(node['children'][0], env)
        member = node.get('value', '')
        
        # Доступ к встроенному модулю: os.output, math.abs и т.д.
        if isinstance(obj, BuiltinModule):
            try:
                return obj.get(member)
            except AttributeError:
                self.error(f"Module '{obj.name}' has no member '{member}'", node)
        
        # Доступ к пользовательскому модулю (Environment)
        if isinstance(obj, Environment):
            try:
                return obj.get(member)
            except NameError:
                self.error(f"Module does not export '{member}'", node)
        
        if isinstance(obj, Table):
            # Пробуем как числовой индекс
            try:
                index = int(member)
                return obj.get(index)
            except ValueError:
                pass
            # Как строковый ключ
            return obj.get(member)
        
        if isinstance(obj, str):
            # Доступ к "методам" строки
            if member == 'length':
                return len(obj)
        
        if isinstance(obj, dict):
            if member in obj:
                return obj[member]
            # Пробуем как числовой индекс
            try:
                index = int(member)
                if index in obj:
                    return obj[index]
            except ValueError:
                pass
        
        self.error(f"Cannot access member '{member}' of {type(obj).__name__}", node)
    
    # ========== Литералы ==========
    
    def eval_number_literal(self, node: dict, env: Environment) -> int | float:
        return node.get('value', 0)
    
    def eval_string_literal(self, node: dict, env: Environment) -> str:
        raw_value = node.get('value', '')
        return self._interpolate_string(raw_value, env)
    
    def eval_boolean_literal(self, node: dict, env: Environment) -> bool:
        return node.get('value', False)
    
    def eval_none_literal(self, node: dict, env: Environment) -> None:
        return None
        
    def eval_identifier(self, node: dict, env: Environment) -> Any:
        name = node.get('value', '')
        try:
            return env.get(name)
        except NameError:
            # Check if it's a function
            if name in self.functions:
                return self.functions[name]
            self.error(f"Name '{name}' is not defined", node)
    
    def eval_table_literal(self, node: dict, env: Environment) -> 'Table':
        table = Table()
        
        for i, entry_node in enumerate(node.get('children', [])):
            entry_type = entry_node.get('type', '')
            
            if entry_type == 'TABLE_ENTRY':
                key = entry_node.get('properties', {}).get('key', None)
                value = self.evaluate(entry_node['children'][0], env)
                
                if key is not None:
                    table.set(key, value)
                else:
                    # Индексация с 1 как в вашем языке
                    table.set(i + 1, value)
        
        return table
    
    def eval_table_entry(self, node: dict, env: Environment) -> Any:
        # Обычно вызывается из eval_table_literal
        return self.evaluate(node['children'][0], env)
    
    # ========== Вспомогательные методы ==========
    
    def _is_truthy(self, value: Any) -> bool:
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

    def _interpolate_string(self, s: str, env: Environment) -> str:
        """Заменяет {variable} и {table.field} на значения из окружения"""
        result = ""
        i = 0
        while i < len(s):
            if s[i] == '{':
                end = s.find('}', i)
                if end != -1:
                    expr = s[i+1:end]
                    
                    # Проверяем, что это допустимое выражение для интерполяции
                    if not self._is_valid_interpolation(expr):
                        self.error(
                            f"Invalid interpolation '{{{expr}}}'. "
                            "Only variable names and field access (a.b.c) are allowed inside {{}}.",
                            {"line": self.current_line}
                        )
                    
                    try:
                        # Разбиваем на части по точке
                        parts = expr.split('.')
                        # Начинаем с переменной
                        value = env.get(parts[0])
                        
                        # Проверяем, что parts[0] — это идентификатор
                        if not parts[0].replace('_', '').isalnum():
                            self.error(
                                f"Invalid expression in interpolation: '{{{expr}}}'. "
                                "Only variables and field access are allowed.",
                                {"line": self.current_line}
                            )
                        
                        # Проходим по цепочке полей/индексов
                        for part in parts[1:]:
                            if isinstance(value, Environment):
                                # MODULE ENVIRONMENT: get the variable from module's scope
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
                        # Переменная не найдена — оставляем как есть
                        result += '{' + expr + '}'
                    except Exception:
                        result += '{' + expr + '}'
                    i = end + 1
                    continue
            result += s[i]
            i += 1
        return result

    def _is_valid_interpolation(self, expr: str) -> bool:
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
            # Проверяем, что каждая часть — это идентификатор или число (индекс таблицы)
            if part in keywords:
                return False
            # Должно быть: буква/подчёркивание + буквы/цифры/подчёркивание
            # Или чистое число (для индексов таблиц)
            if not part.isdigit():
                if not part or not (part[0].isalpha() or part[0] == '_'):
                    return False
                for ch in part:
                    if not (ch.isalnum() or ch == '_'):
                        return False
        
        return True

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

# ========== Запуск ==========

def read_ast_from_file(filepath: str) -> Optional[dict]:
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Если файл содержит и токены, и AST (после ---AST---)
        if '---AST---' in content:
            content = content.split('---AST---')[1]
        
        return json.loads(content)
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return None

def main():
    cache_dir = Path('.cache')
    
    if not cache_dir.exists():
        print("Error: .cache directory not found.")
        sys.exit(1)
    
    if len(sys.argv) > 1:
        filename = sys.argv[1]
        base_name = Path(filename).stem
        cached_file = cache_dir / f"{base_name}.apexc"
        
        if not cached_file.exists():
            print(f"Error: {cached_file} not found. Run parser first.")
            sys.exit(1)
        
        files = [(cached_file, filename)]
    else:
        apexc_files = list(cache_dir.glob("*.apexc"))
        files = []
        for f in apexc_files:
            try:
                with open(f, 'r', encoding='utf-8') as test:
                    content = test.read()
                    if content.strip().startswith('{'):
                        files.append((f, f.stem + '.apex'))
            except Exception:
                pass
        
        if not files:
            print("No cached AST files found in .cache/")
            sys.exit(1)
    
    for filepath, display_name in files:
        ast = read_ast_from_file(str(filepath))
        
        if ast is None:
            print(f"Failed to read AST from {filepath}")
            continue
        
        interpreter = Interpreter(display_name)
        
        try:
            result = interpreter.evaluate(ast)
            if result is not None:
                print(f"Result: {result}")
            else:
                print("[DEBUG] Program finished with no return value")
        except Exception as e:
            import traceback
            print(f"Runtime Error: {e}")
            traceback.print_exc()

if __name__ == "__main__":
    main()