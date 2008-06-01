cmake_minimum_required(VERSION 2.6)

project (XPWN)

include_directories (${XPWN_SOURCE_DIR}/includes)

add_subdirectory (common)
add_subdirectory (minizip)
add_subdirectory (dmg)
add_subdirectory (hdutil)
add_subdirectory (hfs)
add_subdirectory (ipsw-patch)
add_subdirectory (xpwn)

