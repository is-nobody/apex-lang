# source/core/token.py
from dataclasses import dataclass
from source.core.parser.token_type import TokenType

@dataclass
class Token:
    type: TokenType
    value: str
    line: int
    column: int
    
    def __repr__(self):
        return f"Token({self.type.name}, '{self.value}', line={self.line}, col={self.column})"