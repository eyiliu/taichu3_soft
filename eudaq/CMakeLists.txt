option(TAICHU_BUILD_EUDAQ_MODULE "build eudaq module" ON)
if(NOT TAICHU_BUILD_EUDAQ_MODULE)
  message(STATUS "Module eudaq_module_taichu is disabled (TAICHU_BUILD_EUDAQ_MODULE=OFF)")
  return()
endif()
message(STATUS "Module eudaq_module_taichu is enabled (TAICHU_BUILD_EUDAQ_MODULE=ON)")


find_package(eudaq REQUIRED
  COMPONENTS eudaq::core
  CONFIG
  PATHS ${CMAKE_INSTALL_PREFIX}
  # ${CMAKE_INSTALL_PREFIX}/cmake/eudaq
  NO_DEFAULT_PATH
  )

aux_source_directory(src MODULE_SRC)

add_library(eudaq_module_taichu SHARED ${MODULE_SRC})
target_link_libraries(eudaq_module_taichu eudaq::core teldaq mycommon)

install(TARGETS
  eudaq_module_taichu
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
