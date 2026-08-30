cmake_minimum_required(VERSION 3.25)

project(
    ghinfo
    VERSION 0.3.0
    DESCRIPTION "Tiny headless self-hosted GitHub status service"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GHINFO_BUILD_TESTS "Build ghinfo tests" ON)
option(GHINFO_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

include(FetchContent)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# Pinned dependencies. Keep these stable and update intentionally.
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.51.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(ghinfo_lib
    src/config.cpp
    src/model.cpp
    src/github_client.cpp
    src/activity.cpp
    src/poller.cpp
    src/server.cpp
    src/snapshot_builder.cpp
)

target_include_directories(ghinfo_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ghinfo_lib
    PUBLIC
        CURL::libcurl
        Threads::Threads
        httplib::httplib
        nlohmann_json::nlohmann_json
)

target_compile_definitions(ghinfo_lib
    PUBLIC
        GHINFO_VERSION="${PROJECT_VERSION}"
)

ghinfo_set_project_warnings(ghinfo_lib)
ghinfo_enable_sanitizers(ghinfo_lib)

add_executable(ghinfo src/main.cpp)
target_link_libraries(ghinfo PRIVATE ghinfo_lib)
ghinfo_set_project_warnings(ghinfo)
ghinfo_enable_sanitizers(ghinfo)

if(GHINFO_BUILD_TESTS)
    enable_testing()

    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(ghinfo_tests
        tests/activity_test.cpp
        tests/config_test.cpp
        tests/snapshot_test.cpp
        tests/api_test.cpp
        tests/github_client_test.cpp
        tests/snapshot_builder_test.cpp
        tests/poller_test.cpp
    )

    target_link_libraries(ghinfo_tests
        PRIVATE
            ghinfo_lib
            GTest::gtest_main
    )

    target_compile_definitions(ghinfo_tests PRIVATE GHINFO_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

    ghinfo_set_project_warnings(ghinfo_tests)
    ghinfo_enable_sanitizers(ghinfo_tests)

    include(GoogleTest)
    gtest_discover_tests(ghinfo_tests)
endif()
