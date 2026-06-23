#include "tvshow/app/application.hpp"
#include "tvshow/app/browser_window.hpp"
#include "tvshow/util/config.hpp"
#include "tvshow/util/log.hpp"

#include <cstdio>
#include <exception>
#include <string>
#include <string_view>

auto main(int argc, char** argv) -> int {
    // Config file is the baseline; CLI flags override.
    std::string_view config_path;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg.starts_with("--config=")) { config_path = arg.substr(9); }
    }
    const tvshow::util::Config cfg = tvshow::util::load_config(config_path);

    tvshow::app::AddressBarMode mode =
        (cfg.address_bar == "persistent") ? tvshow::app::AddressBarMode::Persistent
                                          : tvshow::app::AddressBarMode::Modal;
    tvshow::util::log::Level log_level = tvshow::util::log::parse_level(cfg.log_level);
    std::string initial_url = cfg.start_url;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--address-bar=persistent") {
            mode = tvshow::app::AddressBarMode::Persistent;
        } else if (arg == "--address-bar=modal") {
            mode = tvshow::app::AddressBarMode::Modal;
        } else if (arg.starts_with("--log-level=")) {
            log_level = tvshow::util::log::parse_level(arg.substr(12));
        } else if (arg.starts_with("--config=")) {
            // already handled above
        } else if (!arg.starts_with("--")) {
            initial_url = std::string(arg);
        }
    }

    tvshow::util::log::init(log_level);

    tvshow::app::Application app(mode);
    try {
        if (!initial_url.empty()) {
            app.open_url(initial_url);
        }
        app.run();
    } catch (const std::exception& ex) {
        app.shutDown();
        std::fprintf(stderr, "tvshow: fatal error: %s\n", ex.what());
        return 1;
    } catch (...) {
        app.shutDown();
        std::fprintf(stderr, "tvshow: unknown fatal error\n");
        return 1;
    }
    app.shutDown();
    return 0;
}
