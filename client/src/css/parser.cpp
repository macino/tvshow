#include "tvshow/css/parser.hpp"

#include "tvshow/css/types.hpp"

#include <katana.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tvshow::css {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Format a double value without trailing zeros for whole numbers.
std::string fmt_num(double v) {
    const auto i = static_cast<long long>(v);
    if (static_cast<double>(i) == v)
        return std::to_string(i);
    return std::to_string(v);
}

// Wrappers for KatanaValue union access — isolated to keep NOLINT out of logic.
const char* kval_string(const KatanaValue& v) noexcept {
    return v.string;  // NOLINT(cppcoreguidelines-pro-type-union-access)
}
double kval_double(const KatanaValue& v) noexcept {
    return v.isInt
               ? static_cast<double>(v.iValue)  // NOLINT(cppcoreguidelines-pro-type-union-access)
               : v.fValue;                      // NOLINT(cppcoreguidelines-pro-type-union-access)
}

// Serialize one KatanaValue to a CSS value string.
// Do NOT use KatanaValue::raw — it is uninitialized in this katana build.
std::string serialize_value_single(const KatanaValue* val) {
    switch (val->unit) {
    case KATANA_VALUE_IDENT:
    case KATANA_VALUE_STRING:
    case KATANA_VALUE_URI:
    case KATANA_VALUE_PARSER_IDENTIFIER: {
        const char* s = kval_string(*val);
        return (s != nullptr) ? std::string(s) : std::string{};
    }
    case KATANA_VALUE_NUMBER:
        return fmt_num(kval_double(*val));
    case KATANA_VALUE_PX:
        return fmt_num(kval_double(*val)) + "px";
    case KATANA_VALUE_PERCENTAGE:
        return fmt_num(kval_double(*val)) + "%";
    case KATANA_VALUE_EMS:
        return fmt_num(kval_double(*val)) + "em";
    case KATANA_VALUE_REMS:
        return fmt_num(kval_double(*val)) + "rem";
    case KATANA_VALUE_CHS:
        return fmt_num(kval_double(*val)) + "ch";
    default:
        return {};
    }
}

// Build a value string from a katana value array.
std::string serialize_values(const KatanaArray* values) {
    if (values == nullptr)
        return {};
    std::string result;
    for (unsigned i = 0; i < values->length; ++i) {
        const auto* val = static_cast<const KatanaValue*>(values->data[i]);
        if (val == nullptr)
            continue;
        if (i > 0)
            result += ' ';
        result += serialize_value_single(val);
    }
    return result;
}

Declaration convert_declaration(const KatanaDeclaration* kd) {
    Declaration d;
    if (kd->property != nullptr)
        d.property = to_lower(std::string(kd->property));
    d.value = serialize_values(kd->values);
    d.important = kd->important;
    return d;
}

SimpleSel convert_simple(const KatanaSelector* ks) {
    SimpleSel s;
    switch (ks->match) {
    case KatanaSelectorMatchTag:
        if (ks->tag != nullptr && ks->tag->local != nullptr) {
            const std::string_view local(ks->tag->local);
            // Katana uses "*" for universal selector
            if (local == "*") {
                s.kind = SimpleSel::Kind::Universal;
            } else {
                s.kind = SimpleSel::Kind::Tag;
                s.value = to_lower(std::string(local));
            }
        } else {
            s.kind = SimpleSel::Kind::Universal;
        }
        break;
    case KatanaSelectorMatchClass:
        s.kind = SimpleSel::Kind::Class;
        if (ks->data != nullptr && ks->data->value != nullptr)
            s.value = std::string(ks->data->value);
        break;
    case KatanaSelectorMatchId:
        s.kind = SimpleSel::Kind::Id;
        if (ks->data != nullptr && ks->data->value != nullptr)
            s.value = std::string(ks->data->value);
        break;
    case KatanaSelectorMatchPseudoClass:
        s.kind = SimpleSel::Kind::PseudoClass;
        // Map KatanaPseudoType to a name string.
        switch (ks->pseudo) {
        case KatanaPseudoHover:
            s.value = "hover";
            break;
        case KatanaPseudoFocus:
            s.value = "focus";
            break;
        case KatanaPseudoActive:
            s.value = "active";
            break;
        case KatanaPseudoLink:
            s.value = "link";
            break;
        case KatanaPseudoVisited:
            s.value = "visited";
            break;
        default:
            s.value = "unknown";
            break;
        }
        break;
    case KatanaSelectorMatchAttributeSet:
    case KatanaSelectorMatchAttributeExact:
        s.kind = SimpleSel::Kind::AttrExists;
        if (ks->data != nullptr && ks->data->attribute != nullptr &&
            ks->data->attribute->local != nullptr)
            s.value = to_lower(std::string(ks->data->attribute->local));
        break;
    default:
        s.kind = SimpleSel::Kind::Universal;
        break;
    }
    return s;
}

// Convert a katana selector chain (right-to-left) into a ComplexSel (left-to-right).
ComplexSel convert_complex(const KatanaSelector* ks) {
    // chain[0]=subject(rightmost), chain[N-1]=root(leftmost).
    // chain[i]->relation = combinator linking chain[i] (right) to chain[i+1] (left).
    std::vector<const KatanaSelector*> chain;
    while (ks != nullptr) {
        chain.push_back(ks);
        ks = ks->tagHistory;
    }
    const auto n = chain.size();

    // Walk right-to-left index (left-to-right visually) to build compounds.
    // When processing chain[i] (0-based from right), the combinator between
    // chain[i] and chain[i-1] is chain[i]->relation.
    ComplexSel result;
    CompoundSel current;

    for (std::size_t idx = n; idx > 0; --idx) {
        const KatanaSelector* sel = chain[idx - 1];
        current.simples.push_back(convert_simple(sel));

        if (idx == 1)
            break;  // last (rightmost = subject) — done

        // Combinator from the element to the right (chain[idx-2], which we process next)
        // is stored on chain[idx-2]->relation.
        const KatanaSelector* right = chain[idx - 2];
        switch (right->relation) {
        case KatanaSelectorRelationSubSelector:
            break;  // same compound
        case KatanaSelectorRelationDescendant:
            result.compounds.push_back(std::move(current));
            current = CompoundSel{};
            result.combinators.push_back(Combinator::Descendant);
            break;
        case KatanaSelectorRelationChild:
            result.compounds.push_back(std::move(current));
            current = CompoundSel{};
            result.combinators.push_back(Combinator::Child);
            break;
        default:
            result.compounds.push_back(std::move(current));
            current = CompoundSel{};
            result.combinators.push_back(Combinator::Descendant);
            break;
        }
    }
    result.compounds.push_back(std::move(current));

    // Compute specificity: a=id, b=class+pseudo+attr, c=tag
    int spec_a = 0;
    int spec_b = 0;
    int spec_c = 0;
    for (const auto& compound : result.compounds) {
        for (const auto& simple : compound.simples) {
            switch (simple.kind) {
            case SimpleSel::Kind::Id:
                ++spec_a;
                break;
            case SimpleSel::Kind::Class:
            case SimpleSel::Kind::PseudoClass:
            case SimpleSel::Kind::AttrExists:
                ++spec_b;
                break;
            case SimpleSel::Kind::Tag:
                ++spec_c;
                break;
            default:
                break;
            }
        }
    }
    result.specificity = spec_a * 10000 + spec_b * 100 + spec_c;
    return result;
}

void extract_style_rule(const KatanaStyleRule* ksr, std::vector<Rule>& out) {
    Rule rule;

    if (ksr->selectors != nullptr) {
        for (unsigned i = 0; i < ksr->selectors->length; ++i) {
            const auto* ks = static_cast<const KatanaSelector*>(ksr->selectors->data[i]);
            if (ks != nullptr)
                rule.selectors.push_back(convert_complex(ks));
        }
    }

    if (ksr->declarations != nullptr) {
        for (unsigned i = 0; i < ksr->declarations->length; ++i) {
            const auto* kd = static_cast<const KatanaDeclaration*>(ksr->declarations->data[i]);
            if (kd != nullptr)
                rule.declarations.push_back(convert_declaration(kd));
        }
    }

    if (!rule.selectors.empty())
        out.push_back(std::move(rule));
}

std::vector<Declaration> extract_declarations(const KatanaArray* decls) {
    std::vector<Declaration> result;
    if (decls == nullptr)
        return result;
    for (unsigned i = 0; i < decls->length; ++i) {
        const auto* kd = static_cast<const KatanaDeclaration*>(decls->data[i]);
        if (kd != nullptr)
            result.push_back(convert_declaration(kd));
    }
    return result;
}

}  // namespace

std::optional<Stylesheet> parse(std::string_view css) {
    KatanaOutput* output = katana_parse(css.data(), css.size(), KatanaParserModeStylesheet);
    if (output == nullptr)
        return std::nullopt;

    Stylesheet ss;

    if (output->stylesheet != nullptr) {
        const KatanaStylesheet* kss = output->stylesheet;
        for (unsigned i = 0; i < kss->rules.length; ++i) {
            const auto* kr = static_cast<const KatanaRule*>(kss->rules.data[i]);
            if (kr != nullptr && kr->type == KatanaRuleStyle) {
                // KatanaStyleRule embeds KatanaRule as first member — cast is safe.
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                extract_style_rule(reinterpret_cast<const KatanaStyleRule*>(kr), ss.rules);
            }
        }
    }

    katana_destroy_output(output);
    return ss;
}

std::vector<Declaration> parse_inline(std::string_view style) {
    if (style.empty())
        return {};

    KatanaOutput* output =
        katana_parse(style.data(), style.size(), KatanaParserModeDeclarationList);
    if (output == nullptr)
        return {};

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    std::vector<Declaration> result = extract_declarations(output->declarations);
    katana_destroy_output(output);
    return result;
}

}  // namespace tvshow::css
