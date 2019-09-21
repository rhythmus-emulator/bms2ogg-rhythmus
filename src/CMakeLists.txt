project(rencoder_lib)

set (RENCODER_LIB_SOURCES
    Sound.cpp
    SoundPool.cpp
    Sampler.cpp
    Encoder.cpp
    Encoder_WAV.cpp
    Encoder_OGG.cpp
    Encoder_FLAC.cpp
    #Encoder_LAME.cpp	# not supported
    Decoder.cpp
    Decoder_WAV.cpp
    Decoder_OGG.cpp
    Decoder_FLAC.cpp
    Decoder_LAME.cpp
    Mixer.cpp
    Midi.cpp
    )

set (RENCODER_LIB_HEADERS
    Error.h
    Sound.h
    SoundPool.h
    Sampler.h
    Encoder.h
    Decoder.h
    Mixer.h
    Midi.h
    dr_wav.h
    dr_mp3.h
    )

include_directories(
    ${RENCODER_INCLUDE_DIR}
    ${RPARSER_INCLUDE_DIR}
	${OGG_INCLUDE_DIR}
	${VORBIS_INCLUDE_DIR}
	${FLAC_INCLUDE_DIR}
    ${TIMIDITY_INCLUDE_DIR}
	)

add_library(rencoder_lib ${RENCODER_LIB_SOURCES} ${RENCODER_LIB_HEADERS})
target_link_libraries(rencoder_lib
	rparser
	${OGG_LIBRARY}
	${VORBIS_LIBRARY}
	${FLAC_LIBRARY}
    timidity
	)