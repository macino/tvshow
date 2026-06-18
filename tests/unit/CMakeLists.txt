add_executable(tvshow-tests
    main.cpp
    render/chargrid_test.cpp
    layout/types_test.cpp
    util/url_test.cpp
    net/http_client_test.cpp
    dom/parser_test.cpp
    css/parser_test.cpp
    style/resolver_test.cpp
    layout/engine_test.cpp
    layout/inline_text_test.cpp
    layout/links_test.cpp
    layout/form_test.cpp
    layout/form_data_test.cpp
    layout/flex_test.cpp
    layout/image_test.cpp
    render/render_test.cpp
    paint/paint_test.cpp
)

target_link_libraries(tvshow-tests
    PRIVATE
        tvshow_core
        tvshow_paint
        tvision
        tvshow_warnings
        doctest::doctest
)

set_target_properties(tvshow-tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_test(NAME unit COMMAND tvshow-tests)
