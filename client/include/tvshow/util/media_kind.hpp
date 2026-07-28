#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tvshow::util {

// adr-external-handlers: classifies a URL by file extension so Enter over a
// media link-out token (SPEC Q-30) can dispatch to an external handler
// instead of trying to navigate/parse it as HTML. Query strings and
// fragments are ignored when locating the extension.
enum class MediaKind { None, Video, Audio };

[[nodiscard]] MediaKind classify_media_url(std::string_view url) noexcept;

// Splits `command_template` on whitespace into argv tokens, substituting any
// token that is exactly "%s" with `url` verbatim (adr-external-handlers).
// No shell interpolation anywhere — this is the argv fork+execvp will use
// directly, so a crafted URL can't inject shell metacharacters. Empty
// template -> empty result (caller treats that as "no handler").
[[nodiscard]] std::vector<std::string> build_handler_argv(std::string_view command_template,
                                                           std::string_view url);

}  // namespace tvshow::util
