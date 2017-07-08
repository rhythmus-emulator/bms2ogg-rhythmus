include_directories(
	${RPARSER_INCLUDE_DIR}
    ${RUTIL_INCLUDE_DIR}
)
add_executable(bms2ogg_rhythmus "main.cpp")
target_link_libraries(bms2ogg_rhythmus
    rutil
    rparser
    ${ZLIB_LIBRARY}
    ${ZIP_LIBRARY}
    ${OGG_LIBRARY}
)