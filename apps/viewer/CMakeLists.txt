add_library(stylized_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ui/ViewerImGuiOpenGL3.cpp
)

target_include_directories(stylized_imgui
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${CMAKE_SOURCE_DIR}/third_party/glad/include
)

target_link_libraries(stylized_imgui
    PUBLIC
        glfw
        stylized_glad
)

set_target_properties(stylized_imgui PROPERTIES
    FOLDER "Dependencies"
)

add_executable(stylized_viewer
    main.cpp
    camera/OrbitCameraController.hpp
    camera/OrbitCameraController.cpp
    scene/SceneRuntimeInstance.hpp
    ui/ViewerPanels.hpp
    ui/ViewerPanels.cpp
    ui/ViewerTheme.hpp
    ui/ViewerTheme.cpp
)

target_link_libraries(stylized_viewer
    PRIVATE
        stylized_engine
        stylized_imgui
        nlohmann_json::nlohmann_json
)

target_compile_definitions(stylized_viewer
    PRIVATE
        STYLIZED_VIEWER_FONT_PATH="${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf"
)

if(MSVC)
    target_compile_options(stylized_viewer PRIVATE
        /W4
        /permissive-
        /utf-8
    )
else()
    target_compile_options(stylized_viewer PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

set_target_properties(stylized_viewer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:stylized_viewer>"
)

