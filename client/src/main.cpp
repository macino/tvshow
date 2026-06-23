#include "tvshow/app/application.hpp"
#include "tvshow/app/browser_window.hpp"

#include <cstdio>
#include <cstring>
#include <exception>
#include <string_view>

auto main(int argc, char** argv) -> int {
    tvshow::app::AddressBarMode mode = tvshow::app::AddressBarMode::Modal;
    std::string_view initial_url;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--address-bar=persistent") {
            mode = tvshow::app::AddressBarMode::Persistent;
        } else if (!arg.starts_with("--")) {
            initial_url = arg;
        }
    }

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
