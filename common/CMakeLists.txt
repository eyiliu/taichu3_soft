add_subdirectory(linenoiseng)
add_subdirectory(getopt)
add_subdirectory(rapidjson)
add_subdirectory(gte)

set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(mycommon INTERFACE)

target_link_libraries(mycommon INTERFACE
  linenoiseng
  getopt
  rapidjson
  Threads::Threads
  ${CMAKE_DL_LIBS} #dl not sure if it works with full static
  gte
  )

target_include_directories(mycommon
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
  )

# set(LIB_PUBLIC_HEADERS mycommon.hh)
# set_target_properties(mycommon PROPERTIES PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}")
