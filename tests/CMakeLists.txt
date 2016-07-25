set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.60.0 REQUIRED COMPONENTS unit_test_framework)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${${CMAKE_PROJECT_NAME}_INCLUDE_DIR}
    ${Boost_INCLUDE_DIR}
)

file(GLOB SRCS *.cc)
add_executable(thestral-tests ${SRCS})
target_link_libraries(thestral-tests thestral-lib ${Boost_LIBRARIES})
add_test(NAME thestral-tests COMMAND thestral-tests -l test_suite -x -r short)

file(GLOB TEST_SSL_FILES *.pem)
file(COPY ${TEST_SSL_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if(BUILD_TESTING AND ENABLE_COVERAGE AND UNIX)
  add_custom_target(
      cov
      COMMAND ${CMAKE_COMMAND} -E echo "Cleaning existing data..."
      COMMAND find ${CMAKE_BINARY_DIR} -type f -name *.gcda -delete
      COMMAND ${CMAKE_COMMAND} -E echo "Generating coverage base line data..."
      COMMAND lcov -i -q -c -d ${CMAKE_SOURCE_DIR} --no-external -o cov_base.info
      COMMAND ${CMAKE_COMMAND} -E echo "Testing..."
      COMMAND thestral-tests -r no
      COMMAND ${CMAKE_COMMAND} -E echo "Generating coverage data..."
      COMMAND lcov -q -c -d ${CMAKE_SOURCE_DIR} --no-external -o cov.info
      COMMAND ${CMAKE_COMMAND} -E echo "Merging coverage data..."
      COMMAND lcov -q -a cov_base.info -a cov.info -o cov.info
      COMMAND ${CMAKE_COMMAND} -E echo "Generating coverage report..."
      COMMAND genhtml -q --legend cov.info -o ${CMAKE_BINARY_DIR}/covhtml
      DEPENDS thestral-tests)
endif()
