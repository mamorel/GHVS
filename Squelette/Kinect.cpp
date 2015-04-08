#include "Kinect.h"

using namespace std;


Kinect::Kinect() : m_hNextColorFrameEvent(INVALID_HANDLE_VALUE), //sert a detecter un evenement camera video
m_pColorStreamHandle(INVALID_HANDLE_VALUE),
m_hNextSkeletonEvent(INVALID_HANDLE_VALUE), //sert a detecter un evenement squelette
m_pSkeletonStreamHandle(INVALID_HANDLE_VALUE),
m_pNuiSensor(NULL) //permet de choisir le capteur à utiliser (camera IR ou couleur)
{
	createFirstConnected();
}


Kinect::~Kinect()
{
	if (m_pNuiSensor)
		m_pNuiSensor->NuiShutdown(); // éteint la caméra de la kinect

	if (m_hNextColorFrameEvent != INVALID_HANDLE_VALUE)
		CloseHandle(m_hNextColorFrameEvent);

	if (m_hNextSkeletonEvent && (m_hNextSkeletonEvent != INVALID_HANDLE_VALUE))
		CloseHandle(m_hNextSkeletonEvent);

	m_pNuiSensor = NULL;
}

void Kinect::update(GLubyte* dest)
{
	if (m_pNuiSensor == NULL)
		return;

	if (((WaitForSingleObject(m_hNextColorFrameEvent, 0)) && (WaitForSingleObject(m_hNextSkeletonEvent, 0))) == WAIT_OBJECT_0)
	{
		process(dest);//permet de mettre à jour le process (detection de la video en couleur et squelette)
	}
}

HRESULT Kinect::createFirstConnected()
{
	INuiSensor * pNuiSensor;
	HRESULT hr;

	int iSensorCount = 0;
	hr = NuiGetSensorCount(&iSensorCount); //compter le nombre de capteurs de la kinect et les mettre dans iSensorCount
	if (FAILED(hr))
	{
		return hr;
	}

	// Look at each Kinect sensor
	for (int i = 0; i < iSensorCount; ++i)
	{
		// Create the sensor so we can check status, if we can't create it, move on to the next
		hr = NuiCreateSensorByIndex(i, &pNuiSensor);
		if (FAILED(hr))
		{
			continue;
		}

		// Get the status of the sensor, and if connected, then we can initialize it
		hr = pNuiSensor->NuiStatus();
		if (hr == S_OK)
		{
			m_pNuiSensor = pNuiSensor;
			break;
		}

		// This sensor wasn't OK, so release it since we're not using it
		pNuiSensor->Release();
	}

	if (m_pNuiSensor != NULL)
	{
		// Initialize the Kinect and specify that we'll be using color AND SKELETON
		hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_SKELETON); //Added NUI_INITIALIZE_FLAG_USES_SKELETON
		// if preivous line not working, use this HRESULT hr = m_pSensorChooser->GetSensor(NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_COLOR, &m_pNuiSensor);
		if (SUCCEEDED(hr))
		{
			// Create an event that will be signaled when color data is available
			m_hNextColorFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			//From Skeleton basics. Create an event that'll be signaled when skeleton data is available
			m_hNextSkeletonEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

			// Open a color image stream to receive color frames
			hr = m_pNuiSensor->NuiImageStreamOpen(
				NUI_IMAGE_TYPE_COLOR, // signifie qu'on utilise le RGB. Autres possibilités: https://msdn.microsoft.com/en-us/library/nuiimagecamera.nui_image_type.aspx
				NUI_IMAGE_RESOLUTION_640x480, //resolution fixee a 640*480. Autres possibilites (le 1280*960 ne marche pas sur la kinect de Telecom): https://msdn.microsoft.com/en-us/library/nuiimagecamera.nui_image_resolution.aspx
				0,
				2,
				m_hNextColorFrameEvent,
				&m_pColorStreamHandle);

			// Open a skeleton stream to receive skeleton data
			hr = m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, 0);
		}
		else
		{
			ResetEvent(m_hNextColorFrameEvent);
			ResetEvent(m_hNextSkeletonEvent);
		}
	}

	if (m_pNuiSensor == NULL || FAILED(hr))
	{
		cout << "Pas de Kinect détectée." << endl;
		return E_FAIL;
	}

	return hr;
}

HRESULT Kinect::process(GLubyte* dest)
{
	HRESULT hr;
	NUI_IMAGE_FRAME imageFrame;
	NUI_SKELETON_FRAME skeletonFrame = { 0 };

	// Attempt to get the color frame
	hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pColorStreamHandle, 0, &imageFrame);
	if (FAILED(hr))
	{
		return hr;
	}

	INuiFrameTexture* pTexture = imageFrame.pFrameTexture;

	// Lock the frame data so the Kinect knows not to modify it while we're reading it
	pTexture->LockRect(0, &m_LockedRect, NULL, 0);

	if (m_LockedRect.Pitch != 0)
	{
		const BYTE* curr = (const BYTE*)m_LockedRect.pBits;
		memcpy(dest, curr, sizeof(char) * 640 * 480 * 4);
	}

	else
		cout << "Erreur de récupération de l'image." << endl;

	// We're done with the texture so unlock it
	pTexture->UnlockRect(0);

	// Release the frame
	m_pNuiSensor->NuiImageStreamReleaseFrame(m_pColorStreamHandle, &imageFrame);

	//ICI s'arrete la capture de la video, le code qui suit concerne la capture du squelette

	hr = m_pNuiSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame);
	if (FAILED(hr))
	{
		return hr;
	}
	// smooth out the skeleton data
	m_pNuiSensor->NuiTransformSmooth(&skeletonFrame, NULL);

	for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
	{
		NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;

		if (NUI_SKELETON_TRACKED == trackingState)
		{
			// We're tracking the skeleton, draw it
			SaveSkeletonToFile(skeletonFrame.SkeletonData[i], 640, 480); //pas sur pour le 640*480
		}
	}

	// Device lost, need to recreate the render target
	// We'll dispose it now and retry drawing
	m_pNuiSensor->NuiImageStreamReleaseFrame(m_pColorStreamHandle, &imageFrame);
	return hr;
}

void Kinect::SaveSkeletonToFile(const NUI_SKELETON_DATA & skel, int windowWidth, int windowHeight)
{
	ofstream myfile;
	//myfile.open("skelcoordinates3.txt", ios_base::out | ios_base::app); //ouverture en écriture, à la fin du fichier TEST
	myfile.open("skelcoordinates.txt", ios_base::out);

	/* PULL */
	/*
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].z - 2.0) / 2.0+2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].z - 2.0) / 2.0+2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT].z - 2.0) / 2.0 +2.0<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT].y) / 1.6 << "\n";
	myfile.close();
	*/

	/* ROBE1 */
	/*
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_LEFT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_RIGHT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT].y) / 1.6 << "\n";

	myfile.close();
	*/

	/* JEAN */
	/*
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_RIGHT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_RIGHT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_LEFT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_LEFT].y) / 1.6 << "\n";

	myfile.close();
	*/

	/* TSHIRT */
/*	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].x) / 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].z - 2.0) / 2.0 + 2.0 << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].y) / 1.6 << "\n";
*/

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].z - 2.0) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER].y) << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].z - 2.0)  << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].y) / 1.6 << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].z - 2.0)  << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SPINE].y) / 1.6 << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0)<< " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].z - 2.0) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].y)  << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].z - 2.0) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].y) << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].z - 2.0) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].y) << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].z - 2.0) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER].y) << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].z - 2.0) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].y) << "\n";

	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].z - 2.0) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].y) << "\n";
	myfile << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].x) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].z - 2.0) << " " << (skel.SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].y)  << "\n";

	myfile.close();
}
