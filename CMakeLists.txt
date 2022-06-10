cmake_minimum_required(VERSION 3.1)

project (iac)
message(STATUS "start build iac")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3") 



include_directories (include)
aux_source_directory (src/common DIR_IAC_COMMON)
aux_source_directory (src/iac_dec DIR_IAC_DEC)
aux_source_directory (src/iac_dec/opus DIR_IAC_DEC_OPUS)
aux_source_directory (src/iac_dec/aac DIR_IAC_DEC_AAC)
aux_source_directory (src/iac_enc DIR_IAC_ENC)

set(CODEC_LIB_DIR  "${PROJECT_SOURCE_DIR}/dep_codecs/lib")
set(EXTER_LIB_DIR  "${PROJECT_SOURCE_DIR}/dep_external/lib")
set(DMPD_LIB_DIR  "${PROJECT_SOURCE_DIR}/src/iac_enc/dmpd")
set(INCLUDE_OPUS_DIR  "${PROJECT_SOURCE_DIR}/dep_codecs/include/opus")
set(INCLUDE_AAC_DIR  "${PROJECT_SOURCE_DIR}/dep_codecs/include/fdk-aac")

include_directories(
    ${INCLUDE_OPUS_DIR}
    ${INCLUDE_AAC_DIR}
    ${PROJECT_SOURCE_DIR}/src/common
    ${PROJECT_SOURCE_DIR}/src/iac_dec
    ${PROJECT_SOURCE_DIR}/src/iac_dec/opus
    ${PROJECT_SOURCE_DIR}/src/iac_dec/aac
    ${PROJECT_SOURCE_DIR}/src/iac_enc
    ${DMPD_LIB_DIR}/asc/include
    ${DMPD_LIB_DIR}/heq/include
)
link_directories (
    ${CODEC_LIB_DIR}
    ${EXTER_LIB_DIR}
    ${DMPD_LIB_DIR}/asc
    ${DMPD_LIB_DIR}/asc/3rdparty/decimate
    ${DMPD_LIB_DIR}/asc/3rdparty/stft
    ${DMPD_LIB_DIR}/heq
)


add_library(${PROJECT_NAME} SHARED ${DIR_IAC_COMMON} ${DIR_IAC_ENC}
    ${DIR_IAC_DEC_OPUS} ${DIR_IAC_DEC_AAC} ${DIR_IAC_DEC})

target_link_libraries (${PROJECT_NAME} opus fdk-aac asc heq flatccrt decimate stft fftw3f )
add_subdirectory(src/iac_enc/dmpd)


set(IAC_PUBLIC_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/include/immersive_audio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/immersive_audio_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/immersive_audio_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/immersive_audio_encoder_defines.h
    )
set_target_properties(iac PROPERTIES PUBLIC_HEADER "${IAC_PUBLIC_HEADER}")


set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(libdir ${CMAKE_INSTALL_PREFIX}/lib)
set(includedir ${CMAKE_INSTALL_PREFIX}/include)
configure_file(iac.pc.in iac.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/iac.pc
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig) 
                                 

install(TARGETS ${PROJECT_NAME}  
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/iac)

