cmake_minimum_required(VERSION 3.5)
project(serialx)

# Find ament_cmake
find_package(ament_cmake REQUIRED)

if(APPLE)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(FOUNDATION_LIBRARY Foundation)
endif()

## Sources
set(serialx_SRCS
    src/serialx.cc
    include/serialx/serialx.h
    include/serialx/v8stdint.h
)
if(APPLE)
    # If OSX
    list(APPEND serialx_SRCS src/impl/unix.cc)
    list(APPEND serialx_SRCS src/impl/list_ports/list_ports_osx.cc)
elseif(UNIX)
    # If unix
    list(APPEND serialx_SRCS src/impl/unix.cc)
    list(APPEND serialx_SRCS src/impl/list_ports/list_ports_linux.cc)
else()
    # If windows
    list(APPEND serialx_SRCS src/impl/win.cc)
    list(APPEND serialx_SRCS src/impl/list_ports/list_ports_win.cc)
endif()

## Add serial library
add_library(${PROJECT_NAME} ${serialx_SRCS})
if(APPLE)
    target_link_libraries(${PROJECT_NAME} ${FOUNDATION_LIBRARY} ${IOKIT_LIBRARY})
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} rt pthread)
else()
    target_link_libraries(${PROJECT_NAME} setupapi)
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

## Uncomment for example
add_executable(serialx_example examples/serialx_example.cc)
add_dependencies(serialx_example ${PROJECT_NAME})
target_link_libraries(serialx_example ${PROJECT_NAME})

## Include headers
include_directories(include)

## Install executable
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

## Install headers
install(FILES include/serialx/serialx.h include/serialx/v8stdint.h
  DESTINATION include/serialx)

## Tests
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})

ament_package()
