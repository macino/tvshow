add_executable(tvshow-tests
    main.cpp
    render/chargrid_test.cpp
    layout/types_test.cpp
    util/url_test.cpp
)

target_link_libraries(tvshow-tests
    PRIVATE
        tvshow_core
        tvshow_warnings
        doctest::doctest
)

set_target_properties(tvshow-tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_test(NAME unit COMMAND tvshow-tests)
