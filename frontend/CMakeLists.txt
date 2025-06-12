aux_source_directory(src THE_SRC_FILES)
add_library(frontend SHARED ${THE_SRC_FILES} )
target_link_libraries (frontend PRIVATE mycommon stdc++fs)
target_include_directories(frontend
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

set(THE_PUBLIC_HEADER
  include/Frontend.hh
  include/TcpBuffer.hh
  include/TcpConnection.hh
  include/rbcp.hh
)

set_target_properties(frontend PROPERTIES PUBLIC_HEADER "${THE_PUBLIC_HEADER}")

install(TARGETS frontend
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
)


add_subdirectory(exe)
