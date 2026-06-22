# Upstream katana-parser bugs in katana_destroy_style_rule():
#
# 1. assert(e->selectors->length) crashes when error-recovery produces a rule
#    with an uninitialised/empty selectors array.
# 2. katana_destroy_array() and katana_parser_deallocate() are called on
#    e->selectors / e->declarations without checking whether those arrays
#    were properly initialised.  Corrupted capacity/data pointers (left by the
#    error-recovery path) cause SIGSEGV or double-free / SIGABRT.
#
# Fix: guard both the selectors and declarations cleanup blocks with
# NULL + capacity > 0 + length > 0 checks.  This accepts a minor memory leak
# in error paths in exchange for stable operation on real-world CSS.
set(_file "${KATANA_SRC}/src/parser.c")
file(READ "${_file}" _contents)

string(FIND "${_contents}" "katana-destroy-style-rule-guard: patched" _already_patched)
if(_already_patched EQUAL -1)
    string(REPLACE
        "void katana_destroy_style_rule(KatanaParser* parser, KatanaStyleRule* e)\n{\n    assert(e->selectors->length);\n\n    katana_destroy_array(parser, katana_destroy_selector, e->selectors);\n    katana_parser_deallocate(parser, (void*) e->selectors);\n\n    katana_destroy_array(parser, katana_destroy_declaration, e->declarations);\n    katana_parser_deallocate(parser, (void*) e->declarations);\n    \n    // katana_parser_deallocate(parser, (void*) e->base.name);\n    katana_parser_deallocate(parser, (void*) e);\n}"
        "void katana_destroy_style_rule(KatanaParser* parser, KatanaStyleRule* e)\n{ /* katana-destroy-style-rule-guard: patched */\n    if (e->selectors != NULL && e->selectors->capacity > 0 && e->selectors->length > 0) {\n        katana_destroy_array(parser, katana_destroy_selector, e->selectors);\n    }\n    if (e->selectors != NULL) {\n        katana_parser_deallocate(parser, (void*) e->selectors);\n    }\n    if (e->declarations != NULL && e->declarations->capacity > 0 && e->declarations->length > 0) {\n        katana_destroy_array(parser, katana_destroy_declaration, e->declarations);\n    }\n    if (e->declarations != NULL) {\n        katana_parser_deallocate(parser, (void*) e->declarations);\n    }\n    // katana_parser_deallocate(parser, (void*) e->base.name);\n    katana_parser_deallocate(parser, (void*) e);\n}"
        _patched "${_contents}")
    if(_patched STREQUAL _contents)
        message(FATAL_ERROR "katana-destroy-style-rule-guard.cmake: anchor not found in ${_file}")
    endif()
    file(WRITE "${_file}" "${_patched}")
endif()
