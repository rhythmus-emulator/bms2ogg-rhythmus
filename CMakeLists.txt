cmake_minimum_required(VERSION 3.2)
project(bms2ogg-rhythmus)

# add common library
add_subdirectory(rhythmus-lib)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/rhythmus-lib")

# add rparser with Sound support
set(ENV{RUTIL_SOUND} ON)
add_subdirectory(rparser/rutil)
add_subdirectory(rparser/src)

# now deal with our main source code
add_subdirectory(src)