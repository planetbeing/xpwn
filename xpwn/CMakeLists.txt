include_directories (include)

add_executable(xpwn-bin src/xpwn.cpp src/libibooter.cpp)

IF(NOT EXISTS ${PROJECT_BINARY_DIR}/libusb)
	file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/libusb)
ENDIF(NOT EXISTS ${PROJECT_BINARY_DIR}/libusb)

IF(WIN32)
	link_directories(libusb-win32)
	include_directories(libusb-win32/src)
	add_dependencies(xpwn-bin ${PROJECT_SOURCE_DIR}/libusb.a)
ELSE(WIN32)
	link_directories(libusb-0.1.12/.libs)
	include_directories(libusb-0.1.12)
	add_dependencies(xpwn-bin ${PROJECT_BINARY_DIR}/libusb/.libs/libusb.a)
	IF(NOT EXISTS ${PROJECT_BINARY_DIR}/libusb/.libs/libusb.a)
		execute_process(COMMAND ${PROJECT_SOURCE_DIR}/libusb-0.1.12/configure WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/libusb)
		execute_process(COMMAND make WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/libusb)
	ENDIF(NOT EXISTS ${PROJECT_BINARY_DIR}/libusb/.libs/libusb.a)
ENDIF(WIN32)

target_link_libraries(xpwn-bin usb)

link_directories(${PROJECT_BINARY_DIR}/common ${PROJECT_BINARY_DIR}/hfs ${PROJECT_BINARY_DIR}/ipsw-patch)

target_link_libraries(xpwn-bin xpwn)

