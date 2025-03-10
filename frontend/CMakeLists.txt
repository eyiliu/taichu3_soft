
add_subdirectory(rbcp)

aux_source_directory(src LIB_SRC)
add_library(frontend SHARED ${LIB_SRC} )
target_link_libraries (frontend PRIVATE mycommon rbcp stdc++fs)
target_include_directories(frontend
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_subdirectory(exe)
  
install(TARGETS frontend
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
