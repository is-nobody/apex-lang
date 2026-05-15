# source/core/token_type.py
from enum import Enum, auto

class TokenType(Enum):
    # Keywords
    FUNCTION = auto()
    IF = auto()
    ELIF = auto()
    ELSE = auto()
    WHILE = auto()
    FOR = auto()
    IN = auto()
    TO = auto()
    RANGE = auto()
    BREAK = auto()
    CONTINUE = auto()
    RETURN = auto()
    IMPORT = auto()
    TRY = auto()
    FAILURE = auto()
    ALWAYS = auto()
    AND = auto()
    OR = auto()
    NOT = auto()
    
    # Literals
    NONE = auto()
    TRUE = auto()
    FALSE = auto()
    NUMBER = auto()
    STRING = auto()
    IDENTIFIER = auto()
    
    # Operators
    PLUS = auto()
    MINUS = auto()
    STAR = auto()
    SLASH = auto()
    PERCENT = auto()
    EQUAL = auto()
    EQUAL_EQUAL = auto()
    NOT_EQUAL = auto()
    LESS = auto()
    GREATER = auto()
    LESS_EQUAL = auto()
    GREATER_EQUAL = auto()
    
    # Delimiters
    LPAREN = auto()
    RPAREN = auto()
    COMMA = auto()
    DOT = auto()
    COLON = auto()
    NEWLINE = auto()
    COMMENT = auto()
    
    # Special
    EOF = auto()