cmake_minimum_required(VERSION 2.6)

PROJECT(ovsd C)
ADD_DEFINITIONS(-Os -Wall -Werror --std=gnu99 -Wmissing-declarations)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

IF(APPLE)
  INCLUDE_DIRECTORIES(/opt/local/include)
  LINK_DIRECTORIES(/opt/local/lib)
ENDIF()

SET(SOURCES
	main.c ovs.c ubus.c ovs-shell.c)

SET(LIBS
	ubox ubus)

IF(DEBUG)
  ADD_DEFINITIONS(-DDEBUG -g3)
  IF(NO_OPTIMIZE)
    ADD_DEFINITIONS(-O0)
  ENDIF()
ENDIF()

ADD_EXECUTABLE(ovsd ${SOURCES})

TARGET_LINK_LIBRARIES(ovsd ${LIBS})

INSTALL(TARGETS ovsd
        RUNTIME DESTINATION sbin
)

