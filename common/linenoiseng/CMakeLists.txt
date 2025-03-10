set(LIB_SRC  linenoise.cpp wcwidth.cpp ConvertUTF.cpp)
add_library(linenoiseng STATIC ${LIB_SRC})
target_include_directories(linenoiseng
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  )

set(LIB_PUBLIC_HEADERS linenoise.h)
set_target_properties(linenoiseng PROPERTIES PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}")
