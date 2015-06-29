# pragma once
#include "stdafx.h"
#include "Sortiment.h"
#include "Suessigkeit.h"
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;


Sortiment::Sortiment()
{
	Initialisiere_Datenbank();
}


Sortiment::~Sortiment()
{
}

int Sortiment::iGetSchranke()
{	//ermittelt die anzahl der posten in ini.cfg
	int i; 
	ifstream file ( "Ini.cfg" );
	string line;
	for (i = 0; getline(file, line); ++i);	
	file.close();
	return i;
}

void Sortiment::Initialisiere_Datenbank()
{	
	int iSchranke = iGetSchranke();
	int iTempID;
	string sTempName, sTempPfad;
	
	
	ifstream file ( "Ini.cfg" );
	for(int i=0; i < iSchranke; i++){
		file>>iTempID;
		file>>sTempName;
		file>>sTempPfad;
		reference.push_back(new Suessigkeit(sTempPfad, sTempName, iTempID));
	}
	file.close();


}
