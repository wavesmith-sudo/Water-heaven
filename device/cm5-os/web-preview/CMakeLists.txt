cmake_minimum_required(VERSION 3.21)
project(cm5-home-preview VERSION 0.4.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Browser preview of the device UI. Builds with Qt for WebAssembly (the Cloudflare Pages
# preview) or with desktop Qt (local checks). Uses the device's QML unchanged.
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../app)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(Qt6 QUIET COMPONENTS Multimedia)

qt_add_resources(PREVIEW_RESOURCES ${APP_DIR}/resources.qrc)
qt_add_executable(cm5-home-preview main_web.cpp demojellyfin.cpp demojellyfin.h ${PREVIEW_RESOURCES})
target_link_libraries(cm5-home-preview PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
if(TARGET Qt6::Multimedia)
    target_link_libraries(cm5-home-preview PRIVATE Qt6::Multimedia)
endif()

if(EMSCRIPTEN)
    # Static Qt: link the QML plugins the device QML imports. The scanner only reads QML under
    # this directory, so qml_imports.qml repeats the device QML's imports.
    qt_import_qml_plugins(cm5-home-preview)
endif()
