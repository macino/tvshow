#pragma once

#include <cstdint>
#include <string_view>

namespace tvshow::util::log {

enum class Level : uint8_t { Debug = 0, Info = 1, Warn = 2, Error = 3 };

// Parse a log level from a string ("debug", "info", "warn"/"warning", "error").
// Returns Warn if unrecognised.
[[nodiscard]] Level parse_level(std::string_view s) noexcept;

// Initialise logging. Call once from the main thread before navigating.
//   min_level: only messages at this level or above are written.
//   path:      "" → default path (~/.cache/tvshow/log)
//              "-" → stderr only (no file)
void init(Level min_level = Level::Warn, std::string_view path = "");

// Write a log message at the named level.
void debug(std::string_view msg);
void info(std::string_view msg);
void warn(std::string_view msg);
void error(std::string_view msg);

}  // namespace tvshow::util::log
