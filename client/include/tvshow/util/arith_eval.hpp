#pragma once

#include <optional>
#include <string_view>

namespace tvshow::util {

// Recursive-descent evaluator for the native Calculator window's keypad
// grammar: + - * / parens, unary minus. No exceptions across the module
// boundary (CLAUDE.md style) -- nullopt covers any parse error, unbalanced
// parens, or division by zero; the caller shows a generic "error".
//
//   expr   := term (('+' | '-') term)*
//   term   := factor (('*' | '/') factor)*
//   factor := '-' factor | primary
//   primary := number | '(' expr ')'
[[nodiscard]] std::optional<double> evaluate_arith(std::string_view expr) noexcept;

}  // namespace tvshow::util
