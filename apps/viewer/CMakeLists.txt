add_executable(stylized_viewer
    main.cpp
    OrbitCameraController.hpp
    OrbitCameraController.cpp
)

target_link_libraries(stylized_viewer
    PRIVATE
        stylized_engine
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

