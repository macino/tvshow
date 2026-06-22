# Upstream katana-parser bug: katanaerror() passes katanaget_text(scanner) to
# snprintf() as a %s argument without guarding against a NULL or garbage pointer.
# During error recovery on vendor-prefixed CSS properties (-webkit-*, -moz-*, etc.)
# the scanner's text buffer can be in an invalid state, causing a SIGSEGV in
# strlen() inside snprintf().  Fix: capture the text pointer and substitute ""
# when NULL.
set(_file "${KATANA_SRC}/src/parser.c")
file(READ "${_file}" _contents)

string(FIND "${_contents}" "katana-error-scan-text: patched" _already_patched)
if(_already_patched EQUAL -1)
    string(REPLACE
        "    snprintf(e->message, KATANA_ERROR_MESSAGE_SIZE, \"%s at %s\", error,\n             katanaget_text(parser->scanner));"
        "    { /* katana-error-scan-text: patched */\n      const char *_scan_text = katanaget_text(parser->scanner);\n      snprintf(e->message, KATANA_ERROR_MESSAGE_SIZE, \"%s at %s\", error,\n               (_scan_text != NULL) ? _scan_text : \"\");\n    }"
        _patched "${_contents}")
    if(_patched STREQUAL _contents)
        message(FATAL_ERROR "katana-error-scan-text.cmake: anchor not found in ${_file}")
    endif()
    file(WRITE "${_file}" "${_patched}")
endif()
