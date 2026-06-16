# Upstream katana-parser bug: tokenizer.c's unit-suffix-stripping switch in
# katana_tokenize() never has a `case KATANA_CSS_CHS:` entry, so the numeric
# value of any `<N>ch` CSS length is never parsed (KatanaValue::fValue stays
# uninitialized/garbage). SPEC §7.3 requires the `ch` unit, so patch it in
# alongside the other 2-char-suffix units (em, ex, px, cm, mm, in, pt, pc),
# which share the same fallthrough group and therefore the same length
# decrement count needed to strip a 2-character suffix.
set(_file "${KATANA_SRC}/src/tokenizer.c")
file(READ "${_file}" _contents)

string(FIND "${_contents}" "case KATANA_CSS_CHS:" _already_patched)
if(_already_patched EQUAL -1)
    string(REPLACE
        "case KATANA_CSS_PXS:\n"
        "case KATANA_CSS_PXS:\n        case KATANA_CSS_CHS:\n"
        _patched "${_contents}")
    if(_patched STREQUAL _contents)
        message(FATAL_ERROR "katana-fix-chs-unit.cmake: anchor not found in ${_file}")
    endif()
    file(WRITE "${_file}" "${_patched}")
endif()
