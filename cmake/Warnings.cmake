# INTERFACE library carrying warning + sanitizer flags applied only to
# tvshow's own targets. Third-party code (tvision, gumbo, katana, ...) is
# not subjected to these flags.

add_library(tvshow_warnings INTERFACE)

target_compile_options(tvshow_warnings INTERFACE
  -Wall
  -Wextra
  -Wpedantic
  -Wshadow
  -Wnon-virtual-dtor
  -Wcast-align
  -Wunused
  -Woverloaded-virtual
  -Wformat=2
  -Wdouble-promotion
  -Wnull-dereference
)

if(TVSHOW_WARNINGS_AS_ERRORS)
  target_compile_options(tvshow_warnings INTERFACE -Werror)
endif()

if(TVSHOW_SANITIZE)
  string(REPLACE ";" "," _tvshow_sanitize_flag "${TVSHOW_SANITIZE}")
  target_compile_options(tvshow_warnings INTERFACE
    -fsanitize=${_tvshow_sanitize_flag}
    -fno-omit-frame-pointer
  )
  target_link_options(tvshow_warnings INTERFACE
    -fsanitize=${_tvshow_sanitize_flag}
  )
endif()
