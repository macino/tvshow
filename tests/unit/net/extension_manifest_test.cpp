#include "tvshow/net/extension_manifest.hpp"

#include <doctest/doctest.h>

using tvshow::net::build_cgi_request;
using tvshow::net::parse_cgi_response;
using tvshow::net::parse_extension_manifest;

TEST_CASE("parse_extension_manifest: name + entry, no install") {
    const auto m = parse_extension_manifest("name = \"calculator\"\nentry = \"python3 server.py\"\n");
    REQUIRE(m.has_value());
    CHECK(m->name == "calculator");
    CHECK(m->entry == "python3 server.py");
    CHECK(m->install.empty());
}

TEST_CASE("parse_extension_manifest: with install") {
    const auto m = parse_extension_manifest(
        "name = \"thing\"\nentry = \"./run.sh\"\ninstall = \"pip install -r requirements.txt\"\n");
    REQUIRE(m.has_value());
    CHECK(m->install == "pip install -r requirements.txt");
}

TEST_CASE("parse_extension_manifest: missing name -> nullopt") {
    CHECK_FALSE(parse_extension_manifest("entry = \"x\"\n").has_value());
}

TEST_CASE("parse_extension_manifest: missing entry -> nullopt") {
    CHECK_FALSE(parse_extension_manifest("name = \"x\"\n").has_value());
}

TEST_CASE("parse_extension_manifest: comments and blank lines ignored") {
    const auto m = parse_extension_manifest("# comment\n\nname = \"a\"\nentry = \"b\"\n");
    REQUIRE(m.has_value());
    CHECK(m->name == "a");
}

TEST_CASE("parse_cgi_response: status + header + body") {
    const auto r = parse_cgi_response("Status: 404\nContent-Type: text/html\n\n<h1>no</h1>");
    CHECK(r.status == 404);
    REQUIRE(r.headers.size() == 1);
    CHECK(r.headers[0].first == "Content-Type");
    CHECK(r.headers[0].second == "text/html");
    CHECK(r.body == "<h1>no</h1>");
}

TEST_CASE("parse_cgi_response: missing Status defaults to 200") {
    const auto r = parse_cgi_response("Content-Type: text/plain\n\nhello");
    CHECK(r.status == 200);
    CHECK(r.body == "hello");
}

TEST_CASE("parse_cgi_response: no headers at all, just a blank-line-led body") {
    const auto r = parse_cgi_response("\nhello world");
    CHECK(r.status == 200);
    CHECK(r.headers.empty());
    CHECK(r.body == "hello world");
}

TEST_CASE("parse_cgi_response: body containing blank lines stays intact") {
    const auto r = parse_cgi_response("Status: 200\n\nline1\n\nline3");
    CHECK(r.body == "line1\n\nline3");
}

TEST_CASE("parse_cgi_response: no blank-line separator -- everything is headers, empty body") {
    const auto r = parse_cgi_response("Status: 200\nContent-Type: text/html\n");
    CHECK(r.status == 200);
    CHECK(r.body.empty());
}

TEST_CASE("build_cgi_request: round-trippable shape") {
    const auto req = build_cgi_request("POST", "/extensions/calculator/", "a=1", "expr=1%2B1");
    CHECK(req == "METHOD POST\nPATH /extensions/calculator/\nQUERY a=1\n\nexpr=1%2B1");
}

TEST_CASE("build_cgi_request: empty query and body") {
    const auto req = build_cgi_request("GET", "/extensions/calculator/", "", "");
    CHECK(req == "METHOD GET\nPATH /extensions/calculator/\nQUERY \n\n");
}
