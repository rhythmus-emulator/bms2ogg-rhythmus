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
	
add_definitions(-DTIMIDITY_BUILD -DTIMIDITY_STATIC)
add_library(timidity STATIC ${TIMIDITY_LIB_SOURCES})