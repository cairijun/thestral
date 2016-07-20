cmake_minimum_required(VERSION 2.8.2)
project(thestral C CXX)

set(CMAKE_BUILD_TYPE Debug)
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

file(GLOB SRCS src/*.cc)
add_executable(thestral ${SRCS})
target_link_libraries(thestral ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
