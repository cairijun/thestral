cmake_minimum_required(VERSION 3.4)
project(thestral C CXX)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)

set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.60.0 REQUIRED COMPONENTS system)

if(APPLE)
  list(INSERT CMAKE_PREFIX_PATH 0 /usr/local/opt/openssl)
endif()
find_package(OpenSSL 1.0.2 REQUIRED)

set(${CMAKE_PROJECT_NAME}_INCLUDE_DIR include)
include_directories(
    ${Boost_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${${CMAKE_PROJECT_NAME}_INCLUDE_DIR}
)

set(SRCS
    src/base.cc
    src/direct_upstream.cc
    src/socks.cc
    src/socks_server.cc
    src/socks_upstream.cc
    src/ssl.cc
    src/tcp_transport.cc)

add_library(thestral-lib ${SRCS})
target_link_libraries(thestral-lib
    ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})

add_executable(thestral src/main.cc)
target_link_libraries(thestral thestral-lib)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
