add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/firmware_reg.json.hh
  COMMAND ${CMAKE_COMMAND} -P ${PROJECT_SOURCE_DIR}/cmake/cpprawstring.cmake ${CMAKE_CURRENT_BINARY_DIR}/firmware_reg.json.hh ${CMAKE_CURRENT_SOURCE_DIR}/firmware_reg.json
  )

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/taichupix3_reg.json.hh
  COMMAND ${CMAKE_COMMAND} -P ${PROJECT_SOURCE_DIR}/cmake/cpprawstring.cmake ${CMAKE_CURRENT_BINARY_DIR}/taichupix3_reg.json.hh ${CMAKE_CURRENT_SOURCE_DIR}/taichupix3_reg.json
  )

add_custom_target(sensor-firmware-builtinJSON-custom ALL DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/firmware_reg.json.hh
  ${CMAKE_CURRENT_BINARY_DIR}/taichupix3_reg.json.hh)

add_library(sensor-firmware-builtinJSON-headers INTERFACE)

add_dependencies(sensor-firmware-builtinJSON-headers sensor-firmware-builtinJSON-custom)
target_include_directories(sensor-firmware-builtinJSON-headers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

install(TARGETS sensor-firmware-builtinJSON-headers
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
