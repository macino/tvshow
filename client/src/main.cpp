#include "tvshow/app/application.hpp"

auto main(int argc, char** argv) -> int {
    tvshow::app::Application app;
    if (argc > 1) {
        tvshow::app::Application::open_file_url(argv[1]);
    }
    app.run();
    app.shutDown();
    return 0;
}
