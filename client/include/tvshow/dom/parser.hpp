#pragma once

#include "tvshow/dom/node.hpp"

#include <optional>
#include <string_view>

namespace tvshow::dom {

// Parse an HTML document. charset is the encoding declared by the HTTP layer
// (defaults to "utf-8"). Gumbo is error-tolerant; this returns nullopt only on
// catastrophic internal failure, not on malformed markup.
[[nodiscard]] std::optional<Document> parse(std::string_view html,
                                            std::string_view charset = "utf-8");

}  // namespace tvshow::dom
