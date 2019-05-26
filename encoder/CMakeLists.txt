project(rencoder_bin)

include_directories(
    ${RENCODER_INCLUDE_DIR}
    ${RPARSER_INCLUDE_DIR}
	)

add_executable(rencoder_bin "main.cpp")

target_link_libraries(rencoder_bin rencoder_lib)