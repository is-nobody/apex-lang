# source/core/ast.py
from enum import Enum, auto
from dataclasses import dataclass, field
from typing import Any

class ASTNodeType(Enum):
    PROGRAM = auto()
    IMPORT = auto()
    VARIABLE_DECL = auto()
    FUNCTION_DECL = auto()
    EXPRESSION_STMT = auto()
    BLOCK = auto()
    IF_STMT = auto()
    WHILE_STMT = auto()
    FOR_STMT = auto()
    FOR_IN_STMT = auto()
    BREAK_STMT = auto()
    CONTINUE_STMT = auto()
    RETURN_STMT = auto()
    TRY_STMT = auto()
    BINARY_EXPR = auto()
    UNARY_EXPR = auto()
    CALL_EXPR = auto()
    MEMBER_EXPR = auto()
    ASSIGNMENT_EXPR = auto()
    LOGICAL_EXPR = auto()
    RANGE_EXPR = auto()
    NUMBER_LITERAL = auto()
    STRING_LITERAL = auto()
    BOOLEAN_LITERAL = auto()
    IDENTIFIER = auto()
    TABLE_LITERAL = auto()
    TABLE_ENTRY = auto()

@dataclass
class ASTNode:
    type: ASTNodeType
    value: Any = None
    children: list = field(default_factory=list)
    properties: dict = field(default_factory=dict)
    line: int = 0
    column: int = 0
    
    def to_dict(self):
        result = {
            "type": self.type.name,
            "line": self.line,
            "column": self.column
        }
        if self.value is not None:
            result["value"] = self.value
        if self.properties:
            result["properties"] = {}
            for key, val in self.properties.items():
                if isinstance(val, ASTNode):
                    result["properties"][key] = val.to_dict()
                elif isinstance(val, list):
                    result["properties"][key] = [
                        item.to_dict() if isinstance(item, ASTNode) else item 
                        for item in val
                    ]
                else:
                    result["properties"][key] = val
        if self.children:
            result["children"] = [child.to_dict() if isinstance(child, ASTNode) else child for child in self.children]
        return result