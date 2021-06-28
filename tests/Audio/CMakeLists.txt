INCLUDE_DIRECTORIES(./libFaad)


aux_source_directory(./libFaad AUDIO_SRC_LIST)
aux_source_directory(./SDLAudioMixer AUDIO_SRC_LIST)
aux_source_directory(. AUDIO_SRC_LIST)

add_definitions(-DHAVE_CONFIG_H)

add_library(audioPlayer ${AUDIO_SRC_LIST} )

message(STATUS "ADD AudioPlayer")