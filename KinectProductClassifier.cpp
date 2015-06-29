#include "stdafx.h"

#include <algorithm>
#include <iostream>
#include "math.h"
#include "Sortiment.h"
#include "Suessigkeit.h"

#include "KinectProductClassifier.h"
#include <fstream>

#define _CRT_SECURE_NO_DEPRECATE
#include "stdio.h"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/nonfree/nonfree.hpp"
#include "opencv2/flann/flann.hpp"
#include "opencv2/objdetect/objdetect.hpp"

#define tresholdcamerafailure 250 // Wird zur beschleunigung und Stabilitaet benötigt! Standart 250
#define tresholdgoodcamerakeypoints 10 // Standart 10
#define minHessian 350           // Schwellwert fuer Surf Detector!

int minHessian_swap = minHessian; // Fuer andere Klassen!

//#include <iostream>
using namespace std;
using namespace cv;



CKinectProductClassifier::CKinectProductClassifier(CKinectShopApp *app)
{
	m_app = app;
	m_pBooker = new CKinectProductBooker(app);

	//Init mysql connection
	SQLHSTMT hstmt;
	SQLRETURN retcode;

	// Allocate environment handle
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_henv);

	// Set the ODBC version environment attribute
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(m_henv, SQL_ATTR_ODBC_VERSION, (void*) SQL_OV_ODBC3, 0); 

		// Allocate connection handle
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, m_henv, &m_hdbc); 

			SQLWCHAR  errmsg[1024];
			// Connect to data source
			retcode = SQLConnect(m_hdbc, (SQLWCHAR*) DB_NAME, SQL_NTS, NULL, SQL_NTS, NULL, SQL_NTS);
			SQLGetDiagRec(SQL_HANDLE_DBC, m_hdbc, 1, NULL,NULL, errmsg, 1024, NULL);

			// Allocate statement handle
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &hstmt); 

				retcode = SQLExecDirect( hstmt, (SQLWCHAR*) L"SELECT id FROM products;", SQL_NTS);

				SQLSMALLINT nCols = 0;
				SQLLEN nRows = 0;
				SQLLEN nIdicator = 0;
				int id;
				SQLNumResultCols( hstmt, &nCols );
				SQLRowCount( hstmt, &nRows );
				while(SQL_SUCCEEDED(retcode = SQLFetch(hstmt)))
				{
					for( int i=1; i <= nCols; ++i )
					{
						retcode = SQLGetData(hstmt, i, SQL_C_LONG, (SQLPOINTER) &id, sizeof(int), &nIdicator);
						if(retcode == SQL_SUCCESS)
						{
							std::cout << id << std::endl;
						}
					}
				}

				SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
			}
		}
	}
	Sortiment *S = new Sortiment();
}

CKinectProductClassifier::~CKinectProductClassifier()
{
	m_app = NULL;
	delete m_pBooker;
	m_pBooker = NULL;
		
	// Clean up mysql connection
	SQLDisconnect(m_hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, m_hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, m_henv);

}

//////////////////////////////////////////////////////////////////////////////////
/// customsurfdetector returns an pointer to the detected object
//////////////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////////////
/// comparehist 
//////////////////////////////////////////////////////////////////////////////////
bool CKinectProductClassifier::compareMatHist(Mat src, MatND ref, int compare_method = CV_COMP_CORREL){
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


/*
int Initialisiere_Datenbank(SWEET* pRefferenze, int iSchranke){
	//iSchranke=63;
	Mat tmpBild;	//Refferenzbild
	string sDataArray[7] = {"1", "2", "3", "4", "5", "6", "7"};//, "8", "9", "10"}; //Array zum einlesen des Refferenzbildes
	string sDataArray2[9] = {"Alpenmilch", "Fritt", "Joghurt", "KatzenOhren", 
							"KitKat", "Knusperflakes", "Lakritz", "Schluempfe", "Yogurette"}; //Array zum einlesen der Refferenzbildordner
	
	
	//Initialisieren des Arrays
	for(int i=0; i < iSchranke; i++){
		pRefferenze[i].fId = 0.0;
		pRefferenze[i].sName = "";
		pRefferenze[i].dFlaeche = 0;
		pRefferenze[i].dMittelwert_Farbwert = 0;
		pRefferenze[i].dMittelwert_Saettigung = 0;
	}

	//über neun ordner a 10 refferenzbildern
	for(int j = 0; j < 9; j++){
		float fCount = 1.0;
	for(int i = 0; i < 7; i++){
		//Kennzeichnung der Süssigkeit

		//Bildanalyse
		string sRefferenze = "IniPics\\"+sDataArray2[j] +"\\" + sDataArray[i] + ".png";
		tmpBild = imread(sRefferenze, 1);
		
		SWEET* sweet1 = new SWEET;
		sweet1->dMittelwert_Farbwert = 0;
		sweet1->dMittelwert_Saettigung = 0;
		sweet1->dFlaeche = 0;
		sweet1->fId = (float)j + fCount;
		sweet1->sName = sDataArray2[j];
		
		HSV_Analyser(tmpBild, sweet1);
		AREA_Analyser(sweet1);
		pRefferenze[i+7*j] = *sweet1;

		//Eindeutige ID
		fCount += 0.01;
	}
	}
	//eingelesene daten in Ini.cfg speichern, zum schnelleren Laden .... für inifunktion2
	ofstream file("Ini.cfg");
	for(int i=0;i<iSchranke;i++)
	{
		file<<pRefferenze[i].fId<<"\t"<<pRefferenze[i].sName<<"\t\t"<<pRefferenze[i].dMittelwert_Farbwert<<"\t"<<pRefferenze[i].dMittelwert_Saettigung<<"\t"<<pRefferenze[i].dFlaeche<<endl;
	}
	file.close();
	return 0;
}
*/








//===============================================================================================================================================


void CKinectProductClassifier::classify(Mat *rgbImage )
{
	Mat colorImage = (*rgbImage);
	vector<Suessigkeit*> auswahl;
	vector<Suessigkeit*> auswahl2;
	vector<Suessigkeit*>::iterator iter;

	auswahl.clear();
	auswahl2.clear();

	Suessigkeit::customresize(&colorImage, 600);

	for (iter = sortiment.begin(); iter != sortiment.end(); iter++)
		{
			if (compareMatHist(colorImage,( (*iter)->hist)))
			{
				auswahl.push_back((*iter));
				cout << (*iter)->sName << " - ";
			}
		}
	cout << endl;

	imshow("Kamerabild", colorImage);
	cvtColor(*colorImage, colorImage, CV_BGR2GRAY);

	Suessigkeit* result = customsurfdetector(auswahl, colorImage);
	if ((result)->sName != "NULL")
	{
		cout << "Es handelt sich um das Objekt " << result->sName << endl;
	}



	/*
	if (ergebniss[0].dAbstand<50)
	m_pBooker->book(ergebniss[0].iId);
	*/
}


/*/MSER Analysefunktion, leider nicht funktionsfähig

//vector<vector<cv::Point>> MSER_Analyser(Mat image,int Matr[GENAU][GENAU])
{

	Mat subimage,yuvimage;
	image=imread("B_Cola.png", 1);

	cvtColor(image, yuvimage, CV_BGR2GRAY);  //COLOR_BGR2YCrCb
	vector<vector<cv::Point>> contourses;
	
	MSER()(yuvimage, contourses);

	return contourses;
	//MSER ms;		
	/*
	for(int aussenx=1; aussenx<=GENAU; aussenx++){
		for(int ausseny=1; ausseny<=GENAU; ausseny++){
			
			//Definiere die Koordinaten im Vektorraum
			int posx=image.cols*aussenx/GENAU-image.cols*(aussenx-1)/GENAU;
			int posy=image.rows*ausseny/GENAU-image.rows*(ausseny-1)/GENAU;
		
			//Lege die Startwerte fest
			int startposx=image.cols*(aussenx-1)/GENAU;
			int startposy=image.rows*(ausseny-1)/GENAU;
			
			//Erzeuge rechteckigen Referenzraum und addiere Ausschlaggebende Punkte
			Rect faceRect(startposx,startposy,posx,posy); //pos x,y je -1
			subimage = yuvimage(faceRect).clone();
			MSER()(subimage, contourses);
			
			//MSER()(subimage, contourses);
			int size= contourses.capacity();
			Matr[aussenx-1][ausseny-1]=size;
		}
	}
	//MSER()(yuvimage, contourses);
}	*/
//goto MSERRES;    


 


//=============================================================================================================================================
//Initialisieren der Vergleichsdatenbank aus Bilddatein für die erste Datenbankerstellung