# -*- cmake -*-

project(plexus)

include(00-Common)
include(LLCommon)

include_directories(
    ${LLCOMMON_INCLUDE_DIRS}
    luajit-2.0/include
    )

set(plexus_SOURCE_FILES
    plexusinit.cpp
    )

set(plexus_HEADER_FILES
    CMakeLists.txt

    plexusinit.h
    )

set_source_files_properties(${plexus_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

if(NOT WORD_SIZE EQUAL 32)
  if(WINDOWS)
    add_definitions(/FIXED:NO)
  else(WINDOWS) # not windows therefore gcc LINUX and DARWIN
    add_definitions(-fPIC)
  endif(WINDOWS)
endif(NOT WORD_SIZE EQUAL 32)

list(APPEND plexus_SOURCE_FILES ${plexus_HEADER_FILES})

add_library (plexus ${plexus_SOURCE_FILES})
