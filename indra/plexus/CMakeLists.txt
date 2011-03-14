# -*- cmake -*-

project(plexus)

include(00-Common)
include(FindOpenGL)
include(LLCommon)
include(LLImage)
include(LLImageJ2COJ)
include(LLMath)
include(LLPrimitive)
include(LLRender)
include(LLUI)
include(LLVFS)
include(LLWindow)
include(LLXML)
include(LLXUIXML)
include(GLOD)

include_directories(
    ${LLCOMMON_INCLUDE_DIRS}
    ${GLOD_INCLUDE_DIR}
    luajit-2.0/include
    ${LIBS_OPEN_DIR}/newview
    ${LLIMAGE_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLPRIMITIVE_INCLUDE_DIRS}
    ${LLRENDER_INCLUDE_DIRS}
    ${LLUI_INCLUDE_DIRS}
    ${LLVFS_INCLUDE_DIRS}
    ${LLWINDOW_INCLUDE_DIRS}
    ${LLXML_INCLUDE_DIRS}
    ${LLXUIXML_INCLUDE_DIRS}
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
