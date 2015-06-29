#pragma once

#include "KinectShop.h"
#include "KinectProductClassifier.h"

class CKinectProductLocator
{
public:
	CKinectProductLocator(CKinectShopApp *app);
	~CKinectProductLocator();

	void locate(NUI_SKELETON_DATA *pSkel, cv::Mat *colorImage, cv::Mat *depthImage);
private:
	CKinectShopApp *m_app;
	CKinectProductClassifier *m_pClassifier;
};
