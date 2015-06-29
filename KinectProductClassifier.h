#pragma once

// database
#include <sql.h>
#include <sqlext.h>
#include <opencv2/core/core.hpp>

#include "KinectShop.h"
#include "KinectProductBooker.h"
#include "Suessigkeit.h"

class CKinectProductClassifier
{
public:
	CKinectProductClassifier(CKinectShopApp *app);
	~CKinectProductClassifier();
	bool compareMatHist(Mat src, MatND ref, int compare_method = CV_COMP_CORREL);
	Suessigkeit* customsurfdetector(vector<Suessigkeit*> &sortiment, Mat &img_scene, double minFlaeche = 300);
	void classify(cv::Mat *colorImage);

private:
	CKinectShopApp *m_app;
	CKinectProductBooker *m_pBooker;

	// Database connection
	SQLHENV                                 m_henv;
	SQLHDBC                                 m_hdbc;
};
