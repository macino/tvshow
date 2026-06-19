#include "tvshow/app/page.hpp"

#include "tvshow/css/parser.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/dom/parser.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/net/cpp_http_client.hpp"
#include "tvshow/net/http_client.hpp"
#include "tvshow/style/resolver.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/util/url.hpp"

#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace tvshow::app {

namespace {

// Either a fetched body, or HTML for an internal error page to render in
// its place (SPEC §15.2: network/HTTP errors render a UA-styled page
// rather than failing the navigation outright).
struct Fetched {
    std::string html;
    bool is_error = false;
};

std::string error_page_html(std::string_view title, std::string_view detail) {
    std::ostringstream out;
    out << "<!doctype html><html><head><title>" << title << "</title></head><body><h1>" << title
        << "</h1><p>" << detail << "</p></body></html>";
    return out.str();
}

std::optional<std::string> read_file(std::string_view path) {
    const std::ifstream in(std::string(path), std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << in.rdbuf();
    return contents.str();
}

Fetched fetch_http(const util::Url& url) {
    net::CppHttpClient client;
    const net::Result result = client.get(url);
    if (const auto* err = std::get_if<net::NetworkError>(&result)) {
        return {error_page_html("Network Error", err->message), true};
    }
    const auto& resp = std::get<net::Response>(result);
    if (resp.status >= 400) {
        return {error_page_html("HTTP " + std::to_string(resp.status),
                                "The server returned an error for " + url.to_string()),
                true};
    }
    return {resp.body, false};
}

// Fetches the stylesheets named by doc.stylesheet_hrefs, resolved against
// `url`, in document order (lowest cascade priority, before inline styles).
std::vector<css::Stylesheet> fetch_external_sheets(std::string_view url, const dom::Document& doc) {
    std::vector<css::Stylesheet> sheets;
    const bool is_file = url.starts_with("file://");
    const auto base = util::Url::parse(url);
    for (const auto& href : doc.stylesheet_hrefs) {
        std::optional<std::string> body;
        if (is_file) {
            body = read_file(
                util::resolve_file_url(url, href).substr(std::string_view("file://").size()));
        } else if (base) {
            if (const auto resolved = base->resolve(href)) {
                if (auto fetched = fetch_http(*resolved); !fetched.is_error) {
                    body = std::move(fetched.html);
                }
            }
        }
        if (!body) {
            continue;
        }
        if (auto sheet = css::parse(*body)) {
            sheets.push_back(std::move(*sheet));
        }
    }
    return sheets;
}

}  // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::optional<Page> post_page(std::string_view action_url, std::string_view body,
                              layout::Viewport vp) {
    const auto parsed = util::Url::parse(action_url);
    if (!parsed) {
        std::cerr << "tvshow: invalid action URL: " << action_url << '\n';
        return std::nullopt;
    }
    net::CppHttpClient client;
    const net::Result result = client.post(*parsed, body);
    Fetched fetched;
    if (const auto* err = std::get_if<net::NetworkError>(&result)) {
        fetched = {error_page_html("Network Error", err->message), true};
    } else {
        const auto& resp = std::get<net::Response>(result);
        if (resp.status >= 400) {
            fetched = {
                error_page_html("HTTP " + std::to_string(resp.status),
                                "The server returned an error for " + std::string(action_url)),
                true};
        } else {
            fetched = {resp.body, false};
        }
    }

    auto doc = dom::parse(fetched.html);
    if (!doc) {
        std::cerr << "tvshow: failed to parse HTML from POST response\n";
        return std::nullopt;
    }
    std::vector<css::Stylesheet> sheets;
    if (!fetched.is_error) {
        sheets = fetch_external_sheets(action_url, *doc);
    }
    for (const auto& css_text : doc->inline_styles) {
        if (auto sheet = css::parse(css_text)) {
            sheets.push_back(std::move(*sheet));
        }
    }
    auto tree = style::resolve(*doc, sheets);
    if (!tree) {
        std::cerr << "tvshow: failed to resolve styles from POST response\n";
        return std::nullopt;
    }
    auto styled_tree = std::make_unique<style::StyledNode>(std::move(*tree));
    Page page{std::string(action_url), std::move(*doc), std::move(styled_tree), {}};
    page.box = layout::layout(*page.tree, vp);
    return page;
}

std::optional<Page> load_page(std::string_view url, layout::Viewport vp) {
    constexpr std::string_view k_file_prefix = "file://";

    Fetched fetched;
    if (url.starts_with(k_file_prefix)) {
        const std::string path(url.substr(k_file_prefix.size()));
        auto body = read_file(path);
        if (!body) {
            std::cerr << "tvshow: cannot open file: " << path << '\n';
            return std::nullopt;
        }
        fetched = {std::move(*body), false};
    } else if (auto parsed = util::Url::parse(url)) {
        fetched = fetch_http(*parsed);
    } else {
        std::cerr << "tvshow: unsupported or invalid URL: " << url << '\n';
        return std::nullopt;
    }

    auto doc = dom::parse(fetched.html);
    if (!doc) {
        std::cerr << "tvshow: failed to parse HTML: " << url << '\n';
        return std::nullopt;
    }

    std::vector<css::Stylesheet> sheets;
    if (!fetched.is_error) {
        sheets = fetch_external_sheets(url, *doc);
    }
    for (const auto& css_text : doc->inline_styles) {
        if (auto sheet = css::parse(css_text)) {
            sheets.push_back(std::move(*sheet));
        }
    }

    auto tree = style::resolve(*doc, sheets);
    if (!tree) {
        std::cerr << "tvshow: failed to resolve styles: " << url << '\n';
        return std::nullopt;
    }

    auto styled_tree = std::make_unique<style::StyledNode>(std::move(*tree));
    Page page{std::string(url), std::move(*doc), std::move(styled_tree), {}};
    page.box = layout::layout(*page.tree, vp);
    return page;
}

}  // namespace tvshow::app
