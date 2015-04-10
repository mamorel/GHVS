#pragma once

#include <Windows.h>
#include <glew.h>
#include <strsafe.h>
#include <iostream>
#include <fstream>
#include "resource.h"
#include <NuiApi.h>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

class Kinect
{
public:
	Kinect();
	~Kinect();

	/// <summary>
	/// Main processing function
	/// </summary>
	void                    update(GLubyte* dest, glm::vec3 ** Bones);

	/// <summary>
	/// Create the first connected Kinect found 
	/// </summary>
	/// <returns>S_OK on success, otherwise failure code</returns>
	HRESULT                 createFirstConnected();

	/// <summary>
	/// Handle new color and skeleton data
	/// </summary>
	HRESULT                    process(GLubyte* dest, glm::vec3 ** Bones);
	void SaveSkeletonToFile(const NUI_SKELETON_DATA & skel, int windowWidth, int windowHeight, glm::vec3 ** Bones);

private:

	NUI_LOCKED_RECT m_LockedRect;

	//bool					m_SeatedMode;

	// Current Kinect
	INuiSensor*             m_pNuiSensor;

	HANDLE                  m_pColorStreamHandle;
	HANDLE                  m_hNextColorFrameEvent;
	//From Skeleton basics
	HANDLE                  m_pSkeletonStreamHandle;
	HANDLE                  m_hNextSkeletonEvent;

	static const int        cColorWidth = 640;
	static const int        cColorHeight = 480;
	static const int        cScreenWidth = 320;
	static const int        cScreenHeight = 240;

	static const int        cStatusMessageMaxLen = MAX_PATH * 2;
};

