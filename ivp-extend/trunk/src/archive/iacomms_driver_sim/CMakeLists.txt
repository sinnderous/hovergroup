#--------------------------------------------------------
# The CMakeLists.txt for:                       iacomms_driver_sim
# Author(s):                              
#--------------------------------------------------------

FILE(GLOB SRC *.cpp)

ADD_EXECUTABLE(iacomms_driver_sim ${SRC})

TARGET_LINK_LIBRARIES(iacomms_driver_sim
  goby_common
	goby_acomms
	boost_system
	boost_signals
	protobuf
	geometry
    ${MOOS_LIBRARIES}
    ${MOOS_DEPEND_LIBRARIES}
	mbutil
	acomms_messages
	goby_util)

