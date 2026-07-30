#include "tvshow/util/arith_eval.hpp"

#include <cctype>
#include <charconv>

namespace tvshow::util {

namespace {

struct Parser {
    std::string_view s;
    bool ok = true;

    void skip_ws() noexcept {
        while (!s.empty() && s.front() == ' ') { s.remove_prefix(1); }
    }

    [[nodiscard]] std::optional<double> primary() noexcept {
        skip_ws();
        if (s.empty()) { ok = false; return std::nullopt; }

        if (s.front() == '(') {
            s.remove_prefix(1);
            const auto v = expr();
            skip_ws();
            if (!ok || s.empty() || s.front() != ')') { ok = false; return std::nullopt; }
            s.remove_prefix(1);
            return v;
        }

        const char* begin = s.data();
        size_t i = 0;
        while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.')) {
            ++i;
        }
        if (i == 0) { ok = false; return std::nullopt; }
        double value = 0;
        const auto res = std::from_chars(begin, begin + i, value);
        if (res.ec != std::errc{}) { ok = false; return std::nullopt; }
        s.remove_prefix(i);
        return value;
    }

    [[nodiscard]] std::optional<double> factor() noexcept {
        skip_ws();
        if (!s.empty() && s.front() == '-') {
            s.remove_prefix(1);
            const auto v = factor();
            if (!ok) { return std::nullopt; }
            return -*v;
        }
        return primary();
    }

    [[nodiscard]] std::optional<double> term() noexcept {
        auto acc = factor();
        if (!ok) { return std::nullopt; }
        while (true) {
            skip_ws();
            if (s.empty() || (s.front() != '*' && s.front() != '/')) { break; }
            const char op = s.front();
            s.remove_prefix(1);
            const auto rhs = factor();
            if (!ok) { return std::nullopt; }
            if (op == '*') {
                acc = *acc * *rhs;
            } else {
                if (*rhs == 0.0) { ok = false; return std::nullopt; }
                acc = *acc / *rhs;
            }
        }
        return acc;
    }

    [[nodiscard]] std::optional<double> expr() noexcept {
        auto acc = term();
        if (!ok) { return std::nullopt; }
        while (true) {
            skip_ws();
            if (s.empty() || (s.front() != '+' && s.front() != '-')) { break; }
            const char op = s.front();
            s.remove_prefix(1);
            const auto rhs = term();
            if (!ok) { return std::nullopt; }
            acc = (op == '+') ? (*acc + *rhs) : (*acc - *rhs);
        }
        return acc;
    }
};

}  // namespace

std::optional<double> evaluate_arith(std::string_view expr) noexcept {
    Parser p{expr, true};
    const auto v = p.expr();
    p.skip_ws();
    if (!p.ok || !p.s.empty()) { return std::nullopt; }  // trailing garbage -> reject
    return v;
}

}  // namespace tvshow::util
