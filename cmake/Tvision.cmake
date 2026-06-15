# Fetch and build magiblot/tvision pinned to TVSHOW_TVISION_TAG.

include(FetchContent)

if(NOT DEFINED TVSHOW_TVISION_TAG OR TVSHOW_TVISION_TAG STREQUAL "")
  set(TVSHOW_TVISION_TAG "9a7a64391297e4ff92f3eb2f4d44140bb9570073"
      CACHE STRING "tvision commit to pin" FORCE)
endif()

# Tvision's own build emits warnings we don't want to gate on.
set(_tvshow_saved_werror "${TVSHOW_WARNINGS_AS_ERRORS}")

FetchContent_Declare(
  tvision
  GIT_REPOSITORY https://github.com/magiblot/tvision.git
  GIT_TAG        ${TVSHOW_TVISION_TAG}
  GIT_SHALLOW    FALSE
)

FetchContent_MakeAvailable(tvision)

# Mark tvision headers as system includes so our -Wshadow/-Wall flags don't
# fire on tvision's own code when our files include it.
set_target_properties(tvision PROPERTIES SYSTEM TRUE)

# Re-assert our setting in case the dependency toggled it.
set(TVSHOW_WARNINGS_AS_ERRORS "${_tvshow_saved_werror}")
