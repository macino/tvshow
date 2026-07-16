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
