#include "tvshow/util/config.hpp"

#include <doctest/doctest.h>

using tvshow::util::parse_config;

TEST_CASE("parse_config: defaults when empty") {
    const auto cfg = parse_config("");
    CHECK(cfg.log_level == "warn");
    CHECK(cfg.address_bar == "modal");
    CHECK(cfg.start_url.empty());
    CHECK(cfg.image_renderer == "alt");
}

TEST_CASE("parse_config: image-renderer key") {
    const auto cfg = parse_config("image-renderer = \"braille\"\n");
    CHECK(cfg.image_renderer == "braille");
}

TEST_CASE("parse_config: quoted string values") {
    const auto cfg = parse_config(
        "log-level = \"debug\"\n"
        "address-bar = \"persistent\"\n"
        "start-url = \"http://localhost:8080/\"\n");
    CHECK(cfg.log_level == "debug");
    CHECK(cfg.address_bar == "persistent");
    CHECK(cfg.start_url == "http://localhost:8080/");
}

TEST_CASE("parse_config: bare (unquoted) string values") {
    const auto cfg = parse_config("log-level = warn\naddress-bar = modal\n");
    CHECK(cfg.log_level == "warn");
    CHECK(cfg.address_bar == "modal");
}

TEST_CASE("parse_config: comments and blank lines are ignored") {
    const auto cfg = parse_config(
        "# tvshow config\n"
        "\n"
        "log-level = \"info\"\n");
    CHECK(cfg.log_level == "info");
}

TEST_CASE("parse_config: inline comment on bare value is stripped") {
    const auto cfg = parse_config("log-level = warn # set level\n");
    CHECK(cfg.log_level == "warn");
}

TEST_CASE("parse_config: unknown keys are silently ignored") {
    const auto cfg = parse_config("unknown-key = \"value\"\nlog-level = \"error\"\n");
    CHECK(cfg.log_level == "error");
}

TEST_CASE("parse_config: whitespace around key and value is trimmed") {
    const auto cfg = parse_config("  log-level  =  \"info\"  \n");
    CHECK(cfg.log_level == "info");
}

TEST_CASE("parse_config: download-dir key (q-download-manager)") {
    const auto cfg = parse_config("download-dir = \"/home/user/Downloads\"\n");
    CHECK(cfg.download_dir == "/home/user/Downloads");
}

TEST_CASE("parse_config: download-dir defaults to empty") {
    const auto cfg = parse_config("");
    CHECK(cfg.download_dir.empty());
}

TEST_CASE("parse_config: handler-video/handler-audio keys (adr-external-handlers)") {
    const auto cfg = parse_config(
        "handler-video = \"mpv %s\"\n"
        "handler-audio = \"mpv --no-video %s\"\n");
    CHECK(cfg.handler_video == "mpv %s");
    CHECK(cfg.handler_audio == "mpv --no-video %s");
}

TEST_CASE("parse_config: window-provider-* keys (adr-external-window-provider)") {
    const auto cfg = parse_config(
        "window-provider-calculator = \"bc -l\"\n"
        "window-provider-translate = \"trans -b :cs\"\n");
    REQUIRE(cfg.window_providers.size() == 2);
    CHECK(cfg.window_providers[0].first == "calculator");
    CHECK(cfg.window_providers[0].second == "bc -l");
    CHECK(cfg.window_providers[1].first == "translate");
    CHECK(cfg.window_providers[1].second == "trans -b :cs");
}

TEST_CASE("parse_config: no window-provider entries yields empty vector") {
    const auto cfg = parse_config("log-level = \"info\"\n");
    CHECK(cfg.window_providers.empty());
}

TEST_CASE("parse_config: translator-script key (adr-translator-native-window)") {
    const auto cfg = parse_config("translator-script = \"/path/to/translator.py\"\n");
    CHECK(cfg.translator_script == "/path/to/translator.py");
}

TEST_CASE("parse_config: translator-script defaults to empty") {
    const auto cfg = parse_config("");
    CHECK(cfg.translator_script.empty());
}

TEST_CASE("parse_config: extension-server-port key (adr-extension-server)") {
    const auto cfg = parse_config("extension-server-port = 9000\n");
    CHECK(cfg.extension_server_port == 9000);
}

TEST_CASE("parse_config: extension-server-port defaults to 8765") {
    const auto cfg = parse_config("");
    CHECK(cfg.extension_server_port == 8765);
}

TEST_CASE("parse_config: extension-server-port ignores non-numeric value") {
    const auto cfg = parse_config("extension-server-port = notanumber\n");
    CHECK(cfg.extension_server_port == 8765);
}
