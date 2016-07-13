#include "FaceTracker.h"
#include "FaceTrackerException.h"
#include <iostream>

FaceTracker::FaceTracker(std::string name) :
	windowName(name)
{

}
void FaceTracker::Initialize()
{
	hResult = S_OK;
	hResult = GetDefaultKinectSensor(&sensor);
	if (FAILED(hResult))
	{
		throw(FaceTrackerException("Error : GetDefaultKinectSensor"));
	}

	hResult = sensor->Open();
	if (FAILED(hResult))
	{
		throw(FaceTrackerException("Error : IKinectSensor::Open()"));
	}

	// Source
	hResult = sensor->get_ColorFrameSource(&colorSource);
	if (FAILED(hResult))
	{
		throw(FaceTrackerException("Error : IKinectSensor::get_ColorFrameSource()"));
	}

	hResult = sensor->get_BodyFrameSource(&bodySource);
	if (FAILED(hResult))
	{
		throw(FaceTrackerException("Error : IKinectSensor::get_BodyFrameSource()"));
	}

	// Reader
	hResult = colorSource->OpenReader(&colorReader);
	if (FAILED(hResult))
	{
		throw(FaceTrackerException("Error : IColorFrameSource::OpenReader()"));
	}

	hResult = bodySource->OpenReader(&bodyReader);
	if (FAILED(hResult))
	{
		throw(FaceTrackerException("Error : IBodyFrameSource::OpenReader()"));
	}

	// Description
	hResult = colorSource->get_FrameDescription(&description);
	if (FAILED(hResult))
	{
		throw(FaceTrackerException("Error : IColorFrameSource::get_FrameDescription()"));
	}
	hResult = sensor->get_CoordinateMapper(&coordinateMapper);
	if (FAILED(hResult))
	{
		throw(FaceTrackerException("Error : IKinectSensor::get_CoordinateMapper()"));
	}

	features = FaceFrameFeatures::FaceFrameFeatures_BoundingBoxInColorSpace
		| FaceFrameFeatures::FaceFrameFeatures_PointsInColorSpace
		| FaceFrameFeatures::FaceFrameFeatures_RotationOrientation
		| FaceFrameFeatures::FaceFrameFeatures_Happy
		| FaceFrameFeatures::FaceFrameFeatures_RightEyeClosed
		| FaceFrameFeatures::FaceFrameFeatures_LeftEyeClosed
		| FaceFrameFeatures::FaceFrameFeatures_MouthOpen
		| FaceFrameFeatures::FaceFrameFeatures_MouthMoved
		| FaceFrameFeatures::FaceFrameFeatures_LookingAway
		| FaceFrameFeatures::FaceFrameFeatures_Glasses
		| FaceFrameFeatures::FaceFrameFeatures_FaceEngagement;

	for (int i = 0; i < BODY_COUNT; ++i)
	{
		hResult = CreateFaceFrameSource(sensor, 0, features, &faceSource[i]);
		if (FAILED(hResult))
		{
			throw(FaceTrackerException("Error: CreateFaceFrameSource"));
		}

		hResult = faceSource[i]->OpenReader(&faceReader[i]);
		if (FAILED(hResult))
		{
			throw(FaceTrackerException("Error : IFaceFrameSource::OpenReader()"));
		}
	}
	description->get_Width(&width);
	description->get_Height(&height);
	bufferSize = width * height * 4 * sizeof(unsigned char);

	bufferMat = cv::Mat(height, width, CV_8UC4);
	faceMat = cv::Mat(height / 2, width / 2, CV_8UC4);

	faceColors[0] = cv::Vec3b(255, 0, 0);
	faceColors[1] = cv::Vec3b(0, 255, 0);
	faceColors[2] = cv::Vec3b(0, 0, 255);
	faceColors[3] = cv::Vec3b(255, 255, 0);
	faceColors[4] = cv::Vec3b(255, 0, 255);
	faceColors[5] = cv::Vec3b(0, 255, 255);

	faceProperties[0] = "Happy";
	faceProperties[1] = "Engaged";
	faceProperties[2] = "WearingGlasses";
	faceProperties[3] = "LeftEyeClosed";
	faceProperties[4] = "RightEyeClosed";
	faceProperties[5] = "MouthOpen";
	faceProperties[6] = "MouthMoved";
	faceProperties[7] = "LookingAway";
}
void FaceTracker::Run()
{
	cv::namedWindow(windowName);
	while (1)
	{
		// Color Frame
		IColorFrame* colorFrame(nullptr);
		hResult = colorReader->AcquireLatestFrame(&colorFrame);
		if (SUCCEEDED(hResult))
		{
			hResult = colorFrame->CopyConvertedFrameDataToArray(bufferSize, reinterpret_cast<BYTE*>(bufferMat.data), ColorImageFormat::ColorImageFormat_Bgra);
			if (SUCCEEDED(hResult))
			{
				cv::resize(bufferMat, faceMat, cv::Size(), 0.5, 0.5);
			}
		}
		SafeRelease(colorFrame);

		// Body Frame
		cv::Point point[BODY_COUNT];
		IBodyFrame* pBodyFrame(nullptr);
		hResult = bodyReader->AcquireLatestFrame(&pBodyFrame);
		if (SUCCEEDED(hResult))
		{
			IBody* pBody[BODY_COUNT] = { 0 };
			hResult = pBodyFrame->GetAndRefreshBodyData(BODY_COUNT, pBody);
			if (SUCCEEDED(hResult))
			{
				for (int i = 0; i < BODY_COUNT; i++) {
					BOOLEAN bTracked(false);
					hResult = pBody[i]->get_IsTracked(&bTracked);
					if (SUCCEEDED(hResult) && bTracked)
					{
						// Set TrackingID to Detect Face
						UINT64 trackingId = _UI64_MAX;
						hResult = pBody[i]->get_TrackingId(&trackingId);
						if (SUCCEEDED(hResult))
						{
							faceSource[i]->put_TrackingId(trackingId);
						}
					}
				}
			}
			for (int i = 0; i < BODY_COUNT; i++)
			{
				SafeRelease(pBody[i]);
			}
		}
		SafeRelease(pBodyFrame);

		for (int i = 0; i < BODY_COUNT; i++)
		{
			IFaceFrame* pFaceFrame(nullptr);
			hResult = faceReader[i]->AcquireLatestFrame(&pFaceFrame);
			if (SUCCEEDED(hResult) && pFaceFrame != nullptr)
			{
				BOOLEAN bFaceTracked(false);
				hResult = pFaceFrame->get_IsTrackingIdValid(&bFaceTracked);
				if (SUCCEEDED(hResult) && bFaceTracked)
				{
					IFaceFrameResult* pFaceResult(nullptr);
					hResult = pFaceFrame->get_FaceFrameResult(&pFaceResult);
					if (SUCCEEDED(hResult) && pFaceResult != nullptr)
					{
						std::vector<std::string> result;

						// Face Point
						PointF facePoint[FacePointType::FacePointType_Count];
						hResult = pFaceResult->GetFacePointsInColorSpace(FacePointType::FacePointType_Count, facePoint);
						if (SUCCEEDED(hResult))
						{
							cv::circle(bufferMat, cv::Point(static_cast<int>(facePoint[0].X), static_cast<int>(facePoint[0].Y)), 5, static_cast<cv::Scalar>(faceColors[i]), -1, CV_AA); // Eye (Left)
							cv::circle(bufferMat, cv::Point(static_cast<int>(facePoint[1].X), static_cast<int>(facePoint[1].Y)), 5, static_cast<cv::Scalar>(faceColors[i]), -1, CV_AA); // Eye (Right)
							cv::circle(bufferMat, cv::Point(static_cast<int>(facePoint[2].X), static_cast<int>(facePoint[2].Y)), 5, static_cast<cv::Scalar>(faceColors[i]), -1, CV_AA); // Nose
							cv::circle(bufferMat, cv::Point(static_cast<int>(facePoint[3].X), static_cast<int>(facePoint[3].Y)), 5, static_cast<cv::Scalar>(faceColors[i]), -1, CV_AA); // Mouth (Left)
							cv::circle(bufferMat, cv::Point(static_cast<int>(facePoint[4].X), static_cast<int>(facePoint[4].Y)), 5, static_cast<cv::Scalar>(faceColors[i]), -1, CV_AA); // Mouth (Right)

							// Face Bounding Box
							RectI boundingBox;
							hResult = pFaceResult->get_FaceBoundingBoxInColorSpace(&boundingBox);
							if (SUCCEEDED(hResult))
							{
								cv::rectangle(bufferMat, cv::Rect(boundingBox.Left, boundingBox.Top, boundingBox.Right - boundingBox.Left, boundingBox.Bottom - boundingBox.Top), static_cast<cv::Scalar>(faceColors[i]));
							}

							// Face Rotation
							Vector4 faceRotation;
							hResult = pFaceResult->get_FaceRotationQuaternion(&faceRotation);
							if (SUCCEEDED(hResult))
							{
								int pitch, yaw, roll;
								ExtractFaceRotationInDegrees(&faceRotation, &pitch, &yaw, &roll);
								result.push_back("Pitch, Yaw, Roll : " + std::to_string(pitch) + ", " + std::to_string(yaw) + ", " + std::to_string(roll));
							}

						}
						SafeRelease(pFaceResult);
					}
				}
				SafeRelease(pFaceFrame);
			}
		}
		cv::resize(bufferMat, faceMat, cv::Size(), 0.5, 0.5);
		imshow(windowName, faceMat);
		if (cv::waitKey(20) == VK_ESCAPE)
		{
			break;
		}
	}
}
void FaceTracker::ExtractFaceRotationInDegrees(const Vector4* pQuaternion, int* pPitch, int* pYaw, int* pRoll)
{
	double x = pQuaternion->x;
	double y = pQuaternion->y;
	double z = pQuaternion->z;
	double w = pQuaternion->w;

	// convert face rotation quaternion to Euler angles in degrees
	*pPitch = static_cast<int>(std::atan2(2 * (y * z + w * x), w * w - x * x - y * y + z * z) / M_PI * 180.0f);
	*pYaw = static_cast<int>(std::asin(2 * (w * y - x * z)) / M_PI * 180.0f);
	*pRoll = static_cast<int>(std::atan2(2 * (x * y + w * z), w * w + x * x - y * y - z * z) / M_PI * 180.0f);
}