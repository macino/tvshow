#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tvshow::css {

enum class Combinator : uint8_t { Descendant, Child };

// One simple-selector unit: tag, .class, #id, :pseudo, or [attr] existence.
struct SimpleSel {
    enum class Kind : uint8_t { Tag, Class, Id, Universal, PseudoClass, AttrExists };
    Kind kind = Kind::Tag;
    std::string value;  // tag name, class, id, pseudo name, or attribute name
};

// A compound selector — all simples must match the same element.
struct CompoundSel {
    std::vector<SimpleSel> simples;
};

// A complex selector: compounds joined by combinators (left-to-right, root to subject).
struct ComplexSel {
    std::vector<CompoundSel> compounds;
    std::vector<Combinator> combinators;  // size == compounds.size() - 1
    // CSS specificity packed as id*10000 + class_like*100 + tag. Higher = more specific.
    int specificity = 0;
};

struct Declaration {
    std::string property;  // lowercase property name
    std::string value;     // raw value string (as written in the stylesheet)
    bool important = false;
};

struct Rule {
    std::vector<ComplexSel> selectors;
    std::vector<Declaration> declarations;
};

struct Stylesheet {
    std::vector<Rule> rules;
};

}  // namespace tvshow::css
