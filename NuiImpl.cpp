//------------------------------------------------------------------------------
// <copyright file="NuiImpl.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

// Implementation of CKinectShopApp methods dealing with NUI processing

#include "stdafx.h"
#include "resource.h"
#include <mmsystem.h>
#include <assert.h>
#include <strsafe.h>

#include "KinectShop.h"
#include "KinectAudioStream.h"

#include <opencv2/imgproc/imgproc.hpp>

#include "KinectProductLocator.h"

// Static initializers
LPCWSTR CKinectShopApp::GrammarFileName = L"SpeechBasics.grxml";

//lookups for color tinting based on player index
static const int g_IntensityShiftByPlayerR[] = { 1, 2, 0, 2, 0, 0, 2, 0 };
static const int g_IntensityShiftByPlayerG[] = { 1, 2, 2, 0, 2, 0, 0, 1 };
static const int g_IntensityShiftByPlayerB[] = { 1, 0, 2, 2, 0, 2, 0, 2 };

static const float g_JointThickness = 3.0f;
static const float g_TrackedBoneThickness = 6.0f;
static const float g_InferredBoneThickness = 1.0f;

const int g_BytesPerPixel = 4;

const int g_ScreenWidth = 320;
const int g_ScreenHeight = 240;

enum _SV_TRACKED_SKELETONS
{
    SV_TRACKED_SKELETONS_DEFAULT = 0,
    SV_TRACKED_SKELETONS_NEAREST1,
    SV_TRACKED_SKELETONS_NEAREST2,
    SV_TRACKED_SKELETONS_STICKY1,
    SV_TRACKED_SKELETONS_STICKY2
} SV_TRACKED_SKELETONS;

enum _SV_TRACKING_MODE
{
    SV_TRACKING_MODE_DEFAULT = 0,
    SV_TRACKING_MODE_SEATED
} SV_TRACKING_MODE;

enum _SV_RANGE
{
    SV_RANGE_DEFAULT = 0,
    SV_RANGE_NEAR,
} SV_RANGE;
/// <summary>
/// Zero out member variables
/// </summary>
void CKinectShopApp::Nui_Zero()
{
    SafeRelease( m_pNuiSensor );

    m_pRenderTarget = NULL;
    m_pBrushJointTracked = NULL;
    m_pBrushJointInferred = NULL;
    m_pBrushBoneTracked = NULL;
    m_pBrushBoneInferred = NULL;
    ZeroMemory(m_Points,sizeof(m_Points));

    m_hNextDepthFrameEvent = NULL;
    m_hNextColorFrameEvent = NULL;
    m_hNextSkeletonEvent = NULL;
    m_pDepthStreamHandle = NULL;
    m_pVideoStreamHandle = NULL;
    m_hThNuiProcess = NULL;
    m_hEvNuiProcessStop = NULL;
    m_LastSkeletonFoundTime = 0;
    m_bScreenBlanked = false;
    m_DepthFramesTotal = 0;
    m_LastDepthFPStime = 0;
    m_LastDepthFramesTotal = 0;
    m_pDrawDepth = NULL;
    m_pDrawColor = NULL;
    m_TrackedSkeletons = 0;
    m_SkeletonTrackingFlags = NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE;
    m_DepthStreamFlags = 0;
    ZeroMemory(m_StickySkeletonIds,sizeof(m_StickySkeletonIds));
}

/// <summary>
/// Callback to handle Kinect status changes, redirects to the class callback handler
/// </summary>
/// <param name="hrStatus">current status</param>
/// <param name="instanceName">instance name of Kinect the status change is for</param>
/// <param name="uniqueDeviceName">unique device name of Kinect the status change is for</param>
/// <param name="pUserData">additional data</param>
void CALLBACK CKinectShopApp::Nui_StatusProcThunk( HRESULT hrStatus, const OLECHAR* instanceName, const OLECHAR* uniqueDeviceName, void * pUserData )
{
    reinterpret_cast<CKinectShopApp *>(pUserData)->Nui_StatusProc( hrStatus, instanceName, uniqueDeviceName );
}

/// <summary>
/// Callback to handle Kinect status changes
/// </summary>
/// <param name="hrStatus">current status</param>
/// <param name="instanceName">instance name of Kinect the status change is for</param>
/// <param name="uniqueDeviceName">unique device name of Kinect the status change is for</param>
void CALLBACK CKinectShopApp::Nui_StatusProc( HRESULT hrStatus, const OLECHAR* instanceName, const OLECHAR* uniqueDeviceName )
{
    // Update UI
    PostMessageW( m_hWnd, WM_USER_UPDATE_COMBO, 0, 0 );

    if( SUCCEEDED(hrStatus) )
    {
        if ( S_OK == hrStatus )
        {
            if ( m_instanceId && 0 == wcscmp(instanceName, m_instanceId) )
            {
                Nui_Init(m_instanceId);
            }
            else if ( !m_pNuiSensor )
            {
                Nui_Init();
            }
        }
    }
    else
    {
        if ( m_instanceId && 0 == wcscmp(instanceName, m_instanceId) )
        {
            Nui_UnInit();
            Nui_Zero();
        }
    }
}

/// <summary>
/// Initialize Kinect by instance name
/// </summary>
/// <param name="instanceName">instance name of Kinect to initialize</param>
/// <returns>S_OK if successful, otherwise an error code</returns>
HRESULT CKinectShopApp::Nui_Init( OLECHAR *instanceName )
{
    // Generic creation failure
    if ( NULL == instanceName )
    {
        MessageBoxResource( IDS_ERROR_NUICREATE, MB_OK | MB_ICONHAND );
        return E_FAIL;
    }

    HRESULT hr = NuiCreateSensorById( instanceName, &m_pNuiSensor );
    
    // Generic creation failure
    if ( FAILED(hr) )
    {
        MessageBoxResource( IDS_ERROR_NUICREATE, MB_OK | MB_ICONHAND );
        return hr;
    }

    SysFreeString(m_instanceId);

    m_instanceId = m_pNuiSensor->NuiDeviceConnectionId();

    return Nui_Init();
}

/// <summary>
/// Initialize Kinect
/// </summary>
/// <returns>S_OK if successful, otherwise an error code</returns>
HRESULT CKinectShopApp::Nui_Init( )
{
    HRESULT  hr;
    bool     result;

    if ( !m_pNuiSensor )
    {
        hr = NuiCreateSensorByIndex(0, &m_pNuiSensor);

        if ( FAILED(hr) )
        {
            return hr;
        }

        SysFreeString(m_instanceId);

        m_instanceId = m_pNuiSensor->NuiDeviceConnectionId();
    }

    m_hNextDepthFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    m_hNextColorFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    m_hNextSkeletonEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

    // reset the tracked skeletons, range, and tracking mode
    SendDlgItemMessage(m_hWnd, IDC_TRACKEDSKELETONS, CB_SETCURSEL, 0, 0);
    SendDlgItemMessage(m_hWnd, IDC_TRACKINGMODE, CB_SETCURSEL, 0, 0);
    SendDlgItemMessage(m_hWnd, IDC_RANGE, CB_SETCURSEL, 0, 0);

    EnsureDirect2DResources();

    m_pDrawDepth = new DrawDevice( );
    result = m_pDrawDepth->Initialize( GetDlgItem( m_hWnd, IDC_DEPTHVIEWER ), m_pD2DFactory, DEPTH_IMAGE_WIDTH, DEPTH_IMAGE_HEIGHT, DEPTH_IMAGE_WIDTH * 4 );
    if ( !result )
    {
        MessageBoxResource( IDS_ERROR_DRAWDEVICE, MB_OK | MB_ICONHAND );
        return E_FAIL;
    }

    m_pDrawColor = new DrawDevice( );
    result = m_pDrawColor->Initialize( GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), m_pD2DFactory, COLOR_IMAGE_WIDTH, COLOR_IMAGE_HEIGHT, COLOR_IMAGE_WIDTH * 4 );
    if ( !result )
    {
        MessageBoxResource( IDS_ERROR_DRAWDEVICE, MB_OK | MB_ICONHAND );
        return E_FAIL;
    }
    
    m_pDrawProduct = new DrawDevice( );
    result = m_pDrawProduct->Initialize( GetDlgItem( m_hWnd, IDC_PRODUCTVIEWER ), m_pD2DFactory, PRODUCT_IMAGE_WIDTH, PRODUCT_IMAGE_HEIGHT, PRODUCT_IMAGE_WIDTH * 4 );
    if ( !result )
    {
        MessageBoxResource( IDS_ERROR_DRAWDEVICE, MB_OK | MB_ICONHAND );
        return E_FAIL;
    }
    
    DWORD nuiFlags = NUI_INITIALIZE_FLAG_USES_AUDIO | NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON |  NUI_INITIALIZE_FLAG_USES_COLOR;

    hr = m_pNuiSensor->NuiInitialize(nuiFlags);
    if ( E_NUI_SKELETAL_ENGINE_BUSY == hr )
    {
        nuiFlags = NUI_INITIALIZE_FLAG_USES_DEPTH |  NUI_INITIALIZE_FLAG_USES_COLOR;
        hr = m_pNuiSensor->NuiInitialize( nuiFlags) ;
    }
  
    if ( FAILED( hr ) )
    {
        if ( E_NUI_DEVICE_IN_USE == hr )
        {
            MessageBoxResource( IDS_ERROR_IN_USE, MB_OK | MB_ICONHAND );
        }
        else
        {
            MessageBoxResource( IDS_ERROR_NUIINIT, MB_OK | MB_ICONHAND );
        }
        return hr;
    }

	hr = InitializeAudioStream();
    if (FAILED(hr))
    {
        MessageBoxResource( IDS_ERROR_AUDIOSTREAM , MB_OK | MB_ICONHAND );
        return hr;
    }

    hr = CreateSpeechRecognizer();
    if (FAILED(hr))
    {
        MessageBoxResource( IDS_ERROR_SPEECHSDK , MB_OK | MB_ICONHAND );
        return hr;
    }

    hr = LoadSpeechGrammar();
    if (FAILED(hr))
    {
        MessageBoxResource( IDS_ERROR_GRAMMAR , MB_OK | MB_ICONHAND );
        return hr;
    }

    hr = StartSpeechRecognition();
    if (FAILED(hr))
    {
        MessageBoxResource( IDS_ERROR_RECOGNITION , MB_OK | MB_ICONHAND );
        return hr;
    }

    if ( HasSkeletalEngine( m_pNuiSensor ) )
    {
        hr = m_pNuiSensor->NuiSkeletonTrackingEnable( m_hNextSkeletonEvent, m_SkeletonTrackingFlags );
        if( FAILED( hr ) )
        {
            MessageBoxResource( IDS_ERROR_SKELETONTRACKING, MB_OK | MB_ICONHAND );
            return hr;
        }
    }

    hr = m_pNuiSensor->NuiImageStreamOpen(
        NUI_IMAGE_TYPE_COLOR,
        COLOR_IMAGE_RESOLUTION,
        0,
        2,
        m_hNextColorFrameEvent,
        &m_pVideoStreamHandle );

    if ( FAILED( hr ) )
    {
        MessageBoxResource( IDS_ERROR_VIDEOSTREAM, MB_OK | MB_ICONHAND );
        return hr;
    }

    hr = m_pNuiSensor->NuiImageStreamOpen(
        HasSkeletalEngine(m_pNuiSensor) ? NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX : NUI_IMAGE_TYPE_DEPTH,
        DEPTH_IMAGE_RESOLUTION,
        m_DepthStreamFlags,
        2,
        m_hNextDepthFrameEvent,
        &m_pDepthStreamHandle );

    if ( FAILED( hr ) )
    {
        MessageBoxResource(IDS_ERROR_DEPTHSTREAM, MB_OK | MB_ICONHAND);
        return hr;
    }

    // Start the Nui processing thread
    m_hEvNuiProcessStop = CreateEvent( NULL, FALSE, FALSE, NULL );
    m_hThNuiProcess = CreateThread( NULL, 0, Nui_ProcessThread, this, 0, NULL );

    return hr;
}

/// <summary>
/// Uninitialize Kinect
/// </summary>
void CKinectShopApp::Nui_UnInit( )
{
    // Stop the Nui processing thread
    if ( NULL != m_hEvNuiProcessStop )
    {
        // Signal the thread
        SetEvent(m_hEvNuiProcessStop);

        // Wait for thread to stop
        if ( NULL != m_hThNuiProcess )
        {
            WaitForSingleObject( m_hThNuiProcess, INFINITE );
            CloseHandle( m_hThNuiProcess );
        }
        CloseHandle( m_hEvNuiProcessStop );
    }

    if ( m_pNuiSensor )
    {
        m_pNuiSensor->NuiShutdown( );
    }
    if ( m_hNextSkeletonEvent && ( m_hNextSkeletonEvent != INVALID_HANDLE_VALUE ) )
    {
        CloseHandle( m_hNextSkeletonEvent );
        m_hNextSkeletonEvent = NULL;
    }
    if ( m_hNextDepthFrameEvent && ( m_hNextDepthFrameEvent != INVALID_HANDLE_VALUE ) )
    {
        CloseHandle( m_hNextDepthFrameEvent );
        m_hNextDepthFrameEvent = NULL;
    }
    if ( m_hNextColorFrameEvent && ( m_hNextColorFrameEvent != INVALID_HANDLE_VALUE ) )
    {
        CloseHandle( m_hNextColorFrameEvent );
        m_hNextColorFrameEvent = NULL;
    }

    SafeRelease( m_pNuiSensor );
    
    // clean up Direct2D graphics
    delete m_pDrawDepth;
    m_pDrawDepth = NULL;

    delete m_pDrawColor;
    m_pDrawColor = NULL;

    delete m_pDrawProduct;
    m_pDrawProduct = NULL;

    DiscardDirect2DResources();
}

/// <summary>
/// Thread to handle Kinect processing, calls class instance thread processor
/// </summary>
/// <param name="pParam">instance pointer</param>
/// <returns>always 0</returns>
DWORD WINAPI CKinectShopApp::Nui_ProcessThread( LPVOID pParam )
{
    CKinectShopApp *pthis = (CKinectShopApp *)pParam;
    return pthis->Nui_ProcessThread( );
}

/// <summary>
/// Thread to handle Kinect processing
/// </summary>
/// <returns>always 0</returns>
DWORD WINAPI CKinectShopApp::Nui_ProcessThread( )
{
    const int numEvents = 4;
    HANDLE hEvents[numEvents] = { m_hEvNuiProcessStop, m_hNextDepthFrameEvent, m_hNextColorFrameEvent, m_hNextSkeletonEvent };
    int    nEventIdx;
    DWORD  t;

    m_LastDepthFPStime = timeGetTime( );

    // Blank the skeleton display on startup
    m_LastSkeletonFoundTime = 0;

    // Main thread loop
    bool continueProcessing = true;
    while ( continueProcessing )
    {
        // Wait for any of the events to be signalled
        nEventIdx = WaitForMultipleObjects( numEvents, hEvents, FALSE, 100 );

        // Timed out, continue
        if ( nEventIdx == WAIT_TIMEOUT )
        {
            continue;
        }

        // stop event was signalled 
        if ( WAIT_OBJECT_0 == nEventIdx )
        {
            continueProcessing = false;
            break;
        }

        // Wait for each object individually with a 0 timeout to make sure to
        // process all signalled objects if multiple objects were signalled
        // this loop iteration

        // In situations where perfect correspondance between color/depth/skeleton
        // is essential, a priority queue should be used to service the item
        // which has been updated the longest ago

        if ( WAIT_OBJECT_0 == WaitForSingleObject( m_hNextDepthFrameEvent, 0 ) )
        {
            //only increment frame count if a frame was successfully drawn
            if ( Nui_GotDepthAlert() )
            {
                ++m_DepthFramesTotal;
            }
        }

        if ( WAIT_OBJECT_0 == WaitForSingleObject( m_hNextColorFrameEvent, 0 ) )
        {
            Nui_GotColorAlert();
        }

        if (  WAIT_OBJECT_0 == WaitForSingleObject( m_hNextSkeletonEvent, 0 ) )
        {
            Nui_GotSkeletonAlert( );
        }

        // Once per second, display the depth FPS
        t = timeGetTime( );
        if ( (t - m_LastDepthFPStime) > 1000 )
        {
            int fps = ((m_DepthFramesTotal - m_LastDepthFramesTotal) * 1000 + 500) / (t - m_LastDepthFPStime);
            PostMessageW( m_hWnd, WM_USER_UPDATE_FPS, IDC_FPS, fps );
            m_LastDepthFramesTotal = m_DepthFramesTotal;
            m_LastDepthFPStime = t;
        }

        // Blank the skeleton panel if we haven't found a skeleton recently
        if ( (t - m_LastSkeletonFoundTime) > 300 )
        {
            if ( !m_bScreenBlanked )
            {
                Nui_BlankSkeletonScreen( );
                m_bScreenBlanked = true;
            }
        }
    }

    return 0;
}

/// <summary>
/// Handle new color data
/// </summary>
/// <returns>true if a frame was processed, false otherwise</returns>
bool CKinectShopApp::Nui_GotColorAlert( )
{
    NUI_IMAGE_FRAME imageFrame;
    bool processedFrame = true;

    HRESULT hr = m_pNuiSensor->NuiImageStreamGetNextFrame( m_pVideoStreamHandle, 0, &imageFrame );

    if ( FAILED( hr ) )
    {
        return false;
    }

    INuiFrameTexture * pTexture = imageFrame.pFrameTexture;
    NUI_LOCKED_RECT LockedRect;
    pTexture->LockRect( 0, &LockedRect, NULL, 0 );
    if ( LockedRect.Pitch != 0 )
    {
        m_pDrawColor->Draw( static_cast<BYTE *>(LockedRect.pBits), LockedRect.size );
		WaitForSingleObject(m_colorImageMutex, INFINITE);
		m_colorImage.create(COLOR_IMAGE_HEIGHT, COLOR_IMAGE_WIDTH, CV_8UC4);
		m_colorImage.data = static_cast<BYTE *>(LockedRect.pBits);
		ReleaseMutex(m_colorImageMutex);
    }
    else
    {
        OutputDebugString( L"Buffer length of received texture is bogus\r\n" );
        processedFrame = false;
    }

    pTexture->UnlockRect( 0 );

    m_pNuiSensor->NuiImageStreamReleaseFrame( m_pVideoStreamHandle, &imageFrame );

    return processedFrame;
}

/// <summary>
/// Handle new depth data
/// </summary>
/// <returns>true if a frame was processed, false otherwise</returns>
bool CKinectShopApp::Nui_GotDepthAlert( )
{
    NUI_IMAGE_FRAME imageFrame;
    bool processedFrame = true;

    HRESULT hr = m_pNuiSensor->NuiImageStreamGetNextFrame(
        m_pDepthStreamHandle,
        0,
        &imageFrame );

    if ( FAILED( hr ) )
    {
        return false;
    }

    INuiFrameTexture * pTexture = imageFrame.pFrameTexture;
    NUI_LOCKED_RECT LockedRect;
    pTexture->LockRect( 0, &LockedRect, NULL, 0 );
    if ( 0 != LockedRect.Pitch )
    {
        DWORD frameWidth, frameHeight;
        
        NuiImageResolutionToSize( imageFrame.eResolution, frameWidth, frameHeight );
        
        // draw the bits to the bitmap
        BYTE * rgbrun = m_depthRGBX;
        const USHORT * pBufferRun = (const USHORT *)LockedRect.pBits;

        // end pixel is start + width*height - 1
        const USHORT * pBufferEnd = pBufferRun + (frameWidth * frameHeight);

        assert( frameWidth * frameHeight * g_BytesPerPixel <= ARRAYSIZE(m_depthRGBX) );

        while ( pBufferRun < pBufferEnd )
        {
            USHORT depth     = *pBufferRun;
            USHORT realDepth = NuiDepthPixelToDepth(depth);
            USHORT player    = NuiDepthPixelToPlayerIndex(depth);

            // transform 13-bit depth information into an 8-bit intensity appropriate
            // for display (we disregard information in most significant bit)
            BYTE intensity = static_cast<BYTE>(~(realDepth >> 4));

            // tint the intensity by dividing by per-player values
            *(rgbrun++) = intensity >> g_IntensityShiftByPlayerB[player];
            *(rgbrun++) = intensity >> g_IntensityShiftByPlayerG[player];
            *(rgbrun++) = intensity >> g_IntensityShiftByPlayerR[player];

            // no alpha information, skip the last byte
            ++rgbrun;

            ++pBufferRun;
        }

        m_pDrawDepth->Draw( m_depthRGBX, frameWidth * frameHeight * g_BytesPerPixel );
		WaitForSingleObject(m_depthImageMutex, INFINITE);
		m_depthImage.create(DEPTH_IMAGE_HEIGHT, DEPTH_IMAGE_WIDTH, CV_16UC1);
		m_depthImage.data = static_cast<BYTE *>(LockedRect.pBits);
		ReleaseMutex(m_depthImageMutex);
	}
    else
    {
        processedFrame = false;
        OutputDebugString( L"Buffer length of received texture is bogus\r\n" );
    }

    pTexture->UnlockRect( 0 );

    m_pNuiSensor->NuiImageStreamReleaseFrame( m_pDepthStreamHandle, &imageFrame );

    return processedFrame;
}

/// <summary>
/// Blank the skeleton display
/// </summary>
void CKinectShopApp::Nui_BlankSkeletonScreen( )
{
	if ( NULL != m_pRenderTarget )
	{
		m_pRenderTarget->BeginDraw();
		m_pRenderTarget->Clear( );
		m_pRenderTarget->EndDraw( );
	}
}

/// <summary>
/// Draws a line between two bones
/// </summary>
/// <param name="skel">skeleton to draw bones from</param>
/// <param name="bone0">bone to start drawing from</param>
/// <param name="bone1">bone to end drawing at</param>
void CKinectShopApp::Nui_DrawBone( const NUI_SKELETON_DATA & skel, NUI_SKELETON_POSITION_INDEX bone0, NUI_SKELETON_POSITION_INDEX bone1 )
{
    NUI_SKELETON_POSITION_TRACKING_STATE bone0State = skel.eSkeletonPositionTrackingState[bone0];
    NUI_SKELETON_POSITION_TRACKING_STATE bone1State = skel.eSkeletonPositionTrackingState[bone1];

    // If we can't find either of these joints, exit
    if ( bone0State == NUI_SKELETON_POSITION_NOT_TRACKED || bone1State == NUI_SKELETON_POSITION_NOT_TRACKED )
    {
        return;
    }
    
    // Don't draw if both points are inferred
    if ( bone0State == NUI_SKELETON_POSITION_INFERRED && bone1State == NUI_SKELETON_POSITION_INFERRED )
    {
        return;
    }

    // We assume all drawn bones are inferred unless BOTH joints are tracked
    if ( bone0State == NUI_SKELETON_POSITION_TRACKED && bone1State == NUI_SKELETON_POSITION_TRACKED )
    {
        m_pRenderTarget->DrawLine( m_Points[bone0], m_Points[bone1], m_pBrushBoneTracked, g_TrackedBoneThickness );
    }
    else
    {
        m_pRenderTarget->DrawLine( m_Points[bone0], m_Points[bone1], m_pBrushBoneInferred, g_InferredBoneThickness );
    }
}

void CKinectShopApp::Nui_DrawProductImage( const cv::Mat* productImage )				
{
	cv::Mat scaledImg, showImg;
	cv::resize(*productImage, scaledImg, cv::Size(PRODUCT_IMAGE_WIDTH, PRODUCT_IMAGE_HEIGHT));
	cv::cvtColor(scaledImg, showImg, CV_BGR2BGRA);
	m_pDrawProduct->Draw( static_cast<BYTE *>(showImg.data), showImg.rows*showImg.cols* (unsigned long) showImg.elemSize() );
}

/// <summary>
/// Draws a skeleton
/// </summary>
/// <param name="skel">skeleton to draw</param>
/// <param name="windowWidth">width (in pixels) of output buffer</param>
/// <param name="windowHeight">height (in pixels) of output buffer</param>
void CKinectShopApp::Nui_DrawSkeleton( const NUI_SKELETON_DATA & skel, int windowWidth, int windowHeight )
{      
    int i;

    for (i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
    {
        m_Points[i] = SkeletonToScreen( skel.SkeletonPositions[i], windowWidth, windowHeight );
    }

    // Render Torso
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT );

    // Left Arm
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT );

    // Right Arm
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT );

    // Left Leg
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT );

    // Right Leg
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT );
    Nui_DrawBone( skel, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT );
    
    // Draw the joints in a different color
    for ( i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
    {
        D2D1_ELLIPSE ellipse = D2D1::Ellipse( m_Points[i], g_JointThickness, g_JointThickness );

        if ( skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_INFERRED )
        {
            m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointInferred);
        }
        else if ( skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_TRACKED )
        {
            m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointTracked);
        }
    }
}

/// <summary>
/// Determines which skeletons to track and tracks them
/// </summary>
/// <param name="skel">skeleton frame information</param>
void CKinectShopApp::UpdateTrackedSkeletons( const NUI_SKELETON_FRAME & skel )
{
    DWORD nearestIDs[2] = { 0, 0 };
    USHORT nearestDepths[2] = { NUI_IMAGE_DEPTH_MAXIMUM, NUI_IMAGE_DEPTH_MAXIMUM };

    // Purge old sticky skeleton IDs, if the user has left the frame, etc
    bool stickyID0Found = false;
    bool stickyID1Found = false;
    for ( int i = 0 ; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_TRACKING_STATE trackingState = skel.SkeletonData[i].eTrackingState;

        if ( trackingState == NUI_SKELETON_TRACKED || trackingState == NUI_SKELETON_POSITION_ONLY )
        {
            if ( skel.SkeletonData[i].dwTrackingID == m_StickySkeletonIds[0] )
            {
                stickyID0Found = true;
            }
            else if ( skel.SkeletonData[i].dwTrackingID == m_StickySkeletonIds[1] )
            {
                stickyID1Found = true;
            }
        }
    }

    if ( !stickyID0Found && stickyID1Found )
    {
        m_StickySkeletonIds[0] = m_StickySkeletonIds[1];
        m_StickySkeletonIds[1] = 0;
    }
    else if ( !stickyID0Found )
    {
        m_StickySkeletonIds[0] = 0;
    }
    else if ( !stickyID1Found )
    {
        m_StickySkeletonIds[1] = 0;
    }

    // Calculate nearest and sticky skeletons
    for ( int i = 0 ; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_TRACKING_STATE trackingState = skel.SkeletonData[i].eTrackingState;

        if ( trackingState == NUI_SKELETON_TRACKED || trackingState == NUI_SKELETON_POSITION_ONLY )
        {
            // Save SkeletonIds for sticky mode if there's none already saved
            if ( 0 == m_StickySkeletonIds[0] && m_StickySkeletonIds[1] != skel.SkeletonData[i].dwTrackingID )
            {
                m_StickySkeletonIds[0] = skel.SkeletonData[i].dwTrackingID;
            }
            else if ( 0 == m_StickySkeletonIds[1] && m_StickySkeletonIds[0] != skel.SkeletonData[i].dwTrackingID )
            {
                m_StickySkeletonIds[1] = skel.SkeletonData[i].dwTrackingID;
            }

            LONG x, y;
            USHORT depth;

            // calculate the skeleton's position on the screen
            NuiTransformSkeletonToDepthImage( skel.SkeletonData[i].Position, &x, &y, &depth );

            if ( depth < nearestDepths[0] )
            {
                nearestDepths[1] = nearestDepths[0];
                nearestIDs[1] = nearestIDs[0];

                nearestDepths[0] = depth;
                nearestIDs[0] = skel.SkeletonData[i].dwTrackingID;
            }
            else if ( depth < nearestDepths[1] )
            {
                nearestDepths[1] = depth;
                nearestIDs[1] = skel.SkeletonData[i].dwTrackingID;
            }
        }
    }

    if ( SV_TRACKED_SKELETONS_NEAREST1 == m_TrackedSkeletons || SV_TRACKED_SKELETONS_NEAREST2 == m_TrackedSkeletons )
    {
        // Only track the closest single skeleton in nearest 1 mode
        if ( SV_TRACKED_SKELETONS_NEAREST1 == m_TrackedSkeletons )
        {
            nearestIDs[1] = 0;
        }
        m_pNuiSensor->NuiSkeletonSetTrackedSkeletons(nearestIDs);
    }

    if ( SV_TRACKED_SKELETONS_STICKY1 == m_TrackedSkeletons || SV_TRACKED_SKELETONS_STICKY2 == m_TrackedSkeletons )
    {
        DWORD stickyIDs[2] = { m_StickySkeletonIds[0], m_StickySkeletonIds[1] };

        // Only track a single skeleton in sticky 1 mode
        if ( SV_TRACKED_SKELETONS_STICKY1 == m_TrackedSkeletons )
        {
            stickyIDs[1] = 0;
        }
        m_pNuiSensor->NuiSkeletonSetTrackedSkeletons(stickyIDs);
    }
}

/// <summary>
/// Converts a skeleton point to screen space
/// </summary>
/// <param name="skeletonPoint">skeleton point to tranform</param>
/// <param name="width">width (in pixels) of output buffer</param>
/// <param name="height">height (in pixels) of output buffer</param>
/// <returns>point in screen-space</returns>
D2D1_POINT_2F CKinectShopApp::SkeletonToScreen( Vector4 skeletonPoint, int width, int height )
{
    LONG x, y;
    USHORT depth;

    // calculate the skeleton's position on the screen
    NuiTransformSkeletonToDepthImage( skeletonPoint, &x, &y, &depth );

    float screenPointX = static_cast<float>(x * width) / g_ScreenWidth;
    float screenPointY = static_cast<float>(y * height) / g_ScreenHeight;

    return D2D1::Point2F(screenPointX, screenPointY);
}

/// <summary>
/// Handle new skeleton data
/// </summary>
bool CKinectShopApp::Nui_GotSkeletonAlert( )
{
    NUI_SKELETON_FRAME SkeletonFrame = {0};

    bool foundSkeleton = false;

    if ( SUCCEEDED(m_pNuiSensor->NuiSkeletonGetNextFrame( 0, &SkeletonFrame )) )
    {
        for ( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
        {
            NUI_SKELETON_TRACKING_STATE trackingState = SkeletonFrame.SkeletonData[i].eTrackingState;

            if ( trackingState == NUI_SKELETON_TRACKED || trackingState == NUI_SKELETON_POSITION_ONLY )
            {
                foundSkeleton = true;
            }
        }
    }

    // no skeletons!
    if( !foundSkeleton )
    {
        return true;
    }

    // smooth out the skeleton data
    HRESULT hr = m_pNuiSensor->NuiTransformSmooth(&SkeletonFrame,NULL);
    if ( FAILED(hr) )
    {
        return false;
    }

    // we found a skeleton, re-start the skeletal timer
    m_bScreenBlanked = false;
    m_LastSkeletonFoundTime = timeGetTime( );

    // Endure Direct2D is ready to draw
    hr = EnsureDirect2DResources( );
    if ( FAILED( hr ) )
    {
        return false;
    }

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear( );
    
    RECT rct;
    GetClientRect( GetDlgItem( m_hWnd, IDC_SKELETALVIEW ), &rct);
    int width = rct.right;
    int height = rct.bottom;

	NUI_SKELETON_DATA *pSkel;

	for ( int i = 0 ; i < NUI_SKELETON_COUNT; i++ )
    {
		pSkel = &(SkeletonFrame.SkeletonData[i]);
        NUI_SKELETON_TRACKING_STATE trackingState = pSkel->eTrackingState;

        if ( trackingState == NUI_SKELETON_TRACKED )
        {
            // We're tracking the skeleton, draw it
            Nui_DrawSkeleton( *pSkel, width, height );
        }
        else if ( trackingState == NUI_SKELETON_POSITION_ONLY )
        {
            // we've only received the center point of the skeleton, draw that
            D2D1_ELLIPSE ellipse = D2D1::Ellipse(
                SkeletonToScreen( pSkel->Position, width, height ),
                g_JointThickness,
                g_JointThickness
                );

            m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointTracked);
        }

		if (
			pSkel->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_LEFT] != NUI_SKELETON_POSITION_NOT_TRACKED &&
			pSkel->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_RIGHT] != NUI_SKELETON_POSITION_NOT_TRACKED
			) {

				cv::Mat colorImage;
				cv::Mat depthImage;
				WaitForSingleObject(m_colorImageMutex, INFINITE);
				m_colorImage.copyTo(colorImage);
				m_depthImage.copyTo(depthImage);
				ReleaseMutex(m_colorImageMutex);

				m_pLocator->locate(pSkel, &colorImage, &depthImage);
		}
    }

    hr = m_pRenderTarget->EndDraw();

    UpdateTrackedSkeletons( SkeletonFrame );

    // Device lost, need to recreate the render target
    // We'll dispose it now and retry drawing
    if ( hr == D2DERR_RECREATE_TARGET )
    {
        hr = S_OK;
        DiscardDirect2DResources();
        return false;
    }

    return true;
}

/// <summary>
/// Invoked when the user changes the selection of tracked skeletons
/// </summary>
/// <param name="mode">skelton tracking mode to switch to</param>
void CKinectShopApp::UpdateTrackedSkeletonSelection( int mode )
{
    m_TrackedSkeletons = mode;

    UpdateSkeletonTrackingFlag(
        NUI_SKELETON_TRACKING_FLAG_TITLE_SETS_TRACKED_SKELETONS,
        (mode != SV_TRACKED_SKELETONS_DEFAULT));
}

/// <summary>
/// Invoked when the user changes the tracking mode
/// </summary>
/// <param name="mode">tracking mode to switch to</param>
void CKinectShopApp::UpdateTrackingMode( int mode )
{
    UpdateSkeletonTrackingFlag(
        NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT,
        (mode == SV_TRACKING_MODE_SEATED) );
}

/// <summary>
/// Invoked when the user changes the range
/// </summary>
/// <param name="mode">range to switch to</param>
void CKinectShopApp::UpdateRange( int mode )
{
    UpdateDepthStreamFlag(
        NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE,
        (mode != SV_RANGE_DEFAULT) );
}

/// <summary>
/// Sets or clears the specified skeleton tracking flag
/// </summary>
/// <param name="flag">flag to set or clear</param>
/// <param name="value">true to set, false to clear</param>
void CKinectShopApp::UpdateSkeletonTrackingFlag( DWORD flag, bool value )
{
    DWORD newFlags = m_SkeletonTrackingFlags;

    if (value)
    {
        newFlags |= flag;
    }
    else
    {
        newFlags &= ~flag;
    }

    if (NULL != m_pNuiSensor && newFlags != m_SkeletonTrackingFlags)
    {
        if ( !HasSkeletalEngine(m_pNuiSensor) )
        {
            MessageBoxResource(IDS_ERROR_SKELETONTRACKING, MB_OK | MB_ICONHAND);
        }

        m_SkeletonTrackingFlags = newFlags;

        HRESULT hr = m_pNuiSensor->NuiSkeletonTrackingEnable( m_hNextSkeletonEvent, m_SkeletonTrackingFlags );

        if ( FAILED( hr ) )
        {
            MessageBoxResource(IDS_ERROR_SKELETONTRACKING, MB_OK | MB_ICONHAND);
        }
    }
}

/// <summary>
/// Sets or clears the specified depth stream flag
/// </summary>
/// <param name="flag">flag to set or clear</param>
/// <param name="value">true to set, false to clear</param>
void CKinectShopApp::UpdateDepthStreamFlag( DWORD flag, bool value )
{
    DWORD newFlags = m_DepthStreamFlags;

    if (value)
    {
        newFlags |= flag;
    }
    else
    {
        newFlags &= ~flag;
    }

    if (NULL != m_pNuiSensor && newFlags != m_DepthStreamFlags)
    {
        m_DepthStreamFlags = newFlags;
        m_pNuiSensor->NuiImageStreamSetImageFrameFlags( m_pDepthStreamHandle, m_DepthStreamFlags );
    }
}

/// <summary>
/// Ensure necessary Direct2d resources are created
/// </summary>
/// <returns>S_OK if successful, otherwise an error code</returns>
HRESULT CKinectShopApp::EnsureDirect2DResources()
{
    HRESULT hr = S_OK;

    if ( !m_pRenderTarget )
    {
        RECT rc;
        GetWindowRect( GetDlgItem( m_hWnd, IDC_SKELETALVIEW ), &rc );  
    
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;
        D2D1_SIZE_U size = D2D1::SizeU( width, height );
        D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
        rtProps.pixelFormat = D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
        rtProps.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

        // Create a Hwnd render target, in order to render to the window set in initialize
        hr = m_pD2DFactory->CreateHwndRenderTarget(
            rtProps,
            D2D1::HwndRenderTargetProperties(GetDlgItem( m_hWnd, IDC_SKELETALVIEW), size),
            &m_pRenderTarget
            );
        if ( FAILED( hr ) )
        {
            MessageBoxResource( IDS_ERROR_DRAWDEVICE, MB_OK | MB_ICONHAND );
            return E_FAIL;
        }

        //light green
        m_pRenderTarget->CreateSolidColorBrush( D2D1::ColorF( 68, 192, 68 ), &m_pBrushJointTracked );

        //yellow
        m_pRenderTarget->CreateSolidColorBrush( D2D1::ColorF( 255, 255, 0 ), &m_pBrushJointInferred );

        //green
        m_pRenderTarget->CreateSolidColorBrush( D2D1::ColorF( 0, 128, 0 ), &m_pBrushBoneTracked );

        //gray
        m_pRenderTarget->CreateSolidColorBrush( D2D1::ColorF( 128, 128, 128 ), &m_pBrushBoneInferred );
    }

    return hr;
}

/// <summary>
/// Dispose Direct2d resources 
/// </summary>
void CKinectShopApp::DiscardDirect2DResources( )
{
    SafeRelease(m_pRenderTarget);

    SafeRelease( m_pBrushJointTracked );
    SafeRelease( m_pBrushJointInferred );
    SafeRelease( m_pBrushBoneTracked );
    SafeRelease( m_pBrushBoneInferred );
}

/// <summary>
/// Initialize Kinect audio stream object.
/// </summary>
/// <returns>
/// <para>S_OK on success, otherwise failure code.</para>
/// </returns>
HRESULT CKinectShopApp::InitializeAudioStream()
{
    INuiAudioBeam*      pNuiAudioSource = NULL;
    IMediaObject*       pDMO = NULL;
    IPropertyStore*     pPropertyStore = NULL;
    IStream*            pStream = NULL;

    // Get the audio source
    HRESULT hr = m_pNuiSensor->NuiGetAudioSource(&pNuiAudioSource);
    if (SUCCEEDED(hr))
    {
        hr = pNuiAudioSource->QueryInterface(IID_IMediaObject, (void**)&pDMO);

        if (SUCCEEDED(hr))
        {
            hr = pNuiAudioSource->QueryInterface(IID_IPropertyStore, (void**)&pPropertyStore);
    
            // Set AEC-MicArray DMO system mode. This must be set for the DMO to work properly.
            // Possible values are:
            //   SINGLE_CHANNEL_AEC = 0
            //   OPTIBEAM_ARRAY_ONLY = 2
            //   OPTIBEAM_ARRAY_AND_AEC = 4
            //   SINGLE_CHANNEL_NSAGC = 5
            PROPVARIANT pvSysMode;
            PropVariantInit(&pvSysMode);
            pvSysMode.vt = VT_I4;
            pvSysMode.lVal = (LONG)(2); // Use OPTIBEAM_ARRAY_ONLY setting. Set OPTIBEAM_ARRAY_AND_AEC instead if you expect to have sound playing from speakers.
            pPropertyStore->SetValue(MFPKEY_WMAAECMA_SYSTEM_MODE, pvSysMode);
            PropVariantClear(&pvSysMode);

            // Set DMO output format
            WAVEFORMATEX wfxOut = {AudioFormat, AudioChannels, AudioSamplesPerSecond, AudioAverageBytesPerSecond, AudioBlockAlign, AudioBitsPerSample, 0};
            DMO_MEDIA_TYPE mt = {0};
            MoInitMediaType(&mt, sizeof(WAVEFORMATEX));
    
            mt.majortype = MEDIATYPE_Audio;
            mt.subtype = MEDIASUBTYPE_PCM;
            mt.lSampleSize = 0;
            mt.bFixedSizeSamples = TRUE;
            mt.bTemporalCompression = FALSE;
            mt.formattype = FORMAT_WaveFormatEx;	
            memcpy(mt.pbFormat, &wfxOut, sizeof(WAVEFORMATEX));
    
            hr = pDMO->SetOutputType(0, &mt, 0);

            if (SUCCEEDED(hr))
            {
                m_pKinectAudioStream = new KinectAudioStream(pDMO);

                hr = m_pKinectAudioStream->QueryInterface(IID_IStream, (void**)&pStream);

                if (SUCCEEDED(hr))
                {
                    hr = CoCreateInstance(CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, __uuidof(ISpStream), (void**)&m_pSpeechStream);

                    if (SUCCEEDED(hr))
                    {
                        hr = m_pSpeechStream->SetBaseStream(pStream, SPDFID_WaveFormatEx, &wfxOut);
                    }
                }
            }

            MoFreeMediaType(&mt);
        }
    }

    SafeRelease(pStream);
    SafeRelease(pPropertyStore);
    SafeRelease(pDMO);
    SafeRelease(pNuiAudioSource);

    return hr;
}

/// <summary>
/// Create speech recognizer that will read Kinect audio stream data.
/// </summary>
/// <returns>
/// <para>S_OK on success, otherwise failure code.</para>
/// </returns>
HRESULT CKinectShopApp::CreateSpeechRecognizer()
{
    ISpObjectToken *pEngineToken = NULL;
    
    HRESULT hr = CoCreateInstance(CLSID_SpInprocRecognizer, NULL, CLSCTX_INPROC_SERVER, __uuidof(ISpRecognizer), (void**)&m_pSpeechRecognizer);

    if (SUCCEEDED(hr))
    {
        m_pSpeechRecognizer->SetInput(m_pSpeechStream, FALSE);
        hr = SpFindBestToken(SPCAT_RECOGNIZERS,L"Language=409;Kinect=True",NULL,&pEngineToken);

        if (SUCCEEDED(hr))
        {
            m_pSpeechRecognizer->SetRecognizer(pEngineToken);
            hr = m_pSpeechRecognizer->CreateRecoContext(&m_pSpeechContext);
        }
    }

    SafeRelease(pEngineToken);

    return hr;
}

/// <summary>
/// Load speech recognition grammar into recognizer.
/// </summary>
/// <returns>
/// <para>S_OK on success, otherwise failure code.</para>
/// </returns>
HRESULT CKinectShopApp::LoadSpeechGrammar()
{
    HRESULT hr = m_pSpeechContext->CreateGrammar(1, &m_pSpeechGrammar);

    if (SUCCEEDED(hr))
    {
        // Populate recognition grammar from file
        hr = m_pSpeechGrammar->LoadCmdFromFile(GrammarFileName, SPLO_DYNAMIC);
    }

    return hr;
}

/// <summary>
/// Start recognizing speech asynchronously.
/// </summary>
/// <returns>
/// <para>S_OK on success, otherwise failure code.</para>
/// </returns>
HRESULT CKinectShopApp::StartSpeechRecognition()
{
    HRESULT hr = m_pKinectAudioStream->StartCapture();

    if (SUCCEEDED(hr))
    {
        // Specify that all top level rules in grammar are now active
        m_pSpeechGrammar->SetRuleState(NULL, NULL, SPRS_ACTIVE);

        // Specify that engine should always be reading audio
        m_pSpeechRecognizer->SetRecoState(SPRST_ACTIVE_ALWAYS);

        // Specify that we're only interested in receiving recognition events
        m_pSpeechContext->SetInterest(SPFEI(SPEI_RECOGNITION), SPFEI(SPEI_RECOGNITION));

        // Ensure that engine is recognizing speech and not in paused state
        hr = m_pSpeechContext->Resume(0);
        if (SUCCEEDED(hr))
        {
            m_hSpeechEvent = m_pSpeechContext->GetNotifyEventHandle();
        }
    }
        
    return hr;
}

/// <summary>
/// Process recently triggered speech recognition events.
/// </summary>
void CKinectShopApp::ProcessSpeech()
{
    const float ConfidenceThreshold = 0.3f;

    SPEVENT curEvent;
    ULONG fetched = 0;
    HRESULT hr = S_OK;

    m_pSpeechContext->GetEvents(1, &curEvent, &fetched);

    while (fetched > 0)
    {
        switch (curEvent.eEventId)
        {
            case SPEI_RECOGNITION:
                if (SPET_LPARAM_IS_OBJECT == curEvent.elParamType)
                {
                    // this is an ISpRecoResult
                    ISpRecoResult* result = reinterpret_cast<ISpRecoResult*>(curEvent.lParam);
                    SPPHRASE* pPhrase = NULL;
                    
                    hr = result->GetPhrase(&pPhrase);
                    if (SUCCEEDED(hr))
                    {
                        if ((pPhrase->pProperties != NULL) && (pPhrase->pProperties->pFirstChild != NULL))
                        {
                            const SPPHRASEPROPERTY* pSemanticTag = pPhrase->pProperties->pFirstChild;
                            if (pSemanticTag->SREngineConfidence > ConfidenceThreshold)
                            {
								for(int i = 0; i < m_speechCallbacks.size(); i++)
								{
									(*m_speechCallbacks[i].second)(m_speechCallbacks[i].first, pSemanticTag);
								}
                            }
                        }
                        ::CoTaskMemFree(pPhrase);
                    }
                }
                break;
        }

        m_pSpeechContext->GetEvents(1, &curEvent, &fetched);
    }

    return;
}

void CKinectShopApp::registerSpeechCallback(void* classPointer, void (*callbackFct)(void*, const SPPHRASEPROPERTY*))
{
	
	m_speechCallbacks.push_back(std::pair<void*, void (*)(void*, const SPPHRASEPROPERTY*)>(classPointer, callbackFct));
}