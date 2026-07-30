#include "tvshow/util/arith_eval.hpp"

#include <doctest/doctest.h>

using tvshow::util::evaluate_arith;

TEST_CASE("evaluate_arith: basic arithmetic") {
    REQUIRE(evaluate_arith("2+3").has_value());
    CHECK(*evaluate_arith("2+3") == doctest::Approx(5));
    CHECK(*evaluate_arith("2-3") == doctest::Approx(-1));
    CHECK(*evaluate_arith("2*3") == doctest::Approx(6));
    CHECK(*evaluate_arith("6/3") == doctest::Approx(2));
}

TEST_CASE("evaluate_arith: operator precedence") {
    CHECK(*evaluate_arith("2+3*4") == doctest::Approx(14));
    CHECK(*evaluate_arith("(2+3)*4") == doctest::Approx(20));
}

TEST_CASE("evaluate_arith: unary minus") {
    CHECK(*evaluate_arith("-5+3") == doctest::Approx(-2));
    CHECK(*evaluate_arith("-(2+3)") == doctest::Approx(-5));
}

TEST_CASE("evaluate_arith: decimals") {
    CHECK(*evaluate_arith("1.5+2.5") == doctest::Approx(4.0));
}

TEST_CASE("evaluate_arith: division by zero is an error") {
    CHECK_FALSE(evaluate_arith("1/0").has_value());
}

TEST_CASE("evaluate_arith: unbalanced parens is an error") {
    CHECK_FALSE(evaluate_arith("(2+3").has_value());
    CHECK_FALSE(evaluate_arith("2+3)").has_value());
}

TEST_CASE("evaluate_arith: trailing garbage is an error") {
    CHECK_FALSE(evaluate_arith("2+3 4").has_value());
}

TEST_CASE("evaluate_arith: empty expression is an error") {
    CHECK_FALSE(evaluate_arith("").has_value());
    CHECK_FALSE(evaluate_arith("   ").has_value());
}

TEST_CASE("evaluate_arith: whitespace is tolerated") {
    CHECK(*evaluate_arith(" 2 + 3 ") == doctest::Approx(5));
}
