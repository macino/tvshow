#include "tvshow/app/page.hpp"

#include "tvshow/css/parser.hpp"
#include "tvshow/render/render.hpp"
#include "tvshow/css/types.hpp"
#include "tvshow/dom/node.hpp"
#include "tvshow/dom/parser.hpp"
#include "tvshow/layout/engine.hpp"
#include "tvshow/layout/types.hpp"
#include "tvshow/net/cpp_http_client.hpp"
#include "tvshow/net/http_client.hpp"
#include "tvshow/style/resolver.hpp"
#include "tvshow/style/tree.hpp"
#include "tvshow/net/cookie_jar.hpp"
#include "tvshow/util/charset.hpp"
#include "tvshow/util/log.hpp"
#include "tvshow/util/url.hpp"

#include <fstream>
#include <ios>
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
    out << "<!doctype html><html><head><title>" << title << "</title>"
        << "<style>body{padding:16px;}p{margin-top:8px;}</style>"
        << "</head><body><h1>" << title << "</h1><p>" << detail << "</p></body></html>";
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

Fetched fetch_http(const util::Url& url, net::CookieJar* jar) {
    net::CppHttpClient client;
    // Inject cookies from the jar if any match this URL.
    net::Headers req_headers;
    if (jar != nullptr) {
        const std::string cookie_val = jar->cookie_header(url.host(), url.path().empty() ? "/" : url.path());
        if (!cookie_val.empty()) {
            req_headers["Cookie"] = cookie_val;
        }
    }
    const net::Result result = client.get(url, req_headers);
    if (const auto* err = std::get_if<net::NetworkError>(&result)) {
        return {error_page_html("Network Error", err->message), true};
    }
    const auto& resp = std::get<net::Response>(result);
    if (jar != nullptr && !resp.set_cookies.empty()) {
        jar->store(url.host(), resp.set_cookies);
    }
    if (resp.status >= 400) {
        return {error_page_html("HTTP " + std::to_string(resp.status),
                                "The server returned an error for " + url.to_string()),
                true};
    }
    // Charset: Content-Type header takes priority; <meta charset> is the fallback.
    // resp.charset() returns "utf-8" when no explicit charset is declared.
    const std::string ct_charset = resp.charset();
    const std::string meta_charset = util::prescan_charset(resp.body);
    const std::string effective = (ct_charset != "utf-8") ? ct_charset
                                  : !meta_charset.empty() ? meta_charset
                                                          : "utf-8";
    return {util::transcode_to_utf8(resp.body, effective), false};
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
                if (auto fetched = fetch_http(*resolved, nullptr); !fetched.is_error) {
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
                              layout::Viewport vp, net::CookieJar* jar) {
    Fetched fetched;
    const auto parsed = util::Url::parse(action_url);
    if (!parsed) {
        fetched = {error_page_html("Invalid URL",
                                   "Invalid form action: " + std::string(action_url)),
                   true};
    } else {
        net::CppHttpClient client;
        net::Headers req_headers;
        if (jar != nullptr) {
            const std::string cookie_val = jar->cookie_header(parsed->host(), parsed->path().empty() ? "/" : parsed->path());
            if (!cookie_val.empty()) { req_headers["Cookie"] = cookie_val; }
        }
        const net::Result result = client.post(*parsed, body, req_headers);
        if (const auto* err = std::get_if<net::NetworkError>(&result)) {
            fetched = {error_page_html("Network Error", err->message), true};
        } else {
            const auto& resp = std::get<net::Response>(result);
            if (jar != nullptr && !resp.set_cookies.empty()) {
                jar->store(parsed->host(), resp.set_cookies);
            }
            if (resp.status >= 400) {
                fetched = {
                    error_page_html("HTTP " + std::to_string(resp.status),
                                    "The server returned an error for " + std::string(action_url)),
                    true};
            } else {
                fetched = {resp.body, false};
            }
        }
    }

    auto doc = dom::parse(fetched.html);
    if (!doc) {
        util::log::error("failed to parse HTML from POST response");
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
        util::log::error("failed to resolve styles from POST response");
        return std::nullopt;
    }
    auto styled_tree = std::make_unique<style::StyledNode>(std::move(*tree));
    Page page{std::string(action_url), std::move(*doc), std::move(sheets), std::move(styled_tree), {}};
    page.box = layout::layout(*page.tree, vp);
    return page;
}

std::optional<Page> load_page(std::string_view url, layout::Viewport vp, net::CookieJar* jar) {
    constexpr std::string_view k_file_prefix = "file://";

    Fetched fetched;
    if (url.starts_with(k_file_prefix)) {
        const std::string path(url.substr(k_file_prefix.size()));
        auto body = read_file(path);
        if (!body) {
            fetched = {error_page_html("File Not Found", "Cannot open: " + path), true};
        } else {
            fetched = {std::move(*body), false};
        }
    } else if (auto parsed = util::Url::parse(url)) {
        fetched = fetch_http(*parsed, jar);
    } else {
        fetched = {error_page_html("Invalid URL",
                                   "Unsupported or invalid URL: " + std::string(url)),
                   true};
    }

    auto doc = dom::parse(fetched.html);
    if (!doc) {
        util::log::error("failed to parse HTML: " + std::string(url));
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
        util::log::error("failed to resolve styles: " + std::string(url));
        return std::nullopt;
    }

    auto styled_tree = std::make_unique<style::StyledNode>(std::move(*tree));
    Page page{std::string(url), std::move(*doc), std::move(sheets), std::move(styled_tree), {}};
    page.box = layout::layout(*page.tree, vp);

    if (!fetched.is_error && render::is_mostly_blank(render::render(page.box))) {
        const std::vector<css::Stylesheet> no_author;
        if (auto fallback_tree = style::resolve(page.doc, no_author)) {
            page.tree = std::make_unique<style::StyledNode>(std::move(*fallback_tree));
            page.box = layout::layout(*page.tree, vp);
            page.fallback = true;
            page.sheets.clear();
        }
        // If fallback render is still blank, the page likely requires JavaScript.
        if (render::is_mostly_blank(render::render(page.box))) {
            const std::string err_html = error_page_html(
                "JavaScript Required",
                "This page renders content with JavaScript, which tvshow does not support. "
                "Try a text/print version of the page if available.");
            if (auto err_doc = dom::parse(err_html)) {
                if (auto err_tree = style::resolve(*err_doc, {})) {
                    page.doc = std::move(*err_doc);
                    page.tree = std::make_unique<style::StyledNode>(std::move(*err_tree));
                    page.box = layout::layout(*page.tree, vp);
                    page.sheets.clear();
                }
            }
        }
    }

    return page;
}

}  // namespace tvshow::app
