add_library(gte INTERFACE)
target_include_directories(gte INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  )
