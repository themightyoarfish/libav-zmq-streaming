CXX_FILES = encode_video_zmq.cpp
CXX_HDRS = avutils.hpp avtransmitter.hpp
CXX_FLAGS = -std=c++14 -g -O0
LIBS = -lavformat -lswscale -lavcodec -lavresample -lavutil -lavdevice -lzmq -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_imgcodecs -lc++
INCLUDE_DIRS = -I /usr/local/include/opencv4/ -I /usr/local/include/
LIBRARY_DIRS = -L /usr/local/lib/
CXX = clang++
EXE_NAME = stream_video_zmq

$(EXE_NAME): $(CXX_FILES) $(CXX_HDRS)
	$(CXX) $(CXX_FLAGS) $(INCLUDE_DIRS) $(LIBRARY_DIRS) -o $(EXE_NAME) $(LIBS) $(CXX_FILES)
