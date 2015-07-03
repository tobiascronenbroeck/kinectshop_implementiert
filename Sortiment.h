#pragma once
#include <vector>
class Suessigkeit;
using namespace std;

class Sortiment
{
public: 
	Sortiment();
	~Sortiment();
	static int iGetSchranke();
	void Initialisiere_Datenbank(); 
	vector<Suessigkeit*> reference;
	static void SQLtoINI();
};

