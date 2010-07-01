INCLUDE(FindZLIB)
INCLUDE(FindBZip2)
INCLUDE(FindPNG)
INCLUDE(FindCURL)
INCLUDE(${PROJECT_SOURCE_DIR}/FindUSB.cmake)

IF(WIN32)
	ADD_LIBRARY(applmobile AppleMobileDevice.cpp)
	ADD_EXECUTABLE(winappleusb WinAppleUSB.cpp)
	TARGET_LINK_LIBRARIES(winappleusb applmobile setupapi)
ELSE(WIN32)
	ADD_LIBRARY(applmobile AppleMobileDevice.cpp)
ENDIF(WIN32)

