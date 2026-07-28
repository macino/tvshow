#pragma once

#include <string_view>

namespace tvshow::app {

// adr-external-handlers: fork+execvp `command_template` (argv built by
// util::build_handler_argv, no shell interpolation) with `url` substituted
// for "%s", then returns immediately — tvshow does not wait on or manage
// the child's window (fire-and-forget, e.g. an external video player).
// No-op if command_template is empty or the executable can't be found/run.
void spawn_handler(std::string_view command_template, std::string_view url);

}  // namespace tvshow::app
