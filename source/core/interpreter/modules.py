# source/core/interpreter/modules.py
import sys
import json
from pathlib import Path
from typing import Any, Optional

from source.libraries import BUILTIN_MODULES
from source.core.interpreter.environment import Environment
from source.core.interpreter.helpers import add_filename_to_functions

class ModulesMixin:
    """Mixin for evaluating import statements and loading modules"""
    
    def eval_import(self, node: dict, env) -> None:
        module_path = node.get('value', '')
        self._load_module(module_path, None, env, node)

    def eval_import_specific(self, node: dict, env) -> None:
        module_path = node.get('value', '')
        imports = node.get('properties', {}).get('imports', [])
        self._load_module(module_path, imports, env, node)

    def _load_module(self, module_path: str, specific_imports: Optional[list], 
                    env, node: dict):
        """Загружает модуль — встроенный или пользовательский"""
        
        if not hasattr(self, '_loading_modules'):
            self._loading_modules = set()
        
        if module_path in self._loading_modules:
            self.error(f"Circular import detected: '{module_path}'", node)
        
        self._loading_modules.add(module_path)
        
        try:
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
            
            module_file = Path(module_path.replace('.', '/') + '.apex')
            
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
            
            cache_key = str(found_file.resolve())
            if cache_key in self.loaded_modules:
                module_env = self.loaded_modules[cache_key]
            else:
                try:
                    with open(found_file, 'r') as f:
                        source = f.read()
                    
                    from source.core.tokenizer import Tokenizer
                    tokenizer = Tokenizer(source, str(found_file))
                    raw_tokens = tokenizer.tokenize()

                    from source.core.parser_main import Token as ParserToken, TokenType as ParserTokenType
                    parser_tokens = [
                        ParserToken(
                            type=ParserTokenType[t.type.name],
                            value=t.value,
                            line=t.line,
                            column=t.column
                        )
                        for t in raw_tokens
                    ]

                    from source.core.parser_main import Parser
                    parser = Parser(parser_tokens, str(found_file))
                    ast = parser.parse()
                    
                    if ast is None:
                        self.error(f"Failed to parse module '{module_path}'", node)
                    
                    module_env = Environment(self.global_env)
                    ast_dict = ast.to_dict() if hasattr(ast, 'to_dict') else ast
                    
                    add_filename_to_functions(ast_dict, str(found_file))
                    
                    module_interp = self.__class__(str(found_file))
                    module_interp.global_env = module_env
                    module_interp.functions = self.functions
                    module_interp.loaded_modules = self.loaded_modules
                    module_interp._loading_modules = self._loading_modules
                    
                    try:
                        module_interp.evaluate(ast_dict, module_env)
                    except RuntimeError:
                        raise
                    except Exception as e:
                        raise RuntimeError(
                            f"Runtime Error in {str(found_file)}: {str(e)}"
                        ) from e
                    
                    self.loaded_modules[cache_key] = module_env
                    
                except RuntimeError:
                    raise
                except Exception as e:
                    self.error(f"Error loading module '{module_path}': {e}", node)
            
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