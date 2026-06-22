# Upstream katana-parser bug: Katana's error-recovery path can add the same
# KatanaRule* pointer twice to a rules array (both the stylesheet top-level rules
# and @media child rule lists).  The destroy functions then call
# katana_destroy_rule() twice on the same object → double-free / SIGABRT.
#
# Fixes:
#  1. katana_destroy_stylesheet top-level rules loop: null-out future duplicates
#     before calling katana_destroy_rule so the second occurrence is skipped.
#  2. katana_destroy_rule_list (@media child rules): same dedup logic, inlined
#     (katana_destroy_array only provides a generic iterator, no way to skip).
#  3. katana_destroy_rule: add NULL-guard so dedup-nulled slots are safe.
set(_file "${KATANA_SRC}/src/parser.c")
file(READ "${_file}" _contents)

string(FIND "${_contents}" "katana-dedup-rules: patched" _already_patched)
if(_already_patched EQUAL -1)
    # 1. Patch top-level rules loop in katana_destroy_stylesheet
    string(REPLACE
        "    // free rules\n    for (size_t i = 0; i < e->rules.length; ++i) {\n        katana_destroy_rule(parser, e->rules.data[i]);\n    }"
        "    // free rules — deduplicate first to avoid double-free in error-recovery paths\n    for (size_t i = 0; i < e->rules.length; ++i) { /* katana-dedup-rules: patched */\n        if (e->rules.data[i] == NULL) continue;\n        for (size_t j = i + 1; j < e->rules.length; ++j)\n            if (e->rules.data[j] == e->rules.data[i])\n                e->rules.data[j] = NULL;\n        katana_destroy_rule(parser, e->rules.data[i]);\n    }"
        _patched "${_contents}")
    if(_patched STREQUAL _contents)
        message(FATAL_ERROR "katana-dedup-rules.cmake: rules-loop anchor not found in ${_file}")
    endif()

    # 2. Replace katana_destroy_rule_list with dedup-aware version
    string(REPLACE
        "void katana_destroy_rule_list(KatanaParser* parser, KatanaArray* rules)\n{\n    katana_destroy_array(parser, katana_destroy_rule, rules);\n    katana_parser_deallocate(parser, (void*) rules);\n}"
        "void katana_destroy_rule_list(KatanaParser* parser, KatanaArray* rules)\n{ /* katana-dedup-rule-list: patched */\n    if (rules == NULL) return;\n    if (rules->data != NULL && rules->capacity > 0) {\n        for (size_t i = 0; i < rules->length; ++i) {\n            if (rules->data[i] == NULL) continue;\n            for (size_t j = i + 1; j < rules->length; ++j)\n                if (rules->data[j] == rules->data[i])\n                    rules->data[j] = NULL;\n            katana_destroy_rule(parser, (KatanaRule*)rules->data[i]);\n        }\n    }\n    katana_array_destroy(parser, rules);\n    katana_parser_deallocate(parser, (void*) rules);\n}"
        _patched2 "${_patched}")
    if(_patched2 STREQUAL _patched)
        message(FATAL_ERROR "katana-dedup-rules.cmake: rule-list anchor not found in ${_file}")
    endif()

    # 3. Add NULL-guard to katana_destroy_rule
    string(REPLACE
        "void katana_destroy_rule(KatanaParser* parser, KatanaRule* rule)\n{"
        "void katana_destroy_rule(KatanaParser* parser, KatanaRule* rule)\n{\n    if (rule == NULL) return; /* katana-dedup-rules: null guard */"
        _patched3 "${_patched2}")
    if(_patched3 STREQUAL _patched2)
        message(FATAL_ERROR "katana-dedup-rules.cmake: destroy_rule anchor not found in ${_file}")
    endif()

    file(WRITE "${_file}" "${_patched3}")
endif()
