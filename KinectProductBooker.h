#pragma once

// database
#include <sql.h>
#include <sqlext.h>

#include "KinectShop.h"

class CKinectProductBooker
{
public:
	CKinectProductBooker(CKinectShopApp *app);
	~CKinectProductBooker();

	void book(int pid);
	void speechAction(const SPPHRASEPROPERTY *pszSpeechTag);
	int getNum_p(int get_type); //type 1:menge der Produkt, type2: price 
	int getNum_u(int user_id,int get_type);
	void update(int amount,int num_p,int user_id,int balance);
private:
	CKinectShopApp *m_app;
	int m_currentPid;
	HANDLE m_productChangeMutex;

	// Database connection
	SQLHENV                                 m_henv;
	SQLHDBC                                 m_hdbc;

	LPCWSTR CKinectProductBooker::getTag(const SPPHRASEPROPERTY* pszSpeechTag, int tagNr);
};

void CKinectProductBooker_speechAction(void *classPointer, const SPPHRASEPROPERTY *pszSpeechTag);
