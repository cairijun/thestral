set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.60.0 REQUIRED COMPONENTS unit_test_framework)
include_directories(
    tests
    ${${CMAKE_PROJECT_NAME}_INCLUDE_DIR}
    ${Boost_INCLUDE_DIR}
)

file(GLOB SRCS *.cc)
add_executable(thestral-tests ${SRCS})
target_link_libraries(thestral-tests thestral-lib ${Boost_LIBRARIES})
add_test(NAME thestral-tests COMMAND thestral-tests)
