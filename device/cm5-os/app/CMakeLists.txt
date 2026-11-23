cmake_minimum_required(VERSION 3.21)
project(cm5-home VERSION 0.4.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(CM5_HOME_BUILD_TESTS "Build host unit tests (needs Qt6::Test)" OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick Network Multimedia)

# Everything except main(), so tests can link the same code the device runs.
add_library(cm5-home-core STATIC
    cloudclient.cpp cloudclient.h
    jellyfinclient.cpp jellyfinclient.h
    gamescores.cpp gamescores.h
    jellyfinimageprovider.cpp jellyfinimageprovider.h
    playbackprofile.cpp playbackprofile.h)
target_include_directories(cm5-home-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cm5-home-core PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network)

qt_add_resources(APP_RESOURCES resources.qrc)
add_executable(cm5-home main.cpp ${APP_RESOURCES})
target_link_libraries(cm5-home PRIVATE cm5-home-core Qt6::Multimedia)
install(TARGETS cm5-home RUNTIME DESTINATION bin)

if(CM5_HOME_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(tst_jellyfinclient tests/tst_jellyfinclient.cpp)
    target_link_libraries(tst_jellyfinclient PRIVATE cm5-home-core Qt6::Test)
    target_compile_definitions(tst_jellyfinclient PRIVATE
        FAKE_JELLYFIN="${CMAKE_CURRENT_SOURCE_DIR}/../tools/fake_jellyfin.py")
    add_test(NAME tst_jellyfinclient COMMAND tst_jellyfinclient)
    set_tests_properties(tst_jellyfinclient PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # Plays generated clips through PlayerView.qml with the host GStreamer (skips without
    # ffmpeg, hlsdemux or gst-libav).
    add_executable(tst_playerview tests/tst_playerview.cpp)
    target_link_libraries(tst_playerview PRIVATE cm5-home-core Qt6::Test)
    target_compile_definitions(tst_playerview PRIVATE
        FAKE_JELLYFIN="${CMAKE_CURRENT_SOURCE_DIR}/../tools/fake_jellyfin.py"
        MAKE_TEST_MEDIA="${CMAKE_CURRENT_SOURCE_DIR}/../tools/make_test_media.sh"
        QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
    add_test(NAME tst_playerview COMMAND tst_playerview)
    set_tests_properties(tst_playerview PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)

    # Runs the real FastAPI backend (services/backend) with uvicorn; needs its Python deps.
    add_executable(tst_cloudclient tests/tst_cloudclient.cpp)
    target_link_libraries(tst_cloudclient PRIVATE cm5-home-core Qt6::Test)
    target_compile_definitions(tst_cloudclient PRIVATE
        BACKEND_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../services/backend")
    add_test(NAME tst_cloudclient COMMAND tst_cloudclient)
    set_tests_properties(tst_cloudclient PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(tst_gamescores tests/tst_gamescores.cpp)
    target_link_libraries(tst_gamescores PRIVATE cm5-home-core Qt6::Test)
    add_test(NAME tst_gamescores COMMAND tst_gamescores)
endif()
