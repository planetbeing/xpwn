link_directories(${PROJECT_BINARY_DIR}/common ${PROJECT_BINARY_DIR}/hfs ${PROJECT_BINARY_DIR}/dmg)

add_executable(dripwn dripwn.c)

target_link_libraries (dripwn xpwn)
include_directories(${PROJECT_SOURCE_DIR}/minizip)
link_directories(${PROJECT_BINARY_DIR}/minizip)

install(TARGETS dripwn DESTINATION .)

