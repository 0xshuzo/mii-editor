cmake_minimum_required(VERSION 3.20)

project(mii_editor VERSION 0.1.0 LANGUAGES CXX)

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO is not set. Configure with cmake/wii-toolchain.cmake inside the Wii development container.")
endif()

set(DEVKITPRO "$ENV{DEVKITPRO}")
set(DEVKITPPC "$ENV{DEVKITPPC}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(MII_EDITOR_RENDERER_DIAGNOSTIC "Start the development-only renderer diagnostic scene" OFF)

set(CTMKF_FONT_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/ctmkf_font.o")
set(NUNITO_FONT_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/nunito_font.o")
find_program(PYTHON_EXECUTABLE NAMES python3 python REQUIRED)

set(UI_ARTWORK_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ui_artwork")
set(UI_ARTWORK_METADATA "${UI_ARTWORK_DIRECTORY}/ui_artwork_metadata.hpp")
set(UI_ARTWORK_NAMES hero luca sora mika rex nina tobi cursor_hand cursor_mask)
set(UI_ARTWORK_RAW_FILES)
set(UI_ARTWORK_OBJECTS)
foreach(UI_ARTWORK_NAME IN LISTS UI_ARTWORK_NAMES)
    list(APPEND UI_ARTWORK_RAW_FILES "${UI_ARTWORK_DIRECTORY}/${UI_ARTWORK_NAME}.rgba")
    list(APPEND UI_ARTWORK_OBJECTS "${CMAKE_CURRENT_BINARY_DIR}/${UI_ARTWORK_NAME}_artwork.o")
endforeach()

add_custom_command(
    OUTPUT ${UI_ARTWORK_RAW_FILES} "${UI_ARTWORK_METADATA}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${UI_ARTWORK_DIRECTORY}"
    COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_ui_assets.py"
        --source "${CMAKE_CURRENT_SOURCE_DIR}/mii_editor_handoff/references/target_main.png"
        --cursor-source "${CMAKE_CURRENT_SOURCE_DIR}/mii_editor_handoff/references/cursor_states.png"
        --cursor-shape "${CMAKE_CURRENT_SOURCE_DIR}/mii_editor_handoff/references/cursor_shape.png"
        --out "${UI_ARTWORK_DIRECTORY}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_ui_assets.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/mii_editor_handoff/references/target_main.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/mii_editor_handoff/references/cursor_states.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/mii_editor_handoff/references/cursor_shape.png"
    COMMENT "Preparing static Mii artwork textures"
)

foreach(UI_ARTWORK_NAME IN LISTS UI_ARTWORK_NAMES)
    set(UI_ARTWORK_RAW "${UI_ARTWORK_DIRECTORY}/${UI_ARTWORK_NAME}.rgba")
    set(UI_ARTWORK_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/${UI_ARTWORK_NAME}_artwork.o")
    add_custom_command(
        OUTPUT "${UI_ARTWORK_OBJECT}"
        COMMAND "${DEVKITPPC}/bin/powerpc-eabi-ld" -r -b binary -o "${UI_ARTWORK_OBJECT}" "${UI_ARTWORK_NAME}.rgba"
        DEPENDS "${UI_ARTWORK_RAW}"
        WORKING_DIRECTORY "${UI_ARTWORK_DIRECTORY}"
        COMMENT "Embedding ${UI_ARTWORK_NAME} artwork"
    )
    set_source_files_properties("${UI_ARTWORK_OBJECT}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
endforeach()
add_custom_command(
    OUTPUT "${CTMKF_FONT_OBJECT}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/ctmkf.ttf"
        "${CMAKE_CURRENT_BINARY_DIR}/ctmkf.ttf"
    COMMAND "${DEVKITPPC}/bin/powerpc-eabi-ld" -r -b binary -o "${CTMKF_FONT_OBJECT}" ctmkf.ttf
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/ctmkf.ttf"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Embedding the CTMKF symbol font"
)
add_custom_command(
    OUTPUT "${NUNITO_FONT_OBJECT}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/nunito-semibold.ttf"
        "${CMAKE_CURRENT_BINARY_DIR}/nunito-semibold.ttf"
    COMMAND "${DEVKITPPC}/bin/powerpc-eabi-ld" -r -b binary -o "${NUNITO_FONT_OBJECT}" nunito-semibold.ttf
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/nunito-semibold.ttf"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Embedding the Nunito UI font"
)
set_source_files_properties("${CTMKF_FONT_OBJECT}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
set_source_files_properties("${NUNITO_FONT_OBJECT}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)

add_executable(mii-editor
    src/main.cpp
    src/app/mii_editor_application.cpp
    src/model/placeholder_mii.cpp
    src/platform/wii_platform.cpp
    src/ui/ui_renderer.cpp
    src/ui/ui_assets.cpp
    src/ui/wii_cursor.cpp
    src/ui/mii_editor_ui.cpp
    "${CTMKF_FONT_OBJECT}"
    "${NUNITO_FONT_OBJECT}"
    ${UI_ARTWORK_OBJECTS}
)

target_include_directories(mii-editor PRIVATE
    src
    third_party
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${DEVKITPRO}/libogc/include"
)
set_source_files_properties(src/ui/ui_assets.cpp PROPERTIES
    OBJECT_DEPENDS "${UI_ARTWORK_METADATA}"
)

target_link_directories(mii-editor PRIVATE
    "${DEVKITPRO}/libogc/lib/wii"
)

target_compile_options(mii-editor PRIVATE
    -Wall
    -Wextra
    -Werror
    -ffunction-sections
    -fdata-sections
)
target_compile_definitions(mii-editor PRIVATE
    MII_EDITOR_RENDERER_DIAGNOSTIC=$<BOOL:${MII_EDITOR_RENDERER_DIAGNOSTIC}>
)

add_custom_target(ui-host-tests
    COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_renderer_assets.py"
            --artwork-dir "${UI_ARTWORK_DIRECTORY}"
    DEPENDS ${UI_ARTWORK_RAW_FILES}
    COMMENT "Running host-only rounded-mesh and RGBA8 texture checks"
)

target_link_options(mii-editor PRIVATE -Wl,--gc-sections)
target_link_libraries(mii-editor PRIVATE wiiuse bte ogc m)

set(MII_EDITOR_APP_DIRECTORY "${CMAKE_BINARY_DIR}/apps/mii-editor")
set(MII_EDITOR_DOL "${MII_EDITOR_APP_DIRECTORY}/boot.dol")

add_custom_command(TARGET mii-editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MII_EDITOR_APP_DIRECTORY}"
    COMMAND "${DEVKITPRO}/tools/bin/elf2dol" "$<TARGET_FILE:mii-editor>" "${MII_EDITOR_DOL}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/meta.xml"
        "${MII_EDITOR_APP_DIRECTORY}/meta.xml"
    BYPRODUCTS "${MII_EDITOR_DOL}"
    COMMENT "Creating Homebrew Channel application bundle"
)
