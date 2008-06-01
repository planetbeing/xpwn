INCLUDE(FindOpenSSL)
INCLUDE(FindZLIB)
INCLUDE(FindBZip2)
INCLUDE(FindPNG)

IF(NOT ZLIB_FOUND)
	message(FATAL_ERROR "zlib is required for ipsw!")
ENDIF(NOT ZLIB_FOUND)

IF(NOT OPENSSL_FOUND)
	message(FATAL_ERROR "libcrypto is required for ipsw!")
ENDIF(NOT OPENSSL_FOUND)

IF(NOT BZIP2_FOUND)
	message(FATAL_ERROR "libbz2 is required for ipsw!")
ENDIF(NOT BZIP2_FOUND)

IF(NOT PNG_FOUND)
	message(FATAL_ERROR "libpng is required for ipsw!")
ENDIF(NOT PNG_FOUND)

include_directories(${ZLIB_INCLUDE_DIR})
link_directories(${ZLIB_LIBRARIES})
include_directories(${OPENSSL_INCLUDE_DIR})
link_directories(${OPENSSL_LIBRARIES})
include_directories(${BZIP2_INCLUDE_DIR})
link_directories(${BZIP2_LIBRARIES})
include_directories(${PNG_INCLUDE_DIR})
link_directories(${PNG_LIBRARIES})

include_directories(${PROJECT_SOURCE_DIR}/minizip)
link_directories(${PROJECT_SOURCE_DIR}/minizip)

link_directories(${PROJECT_BINARY_DIR}/common ${PROJECT_BINARY_DIR}/hfs ${PROJECT_BINARY_DIR}/dmg)

add_library(xpwn 8900.c bspatch.c ibootim.c img2.c img3.c libxpwn.c lzss.c lzssfile.c nor_files.c outputstate.c plist.c pwnutil.c)

target_link_libraries(xpwn z crypto bz2 png minizip common dmg hfs)

add_executable(pch main.c)
target_link_libraries(pch xpwn)

