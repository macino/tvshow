#!/usr/bin/env python3
"""tvshow window-provider example: a calculator with real buttons.

Speaks the adr-extension-ui-protocol structured UI mode (a mode of the
adr-external-window-provider protocol): print UI_INIT first, then describe
BUTTON/TEXT widgets; tvshow renders them as real TButton/LabelView views and
sends "CLICK id=<n>" lines back over stdin when a button is pressed. See
extensions/README.md for the full protocol grammar.

Usage in ~/.config/tvshow/config.toml:
    window-provider-calculator = "/path/to/tvshow/extensions/calculator/calculator.py"

No eval() -- expressions are parsed into an AST and walked manually so
arbitrary code can't be smuggled in through a crafted button sequence.
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


def esc(s: str) -> str:
    """Escape a value for the UI protocol's quoted-string fields."""
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


# Button layout: id -> (label, row, col). row/col map to real (x, y) below.
_KEYS = [
    (1, "C", 0, 0), (2, "(", 0, 1), (3, ")", 0, 2), (4, "/", 0, 3),
    (5, "7", 1, 0), (6, "8", 1, 1), (7, "9", 1, 2), (8, "*", 1, 3),
    (9, "4", 2, 0), (10, "5", 2, 1), (11, "6", 2, 2), (12, "-", 2, 3),
    (13, "1", 3, 0), (14, "2", 3, 1), (15, "3", 3, 2), (16, "+", 3, 3),
    (17, "0", 4, 0), (18, ".", 4, 1), (19, "<-", 4, 2), (20, "=", 4, 3),
]
_DISPLAY_ID = 100
_COL_W = 6
_ROW_Y0 = 2  # display occupies y=0


def emit_display(expr: str) -> None:
    shown = expr if expr else "0"
    print(f'TEXT id={_DISPLAY_ID} x=0 y=0 w=24 value="{esc(shown)}"', flush=True)


def main() -> None:
    print("UI_INIT", flush=True)
    print('TITLE value="Calculator"', flush=True)
    for kid, label, row, col in _KEYS:
        x = col * _COL_W
        y = _ROW_Y0 + row * 2
        print(f'BUTTON id={kid} x={x} y={y} w=5 label="{esc(label)}"', flush=True)

    expr = ""
    emit_display(expr)

    key_by_id = {kid: label for kid, label, _row, _col in _KEYS}

    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("CLICK id="):
            continue
        try:
            kid = int(line[len("CLICK id="):])
        except ValueError:
            continue
        label = key_by_id.get(kid)
        if label is None:
            continue

        if label == "C":
            expr = ""
        elif label == "<-":
            expr = expr[:-1]
        elif label == "=":
            try:
                expr = str(calculate(expr))
            except (CalcError, SyntaxError, ZeroDivisionError, ValueError, TypeError) as exc:
                expr = f"error: {exc}"
        else:
            expr += label
        emit_display(expr)


if __name__ == "__main__":
    main()
