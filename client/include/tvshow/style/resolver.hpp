#pragma once

#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/style/types.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace tvshow::style {

// Parse a CSS color value string. Returns Color{none=true} for unknown values.
[[nodiscard]] Color parse_color(std::string_view s) noexcept;

// Parse a CSS length value string. Returns Length{is_auto=true} for "auto" / unknown.
[[nodiscard]] Length parse_length(std::string_view s) noexcept;

// The built-in UA default stylesheet. Parsed once; returns a stable reference.
[[nodiscard]] const css::Stylesheet& ua_stylesheet();

// Resolve computed styles for the entire DOM tree.
// author_sheets: in cascade order (index 0 = lowest priority, last = highest).
// Returns a StyledNode tree rooted at doc.root (the <html> element).
// Returns std::nullopt only if doc.root is null.
[[nodiscard]] std::optional<StyledNode> resolve(const dom::Document& doc,
                                                std::span<const css::Stylesheet> author_sheets);

}  // namespace tvshow::style
