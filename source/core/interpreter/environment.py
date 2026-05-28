from typing import Any, Optional

class Return(Exception):
    def __init__(self, value):
        self.value = value

class Break(Exception):
    pass

class Continue(Exception):
    pass

class Environment:
    def __init__(self, parent: Optional['Environment'] = None):
        self.variables: dict[str, Any] = {}
        self.types: dict[str, str] = {}
        self.constants: set[str] = set()
        self.parent = parent

    def _get_type_category(self, value: Any) -> str:
        if isinstance(value, bool):
            return "boolean"
        if isinstance(value, (int, float)):
            return "number"
        if isinstance(value, str):
            return "string"
        if type(value).__name__ == 'Table':
            return "table"
        if isinstance(value, list):
            return "list"
        if isinstance(value, dict):
            return "dict"
        if callable(value):
            return "function"
        return type(value).__name__

    def _category_display_name(self, category: str) -> str:
        names = {
            "boolean": "boolean",
            "number": "number",
            "string": "string",
            "table": "table",
            "list": "list",
            "dict": "dict",
            "function": "function",
        }
        return names.get(category, category)

    def define(self, name: str, value: Any, is_constant: bool = False):
        self.variables[name] = value
        self.types[name] = self._get_type_category(value)
        if is_constant:
            self.constants.add(name)

    def assign(self, name: str, value: Any):
        # check if it's a constant
        if name in self.constants:
            raise RuntimeError(f"Cannot assign to read-only variable '{name}'")

        if name in self.variables:
            expected_category = self.types.get(name)
            actual_category = self._get_type_category(value)

            if expected_category == "none" and actual_category != "none":
                self.types[name] = actual_category
            elif expected_category != actual_category:
                expected_name = self._category_display_name(expected_category)
                actual_name = self._category_display_name(actual_category)
                raise TypeError(
                    f"Cannot assign {actual_name} to variable '{name}' of type {expected_name}"
                )

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