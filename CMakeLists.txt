project(zmq-streaming C CXX)
cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
set(CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR})

find_package(OpenCV COMPONENTS core highgui imgcodecs imgproc REQUIRED)
# Caution: due to either buggyness or horrendous design of gcc/ld/whatever, the order of
# the libraries is important for linking: if lib A depends on stuff from lib B, lib B
# *must* come after lib A. it does not help if lib B has already been passed previously
find_package(FFMPEG REQUIRED COMPONENTS  avdevice  avformat swscale
    avcodec avutil swresample)
message(STATUS ${FFMPEG_LIBRARIES})
find_package(Spinnaker 1.24.0.60 REQUIRED)
find_package(cppzmq REQUIRED)
set(cppzmq_LIBRARIES cppzmq)
set(LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR})
set(THIRD_PARTY_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS} ${cppzmq_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS} ${Spinnaker_INCLUDE_DIR})
set(THIRD_PARTY_LIBRARIES ${OpenCV_LIBRARIES} ${cppzmq_LIBRARIES}
    ${FFMPEG_LIBRARIES} ${Spinnaker_LIBRARIES} ${CMAKE_DL_LIBS})
set(ENCODER_SRC ${CMAKE_CURRENT_LIST_DIR}/encode_video_zmq.cpp)
set(DECODER_SRC ${CMAKE_CURRENT_LIST_DIR}/decode_video_zmq.cpp)
add_executable(encode_video_zmq)
target_sources(encode_video_zmq PRIVATE ${ENCODER_SRC})
target_include_directories(encode_video_zmq PRIVATE ${LOCAL_INCLUDE_DIRS}
    ${THIRD_PARTY_INCLUDE_DIRS})
target_link_libraries(encode_video_zmq ${THIRD_PARTY_LIBRARIES})

add_executable(decode_video_zmq)
target_sources(decode_video_zmq PRIVATE ${DECODER_SRC})
target_include_directories(decode_video_zmq PRIVATE ${LOCAL_INCLUDE_DIRS}
    ${THIRD_PARTY_INCLUDE_DIRS})
target_link_libraries(decode_video_zmq ${THIRD_PARTY_LIBRARIES})

add_executable(encode_spinnaker_zmq)
target_sources(encode_spinnaker_zmq PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/encode_spinnaker.cpp)
target_include_directories(encode_spinnaker_zmq PRIVATE ${LOCAL_INCLUDE_DIRS}
    ${THIRD_PARTY_INCLUDE_DIRS})
target_link_libraries(encode_spinnaker_zmq ${THIRD_PARTY_LIBRARIES})
