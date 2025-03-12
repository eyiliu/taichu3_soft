aux_source_directory(. LIB_SRC)

add_library(tcpcon STATIC ${LIB_SRC})
target_include_directories(tcpcon
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  )

set(LIB_PUBLIC_HEADERS TcpConnection.hh)
set_target_properties(tcpcon PROPERTIES PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}")
