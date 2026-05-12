#include "tvshow/app/application.hpp"

auto main() -> int {
    tvshow::app::Application app;
    app.run();
    app.shutDown();
    return 0;
}
