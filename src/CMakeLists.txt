cmake_minimum_required(VERSION 3.6)

include(GNUInstallDirs)

project(pisp)

add_compile_options(-DBOOST_LOG_DYN_LINK)

file(GLOB SOURCES common/*.cpp frontend/*.cpp variants/*.cpp backend/*.cpp backend/tiling/*.cpp)
add_library(pisp SHARED ${SOURCES})

set(MY_PUBLIC_HEADERS
    frontend/frontend.h
)

find_package(Boost REQUIRED COMPONENTS log log_setup thread)
message(STATUS "${Boost_FOUND}")
message(STATUS "${Boost_LIBRARIES}")

set_target_properties(pisp PROPERTIES PREFIX "lib" IMPORT_PREFIX "lib")
set_target_properties(pisp PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}")
target_link_libraries(pisp pthread ${Boost_LIBRARIES})

install(TARGETS pisp
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        # PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libpisp
)

install(
    DIRECTORY "./"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libpisp
    FILES_MATCHING PATTERN "*.h"
)