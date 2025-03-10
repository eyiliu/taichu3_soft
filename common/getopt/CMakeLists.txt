set(LIB_SRC getopt.c)
add_library(getopt STATIC ${LIB_SRC})
target_include_directories(getopt
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  )

set(LIB_PUBLIC_HEADERS getopt.h)
set_target_properties(getopt PROPERTIES PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}")
set_target_properties(getopt PROPERTIES LINKER_LANGUAGE CXX)
