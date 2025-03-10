set(LIB_SRC rbcp.cc)
add_library(rbcp STATIC ${LIB_SRC})
target_include_directories(rbcp
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  )

set(LIB_PUBLIC_HEADERS rbcp.h)
set_target_properties(rbcp PROPERTIES PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}")
