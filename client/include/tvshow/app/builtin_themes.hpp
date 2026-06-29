#pragma once

namespace tvshow::app {

// Embedded CSS for the three built-in force-style overrides.
// These are the same files shipped with the demo server, inlined here so the
// client can apply them without a network round-trip.

extern const char* k_css_tvision;
extern const char* k_css_light;
extern const char* k_css_dark;

}  // namespace tvshow::app
