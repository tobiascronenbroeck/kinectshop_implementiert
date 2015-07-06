#pragma once

// database
#include <sql.h>
#include <sqlext.h>
#include <opencv2/core/core.hpp>

#include "KinectShop.h"
#include "KinectProductBooker.h"
#include "Suessigkeit.h"


/**
 * Represents a Classifier that can detect sweet packagings based on their aspect ratios,
 * their color-histogram and finally a surf detection algorithm.
 * Picture has to be cut out sufficiently precise.
 */
class CKinectProductClassifier
{
public:
	CKinectProductClassifier(CKinectShopApp *app);
	~CKinectProductClassifier();
	bool compareMatHist(Mat src, MatND ref, int compare_method = CV_COMP_CORREL);
	Suessigkeit* customsurfdetector(vector<Suessigkeit*> &sortiment, Mat &img_scene, double minFlaeche = 300);
	void classify(cv::Mat *colorImage);
	bool compareRatio(Mat source, float fRefRatio);

private:
	CKinectShopApp *m_app;
	CKinectProductBooker *m_pBooker;

	// Database connection
	SQLHENV                                 m_henv;
	SQLHDBC                                 m_hdbc;
};
