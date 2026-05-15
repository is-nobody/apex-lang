# source/libraries/ui_module.py
from .builtin_module import BuiltinModule


class UI(BuiltinModule):
    def __init__(self):
        super().__init__("ui")
        self._funcs = {
            "create_window": self.create_window,
            "create_button": self.create_button,
            "create_label": self.create_label,
            "create_input": self.create_input,
            "show_message": self.show_message,
            "set_title": self.set_title,
        }
    
    @staticmethod
    def create_window(title="Apex Window", width=800, height=600):
        print(f"Warning: ui.create_window('{title}', {width}, {height}) is not implemented yet")
        return None
    
    @staticmethod
    def create_button(text, x=0, y=0):
        print(f"Warning: ui.create_button('{text}', {x}, {y}) is not implemented yet")
        return None
    
    @staticmethod
    def create_label(text, x=0, y=0):
        print(f"Warning: ui.create_label('{text}', {x}, {y}) is not implemented yet")
        return None
    
    @staticmethod
    def create_input(x=0, y=0, width=200):
        print(f"Warning: ui.create_input({x}, {y}, {width}) is not implemented yet")
        return None
    
    @staticmethod
    def show_message(message, title="Message"):
        print(f"{title}: {message}")
    
    @staticmethod
    def set_title(title):
        print(f"Warning: ui.set_title('{title}') is not implemented yet")
        return None