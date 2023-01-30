cmake_minimum_required(VERSION 3.1)

project(heq)

#message(STATUS "start build")
#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG --std=c99") 
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG --std=c99 -g -fPIC") 

set(sources
    DHE.c
    utils.c
    wave.c
)

INCLUDE_DIRECTORIES(
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/include
)

if(DISABLE_HEQ_EXECUTION)
message(STATUS ${DISABLE_HEQ_EXECUTION})
add_library(${PROJECT_NAME} STATIC ${sources})
else()
message(STATUS "Enabale HEQ Execution!!!!")
add_executable(${PROJECT_NAME} ${sources})
endif()

TARGET_LINK_LIBRARIES(${PROJECT_NAME} m)
