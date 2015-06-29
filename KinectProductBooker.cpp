#include "stdafx.h"

#include <algorithm>

#include "KinectProductBooker.h"

#include <list>

using namespace std;

CKinectProductBooker::CKinectProductBooker(CKinectShopApp *app)
{
	m_app = app;

	m_currentPid = -1;

	//Init mysql connection
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
		}
	}

	// register the callback function to handle speech commands
	m_app->registerSpeechCallback(this, CKinectProductBooker_speechAction);
}

CKinectProductBooker::~CKinectProductBooker()
{
	m_app = NULL;

	// Clean up mysql connection
	SQLDisconnect(m_hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, m_hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, m_henv);

}

void CKinectProductBooker::book(int pid)
{
	// Init mysql statement
	SQLHSTMT hstmt;
	SQLRETURN retcode;

	// Allocate statement handle
	retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &hstmt); 

	//exec statement
	wchar_t buffer[1024];
	swprintf_s(buffer, 1023, L"SELECT photo FROM products WHERE id = %d;", pid);
	retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);

	SQLSMALLINT nCols = 0;
	SQLLEN nRows = 0;
	SQLLEN nIdicator = 0;
	uchar *buf = new uchar[MAX_IMAGE_SIZE];
	SQLNumResultCols( hstmt, &nCols );
	SQLRowCount( hstmt, &nRows );
	while(SQL_SUCCEEDED(retcode = SQLFetch(hstmt)))
	{
		for( int i=1; i <= nCols; ++i )
		{
			retcode = SQLGetData(hstmt, i, SQL_C_BINARY, (SQLCHAR*) buf, MAX_IMAGE_SIZE, &nIdicator);
			if(retcode == SQL_SUCCESS)
			{
				cv::Mat decodedImage = cv::imdecode(cv::Mat(1, (int) nIdicator, CV_8UC1, buf), CV_LOAD_IMAGE_COLOR);
				m_app->Nui_DrawProductImage(&decodedImage);
			}
			WaitForSingleObject(m_productChangeMutex, INFINITE);
			m_currentPid = pid;
			ReleaseMutex(m_productChangeMutex);
		}
	}
	delete(buf);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

LPCWSTR CKinectProductBooker::getTag(const SPPHRASEPROPERTY* pszSpeechTag, int tagNr)
{
	const SPPHRASEPROPERTY* tmpSpeechTag = pszSpeechTag->pFirstChild;

	if(tmpSpeechTag == NULL) return L"";

	for(int i = 0; i < tagNr; i++)
	{
		if(tmpSpeechTag->pNextSibling != NULL)
		{
			tmpSpeechTag = tmpSpeechTag->pNextSibling;
		}
		else
		{
			return L"";
		}
	}

	return tmpSpeechTag->pszValue;
}

void CKinectProductBooker::speechAction(const SPPHRASEPROPERTY *pszSpeechTag)
{
	// Init mysql statement
	SQLHSTMT hstmt;
	SQLRETURN retcode;

	// Allocate statement handle
	retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &hstmt);
	
	LPCWSTR speechCommand[20];

	speechCommand[0] = getTag(pszSpeechTag, 0);
	int i = 1;
	while(i<20)
	{
		speechCommand[i] = getTag(pszSpeechTag, i);
		i++;
	}
	//LPCWSTR speechCommand = getTag(pszSpeechTag, 0);
	//LPCWSTR speechCommand2 = getTag(pszSpeechTag, 1);
	i = 0;
	
	bool buy_or_not = 0;
	int amount = 1;
	int user_id = 0;
	bool user_ef = 0;
	bool quit = 0;
 	while(i<20) //lese alle erkennbare Woerte aus. 
	{
		if (0 == wcscmp(L"BUY", speechCommand[i]))
		{
			buy_or_not = 1; //Schalter fuer Durchfuerung
		}

		//sonstige  menge 1 als default
		if (0 == wcscmp(L"2", speechCommand[i]))
		{
			amount = 2;  
		}
		else if(0 == wcscmp(L"3", speechCommand[i]))
		{
			amount = 3;
		}
		else if(0 == wcscmp(L"4", speechCommand[i]))
		{
			amount = 4;
		}
		else if(0 == wcscmp(L"5", speechCommand[i]))
		{
			amount = 5;
		}
		else if(0 == wcscmp(L"6", speechCommand[i]))
		{
			amount = 6;
		}
		else if(0 == wcscmp(L"7", speechCommand[i]))
		{
			amount = 7;
		}
		else if(0 == wcscmp(L"8", speechCommand[i]))
		{
			amount = 8;
		}
		else if(0 == wcscmp(L"9", speechCommand[i]))
		{
			amount = 9;		
		}

		if(0 == wcscmp(L"James", speechCommand[i]))
		{
			user_id = 1;
			user_ef = 1;
		}
		else if(0 == wcscmp(L"Watson", speechCommand[i]))
		{
			user_id = 2;
			user_ef = 1;
		}
		else if(0 == wcscmp(L"Peter", speechCommand[i]))
		{
			user_id = 3;
			user_ef = 1;
		}

		if(0 == wcscmp(L"EXIT", speechCommand[i]))
		{
			quit = 1; 
		}
		i++;
	}


	if (buy_or_not&&user_ef)
    {
		WaitForSingleObject(m_productChangeMutex, INFINITE);
		if(m_currentPid != -1)
		{
			wchar_t buffer[1024];
			
			// Get stock
			swprintf_s(buffer, 1023, L"SELECT stock FROM products WHERE id = %d;", m_currentPid); 
			retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);
			
			SQLLEN nIdicator = 0;
			SQLSMALLINT num = 0;
			int num_p;
			bool change1 = false;
			while(SQL_SUCCEEDED(retcode = SQLFetch(hstmt)))
			{
				retcode = SQLGetData(hstmt, 1, SQL_C_SSHORT, &num, 0, &nIdicator);
				if(retcode == SQL_SUCCESS) 
				{
					num_p = num;
					num_p = num_p -	 amount ;
					if(!(num_p < 0))
						change1 = true;
				}
			}

			//get price
			SQLLEN nIdicator1 = 0;
			SQLSMALLINT num1 = 0;
			
			swprintf_s(buffer, 1023, L"SELECT price FROM products WHERE id = %d;", m_currentPid); 
			retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);
 
			int price;
			while(SQL_SUCCEEDED(retcode = SQLFetch(hstmt)))
			{
				retcode = SQLGetData(hstmt, 1, SQL_C_SSHORT, &num1, 0, &nIdicator1);
				if(retcode == SQL_SUCCESS) 
				{
					price = num1;
				}
			}
			//Get balance

			SQLLEN nIdicator2 = 0;
			SQLSMALLINT num2 = 0;
			swprintf_s(buffer, 1023, L"SELECT balance FROM users WHERE id = %d;", user_id); 
			retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);
 
			int balance;
			bool change2 = false;
			while(SQL_SUCCEEDED(retcode = SQLFetch(hstmt)))
			{
				retcode = SQLGetData(hstmt, 1, SQL_C_SSHORT, &num2, 0, &nIdicator2);
				if(retcode == SQL_SUCCESS) 
				{
					balance = num2;
					balance = balance- amount*price;
					if(!(balance < 0))
						change2 = true;
				}
			}
				
			//
			if(change1&&change2)
			{
				swprintf_s(buffer, 1023, L"INSERT INTO bookings (pid, uid, amount) VALUES(%d, %d, %d)", m_currentPid, user_id, amount);
				retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);
				swprintf_s(buffer, 1023, L"UPDATE products SET stock = %d WHERE id =%d;", num_p,m_currentPid); 
				retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);
				swprintf_s(buffer, 1023, L"UPDATE users SET balance = %d WHERE id =%d;", balance,user_id); //write a select uid programm by using speech
				retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);
			}
			// wenn man exit, over, oder  byebye sagt, wird den Programm abgeschlossen.
			if(quit==1)
			{
				exit(1);
			}
			ReleaseMutex(m_productChangeMutex);
		}
	}
	
	/*if (buy_or_not)
    {
		WaitForSingleObject(m_productChangeMutex, INFINITE);
		if(m_currentPid != -1)
		{
			wchar_t buffer[1024];
			
			//for product's stock
			bool change1 = false;
			int num_p = getNum_p(1);
			if(num_p!=-1) // -1 means doesn't connecting with SQL
			{num_p-=amount;}
			 
			if(!(num_p < 0))
				change1 = true;
				
			//for user's balance
			bool change2 = false;
			int balance = getNum_u(user_id,1);
			if(balance!=-1)
			{
				int preis = getNum_p(2);
				balance -= preis*amount;
			}
				
			if(!(balance < 0))
				change2 = true;
			//

			
			if(change1&&change2)
			{
				update(amount,num_p,user_id,balance);
			}

			ReleaseMutex(m_productChangeMutex);
		}
		
	}*/
}

void CKinectProductBooker_speechAction(void *classPointer, const SPPHRASEPROPERTY *pszSpeechTag)
{
	((CKinectProductBooker*) classPointer)->speechAction(pszSpeechTag);
}

int CKinectProductBooker::getNum_p(int get_type)
{
	// Init mysql statement
	SQLHSTMT hstmt;
	SQLRETURN retcode;

	// Allocate statement handle
	retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &hstmt);

	wchar_t buffer[1024];
	int num_p=-1;

	if(get_type==1) //get stock
	{
		// Get stock
		swprintf_s(buffer, 1023, L"SELECT stock FROM products WHERE id = %d;", m_currentPid); 
		retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);
		
		SQLLEN nIdicator = 0;
		SQLSMALLINT num = 0;
			
		while(SQL_SUCCEEDED(retcode = SQLFetch(hstmt)))
		{
			retcode = SQLGetData(hstmt, 1, SQL_C_SSHORT, &num, 0, &nIdicator);
			if(retcode == SQL_SUCCESS) 
			{
				num_p = num;
			}
		}
	}
	
	else if(get_type==2)
	{
		swprintf_s(buffer, 1023, L"SELECT price FROM products WHERE id = %d;", m_currentPid); 
		retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);

		SQLLEN nIdicator = 0;
		SQLSMALLINT num = 0;
		while(SQL_SUCCEEDED(retcode = SQLFetch(hstmt)))
		{
			retcode = SQLGetData(hstmt, 1, SQL_C_SSHORT, &num, 0, &nIdicator);
			if(retcode == SQL_SUCCESS) 
			{
				num_p = num;
			}
		}
	}
	return num_p;
}

int CKinectProductBooker::getNum_u(int user_id,int get_type)
{
	// Init mysql statement
	SQLHSTMT hstmt;
	SQLRETURN retcode;

	// Allocate statement handle
	retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &hstmt);

	wchar_t buffer[1024];
	int balance=-1;

	if(get_type==1)
	{
		swprintf_s(buffer, 1023, L"SELECT balance FROM users WHERE id = %d;", m_currentPid); 
		retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);

		SQLLEN nIdicator = 0;
		SQLSMALLINT num = 0;
		while(SQL_SUCCEEDED(retcode = SQLFetch(hstmt)))
		{
			retcode = SQLGetData(hstmt, 1, SQL_C_SSHORT, &num, 0, &nIdicator);
			if(retcode == SQL_SUCCESS) 
			{
				balance = num;
			}
		}
	}
	return balance;
}

void CKinectProductBooker::update(int amount,int num_p,int user_id,int balance)
{
	// Init mysql statement
	SQLHSTMT hstmt;
	SQLRETURN retcode;

	// Allocate statement handle
	retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &hstmt);

	wchar_t buffer[1024];

	swprintf_s(buffer, 1023, L"INSERT INTO bookings (pid, uid, amount) VALUES(%d, %d, %d)", m_currentPid, 1, amount);
	retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);
	swprintf_s(buffer, 1023, L"UPDATE products SET stock = %d WHERE id =%d;", num_p,m_currentPid); 
	retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);
	swprintf_s(buffer, 1023, L"UPDATE users SET balance = %d WHERE id =%d;", num_p,1); //write a select uid programm by using speech
	retcode = SQLExecDirect( hstmt, (SQLWCHAR*) buffer, SQL_NTS);
}