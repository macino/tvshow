# Upstream katana-parser bug: katana_destroy_array_using_deallocator() does
# not guard against array->data being NULL when array->length > 0.  Certain
# CSS constructs (e.g. vendor-prefixed properties like -webkit-user-select)
# leave a KatanaStyleRule whose selectors KatanaArray has length>0 but a
# NULL data pointer, which causes a SIGSEGV during katana_destroy_output().
# Fix: add a NULL check for array->data before iterating.
set(_file "${KATANA_SRC}/src/parser.c")
file(READ "${_file}" _contents)

string(FIND "${_contents}" "katana-null-data-ptr: patched" _already_patched)
if(_already_patched EQUAL -1)
    string(REPLACE
        "    if ( NULL == array )\n        return;\n    for (size_t i = 0"
        "    if ( NULL == array )\n        return;\n    if ( NULL == array->data || 0 == array->capacity ) /* katana-null-data-ptr: patched */\n        return;\n    for (size_t i = 0"
        _patched "${_contents}")
    if(_patched STREQUAL _contents)
        message(FATAL_ERROR "katana-null-data-ptr.cmake: anchor not found in ${_file}")
    endif()
    file(WRITE "${_file}" "${_patched}")
endif()
