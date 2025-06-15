

find_package(eudaq 
  COMPONENTS eudaq::core
  CONFIG
  PATHS ${CMAKE_INSTALL_PREFIX}
  # ${CMAKE_INSTALL_PREFIX}/cmake/eudaq
  NO_DEFAULT_PATH
  )

if(NOT eudaq_FOUND)
  message("")
  message("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
  message("!!!Missing eudaq dependency, please build eudaq package by run command (AND then re-run cmake to complete config): ")
  message("!!!>> make external_eudaq")
  message("!!!>> cmake ..")
  message("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
  message("")
  message("")
  add_subdirectory(eudaq)
  set(EXTERNAL_BUILD ON)
  return(PROPAGATE EXTERNAL_BUILD)
endif()
