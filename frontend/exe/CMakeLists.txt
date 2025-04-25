find_package(ROOT REQUIRED COMPONENTS RIO Tree)

# add_executable(rhcisctrl regctrl_main.cc)
# target_link_libraries(rhcisctrl PRIVATE datactrl-fe mycommon stdc++fs ROOT::RIO ROOT::Tree)


add_executable(rbcptool rbcptool.cc)
target_link_libraries(rbcptool PRIVATE mycommon frontend stdc++fs)

add_executable(tcpcontool tcpcontool.cc)
target_link_libraries(tcpcontool PRIVATE mycommon frontend tcpcon stdc++fs ROOT::RIO ROOT::Tree)

add_executable(datatool datatool.cc)
target_link_libraries(datatool PRIVATE mycommon frontend tcpcon stdc++fs ROOT::RIO ROOT::Tree)


install(TARGETS rbcptool tcpcontool datatool
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
