project(zmq-streaming C CXX)
cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# ##############################################################################
# DETECT SYSTEM  #
# ##############################################################################
if(${CMAKE_SYSTEM_NAME} MATCHES Darwin)
  set(OSX TRUE)
elseif(${CMAKE_SYSTEM_NAME} MATCHES Linux)
  set(LINUX TRUE)
elseif(${WIN32})
  set(WINDOWS TRUE)
  include(${CMAKE_CURRENT_LIST_DIR}/tools/master_control/windows/CMakeLists.txt)
endif()


LIST(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR})
message(STATUS "stream CMAKE_PREFIX_PATH  ${CMAKE_PREFIX_PATH}")
add_compile_options(-fno-lto)
option(WITH_SPINNAKER "Use spinnaker?" OFF)

set(Boost_ROOT ${AUTOCRANE_DEPS})
find_package(Boost COMPONENTS thread REQUIRED PATHS)
message(STATUS " Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS} ")
message(STATUS " Boost_LIBRARIES ${Boost_LIBRARIES} ")
set(OpenCV_ROOT ${AUTOCRANE_DEPS})
find_package(OpenCV COMPONENTS core highgui imgcodecs imgproc REQUIRED)

# Caution: due to either buggyness or horrendous design of gcc/ld/whatever, the order of
# the libraries is important for linking: if lib A depends on stuff from lib B, lib B
# *must* come after lib A. it does not help if lib B has already been passed previously
set(FFMPEG_ROOT ${AUTOCRANE_DEPS})
find_package(FFMPEG 4.4 EXACT REQUIRED COMPONENTS avdevice avformat swscale
    avcodec avutil swresample)
message(STATUS " FFMPEG_LIBRARIES: ${FFMPEG_LIBRARIES} ")
message(STATUS " FFMPEG_INCLUDE_DIRS: ${FFMPEG_INCLUDE_DIRS} ")

if(WITH_SPINNAKER)
    find_package(Spinnaker 1.24.0.60)
endif()

find_package(cppzmq REQUIRED)

# find_package(OpenMP REQUIRED)
# set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS} -DWITH_OPENMP ")
# message(STATUS " OpenMP: ${OpenMP_CXX_LIBRARIES} ${OpenMP_INCLUDE_DIRS} ")
# set(tclap_ROOT " /usr/local/Cellar/")
# find_package(tclap REQUIRED PATHS " /usr/local/Cellar")
# message(STATUS " tclap headers at ${tclap_INCLUDE_DIR} ")

# tclap
if(OSX)
  execute_process(COMMAND brew --prefix tclap ERROR_QUIET OUTPUT_VARIABLE
      TCLAP_PREFIX RESULT_VARIABLE find-tclap-status)
  if(find-tclap-status EQUAL 0)
    string(REPLACE "\n" "" TCLAP_PREFIX ${TCLAP_PREFIX})
  else()
    message(FATAL_ERROR "Could not find tclap. Status was ${find-tclap-status}")
  endif()
  set(tclap_INCLUDE_DIR "${TCLAP_PREFIX}/include")
endif()


set(cppzmq_LIBRARIES cppzmq)
set(LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR})
set(THIRD_PARTY_INCLUDE_DIRS ${BOOST_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS} ${cppzmq_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS} ${Spinnaker_INCLUDE_DIR} ${tclap_INCLUDE_DIR})
set(THIRD_PARTY_LIBRARIES ${Boost_LIBRARIES} ${OpenCV_LIBRARIES} ${cppzmq_LIBRARIES}
    ${FFMPEG_LIBRARIES} ${Spinnaker_LIBRARIES} ${CMAKE_DL_LIBS})
set(COMMON_SRC ${CMAKE_CURRENT_LIST_DIR}/avutils.cpp)
set(ENCODER_SRC ${CMAKE_CURRENT_LIST_DIR}/encode_video_fromdir.cpp
    ${CMAKE_CURRENT_LIST_DIR}/avtransmitter.cpp ${COMMON_SRC})
set(DECODER_SRC ${CMAKE_CURRENT_LIST_DIR}/decode_video_zmq.cpp
    ${CMAKE_CURRENT_LIST_DIR}/avreceiver.cpp)
set(ZMQ_ENCODER_SRC ${CMAKE_CURRENT_LIST_DIR}/encode_from_zmq.cpp
    ${CMAKE_CURRENT_LIST_DIR}/avtransmitter.cpp ${COMMON_SRC})
add_executable(encode_video_fromdir)
target_sources(encode_video_fromdir PRIVATE ${ENCODER_SRC} ${COMMON_SRC})
target_include_directories(encode_video_fromdir PRIVATE ${LOCAL_INCLUDE_DIRS})
target_include_directories(encode_video_fromdir PRIVATE SYSTEM ${THIRD_PARTY_INCLUDE_DIRS})
target_link_libraries(encode_video_fromdir ${THIRD_PARTY_LIBRARIES})

add_executable(encode_from_zmq)
target_sources(encode_from_zmq PRIVATE ${ZMQ_ENCODER_SRC} ${COMMON_SRC})
target_include_directories(encode_from_zmq PRIVATE ${LOCAL_INCLUDE_DIRS})
target_include_directories(encode_from_zmq PRIVATE SYSTEM ${THIRD_PARTY_INCLUDE_DIRS})
target_link_libraries(encode_from_zmq ${THIRD_PARTY_LIBRARIES})

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