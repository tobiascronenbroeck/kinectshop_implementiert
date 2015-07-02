# pragma once
#include "stdafx.h"
#include "Sortiment.h"
#include "Suessigkeit.h"
#include <windows.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <sql.h>
#include <sqltypes.h>
#include <sqlext.h>

SQLHENV                                 m_henv;
SQLHDBC                                 m_hdbc;

#define DB_NAME L"kinectshop2015"   //L

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

void Sortiment::SQLtoINI()
{
	//Init mysql connection
	SQLHSTMT hstmt;
	SQLRETURN retcode;


	// Allocate environment handle
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_henv);
	// Set the ODBC version environment attribute
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(m_henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
		// Allocate connection handle
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, m_henv, &m_hdbc);
			SQLWCHAR  errmsg[1024];
			// Connect to data source
			retcode = SQLConnect(m_hdbc, (SQLWCHAR*)DB_NAME, SQL_NTS, NULL, SQL_NTS, NULL, SQL_NTS);
			SQLGetDiagRec(SQL_HANDLE_DBC, m_hdbc, 1, NULL, NULL, errmsg, 1024, NULL);

			// Allocate statement handle
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &hstmt);

				retcode = SQLExecDirect(hstmt, (SQLWCHAR*)L"SELECT id, title, OCTET_LENGTH(photo), photo FROM products;", SQL_NTS);

				SQLSMALLINT nCols = 0;
				SQLLEN nRows = 0;
				SQLLEN nIdicator = 0;
				SQLLEN nIdicator_1 = 0;
				SQLLEN nIdicator_2 = 0;
				SQLLEN BinaryLenOrInd;
				SQLCHAR pPicture[800000] = { 0 };
				int id;
				int length;
				SQLCHAR name[50];

				SQLNumResultCols(hstmt, &nCols);
				SQLRowCount(hstmt, &nRows);
				while (SQL_SUCCEEDED(retcode = SQLFetch(hstmt)))
				{
					for (int i = 1; i <= nCols; ++i)
					{
						retcode = SQLGetData(hstmt, i, SQL_C_LONG, (SQLPOINTER)&id, sizeof(int), &nIdicator);
						SQLGetData(hstmt, i + 1, SQL_C_CHAR, (SQLCHAR *)name, 50, &nIdicator_1);
						SQLGetData(hstmt, i + 2, SQL_C_LONG, &length, sizeof(int), &nIdicator_2);
						retcode = SQLGetData(hstmt, i + 3, SQL_C_BINARY, pPicture, sizeof(pPicture), NULL);
						string str(reinterpret_cast<const char*>(pPicture), reinterpret_cast<const char*>(pPicture)+sizeof(pPicture));
						str.resize(length);

						if (retcode == SQL_SUCCESS)
						{

							//cout << id << " " << name << endl;
							string sName((const char*)name);
							string filename = "refimg/" + sName + ".png";
							ofstream ofs(filename, ios::binary);
							ofs << str;
							ofs.close();

							int iSchranke = iGetSchranke();
							ofstream file("Ini.cfg");
							file << id << "\t" << sName << "\t" << filename << endl;
							file.close();
						}

					}
				}

				SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
			}
		}
	}

	//MYSQL-Disconnect
	SQLDisconnect(m_hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, m_hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, m_henv);
}
