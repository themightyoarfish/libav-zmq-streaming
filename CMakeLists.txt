project(zmq-streaming C CXX)
cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 14)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR})
# Use linked libraries in rpath, otherwise binaries in /bin try to call wrong libs in /usr/local etc.
list(APPEND CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
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

# # ##############################################################################
# # Files
# # ##############################################################################

LIST(APPEND STREAM_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/avtransmitter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/avreceiver.cpp
  ${CMAKE_CURRENT_LIST_DIR}/avutils.cpp
  ${CMAKE_CURRENT_LIST_DIR}/decode_rtp.cpp
  ${CMAKE_CURRENT_LIST_DIR}/decode_video_zmq.cpp
  ${CMAKE_CURRENT_LIST_DIR}/encode_spinnaker.cpp
  ${CMAKE_CURRENT_LIST_DIR}/encode_from_zmq.cpp
  ${CMAKE_CURRENT_LIST_DIR}/encode_video_fromdir.cpp
)

LIST(APPEND STREAM_HEADERS
${CMAKE_CURRENT_LIST_DIR}/avtransmitter.hpp
${CMAKE_CURRENT_LIST_DIR}/avreceiver.hpp
${CMAKE_CURRENT_LIST_DIR}/avutils.hpp
)

LIST(APPEND FILES
  ${STREAM_SRCS}
  ${STREAM_HEADERS})


# define subgroups for XCode and other IDEs
source_group( src FILES ${STREAM_HEADERS} ${STREAM_SRCS} )

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
target_include_directories(${libavstream_LIBRARY} PRIVATE SYSTEM ${THIRD_PARTY_INCLUDE_DIRS})
target_link_libraries(${libavstream_LIBRARY} PUBLIC ${THIRD_PARTY_LIBRARIES})
target_sources(${libavstream_LIBRARY} PRIVATE
  ${${libavstream_LIBRARY}_DIR}/avutils.cpp
  ${${libavstream_LIBRARY}_DIR}/avtransmitter.cpp)

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
  set_target_properties(${libavstream_LIBRARY} PROPERTIES PUBLIC_HEADER ${file})
  get_filename_component(dir ${file} DIRECTORY BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
  string(REGEX REPLACE "^${CMAKE_CURRENT_LIST_DIR}" "" dir "${dir}")
  string(REGEX REPLACE "^/" "" dir "${dir}")

  # During install, all headers are moved to a include/${libavstream_LIBRARY} subdirectory,
  # namespacing them away from other libraries to avoid conflicts.
  install(FILES ${file} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${libavstream_LIBRARY}/${dir})

  # During build time we create the include/${libavstream_LIBRARY}/**/*.h structure within
  # the ./build directory, such that it resembles what we have after an install
  # without the actual need to move the headers out of the ./src directory.
  # message(STATUS "Copy ${file} to ${CMAKE_BUILD_INCLUDEDIR}/${libavstream_LIBRARY}/${dir}")
  file(MAKE_DIRECTORY ${CMAKE_BUILD_INCLUDEDIR}/${libavstream_LIBRARY}/${dir})
  configure_file(${file} ${CMAKE_BUILD_INCLUDEDIR}/${libavstream_LIBRARY}/${dir} COPYONLY)
endforeach()

# Only use the mocked ./build/include dir as include directory. CMake will take
# care of always moving the up-to-date headers there.
target_include_directories(${libavstream_LIBRARY} PUBLIC ${CMAKE_BUILD_INCLUDEDIR})


INSTALL(TARGETS ${libavstream_LIBRARY} decode_rtp encode_from_zmq
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
message(STATUS "${libavstream_LIBRARY} CMAKE_INSTALL_INCLUDEDIR: ${CMAKE_INSTALL_INCLUDEDIR}")
