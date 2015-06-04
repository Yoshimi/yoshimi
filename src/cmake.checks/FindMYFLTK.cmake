# The CMake supplied FindFLTK delivers FLTK_LIBRARIES as a full list of
# static and shared libs, with full paths. We really want just a list of
# the lib names. This slight perversion defines ...
#  MYFLTK_FOUND   true or false
#  MYFLTK_CONFIG  fltk-config executable
#  FLTK_FLUID_EXECUTABLE  fluid executable
#  MYFLTK_LIBRARIES  a list of libs required for linking
#

if (MYFLTK_LIBRARIES)
  # in cache already
  set(MYFLTK_FOUND TRUE)
else (MYFLTK_LIBRARIES)
    find_program (MYFLTK_CONFIG fltk-config)
    if (MYFLTK_CONFIG)
        execute_process (COMMAND ${MYFLTK_CONFIG} --ldflags
            OUTPUT_VARIABLE MYFLTK_LIBRARIES)
    string(STRIP ${MYFLTK_LIBRARIES} MYFLTK_LIBRARIES)
    string(REPLACE "-l" "" MYFLTK_LIBRARIES ${MYFLTK_LIBRARIES})
    string(REPLACE " " "; " MYFLTK_LIBRARIES ${MYFLTK_LIBRARIES})
    find_program (FLTK_FLUID_EXECUTABLE fluid)
        if (FLTK_FLUID_EXECUTABLE)
            mark_as_advanced(MYFLTK_CONFIG)
            mark_as_advanced(FLTK_EXECUTABLE)
            mark_as_advanced(MYFLTK_LIBRARIES)
            set(MYFLTK_FOUND TRUE)
            set(FLTK_WRAP_UI 1)
            
        endif(FLTK_FLUID_EXECUTABLE)
    endif (MYFLTK_CONFIG)
endif (MYFLTK_LIBRARIES)

if (MYFLTK_FOUND)
    if (NOT MYFLTK_FIND_QUIETLY)
        message(STATUS "found ${MYFLTK_CONFIG}")
        message(STATUS "found ${FLTK_FLUID_EXECUTABLE}")
    endif (NOT MYFLTK_FIND_QUIETLY)
else (MYFLTK_FOUND)
    if (MYFLTK_FIND_REQUIRED)
      message(FATAL_ERROR "could not find FLTK, aborting.")
    endif (MYFLTK_FIND_REQUIRED)
endif (MYFLTK_FOUND)
