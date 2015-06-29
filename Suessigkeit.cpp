#pragma once
#include "StdAfx.h"
#include "Suessigkeit.h"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/nonfree/nonfree.hpp"
#include "opencv2/core/core.hpp"

extern int minHessian_swap;

using namespace std;
using namespace cv;
Suessigkeit::Suessigkeit(){ sName = "NULL"; }
Suessigkeit::Suessigkeit(string Pfad, string Name, int ID, int longerborder)
{
	RGBimage = imread(Pfad);
	if (!RGBimage.data){cout << " Error reading image " << Name << endl;}
	customresize(RGBimage, longerborder);
	cvtColor(RGBimage, GrayScaleImage, CV_BGR2GRAY);
	sName = Name;
	iID = ID;
	fRatio =fGetRatio(RGBimage);

	///////////////////////////////////////
	/// SurfFeatures werden fuer Referenzobjekt berechnet
	SurfFeatureDetector detector(minHessian_swap);
	detector.detect(GrayScaleImage, referencekeypoints);
	///////////////////////////////////////
	/// SurfDescriptors werden berechnet
	SurfDescriptorExtractor extractor;
	extractor.compute(GrayScaleImage,referencekeypoints, referencedescriptors);

	// Histogram
	Mat HSV;
	int histSize[2] = { 50, 60 };
	float h_ranges[2] = { 0, 180 };
	float s_ranges[2] = { 0, 256 };
	const float* ranges[2] = { h_ranges, s_ranges };
	int channels[2] = { 0, 1 };

	cvtColor(RGBimage, HSV, COLOR_BGR2HSV);
	calcHist(&HSV, 1, channels, Mat(), hist, 2, histSize, ranges, true, false);
	normalize(hist, hist, 0, 1, NORM_MINMAX, -1, Mat());


}
Suessigkeit::~Suessigkeit(){}

//////////////////////////////////////////////////////////////////////////////////
///  Suessigkeit::customresize() verkleinert eine Matrix und behaelt dabei die 
///  Seitenverhaeltnisse bei!
//////////////////////////////////////////////////////////////////////////////////
void Suessigkeit::customresize(Mat &reference, int longerborder)
{
	if ((reference.cols > longerborder) || (reference.rows > longerborder)){
		if ((reference.cols) >= (reference.rows))
		{
			// Das Bild ist im Querformat vorhanden!
			double factor = (double)longerborder / (double)reference.cols;
			resize(reference, reference, Size(longerborder, (reference.rows)*factor));
		}
		else
		{
			// Das Bild ist im Hochformat vorhanden!
			double factor = (double)longerborder / (double)reference.rows;
			resize(reference, reference, Size((reference.cols)*factor, longerborder));
		}
	}
}

float Suessigkeit::fGetRatio(Mat &reference)
{
	float temp = reference.rows/reference.cols;
	if(temp < 1){
		temp = 1/temp;
	}
	return temp;

}