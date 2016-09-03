find_package(Boost 1.58.0 REQUIRED COMPONENTS unit_test_framework)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${${CMAKE_PROJECT_NAME}_INCLUDE_DIR}
    ${Boost_INCLUDE_DIR}
)
add_definitions(-DBOOST_TEST_DYN_LINK)

file(GLOB SRCS test_*.cc)
add_executable(unit_tests mocks.cc ${SRCS})
target_link_libraries(unit_tests thestral-lib ${Boost_LIBRARIES})
add_test(NAME unit COMMAND unit_tests -l test_suite -x -r short)

file(GLOB TEST_SSL_FILES *.pem)
file(COPY ${TEST_SSL_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if(UNIX)
    file(GLOB SRCS e2e_*.cc)
    add_executable(e2e_tests mocks.cc ${SRCS})
    add_dependencies(e2e_tests thestral)
    target_link_libraries(e2e_tests thestral-lib ${Boost_LIBRARIES})
    add_test(
        NAME e2e
        COMMAND e2e_tests -l test_suite -x -r short -- --thestral_bin $<TARGET_FILE:thestral>)

    file(GLOB TEST_CONFIG_FILES *.conf)
    file(COPY ${TEST_CONFIG_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
else()
    message(WARNING "Not on a UNIX nor UNIX-like system, end-to-end tests will not be built.")
endif()

if(BUILD_TESTING AND ENABLE_COVERAGE AND UNIX)
  add_custom_target(
      cov
      COMMAND ${CMAKE_COMMAND} -E echo "Cleaning existing data..."
      COMMAND find ${CMAKE_BINARY_DIR} -type f "\\(" -name *.gcda -or -name *.gcna "\\)" -delete
      COMMAND ${CMAKE_COMMAND} -E echo "Generating coverage base line data..."
      COMMAND lcov -i -q -c -d ${CMAKE_SOURCE_DIR} --no-external -o cov_base.info
      COMMAND ${CMAKE_COMMAND} -E echo "Testing..."
      COMMAND unit_tests -r no
      COMMAND ${CMAKE_COMMAND} -E echo "Generating coverage data..."
      COMMAND lcov -q -c -d ${CMAKE_SOURCE_DIR} --no-external -o cov.info
      COMMAND ${CMAKE_COMMAND} -E echo "Merging coverage data..."
      COMMAND lcov -q -a cov_base.info -a cov.info -o cov.info
      COMMAND ${CMAKE_COMMAND} -E echo "Generating coverage report..."
      COMMAND genhtml -q --legend cov.info -o ${CMAKE_BINARY_DIR}/covhtml
      DEPENDS unit_tests)
endif()
