#!/usr/bin/env python3
"""tvshow window-provider example: a basic calculator.

Speaks the adr-external-window-provider protocol: read one line from stdin,
write the result to stdout, flush, repeat. tvshow pipes ExtensionWindow's
input line to our stdin and appends whatever we write to its scrollback.

Usage in ~/.config/tvshow/config.toml:
    window-provider-calculator = "/path/to/tvshow/extensions/calculator/calculator.py"

Supports: + - * / // % ** parentheses, unary -, and a small function set
(sqrt, abs, round, min, max). No eval() -- expressions are parsed into an AST
and walked manually so arbitrary code can't be smuggled in through the input
line.
"""

import ast
import math
import operator
import sys

_BIN_OPS = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
    ast.Div: operator.truediv,
    ast.FloorDiv: operator.floordiv,
    ast.Mod: operator.mod,
    ast.Pow: operator.pow,
}
_UNARY_OPS = {
    ast.UAdd: operator.pos,
    ast.USub: operator.neg,
}
_FUNCS = {
    "sqrt": math.sqrt,
    "abs": abs,
    "round": round,
    "min": min,
    "max": max,
    "sin": math.sin,
    "cos": math.cos,
    "log": math.log,
}


class CalcError(Exception):
    pass


def eval_node(node):
    if isinstance(node, ast.Expression):
        return eval_node(node.body)
    if isinstance(node, ast.Constant):
        if isinstance(node.value, (int, float)):
            return node.value
        raise CalcError("only numeric constants allowed")
    if isinstance(node, ast.BinOp):
        op = _BIN_OPS.get(type(node.op))
        if op is None:
            raise CalcError(f"unsupported operator: {type(node.op).__name__}")
        return op(eval_node(node.left), eval_node(node.right))
    if isinstance(node, ast.UnaryOp):
        op = _UNARY_OPS.get(type(node.op))
        if op is None:
            raise CalcError(f"unsupported unary operator: {type(node.op).__name__}")
        return op(eval_node(node.operand))
    if isinstance(node, ast.Call):
        if not isinstance(node.func, ast.Name) or node.func.id not in _FUNCS:
            raise CalcError("unsupported function call")
        args = [eval_node(a) for a in node.args]
        return _FUNCS[node.func.id](*args)
    raise CalcError(f"unsupported expression: {type(node).__name__}")


def calculate(expr: str):
    tree = ast.parse(expr, mode="eval")
    return eval_node(tree)


def main() -> None:
    print("calculator ready -- type an expression, e.g. 2 * (3 + 4)", flush=True)
    for line in sys.stdin:
        expr = line.strip()
        if not expr:
            continue
        try:
            result = calculate(expr)
            print(f"= {result}", flush=True)
        except (CalcError, SyntaxError, ZeroDivisionError, ValueError, TypeError) as exc:
            print(f"error: {exc}", flush=True)


if __name__ == "__main__":
    main()
