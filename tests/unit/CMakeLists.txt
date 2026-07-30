add_executable(tvshow-tests
    main.cpp
    render/chargrid_test.cpp
    layout/types_test.cpp
    util/url_test.cpp
    util/charset_test.cpp
    util/config_test.cpp
    util/media_kind_test.cpp
    util/arith_eval_test.cpp
    util/month_grid_test.cpp
    net/extension_manifest_test.cpp
    script/lua_engine_test.cpp
    net/http_client_test.cpp
    net/cookie_jar_test.cpp
    net/blocklist_test.cpp
    net/request_log_test.cpp
    app/bookmarks_test.cpp
    app/extension_ui_protocol_test.cpp
    dom/parser_test.cpp
    dom/node_test.cpp
    css/parser_test.cpp
    style/resolver_test.cpp
    layout/engine_test.cpp
    layout/inline_text_test.cpp
    layout/links_test.cpp
    layout/form_test.cpp
    layout/form_data_test.cpp
    layout/flex_test.cpp
    layout/image_test.cpp
    images/decode_test.cpp
    images/ascii_art_renderer_test.cpp
    render/render_test.cpp
    render/golden_test.cpp
    paint/paint_test.cpp
)

target_compile_definitions(tvshow-tests PRIVATE
    TVSHOW_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden"
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
