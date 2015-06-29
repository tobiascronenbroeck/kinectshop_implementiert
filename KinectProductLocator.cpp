#include "stdafx.h"

#include <algorithm>
#include <opencv2\opencv.hpp>

#include "KinectProductLocator.h"

using namespace std;
#define DEPTH_THRESHOLD 100

CKinectProductLocator::CKinectProductLocator(CKinectShopApp *app)
{
	m_app = app;
	m_pClassifier = new CKinectProductClassifier(app);
}

CKinectProductLocator::~CKinectProductLocator()
{
	m_app = NULL;
	delete m_pClassifier;
	m_pClassifier = NULL;
}
    /*  CKinectProductLocator::locate
     *  -------------------------------------------------------------------------------------------------
     *  Task:
     *
     *      - Locate a product that is being holded in front of the camera.
     *      - Create a new image containing only the product
     *
     *  -------------------------------------------------------------------------------------------------
     *  Received parameters:
     *
     *      - NUI_SKELETON_DATA
     *      - Color image
     *      - Depth image
     *
     *  -------------------------------------------------------------------------------------------------
     *  Summary:
     *
     *      - Start from the head
	 *		- Iterate downwards and find product
	 *		- Iterate to the sides and find borders
	 *		- Export image to parent directory
	 *		- Call classifier functions
     */
void CKinectProductLocator::locate(NUI_SKELETON_DATA *pSkel, cv::Mat *colorImage, cv::Mat *depthImage)
{
    /*  VARIABLE DECLARATION
     *
     *      All required variables are declared in this section
     *
     */

    POINT tempPoint;                    //    Temporarily stores the position of the requested skeleton node
    USHORT tempDepth;                   //    Temporarily stores the depth of the requested skeleton node
    POINT ColorPositionOfHand[2];       //    Stores the color coordinates for both hands
    POINT DepthPositionOfHand[2];       //    Stores the depth coordinates for both hands
    USHORT DepthOfHand[2];              //    Stores the depth information for both hands
    POINT DepthPositionOfShoulder[2];   //    Stores the depth information for both shoulders
    POINT DepthPositionOfHead;          //    Stores the depth coordinates for the head
    USHORT DepthOfHead;                 //    Stores the depth information for the head
    POINT ColorPositionOfHead;          //    (VISUAL OUTPUT ONLY) Stores the color coordinates for the head
    POINT DepthPositionOfCenterOfHands; //    (VISUAL OUTPUT ONLY) Stores the depth coordinates for the mid-way position between both hands
    RECT ColorLocation;                 //    Stores the location of the product in the color image
    RECT DepthLocation;                 //    Stores the location of the product in the depth image
    cv::Mat thresholdImage;             //    Stores the thresholded image
    cv::Mat finalImage;
    cv::Rect finalRectangle;
    int height;                         //    Stores the final image height
    int i, j;                           //    Loop variables

    /*  FETCH SKELETON INFORMATION
     *
     *      Fetches the position for:
     *              - Left hand
     *              - Right hand
     *              - Head
     *              - Left shoulder
     *              - Right shoulder
     *
     *      and stores both color and depth coordintes, as well as the depth information at each skeleton node.
     *
     */

    // Left hand
    NuiTransformSkeletonToDepthImage(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT], &tempPoint.x, &tempPoint.y, &tempDepth, DEPTH_IMAGE_RESOLUTION);
    NuiImageGetColorPixelCoordinatesFromDepthPixel(COLOR_IMAGE_RESOLUTION, NULL, tempPoint.x, tempPoint.y, tempDepth, &ColorPositionOfHand[0].x, &ColorPositionOfHand[0].y);
    DepthPositionOfHand[0].x = tempPoint.x;
    DepthPositionOfHand[0].y = tempPoint.y;
    DepthOfHand[0] = tempDepth;

    // Right hand
    NuiTransformSkeletonToDepthImage(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT], &tempPoint.x, &tempPoint.y, &tempDepth, DEPTH_IMAGE_RESOLUTION);
    NuiImageGetColorPixelCoordinatesFromDepthPixel(COLOR_IMAGE_RESOLUTION, NULL, tempPoint.x, tempPoint.y, tempDepth, &ColorPositionOfHand[1].x, &ColorPositionOfHand[1].y);
    DepthPositionOfHand[1].x = tempPoint.x;
    DepthPositionOfHand[1].y = tempPoint.y;
    DepthOfHand[1] = tempDepth;

    // Left shoulder
    NuiTransformSkeletonToDepthImage(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT], &tempPoint.x, &tempPoint.y, &tempDepth, DEPTH_IMAGE_RESOLUTION);
    DepthPositionOfShoulder[0].x = tempPoint.x;
    DepthPositionOfShoulder[0].y = tempPoint.y;
	
    // Right shoulder
    NuiTransformSkeletonToDepthImage(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT], &tempPoint.x, &tempPoint.y, &tempDepth, DEPTH_IMAGE_RESOLUTION);
    DepthPositionOfShoulder[1].x = tempPoint.x;
    DepthPositionOfShoulder[1].y = tempPoint.y;

    // Head
    NuiTransformSkeletonToDepthImage(pSkel->SkeletonPositions[NUI_SKELETON_POSITION_HEAD], &tempPoint.x, &tempPoint.y, &tempDepth, DEPTH_IMAGE_RESOLUTION);
    NuiImageGetColorPixelCoordinatesFromDepthPixel(COLOR_IMAGE_RESOLUTION, NULL, tempPoint.x, tempPoint.y, tempDepth, &ColorPositionOfHead.x, &ColorPositionOfHead.y);
    DepthPositionOfHead.x = tempPoint.y;
    DepthPositionOfHead.y = tempPoint.x;
    DepthOfHead = tempDepth;

	// calculate mid-hand position in depth image
    DepthPositionOfCenterOfHands.x = (DepthPositionOfHand[0].x+DepthPositionOfHand[1].x)/2;
    DepthPositionOfCenterOfHands.y = (DepthPositionOfHand[0].y+DepthPositionOfHand[1].y)/2;

    /*  THRESHOLD FILTER
     *
     *      Iterates through depthImage and applies the threshold filter on thresholdImage.
     *      The threshold value is calculated dynamically depending on the depth of the head (and therefore the whole body).
     *
     */
    depthImage->copyTo(thresholdImage);
	for (int i = 10; i < depthImage->rows-10; ++i) {
		for (int j = 10; j < depthImage->cols-10; ++j) {
            if(depthImage->at<USHORT>(i,j) > DepthOfHead-3500)
            {
					thresholdImage.at<USHORT>(i,j) = 0;
			}
			else
			{
				thresholdImage.at<USHORT>(i,j) = 20000;
			}
		}
	}

    /*  PRODUCT LOCATION
     *
     *      In the thresholded image, start at the head position and iterate downwards.
     *          - Value changes ->  top border of the product is found
     *      Continue iterating downwards
     *          - Value changes ->  bottom border of the product is found
     *
     *      At mid-height position on the product, iterate left:
     *          - Value changes ->  left border of the product is found
     *      Iterate to the right,
     *          - Value changes ->  right border of the product if found
     *
     */

    // Select start position (head) - Never start outside the depthImage range
    if(DepthPositionOfHead.x>239)
	{
        i = 239;
	}
	else
	{
        i = abs(DepthPositionOfHead.x);
	}
    if(abs(DepthPositionOfHead.y)>319)
	{
		j = 319;	
	}
	else
	{
        j = abs(DepthPositionOfHead.y);
	}

    // Iterate downwards until the value changes (or border of image)
	while(i < 239 && i>= 0 && thresholdImage.at<USHORT>(i, j) == 0)
	{
        thresholdImage.at<USHORT>(i, j) = 50000;// VISUAL OUTPUT ONLY
		i++;
	}
	height = i;
    DepthLocation.top = i+(240-i)*0.06;         // Camera offset calculation for depth/color

    // Continue iterating downwards until the value changes again (or border of image)
	while(i<239 && i>= 0 && thresholdImage.at<USHORT>(i, j) != 0)
	{
        thresholdImage.at<USHORT>(i, j) = 2000; // VISUAL OUTPUT ONLY
		i++;
	}
	height -= i;
	height *= -1;

    DepthLocation.bottom = i+(240-i)*0.06;      // Camera offset calculation for depth/color
    thresholdImage.at<USHORT>(i, j) = 50000;    // VISUAL OUTPUT ONLY

	i -= height/2;


    // Iterate to the left until depth value changes (or border of image)
    while(j < 319 && j>= 0 && thresholdImage.at<USHORT>(i, j) != 0 && j>DepthPositionOfShoulder[0].x)
	{
		thresholdImage.at<USHORT>(i, j) = 2000;
		j--;
	}
	thresholdImage.at<USHORT>(i, j) = 50000;
    DepthLocation.left = j;


    // Iterate to the right until depth value changes (or border of image)
    j = abs(DepthPositionOfHead.y);
    while(j < 319 && j> 0 && thresholdImage.at<USHORT>(i, j) != 0 && j<DepthPositionOfShoulder[1].x)
	{
		thresholdImage.at<USHORT>(i, j) = 2000;
		j++;
	}
	thresholdImage.at<USHORT>(i, j) = 50000;
    DepthLocation.right = j;

    /*  VISUAL OUTPUT
     *
     *      Colorizes certain parameters in the color image for developing purposes
     *
     *      DO NOT USE FOR NORMAL PURPOSES, THE FINAL IMAGE WILL BE AFFECTED
     */
	/*
	for (int i = 0; i < colorImage->rows; ++i) {
		for (int j = 0; j < colorImage->cols; ++j) {
			cv::Vec4b& rgba = colorImage->at<cv::Vec4b>(i, j);

			// Bright box
            if((i<DepthLocation.top*4 || i>DepthLocation.bottom*4) || (j<DepthLocation.left*4 || j>DepthLocation.right*4))
			{
				rgba[0] *= 0.4;
				rgba[1] *= 0.4;
				rgba[2] *= 0.4;
			}
			else
			{rgba[3] = 0;}

			// Green cross for center hands position
            if(j==DepthPositionOfCenterOfHands.x*4)
			{
				rgba[1] = UCHAR_MAX;
			}
            if(i==DepthPositionOfCenterOfHands.y*4)
			{
				rgba[1] = UCHAR_MAX;
			}

			// Blue cross for head
            if(j==ColorPositionOfHead.x)
			{
				rgba[0] = UCHAR_MAX;
			}
            if(i==ColorPositionOfHead.y)
			{
				rgba[0] = UCHAR_MAX;
			}

			// Red crosses for hand positions
            if(i==ColorPositionOfHand[0].y || i==ColorPositionOfHand[1].y)
			{
				rgba[2] = UCHAR_MAX;
			}
            if(j==ColorPositionOfHand[0].x || j==ColorPositionOfHand[1].x)
			{
				rgba[2] = UCHAR_MAX;
            }
		}
    }
	*/
    /*  EXPORT FINAL IMAGE
     *
     *      Transforms the located product in the depth image to color coordinates and creates the final image, later used by the classifier
     *
     */

    // Calculate the product location in the color image form the product location in the depth image
    ColorLocation.bottom = DepthLocation.bottom*4;
    ColorLocation.top = DepthLocation.top*4;
    ColorLocation.left = DepthLocation.left*4;
    ColorLocation.right = DepthLocation.right*4;

    // Calculate the rectangle that will be exported to the final image
    finalRectangle.x = ColorLocation.left+40;
    finalRectangle.y = ColorLocation.top;
    finalRectangle.width = abs(ColorLocation.right-ColorLocation.left-40);
    finalRectangle.height = ColorLocation.bottom-ColorLocation.top;

    // Export rentangle to final image
	finalImage = colorImage->clone();
	finalImage = finalImage(finalRectangle).clone();
	//cv::imshow("thresholdImage", thresholdImage);
    cv::imwrite("finalImage.png", finalImage);

	//Call only if Size of Picture is big enough
	if(finalRectangle.width*finalRectangle.height > 800)
		{
		// Call classifier function, which will use the allocated finalImage.png from the parent directory
		m_pClassifier->classify(&finalImage);
		}
}


// TODO:
/*
 */
