add_executable(tvshow-srv
  src/main.cpp
)

target_include_directories(tvshow-srv
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tvshow-srv
  PRIVATE
    tvshow_warnings
)

set_target_properties(tvshow-srv PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
