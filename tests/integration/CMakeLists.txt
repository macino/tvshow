add_executable(tvshow-itests
    main.cpp
    server_fixture.cpp
    demo_pages_test.cpp
)

target_include_directories(tvshow-itests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tvshow-itests
    PRIVATE
        tvshow_core
        tvshow_io
        tvshow_warnings
        doctest::doctest
)

target_compile_definitions(tvshow-itests
    PRIVATE
        TVSHOW_SRV_PATH="$<TARGET_FILE:tvshow-srv>"
)

add_dependencies(tvshow-itests tvshow-srv)

set_target_properties(tvshow-itests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_test(NAME integration COMMAND tvshow-itests)
