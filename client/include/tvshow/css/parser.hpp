#pragma once

#include "tvshow/css/types.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace tvshow::css {

// Parse a CSS stylesheet. Returns nullopt only on catastrophic internal failure;
// katana is lenient and ignores unknown rules/properties gracefully.
[[nodiscard]] std::optional<Stylesheet> parse(std::string_view css);

// Parse CSS inline declarations (the value of a style="..." attribute).
// Returns an empty vector on failure.
[[nodiscard]] std::vector<Declaration> parse_inline(std::string_view style);

}  // namespace tvshow::css
