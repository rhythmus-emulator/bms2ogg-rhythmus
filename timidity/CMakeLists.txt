project(timidity)

set (TIMIDITY_LIB_SOURCES
    common.c
    instrum.c
    mix.c
    output.c
    playmidi.c
    readmidi.c
    resample.c
    stream.c
    tables.c
    timidity.c
    )

set (TIMIDITY_LIB_HEADERS
    common.h
    instrum.h
    mix.h
    options.h
    ospaths.h
    output.h
    playmidi.h
    readmidi.h
    resample.h
    tables.h
    timi_endian.h
    timidity.h
    timidity_internal.h
    timidity_realtime.h
    )

# why not MT?
if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/MT /Ot")
    set(CMAKE_CXX_FLAGS_DEBUG "/MTd /Zi /Od")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_definitions(-DTIMIDITY_BUILD -DTIMIDITY_STATIC)
add_library(timidity ${TIMIDITY_LIB_SOURCES} ${TIMIDITY_LIB_HEADERS})