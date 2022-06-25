project(zmq-streaming C CXX)
cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
set(CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR})
add_compile_options(-fno-lto)

option(WITH_SPINNAKER "Use spinnaker?" OFF)

find_package(Boost COMPONENTS thread REQUIRED)
message(STATUS "Boost_LIBRARIES ${Boost_LIBRARIES}")

find_package(OpenCV COMPONENTS core highgui imgcodecs imgproc REQUIRED)
# Caution: due to either buggyness or horrendous design of gcc/ld/whatever, the order of
# the libraries is important for linking: if lib A depends on stuff from lib B, lib B
# *must* come after lib A. it does not help if lib B has already been passed previously
find_package(FFMPEG REQUIRED COMPONENTS  avdevice  avformat swscale
    avcodec avutil swresample)
message(STATUS "FFMPEG_LIBRARIES: ${FFMPEG_LIBRARIES}")
message(STATUS "FFMPEG_INCLUDE_DIRS: ${FFMPEG_INCLUDE_DIRS}")
if(WITH_SPINNAKER)
  find_package(Spinnaker 1.24.0.60)
endif()
find_package(cppzmq REQUIRED)
set(cppzmq_LIBRARIES cppzmq)
set(LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR})
set(THIRD_PARTY_INCLUDE_DIRS ${BOOST_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS} ${cppzmq_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS} ${Spinnaker_INCLUDE_DIR})
  set(THIRD_PARTY_LIBRARIES ${Boost_LIBRARIES} ${OpenCV_LIBRARIES} ${cppzmq_LIBRARIES}
    ${FFMPEG_LIBRARIES} ${Spinnaker_LIBRARIES} ${CMAKE_DL_LIBS})
set(COMMON_SRC ${CMAKE_CURRENT_LIST_DIR}/avutils.cpp)
set(ENCODER_SRC ${CMAKE_CURRENT_LIST_DIR}/encode_video_fromdir.cpp
    ${CMAKE_CURRENT_LIST_DIR}/avtransmitter.cpp ${COMMON_SRC})
set(DECODER_SRC ${CMAKE_CURRENT_LIST_DIR}/decode_video_zmq.cpp
    ${CMAKE_CURRENT_LIST_DIR}/avreceiver.cpp)
add_executable(encode_video_fromdir)
target_sources(encode_video_fromdir PRIVATE ${ENCODER_SRC} ${COMMON_SRC})
target_include_directories(encode_video_fromdir PRIVATE ${LOCAL_INCLUDE_DIRS}
    )
target_include_directories(encode_video_fromdir PRIVATE SYSTEM ${THIRD_PARTY_INCLUDE_DIRS})
target_link_libraries(encode_video_fromdir ${THIRD_PARTY_LIBRARIES})

add_executable(decode_video_zmq)
target_sources(decode_video_zmq PRIVATE ${DECODER_SRC} ${COMMON_SRC})
target_include_directories(decode_video_zmq PRIVATE ${LOCAL_INCLUDE_DIRS}
    ${THIRD_PARTY_INCLUDE_DIRS})
target_link_libraries(decode_video_zmq ${THIRD_PARTY_LIBRARIES})

if(${SPINNAKER_FOUND})
  add_executable(encode_spinnaker)
  target_sources(encode_spinnaker PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/encode_spinnaker.cpp
      ${CMAKE_CURRENT_LIST_DIR}/avtransmitter.cpp ${COMMON_SRC})
  target_include_directories(encode_spinnaker PRIVATE ${LOCAL_INCLUDE_DIRS}
      ${THIRD_PARTY_INCLUDE_DIRS})
  target_link_libraries(encode_spinnaker ${THIRD_PARTY_LIBRARIES})
endif()
add_executable(decode_rtp)
target_sources(decode_rtp PRIVATE
    decode_rtp.cpp ${COMMON_SRC})
target_link_libraries(decode_rtp ${THIRD_PARTY_LIBRARIES})
target_include_directories(decode_rtp PRIVATE ${LOCAL_INCLUDE_DIRS} ${THIRD_PARTY_INCLUDE_DIRS})
