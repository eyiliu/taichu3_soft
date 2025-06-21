add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/taichu_layer_conf.json.hh
  COMMAND ${CMAKE_COMMAND} -P ${PROJECT_SOURCE_DIR}/cmake/cpprawstring.cmake ${CMAKE_CURRENT_BINARY_DIR}/taichu_layer_conf.json.hh ${CMAKE_CURRENT_SOURCE_DIR}/taichu_layer_conf.json
  )

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/taichu_tel_conf.json.hh
  COMMAND ${CMAKE_COMMAND} -P ${PROJECT_SOURCE_DIR}/cmake/cpprawstring.cmake ${CMAKE_CURRENT_BINARY_DIR}/taichu_tel_conf.json.hh ${CMAKE_CURRENT_SOURCE_DIR}/taichu_tel_conf.json
  )

add_custom_target(taichu-teldaq-builtinJSON-custom ALL DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/taichu_layer_conf.json.hh
  ${CMAKE_CURRENT_BINARY_DIR}/taichu_tel_conf.json.hh)

add_library(taichu-teldaq-builtinJSON-headers INTERFACE)

add_dependencies(taichu-teldaq-builtinJSON-headers taichu-teldaq-builtinJSON-custom)
target_include_directories(taichu-teldaq-builtinJSON-headers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

install(TARGETS taichu-teldaq-builtinJSON-headers
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
