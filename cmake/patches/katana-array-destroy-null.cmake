# Upstream katana-parser bug: katana_array_destroy() frees array->data but does
# not zero the pointer or capacity, leaving dangling references.  When Katana's
# error-recovery path frees an intermediate array and the same KatanaArray struct
# is later visited by katana_destroy_output(), the loop in
# katana_destroy_array_using_deallocator() dereferences the freed pointer (SIGSEGV).
#
# Fix: after freeing, set data=NULL and capacity=0.  Combined with the companion
# patch katana-null-data-ptr.cmake (which guards the loop against NULL data), the
# double-path is closed: data is NULL after destroy, loop is skipped, and a second
# katana_array_destroy() call on the same array is a no-op (capacity==0).
set(_file "${KATANA_SRC}/src/foundation.c")
file(READ "${_file}" _contents)

string(FIND "${_contents}" "katana-array-destroy-null: patched" _already_patched)
if(_already_patched EQUAL -1)
    string(REPLACE
        "        katana_parser_deallocate(parser, array->data);\n    }\n}\n\nstatic void enlarge_array_if_full"
        "        katana_parser_deallocate(parser, array->data);\n        array->data = NULL; /* katana-array-destroy-null: patched */\n        array->capacity = 0;\n    }\n}\n\nstatic void enlarge_array_if_full"
        _patched "${_contents}")
    if(_patched STREQUAL _contents)
        message(FATAL_ERROR "katana-array-destroy-null.cmake: anchor not found in ${_file}")
    endif()
    file(WRITE "${_file}" "${_patched}")
endif()
