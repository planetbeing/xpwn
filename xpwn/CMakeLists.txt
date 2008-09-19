INCLUDE(${PROJECT_SOURCE_DIR}/FindUSB.cmake)

IF(NOT APPLE OR NOT BUILD_STATIC)
	IF(NOT USB_FOUND)
		message(STATUS "libusb is required for xpwn!")
	ELSE(NOT USB_FOUND)
		include_directories(include)

		include_directories(${USB_INCLUDE_DIR})
		link_directories(${USB_LIBRARIES})

		add_executable(xpwn-bin src/xpwn.cpp src/libibooter.cpp)

		target_link_libraries(xpwn-bin ${USB_LIBRARIES})

		link_directories(${PROJECT_BINARY_DIR}/common ${PROJECT_BINARY_DIR}/hfs ${PROJECT_BINARY_DIR}/ipsw-patch)

		target_link_libraries(xpwn-bin xpwn)

		set_target_properties(xpwn-bin PROPERTIES OUTPUT_NAME "xpwn")

		IF(APPLE)
			SET_TARGET_PROPERTIES(xpwn-bin PROPERTIES LINK_FLAGS "-framework CoreFoundation -framework IOKit")
		ENDIF(APPLE)

		install(FILES ramdisk.dmg DESTINATION .)
		install(TARGETS xpwn-bin DESTINATION .)

	ENDIF(NOT USB_FOUND)
ENDIF(NOT APPLE OR NOT BUILD_STATIC)

