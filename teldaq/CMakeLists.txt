# add_subdirectory(resource)

aux_source_directory(src THE_SRC_FILES)
add_library(teldaq SHARED ${THE_SRC_FILES} )
target_link_libraries (teldaq PRIVATE frontend mycommon stdc++fs )
target_include_directories(teldaq
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

set(THE_PUBLIC_HEADER
  include/Telescope.hh
  include/TelEvent.hpp
)

set_target_properties(teldaq PROPERTIES PUBLIC_HEADER "${THE_PUBLIC_HEADER}")

install(TARGETS teldaq
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  # PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
)

# add_subdirectory(exe)
