
#define WIN32_LEAN_AND_MEAN
#include "Logger/Logger.h"
#include "labview_lasershot.h"
#include <process.h>
#include "stdio.h"
#include "ew_Scene.h"


bool bStop = false;
LPFNDLLFUNC1	lpfnDllFunc1;		// Function pointer

ew_Labview_lasershot *ew_Labview_lasershot::s_instance = NULL;



ew_Labview_lasershot *ew_Labview_lasershot::GetSingleton()
{
	if (!s_instance)
		s_instance = new ew_Labview_lasershot;

	return s_instance;
}

void ew_Labview_lasershot::FreeSingleton()
{
	if (s_instance)
		delete s_instance;
	s_instance = NULL;
}

ew_Labview_lasershot::ew_Labview_lasershot()
{
	m_bInit = false;
	m_hDLL = NULL;
	
	m_Data.shotcount = 0;
	for (int i=0; i<32; i++)
	{
		m_Data.shots[i].x = -1;
		m_Data.shots[i].y = -1;
	}

	m_Data.RECOIL = 0;


	m_bHandgun = true;
	m_bRecAssoc = false;
	m_bsetChannel = false;
	m_bDoor = false;
	m_bDimmer = false;
	m_bFan = false;
	m_ShotLoadCounter =0;


	ew_Scene::GetSingleton()->getLogger()->LogHeader(L"ew_Labview_lasershot::ew_Labview_lasershot()  .. constructor");
}



ew_Labview_lasershot::~ew_Labview_lasershot()
{
	closeDLL();
}


void ew_Labview_lasershot::openDLL()
{
	ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"::openDLL()");

	m_hDLL = LoadLibrary( L"controllers/LaserShotDLL.dll" );
	if (m_hDLL != NULL)
	{
		if( (m_lpfnDllFunc1 = (LPFNDLLFUNC1)GetProcAddress( m_hDLL, "LaserShotDLL" )) == NULL )
		{
			ew_Scene::GetSingleton()->getLogger()->Log(LOG_ERROR, L"... failed.. getFunction()");
			m_bInit = false;
			return;
		}
		else
		{
			ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"... sucsess");
			lpfnDllFunc1 = m_lpfnDllFunc1;
		}
	}
	else
	{
		ew_Scene::GetSingleton()->getLogger()->Log(LOG_ERROR, L"... failed ... LoadLibrary()");
		m_bInit = false;
		return;
	}

	setValves( 0 );	// CLOSE all the valves on shutdown
	m_bInit = true;
}



void ew_Labview_lasershot::closeDLL()
{
	if (m_bInit)
	{
		bStop = true;
		int LV_length = 32;

		setValves( 0 );	// CLOSE all the valves on shutdown
		lpfnDllFunc1( m_Data.Rounds, &m_bFan, &bStop, &m_bHandgun, &m_bRecAssoc, &m_bsetChannel, &m_bDoor, &m_bDimmer, m_Data.RECOIL, &m_Data.shotcount, &m_Data.shots[0], LV_length, &m_Data.pImage[0], 480*752 );
		

		Sleep(1000);
		FreeLibrary( m_hDLL );
		m_hDLL = NULL;
		m_bInit = false;
	}

}



void ew_Labview_lasershot::restartDLL()
{
	closeDLL();
	Sleep(1000);
	openDLL();
} 


_Lasershot *ew_Labview_lasershot::read()
{
	if (!m_bInit) openDLL();		// first try to open dll, then fail
	if (!m_bInit) return NULL;
	int LV_length = 32;

	//static int LASTSHOTCOUNT = 0; 
	//static int PREV_CNT = 0;
	for (int i=0; i<32; i++)
	{
		m_Data.shots[i].x = -999;
		m_Data.shots[i].y = -999;
	}
	m_Data.bError_TooLittleData = false;
	m_Data.bError_TooMuchData = false;

	static int SHOTCOUNT = 0;

	bStop = false;
	lpfnDllFunc1( m_Data.Rounds, &m_bFan, &bStop, &m_bHandgun, &m_bRecAssoc, &m_bsetChannel, &m_bDoor, &m_bDimmer, m_Data.RECOIL, &m_Data.shotcount, &m_Data.shots[0], LV_length, &m_Data.pImage[0], 480*752 );
	m_ShotLoadCounter --;
	if (m_ShotLoadCounter < 0)	m_Data.Rounds = 0;		// reset roudns to zero - just flash the value

	setRequestChannel(false);
	m_Data.readcount ++;

	int cntArray = 0;
	for (int i=0; i<32; i++)	{	if ( m_Data.shots[i].x > -900 ) cntArray++;	}
	SHOTCOUNT += cntArray;
	m_Data.shotcount = SHOTCOUNT;
	

	return &m_Data;
}

