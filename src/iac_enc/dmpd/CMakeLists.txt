cmake_minimum_required(VERSION 3.1)

project(dmpd)

message(STATUS "start dmpd build...")
message(STATUS ${PROJECT_SOURCE_DIR})
message(STATUS ${CMAKE_CURRENT_BINARY_DIR})
message(STATUS ${CMAKE_INSTALL_PREFIX})
message(STATUS "-------------------")

#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG --std=c99") 
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG --std=c99 -g") 

ADD_DEFINITIONS(-DDISABLE_EXECUTION)
ADD_DEFINITIONS(-DUSE_QUEUE_METHOD)

SET(DISABLE_ASC_EXECUTION "Disabale ASC Execution!!!!")
SET(DISABLE_HEQ_EXECUTION "Disabale HEQ Execution!!!!") 
#SET_PROPERTY( GLOBAL PROPERTY DISABLE_ASC_EXECUTION "Disabale ASC Execution")
#SET_PROPERTY( GLOBAL PROPERTY DISABLE_HEQ_EXECUTION "Disabale HEQ Execution")

add_subdirectory(asc)
add_subdirectory(heq)



if(INSTALL_DMPD)
message(STATUS "install dmpd...")

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(libdir ${CMAKE_INSTALL_PREFIX}/lib)
set(includedir ${CMAKE_INSTALL_PREFIX}/include)
configure_file(dmpd.pc.in dmpd.pc)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/dmpd.pc
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)

#INSTALL(FILES ${PROJECT_SOURCE_DIR}/asc/include/ia_asc.h DESTINATION ${PROJECT_SOURCE_DIR}/include)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/asc/include/ia_asc.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/dmpd)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/heq/include/ia_heq.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/dmpd)

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/asc/libasc.so DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/heq/libheq.so DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

endif(INSTALL_DMPD)
