#ifndef FACE_TRACKER_H
#define FACE_TRACKER_H
#include <Windows.h>
#include <Kinect.h>
#include <Kinect.Face.h>
#include <opencv2/opencv.hpp>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class FaceTracker
{
public:
	FaceTracker(std::string name="Face Tracker");
	void Initialize();
	void Run();

private:
	HRESULT hResult;

	//Kinect camera functionality
	IKinectSensor* sensor;
	IColorFrameSource* colorSource;
	IBodyFrameSource* bodySource;
	IColorFrameReader* colorReader;
	IBodyFrameReader* bodyReader;
	IFrameDescription* description;
	ICoordinateMapper* coordinateMapper;
	IFaceFrameReader* faceReader[BODY_COUNT];
	IFaceFrameSource* faceSource[BODY_COUNT];
	//features we are using with Kinect
	DWORD features;

	//image sizes
	int width = 0;
	int height = 0;
	unsigned bufferSize;

	//opencv buffers to hold data
	cv::Mat bufferMat;
	cv::Mat faceMat;
	//title of window
	std::string windowName;
	//colors for face tracking
	cv::Vec3b faceColors[BODY_COUNT];

	std::string faceProperties[FaceProperty::FaceProperty_Count];

	template<class Interface>
	inline void SafeRelease(Interface *& interfaceToRelease)
	{
		if (interfaceToRelease != NULL) 
		{
			interfaceToRelease->Release();
			interfaceToRelease = NULL;
		}
	}
	void FaceTracker::ExtractFaceRotationInDegrees(const Vector4* pQuaternion, int* pPitch, int* pYaw, int* pRoll);
};
#endif