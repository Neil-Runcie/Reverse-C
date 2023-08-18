CMAKE_MINIMUM_REQUIRED(VERSION 3.3)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

project(Reverse-C
    VERSION 0.1.0
    DESCRIPTION "Reverse-C Programming Language"
    LANGUAGES C)

include(ExternalProject)

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/NDR_LAP)

ExternalProject_Add(ndr_lap
GIT_REPOSITORY https://github.com/Neil-Runcie/NDR_LAP.git
SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/NDR_LAP
)

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

ADD_EXECUTABLE(reverse-c src/reverse.c)

target_link_libraries(reverse-c ${CMAKE_SOURCE_DIR}/src/NDR_LAP/lib/libndr_lap.a)

add_custom_command(TARGET reverse-c  POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_SOURCE_DIR}/src/reverse-c.lex ${CMAKE_SOURCE_DIR}/bin/)
                       
add_custom_command(TARGET reverse-c  POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_SOURCE_DIR}/src/reverse-c.par ${CMAKE_SOURCE_DIR}/bin/)

add_custom_command(TARGET reverse-c  POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_SOURCE_DIR}/reverse-c ${CMAKE_SOURCE_DIR}/bin/)
                       
install (TARGETS reverse-c
         ARCHIVE DESTINATION ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
         LIBRARY DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
         RUNTIME DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
         
