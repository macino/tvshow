#include "tvshow/dom/node.hpp"
#include "tvshow/layout/form.hpp"

#include <doctest/doctest.h>

#include <initializer_list>
#include <string_view>
#include <utility>

using tvshow::dom::Node;
using tvshow::dom::NodeKind;
using tvshow::layout::form_control_kind;
using tvshow::layout::form_control_size;
using tvshow::layout::FormControlKind;

static Node
make_element(std::string_view tag,
             std::initializer_list<std::pair<std::string_view, std::string_view>> attrs = {}) {
    Node n;
    n.kind = NodeKind::Element;
    n.tag = std::string(tag);
    for (auto [k, v] : attrs) {
        n.attrs.push_back({std::string(k), std::string(v)});
    }
    return n;
}

TEST_CASE("form: non-form elements return None") {
    CHECK(form_control_kind(make_element("div")) == FormControlKind::None);
    CHECK(form_control_kind(make_element("p")) == FormControlKind::None);
    CHECK(form_control_kind(make_element("a")) == FormControlKind::None);
}

TEST_CASE("form: input types map to correct kinds") {
    CHECK(form_control_kind(make_element("input", {{"type", "text"}})) == FormControlKind::Text);
    CHECK(form_control_kind(make_element("input", {{"type", "password"}})) ==
          FormControlKind::Password);
    CHECK(form_control_kind(make_element("input", {{"type", "checkbox"}})) ==
          FormControlKind::Checkbox);
    CHECK(form_control_kind(make_element("input", {{"type", "radio"}})) == FormControlKind::Radio);
    CHECK(form_control_kind(make_element("input", {{"type", "submit"}})) ==
          FormControlKind::Submit);
    CHECK(form_control_kind(make_element("input", {{"type", "hidden"}})) ==
          FormControlKind::Hidden);
    // input with no type defaults to text
    CHECK(form_control_kind(make_element("input")) == FormControlKind::Text);
}

TEST_CASE("form: button and textarea kinds") {
    CHECK(form_control_kind(make_element("button")) == FormControlKind::Submit);
    CHECK(form_control_kind(make_element("textarea")) == FormControlKind::Textarea);
    CHECK(form_control_kind(make_element("select")) == FormControlKind::Select);
}

TEST_CASE("form: default sizes") {
    // text input: 20 cols wide (with brackets = 22), 1 row
    const auto text_sz = form_control_size(make_element("input", {{"type", "text"}}));
    CHECK(text_sz.cols == 22);
    CHECK(text_sz.rows == 1);

    // checkbox/radio: 3 cols, 1 row
    const auto cb_sz = form_control_size(make_element("input", {{"type", "checkbox"}}));
    CHECK(cb_sz.cols == 3);
    CHECK(cb_sz.rows == 1);

    // textarea: 42 cols (40 + 2 border), 7 rows (5 + 2 border)
    const auto ta_sz = form_control_size(make_element("textarea"));
    CHECK(ta_sz.cols == 42);
    CHECK(ta_sz.rows == 7);
}

TEST_CASE("form: size attr respects cols for text input") {
    const auto sz = form_control_size(make_element("input", {{"type", "text"}, {"size", "10"}}));
    CHECK(sz.cols == 12);  // 10 cols + 2 for brackets
    CHECK(sz.rows == 1);
}
