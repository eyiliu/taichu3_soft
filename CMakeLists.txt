cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(taichu3_soft VERSION 1.0.0 LANGUAGES CXX)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/INSTALL CACHE PATH "Default prefix of install directories" FORCE)
endif()

set(CMAKE_CXX_STANDARD C++17)

# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
# set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++ -static-libgcc")


set(EXTERNAL_BUILD OFF)
add_subdirectory(external)
if(EXTERNAL_BUILD)
  return()
endif()

include(common/mysystem.cmake)
add_subdirectory(common)
add_subdirectory(frontend)
add_subdirectory(teldaq)

add_subdirectory(eudaq)
