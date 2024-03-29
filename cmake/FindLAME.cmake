# https://github.com/sipwise/sems/blob/master/cmake/FindLame.cmake
FIND_PATH(LAME_INCLUDE_DIR lame/lame.h)
FIND_LIBRARY(LAME_LIBRARIES NAMES mp3lame)

IF(LAME_INCLUDE_DIR AND LAME_LIBRARIES)
	SET(LAME_FOUND TRUE)
ENDIF(LAME_INCLUDE_DIR AND LAME_LIBRARIES)

IF(LAME_FOUND)
	IF (NOT Lame_FIND_QUIETLY)
		MESSAGE(STATUS "Found lame includes:	${LAME_INCLUDE_DIR}/lame/lame.h")
		MESSAGE(STATUS "Found lame library: ${LAME_LIBRARIES}")
	ENDIF (NOT Lame_FIND_QUIETLY)
ELSE(LAME_FOUND)
	IF (Lame_FIND_REQUIRED)
		MESSAGE(FATAL_ERROR "Could NOT find lame development files")
	ENDIF (Lame_FIND_REQUIRED)
ENDIF(LAME_FOUND)