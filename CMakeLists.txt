project(zmq-streaming C CXX)
message(STATUS "--------------------- stream ---------------------------------")
cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR})
# Use linked libraries in rpath, otherwise binaries in /bin try to call wrong libs in /usr/local etc.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
# ##############################################################################
# DETECT SYSTEM  #
# ##############################################################################
if(${CMAKE_SYSTEM_NAME} MATCHES Darwin)
  set(OSX TRUE)
elseif(${CMAKE_SYSTEM_NAME} MATCHES Linux)
  set(LINUX TRUE)
endif()

message(STATUS "stream CMAKE_PREFIX_PATH  ${CMAKE_PREFIX_PATH}")
option(WITH_SPINNAKER "Use spinnaker?" OFF)

find_package(Boost COMPONENTS thread REQUIRED)
message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARIES ${Boost_LIBRARIES}")
find_package(OpenCV COMPONENTS core highgui imgcodecs imgproc REQUIRED)
find_package(cppzmq REQUIRED)
find_package(tclap REQUIRED)

# Caution: due to either buggyness or horrendous design of gcc/ld/whatever, the order of
# the libraries is important for linking: if lib A depends on stuff from lib B, lib B
# *must* come after lib A. it does not help if lib B has already been passed previously
find_package(FFMPEG 4.4 EXACT REQUIRED COMPONENTS avdevice avformat swscale
    avcodec avutil swresample)
message(STATUS "FFMPEG_LIBRARIES: ${FFMPEG_LIBRARIES}")
message(STATUS "FFMPEG_INCLUDE_DIRS: ${FFMPEG_INCLUDE_DIRS}")

if(WITH_SPINNAKER)
    find_package(Spinnaker 1.24.0.60)
endif()

set(cppzmq_LIBRARIES cppzmq)
set(LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR})
set(THIRD_PARTY_INCLUDE_DIRS ${BOOST_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS} ${cppzmq_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS} ${Spinnaker_INCLUDE_DIR} ${tclap_INCLUDE_DIR})
  message(STATUS "stream THIRD_PARTY_INCLUDE_DIRS: ${THIRD_PARTY_INCLUDE_DIRS}")
set(THIRD_PARTY_LIBRARIES ${Boost_LIBRARIES} ${OpenCV_LIBRARIES} ${cppzmq_LIBRARIES}
    ${FFMPEG_LIBRARIES} ${Spinnaker_LIBRARIES} ${CMAKE_DL_LIBS})
set(COMMON_SRC ${CMAKE_CURRENT_LIST_DIR}/avutils.cpp)
set(ENCODER_SRC ${CMAKE_CURRENT_LIST_DIR}/avtransmitter.cpp)
set(DECODER_SRC ${CMAKE_CURRENT_LIST_DIR}/avreceiver.cpp ${CMAKE_CURRENT_LIST_DIR}/rtp_receiver.cpp)
set(ZMQ_ENCODER_SRC ${CMAKE_CURRENT_LIST_DIR}/encode_from_zmq.cpp
    ${CMAKE_CURRENT_LIST_DIR}/avtransmitter.cpp ${COMMON_SRC})
add_executable(encode_video_fromdir)
target_sources(encode_video_fromdir PRIVATE ${CMAKE_CURRENT_LIST_DIR}/encode_video_fromdir.cpp ${ENCODER_SRC} ${COMMON_SRC})
target_include_directories(encode_video_fromdir PRIVATE ${LOCAL_INCLUDE_DIRS})
target_include_directories(encode_video_fromdir SYSTEM PRIVATE ${THIRD_PARTY_INCLUDE_DIRS})

option(BUILD_zmq_encoder OFF)
if (BUILD_zmq_encoder)
    find_package(Poco REQUIRED COMPONENTS Crypto JWT)
    find_package(LibDataChannel REQUIRED)
    find_package(nlohmann_json REQUIRED)
    target_link_libraries(encode_video_fromdir ${THIRD_PARTY_LIBRARIES}
        LibDataChannel::LibDataChannel nlohmann_json::nlohmann_json)

    add_executable(encode_from_zmq)
    target_sources(encode_from_zmq PRIVATE ${ZMQ_ENCODER_SRC} ${COMMON_SRC})
    target_include_directories(encode_from_zmq PRIVATE ${LOCAL_INCLUDE_DIRS})
    target_include_directories(encode_from_zmq SYSTEM PRIVATE ${THIRD_PARTY_INCLUDE_DIRS})
    target_link_libraries(encode_from_zmq ${THIRD_PARTY_LIBRARIES} nlohmann_json::nlohmann_json
        LibDataChannel::LibDataChannel Poco::Crypto Poco::JWT)

    add_executable(decode_video_zmq)
    target_sources(decode_video_zmq PRIVATE ${CMAKE_CURRENT_LIST_DIR}/decode_video_zmq.cpp ${DECODER_SRC} ${COMMON_SRC})
    target_include_directories(decode_video_zmq PRIVATE ${LOCAL_INCLUDE_DIRS}
        ${THIRD_PARTY_INCLUDE_DIRS})
    target_link_libraries(decode_video_zmq ${THIRD_PARTY_LIBRARIES})
endif()

if(${SPINNAKER_FOUND})
    message(STATUS "Building spinnaker encoding")
    add_executable(encode_spinnaker)
    target_sources(encode_spinnaker PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/encode_spinnaker.cpp
        ${CMAKE_CURRENT_LIST_DIR}/avtransmitter.cpp ${COMMON_SRC})
    target_include_directories(encode_spinnaker PRIVATE ${LOCAL_INCLUDE_DIRS}
        ${THIRD_PARTY_INCLUDE_DIRS})
    target_link_libraries(encode_spinnaker ${THIRD_PARTY_LIBRARIES})
else()
    message(STATUS "Not building spinnaker encoding")
endif()

add_executable(decode_rtp)
target_sources(decode_rtp PRIVATE
    decode_rtp.cpp ${COMMON_SRC} ${DECODER_SRC})
target_link_libraries(decode_rtp ${THIRD_PARTY_LIBRARIES})
target_include_directories(decode_rtp PRIVATE ${LOCAL_INCLUDE_DIRS} ${THIRD_PARTY_INCLUDE_DIRS})

# # ##############################################################################
# # Files
# # ##############################################################################

LIST(APPEND STREAM_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/avtransmitter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/avreceiver.cpp
  ${CMAKE_CURRENT_LIST_DIR}/avutils.cpp
  ${CMAKE_CURRENT_LIST_DIR}/decode_rtp.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rtp_receiver.cpp
  ${CMAKE_CURRENT_LIST_DIR}/decode_video_zmq.cpp
  ${CMAKE_CURRENT_LIST_DIR}/encode_spinnaker.cpp
  ${CMAKE_CURRENT_LIST_DIR}/encode_from_zmq.cpp
  ${CMAKE_CURRENT_LIST_DIR}/encode_video_fromdir.cpp
)

LIST(APPEND STREAM_HEADERS
  ${CMAKE_CURRENT_LIST_DIR}/avtransmitter.hpp
  ${CMAKE_CURRENT_LIST_DIR}/avreceiver.hpp
  ${CMAKE_CURRENT_LIST_DIR}/avutils.hpp
  ${CMAKE_CURRENT_LIST_DIR}/rtp_receiver.hpp
)

LIST(APPEND FILES
  ${STREAM_SRCS}
  ${STREAM_HEADERS})


# define subgroups for XCode and other IDEs
source_group(src FILES ${STREAM_HEADERS} ${STREAM_SRCS})

# ##############################################################################
# libavstream library
# ##############################################################################
set(libavstream_LIBRARY av-streaming)
set(${libavstream_LIBRARY}_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(${libavstream_LIBRARY}_INCLUDE_DIR ${${libavstream_LIBRARY}_DIR})
set(${libavstream_LIBRARY}_SOURCES ${${libavstream_LIBRARY}_DIR})
message(STATUS "${libavstream_LIBRARY}_INCLUDE_DIR ${${libavstream_LIBRARY}_DIR}")
# annoying thing about OBJECT libraries: They are propagated to direct
# dependents only, so linking to A from B and from C to B does not link
# A into C. Apparently the intended way to do this is a TARGET_OBJECTS
# generator expression
# (https://gitlab.kitware.com/cmake/cmake/-/issues/18010#note_422196).
# whatever man, i don't even understand. So we make this a simple
# static lib, it's easier.
add_library(${libavstream_LIBRARY} STATIC)
target_include_directories(${libavstream_LIBRARY} PRIVATE ${LOCAL_INCLUDE_DIRS})
target_include_directories(${libavstream_LIBRARY} SYSTEM PRIVATE ${THIRD_PARTY_INCLUDE_DIRS})
target_link_libraries(${libavstream_LIBRARY} PUBLIC ${THIRD_PARTY_LIBRARIES})
target_sources(${libavstream_LIBRARY} PRIVATE ${DECODER_SRC} ${ENCODER_SRC} ${COMMON_SRC})

# ##############################################################################
# INSTALL
# ##############################################################################

# Install headers into include/${libavstream_LIBRARY}/*, keeping the src directory structure.
# This matches what other libraries (opencv, tensorflow..) do, although they
# already maintain the explicit include/LIBRARY_NAME structure in their source
# tree. We prefer to have the headers next to the sources, so we need some
# mockery to still allow namespaced includes.
set(CMAKE_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/include)
set(CMAKE_BUILD_INCLUDEDIR ${CMAKE_BINARY_DIR}/include)
foreach(file ${STREAM_HEADERS})
  get_filename_component(dir ${file} DIRECTORY BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
  string(REGEX REPLACE "^${CMAKE_CURRENT_LIST_DIR}" "" dir "${dir}")
  string(REGEX REPLACE "^/" "" dir "${dir}")

  # During install, all headers are moved to a include/${libavstream_LIBRARY} subdirectory,
  # namespacing them away from other libraries to avoid conflicts.
  install(FILES ${file} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${libavstream_LIBRARY}/${dir})

  # During build time we create the include/${libavstream_LIBRARY}/**/*.h structure within
  # the ./build directory, such that it resembles what we have after an install
  # without the actual need to move the headers out of the ./src directory.
  message(STATUS "Copy ${file} to ${CMAKE_BUILD_INCLUDEDIR}/${libavstream_LIBRARY}/${dir}")
  file(MAKE_DIRECTORY ${CMAKE_BUILD_INCLUDEDIR}/${libavstream_LIBRARY}/${dir})
  configure_file(${file} ${CMAKE_BUILD_INCLUDEDIR}/${libavstream_LIBRARY}/${dir} COPYONLY)
endforeach()

# Only use the mocked ./build/include dir as include directory. CMake will take
# care of always moving the up-to-date headers there.
target_include_directories(${libavstream_LIBRARY} PUBLIC ${CMAKE_BUILD_INCLUDEDIR})


INSTALL(TARGETS
    ${libavstream_LIBRARY}
    decode_rtp
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
if (BUILD_zmq_encoder)
    INSTALL(TARGETS
        encode_from_zmq
        )
endif()
message(STATUS "${libavstream_LIBRARY} CMAKE_INSTALL_INCLUDEDIR: ${CMAKE_INSTALL_INCLUDEDIR}")

message(STATUS "--------------------- stream end ---------------------------------")
