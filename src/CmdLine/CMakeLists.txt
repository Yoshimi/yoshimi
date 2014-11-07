set (CmdLineSources
     CmdLine.cpp)

add_executable (yoshimi-cmdline ${CmdLineSources})
install (TARGETS yoshimi-cmdline RUNTIME DESTINATION bin)
