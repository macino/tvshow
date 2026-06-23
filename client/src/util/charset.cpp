#include "tvshow/util/charset.hpp"

#include <iconv.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <string>
#include <string_view>

namespace tvshow::util {

namespace {

std::string to_lower(std::string_view s) {
    std::string out(s.size(), '\0');
    for (size_t i = 0; i < s.size(); ++i) {
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    }
    return out;
}

bool is_utf8_compatible(std::string_view charset) noexcept {
    const std::string lc = to_lower(charset);
    return lc.empty() || lc == "utf-8" || lc == "utf8" || lc == "us-ascii" || lc == "ascii";
}

}  // namespace

std::string transcode_to_utf8(std::string_view src, std::string_view from_charset) {
    if (is_utf8_compatible(from_charset)) {
        return std::string(src);
    }

    const std::string from(from_charset);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    iconv_t cd = iconv_open("UTF-8", from.c_str());
    if (cd == reinterpret_cast<iconv_t>(-1)) {
        return std::string(src);  // unknown charset — return as-is
    }

    std::string in_buf(src);
    char* in_ptr = in_buf.data();
    size_t in_left = in_buf.size();

    std::string out;
    out.reserve(src.size() * 2);

    constexpr size_t k_buf_sz = 4096;
    char buf[k_buf_sz];

    while (in_left > 0) {
        char* out_ptr = buf;
        size_t out_left = k_buf_sz;
        const size_t res = iconv(cd, &in_ptr, &in_left, &out_ptr, &out_left);
        out.append(buf, k_buf_sz - out_left);
        if (res == static_cast<size_t>(-1)) {
            if (errno == EILSEQ || errno == EINVAL) {
                // Invalid or incomplete sequence: skip one byte, emit U+FFFD.
                if (in_left > 0) {
                    ++in_ptr;
                    --in_left;
                    out += '\xEF';
                    out += '\xBF';
                    out += '\xBD';
                }
                iconv(cd, nullptr, nullptr, nullptr, nullptr);  // reset shift state
            } else {
                break;
            }
        }
    }

    iconv_close(cd);
    return out;
}

std::string prescan_charset(std::string_view html_bytes) {
    constexpr size_t k_scan_limit = 2048;
    const std::string_view scan = html_bytes.substr(0, k_scan_limit);

    // Build a lowercase copy for case-insensitive search.
    const std::string lc = to_lower(scan);

    size_t pos = 0;
    while ((pos = lc.find("charset=", pos)) != std::string::npos) {
        pos += 8;  // skip "charset="
        if (pos >= lc.size()) {
            break;
        }
        // Skip optional quote in the ORIGINAL source (not lc, to preserve case).
        const char quote =
            (pos < scan.size() && (scan[pos] == '"' || scan[pos] == '\'')) ? scan[pos] : '\0';
        if (quote != '\0') {
            ++pos;
        }
        if (pos >= scan.size()) {
            break;
        }
        const size_t start = pos;
        while (pos < scan.size()) {
            const char c = scan[pos];
            if (quote != '\0') {
                if (c == quote) {
                    break;
                }
            } else if (c == ';' || c == '"' || c == '\'' || c == '>' || c == ' ' ||
                       c == '\t' || c == '\r' || c == '\n') {
                break;
            }
            ++pos;
        }
        if (pos > start) {
            return std::string(scan.substr(start, pos - start));
        }
    }
    return {};
}

}  // namespace tvshow::util
