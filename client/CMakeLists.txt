add_executable(tvshow
  src/main.cpp
  src/app/application.cpp
)

target_include_directories(tvshow
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tvshow
  PRIVATE
    tvision
    tvshow_warnings
)

set_target_properties(tvshow PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
