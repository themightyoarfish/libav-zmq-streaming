project(zmq-streaming C CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

find_package(OpenCV COMPONENTS core highgui imgcodecs imgproc REQUIRED)
find_package(LIBAV REQUIRED)
find_package(cppzmq REQUIRED)
set(cppzmq_LIBRARIES cppzmq)
set(LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR})
set(THIRD_PARTY_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS} ${cppzmq_INCLUDE_DIRS}
    ${LIBAV_INCLUDE_DIRS})
set(THIRD_PARTY_LIBRARIES ${OpenCV_LIBRARIES} ${cppzmq_LIBRARIES}
    ${LIBAV_LIBRARIES})
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
