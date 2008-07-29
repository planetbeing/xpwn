IF(WIN32)
	INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/idevice/QuicktimeSDK/CIncludes)
	LINK_DIRECTORIES(${PROJECT_SOURCE_DIR}/idevice/QuicktimeSDK/Libraries)

	ADD_EXECUTABLE(idevice idevice.c windows.c)
	TARGET_LINK_LIBRARIES(idevice xpwn QTMLClient)
	install(TARGETS idevice RUNTIME DESTINATION .)
	install(FILES restore.img3 DESTINATION .)
ENDIF(WIN32)

