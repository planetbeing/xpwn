INCLUDE(${PROJECT_SOURCE_DIR}/FindUSB.cmake)

IF(NOT APPLE OR NOT BUILD_STATIC)
	IF(NOT USB_FOUND)
		message(STATUS "libusb is required for dfu-util!")
		ELSE(NOT USB_FOUND)
		include_directories(${USB_INCLUDE_DIR})
		link_directories(${USB_LIBRARIES})

		add_executable(dfu-util dfu.c sam7dfu.c main.c)

		link_directories(${PROJECT_BINARY_DIR}/common ${PROJECT_BINARY_DIR}/hfs ${PROJECT_BINARY_DIR}/ipsw-patch)

		IF(APPLE)
			SET_TARGET_PROPERTIES(dfu-util PROPERTIES LINK_FLAGS "-framework CoreFoundation -framework IOKit")
		ENDIF(APPLE)

		target_link_libraries(dfu-util xpwn)
		target_link_libraries(dfu-util ${USB_LIBRARIES})

		install(TARGETS dfu-util DESTINATION .)
	ENDIF(NOT USB_FOUND)
ENDIF(NOT APPLE OR NOT BUILD_STATIC)
