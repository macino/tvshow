#pragma once

#include "tvshow/dom/node.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tvshow::layout {

struct FormField {
    std::string name;
    std::string value;
};

// Collect form control name/value pairs from a <form> element's DOM subtree
// per SPEC §13.1. text_values and checked_values shadow the DOM attr defaults
// with live user input (the same maps stored in render::FormValues).
[[nodiscard]] std::vector<FormField>
collect_form_fields(const dom::Node& form,
                    const std::unordered_map<const dom::Node*, std::string>& text_values,
                    const std::unordered_map<const dom::Node*, bool>& checked_values);

// Percent-encode a string for application/x-www-form-urlencoded.
// Space encodes as '+'; other non-unreserved chars use %HH.
[[nodiscard]] std::string url_encode(std::string_view s);

// Encode a field list as "name=value&name=value&..." (application/x-www-form-urlencoded).
[[nodiscard]] std::string encode_fields(const std::vector<FormField>& fields);

}  // namespace tvshow::layout
