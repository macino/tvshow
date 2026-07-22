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

// A <input type="file"> field's picked path (SPEC Q-28). No file content
// here -- reading the file from disk is I/O, done by the caller (app layer);
// this module stays pure.
struct FormFileRef {
    std::string name;
    std::string path;  // full path as picked via the file dialog
};

// Already-loaded content for one file field, ready for multipart encoding.
struct FormFilePart {
    std::string field_name;
    std::string filename;      // basename only (last path component)
    std::string content_type;  // e.g. "application/octet-stream"
    std::string content;       // raw file bytes
};

// Collect form control name/value pairs from a <form> element's DOM subtree
// per SPEC §13.1. text_values and checked_values shadow the DOM attr defaults
// with live user input (the same maps stored in render::FormValues).
// <input type="file"> fields are excluded (see collect_form_files below).
[[nodiscard]] std::vector<FormField>
collect_form_fields(const dom::Node& form,
                    const std::unordered_map<const dom::Node*, std::string>& text_values,
                    const std::unordered_map<const dom::Node*, bool>& checked_values);

// Collect <input type="file"> fields with a non-empty picked path from a
// <form> element's DOM subtree. text_values is the same map passed to
// collect_form_fields (file inputs store their picked path there, reusing
// the generic text-value store).
[[nodiscard]] std::vector<FormFileRef>
collect_form_files(const dom::Node& form,
                   const std::unordered_map<const dom::Node*, std::string>& text_values);

// Percent-encode a string for application/x-www-form-urlencoded.
// Space encodes as '+'; other non-unreserved chars use %HH.
[[nodiscard]] std::string url_encode(std::string_view s);

// Encode a field list as "name=value&name=value&..." (application/x-www-form-urlencoded).
[[nodiscard]] std::string encode_fields(const std::vector<FormField>& fields);

// Encodes fields and files as a multipart/form-data body (SPEC Q-28).
// boundary must not appear in any field value or file content (the caller
// generates it -- picking one is not this pure module's job, see
// BrowserView::submit_form).
[[nodiscard]] std::string encode_multipart(const std::vector<FormField>& fields,
                                           const std::vector<FormFilePart>& files,
                                           std::string_view boundary);

}  // namespace tvshow::layout
