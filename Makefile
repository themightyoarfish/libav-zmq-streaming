CXX_FILES = encode_video_zmq.cpp
CXX_FLAGS = -std=c++14
LIBS = -lavformat -lswscale -lavcodec -lavresample -lavutil -lavdevice -lzmq -lopencv_core
INCLUDE_DIRS = -I /usr/local/include/opencv4/ -I /usr/local/include/
LIBRARY_DIRS = -L /usr/local/lib/
CXX = clang++

all: $(CXX_FILES)
	$(CXX) $(CXX_FLAGS) $(INCLUDE_DIRS) $(LIBRARY_DIRS) -o encode_video_rtsp $(LIBS) $(CXX_FILES)
