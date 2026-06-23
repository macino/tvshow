#pragma once

#include <string>
#include <string_view>

namespace tvshow::util {

// Transcode src from from_charset to UTF-8 using POSIX iconv.
// Returns src unchanged if from_charset is empty, "utf-8", "utf8", or "us-ascii" (all
// case-insensitive), or if iconv cannot open the conversion (unknown charset).
// Invalid byte sequences are replaced with U+FFFD (three-byte UTF-8 sequence).
[[nodiscard]] std::string transcode_to_utf8(std::string_view src, std::string_view from_charset);

// Scan the first 2048 bytes of html_bytes for a charset= declaration inside a <meta> tag.
// Returns the raw (unmodified-case) charset name, or "" if none found.
// Caller should treat this result as lower-priority than a Content-Type header charset.
[[nodiscard]] std::string prescan_charset(std::string_view html_bytes);

}  // namespace tvshow::util
