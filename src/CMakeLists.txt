project(rencoder_lib)

set (RENCODER_LIB_SOURCES
    Sound.cpp
    Sampler.cpp
    Encoder.cpp
    Encoder_WAV.cpp
    Encoder_OGG.cpp
    Decoder.cpp
    Decoder_WAV.cpp
    Decoder_OGG.cpp)

set (RENCODER_LIB_HEADERS
    Error.h
    Sound.h
    Sampler.h
    Encoder.h
    Decoder.h
    dr_wav.h)

include_directories(
    ${RENCODER_INCLUDE_DIR}
    ${RPARSER_INCLUDE_DIR}
	${OGG_INCLUDE_DIR}
	${VORBIS_INCLUDE_DIR}
	)

add_library(rencoder_lib ${RENCODER_LIB_SOURCES} ${RENCODER_LIB_HEADERS})
target_link_libraries(rencoder_lib
	rparser
	${OGG_LIBRARY}
	${VORBIS_LIBRARY}
	)