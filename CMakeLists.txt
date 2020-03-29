#set to minimum version that supports clean build on cygwin
cmake_minimum_required(VERSION 2.8.4)

project(evohomeclient)


if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CXX_EXTENSIONS NO)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wall")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/evohomeclient)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/evohomeclient)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "")


# main include dirs
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)


## Sources

# Targets
file(GLOB EVO_demo_SRCS demo/*.cpp)


# Main library
file(GLOB_RECURSE include_SRCS include/*.cpp)
file(GLOB_RECURSE EVO_client_SRCS evohomeclient/*.cpp)
file(GLOB_RECURSE EVO_http_SRCS src/*.cpp)
add_library(evohomeclient STATIC ${EVO_client_SRCS} ${EVO_http_SRCS} ${include_SRCS})


# CURL library
find_package(CURL)
if(CURL_FOUND)
  message(STATUS "Curl ${CURL_VERSION} found at: ${CURL_LIBRARIES}")
  message(STATUS "Curl includes found at: ${CURL_INCLUDE_DIRS}")
  include_directories(${CURL_INCLUDE_DIRS})
else()
  message(FATAL_ERROR "cURL not found on your system, see install.txt how to get them installed. (for example 'sudo apt-get install curl libcurl4-gnutls-dev')")
endif(CURL_FOUND)



foreach(demo ${EVO_demo_SRCS})
get_filename_component(exefile ${demo} NAME_WE)
add_executable(${exefile} ${demo})
target_link_libraries(${exefile} evohomeclient ${CURL_LIBRARIES})
endforeach(demo)

