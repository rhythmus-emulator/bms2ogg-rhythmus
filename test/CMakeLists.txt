project(rencoder_test)

# COMMENT:
# Should set "debug --> working directory" if you want to debug program using visual studio.

set (RENCODER_TEST_SOURCES
    "main.cpp")

set (RENCODER_TEST_HEADERS
    )

set (GTEST_HEADERS
    "${CMAKE_SOURCE_DIR}/../googletest/googletest/include"
    )


# lib: gtest_main
add_subdirectory("${CMAKE_SOURCE_DIR}/../googletest"
                 "${CMAKE_SOURCE_DIR}/build/googletest"
                 EXCLUDE_FROM_ALL)

include_directories(
    ${RENCODER_INCLUDE_DIR}
    ${RPARSER_INCLUDE_DIR}
    ${GTEST_HEADERS}
	)

# executable file
add_executable(rencoder_test ${RENCODER_TEST_SOURCES} ${RENCODER_TEST_HEADERS})

target_link_libraries(rencoder_test
    rparser
    rencoder_lib
    timidity
    ${OPENSSL_LIBRARY}
    ${ZIP_LIBRARY}
    ${ZLIB_LIBRARY}
    gtest
    )
