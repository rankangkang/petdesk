cmake_minimum_required(VERSION 3.21)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum supported macOS")
endif()
project(PetDesk VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

add_library(petformat STATIC src/petformat.cpp)
target_include_directories(petformat PUBLIC src)
target_link_libraries(petformat PUBLIC Qt6::Core Qt6::Gui)
if(MSVC)
    target_compile_options(petformat PRIVATE /W4 /utf-8)
else()
    target_compile_options(petformat PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(PetDesk MACOSX_BUNDLE WIN32
    src/main.cpp src/petwindow.cpp src/controller.cpp)
target_link_libraries(PetDesk PRIVATE petformat Qt6::Widgets)
if(MSVC)
    target_compile_options(PetDesk PRIVATE /W4 /utf-8)
else()
    target_compile_options(PetDesk PRIVATE -Wall -Wextra -Wpedantic)
endif()
set_target_properties(PetDesk PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER dev.cmkk.petdesk
    MACOSX_BUNDLE_BUNDLE_NAME PetDesk
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/PetDesk.icns")
    target_sources(PetDesk PRIVATE assets/PetDesk.icns)
    set_source_files_properties(assets/PetDesk.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(importer_tests tests/importer_tests.cpp)
    target_link_libraries(importer_tests PRIVATE petformat Qt6::Test)
    add_test(NAME importer COMMAND importer_tests)
endif()
