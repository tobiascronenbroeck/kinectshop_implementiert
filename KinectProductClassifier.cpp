#include "stdafx.h"
#include "math.h"
#include "Sortiment.h"
#include "Suessigkeit.h"
#include "KinectProductClassifier.h"
#include "stdio.h"

#include <fstream>
#include <algorithm>
#include <iostream>

#define _CRT_SECURE_NO_DEPRECATE
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/nonfree/nonfree.hpp"
#include "opencv2/flann/flann.hpp"
#include "opencv2/objdetect/objdetect.hpp"

#define tresholdcamerafailure 50 // Wird zur beschleunigung und Stabilitaet benötigt! Standart 250
#define tresholdgoodcamerakeypoints 10 // Standart 10
#define minHessian 350           // Schwellwert fuer Surf Detector!

int minHessian_swap = minHessian; // Fuer andere Klassen!

using namespace std;
using namespace cv;

Sortiment *S = new Sortiment();

CKinectProductClassifier::CKinectProductClassifier(CKinectShopApp *app)
{
	m_app = app;
	m_pBooker = new CKinectProductBooker(app);

}

CKinectProductClassifier::~CKinectProductClassifier()
{
	m_app = NULL;
	delete m_pBooker;
	m_pBooker = NULL;
}

Suessigkeit* CKinectProductClassifier::customsurfdetector(vector<Suessigkeit*> &sortiment, Mat &img_scene, double minFlaeche)
{
	
	if (!(sortiment.empty())){
        
			//SURF Keypoints fuer das Kamerabild wird berechnet!
			SurfFeatureDetector detector(minHessian);
			vector<KeyPoint> keypoints_scene;
            detector.detect(img_scene, keypoints_scene);

			/* Prueft ob Vergleich Sinn macht! ->
			falls zu wenige Keypoints gefunden werden wird hier schon NULL zurueckgegeben.*/
			if (keypoints_scene.size() < tresholdcamerafailure)  { return new Suessigkeit(); }

			/*Wenn Vergleich sinn macht werden die beschreibenden Merkmale des Kamerabildes erzeugt!
			Diese werden in der Matrix descriptors_scene gespeichert!*/
			SurfDescriptorExtractor extractor;
			Mat descriptors_scene;
			extractor.compute(img_scene, keypoints_scene, descriptors_scene); // Anwendung des SurfExtractors

            vector<Suessigkeit*>::iterator iter;
		    for (iter = sortiment.begin(); iter != sortiment.end(); iter++){
            //FLANNBasedMatcher
			FlannBasedMatcher matcher;
			vector< DMatch > matches;
			matcher.match((*iter)->referencedescriptors, descriptors_scene, matches);

			double max_dist=0;  double min_dist=150;
			//-- Quick calculation of max and min distances between keypoints
			for (int i = 0; i < ((*iter)->referencedescriptors).rows; i++)
			{
				double dist = matches[i].distance;
				if (dist < min_dist) min_dist = dist;
				if (dist > max_dist) max_dist = dist;
			}
            vector< DMatch > good_matches;

			//-- Draw only "good" matches (i.e. whose distance is less than 3*min_dist )
			for (int i = 0; i < ((*iter)->referencedescriptors).rows; i++)
			{
				if (matches[i].distance < 3 * min_dist)
				{
					good_matches.push_back(matches[i]);
				}
			}
            Mat img_matches;
			drawMatches((*iter)->GrayScaleImage, (*iter)->referencekeypoints, img_scene, keypoints_scene,
				good_matches, img_matches, Scalar::all(-1), Scalar::all(-1),
				vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
            
			vector<Point2f> obj;  vector<Point2f> scene;
			for (int i = 0; i < good_matches.size(); i++)
			{
				//-- Get the keypoints from the good matches
				obj.push_back(((*iter)->referencekeypoints)[good_matches[i].queryIdx].pt);
				scene.push_back(keypoints_scene[good_matches[i].trainIdx].pt);
			}
			
			

			// Es wird geprueft ob genuegend gute Keypoints vorhanden sind. (Performance und Stabilitaet)
			if (scene.size() < tresholdgoodcamerakeypoints){ break; }

			Mat H = findHomography(obj, scene, CV_RANSAC);
			
			std::vector<Point2f> obj_corners(4); 
			obj_corners[0] = cvPoint(0, 0); obj_corners[1] = cvPoint(((*iter)->GrayScaleImage).cols, 0);
			obj_corners[2] = cvPoint(((*iter)->GrayScaleImage).cols, ((*iter)->GrayScaleImage).rows); obj_corners[3] = cvPoint(0, ((*iter)->GrayScaleImage).rows);
			std::vector<Point2f> scene_corners(4);

			perspectiveTransform(obj_corners, scene_corners, H);

			Point2f aufpunkt = scene_corners[0];
			Point2f xEndpunkt = scene_corners[1];
			Point2f yEndpunkt = scene_corners[3];
			Point2f opposite = scene_corners[2];


			Point2f dist1 = xEndpunkt - aufpunkt;
			Point2f dist2 = yEndpunkt - aufpunkt;
			Point2f checkpoint1 = aufpunkt + dist1 + dist2;
			// 1. Qualifizierungsschritt
			double distx = sqrt((dist1.x)*(dist1.x) + (dist1.y*dist1.y));
			double disty = sqrt((dist2.x)*(dist2.x) + (dist2.y*dist2.y));
			// 2. Qualifizierungsschritt
			double area = fabs((dist1.x * dist2.x) + (dist1.y * dist2.y));
            // 3. Qualifizierungsschritt
			//double diag = sqrt((distx*distx) + (disty*disty)); // Laenge des Diagonalvektors, berechnet aus den 2 Stuetzvektoren
			//double diagref = sqrt(((checkpoint1.x) - (aufpunkt.x))*((checkpoint1.x) - (aufpunkt.x)) + ((checkpoint1.y) - (aufpunkt.y))*((checkpoint1.y) - (aufpunkt.y)));
			//if (fabs((diagref / diag) - 1) >= 0.2){ cout << "dist Qualifizierung 3 : " << fabs((diagref / diag) - 1) << endl; break; }  // Falls Perspektive zu deformiert ist, wird Berechnung abgebrochen!
			if ((distx > 60) && (disty > 60))
			{
				if ((area >= minFlaeche) && (fabs((checkpoint1.x) - (opposite.x)) < 30 && (fabs((checkpoint1.y) - (opposite.y)) < 30)))
				{   
			        //-- Draw lines between the corners (the mapped object in the scene - image_2 )
					line(img_matches, scene_corners[0] + Point2f(((*iter)->GrayScaleImage).cols, 0), scene_corners[1] + Point2f(((*iter)->GrayScaleImage).cols, 0), Scalar(0, 255, 0), 4);
					line(img_matches, scene_corners[1] + Point2f(((*iter)->GrayScaleImage).cols, 0), scene_corners[2] + Point2f(((*iter)->GrayScaleImage).cols, 0), Scalar(0, 255, 0), 4);
					line(img_matches, scene_corners[2] + Point2f(((*iter)->GrayScaleImage).cols, 0), scene_corners[3] + Point2f(((*iter)->GrayScaleImage).cols, 0), Scalar(0, 255, 0), 4);
					line(img_matches, scene_corners[3] + Point2f(((*iter)->GrayScaleImage).cols, 0), scene_corners[0] + Point2f(((*iter)->GrayScaleImage).cols, 0), Scalar(0, 255, 0), 4);
			
					putText(img_matches, (*iter)->sName, opposite + Point2f(200, 30), FONT_HERSHEY_COMPLEX, 1, Scalar(0, 255, 0));
					//Show detected matches
					imshow("Good Matches & Object detection", img_matches);
					return (*iter);
				}
			}
		}
	}
	return new Suessigkeit();
}

bool CKinectProductClassifier::compareMatHist(Mat src, MatND ref, int compare_method){
	Mat HSV;
	MatND hist;
	int histSize[2] = { 50, 60 };
	float h_ranges[2] = { 0, 180 };
	float s_ranges[2] = { 0, 256 };
	const float* ranges[2] = { h_ranges, s_ranges };
	int channels[2] = { 0, 1 };

	cvtColor(src, HSV, COLOR_BGR2HSV);
	calcHist(&HSV, 1, channels, Mat(), hist, 2, histSize, ranges, true, false);
	normalize(hist, hist, 0, 1, NORM_MINMAX, -1, Mat());
	return (compareHist(hist, ref, compare_method) > 0.05);
}

bool CKinectProductClassifier::compareRatio(Mat source, float fRefRatio){

	float temp = source.rows / source.cols; //temp = ratio of the source-image

	if (temp<1){

		temp = temp / 1;

	}



	temp = Suessigkeit::fGetRatio(source) / fRefRatio; //temp = Ratio of the ratios

	if (temp<1){

		temp = temp / 1;

	}

	return (temp < 1.3); //or alternative threshold. 1,3 means 3:4 would be accepted as square. 

}

void CKinectProductClassifier::classify(Mat *rgbImage )
{
	Mat colorImage = (*rgbImage);
	vector<Suessigkeit*> auswahl;
	vector<Suessigkeit*> auswahl2;
	vector<Suessigkeit*>::iterator iter;
	imshow("test", colorImage);
	auswahl.clear();
	auswahl2.clear();

	// Vergleich der Seitenverhaeltnisse
	for (iter = (S->reference).begin(); iter != (S->reference).end(); iter++)
	{
		if (compareRatio(*rgbImage, (*iter)->fRatio) == true) 
		{ 
			auswahl.push_back((*iter)); 
		}
	}
	
    Suessigkeit::customresize(colorImage, 600);

	// Vergleich der Farbwerte
	for (iter = auswahl.begin(); iter != auswahl.end(); iter++)
	{
			if (compareMatHist(colorImage,( (*iter)->hist)))
			{ 
				auswahl2.push_back((*iter)); 
			}
	}

	cvtColor(colorImage, colorImage, CV_BGR2GRAY);
	Suessigkeit* result = customsurfdetector(auswahl2, colorImage);

	if ((result)->sName != "NULL")
	{
		m_pBooker->book(result->iID);
	}
}

