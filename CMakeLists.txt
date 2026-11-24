cmake_minimum_required(VERSION 3.21)
project(TodolistPanel VERSION 0.1.0 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

qt_add_executable(TodolistPanel WIN32
    src/main.cpp
    src/GlassSurface.cpp
    src/GlassSurface.h
    src/GlassMenu.cpp
    src/GlassMenu.h
    src/Panel.cpp
    src/Panel.h
    src/TaskCategories.cpp
    src/ProjectColorPicker.cpp
    src/ProjectColorPicker.h
    src/TaskLabels.h
    src/WindowBackdrop.cpp
    src/WindowBackdrop.h
)
target_compile_features(TodolistPanel PRIVATE cxx_std_17)
target_link_libraries(TodolistPanel PRIVATE Qt6::Widgets)
qt_add_resources(TodolistPanel "app_icons" PREFIX "/icons" BASE assets FILES assets/todo.ico)

if(WIN32)
    target_sources(TodolistPanel PRIVATE assets/todo.rc)
    target_link_libraries(TodolistPanel PRIVATE dwmapi wtsapi32)
endif()

install(TARGETS TodolistPanel RUNTIME DESTINATION bin)
qt_generate_deploy_app_script(
    TARGET TodolistPanel
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    NO_TRANSLATIONS
)
install(SCRIPT "${deploy_script}")
