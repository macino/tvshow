#include "tvshow/dom/node.hpp"
#include "tvshow/layout/form_data.hpp"

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using tvshow::dom::Node;
using tvshow::dom::NodeKind;
using tvshow::layout::collect_form_fields;
using tvshow::layout::encode_fields;
using tvshow::layout::FormField;
using tvshow::layout::url_encode;

static Node make_elem(std::string_view tag) {
    Node n;
    n.kind = NodeKind::Element;
    n.tag = std::string(tag);
    return n;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static Node make_input(std::string_view type, std::string_view name, std::string_view value = "") {
    Node n = make_elem("input");
    n.attrs.push_back({"type", std::string(type)});
    if (!name.empty()) {
        n.attrs.push_back({"name", std::string(name)});
    }
    if (!value.empty()) {
        n.attrs.push_back({"value", std::string(value)});
    }
    return n;
}

// Build a minimal <form> DOM with one child by pointer.
static Node make_form_with_child(Node child) {
    Node form = make_elem("form");
    form.children.push_back(std::make_unique<Node>(std::move(child)));
    return form;
}

TEST_CASE("url_encode: unreserved chars are not encoded") {
    CHECK(url_encode("abc") == "abc");
    CHECK(url_encode("ABC") == "ABC");
    CHECK(url_encode("123") == "123");
    CHECK(url_encode("-_.~") == "-_.~");
}

TEST_CASE("url_encode: space becomes +") {
    CHECK(url_encode("hello world") == "hello+world");
}

TEST_CASE("url_encode: special chars are percent-encoded") {
    CHECK(url_encode("a=b") == "a%3Db");
    CHECK(url_encode("a&b") == "a%26b");
    CHECK(url_encode("a+b") == "a%2Bb");
}

TEST_CASE("encode_fields: empty → empty string") {
    CHECK(encode_fields({}) == "");
}

TEST_CASE("encode_fields: single field") {
    CHECK(encode_fields({{"name", "Alice"}}) == "name=Alice");
}

TEST_CASE("encode_fields: multiple fields joined by &") {
    const std::vector<FormField> fields = {{"a", "1"}, {"b", "2"}};
    CHECK(encode_fields(fields) == "a=1&b=2");
}

TEST_CASE("collect_form_fields: text input with DOM value") {
    const Node form = make_form_with_child(make_input("text", "q", "hello"));
    const std::unordered_map<const tvshow::dom::Node*, std::string> text_vals;
    const std::unordered_map<const tvshow::dom::Node*, bool> checked_vals;
    const auto fields = collect_form_fields(form, text_vals, checked_vals);
    REQUIRE(fields.size() == 1);
    CHECK(fields[0].name == "q");
    CHECK(fields[0].value == "hello");
}

TEST_CASE("collect_form_fields: text input with live value overrides DOM") {
    Node form = make_form_with_child(make_input("text", "q", "old"));
    const tvshow::dom::Node* input_node = form.children[0].get();
    const std::unordered_map<const tvshow::dom::Node*, std::string> text_vals = {
        {input_node, "new"}};
    const std::unordered_map<const tvshow::dom::Node*, bool> checked_vals;
    const auto fields = collect_form_fields(form, text_vals, checked_vals);
    REQUIRE(fields.size() == 1);
    CHECK(fields[0].value == "new");
}

TEST_CASE("collect_form_fields: hidden input always included") {
    const Node form = make_form_with_child(make_input("hidden", "tok", "abc123"));
    const std::unordered_map<const tvshow::dom::Node*, std::string> text_vals;
    const std::unordered_map<const tvshow::dom::Node*, bool> checked_vals;
    const auto fields = collect_form_fields(form, text_vals, checked_vals);
    REQUIRE(fields.size() == 1);
    CHECK(fields[0].name == "tok");
    CHECK(fields[0].value == "abc123");
}

TEST_CASE("collect_form_fields: unchecked checkbox not included") {
    const Node form = make_form_with_child(make_input("checkbox", "agree"));
    const std::unordered_map<const tvshow::dom::Node*, std::string> text_vals;
    const std::unordered_map<const tvshow::dom::Node*, bool> checked_vals;
    const auto fields = collect_form_fields(form, text_vals, checked_vals);
    CHECK(fields.empty());
}

TEST_CASE("collect_form_fields: checked checkbox included with value 'on'") {
    Node cb = make_input("checkbox", "agree");
    cb.attrs.push_back({"checked", ""});
    const Node form = make_form_with_child(std::move(cb));
    const std::unordered_map<const tvshow::dom::Node*, std::string> text_vals;
    const std::unordered_map<const tvshow::dom::Node*, bool> checked_vals;
    const auto fields = collect_form_fields(form, text_vals, checked_vals);
    REQUIRE(fields.size() == 1);
    CHECK(fields[0].name == "agree");
    CHECK(fields[0].value == "on");
}

TEST_CASE("collect_form_fields: submit button not included") {
    const Node form = make_form_with_child(make_input("submit", "sub", "Send"));
    const std::unordered_map<const tvshow::dom::Node*, std::string> text_vals;
    const std::unordered_map<const tvshow::dom::Node*, bool> checked_vals;
    const auto fields = collect_form_fields(form, text_vals, checked_vals);
    CHECK(fields.empty());
}

TEST_CASE("collect_form_fields: unnamed controls are skipped") {
    const Node form = make_form_with_child(make_input("text", "" /*no name*/, "ignored"));
    const std::unordered_map<const tvshow::dom::Node*, std::string> text_vals;
    const std::unordered_map<const tvshow::dom::Node*, bool> checked_vals;
    const auto fields = collect_form_fields(form, text_vals, checked_vals);
    CHECK(fields.empty());
}
