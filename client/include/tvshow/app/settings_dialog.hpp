#pragma once

#include "tvshow/app/browser_view.hpp"

namespace tvshow::app {

// Show the Settings dialog modally. Reads current config from disk, lets the
// user edit it, and on OK saves to disk + applies live changes to shared state.
void show_settings_dialog(SharedBrowsingState& shared);

}  // namespace tvshow::app
