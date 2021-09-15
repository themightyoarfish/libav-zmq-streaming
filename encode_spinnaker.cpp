#include "avreceiver.hpp"
#include "avtransmitter.hpp"
#include "avutils.hpp"
#include <chrono>
#include <csignal>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <thread>
#include <vector>

#include "SpinGenApi/SpinnakerGenApi.h"
#include "Spinnaker.h"

using std::string;
using namespace Spinnaker::GenApi;

static Spinnaker::SystemPtr spinnaker_system = nullptr;
static Spinnaker::CameraPtr camera = nullptr;
static Spinnaker::CameraList camList;
static Spinnaker::ImagePtr currentFrame = nullptr;

static volatile bool stop = false;

void shutdown_camera(int signal) { stop = true; }

int setCameraSetting(const string &node, const string &value) {
  INodeMap &nodeMap = camera->GetNodeMap();

  // Retrieve enumeration node from nodemap
  CEnumerationPtr ptr = nodeMap.GetNode(node.c_str());
  if (!IsAvailable(ptr)) {
    return -1;
  }
  if (!IsWritable(ptr)) {
    return -1;
  }
  // Retrieve entry node from enumeration node
  CEnumEntryPtr ptrValue = ptr->GetEntryByName(value.c_str());
  if (!IsAvailable(ptrValue)) {
    return -1;
  }
  if (!IsReadable(ptrValue)) {
    return -1;
  }
  // retrieve value from entry node
  const int64_t valueToSet = ptrValue->GetValue();

  // Set value from entry node as new value of enumeration node
  ptr->SetIntValue(valueToSet);

  return 0;
}

int setCameraSetting(const string &node, int val) {
  INodeMap &nodeMap = camera->GetNodeMap();

  CIntegerPtr ptr = nodeMap.GetNode(node.c_str());
  if (!IsAvailable(ptr)) {
    return -1;
  }
  if (!IsWritable(ptr)) {
    return -1;
  }
  ptr->SetValue(val);
  return 0;
}
int setCameraSetting(const string &node, float val) {
  INodeMap &nodeMap = camera->GetNodeMap();
  CFloatPtr ptr = nodeMap.GetNode(node.c_str());
  if (!IsAvailable(ptr)) {
    return -1;
  }
  if (!IsWritable(ptr)) {
    return -1;
  }
  ptr->SetValue(val);

  return 0;
}
int setCameraSetting(const string &node, bool val) {
  INodeMap &nodeMap = camera->GetNodeMap();

  CBooleanPtr ptr = nodeMap.GetNode(node.c_str());
  if (!IsAvailable(ptr)) {
    return -1;
  }
  if (!IsWritable(ptr)) {
    return -1;
  }
  ptr->SetValue(val);

  return 0;
}

int setPixFmt() {
  try {
    return setCameraSetting("PixelFormat", string("BayerBG8"));
  } catch (const std::exception &) {
  }
  try {
    return setCameraSetting("PixelFormat", string("BayerRG8"));
  } catch (const std::exception &) {
  }
  return -1;
}

void resetUserSet() {
  std::this_thread::sleep_for(std::chrono::seconds(1));
  camera->UserSetSelector.SetValue(
      Spinnaker::UserSetSelectorEnums::UserSetSelector_Default);

  camera->UserSetLoad.Execute();
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

int setExposureTime(float exposure_time) {
  return setCameraSetting("ExposureTime", exposure_time);
}

int setExposureAuto(const string &mode) {
  return setCameraSetting("ExposureAuto", mode);
}

int setGainAutoDisable() { return setCameraSetting("GainAuto", string("Off")); }

int setSharpeningDisable() {
  return setCameraSetting("SharpeningEnable", false);
}

int setWhiteBalanceAuto() {
  return setCameraSetting("BalanceWhiteAuto", string("Continuous"));
}

int main(int argc, char *argv[]) {
  avformat_network_init();
  std::signal(SIGINT, shutdown_camera);

  std::string serial;
  std::string rtp_rcv_host;
  unsigned int rtp_rcv_port;

  if (argc > 3) {
    serial = argv[1];
    rtp_rcv_host = argv[2];
    rtp_rcv_port = std::atoi(argv[3]);
  } else {
    std::cout << "Usage: " << argv[0] << " <serial> <host> <port>" << std::endl;
    return 1;
  }
  constexpr int fps = 20;
  AVTransmitter transmitter(rtp_rcv_host, rtp_rcv_port, fps);

  spinnaker_system = Spinnaker::System::GetInstance();

  // Retrieve list of cameras from the spinnaker_system
  camList = spinnaker_system->GetCameras();

  camera = camList.GetBySerial(serial);

  if (!camera) {
    std::cout << "Camera could not be gotten." << std::endl;
    return 2;
  }

  camList.Clear();

  // Initialize camera
  std::cout << "Init camera" << std::endl;
  camera->Init();

  /* resetUserSet(); */

  std::cout << "Setting params" << std::endl;
  // Set acquisition mode to continuous
  if (setCameraSetting("AcquisitionMode", string("Continuous")) == -1) {
    throw std::runtime_error("Could not set AcquisitionMode");
  }

  setCameraSetting("AcquisitionFrameRateEnabled", true);
  setCameraSetting("AcquisitionFrameRateEnable", true);
  setCameraSetting("AcquisitionFrameRateAuto", std::string("Off"));
  setCameraSetting("AcquisitionFrameRate", fps);

  // Important, otherwise we don't get frames at all
  if (setPixFmt() == -1) {
    throw std::runtime_error("Could not set pixel format");
  }

  setCameraSetting("ExposureAuto", string("On"));

  std::cout << "Beginning acquisition" << std::endl;
  camera->BeginAcquisition();

  // wait a bit for camera to start streaming
  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::cout << "Beginning capture." << std::endl;

  while (!stop) {
    /* std::cout << "Gettin frame" << std::endl; */
    try {
      currentFrame = camera->GetNextImage(100);
      if (currentFrame->IsIncomplete()) {
        std::cout << "Incomplete" << std::endl;
        currentFrame->Release();
        currentFrame = nullptr;
      } else if (currentFrame->GetImageStatus() != Spinnaker::IMAGE_NO_ERROR) {
        std::cout << "Image Error" << std::endl;
        currentFrame->Release();
        currentFrame = nullptr;
      }
    } catch (const Spinnaker::Exception &e) {
      /* std::cout << "Exception: " << e.what() << std::endl; */
    }

    if (currentFrame) {
      /* std::cout << "Convert" << std::endl; */
      Spinnaker::ImagePtr convertedImage;

      convertedImage = currentFrame->Convert(Spinnaker::PixelFormat_RGB8,
                                             Spinnaker::NEAREST_NEIGHBOR);
      currentFrame->Release();
      currentFrame = convertedImage;

      cv::Mat image(currentFrame->GetHeight() + currentFrame->GetYPadding(),
                    currentFrame->GetWidth() + currentFrame->GetXPadding(),
                    CV_8UC3, currentFrame->GetData(),
                    currentFrame->GetStride());

      /* std::cout << "Sending image" << std::endl; */
      transmitter.encode_frame(image);
      /* std::cout << "Sent image" << std::endl; */
      currentFrame = nullptr;
    }
  }

  std::cout << "Shitting down cameras." << std::endl;

  if (currentFrame) {
    try {
      std::cout << "Release current frame" << std::endl;
      currentFrame->Release();
    } catch (Spinnaker::Exception &e) {
      std::cout << "Caught error " << e.what() << std::endl;
    }
    currentFrame = nullptr;
  }
  std::cout << "End acquisition" << std::endl;
  camera->EndAcquisition();
  std::cout << "Deinit camera" << std::endl;
  camera->DeInit();
  // when spinnaker_system  is released in same scope, camera ptr must be
  // cleaned up before
  camera = nullptr;
  std::cout << "Clear camera list" << std::endl;
  camList.Clear();
  std::cout << "Release system" << std::endl;
  spinnaker_system->ReleaseInstance();
  spinnaker_system = nullptr;

  return 0;
}
