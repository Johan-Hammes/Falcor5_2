#include "Controllers/ctrl_DirectWeapons/ctrl_DirectWeapons.h"
#include "ew_Scene.h"
#include "d3dx9.h"
#include "d3d9.h"
#include "math.h"
#include "server_defines.h"
#include "earthworks_Raknet.h"
#include "Logger/CSVFile.h"
#include "Util/DialogUtil.h"
#include "ljackuw.h"
#include "VoiceRecognition/VoiceRecognition.h"
#include "Controllers/ctrl_DirectWeapons/TargetManager.h"
#include <iostream>
#include <sstream>
#include <algorithm>



DmString			VERSION(L"Kolskoot version 3.5.0  2015-09-05");
DmString			COPYRIGHT(L"Copyright © Dimension Software Engineering - 2012");

ctrl_DirectWeapons g_Instance;
ew_Controller *s_pInstance = &g_Instance;
 


#define XXXXX 0x...1...2...3...4...5...6...7...8
#define Lanes 0x0f000000
#define Modu  0x000f0000
#define Date  0x00000ff0

unsigned int SEC_code( unsigned int serial )
{
	return ((serial >> 3) + (serial << 28) + (serial << 12)) & 0x7fffffff;
}

unsigned int SEC_addLanes(unsigned int code, int lanes)
{
	unsigned int L = (lanes & 0xf) << 24;
	return code ^ L;
}

unsigned char SEC_getLanes( unsigned int serial, unsigned int code )
{
	unsigned int L = ( SEC_code(serial) ^ code) & Lanes;
	return (unsigned char)(L>>24);
}

bool SEC_isValid(unsigned int serial, unsigned int code)
{
	unsigned int MASK = ~(Lanes | Modu | Date);
	if ( (SEC_code(serial)&MASK) == (code&MASK))
	{
			return true;
	}
	return false;
}


bool cmp_Initials(_user A, _user B)
{
	if (A.initials<B.initials) return true;
	else return false;
}

bool cmp_Surname(_user A, _user B)
{
	if (A.surname<B.surname) return true;
	else if (A.surname==B.surname) return cmp_Initials(A, B);
	else return false;
}

bool cmp_Users(_user A, _user B)
{
	if (A.score>B.score) return true;
	else if (A.score==B.score) return cmp_Surname( A, B );
	else return false;
}



ctrl_DirectWeapons::ctrl_DirectWeapons()
{
	m_pScene = ew_Scene::GetSingleton();
	m_pScene->fastload_enableRender( false );
	m_pCfgMng = m_pScene->getConfigManager();
	m_pLogger = m_pScene->getLogger();
	m_pLogger->Log( LOG_INFO, L"ctrl_DirectWeapons::ctrl_DirectWeapons()" );
	m_pLogger->Tab();
	m_pLogger->Log( LOG_INFO,	VERSION.c_str() );
	m_pLogger->Log( LOG_INFO, COPYRIGHT.c_str() );
	
	dw_scenario::s_pCTRL = this;
	dw_scenario::s_pSCENE = m_pScene;
	m_Camera.setFOV( 90 );
	m_pScene->setCamera( &m_Camera );
	m_pScene->m_Z_near = 0.2f;
	m_pScene->m_Z_far = 2000;

	m_bShowFPS = false;
	ew_Scene::GetSingleton()->getResourceManager()->addResourcePath( ((*m_pCfgMng)["Earthworks_3"]["INSTALL_DIR"] + "/media/controllers/DirectWeapons3/").getWString().c_str() );
	ew_Scene::GetSingleton()->getResourceManager()->addResourcePath( ((*m_pCfgMng)["Earthworks_3"]["INSTALL_DIR"] + "/user_Scenes/").getWString().c_str() );

	m_bLabjackTransnetShift = false;
	m_bLabjackTransnetShift = !(*m_pCfgMng)["controller_DirectWeapons"]["Labjack"].compare("Transnet");
	if (m_bLabjackTransnetShift) m_pLogger->Log( LOG_INFO, L"WARNING ----------> LABJACK IS IN TRANSNET CONFIGURATION < ----------" );

	m_bInstructorStation = false;
	m_b3DTerminal = false;
	m_bSocketCommands = false;
	m_TerminalGoMenuCounter = 80;
	m_bInstructorStation = !(*m_pCfgMng)["controller_DirectWeapons"]["InstructorStation"].compare("INSTRUCTOR");
	m_b3DTerminal = !(*m_pCfgMng)["controller_DirectWeapons"]["InstructorStation"].compare("TERMINAL");
	m_ballowMouseFire = !(*m_pCfgMng)["controller_DirectWeapons"]["mouse"].compare("allowMouseShots");
	m_bLiveFire = (int)	((*m_pCfgMng)["controller_DirectWeapons"]["live_fire"]); 
	m_paper_advance_ms = (int)	((*m_pCfgMng)["controller_DirectWeapons"]["paper_advance_ms"]); 
	m_paper_post_delay_ms = (int)	((*m_pCfgMng)["controller_DirectWeapons"]["paper_post_delay_ms"]); 
	m_bDemoMode = false;
	m_ZigbeePacketVersion = 0;	// old packets
	m_ZigbeePacketVersion = (int) ((*m_pCfgMng)["controller_DirectWeapons"]["ZigbeePacketVersion"]); 
	m_bAllowWalkabout = false;
	if ( !(*m_pCfgMng)["controller_DirectWeapons"]["allowWalkabout"].compare("true") ) m_bAllowWalkabout = true;	

	m_pScene->setIncrementBuffer( !m_bLiveFire );

	m_FOV_scale = 1.0;
	m_FOV_scale = (*m_pCfgMng)["controller_DirectWeapons"]["FOV_scale"].getFloat();
	if ( m_FOV_scale < 0.5 || m_FOV_scale > 2.0) m_FOV_scale = 1.0;
	if (m_FOV_scale != 1.0f)
	{
		m_pLogger->Log( LOG_INFO, L"fov scale factor ( %d %% )", m_FOV_scale*100 - 100 );
	}


	// LUA ------------------------------------------------------------------------------------------------------
	LUAManager *pLUAManager = LUAManager::GetSingleton();
	m_pLUAscript = NULL;
	registerLUA();

	// SQL ------------------------------------------------------------------------------------------------------
	ewstring sqlServer = (*m_pCfgMng)["MySQL"]["SERVER_Primary"];
	if (sqlServer.length() > 0)
	{
		m_pMySQLManager = new MySQLManager("earthworks", ( (*m_pCfgMng)["MySQL"]["SERVER_Primary"]).c_str(), "earthworks", "#!rthW0rks!" );
		//m_pMySQLManager = new MySQLManager("earthworks3d", ( (*m_pCfgMng)["MySQL"]["SERVER_Primary"]).c_str(), "earthworks3d", "A#!rthW0rks!" );
		m_bMySQLError = false;
		m_bAuthorized = false;
		m_iCompanyID = -1;
		if (!m_pMySQLManager->connect())
		{
			m_pLogger->Log( LOG_ERROR, L"Error opening MySQL - %s  -  %s", ((*m_pCfgMng)["MySQL"]["SERVER_Primary"]).getWString().c_str(), m_pMySQLManager->getLastError() );
			
		}
	}
	

	// Security--------------------------------------------------------------------------------------------------
	if (!m_bInstructorStation)
	{
		#ifndef EW_BYPASS_SECURITY
			if (!m_pScene->isPointGreyConnected())
			{
				m_pLogger->Log( LOG_INFO, L"camera - NOT CONNECTED" );
				MessageBox(NULL, L"No camera found - please connect the camera and try again", L"Error", MB_OK);
				m_pScene->Quit();
				return;
			}
		#endif
	}
	checkLicense();


	// Voice Recognition-----------------------------------------------------------------------------------------
/*	m_pLogger->Log( LOG_ERROR, L"Loading voice recognition");
	if (m_pScene->getVoiceRecognition())
	{
		m_VoiceRecognitionConnection =	DmConnect(m_pScene->getVoiceRecognition()->phraseRecognised, *this, &ctrl_DirectWeapons::voiceRecognitionCallback);
		m_VoiceNoRec =					DmConnect(m_pScene->getVoiceRecognition()->phraseNotRecognised, *this, &ctrl_DirectWeapons::voiceNoRecCallback);
		m_VoiceStartEnd =				DmConnect(m_pScene->getVoiceRecognition()->soundStartEnd, *this, &ctrl_DirectWeapons::voiceStartStopCallback);
		m_VoiceAudio =					DmConnect(m_pScene->getVoiceRecognition()->audioLevel, *this, &ctrl_DirectWeapons::voiceVolumeCallback);
	}*/
	//m_pScene->getVoiceRecognition()->loadGrammar( L"grammar_Skeet.xml" );
	//m_pScene->getVoiceRecognition()->enableRecognition( true );
	
	// Camera and weapon distortions ----------------------------------------------------------------------------
	//m_pLogger->Log( LOG_INFO, L"Loading camera configuration");
	FILE *pDfile = fopen("../../config/camera_distortion.config", "rb");
	if (pDfile)
	{
		float xOffset;
		fread( &xOffset, 1, sizeof(float), pDfile );
		for (int y=0; y<4; y++)
		{
			for (int x=0; x<8; x++)
			{
				fread( distortion_blocks[0][y][x].corner, 2*4, sizeof(f_Vector), pDfile );
				fread( distortion_blocks[0][y][x].edge, 4, sizeof(f_Vector), pDfile );
			}
		}
		fclose(pDfile);
	}
	else
	{
		m_pLogger->Log( LOG_ERROR, L"failed to open   ../../config/camera_distortion.config" );
	}

	pDfile = fopen("../../config/camera_distortion_B.config", "rb");
	if (pDfile)
	{
		float xOffset;
		fread( &xOffset, 1, sizeof(float), pDfile );
		for (int y=0; y<4; y++)
		{
			for (int x=0; x<8; x++)
			{
				fread( distortion_blocks[1][y][x].corner, 2*4, sizeof(f_Vector), pDfile );
				fread( distortion_blocks[1][y][x].edge, 4, sizeof(f_Vector), pDfile );
			}
		}
		fclose(pDfile);
	}
	else
	{
		m_pLogger->Log( LOG_INFO, L"failed to open   ../../config/camera_distortion_B.config" );
	}

	loadIRgrid();

	// Create all the targets ----------------------------------------------------------------------------------
	m_pLogger->Log( LOG_INFO, L"m_targetManager->loadTargets( %s )", ((*m_pCfgMng)["Earthworks_3"]["INSTALL_DIR"] + "/media/controllers/DirectWeapons3/targets").getWString().c_str() );
	m_targetManager = new TargetManager(m_pScene);
	m_targetManager->loadTargets( (*m_pCfgMng)["Earthworks_3"]["INSTALL_DIR"] + "/media/controllers/DirectWeapons3/targets" );


	// Some debug lanes ----------------------------------------------------------------------------------------
	// AAAAAAHHHHHHH ######################################################################################
	ewstring str_numlanes = (*m_pCfgMng)["controller_DirectWeapons"]["lanes"];
	_laneInfo LI;
	LI.bActive = false;
	LI.weaponOffset = f_Vector(0, 0, 0);
	LI.cnt = 0;
	LI.userID = L"";
	LI.userNAME = L"";
	LI.info = NULL;
	m_Scenario.init( __max( 1, __min( (int)str_numlanes, m_LicensedLanes ) ) );
	for (int i=0; i<m_Scenario.m_pc_lanes; i++)		m_lanes.push_back( LI );
	m_LaneZero = (int)( (*m_pCfgMng)["controller_DirectWeapons"]["laneZero"] );
	m_NumLanes = (int)( (*m_pCfgMng)["controller_DirectWeapons"]["lanes"] );

	

	// This is used by Zigbee controler for rounds and cycling of R4 -----------------------------------------
	int zigbeeCOM = (int)((*m_pCfgMng)["controller_DirectWeapons"]["zigbeeCOM"]) -1;
	if (zigbeeCOM >=0)
	{
		m_pLogger->Log( LOG_INFO, L"opening zigbee on COM%d", zigbeeCOM+1 );
		int ret = ZIGBEE.OpenPort( (eComPort)zigbeeCOM, br_38400, 8, ptNONE, sbONE );
		if (ret !=0 ) 
		{
			m_pLogger->Log( LOG_INFO, L"ZIGBEE failed to open" );
			MessageBox(NULL, L"Failed to open COM port, please restart the computer.", L"Error", MB_OK);
		}

		int zigbeeID = (int)((*m_pCfgMng)["controller_DirectWeapons"]["zigbeeID"]);
		zigbeeID_h = (zigbeeID >> 8) & 0xff;
		zigbeeID_l = zigbeeID & 0xff;
		m_pLogger->Log( LOG_INFO, L"Zigbee ID  %d  %d", zigbeeID_h, zigbeeID_l );
	}

	// RakNet ------------------------------------------------------------------------------	
	m_RakNet = m_pScene->getRAKNET();

	//m_pLogger->Log( LOG_INFO, L"BuildGUI(..)");
	buildGUI(); 
	//m_pLogger->Log( LOG_INFO, L"constructor gomenu();");
	goMenu();
	//m_pLogger->Log( LOG_INFO, L"constructor setupPrinting();");
	//setupPrinting();

	
	int shotDelay = (int)( (*m_pCfgMng)["controller_DirectWeapons"]["shotDelay_ms"] );
	shotDelay = __min( 1000, __max( 50, shotDelay ) );
	this->m_laneGUI.setShotDelay( shotDelay );
	m_pLogger->Log( LOG_INFO, L"Shot delay set at %d ms", shotDelay );
	

	// LABJACK -----------------------------------------------------------------------------------------------
	long id = -1;
//	EDigitalOut( &id, 0, 0, 1, 1);				// 0-lights on
												// 1-paper move is later
//	EDigitalOut( &id, 0, 2, m_bLiveFire, 1); 	// 2-fans
												// 3-dimmer - ignore
//	EDigitalOut( &id, 0, 4, 1, 1);				// 4-power on
	long trisD = 0x1f;
	long trisIO = 0x0;
	long stateD = 0;
		 stateD += (1<<0);		// lights on
		 if (m_bLiveFire)	stateD += (1<<2);	//fans
		 stateD += (1<<4);						// power on
		 if (m_bLabjackTransnetShift)	stateD += (1<<3);						// power on line 3 at transnet only
	long stateIO = 0;
	long outputD = 0;
	DigitalIO( &id, 0, &trisD, trisIO, &stateD, &stateIO, 1, &outputD);

	m_ScreenSideBuffer = 0.0f;
	m_ScreenSideBuffer = ((int) ((*m_pCfgMng)["controller_DirectWeapons"]["screenBufferPercentage"])) / 100.0f;
	m_pLogger->Log( LOG_INFO, L"setting m_ScreenSideBuffer %d",  (int)(m_ScreenSideBuffer*100));

	m_TotalShotsLIVE = (int) ((*m_pCfgMng)["controller_DirectWeapons"]["TotalShots_LIVE"]);
	m_TotalShotsLASER = (int) ((*m_pCfgMng)["controller_DirectWeapons"]["TotalShots_LASER"]);
	m_correctionsDOWN = (int) ((*m_pCfgMng)["controller_DirectWeapons"]["LIVE_correction_DOWN"]);
	m_correctionsNONE = (int) ((*m_pCfgMng)["controller_DirectWeapons"]["LIVE_correction_NONE"]);
	m_correctionsUP = (int) ((*m_pCfgMng)["controller_DirectWeapons"]["LIVE_correction_UP"]);

	// playlists
	m_bAutoPrint = false;


	// AAAAAAHHHHHHH ######################################################################################
	//m_strAADString = "IFSEC 2013 - 1 Lane";		// doen ioets hoermee
	
	// clear camera logs
	//system( "del ../../camera_logs/*.jpg" );

	//ewstring sys = "del ../../camera_logs/*.jpg";
	//system( sys.c_str() );

	ewstring cameraDir = ewstring( "del " ) + ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"]) + ewstring("\\camera_logs\\*.jpg");
	system( cameraDir.c_str() );
			cameraDir = ewstring( "del " ) + ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"]) + ewstring("\\camera_logs\\*.pdf");
	system( cameraDir.c_str() );
	m_pLogger->Log( LOG_INFO, L"Clearing pictures - %s", cameraDir.getWString().c_str() );


	// WINSOCK --------------------------------------------------------------------------------------------------------------------------------
	m_pLogger->Log( LOG_INFO, L"WINSOCK" );
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {  }

	char ac[80];
    if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {	m_pLogger->Log( LOG_ERROR, L"WS - gethostname(ac, sizeof(ac)) - failed" );	   }
	m_pLogger->Log( LOG_INFO, L"WS - hostname : %s", DmString( ac ).c_str() );

    struct hostent *phe = gethostbyname(ac);
    if (phe == 0) {	  m_pLogger->Log( LOG_ERROR, L"WS - gethostbyname(ac) - failed" );	    }

    for (int i = 0; phe->h_addr_list[i] != 0; ++i) 
	{
        memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
		subnet.S_un.S_addr = 0x00ffffff;
		broadcastaddr.S_un.S_addr = addr.S_un.S_addr | (~ subnet.S_un.S_addr );
		m_pLogger->Log( LOG_INFO, L"WS - addr : %s", ewstring( inet_ntoa(addr) ).getWString().c_str() );
		m_pLogger->Log( LOG_INFO, L"WS - subn : %s", ewstring( inet_ntoa(subnet) ).getWString().c_str() );
		m_pLogger->Log( LOG_INFO, L"WS - brdc : %s", ewstring( inet_ntoa(broadcastaddr) ).getWString().c_str() );
		broadcast_addr.sin_addr = broadcastaddr;
		broadcast_addr.sin_family = AF_INET;
		broadcast_addr.sin_port = htons( ctrlPORT );
    }

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	char broadcast = 'a';
	if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
    {
		m_pLogger->Log( LOG_ERROR, L"WS - gsetsockopt(sock, SOL_SOCKET... - failed" );	 
        closesocket(sock);
    }

	//Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( ctrlPORT );
     
    //Bind
    if( bind(sock ,(struct sockaddr *)&server , sizeof(server)) == SOCKET_ERROR)
    {
		m_pLogger->Log( LOG_ERROR, L"WS - Bind failed with error code : %d" , WSAGetLastError() );	 
    }

	u_long iMode=1;
	ioctlsocket(sock,FIONBIO,&iMode);

	// tcpip -0----------------------------------------------
	{
		SOCKADDR_IN addr; // The address structure for a TCP socket
	    addr.sin_family = AF_INET;      // Address family
		addr.sin_port = htons (ctrlPORT+1);   // Assign port to this socket
		addr.sin_addr.s_addr = htonl (INADDR_ANY);  

		tcpSock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP); // Create socket
		if (tcpSock != INVALID_SOCKET)
		{
			if (bind(tcpSock, (LPSOCKADDR)&addr, sizeof(addr)) != SOCKET_ERROR)
			{
				listen(tcpSock, SOMAXCONN);
				unsigned long iMode = 1;
				int iResult = ioctlsocket(tcpSock, FIONBIO, &iMode);
			}
		}
		m_bSocketsOK = false;
	}

	// end .. WINSOCK -------------------------------------------------------------------------------------------------------------------------

	m_ActiveCamera = 0;		// FIXME 2 kamera verandering

	m_pLogger->UnTab();
	m_pLogger->Log( LOG_INFO, L"}" );
}



ctrl_DirectWeapons::~ctrl_DirectWeapons()
{
	OutputDebugString(L"ctrl_DirectWeapons::~ctrl_DirectWeapons()\n");
	m_pLogger->Log( LOG_INFO, L"ctrl_DirectWeapons::~ctrl_DirectWeapons()" );

	(*m_pCfgMng)["controller_DirectWeapons"].setConfig(L"TotalShots_LIVE", ewstring(m_TotalShotsLIVE).c_str() );
	(*m_pCfgMng)["controller_DirectWeapons"].setConfig(L"TotalShots_LASER", ewstring(m_TotalShotsLASER).c_str() );
	(*m_pCfgMng)["controller_DirectWeapons"].setConfig(L"LIVE_correction_DOWN", ewstring(m_correctionsDOWN).c_str() );
	(*m_pCfgMng)["controller_DirectWeapons"].setConfig(L"LIVE_correction_NONE", ewstring(m_correctionsNONE).c_str() );
	(*m_pCfgMng)["controller_DirectWeapons"].setConfig(L"LIVE_correction_UP", ewstring(m_correctionsUP).c_str() );
	if( !m_pCfgMng->saveConfig() ) m_pLogger->Log( LOG_ERROR, L"Saving Config file failed" );

	// WINSOCK --------------------------------------------------------------------------------------------------------------------------------
	closesocket( sock );
	closesocket( tcpSock );
    WSACleanup();
	// end .. WINSOCK -------------------------------------------------------------------------------------------------------------------------

	if (ZIGBEE.IsOpen())
	{
		zigbeeRounds( 0, 0, true );
		m_pLogger->Log( LOG_INFO, L"ZIGBEE about to close the PORT" );
		Sleep( 100 );
		ZIGBEE.ClosePort();
	}
	/*
	for (unsigned int i=0; i< m_lanes.size(); i++)
	{
		ewstring lane_str = "lane_" + ewstring( i );
		if (!m_pCfgMng->hasGroup(lane_str))				m_pCfgMng->addGroup(lane_str);
		(*m_pCfgMng)[lane_str].setConfig( "active_ammo", m_lanes[i].ammo.ammoIndex );
		(*m_pCfgMng)[lane_str].setConfig( "active_ammo_X", m_lanes[i].weaponOffset.x );
		(*m_pCfgMng)[lane_str].setConfig( "active_ammo_Y", m_lanes[i].weaponOffset.y );
	}
	(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig(L"menuPosition", DmString(m_AnimatedPosition).c_str() );

	m_pCfgMng->saveConfig();
	*/

	

	// LABJACK turn the fans off, leave lights and power -----------------------------------------------------------------------------------------------
	long id = -1;
	long trisD = 0x1f;
	long trisIO = 0x0;
	long stateD = 0;
		 stateD += (1<<0);						// lights on
		 stateD += (1<<4);										// power on
		 if (m_bLabjackTransnetShift)	stateD += (1<<3);						// power on line 3 at transnet only
	long stateIO = 0;
	long outputD = 0;
	DigitalIO( &id, 0, &trisD, trisIO, &stateD, &stateIO, 1, &outputD);

	DmWindow *_pWindow = m_pScene->getGUI_Window();
	dynamic_cast<DmPanel*>(_pWindow->findWidget(L"pnlBottomTools"))->clearWidgets();
	dynamic_cast<DmPanel*>(_pWindow->findWidget(L"pnl3D"))->clearWidgets();
	dynamic_cast<DmPanel*>(_pWindow->findWidget(L"pnlBottom"))->clearWidgets();
	dynamic_cast<DmPanel*>(_pWindow->findWidget( L"pnlBottomRadio" ))->clearWidgets();

	m_pMySQLManager->disconnect();
	SAFE_DELETE(m_pMySQLManager);

	for (int i = 0; i < 3; i++)
		m_KeyboardConnection[i].disconnect();

	// save all weapons lanes and offsets   -----------------------------------------------------------------------------
	/* FIXME imporve in future by moving this to teh weapon seletc code
	   will be nice if it cna remember both pistlos and rilfes for all lanes for Lephahalel for instance
	   but also have to remember last assigned weapon for each as well
	   */

	/*
	m_VoiceRecognitionConnection.disconnect();
	m_VoiceNoRec.disconnect();
	m_VoiceStartEnd.disconnect();
	m_VoiceAudio.disconnect();
	*/

	m_users.clear();
	m_lanes.clear();

	cleanupPrinting();


	SAFE_DELETE(m_targetManager);
}







void ctrl_DirectWeapons::setupPrinting()
{
	m_pLogger->Log( LOG_INFO, L"setupPrinting()" );
	m_bFirstPrint = true;

	/*
	PAGESETUPDLG ps;
    ZeroMemory(&ps, sizeof ps);
    ps.lStructSize = sizeof ps;
    ps.Flags = PSD_RETURNDEFAULT | PSD_INTHOUSANDTHSOFINCHES;
    PageSetupDlg(&ps);
    HANDLE hDevMode = ps.hDevMode;
    HANDLE hDevNames = ps.hDevNames;

    ((DEVMODE *)hDevMode)->dmPrintQuality = 300;

	memset(&m_printDlg, 0, sizeof(m_printDlg));
    m_printDlg.lStructSize = sizeof(m_printDlg);
    m_printDlg.hInstance = GetModuleHandle(NULL);
	m_printDlg.hwndOwner   = m_pScene->getDmWin32()->getMainHandle();
    m_printDlg.hDevMode    = hDevMode;
    m_printDlg.hDevNames   = hDevNames;
	m_printDlg.Flags       = PD_USEDEVMODECOPIESANDCOLLATE | PD_RETURNDC | PD_NOPAGENUMS; 
    m_printDlg.nCopies     = 1;
    //m_printDlg.nFromPage   = 0xFFFF; 
    //m_printDlg.nToPage     = 0xFFFF; 
    m_printDlg.nMinPage    = 1; 
    m_printDlg.nMaxPage    = 0xFFFF;
	*/

    LPPRINTPAGERANGE pPageRanges = NULL;

    // Allocate an array of PRINTPAGERANGE structures.
    pPageRanges = (LPPRINTPAGERANGE) GlobalAlloc(GPTR, 10 * sizeof(PRINTPAGERANGE));
    if (!pPageRanges)
        return;

	memset(&m_printDlg, 0, sizeof(m_printDlg));
    //  Initialize the PRINTDLGEX structure.
    m_printDlg.lStructSize = sizeof(PRINTDLGEX);
    m_printDlg.hwndOwner = m_pScene->getDmWin32()->getMainHandle();
    m_printDlg.hDevMode = NULL;
    m_printDlg.hDevNames = NULL;
    m_printDlg.hDC = NULL;
    m_printDlg.Flags = PD_RETURNDC | PD_COLLATE;
    m_printDlg.Flags2 = 0;
    m_printDlg.ExclusionFlags = 0;
    m_printDlg.nPageRanges = 0;
    m_printDlg.nMaxPageRanges = 10;
    m_printDlg.lpPageRanges = pPageRanges;
    m_printDlg.nMinPage = 1;
    m_printDlg.nMaxPage = 1000;
    m_printDlg.nCopies = 1;
    m_printDlg.hInstance = 0;
    m_printDlg.lpPrintTemplateName = NULL;
    m_printDlg.lpCallback = NULL;
    m_printDlg.nPropertyPages = 0;
    m_printDlg.lphPropertyPages = NULL;
    m_printDlg.nStartPage = START_PAGE_GENERAL;
    m_printDlg.dwResultAction = 0;
}


void ctrl_DirectWeapons::cleanupPrinting()
{
	DeleteDC(m_printDlg.hDC);
}


bool ctrl_DirectWeapons::checkLicense()
{
	m_serialNumber = 123456;
	m_bAuthorized = false;

	// Critical bit og CODE that GRANT uses to run EW
	#ifdef EW_BYPASS_SECURITY
	{
		m_bAuthorized = true;
	}	
	#else
	{
		if(!m_bAuthorized)
		{
			m_serialNumber = m_pScene->getPointGreySerial();
			if (m_bInstructorStation)
			{
				m_serialNumber = (*m_pCfgMng)["controller_DirectWeapons"]["InstructorStationSerial"].getInt();
				m_pLogger->Log( LOG_INFO, L"serial code INSTRUCTOR from InstructorStationSerial: %d",  m_serialNumber);
			}
			else
			{
				/*RakNet::BitStream Bs;
				Bs.Write( (unsigned short)RPC_SERIAL );
				Bs.Write( ewstring(m_serialNumber).c_str() );
				Bs.Write( "" );
				Bs.Write( "" );
				m_pScene->rpc_call( "rpc_CALL", &Bs );*/
			}
			m_pLogger->Log( LOG_INFO, L"serial code : %d",  m_serialNumber);
				
			// Scan all the lisences in the config file for a valid one -------------------------------------------------
			if (m_pCfgMng->hasGroup("License"))
			{
				ewstring licenseKey = (*m_pCfgMng)["License"]["LicenseKey"];
				char keys[1024];
				strcpy(keys, licenseKey.c_str());		// BUG lisence strign could grow big -------------------------------------------------
				char *tok = strtok(keys, ",");
				while (tok)
				{
					unsigned int LicenceKey = atoi( tok );
					if ( SEC_isValid( m_serialNumber, LicenceKey ) )
					{
						m_bAuthorized = true;
						m_LicensedLanes = SEC_getLanes( m_serialNumber, LicenceKey );
						m_pLogger->Log( LOG_INFO, L"this license allows %d lanes",  m_LicensedLanes);
					}
					tok = strtok(NULL, ",");
				}
			}

			// No valid lisence, try to install one from disk ----------------------------------------------------------
			if (!m_bAuthorized && !m_bInstructorStation)
			{
				MessageBox(NULL, L"No valid license found. Please open your licence file (.lic) to intall the license.", L"Error", MB_OK);

				wchar_t licPath[MAX_PATH];
				wchar_t licFilename[MAX_PATH];
				if (!DialogUtil::OpenDialog(licPath, MAX_PATH, licFilename, L"EarthWorks License file (*.lic)\0*.lic\0\0", L"ext", NULL))
				{
					MessageBox(NULL, L"No valid license found - please contact Dimension Software Engineering, www.kolskoot.net", L"Error", MB_OK);
					return false; //break;
				}
				else
				{
					CSVFile licFile;
					if (!licFile.openForRead(ewstring(licPath).c_str()))
					{
						MessageBox(NULL, L"Error reading license file", L"Error", MB_OK);
						return false; //continue;
					}

					if (!m_pCfgMng->hasGroup("License"))
						m_pCfgMng->addGroup("License");

					Util_ConfigManager::ConfigGroup &group = (*m_pCfgMng)["License"];

					string keys = "";
					while (licFile.read())
					{
						if (keys.length() > 0)
							keys += ",";
						keys += licFile[0].c_str();
					}

					group.setConfig("LicenseKey", keys);
					m_pCfgMng->saveConfig();

					licFile.close();
				}
			}
		}
	}
	#endif

	if (m_bAuthorized)
	{
		if ( m_pMySQLManager->isConnected() )
		{
			string query = "SELECT companyID, (SELECT name FROM company WHERE id = companyID) AS companyName FROM license WHERE serialNo = %1q:serialNo;";
			MySQLInput input;
			vector<mysqlpp::Row> result;
			input["serialNo"] = m_serialNumber;
		
			if (!m_pMySQLManager->isConnected() || !m_pMySQLManager->getObjects(query.c_str(), input, &result) || !result.size())
			{
				m_pLogger->Log( LOG_ERROR, L"This lisence is unregistered - reverting to Dimension Software Engineering demonstration database.",  result[0]["name"], m_iCompanyID);
				m_iCompanyID = 4;
				return m_bAuthorized;
			}

			m_iCompanyID = ewstring(result[0]["companyID"].c_str()).getUInt();
			m_pLogger->Log( LOG_INFO, L"Registered to %s (%d)",  ewstring(result[0]["companyName"].c_str()).getWString().c_str(), m_iCompanyID);

			loadUsers();
		}
		else
		{
			m_pLogger->Log( LOG_ERROR, L"SQL is not available - no database selected" );
			m_iCompanyID = -1;
			return m_bAuthorized;
		}
	}
	return m_bAuthorized;
}


void ctrl_DirectWeapons::registerLUA()
{
	m_pLUAManager = LUAManager::GetSingleton();
	m_pLUAManager->addResourcePath( ((*m_pCfgMng)["Earthworks_3"]["INSTALL_DIR"] + "/media/controllers/DirectWeapons3/").getWString().c_str() );
	m_pLUAManager->addResourcePath( ((*m_pCfgMng)["Earthworks_3"]["INSTALL_DIR"] + "/user_Scenes/").getWString().c_str() );
	//m_pLUAManager->addResourcePath( L"C:/DSE/Kolskoot/user_Scenes" );

	//m_pLUAManager->registerClassMethod( "fo_textToSpeech",						*this, &ctrl_Viewer::textToSpeech );
}


DmString cleanRank(DmString Rank)
{
	DmString ans = Rank;
	if (Rank.find(L"None") != wstring::npos) ans = L"";
	else if (Rank.find(L"NULL") != wstring::npos) ans = L"";
	else if (Rank.find(L"null") != wstring::npos) ans = L"";

	else if (Rank.find(L"General") != wstring::npos) ans = L"Gen ";
	else if (Rank.find(L"Brigadier") != wstring::npos) ans = L"Brig ";
	else if (Rank.find(L"Colonel") != wstring::npos) ans = L"Col ";
	else if (Rank.find(L"Major") != wstring::npos) ans = L"Maj ";
	else if (Rank.find(L"Captain") != wstring::npos) ans = L"Capt ";
	else if (Rank.find(L"Lieutenant") != wstring::npos) ans = L"Lt ";
	else if (Rank.find(L"Warrant") != wstring::npos) ans = L"WO ";
	else if (Rank.find(L"ergeant") != wstring::npos) ans = L"Sgt ";
	else if (Rank.find(L"Corporal") != wstring::npos) ans = L"Corp ";
	else if (Rank.find(L"ombardier") != wstring::npos) ans = L"Bmb ";
	else if (Rank.find(L"Private") != wstring::npos) ans = L"Pvt ";

	return ans;
}


DmString cleanString(DmString Input)
{
	DmString ans;
	if (Input.find(L"None") != wstring::npos) ans = L"";
	else if (Input.find(L"NULL") != wstring::npos) ans = L"";
	else if (Input.find(L"null") != wstring::npos) ans = L"";
	else ans = Input + L" ";

	return ans;
}


bool ctrl_DirectWeapons::loadUsers()
{
	m_pLogger->Log( LOG_INFO, L"loadUsers(..)");

	if (m_iCompanyID == -1)
		return true;

	//return false;
	if (m_pMySQLManager->isConnected())
	{
		vector<mysqlpp::Row> result;
		string query = "SELECT id, userID, initials, lastName, (SELECT name FROM rank WHERE id = rankID) AS rank, (SELECT name FROM unit WHERE id = unitID) AS unit, subUnit, (SELECT COALESCE(MAX(totalScore), 0) FROM exerciseSession AS es WHERE es.userID = u.id AND exerciseSessionDefinitionID = (SELECT id FROM exerciseSessionDefinition WHERE name = %1q:definitionName)) AS bestScore FROM user AS u WHERE companyID = %2q:companyID ORDER BY bestScore DESC;";
		MySQLInput input;
		input["definitionName"] = "IFSEC 2013 - 1 Lane";	// FIXME bietjei van 'n bug - ons laai spesifieke oefening se data ook
		input["companyID"] = m_iCompanyID;
		if ( m_pMySQLManager->getObjects(query.c_str(), input, &result) )
		{
			m_users.clear();

			for (unsigned int i = 0; i < result.size(); i++)
			{
				_user U;
				U.id = DmString(result[i]["id"].c_str());
				U.userID = DmString(result[i]["userID"].c_str());
				U.initials = cleanString(result[i]["initials"].c_str());
				U.surname = DmString(result[i]["lastName"].c_str());
				U.rank = cleanRank( result[i]["rank"].c_str() );
				U.unit = cleanString(result[i]["unit"].c_str());
				U.subunit = cleanString(result[i]["subUnit"].c_str());
				U.bestScore = result[i]["bestScore"];

				if (U.rank.size() > 0)	U.displayName = U.rank + L" " + U.initials + U.surname;
				else					U.displayName = U.initials + U.surname;
				U.score = 0;
				U.rank.toLower();
				U.initials.toLower();
				U.surname.toLower();
				U.id.toLower();
				U.unit.toLower();
				U.subunit.toLower();

				if (U.surname != L"")		m_users.push_back( U );
			}
		}


		saveUsersCSV();
	}
	else
	{
		if (!m_bMySQLError)
		{
			MessageBox(NULL, L"Error reading from database", L"Error", MB_OK);
			m_bMySQLError = true;
		}

		CSVFile csv;
		if (!csv.openForRead("users.csv"))
		{
			MessageBox(NULL, L"Error loading users from local disk", L"Error", MB_OK | MB_ICONERROR);
			return false;
		}


		m_users.clear();
		while (csv.read())
		{
			int csvIdx = 0;

			_user U;
			U.id = DmString(csv[csvIdx++].c_str());
			U.initials = cleanString(csv[csvIdx++].c_str());
			U.surname = DmString(csv[csvIdx++].c_str());
			U.rank = cleanRank( csv[csvIdx++].c_str() );
			U.unit = cleanString(csv[csvIdx++].c_str());
			U.subunit = cleanString(csv[csvIdx++].c_str());
			U.bestScore = atoi(csv[csvIdx++].c_str());

			if (U.rank.size() > 0)	U.displayName = U.rank + L" " + U.initials + U.surname;
			else					U.displayName = U.initials + U.surname;
			U.score = 0;
			U.rank.toLower();
			U.initials.toLower();
			U.surname.toLower();
			U.id.toLower();
			U.unit.toLower();
			U.subunit.toLower();

			if (U.surname != L"")		m_users.push_back( U );
		}

		csv.close();
	}
	
	return true;
}


void ctrl_DirectWeapons::saveUsersCSV()
{
	char tmp[32];
	CSVFile csv;
	bool csvOpen = false;
	if (!csv.openForWrite("users.csv", false))
		MessageBox(NULL, L"Error writing local users file", L"Error", MB_OK | MB_ICONERROR);
	else
	{
		for (std::list<_user>::iterator it = m_users.begin(); it != m_users.end(); it++)
		{
			int csvIdx = 0;
			csv[csvIdx++] = it->id.getMBString().c_str();
			csv[csvIdx++] = it->initials.getMBString().c_str();
			csv[csvIdx++] = it->surname.getMBString().c_str();
			csv[csvIdx++] = it->rank.getMBString().c_str();
			csv[csvIdx++] = it->unit.getMBString().c_str();
			csv[csvIdx++] = it->subunit.getMBString().c_str();
			itoa(it->bestScore, tmp, 10);
			csv[csvIdx++] = tmp;
			csv.write();
		}
		csv.close();
	}
}


void ctrl_DirectWeapons::buildGUI()
{
	m_pLogger->Log( LOG_INFO, L"ctrl_DirectWeapons::buildGUI()");
	m_pLogger->Tab();

	wstring mediaDir = ((*m_pCfgMng)["Earthworks_3"]["INSTALL_DIR"]).getWString() + L"/media/";
	DmWin32* pW32 = m_pScene->getDmWin32();
	DmGui *pGUI = m_pScene->getGUI();	

	

	m_KeyboardConnection[0] = DmConnect(m_pScene->getDmInput()->keyPress, *this, &ctrl_DirectWeapons::onKeyPress); 
	m_KeyboardConnection[1] = DmConnect(m_pScene->getDmInput()->keyUp, *this, &ctrl_DirectWeapons::onKeyUp); 
	m_KeyboardConnection[2] = DmConnect(m_pScene->getDmInput()->keyDown, *this, &ctrl_DirectWeapons::onKeyDown); 
	
	// Laai altyd engels eerste vir defaults  ---------------------------------------------------------------------
	DmSpriteManager& sprites = *m_pScene->getSpriteManager();
	sprites.loadImages(mediaDir + L"/gui/kolskoot/Menu/English", 0, 0, false, true);
	// Laai nou taal vanaf config  --------------------------------------------------------------------------------
	sprites.loadImages(mediaDir + L"/gui/kolskoot/Menu/" + (*m_pCfgMng)["Earthworks_3"]["Language"].getWString(), 0, 0, false, true);

	if ( DmWindow *_pWindow = m_pScene->getGUI_Window() )
	{
		// Get split screne mode and allocate rectangles  --------------------------------------------------------------------------------
		vector<DmMonitorInfo> MI = pW32->getMonitors();
		m_bSplitScreen = (int)(*m_pCfgMng)["controller_DirectWeapons"]["splitScreen"];
		bool bSwapScreen = (int)(*m_pCfgMng)["controller_DirectWeapons"]["swapScreen"];
		if( MI.size()==1 ) m_bSplitScreen=false;
		if (m_bSplitScreen)
		{
			if (bSwapScreen)
			{
				m_rect_3D = DmRect2D( (float)MI[0].rect.left, (float)MI[0].rect.top, (float)MI[0].rect.right, (float)MI[0].rect.bottom );
				m_rect_Menu = DmRect2D( (float)MI[MI.size()-1].rect.left, (float)MI[MI.size()-1].rect.top, (float)MI[MI.size()-1].rect.right, (float)MI[MI.size()-1].rect.bottom );
				m_pLogger->Log( LOG_INFO, L"Screen - 2 - SWAPPED  wh(%d, %d)  menu(%d, %d, %d, %d)  3D(%d, %d, %d, %d)",
											pW32->getClientWidth(), pW32->getClientHeight(),
											(int)m_rect_Menu.topLeft.x, (int)m_rect_Menu.topLeft.y, (int)m_rect_Menu.bottomRight.x, (int)m_rect_Menu.bottomRight.y, 
											(int)m_rect_3D.topLeft.x, (int)m_rect_3D.topLeft.y, (int)m_rect_3D.bottomRight.x, (int)m_rect_3D.bottomRight.y  );
			}
			else
			{
				m_rect_Menu = DmRect2D( (float)MI[0].rect.left, (float)MI[0].rect.top, (float)MI[0].rect.right, (float)MI[0].rect.bottom );
				m_rect_3D = DmRect2D( (float)MI[MI.size()-1].rect.left, (float)MI[MI.size()-1].rect.top, (float)MI[MI.size()-1].rect.right, (float)MI[MI.size()-1].rect.bottom );
				//int w = m_rect_3D.width();
				//m_rect_3D.topLeft.x = m_rect_Menu.bottomRight.x;
				//m_rect_3D.bottomRight.x = m_rect_3D.topLeft.x + w;
				m_pLogger->Log( LOG_INFO, L"Screen - 2 SCREENS  wh(%d, %d)  menu(%d, %d, %d, %d)  3D(%d, %d, %d, %d)",
											pW32->getClientWidth(), pW32->getClientHeight(),
											(int)m_rect_Menu.topLeft.x, (int)m_rect_Menu.topLeft.y, (int)m_rect_Menu.bottomRight.x, (int)m_rect_Menu.bottomRight.y, 
											(int)m_rect_3D.topLeft.x, (int)m_rect_3D.topLeft.y, (int)m_rect_3D.bottomRight.x, (int)m_rect_3D.bottomRight.y  );
			}
		}
		else
		{
			m_rect_Menu = DmRect2D( 0.0f, 0.0f, (float)pW32->getClientWidth(), (float)pW32->getClientHeight() );
			m_rect_3D = m_rect_Menu;
			m_pLogger->Log( LOG_INFO, L"Screen - SINGLE  w,h (%d, %d)",  pW32->getClientWidth(), pW32->getClientHeight() );
		}
		m_pScene->setViewport(m_rect_3D.topLeft.x, m_rect_3D.topLeft.y, m_rect_3D.width(), m_rect_3D.height() );

		


		// calculate perfect blocks for screen size  --------------------------------------------------------------------------------
		float hB =m_rect_Menu.height() / 7.0f;
		for (int i=8; i<32; i++ )
		{
			float wB = floor( m_rect_Menu.width() / i );
			if (wB <= hB)
			{
				m_MetroSpace = floor( wB / 8 );	
				m_MetroBlock = floor( wB - m_MetroSpace );
				break;
			}
		}
		

		dynamic_cast<DmPanel*>(_pWindow->findWidget(L"pnlTop"))->hide();
		dynamic_cast<DmPanel*>(_pWindow->findWidget(L"pnlBottom"))->hide();
		if( DmPanel *p3D = dynamic_cast<DmPanel*>(_pWindow->findWidget(L"pnl3D")) )
		{
			p3D->setPosition( 0.0f, 0.0f );
			p3D->setSize( (float)pW32->getClientWidth(), (float)pW32->getClientHeight() );

			// MENU --------------------------------------------------------------------------------------------------------------
			//if (m_bSplitScreen)
			{
				m_pMenu3D = &p3D->addWidget<DmPanel>( L"liveBackdrop", L"",  m_rect_3D.topLeft.x, m_rect_3D.topLeft.y, m_rect_3D.bottomRight.x, m_rect_3D.bottomRight.y, L"metro_darkblue" );
				m_pMenu3D->addWidget<DmPanel>( L"LiveLogo", L"", m_rect_3D.midX()-400-m_rect_3D.topLeft.x, m_rect_3D.midY()-100-m_rect_3D.topLeft.y, m_rect_3D.midX()+400-m_rect_3D.topLeft.x, m_rect_3D.midY()+100-m_rect_3D.topLeft.y, L"Splash" );
				if(m_bLiveFire)
				{
					m_pMenu3D->setStyleName( L"metro_darkred" );
					//m_pMenu3D->setText( L"live fire" );
				}
			}
			m_pMenu = &p3D->addWidget<DmPanel>( L"menu", L"",  m_rect_Menu.topLeft.x, m_rect_Menu.topLeft.y, m_rect_Menu.bottomRight.x, m_rect_Menu.bottomRight.y, L"metro_darkblue" );
			//if (m_bInstructorStation) m_pMenu->setStyleName( L"metro_cyan" );
			buildGUI_menu();

			
			
			//CAMERA  -----------------------------------------------------------------------------------------------------------
			m_pCam_Intensity = &p3D->addWidget<DmPanel>( L"CamZoomIntensity", L"", L"metro_darkblue" );
			m_pCam_ZoomPanFocus = &p3D->addWidget<DmPanel>( L"CamZoomPanFocus", L"", L"metro_darkblue" );
			m_pCam_Distortion = &p3D->addWidget<DmPanel>( L"CamDistortion", L"", L"metro_darkblue" );
			
			buildGUI_camera();

			// m_pQuickScenario --------------------------------------------------------------------------------------------------------------
			QuickRange.buildGUI( p3D, m_rect_Menu, m_MetroBlock, m_MetroSpace, this );

			// m_pScreenSetup --------------------------------------------------------------------------------------------------------------
			m_pScreenSetup = &p3D->addWidget<DmPanel>( L"ScreenSetup", L"", m_rect_Menu.topLeft.x, m_rect_Menu.topLeft.y, m_rect_Menu.bottomRight.x, m_rect_Menu.bottomRight.y, L"metro_darkblue" );
			DmPanel *pI = &m_pScreenSetup->addWidget<DmPanel>( L"Image", L"", L"blank_image" );

			pGUI->loadFragment( *m_pScreenSetup, mediaDir + L"/gui/Gui/frag_ScreenSetup.xml");
			m_pScreenSetup->findWidget( L"SS_lanes" )->setText( DmString((*m_pCfgMng)["controller_DirectWeapons"]["lanes"].c_str()) );
			m_pScreenSetup->findWidget( L"SS_width" )->setText( DmString((*m_pCfgMng)["controller_DirectWeapons"]["screen_width"].c_str()) );
			m_pScreenSetup->findWidget( L"SS_distance" )->setText( DmString((*m_pCfgMng)["controller_DirectWeapons"]["shooting_distance"].c_str()) );
			float DST = m_pScreenSetup->findWidget( L"SS_distance" )->getText().toFloat() / 1000.0f;
			m_pMenu->findWidget( L"Distance" ) ->setText( DmString(DST, 1) + L" m" );

			float w = m_rect_Menu.width();
			float h = m_rect_Menu.height();
			float ico = m_MetroBlock * 0.5;
			DmPanel *pP = &pI->addWidget<DmPanel>( L"menuico", L"", w-ico*2, h-ico*2, w-ico, h-ico, L"" );
			pP->setValue( L"icon_menu" );			
			DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
			pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
			pP->setCommandText( L"menu" );
			DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

			// nuew ammo en naam kies skerm -----------------------------------------------------------------------------------------------------------------------
			m_pLogger->Log( LOG_INFO, L"buildGUIammo(..)");
			buildGUIammo();

			// live --------------------------------------------------------------------------------------------------------------
			m_pLive = &p3D->addWidget<DmPanel>( L"live", L"" );
			m_pLogger->Log( LOG_INFO, L"buildGUI_live(..)");
			buildGUI_live();

			//ADD THIS TO MENU
			// framerate debug --------------------------------------------------------------------------------------------------------------
			m_pFramerateInfo= &p3D->addWidget<DmPanel>( L"framerate", L"framerate", L"metro_info" );
			m_pFramerateInfo->setPosition(m_rect_Menu.topLeft.x + m_rect_Menu.width() - 300, m_rect_Menu.height() - m_MetroBlock/4);
			m_pFramerateInfo->setSize( 300, m_MetroBlock/4 );
			for (int c=0; c<5; c++)
			{
				m_FramerateBuffer[c] = 0;
				m_FramerateCounter[c] = 20;
			}
			
			//Messagebox  -----------------------------------------------------------------------------------------------------------
			m_pMessagebox = &p3D->addWidget<DmPanel>( L"messagebox", L"",  m_rect_Menu.topLeft.x, m_rect_Menu.topLeft.y, m_rect_Menu.bottomRight.x, m_rect_Menu.bottomRight.y, L"metro_darkblue_semi" );
			buildGUI_messageBox();

		}
	}

	m_pLogger->UnTab();
	m_pLogger->Log( LOG_INFO, L"}");
}

void ctrl_DirectWeapons::buildGUI_menu()
{
	DmPanel *m_pTxt= &m_pMenu->addWidget<DmPanel>( L"Copyrigth", COPYRIGHT, L"Copyright" );
	m_pTxt->setPosition (m_MetroBlock/2, m_rect_Menu.height() - m_MetroBlock/2);
	m_pTxt->setSize (800, m_MetroBlock/4);
	m_pTxt->setHorizontalTextAlignment( dmAlignLeft );

	m_pTxt= &m_pMenu->addWidget<DmPanel>( L"Version", VERSION, L"Copyright" );
	m_pTxt->setPosition (m_MetroBlock/2, m_rect_Menu.height() - m_MetroBlock/4);
	m_pTxt->setSize (800, m_MetroBlock/4);
	m_pTxt->setHorizontalTextAlignment( dmAlignLeft );

	m_pTxt= &m_pMenu->addWidget<DmPanel>( L"Logo", L"", L"Splash" );
	m_pTxt->setPosition (m_rect_Menu.width()/2-400, m_rect_Menu.height()/2-100);
	m_pTxt->setSize (800, 200);

	

	

	m_pMenuSlide= &m_pMenu->addWidget<DmPanel>( L"menuSlide", L"", L"empty" );
	m_MenuWidth = m_rect_Menu.width();
	m_MenuPage = 0;
	m_MenuNewPage = 0;
	m_AnimatedPosition = (*m_pCfgMng)["controller_DirectWeapons"]["menuPosition"];;
	m_pMenuSlide->setWidth(m_MenuWidth * 10.0f);
	m_pMenuSlide->setPosition( m_AnimatedPosition, 96 );
	DmConnect( m_pMenuSlide->mouseDrag, *this, &ctrl_DirectWeapons::onMouseDrag3D );

	float topBAR = __max(64 , m_rect_Menu.width() / 24);
	// line --------------------------------------------------------------------------------------------------------------
		DmPanel *pLine = &m_pMenu->addWidget<DmPanel>( L"Line", L"", L"white" );
		pLine->setPosition(0, topBAR + 10);
		pLine->setSize( m_rect_Menu.width(), 1 );
				
	// exit --------------------------------------------------------------------------------------------------------------
		DmPanel *pExit = &m_pMenu->addWidget<DmPanel>( L"Exit", L"", L"empty" );
		pExit->setPosition(m_rect_Menu.width()-topBAR, 0);
		pExit->setSize( topBAR, topBAR );
		pExit->setCommandText( L"exit" );
		DmConnect( pExit->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
		pExit->setValue( L"icon_quit" );			
		DmConnect( pExit->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );

	// settings --------------------------------------------------------------------------------------------------------------
		DmPanel *pSettings = &m_pMenu->addWidget<DmPanel>( L"Setting", L"", L"empty" );
		pSettings->setPosition(0, 0);
		pSettings->setSize( topBAR, topBAR );
		DmConnect( pSettings->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
		pSettings->setValue( L"icon_settings" );			
		DmConnect( pSettings->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		DmPanel *pO1 = &pSettings->addWidget<DmPanel>( L"1", L"", L"metro_border" );
		pO1->setCommandText( L"Settings" );

	// camera --------------------------------------------------------------------------------------------------------------
		DmPanel *pCam = &m_pMenu->addWidget<DmPanel>( L"Cam", L"", L"empty" );
		pCam->setPosition(topBAR, 0);
		pCam->setSize( topBAR, topBAR );
		pCam->setValue( L"icon_camera" );			
		DmConnect( pCam->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		//DmConnect( pCam->mouseOver, *this, &ctrl_DirectWeapons::camera_onMouseOver );
		//DmConnect( pCam->mouseOut, *this, &ctrl_DirectWeapons::camera_onMouseOut );
		DmPanel *pO2 = &pCam->addWidget<DmPanel>( L"1", L"", L"metro_border" );
		pO2->setCommandText( L"Cam_Intensity" );
		DmConnect( pO2->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

		m_pCamView = &m_pMenu->addWidget<DmPanel>( L"CamvIEW", L"", 0, 96, m_rect_Menu.width(), m_rect_Menu.height(), L"metro_darkblue_semi" );
					m_pCamView->addWidget<DmPanel>( L"CamvIEW", L"", (m_rect_Menu.width()-752)/2, (m_rect_Menu.height()-480)/2, (m_rect_Menu.width()+752)/2, (m_rect_Menu.height()+480)/2,  L"video_placeholder" );
					//DmPanel *pP2 = &m_pCamView->addWidget<DmPanel>( L"CamvIEW", L"",  L"video_placeholder" );
					//pP2->setSize( 752, 480 );
					//pP2->setLayout( 0, 0, 0, 0 );
		m_pCamView->hide();

	// distance --------------------------------------------------------------------------------------------------------------
		DmPanel *pDist = &m_pMenu->addWidget<DmPanel>( L"Distance", L"10 m", L"Edit_30" );
		pDist->setPosition(topBAR*2, 0);
		pDist->setSize( topBAR*2, topBAR );
		DmConnect( pDist->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
		pDist->setValue( L"icon_distance" );			
		DmConnect( pDist->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		DmPanel *pO3 = &pDist->addWidget<DmPanel>( L"1", L"", L"metro_border" );
		pO3->setCommandText( L"Layout" );
		pO3->setCommandText( L"setup_screen" );
		DmConnect( pO3->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

	// laser --------------------------------------------------------------------------------------------------------------
		DmPanel *pLaser = &m_pMenu->addWidget<DmPanel>( L"LaserLive", L"", 320, 0, 400, 80, L"empty" );	
		pLaser->setPosition(topBAR*4, 0);
		pLaser->setSize( topBAR, topBAR );
		DmConnect( pLaser->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		pLaser->setValue( L"icon_IRLaser" );
		if( isLiveFire() ) pLaser->setValue( L"icon_LIVE" );
		//DmPanel *pO4 = &pLaser->addWidget<DmPanel>( L"1", L"", L"metro_border" );
		//pO4->setCommandText( L"Laser" );
		//DmConnect( pO4->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
		


	// lanes --------------------------------------------------------------------------------------------------------------
		float links = topBAR*6;
		float regs = m_rect_Menu.width() - topBAR;
		float wydte = (regs - links) /m_lanes.size();	
		for (unsigned int i=0; i<m_lanes.size(); i++)
		{
			DmPanel *pLn = &m_pMenu->addWidget<DmPanel>( L"Lane"+DmString(i), DmString(m_LaneZero+i+1), L"Edit_48" );
			pLn->setPosition(links+i*wydte, 5);
			pLn->setSize( wydte, topBAR );
			//pLn->setHorizontalTextAlignment( dmAlignLeft );

			pLn = &m_pMenu->addWidget<DmPanel>( L"LaneName"+DmString(i), L"", L"metro_info" );
			pLn->setPosition(links+topBAR/2+i*wydte, 5);
			pLn->setSize( wydte-topBAR/2, topBAR/2 );
			//pLn->setHorizontalTextAlignment(dmAlignLeft);
			//pLn->setHorizontalTextMargin(10);
			pLn->setVerticalTextAlignment(dmAlignTop);
			pLn->setPosition(links+i*wydte, 5);
			pLn->setSize( wydte, topBAR );

			pLn = &m_pMenu->addWidget<DmPanel>( L"LaneAmmo"+DmString(i), L"", L"metro_info" );
			pLn->setPosition(links+topBAR/2+i*wydte, topBAR/2+5);
			pLn->setSize( wydte-topBAR/2, topBAR/2 );
			//pLn->setHorizontalTextAlignment(dmAlignLeft);
			//pLn->setHorizontalTextMargin(10);
			pLn->setVerticalTextAlignment(dmAlignBottom);
			pLn->setPosition(links+i*wydte, 5);
			pLn->setSize( wydte, topBAR );

			pLn = &m_pMenu->addWidget<DmPanel>( L"Lane_OVL"+DmString(i), L"", L"metro_border" );
			pLn->setPosition(links+i*wydte, 5);
			pLn->setSize( wydte, topBAR );
			pLn->setCommandText("showAmmoScreen");
			pLn->setValue( DmString(i) );
			DmConnect( pLn->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );
		}
	




		// playlists --------------------------------------------------------------------------------------------------------------
		DmPanel *pList = &m_pMenu->addWidget<DmPanel>( L"List", L"", L"empty" );
		pList->setPosition(topBAR*5, 0);
		pList->setSize( topBAR, topBAR );
		pList->setValue( L"icon_playlist" );			
		DmConnect( pList->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		//DmConnect( pList->mouseOver, *this, &ctrl_DirectWeapons::camera_onMouseOver );
		//DmConnect( pList->mouseOut, *this, &ctrl_DirectWeapons::camera_onMouseOut );
		DmPanel *pO5 = &pList->addWidget<DmPanel>( L"1", L"", L"metro_border" );
		pO5->setCommandText( L"Playlist" );
		DmConnect( pO5->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

		m_PLaylistNumLanes = m_Scenario.m_pc_lanes;

		m_pPlaylist = &m_pMenu->addWidget<DmPanel>( L"Playlist", L"", 0, 0, m_rect_Menu.width(), m_rect_Menu.height(), L"metro_darkblue" );
		m_pPlaylist->hide();

		DmPanel *pList2 = &m_pPlaylist->addWidget<DmPanel>( L"List", L"", L"empty" );
		pList2->setPosition(0, 0);
		pList2->setSize( topBAR, topBAR );
		pList2->setValue( L"icon_playlist" );			
		DmConnect( pList2->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );

		float w = m_rect_Menu.width();
		float h = m_rect_Menu.height();
		float ico = m_MetroBlock * 0.5;
		DmPanel *pP = &m_pPlaylist->addWidget<DmPanel>( L"menuico", L"", w-ico*2, h-ico*2, w-ico, h-ico, L"" );
		pP->setValue( L"icon_menu" );			
		DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
		pP->setCommandText( L"menu" );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );


		pList2 = &m_pPlaylist->addWidget<DmPanel>( L"List2", L"Playlist", L"Edit_48" );
		pList2->setPosition(topBAR*2, 0);
		pList2->setSize( 200, topBAR );
		pList2->setHorizontalTextAlignment( dmAlignLeft );

				pP = &m_pPlaylist->addWidget<DmPanel>( L"load", L"", m_MetroBlock*0.5, h-m_MetroBlock*1.0, m_MetroBlock*1.0, h-m_MetroBlock*0.5, L"Edit_48" );
				pP = &pP->addWidget<DmPanel>( L"nextB", L"", L"metro_border" );
				pP->setCommandText( L"playlistLoad" );
				DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
				pP->setValue( L"icon_load" );			
				DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );

				pP = &m_pPlaylist->addWidget<DmPanel>( L"save", L"", m_MetroBlock*1.5, h-m_MetroBlock*1.0, m_MetroBlock*2.0, h-m_MetroBlock*0.5, L"Edit_48" );
				pP = &pP->addWidget<DmPanel>( L"nextB", L"", L"metro_border" );
				pP->setCommandText( L"playlistSave" );
				DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
				pP->setValue( L"icon_save" );			
				DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );

				pP = &m_pPlaylist->addWidget<DmPanel>( L"clear", L"", m_MetroBlock*2.5, h-m_MetroBlock*1.0, m_MetroBlock*3.0, h-m_MetroBlock*0.5, L"Edit_48" );
				pP = &pP->addWidget<DmPanel>( L"nextB", L"", L"metro_border" );
				pP->setCommandText( L"playlistClear" );
				DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
				pP->setValue( L"icon_delete" );			
				DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );

				pP = &m_pPlaylist->addWidget<DmPanel>( L"printPL", L"", m_MetroBlock*3.5, h-m_MetroBlock*1.0, m_MetroBlock*4.0, h-m_MetroBlock*0.5, L"Edit_48" );
				pP = &pP->addWidget<DmPanel>( L"nextB", L"", L"metro_border" );
				pP->setCommandText( L"playlistPrint" );
				DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
				pP->setValue( L"icon_print" );			
				DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );


				

				

				m_pPlaylistSearch = &m_pPlaylist->addWidget<DmPanel>( L"search", L"", m_MetroBlock*4.5, h-m_MetroBlock*1.0, w-m_MetroBlock*2.5, h-m_MetroBlock*0.5, L"block_yellow" );
				pP = &m_pPlaylist->addWidget<DmPanel>( L"add", L"Add", w-m_MetroBlock*2.3, h-m_MetroBlock*1.0, w-m_MetroBlock*1.5, h-m_MetroBlock*0.5, L"Edit_48" );
				pP = &pP->addWidget<DmPanel>( L"nextB", L"", L"metro_border" );
				pP->setCommandText( L"playlistAdd" );
				DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

				pP = &m_pPlaylist->addWidget<DmPanel>( L"print", L"", w-m_MetroBlock*1.3, m_MetroBlock*0.0, w-m_MetroBlock*0.5, m_MetroBlock*0.8, L"metro_info" );
				pP->setValue( L"icon_autoprintOFF" );			
				DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
				pP->setVerticalTextAlignment( dmAlignBottom );
				pP = &pP->addWidget<DmPanel>( L"nextB", L"", L"metro_border" );
				pP->setCommandText( L"playlistTogglePrint" );
				DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
				


		{
			float links = topBAR*1;
			float regs = m_rect_Menu.width();// - topBAR*2;
			float wydte = (regs - links) /m_lanes.size();	
			float bo = 200;
			float hoogte = (h - 400) / 10;

			for (int l=0; l<10; l++)
			{
				pP = &m_pPlaylist->addWidget<DmPanel>( L"currentLane"+DmString(l), DmString(l+1), 0, bo, regs, bo + hoogte, L"metro_border" );
				pP->setCommandText( L"playlistClickRow" );
				pP->setHelpText( DmString(l) );
				pP->setHorizontalTextAlignment( dmAlignLeft );
				DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

				for (int k=0; k<m_PLaylistNumLanes; k++)
				{
					pP = &m_pPlaylist->addWidget<DmPanel>( L"X"+DmString(l)+L"_"+DmString(k), L"X", links + wydte*k, bo, links+wydte*(k)+10, bo + hoogte, L"Edit_30" );
					pP->setCommandText( L"playlistClickX" );
					pP->setValue( DmString( l*m_PLaylistNumLanes + k ) );
					DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );


					pP = &m_pPlaylist->addWidget<DmPanel>( L"user"+DmString(l)+L"_"+DmString(k), L"", links + wydte*k + 10, bo, links+wydte*(k+1), bo + hoogte, L"metro_info" );
					pP->setHorizontalTextAlignment( dmAlignLeft );
					//pP->setHorizontalTextMargin( 30 );
				}
				bo += hoogte;
			}
		}


}


void ctrl_DirectWeapons::buildGUI_lanes()
{

}


void ctrl_DirectWeapons::CamChange(DmWidget& sender, const DmString& text)
{
	(*m_pCfgMng)[L"Camera"].setConfig( "threshold", m_pCam_Thresh->getText() );
	(*m_pCfgMng)[L"Camera"].setConfig( "dot_min", m_pCam_Min->getText() );
	(*m_pCfgMng)[L"Camera"].setConfig( "dot_max", m_pCam_Max->getText() );
	(*m_pCfgMng)[L"Camera"].setConfig( "framerate", m_pCam_Framerate->getText() );
	(*m_pCfgMng)[L"Camera"].setConfig( "gain", m_pCam_Gain->getText() );
	(*m_pCfgMng)[L"Camera"].setConfig( "gamma", m_pCam_Gamma->getText() );

	m_pScene->setDots( m_pCam_Thresh->getText().toFloat(), m_pCam_Min->getText().toFloat(), m_pCam_Max->getText().toFloat(), m_pScene->m_PG_dot_position );
	m_pScene->setPointGreyParams( m_pCam_Gamma->getText().toFloat(), m_pCam_Framerate->getText().toFloat(), m_pCam_Gain->getText().toFloat() );
}


void ctrl_DirectWeapons::buildGUI_camera()
{
	

	// Camera Intensity --------------------------------------------------------------------------------------------------------------
	{	
		float w = m_rect_Menu.width();
		float h = m_rect_Menu.height();
		float ico = m_MetroBlock * 0.5;

		DmPanel *p3D = &m_pCam_Intensity->addWidget<DmPanel>( L"vid", L"", m_rect_3D.topLeft.x, m_rect_3D.topLeft.y, m_rect_3D.bottomRight.x, m_rect_3D.bottomRight.y, L"video_placeholder" );
		DmPanel *gui = &m_pCam_Intensity->addWidget<DmPanel>( L"gui", L"", m_rect_Menu.topLeft.x, m_rect_Menu.topLeft.y, m_rect_Menu.bottomRight.x, m_rect_Menu.bottomRight.y, L"empty" );
		if (m_bSplitScreen) 
		{
			DmPanel *pVidB = &gui->addWidget<DmPanel>( L"vidB", L"", L"video_placeholder" );
			//pVidB->setSize( h*0.7f*752/480, h*0.7f );
			//pVidB->setPosition( m_MetroBlock, m_MetroBlock );
		}

		DmPanel *pMouseDrag = &gui->addWidget<DmPanel>( L"MouseDrag", L"", L"empty" );
		DmConnect( pMouseDrag->mouseDrag, *this, &ctrl_DirectWeapons::onMouseDragCameraAdjust );

		m_pCam_IR = &p3D->addWidget<DmPanel>( L"CamIR", L"", L"empty" );
		{
			float dX = (m_rect_3D.width() - 40) / 2;
			float dY = (m_rect_3D.height()-40) / 2;
			for(int x=0; x<3; x++)
			{
				for (int y=0; y<3; y++)
				{
					int pX = 20 + x*dX;
					int pY = 20 + y*dY;
					m_pCam_IR_grid[y][x] = &m_pCam_IR->addWidget<DmPanel>( DmString(x) + DmString(y), L"test", pX-20, pY-20, pX+120, pY+20, L"Edit_30" );
					m_pCam_IR_grid[y][x]->setHorizontalTextAlignment( dmAlignLeft );
					//distortion_grid[y][x].screen = f_Vector( m_rect_3D.topLeft.x + pX, m_rect_3D.topLeft.y + pY, 0 );
				}
			}

			m_pCam_IR_info = &m_pCam_IR->addWidget<DmPanel>( "info", L"test", 50, 50, 950, 80, L"Edit_30" );
		}
		 m_pCam_IR->hide(); 

		// icons at the bottom   ----------------------------------------------------------------------------------------------
		DmPanel *pP = &gui->addWidget<DmPanel>( L"param", L"", ico, h-ico*2, ico*2, h-ico, L"" );
		pP->setValue( L"icon_camera" );			
		DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
		pP->setCommandText( L"toggle_cam_info" );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

		pP = &gui->addWidget<DmPanel>( L"grid", L"", ico*3, h-ico*2, ico*4, h-ico, L"" );
		pP->setValue( L"icon_grid" );			
		DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
		pP->setCommandText( L"setup_camera" );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

		pP = &gui->addWidget<DmPanel>( L"Xf", L"", ico*5, h-ico*2, ico*6, h-ico, L"" );
		pP->setValue( L"icon_X" );			
		DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
		pP->setCommandText( L"Cam_IR1" );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

		pP = &gui->addWidget<DmPanel>( L"XXf", L"", ico*7, h-ico*2, ico*8, h-ico, L"" );
		pP->setValue( L"icon_XX" );			
		DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
		pP->setCommandText( L"Cam_IR2" );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );


		pP = &gui->addWidget<DmPanel>( L"paper", L"", ico*9, h-ico*2, ico*10, h-ico, L"" );
		pP->setValue( L"icon_papermove" );			
		DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
		pP->setCommandText( L"PaperAdvance" );
		pP->setValue( L"20" );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

		pP = &gui->addWidget<DmPanel>( L"menuico", L"", w-ico*2, h-ico*2, w-ico, h-ico, L"" );
		pP->setValue( L"icon_menu" );			
		DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
		pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
		pP->setCommandText( L"menu" );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );


		for (int i=0; i<20; i++)
		{
			m_pCam_Skoot[i] = &m_pCam_Intensity->addWidget<DmPanel>( L"kol"+ DmString(i), L"", 0-15, 0-15, 0+15, 0+15, L"bulletX" );
			m_pCam_SkootInfo[i] = &m_pCam_Intensity->addWidget<DmPanel>( L"inf"+ DmString(i), L"", 0-15, 0-15, 0+40, 0+40, L"Edit_30" );
		}


		m_pCam_Info = &m_pCam_Intensity->addWidget<DmPanel>( L"info", L"",  0, h - ico*2-240,  ico + 400, h - ico*2, L"empty" );
		m_pCam_Info->hide();
		{
			pP = &m_pCam_Info->addWidget<DmPanel>( L"a1", L"threshold:",	0, 0, 100, 30, L"metro_info" );
			pP->setHorizontalTextAlignment( dmAlignRight );
			pP = &m_pCam_Info->addWidget<DmPanel>( L"a2", L"dot min:",		0, 30, 100, 60, L"metro_info" );
			pP->setHorizontalTextAlignment( dmAlignRight );
			pP = &m_pCam_Info->addWidget<DmPanel>( L"a3", L"dot max:",		0, 60, 100, 90, L"metro_info" );
			pP->setHorizontalTextAlignment( dmAlignRight );
			pP = &m_pCam_Info->addWidget<DmPanel>( L"a4", L"gain:",			0, 90, 100, 120, L"metro_info" );
			pP->setHorizontalTextAlignment( dmAlignRight );
			pP = &m_pCam_Info->addWidget<DmPanel>( L"a5", L"gamma:",		0, 120, 100, 150, L"metro_info"  );
			pP->setHorizontalTextAlignment( dmAlignRight );
			pP = &m_pCam_Info->addWidget<DmPanel>( L"a6", L"framerate:",	0, 150, 100, 180, L"metro_info" );
			pP->setHorizontalTextAlignment( dmAlignRight );
			pP = &m_pCam_Info->addWidget<DmPanel>( L"a7", L"resolution:",	0, 180, 100, 210, L"metro_info" );
			pP->setHorizontalTextAlignment( dmAlignRight );
			pP = &m_pCam_Info->addWidget<DmPanel>( L"a8", L"serial:",		0, 210, 100, 240, L"metro_info" );
			pP->setHorizontalTextAlignment( dmAlignRight );

			m_pCam_Thresh = &m_pCam_Info->addWidget<DmNumeric>( L"thresh", L"", 110, 0, 200, 30, L"Edit_30" );
			m_pCam_Thresh->setMinVal( 4 );
			m_pCam_Thresh->setMaxVal( 128 );
			m_pCam_Thresh->setMouseWheelChangeValue( 1.0 );
			m_pCam_Thresh->setHorizontalTextAlignment( dmAlignLeft );

			m_pCam_Min = &m_pCam_Info->addWidget<DmNumeric>( L"min", L"", 110, 30, 200, 60, L"Edit_30" );
			m_pCam_Min->setMinVal( 1 );
			m_pCam_Min->setMaxVal( 100 );
			m_pCam_Min->setMouseWheelChangeValue( 1.0 );
			m_pCam_Min->setHorizontalTextAlignment( dmAlignLeft );

			m_pCam_Max = &m_pCam_Info->addWidget<DmNumeric>( L"max", L"", 110, 60, 200, 90, L"Edit_30" );
			m_pCam_Max->setMinVal( 30 );
			m_pCam_Max->setMaxVal( 1000 );
			m_pCam_Max->setMouseWheelChangeValue( 1.0 );
			m_pCam_Max->setHorizontalTextAlignment( dmAlignLeft );

			m_pCam_Gain = &m_pCam_Info->addWidget<DmNumeric>( L"gain", L"", 110, 90, 200, 120, L"Edit_30" );
			m_pCam_Gain->setMinVal( 0.1 );
			m_pCam_Gain->setMaxVal( 15.0 );
			m_pCam_Gain->setMouseWheelChangeValue( 0.1 );
			m_pCam_Gain->setPrecision(1);
			m_pCam_Gain->setHorizontalTextAlignment( dmAlignLeft );

			m_pCam_Gamma = &m_pCam_Info->addWidget<DmNumeric>( L"gamma", L"", 110, 120, 200, 150, L"Edit_30" );
			m_pCam_Gamma->setMinVal( 0 );
			m_pCam_Gamma->setMaxVal( 1 );
			m_pCam_Gamma->setMouseWheelChangeValue( 1 );
			m_pCam_Gamma->setPrecision(0);
			m_pCam_Gamma->setHorizontalTextAlignment( dmAlignLeft );

			m_pCam_Framerate = &m_pCam_Info->addWidget<DmNumeric>( L"framerate", L"", 110, 150, 200, 180, L"Edit_30" );
			m_pCam_Framerate->setMinVal( 30 );
			m_pCam_Framerate->setMaxVal( 60 );
			m_pCam_Framerate->setMouseWheelChangeValue( 1 );
			m_pCam_Framerate->setHorizontalTextAlignment( dmAlignLeft );

			m_pCam_Resolution = &m_pCam_Info->addWidget<DmPanel>( L"resolution", L"", 110, 180, 200, 210, L"Edit_30" );
			m_pCam_Resolution->setHorizontalTextAlignment( dmAlignLeft );
			m_pCam_Resolution->setText( ((*m_pCfgMng)[L"Camera"][L"width"]).getWString() + L" x " + ((*m_pCfgMng)[L"Camera"][L"height"]).getWString() );

			m_pCam_Serial = &m_pCam_Info->addWidget<DmPanel>( L"serial", L"", 110, 210, 200, 240, L"Edit_30" );
			m_pCam_Serial->setHorizontalTextAlignment( dmAlignLeft );
			m_pCam_Serial->setText( DmString(m_serialNumber) );

			m_pCam_Thresh->setText( ((*m_pCfgMng)[L"Camera"][L"threshold"]).getWString() );
			m_pCam_Min->setText( ((*m_pCfgMng)[L"Camera"][L"dot_min"]).getWString() );
			m_pCam_Max->setText( ((*m_pCfgMng)[L"Camera"][L"dot_max"]).getWString() );

			m_pCam_Framerate->setText( ((*m_pCfgMng)[L"Camera"][L"framerate"]).getWString() );
			m_pCam_Gain->setText( ((*m_pCfgMng)[L"Camera"][L"gain"]).getWString() );
			m_pCam_Gamma->setText( ((*m_pCfgMng)[L"Camera"][L"gamma"]).getWString() );
		
			DmConnect( m_pCam_Thresh->textChange, this, &ctrl_DirectWeapons::CamChange);
			DmConnect( m_pCam_Min->textChange, this, &ctrl_DirectWeapons::CamChange);
			DmConnect( m_pCam_Max->textChange, this, &ctrl_DirectWeapons::CamChange);

			DmConnect( m_pCam_Framerate->textChange, this, &ctrl_DirectWeapons::CamChange);
			DmConnect( m_pCam_Gain->textChange, this, &ctrl_DirectWeapons::CamChange);
			DmConnect( m_pCam_Gamma->textChange, this, &ctrl_DirectWeapons::CamChange);
		}

		

	}

	

	

	// Camera Zoom pan focus --------------------------------------------------------------------------------------------------------------
	{
		float w = m_rect_3D.width();
		float h = m_rect_3D.height();
		float vW = w / 1.55;
		float vH = vW * 480 / 752;
		
		DmPanel *gui = &m_pCam_ZoomPanFocus->addWidget<DmPanel>( L"gui", L"", m_rect_Menu.topLeft.x, m_rect_Menu.topLeft.y, m_rect_Menu.bottomRight.x, m_rect_Menu.bottomRight.y, L"empty" );
		if (m_bSplitScreen) 
		{
			DmPanel *pVidB = &gui->addWidget<DmPanel>( L"vidB", L"", L"video_placeholder" );
			pVidB->setSize( h*0.7f*752/480, h*0.7f );
			pVidB->setPosition( m_MetroBlock, m_MetroBlock );
		}
//		DmPanel *pTxt8 = &gui->addWidget<DmPanel>( L"txtPZ", L"Camera pan/zoom/focus", m_MetroBlock, m_MetroBlock/2, m_MetroBlock*6, m_MetroBlock, L"Edit_60" );
//				 pTxt8->setHorizontalTextAlignment( dmAlignLeft );

		DmPanel *pO = &m_pCam_ZoomPanFocus->addWidget<DmPanel>( L"overlay", L"", m_rect_3D.topLeft.x, m_rect_3D.topLeft.y, m_rect_3D.bottomRight.x, m_rect_3D.bottomRight.y, L"blank_image" );
		DmPanel *pVid = &pO->addWidget<DmPanel>( L"vid", L"", w- m_MetroBlock/2-vW, h-m_MetroBlock-vH, w-m_MetroBlock/2, h-m_MetroBlock, L"video_placeholder" );
		DmPanel *pTxt7 = &pO->addWidget<DmPanel>( L"txtPZ", L"1)   Remove the lens cap", w/2-m_MetroBlock*2, m_MetroBlock*0.3, w/2+m_MetroBlock*7, m_MetroBlock*0.3+40, L"metro_title" );
				pTxt7->setHorizontalTextAlignment( dmAlignLeft );
				pTxt7 = &pO->addWidget<DmPanel>( L"txtP2", L"2)   Pan and zoom the image",w/2-m_MetroBlock*2, m_MetroBlock*0.6, w/2+m_MetroBlock*7, m_MetroBlock*0.6+40, L"metro_title" );
				pTxt7->setHorizontalTextAlignment( dmAlignLeft );
				pTxt7 = &pO->addWidget<DmPanel>( L"txtP3", L"3)   Focus using the bars and text",w/2-m_MetroBlock*2, m_MetroBlock*0.9, w/2+m_MetroBlock*7, m_MetroBlock*0.9+40, L"metro_title" );
				pTxt7->setHorizontalTextAlignment( dmAlignLeft );
				
		DmPanel *pP = &m_pCam_ZoomPanFocus->addWidget<DmPanel>( L"next", L"Calibrate camera", w/2 - m_MetroBlock*1.5, h-m_MetroBlock*1, w/2 + m_MetroBlock*2, h, L"block_yellow" );
					pP = &pP->addWidget<DmPanel>( L"nextB", L"", L"metro_border" );
					pP->setCommandText( L"Cam_Distortion" );
					DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
	}

	
	// Camera Distortion ---------------------------------------------------
	{	
		float w = m_rect_Menu.width();
		float h = m_rect_Menu.height();
		DmPanel *gui = &m_pCam_Distortion->addWidget<DmPanel>( L"gui", L"", m_rect_Menu.topLeft.x, m_rect_Menu.topLeft.y, m_rect_Menu.bottomRight.x, m_rect_Menu.bottomRight.y, L"empty" );
		if (m_bSplitScreen) 
		{
			DmPanel *pVidB = &gui->addWidget<DmPanel>( L"vidB", L"", L"video_placeholder" );
			pVidB->setSize( h*0.7f*752/480, h*0.7f );
			pVidB->setPosition( m_MetroBlock, m_MetroBlock );

			DmPanel *pTxt8 = &gui->addWidget<DmPanel>( L"txtPZ", L"Camera distortion", m_MetroBlock, m_MetroBlock/2, m_MetroBlock*6, m_MetroBlock, L"Edit_60" );
				 pTxt8->setHorizontalTextAlignment( dmAlignLeft );
/*
			DmPanel *pP = &gui->addWidget<DmPanel>( L"next", L"Cancel", w/2 - m_MetroBlock*1.5, h-m_MetroBlock*0.3, w/2 + m_MetroBlock*1.5, h, L"block_yellow" );
					pP = &pP->addWidget<DmPanel>( L"nextB", L"", L"metro_border" );
					pP->setCommandText( L"Cam_Intensity" );
					DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
					*/
		}

		w = m_rect_3D.width();
		h = m_rect_3D.height();
		float vW = w / 1.55;
		float vH = vW * 480 / 752;
		/*
		DmPanel *pVid = &m_pCam_Distortion->addWidget<DmPanel>( L"vid", L"", w- m_MetroBlock/2-vW, h-m_MetroBlock/2-vH, w-m_MetroBlock/2, h-m_MetroBlock/2, L"video_placeholder" );
		DmPanel *pP = &m_pCam_Distortion->addWidget<DmPanel>( L"next", L"", w/2 - m_MetroBlock*1.5, h-m_MetroBlock*0.3, w/2 + m_MetroBlock*1.5, h, L"metro_darkblue" );
		pP = &pP->addWidget<DmPanel>( L"nextB", L"", L"metro_border" );
		pP->setCommandText( L"Cam_Intensity" );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
			*/	
		
		DmPanel *dots = &m_pCam_Distortion->addWidget<DmPanel>( L"dots", L"", m_rect_3D.topLeft.x, m_rect_3D.topLeft.y, m_rect_3D.bottomRight.x, m_rect_3D.bottomRight.y, L"metro_black" );
		{
			int	dX = (w-40) / 8;
			int	dY = (h-40) / 4;

			for(int x=0; x<9; x++)
			{
				for (int y=0; y<5; y++)
				{
					int pX = 20 + x*dX;
					int pY = 20 + y*dY;
					dots->addWidget<DmPanel>( DmString(x) + DmString(y), L"", pX-15, pY-15, pX+15, pY+15, L"bullet" );
					distortion_grid[y][x].screen = f_Vector( m_rect_3D.topLeft.x + pX, m_rect_3D.topLeft.y + pY, 0 );
				}
			}
		}
		m_pCam_DistortionBLACK = &dots->addWidget<DmPanel>( L"black", L"", L"metro_black" );
		m_pCam_DistortionWHITE = &dots->addWidget<DmPanel>( L"white", L"", L"white" );
	}


	


}


void ctrl_DirectWeapons::buildGUI_live()
{
	m_p_ImageOverlay = &m_pLive->addWidget<DmPanel>( L"overlay", L"", m_rect_3D.topLeft.x, m_rect_3D.topLeft.y, m_rect_3D.bottomRight.x, m_rect_3D.bottomRight.y, L"blank_image" );		
	m_p_BloodSplatter = &m_pLive->addWidget<DmPanel>( L"blood", L"", m_rect_3D.topLeft.x, m_rect_3D.topLeft.y, m_rect_3D.bottomRight.x, m_rect_3D.bottomRight.y, L"bloodsplatter" );		

	m_pMouseClick = &m_pLive->addWidget<DmPanel>( L"mouseclick", L"", m_rect_3D.topLeft.x, m_rect_3D.topLeft.y, m_rect_3D.bottomRight.x, m_rect_3D.bottomRight.y );
	m_pMouseClick->setCommandText( L"mouseShot" );
	DmConnect( m_pMouseClick->mouseClick, *this, &ctrl_DirectWeapons::toolClick );


	m_laneGUI.buildGUI( m_pLive, m_Scenario.m_pc_lanes, m_bSplitScreen, m_rect_Menu, m_rect_3D, m_MetroBlock, m_MetroSpace, this );
	m_laneGUI.setLive( m_bLiveFire );

	float EYE1 = (*m_pCfgMng)["controller_DirectWeapons"]["eye_level_1"];
	float EYE2 = (*m_pCfgMng)["controller_DirectWeapons"]["eye_level_2"];
	float EYE3 = (*m_pCfgMng)["controller_DirectWeapons"]["eye_level_3"];
	m_eye_level[eye_PRONE] = 1.0f - EYE1 / m_rect_3D.height();
	m_eye_level[eye_SIT] = 1.0f - EYE2 / m_rect_3D.height();
	m_eye_level[eye_KNEEL] = 1.0f - EYE2 / m_rect_3D.height();
	m_eye_level[eye_STAND] = 1.0f - EYE3 / m_rect_3D.height();
	m_pScene->setProjectionOffset( 0.5, m_eye_level[eye_PRONE] );
	m_live_eye_dx = 0;
	m_live_eye_dy = 0;

	//bool bSpotting = !(*m_pCfgMng)["controller_DirectWeapons"]["spotting"].compare("top");
	int spottingPos = 0;	// HIDE
	if ( !(*m_pCfgMng)["controller_DirectWeapons"]["spotting"].compare("top") ) spottingPos = 1; //TOP MIDDLE
	if ( !(*m_pCfgMng)["controller_DirectWeapons"]["spotting"].compare("topleft") ) spottingPos = 2; //TOP :EFT
	if ( !(*m_pCfgMng)["controller_DirectWeapons"]["spotting"].compare("bottom") ) spottingPos = 3; //BOTTOM MIDDLE
	if ( !(*m_pCfgMng)["controller_DirectWeapons"]["spotting"].compare("bottomleft") ) spottingPos = 4; //BOTTOM LEFT
	m_laneGUI.movescope(spottingPos);


	DmPanel *pTxt2 = &m_pLive->addWidget<DmPanel>( L"eye_main", L"", m_rect_3D.bottomRight.x-1, 0,  m_rect_3D.bottomRight.x, m_rect_3D.height(), L"metro_darkblue_semi" );
	DmConnect( pTxt2->mouseOver, *this, &ctrl_DirectWeapons::eye_onMouseOver );
	DmConnect( pTxt2->mouseOut, *this, &ctrl_DirectWeapons::eye_onMouseOut );
	
	/*
	DmPanel *pT = &pTxt2->addWidget<DmPanel>( L"top", L"click", 0, m_rect_3D.height()/12,  150, m_rect_3D.height()*4/12, L"metro_cyan" );
	pT->setCommandText( L"moveInfoTop" );
	DmConnect(pT->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

	DmPanel *pB = &pTxt2->addWidget<DmPanel>( L"bottom", L"click", 0, m_rect_3D.height()*9/12,  150, m_rect_3D.height(), L"metro_cyan" );
	pB->setCommandText( L"moveInfoBottom" );
	DmConnect(pB->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
	*/
	DmPanel *pE = &pTxt2->addWidget<DmPanel>( L"center_height1", L"Prone", 0, EYE1-15,  150, EYE1+15, L"metro_yellow" );
	DmConnect( pE->mouseDrag, *this, &ctrl_DirectWeapons::onMouseDrag3D );

			pE = &pTxt2->addWidget<DmPanel>( L"center_height2", L"Kneel/Sit", 0, EYE2-15,  150, EYE2+15, L"metro_yellow" );
	DmConnect( pE->mouseDrag, *this, &ctrl_DirectWeapons::onMouseDrag3D );

			pE = &pTxt2->addWidget<DmPanel>( L"center_height3", L"Standing", 0, EYE3-15,  150, EYE3+15, L"metro_yellow" );
	DmConnect( pE->mouseDrag, *this, &ctrl_DirectWeapons::onMouseDrag3D );


	m_p_IntroOverlay = &m_pLive->addWidget<DmPanel>( L"intro", L"", m_rect_3D.topLeft.x, m_rect_3D.topLeft.y, m_rect_3D.bottomRight.x, m_rect_3D.bottomRight.y, L"metro_darkblue" );		
	{
		float w = m_rect_3D.width();
		float h = m_rect_3D.height();
		float h2 = h - 200;
		float b1 = h2*0.25;
		float b2 = h2*0.6;
		float b3 = h2 / 5;

		m_p_IntroExercise = &m_p_IntroOverlay->addWidget<DmPanel>( L"ex", L"exercise 1 :  ", 0, 50, w/3, 110, L"Edit_48" );
		m_p_IntroExercise->setHorizontalTextAlignment( dmAlignRight );
		m_p_IntroExercise->setVerticalTextAlignment( dmAlignBottom ) ;

		m_p_IntroName = &m_p_IntroOverlay->addWidget<DmPanel>( L"name", L"Sight setting", w/3, 50, w , 130, L"Edit_100" );
		m_p_IntroName->setHorizontalTextAlignment( dmAlignLeft );
		m_p_IntroName->setVerticalTextAlignment( dmAlignBottom ) ;

		m_p_IntroTarget = &m_p_IntroOverlay->addWidget<DmPanel>( L"tgt", L"", 80, 180, 80 + b1 , 180 + b1, L"empty" );
		m_p_IntroTarget->setValue( L"target_fig11" );
		DmConnect( m_p_IntroTarget->drawCustomBackground, this, &ctrl_DirectWeapons::drawCustomSprite );

		m_p_IntroDistance = &m_p_IntroOverlay->addWidget<DmPanel>( L"distance", L"", 80, 180+b1, 80 + b1 , 180 + b1+50, L"Edit_60" );

		//m_p_IntroDescription = &m_p_IntroOverlay->addWidget<DmPanel>( L"dsc", L"- Static", w/2.5, h-70-2*b, w , h -70-b, L"Edit_100" );
		//m_p_IntroDescription->setHorizontalTextAlignment( dmAlignLeft );
		
		m_p_IntroPosition = &m_p_IntroOverlay->addWidget<DmPanel>( L"pos", L"", 20, h-b2, 20 + b2 , h, L"empty" );
		m_p_IntroPosition->setValue( L"icon_standing" );
		DmConnect( m_p_IntroPosition->drawCustomBackground, this, &ctrl_DirectWeapons::drawCustomSprite );

		//m_p_IntroRounds = &m_p_IntroOverlay->addWidget<DmPanel>( L"rnd", L"- 5 rounds", w/2.5, h-20-b, w , h -20, L"Edit_100" );
		//m_p_IntroRounds->setHorizontalTextAlignment( dmAlignLeft );

		for (int i=0; i<5; i++)
		{
			m_p_IntroBullet[i] = &m_p_IntroOverlay->addWidget<DmPanel>( L"t" + DmString( i ), L"Testing", w/2.5, 200 + i*b3, w , 200 + i*b3 + 80, L"Edit_100" );
			m_p_IntroBullet[i]->setHorizontalTextAlignment( dmAlignLeft );
			m_p_IntroBullet[i]->setHorizontalTextMargin( 50 );
			DmPanel *pP = &m_p_IntroBullet[i]->addWidget<DmPanel>( L"b", L"", 0, 30 , 30 , 60, L"bullet" );
		}
	}
}



void ctrl_DirectWeapons::buildGUI_messageBox()
{
	m_pMB_title = &m_pMessagebox->addWidget<DmPanel>( L"title", L"Testing", L"metro_yellow" );
	m_pMB_title->setHeight( m_MetroBlock/2 );
	m_pMB_title->setTopMargin( m_MetroBlock/2 );
	m_pMB_title->setHorizontalTextAlignment( dmAlignLeft );
	m_pMB_title->setHorizontalTextMargin( m_MetroBlock );

	m_pMB_txt = &m_pMessagebox->addWidget<DmPanel>( L"text", L"Testing sentence", L"Edit_60" );
	m_pMB_txt->setLayout( m_MetroBlock/2, m_MetroBlock, m_MetroBlock/2, m_MetroBlock );
	
	DmPanel *pOK = &m_pMessagebox->addWidget<DmPanel>( L"OK", L"OK", L"metro_yellow" );
	pOK->setSize( m_MetroBlock, m_MetroBlock/2 );
	pOK->setPosition( m_rect_Menu.width() - m_MetroBlock*2, m_rect_Menu.height() - m_MetroBlock );
	
	m_pMessagebox->hide();
}

void ctrl_DirectWeapons::showMessagebox( DmString title, DmString txt )
{
	m_pMB_txt->setText( txt );
	m_pMB_title->setText( title );
	m_pMessagebox->show();
}


void ctrl_DirectWeapons::ExtractMenuItem( TiXmlNode *node, _MenuItem *MI)
{
	DmSpriteManager& pSM = *m_pScene->getSpriteManager();

	MI->colour = L"0";		// 0xfffffff
	MI->height = 1;
	MI->icon = L"0";
	MI->link = L"0";
	MI->name = L"0";
	MI->mysql = L"0";
	MI->bIcon = false;
	MI->bSpacer = true;


	// Bou UDP buffer -----------------------------------------------------
	if( node->ToElement()->Attribute(L"name") )		MI->name = node->ToElement()->Attribute(L"name");
	if( node->ToElement()->Attribute(L"icon") )		MI->icon = node->ToElement()->Attribute(L"icon");
	if( node->ToElement()->Attribute(L"link") )		MI->link = node->ToElement()->Attribute(L"link");
	if( node->ToElement()->Attribute(L"mysql") )	MI->mysql = node->ToElement()->Attribute(L"mysql");
	if( node->ToElement()->Attribute(L"h") )		MI->height = DmString( node->ToElement()->Attribute(L"h") ).toInt();
	if( node->ToElement()->Attribute(L"colour") )
	{
		unsigned long C;
		std::wstringstream ws;
		ws << std::hex << node->ToElement()->Attribute(L"colour");
		ws >> C;
		MI->colour = DmString( (int)C );
	}
	ewstring udp = L"MI," + MI->colour + L"," + DmString(MI->height) + L"," + MI->icon + L"," + MI->link + L"," + MI->name + L"," + MI->mysql;  
	MI->UDP = udp;

	if( node->ToElement()->Attribute(L"name") )		
	{
		MI->bSpacer = false;
		MI->name = node->ToElement()->Attribute(L"name");
		if( !pSM.containsSprite(MI->name) ) MI->name = L"error";
	}
	if( node->ToElement()->Attribute(L"icon") )
	{
		MI->bIcon = true;
		MI->icon = node->ToElement()->Attribute(L"icon");
		if( !pSM.containsSprite(MI->icon) ) MI->icon = L"error";
	}

}


static int CNT = 0;
void ctrl_DirectWeapons::addUserVideos( DmString directory, float *X, float *Y )
{
	//m_pLogger->Log( LOG_INFO, L"	user videos (%s)", directory.c_str() );
	// sit User Scene tab by -------------------------------------------------------------------------------------------------------------------------------
	CNT ++;
	float dH = (m_MetroBlock + m_MetroSpace) / 4; 
	float dW = m_MetroBlock*2 + m_MetroSpace;
	FILE *file= NULL;
	WIN32_FIND_DATA	FindFileData;
	HANDLE			hFind;
	BOOL			bFile = true;
	int x=0; 
	float y=0; 
	hFind = FindFirstFile( (directory + L"*.*").c_str(), &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		
		do
		{
			DmString shortname;
			DmString Fname = FindFileData.cFileName;
			DmString Fullname = directory + Fname; 
			if ((Fname.find(L"avi") !=wstring::npos) || 
				(Fname.find(L"AVI") !=wstring::npos) || 
				(Fname.find(L"mov") !=wstring::npos) || 
				(Fname.find(L"MOV") !=wstring::npos) ||
				(Fname.find(L"wmv") !=wstring::npos) || 
				(Fname.find(L"WMV") !=wstring::npos)  )
			{
				DmPanel *pP = &m_pMenuSlide->addWidget<DmPanel>( DmString(CNT), FindFileData.cFileName, *X, *Y , *X+dW, *Y+dH, L"metro_cyan" );
				if (CNT & 0x1)	pP->setValue( L"ff9B3FBE" );
				else			pP->setValue( L"ff1B501E" );
				DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomMenu );
				pP->setHorizontalTextMargin( 4 );
				pP->setHorizontalTextAlignment( dmAlignLeft );
				pP->setVerticalTextAlignment( dmAlignTop );
				DmPanel *pO = &pP->addWidget<DmPanel>( DmString(CNT+1000), L"", L"metro_border" );
				pO->setCommandText( L"I-Video" );
				shortname = wstring(Fname.begin(), Fname.end()-4);
				pP->setText( shortname );
				pO->setHelpText( Fullname );
				DmConnect( pO->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
				CNT ++;

				*Y += dH;
				if (*Y >= dH*22)
				{
					*Y = 90 + dH;
					*X += dW + m_MetroSpace;
				}
			}

			bFile = FindNextFile( hFind, &FindFileData );
		} while(bFile == TRUE);
	}
}


void ctrl_DirectWeapons::addUserScenes( DmString directory, float *X, float *Y, DmString sqlGroup )
{
	//m_pLogger->Log( LOG_INFO, L"addUserVideos (%s)", directory.c_str() );
	// sit Video tab by -------------------------------------------------------------------------------------------------------------------------------
	CNT ++;
	float dH = (m_MetroBlock + m_MetroSpace) / 4; //1 spacer grootte
	float dW = m_MetroBlock*2 + m_MetroSpace;
	FILE *file= NULL;
	WIN32_FIND_DATA	FindFileData;
	HANDLE			hFind;
	BOOL			bFile = true;
	int x=0; 
	float y=0; 
	hFind = FindFirstFile( (directory + L"*.*").c_str(), &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		
		do
		{
			//m_pLogger->Log( LOG_INFO, L"	user scenes (%s)", FindFileData.cFileName );

			DmString shortname;
			DmString Fname = FindFileData.cFileName;
			DmString Fullname = directory + Fname; 
			if ( (Fname.find(L"lua") !=wstring::npos) &&
				 (Fname.find(L"commonFunctions") != 0)  )
			{
				//m_pLogger->Log( LOG_INFO, L"	user scenes inside (%s)", FindFileData.cFileName );
				DmPanel *pP = &m_pMenuSlide->addWidget<DmPanel>( DmString(CNT), FindFileData.cFileName, *X, *Y , *X+dW, *Y+dH, L"metro_cyan" );
				if (CNT & 0x1)	pP->setValue( L"ff9B3F1E" );
				else			pP->setValue( L"ff9B501E" );
				DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomMenu );
				pP->setHorizontalTextMargin( 4 );
				pP->setHorizontalTextAlignment( dmAlignLeft );
				pP->setVerticalTextAlignment( dmAlignTop );
				//pP->setValue( L"interactive-video" );	
				//DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomMenu );
				DmPanel *pO = &pP->addWidget<DmPanel>( DmString(CNT+1000), L"", L"metro_border" );
				pO->setCommandText( Fname );
				shortname = wstring(Fname.begin(), Fname.end()-4);
				pP->setText( shortname );
				pO->setHelpText( L"Quick Range" );  // For now we are goign t store all user scenes under wuick rnage untill we rework the fatabase
				pO->setValue( sqlGroup );           // Value holds the SQL group
				DmConnect( pO->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
				CNT ++;

				*Y += dH;
				if (*Y >= dH*22)
				{
					*Y = 90 + dH;
					*X += dW + m_MetroSpace;
				}
			}

			bFile = FindNextFile( hFind, &FindFileData );
		} while(bFile == TRUE);
	}
}

/*
	This opens the same config again but broadcasts it for the tablet
*/
void ctrl_DirectWeapons::SENDUDP(ewstring S)
{
	sendto(sock, S.c_str(), S.length(), 0, (struct sockaddr*) &broadcast_addr, sizeof(broadcast_addr));
}
void ctrl_DirectWeapons::SEND(ewstring S)
{
	if(m_bSocketsOK)
	{
		send( acceptSocket, S.c_str(), S.length(), 0 );
	}
}

void ctrl_DirectWeapons::BroadcastMenu()
{
	m_pLogger->Log( LOG_INFO, L"clickProcessBroadcastMenu() " );
	TiXmlDocument		document;
	if ( document.LoadFile( configName.c_str() ) )
	{
		m_pLogger->Log( LOG_INFO, L"LoadFile( %s )", configName.c_str() );
		// load the menu ---------------------------------------------------------------------------------
		TiXmlNode *menuNode = document.FirstChild( L"menu" );
		//m_pLogger->Log( LOG_INFO, L"SEND( menu_clear );");
		SEND( ewstring("menu_clear") );

		_MenuItem MI;
		m_MaxPage = 0;
		DmString sqlGroup;
		DmString currentTab;

		if (menuNode)
		{
			int numTab = 0;
			TiXmlNode *tab = menuNode->FirstChild( L"tab" );
			while(tab )
			{
				ExtractMenuItem( tab, &MI);
				//m_pLogger->Log( LOG_INFO, L"SEND( tab - %s )", MI.name.c_str());
				ewstring ts = "tab," + ewstring( MI.name );
				SEND( ts );
				Sleep( 20 );

				TiXmlNode *item = tab->FirstChild( L"item" );
				while(item )
				{
					ExtractMenuItem( item, &MI);
					//m_pLogger->Log( LOG_INFO, L"SEND( MI - %s )   UDP - %s;", MI.name.c_str(), MI.UDP.getWString().c_str() );
					SEND( MI.UDP );
					Sleep( 20 );

/*
					if (MI.name.find( L"Videos" ) == 0)
					{
						addUserVideos( L"../../user_Videos/" +	currentTab +L"/" , &X, &Y );
					}

					if (MI.name.find( L"UserExercises" ) == 0)
					{
						addUserScenes( L"../../user_Scenes/" +	currentTab +L"/" , &X, &Y, sqlGroup );
					}*/

					item = item->NextSibling();
					CNT ++;
				}
				
				tab = tab->NextSibling();
			}
		}
		return;
	}
	ew_Scene::GetSingleton()->getLogger()->Log( LOG_ERROR, L"ctrl_DirectWeapons::BroadcastMenu failed" );
	return;
}


bool ctrl_DirectWeapons::loadConfig( std::wstring config )
{
	configName = config;
	if (!m_bInstructorStation && !m_pScene->isPointGreyConnected()) return true;
	m_pMenuSlide->clearWidgets();


	m_pLogger->Log( LOG_INFO, L"loadConfig( %s )", config.c_str() );
	TiXmlDocument		document;
	if ( document.LoadFile( config.c_str() ) )
	{
		// load the menu ---------------------------------------------------------------------------------
		TiXmlNode *menuNode = document.FirstChild( L"menu" );
		
		float dH = (m_MetroBlock + m_MetroSpace) / 4; //1 spacer grootte
		float dW = m_MetroBlock*2 + m_MetroSpace;
		float X = (m_MetroBlock + m_MetroSpace) / 2;
		float Y = 90;
		float right = X;
		

		_MenuItem MI;
		m_MaxPage = 0;
		DmString sqlGroup;
		DmString currentTab;

		if (menuNode)
		{
			int numTab = 0;
			TiXmlNode *tab = menuNode->FirstChild( L"tab" );
			while(tab )
			{
				ExtractMenuItem( tab, &MI);
				m_pLogger->Log( LOG_INFO, L"    tab(%s)", MI.name.c_str() );
				DmPanel *pTXT = &m_pMenuSlide->addWidget<DmPanel>( MI.name + DmString(CNT), L"", X, 40, X+300, 40+40, L"empty" );
				pTXT->setValue( MI.name );
				sqlGroup = MI.mysql;
				DmConnect( pTXT->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
				m_PageOffsets[ m_MaxPage ] = X - (m_MetroBlock + m_MetroSpace) / 2;
				m_MaxPage ++;
				currentTab = MI.name;

				TiXmlNode *item = tab->FirstChild( L"item" );
				while(item )
				{
					ExtractMenuItem( item, &MI);
					
					bool bLink = MI.link == L"";
					if (Y==90 && !bLink) Y += dH;

					if (MI.bSpacer)
					{
						if (MI.height==0 && Y > 90)
						{
							Y = 90;
							X += dW + m_MetroSpace*2;
						}
					}
					else
					{
						DmPanel *pP = &m_pMenuSlide->addWidget<DmPanel>( DmString(CNT), L"", X, Y , X+dW, Y+dH*MI.height, L"empty" );
						pP->setValue( MI.colour );
						DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomMenu );

						DmPanel *pTxt;
						if (MI.bIcon)	pTxt = &pP->addWidget<DmPanel>( L"txt", L"", 0, dH*(MI.height-1), dW, dH*MI.height, L"empty" );
						else			pTxt = &pP->addWidget<DmPanel>( L"txt", L"", 0, 0, dW, dH*MI.height, L"empty" );
						pTxt->setValue( MI.name );
						DmConnect( pTxt->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );

						if (MI.bIcon)
						{
							DmPanel *pIco = &pP->addWidget<DmPanel>( L"ico", L"", dW - dH*MI.height-5, 0, dW-5, dH*MI.height, L"empty" );
							pIco->setValue( MI.icon );
							DmConnect( pIco->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomSprite );
						}


						if (!bLink  )
						{
							DmPanel *pO = &pP->addWidget<DmPanel>( DmString(CNT+1000), L"", L"metro_border" );
							pO->setCommandText( MI.link );
							pO->setHelpText( MI.mysql );
							pO->setValue( sqlGroup );
							DmConnect( pO->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
						}
						
					}

					Y += dH*MI.height;
					if (MI.height>2) Y += dH;
					if (Y >= dH*22)
					{
						Y = 90;
						X += dW + m_MetroSpace*2;
					}

					if (MI.name.find( L"Videos" ) == 0)
					{
						addUserVideos( L"../../user_Videos/" +	currentTab +L"/" , &X, &Y );
					}

					if (MI.name.find( L"UserExercises" ) == 0)
					{
						addUserScenes( L"../../user_Scenes/" +	currentTab +L"/" , &X, &Y, sqlGroup );
					}

					item = item->NextSibling();
					CNT ++;
				}
				
				if (Y>90)
				{
					X += dW + m_MetroSpace;
					Y = 90;
				}
				X += dW/2 - m_MetroSpace;

				numTab ++;
				tab = tab->NextSibling();
			}
		}
		
		m_MaxPage --;
		goMenu();
		return true;
	}
	ew_Scene::GetSingleton()->getLogger()->Log( LOG_ERROR, L"ctrl_DirectWeapons::loadConfig failed" );
	return false;

}






void ctrl_DirectWeapons::updateRPC()
{
	vector<rpcDATA>::iterator it;
	for (it=m_pScene->m_RPC_Vector.begin(); it!=m_pScene->m_RPC_Vector.end(); it++)
	{
		switch( it->type )
		{
		case RPC_SERIAL:
			{
				m_RPC_serial = it->str[0].getUInt();
			}
			break;
		case RPC_TOOLCLICK:
			{
				if ( !m_bInstructorStation )
				{
					clickProcess( it->str[0], it->str[1], it->str[2], it->str[3] );
					m_pLogger->Log( LOG_INFO, L"RPC_TOOLCLICK   %s, %s, %s", it->str[0].getWString().c_str(), it->str[1].getWString().c_str(), it->str[2].getWString().c_str() );
				}
			}
			break;
		case RPC_TOOLCLICKAMMO:
			{
				if ( !m_bInstructorStation )
				{
					clickProcessAmmo( it->str[0], it->str[1], it->str[2], it->str[3] );
					m_pLogger->Log( LOG_INFO, L"RPC_TOOLCLICKAMMO   %s, %s, %s", it->str[0].getWString().c_str(), it->str[1].getWString().c_str(), it->str[2].getWString().c_str() );
				}
			}
			break;
		case RPC_SHOWINTRO:
			{
				if ( !m_bInstructorStation )
				{
					gui_showIntro( it->sht[0], DmString(it->str[0].getWString()), it->sht[1], it->sht[2], DmString(it->str[1].getWString()), DmString(it->str[2].getWString()), it->sht[3] );
				}
			}
			break;
		case RPC_CAMERA:	// FIXME dis verkeerd aanvaar 2 erweredig gespitte skerms, wil meer algemeen weees
			{
				//m_pLogger->Log( LOG_INFO, L"RPC_CAMERA   %f", it->flt[0] );
				m_pScene->getCamera()->setMatrix( it->mat );
				if( it->flt[0] > 0 )
				{	
					m_pScene->getCamera()->setFOV( it->flt[0] );
					/*
					if (m_LaneZero == -1)
					{
						m_pScene->getCamera()->setFOV( it->flt[0] );
					}
					else
					{
						m_pScene->getCamera()->setFOV( it->flt[0]/2 );
					}
					*/
				}
				else
				{
					
					if (m_LaneZero == -1)
					{
						m_live_eye_dx = 0;
						m_live_eye_dy = 0;
						m_pScene->setProjectionOffset( 0.5f + m_live_eye_dx, m_eye_level[m_eye_Position] + m_live_eye_dy );
					}
					else if (m_LaneZero == 0)
					{
						m_live_eye_dx = 0.5;
						m_live_eye_dy = 0;
						m_pScene->setProjectionOffset( 0.5f + m_live_eye_dx, m_eye_level[m_eye_Position] + m_live_eye_dy );
					}
					else
					{
						m_live_eye_dx = -0.5;
						m_live_eye_dy = 0;
						m_pScene->setProjectionOffset( 0.5f + m_live_eye_dx, m_eye_level[m_eye_Position] + m_live_eye_dy );
					}
					
				}
			}
			break;
		case RPC_ON_FIRE:
			{
				if ( !m_b3DTerminal )
				{
					//m_pLogger->Log( LOG_INFO, L"RPC_ON_FIRE   %d, %d, %s, %s", it->sht[0], it->sht[1], it->str[0].c_str(), it->str[1].c_str() );
					m_laneGUI.onFire( it->sht[0], it->sht[1], it->str[0].getWString(), it->flt[0], it->flt[1], it->str[1].getWString(), it->vec[0], it->vec[0], it->flt[2] );
					m_Scenario.onFire( it->sht[0] );
				}
			}
			break;
		case RPC_ADJUST:
			{
				if ( m_b3DTerminal )
				{
					m_pLogger->Log( LOG_INFO, L"RPC_ADJUST   %d, %f, %f, %f", it->sht[0], it->flt[0], it->flt[1], it->flt[2] );
					int L = it->sht[0] - this->m_LaneZero;
					if (L>=0 && L<m_NumLanes)
					{
						adjustWeaponOffset( L, it->flt[0], it->flt[1], it->flt[2] );
					}
				}
			}
			break;
		case RPC_ON_HIT:
			{
				if ( m_b3DTerminal )
				{
					//m_pLogger->Log( LOG_INFO, L"RPC_ON_HIT lane(%d) (%d)  ", it->sht[0], it->sht[0] - m_LaneZero );
					int lane = it->sht[0] - m_LaneZero;
					if (lane >=0 && lane < m_NumLanes )
					{
						m_laneGUI.onHit( lane, it->vec[0], it->flt[0], it->flt[1], it->flt[2], it->sht[1], it->sht[2], it->flt[3] );
					}
				}
			}
			break;
		case RPC_BULLET:
			{
				m_Scenario.bullet_hit( it->GUID, it->vec[0], it->flt[0] );
			}
			break;
		case RPC_MYSELF_HIT:
			m_p_BloodSplatter->show();
			m_TimeBloodSplatter = 0.5f;

			break;
		}
		
	}

	m_pScene->m_RPC_Vector.clear();
}

void ctrl_DirectWeapons::animateMenu( float dTime )
{
	float targetPos = -m_PageOffsets[m_MenuNewPage];//-m_MenuNewPage * m_MenuWidth;
	float diff = targetPos - m_AnimatedPosition;
	float sign = diff / fabs(diff);

	//static int CNT = 0;
	//CNT ++;
	//if (m_bMouseIsDragging)	CNT = 0;
	//m_bMouseIsDragging = false;

	

	if (m_MenuPage != m_MenuNewPage)
	{
		if (fabs(diff) > 300)			m_AnimatedPosition += 300 * sign * dTime * 5;
		else							m_AnimatedPosition += diff * dTime * 5;
		m_pMenuSlide->setPosition(m_AnimatedPosition, 96);


		if (fabs(diff) < 3)
		{
			m_AnimatedPosition = targetPos;
			m_pMenuSlide->setPosition(m_AnimatedPosition, 96);
			m_MenuPage = m_MenuNewPage;
		}
	}
	/*
	else if( CNT > 10 )	// check hier vir muis her ver beweeg
	{
//		if (fabs(diff) > 100)
//		{
//			if (diff < 0)	m_MenuNewPage = __max( 0, m_MenuPage -1 );
//			else			m_MenuNewPage = __min( m_MaxPage, m_MenuPage +1 );
//		}

		//else
		{
			//if (fabs(diff) > 300)			m_AnimatedPosition += 300 * sign * dTime * 5;
			//else							m_AnimatedPosition += diff * dTime * 5;
			//m_pMenuSlide->setPosition(m_AnimatedPosition, 96);


			if (fabs(diff) < 3)
			{
				//m_AnimatedPosition = targetPos;
				//m_pMenuSlide->setPosition(m_AnimatedPosition, 96);
				//m_MenuPage = m_MenuNewPage;
			}
		}
	}
	*/
	
			
}



#define SLOW(x, y)	{ static int cnt = 0; cnt ++; if (cnt > x) { y; 	cnt = 0; }	}
void ctrl_DirectWeapons::update( double dTime )
{

	// WINSOCK --------------------------------------------------------------------------------------------------------------------------------
	if (!m_bSocketsOK)			// no cpnnection, boradcast address on UDP, and wait for accept
	{
		SLOW( 50, m_pLogger->Log( LOG_ERROR, L"SENDUDP( Kolskoot );" );	SENDUDP( "Kolskoot" ););

		//acceptSocket = accept( tcpSock, &acceptaddr, sizeof(acceptaddr) );
		acceptSocket = accept( tcpSock, NULL, NULL );
		if (acceptSocket != INVALID_SOCKET)  m_bSocketsOK = true;
	}
	else
	{
		// we are connected, listen for new data
		char buffer[1024];
		int len = recv( acceptSocket, buffer, 1024, 0 );
		while (len > 0)
		{
			 buffer[len] = 0;	// make the buffer zero terminated
		 
			 ewstring CMD[4];
			 CMD[0] = "";	// cmd
			 CMD[1] = "";	// val
			 CMD[2] = "";	// hlp
			 CMD[3] = "";	// ext

			 char keys[1024];
			char *tok = strtok(buffer, ",");
			int cnt = 0;
			while (tok && (cnt < 4))
			{
				CMD[cnt] = ewstring( tok );
				tok = strtok(NULL, ",");
				cnt ++;
			}

			 if (CMD[0] != "Kolskoot")
			 {
				 m_pLogger->Log( LOG_ERROR, L"WS - recvfrom - %s, %s, %s, %s", CMD[0].getWString().c_str(), CMD[1].getWString().c_str(), CMD[2].getWString().c_str(), CMD[3].getWString().c_str() );
				m_bSocketCommands = true;
				clickProcess( CMD[0], CMD[1], CMD[2], CMD[3] );
				m_bSocketCommands = false;
			 }

			len = recv( acceptSocket, buffer, 1024, 0 );
		}
	}
	
	/*
	 sockaddr_in from;
     int size = sizeof(from);
	 char buffer[1024];
     int ret = recvfrom(sock, buffer, 1024, 0, reinterpret_cast<SOCKADDR *>(&from), &size);
     if (ret > 0)
	 {
		 buffer[ret] = 0;	// make the buffer zero terminated
		 
		 ewstring CMD[4];
		 CMD[0] = "";	// cmd
		 CMD[1] = "";	// val
		 CMD[2] = "";	// hlp
		 CMD[3] = "";	// ext

		 char keys[1024];
		//strcpy(keys, licenseKey.c_str());		// BUG lisence strign could grow big -------------------------------------------------
		char *tok = strtok(buffer, ",");
		int cnt = 0;
		while (tok && (cnt < 4))
		{
			CMD[cnt] = ewstring( tok );
			tok = strtok(NULL, ",");
			cnt ++;
		}

		 if (CMD[0] != "Kolskoot")
		 {
			 m_pLogger->Log( LOG_ERROR, L"WS - recvfrom - %s, %s, %s, %s", CMD[0].getWString().c_str(), CMD[1].getWString().c_str(), CMD[2].getWString().c_str(), CMD[3].getWString().c_str() );
			 //m_b3DTerminal = true;
			m_bSocketCommands = true;
			clickProcess( CMD[0], CMD[1], CMD[2], CMD[3] );
			m_bSocketCommands = false;
		 }
	 }
	 */
	// end .. WINSOCK -------------------------------------------------------------------------------------------------------------------------

	m_NextDelay -= dTime;
	if (!m_bInstructorStation && !m_pScene->isPointGreyConnected()) return;

	if (m_b3DTerminal)
	{
		m_TerminalGoMenuCounter --;
		if ( m_TerminalGoMenuCounter == 0) goMenu();
	}

	if ( !m_RakNet->isConnected() )
	{
		m_pMenu->setStyleName( L"metro_green" );
		m_RaknetTimeout += dTime;
	}
	else
	{
		m_pMenu->setStyleName( L"metro_darkblue" );
		m_RaknetTimeout = 0;
	}
	if (m_RaknetTimeout > 5.0) m_pScene->Quit();
	

	updateRPC();

	if (m_TimeBloodSplatter <= 0)
	{
		m_p_BloodSplatter->hide();
	}
	if (m_TimeBloodSplatter > 0)
	{
		m_TimeBloodSplatter -= dTime;
		if (m_TimeBloodSplatter < 0)
		{
			m_p_BloodSplatter->hide();
		}
	}

	


	// framerate debug ----------------------------------------
	float fps = 1.0f / (float)dTime;
	int idx;
	if (dTime < 0.019)	idx = 0;
	else if (dTime < 0.036)  idx = 1;
	else if (dTime < 0.052)  idx = 2;
	else                idx = 3;
	m_FramerateBuffer[idx] ++;
	m_FramerateCounter[idx] = 20;

	for (int c=0; c<5; c++)
	{
		m_FramerateCounter[c] --;
	}

	if (m_bShowFPS)
	{
		int MOI = 0;
		for (int i=0; i<1024; i++)
		{
			if( m_pScene->m_pMOI[ i ].pMOI ) MOI++;
		}

		m_pFramerateInfo->show();
		DmString framestring = L"fps : " + DmString ((int)fps) 
			+ L"    " + DmString( m_FramerateBuffer[1] )  + L"    " + DmString( m_FramerateBuffer[2] )  + L"    " + DmString( m_FramerateBuffer[3] + L" ---> " + DmString(MOI) );
		m_pFramerateInfo->setText( framestring );
	}
	else
	{
		m_pFramerateInfo->hide();
	}




	switch (m_mode)
	{
	case mode_MENU:	

		m_MENU_loadDelay -= dTime;
		
		animateAmmo( dTime );
		animateMenu( (float)dTime );
		gui_LoadLabviewImage();
		m_SlideShowTimer += (float)dTime;
		if (m_bDemoMode)
		{
			//gui_LoadLabviewImage();	
			if( !m_bSlideShow && m_SlideShowTimer > 60) {m_bSlideShow = true; m_SlideShowTimer = 0;} 
		}
		else m_bSlideShow = false;

		if (bBoresight)		// if we are in boresight mode  pass shots onto a special function there
		{
			gui_LoadLabviewImage();	
		}

		{
			static float Ctime = 0.5;
			Ctime -= (float)dTime;
			if (Ctime < 0)
			{
				Ctime = 0.7f;
				m_bSearchCaret = ! m_bSearchCaret;
				if (m_bSearchCaret) m_pName_Search->setText( m_SearchText + L"|" );
				else				m_pName_Search->setText( m_SearchText );
			}
		}


		break;
	case mode_CAMERA_PZF:			gui_LoadLabviewImage();	break;
	case mode_CAMERA_DISTORTION:	
		m_CamDistortionPRE_blank ++;	
		if (m_CamDistortionPRE_blank == 15)		
		{ 
			m_pScene->setRefFrame( true, 3 ); 
			m_pScene->setDots( m_pCam_Thresh->getText().toFloat(), m_pCam_Min->getText().toFloat(), m_pCam_Max->getText().toFloat(), 1 );  // SET DOTS TO MIDDLE
		}
		if (m_CamDistortionPRE_blank == 25)		
		{
			m_pScene->setDots( m_pCam_Thresh->getText().toFloat(), m_pCam_Min->getText().toFloat(), m_pCam_Max->getText().toFloat(), 1 );  // SET DOTS TO MIDDLE
			m_pCam_DistortionBLACK->hide();		
			m_pCam_DistortionWHITE->hide();	  
		}	
		gui_LoadLabviewImage();	
		break;
	case mode_CAMERA_TEST:	
		m_pScene->setDots( m_pCam_Thresh->getText().toFloat(), m_pCam_Min->getText().toFloat(), m_pCam_Max->getText().toFloat(), m_pScene->m_PG_dot_position );		// reset this all
			SLOW( 100, m_pCam_Resolution->setText( DmString(m_pScene->m_PG_width) + L" x " + DmString(m_pScene->m_PG_height) ); );
		gui_LoadLabviewImage();	
		for (int i=0; i<this->m_lanes.size(); i++)
		{
			zigbeeRounds( i, 100 );
		}
		break;
	case mode_CAMERA_IR1:	
		m_pScene->setDots( m_pCam_Thresh->getText().toFloat(), m_pCam_Min->getText().toFloat(), m_pCam_Max->getText().toFloat(), 1 );  // SET DOTS TO MIDDLE
		gui_LoadLabviewImage();	
		break;
	case mode_CAMERA_IR2:			
		m_pScene->setDots( m_pCam_Thresh->getText().toFloat(), m_pCam_Min->getText().toFloat(), m_pCam_Max->getText().toFloat(), 1 );  // SET DOTS TO MIDDLE
		gui_LoadLabviewImage();	
		break;
	case mode_VIDEO:	

		if (!m_pScene->videoIsPlaying()) goMenu();
		break;
	case mode_INTERACTIVE_VIDEO:
		if (!m_bWaitingForLaneSelect)
		{
			//if ( !m_pScene->videoIsPlaying() )
			{
				m_pScene->videoPlay();
			}
			for (int i=0; i<m_lanes.size(); i++) zigbeeRounds( i, m_laneGUI.getRoundsLeft( i ) );
			m_VideoTime = ew_Scene::GetSingleton()->getVidTime();//+= (float)dTime;
			if (!m_Scenario.update( dTime ) ){/* this->goMenu(); */}
			gui_LoadLabviewImage();	
			m_laneGUI.update( dTime );
			if (!m_pScene->videoIsPlaying())
			{
				//goMenu();
				m_VideoTime = 0;
				m_CurrentDebriefShot = -1;
				m_mode = mode_IVID_DEBRIEF;
				m_pScene->videoReplay( 2.0 );
				m_laneGUI.setTime( 0.0f );	// FIXME only in 3D
				m_pScene->videoPause();
			}
		}
		else
		{
			m_laneGUI.showLaneSelect();
		}
		break;
	case mode_IVID_DEBRIEF:
		{
			m_VideoTime = ew_Scene::GetSingleton()->getVidTime();//+= (float)dTime;
			m_laneGUI.replayUpdate( dTime );	// FIXME only in 3D
			
			if (!m_pScene->videoIsPlaying())
			{
				m_VideoTime = 0;
				m_CurrentDebriefShot = -1;
				//m_mode = mode_IVID_DEBRIEF;
				m_pScene->videoReplay( 2.0 );
				m_laneGUI.setTime( 0.0f );	// FIXME only in 3D
				m_pScene->videoPause();
				//goMenu();
			}

			
		}
		break;
	case mode_3D:	
		// LUA delayed loading -----------------------------------------------------------------------------------------------
		//m_pLogger->Log( LOG_INFO, L"u");
		
		m_MENU_loadDelay -= dTime;
		if (m_MENU_loadDelay < 0)	m_LUA_delayLoad --;
		//m_pLogger->Log( LOG_INFO, L"m_LUA_delayLoad == %d;", m_LUA_delayLoad);
		if (m_LUA_delayLoad == 0)
		{
			m_pLogger->Log( LOG_INFO, L"update - loading LUA - %s", m_LUA_name.c_str() );
			//m_pLogger->Log( LOG_INFO, L"m_LUA_delayLoad == 0;");
			if (m_pLUAscript) 			m_pLUAscript->terminate();
			m_pLUAscript = m_pLUAManager->addScript( m_LUA_name.c_str() );		
			if (m_pLUAscript) 
			{
				
				if (!m_pLUAscript->execute())
				{
					m_pLogger->Log( LOG_INFO, L"Lua error : %s", m_pLUAscript->getLastError() );
					m_pScene->messageBox(m_pLUAscript->getLastError(), L"LUA Error");
					goMenu();
				}
				else
				{	
					m_Scenario.setSQLname( m_SQL_group, m_SQL_name );		
					m_Scenario.start();	
					m_laneGUI.showIntro( guitype_TARGET, m_Scenario.getName(), m_Scenario.get_numLanes() );
					if ( m_Scenario.get_numLanes() == 1 && (this->m_lanes.size() > 1) )	m_bWaitingForLaneSelect = true;
				}	
			}
			else m_pLUAscript = NULL;
		}

		if ( m_bWaitingForLaneSelect )
		{
			if ( m_LUA_delayLoad < -60 ) m_laneGUI.showLaneSelect();
		}
		else
		{
			if ( m_LUA_delayLoad <= 0 )
			{
				for (int i=0; i<m_lanes.size(); i++) zigbeeRounds( i, m_laneGUI.getRoundsLeft( i ) );
				gui_LoadLabviewImage();	
				m_laneGUI.update( dTime );
				if (!m_b3DTerminal)
				{
					!m_Scenario.update( dTime );
					//if (!m_Scenario.update( dTime )  ) this->goMenu();
				}
			}
		}
		 //m_pLogger->Log( LOG_INFO, L".");
		break;
	}

	
	


	
	
}



void	ctrl_DirectWeapons::goQuickScenario()
{
	int B[3];
	int T[3];
	m_Scenario.clear();
	m_Scenario.create_modelScenario( L"Quick Range", QuickRange.getRange().c_str(), L"" );
	m_Scenario.load_Sky( QuickRange.getSky().c_str() );
	m_Scenario.load_Cameras( L"IndoorRange_cameras.xml" );
	m_Scenario.setSQLname( QuickRange.getSQLgroup(), L"Quick Range" );


	if (QuickRange.getSingleLane()) m_Scenario.maxNumLanes( 1 );
	//m_Scenario.add_exercise( L"Loading", L"Loading", 0 );		//QuickRange
	//m_Scenario.add_scene( WAIT_LANES );
	//m_Scenario.act_Image( L"Loading.jpg" );
	//m_Scenario.act_Camera( L"cam_fast" );
	//m_Scenario.tr_time( 5 );
	int numEx = QuickRange.getNumEx();
	for (int i=0; i<numEx; i++)
	{
		int R = (int)QuickRange.getRouds( i );	//QuickRange
		float D = QuickRange.getDistance( i );	//QuickRange

		DmString desc = L"Exercise " + DmString (i+1) + L" : " + QuickRange.getTarget( i ) +  L" - " + DmString((int)QuickRange.getDistance( i )) + L"m";
		m_Scenario.add_exercise( desc.c_str(), desc.c_str(), R*5 );	

		m_Scenario.set_Eye( QuickRange.getShootingPosition() );

		//B[i] = m_Scenario.add_backdrop( L"outdoor_laan" );
		T[i] = m_Scenario.add_target( QuickRange.getTarget( i ).c_str() );

		

		m_Scenario.add_scene( WAIT_LANES );
		m_Scenario.act_BasicCamera( D );
		m_Scenario.tr_time( 0.5f );

		m_Scenario.add_scene( WAIT_INSTRUCTOR );
		{
			m_Scenario.act_Loadrounds( R );
			//m_Scenario.act_ShowTarget( B[i], 0.0f, D );
			m_Scenario.act_ShowTarget( T[i], 0.0f, D );
			switch (QuickRange.getMode( i ))
			{
			case QR_STATIC:	break;
			case QR_SNAP:		m_Scenario.act_TargetPopup( T[i], QuickRange.getRepeats(i), 2, 3 );	break;
			case QR_RAPID:		m_Scenario.act_TargetPopup( T[i], 1, 2, 30 );	break;
			case QR_MOVINGLR:	m_Scenario.act_TargetMove( T[i], QuickRange.getRepeats(i), 3, 1 );		break;
			case QR_MOVINGRL:	m_Scenario.act_TargetMove( T[i], QuickRange.getRepeats(i), 4, 1 );		break;
			case QR_CHARGE:		m_Scenario.act_TargetMove( T[i], QuickRange.getRepeats(i), 5, 7 );		break;
			case QR_ZIGZAG:		m_Scenario.act_TargetMove( T[i], QuickRange.getRepeats(i), 6, 4 );		break;
			case QR_ATTACK:		m_Scenario.act_TargetMove( T[i], QuickRange.getRepeats(i), 7, 3 );		break;
			case QR_RANDOM:		m_Scenario.act_TargetMove( T[i], QuickRange.getRepeats(i), 8, 1 );	break;
			}
			m_Scenario.tr_roundsCompleted( 2 );
			m_Scenario.act_ShowFeedback();
		}

		m_Scenario.add_scene( WAIT_INSTRUCTOR );
		{
			m_Scenario.act_Loadrounds( 0 );
			m_Scenario.act_ShowFeedback();
		}

	}
	m_Scenario.add_scene( WAIT_LANES );
	m_Scenario.act_SavetoSQL();
	
	m_Scenario.start();		
	m_laneGUI.showIntro( guitype_TARGET, L"Quick Range", m_Scenario.get_numLanes() );		
	goLive( mode_3D );	

}


void	ctrl_DirectWeapons::drawCustomMenu(DmPanel& sender, const DmRect2D& rect )
{
	if (sender.getValue() == L"") return;

	DmSpriteManager& pSM = *m_pScene->getSpriteManager();
	DmRect2D R = rect;

	// Background colour- ---------------------------------------------------
	DmSprite& spr = pSM[L"menu_white"];
	spr.setColourAlphaMultiplier( sender.getValue().toUInt() );
	spr.draw( R );

	//DmSprite& spr2 = pSM[ sender.getValue() ];
	//spr2.setColourAlphaMultiplier( 0xffffffff );
	//spr2.draw( R );
}

void	ctrl_DirectWeapons::drawCustomIcon(DmPanel& sender, const DmRect2D& rect )
{
	DmSpriteManager& pSM = *m_pScene->getSpriteManager();
	DmSprite& spr = pSM[L"icons"];
	int fr = spr.getFrameByName( sender.getValue() );
	DmRect2D R = rect;
	spr.drawFrame(R, fr );
}

void	ctrl_DirectWeapons::drawCustomSprite(DmPanel& sender, const DmRect2D& rect )
{
	DmSpriteManager& pSM = *m_pScene->getSpriteManager();
	if (pSM.containsSprite( sender.getValue() ))
	{
		DmSprite& spr = pSM[sender.getValue()];
		DmRect2D R = rect;
		spr.draw(R);
	}
}

#define RND_1_1		((float)rand() / (float)RAND_MAX - 0.5f) * 2.0f		// minus 1 to plus 1
void ctrl_DirectWeapons::paperAdvance()
{
	if (m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "PaperAdvance" );
		Bs.Write( (unsigned short)m_numShotsSinceLastPaperAdvance );
		Bs.Write( "" );
		Bs.Write( "" );
		m_pScene->rpc_broadcast( "rpc_CALL", &Bs );
	}
	 
	m_pLogger->Log( LOG_INFO, L"ctrl_DirectWeapons::paperAdvance()  m_numShotsSinceLastPaperAdvance = %d ", m_numShotsSinceLastPaperAdvance);

	if (m_bLiveFire)
	{
		m_live_eye_dx = RND_1_1 * 0.05f;
		m_live_eye_dy = RND_1_1 * 0.02f;
	}
	else
	{
		m_live_eye_dx = 0;
		m_live_eye_dy = 0;
	}

	m_pScene->setProjectionOffset( 0.5f + m_live_eye_dx, m_eye_level[m_eye_Position] + m_live_eye_dy );


	//if (m_bLiveFire && (m_numShotsSinceLastPaperAdvance >= 5 ) )	// FIXME also check that shots where fired
	if ( m_numShotsSinceLastPaperAdvance >= 5 )	// only check this so button works in laser as well
	{
		
		m_numShotsSinceLastPaperAdvance = 0;


		// LABJACK -----------------------------------------------------------------------------------------------
		long id = -1;
		long trisD = 0x1f;
		long trisIO = 0x0;
		long stateD = 0;
			 stateD += (1<<0);						// lights on
			 if (m_bLiveFire)	stateD += (1<<2);	//fans
			 stateD += (1<<4);						// power on
			 if (m_bLabjackTransnetShift)	stateD += (1<<3);						// power on line 3 at transnet only
		long stateIO = 0;
		long outputD = 0;

		stateD |=  1<<1;
		DigitalIO( &id, 0, &trisD, trisIO, &stateD, &stateIO, 1, &outputD);
		zigbeePaperMove();
		Sleep( m_paper_advance_ms );
		stateD &=  ~(1<<1);
		DigitalIO( &id, 0, &trisD, trisIO, &stateD, &stateIO, 1, &outputD);
		zigbeePaperStop();
		Sleep( m_paper_post_delay_ms );										// wag oomblik voor ons kamera reset - Hierdie is vir Darek - ons sal moet maak dat in die config file kom

		m_pScene->setRefFrame( true, 4 );

		//m_live_eye_dx -= 0.05;
		//if (m_live_eye_dx <= -0.06) m_live_eye_dx = 0.05;
		
	}
	else
	{
		m_pScene->setRefFrame( false, 4 );
	}
}

void ctrl_DirectWeapons::buildDots()
{
	if (m_bSplitScreen) return;							// return on split this is for 2 camera mpde only
	if (m_pScene->m_PG_frameCNT[1] == 0)	return;		// return we only haev 1 camwera

	m_pCam_Distortion->clearWidgets();

	DmWin32* pW32 = m_pScene->getDmWin32();
	vector<DmMonitorInfo> MI = pW32->getMonitors();
	DmRect2D rect = DmRect2D( (float)MI[m_ActiveCamera].rect.left, (float)MI[m_ActiveCamera].rect.top, (float)MI[m_ActiveCamera].rect.right, (float)MI[m_ActiveCamera].rect.bottom );

	DmPanel *dots = &m_pCam_Distortion->addWidget<DmPanel>( L"dots", L"", rect.topLeft.x, rect.topLeft.y, rect.bottomRight.x, rect.bottomRight.y, L"metro_black" );
	{
		int	dX = (rect.width()-40) / 8;
		int	dY = (rect.height()-40) / 4;

		for(int x=0; x<9; x++)
		{
			for (int y=0; y<5; y++)
			{
				int pX = 20 + x*dX;
				int pY = 20 + y*dY;
				dots->addWidget<DmPanel>( DmString(x) + DmString(y), L"", pX-15, pY-15, pX+15, pY+15, L"bullet" );
				distortion_grid[y][x].screen = f_Vector( rect.topLeft.x + pX, rect.topLeft.y + pY, 0 );
			}
		}
	}
	m_pCam_DistortionBLACK = &dots->addWidget<DmPanel>( L"black", L"", L"metro_black" );
	m_pCam_DistortionWHITE = &dots->addWidget<DmPanel>( L"white", L"", L"white" );




	m_pCam_IR->clearWidgets();
	{
		float dX = (rect.width() - 40) / 2;
		float dY = (rect.height()-40) / 2;
		for(int x=0; x<3; x++)
		{
			for (int y=0; y<3; y++)
			{
				int pX = rect.topLeft.x + 20 + x*dX;
				int pY = 20 + y*dY;
				m_pCam_IR_grid[y][x] = &m_pCam_IR->addWidget<DmPanel>( DmString(x) + DmString(y), L"test", pX-20, pY-20, pX+120, pY+20, L"Edit_30" );
				m_pCam_IR_grid[y][x]->setHorizontalTextAlignment( dmAlignLeft );
				//distortion_grid[y][x].screen = f_Vector( m_rect_3D.topLeft.x + pX, m_rect_3D.topLeft.y + pY, 0 );
			}
		}

		m_pCam_IR_info = &m_pCam_IR->addWidget<DmPanel>( "info", L"test", 50, 50, 950, 80, L"Edit_30" );
	}
}

void	ctrl_DirectWeapons::setEyePosition( int eyePosition )
{
	// FIXME ADD RAKNET
	if (m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "eye_position" );
		Bs.Write( ewstring(eyePosition).c_str() );
		Bs.Write( "" );
		Bs.Write( "" );
		ew_Scene::GetSingleton()->rpc_broadcast( "rpc_CALL", &Bs );
	}

	m_eye_Position = eyePosition;	
	m_pScene->setProjectionOffset( 0.5f + m_live_eye_dx, m_eye_level[m_eye_Position] + m_live_eye_dy );
	m_laneGUI.setEyePosition( eyePosition );
}


#define _onCLICK(x,y) if (cmd == x) { y; return; }
void	ctrl_DirectWeapons::clickProcess( ewstring cmd, ewstring val, ewstring hlp, ewstring ext )
{
	//m_pLogger->Log( LOG_ERROR, L"clickProcess" );
	//m_pLogger->Log( LOG_ERROR, L"clickProcess %s, %s, %s, %s", cmd.getWString().c_str(), val.getWString().c_str(), hlp.getWString().c_str(), ext.getWString().c_str() );
	
	if ( m_b3DTerminal || m_bSocketCommands )
	{
		m_pLogger->Log( LOG_ERROR, L"clickProcess - play   %s, %s, %s, %s", cmd.getWString().c_str(), val.getWString().c_str(), hlp.getWString().c_str(), ext.getWString().c_str() );
		_onCLICK(	"play", 
					// Speel standaar videos terug
					if ( (val.find(".avi") != string::npos) ||
						 (val.find(".wmv") != string::npos) ||
						 (val.find(".mov") != string::npos) )
					{
						m_pLogger->Log( LOG_ERROR, L"video found");
						WCHAR filePath[MAX_PATH];
						if ( m_pScene->getResourceManager()->findFile( val.getWString().c_str(), filePath, MAX_PATH ) )
						m_pScene->PlayVideo( filePath, true );
						goLive( mode_VIDEO );
						m_laneGUI.showVideo();
						return;
					}

					if ( val.find(".lua") != string::npos )
					{
						m_pLogger->Log( LOG_ERROR, L"lua found");
						//m_pLogger->Log( LOG_INFO, L"sender.getCommandText().find( .lua )");
						goLive( mode_3D );
						m_LUA_delayLoad = 2;
						m_LUA_name = val.getWString();
						m_SQL_group = L"";
						m_SQL_name = L"";
						//m_pLogger->Log( LOG_INFO, L"H");
						m_laneGUI.showIntro( guitype_TARGET, DmString( L"" ), this->m_NumLanes );
						//m_pLogger->Log( LOG_INFO, L"I");
		
						return;
					}
		);
		_onCLICK( "getMenu",		BroadcastMenu();	);
		_onCLICK( "go_Live",		goLive( val.getInt() );	);
		_onCLICK( "go_Intro",		goIntro( );	);
		_onCLICK( "go_Menu",		goMenu();	);
		_onCLICK( "hide_overlay",	hideOverlay();	);
		_onCLICK( "TERMINAL_assign_name_SHOW",	int L = val.getInt() - m_LaneZero;   if ((L>=0) && (L<m_NumLanes)) 
											{
												m_lanes[L].userNAME = hlp.getWString(); 
												populateAmmo(L, true);		
												m_activeAmmoLane = L; 
												m_activeAmmunition = ext.getInt();	
												ammosetThis( true ); 
											}	);
		_onCLICK( "TERMINAL_assign_name_HIDE",	int L = val.getInt() - m_LaneZero;   if ((L>=0) && (L<m_NumLanes)) 
											{
												m_lanes[L].userNAME = hlp.getWString(); 
												populateAmmo(L, false);		
												m_activeAmmoLane = L; 
												m_activeAmmunition = ext.getInt();	
												ammosetThis( false ); 
											}	);
		_onCLICK( "eye_position",	setEyePosition( val.getInt() ); );
		_onCLICK( "eye_level",		m_pLogger->Log( LOG_INFO, L"_onCLICK eye_level   %s, %s", val.getWString().c_str(), hlp.getWString().c_str() );
									setEyePosition( hlp.getInt() );// Comes from instructor station likelephal ------------------------------------------------------------------------------------------
									float EYE_LEVEL=(1.0 - m_eye_level[m_eye_Position]) * m_rect_3D.height();
									m_eye_level[m_eye_Position] = val.getFloat();
									switch(m_eye_Position)
									{
									case eye_PRONE:
										(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig("eye_level_1", DmString(EYE_LEVEL).c_str() );
										((DmPanel*)m_pLive->findWidget( L"center_height1" ))->setPosition( 0, EYE_LEVEL );
										break;
									case eye_SIT:
										(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig("eye_level_2", DmString(EYE_LEVEL).c_str() );
										((DmPanel*)m_pLive->findWidget( L"center_height2" ))->setPosition( 0, EYE_LEVEL );
										break;
									case eye_KNEEL:
										(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig("eye_level_2", DmString(EYE_LEVEL).c_str() );
										((DmPanel*)m_pLive->findWidget( L"center_height2" ))->setPosition( 0, EYE_LEVEL );
										break;
									case eye_STAND:
										(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig("eye_level_3", DmString(EYE_LEVEL).c_str() );
										((DmPanel*)m_pLive->findWidget( L"center_height3" ))->setPosition( 0, EYE_LEVEL );
										break;
									}
									
									);
		_onCLICK( "loadSky",		m_pScene->loadSky( val.getWString().c_str() ); );

		_onCLICK( "lane_GUI::showLive",			m_laneGUI.showLive(); );
		_onCLICK( "lane_GUI::showLaneSelect",	m_laneGUI.showLaneSelect(); );
		_onCLICK( "lane_GUI::showFinal",		m_laneGUI.showFinal(); );
		_onCLICK( "lane_GUI::showVideo",		m_laneGUI.showVideo(); );
		_onCLICK( "lane_GUI::showIntro",		m_laneGUI.showIntro( guitype_TARGET, hlp.getWString(), __min(val.getInt(), m_NumLanes) ); SEND( "showLive" );); 
		_onCLICK( "lane_GUI::setAmmunition",	int L = val.getInt() - m_LaneZero;   if ((L>=0) && (L<m_NumLanes)) m_laneGUI.setAmmunition( L, hlp.getInt() ); );
		_onCLICK( "lane_GUI::showFeedback",		int L = val.getInt() - m_LaneZero;   if ((L>=0) && (L<m_NumLanes)) m_laneGUI.showFeedback( L, m_bLiveFire, hlp.getInt() ); );
		_onCLICK( "lane_GUI::setTargetType",	int L = val.getInt() - m_LaneZero;   if ((L>=0) && (L<m_NumLanes)) m_laneGUI.setTargetType( L, hlp.getWString(), ext.getFloat() ); );
		_onCLICK( "lane_GUI::loadRounds",		int L = val.getInt() - m_LaneZero;   if ((L>=0) && (L<m_NumLanes)) m_laneGUI.loadRounds( L, hlp.getInt(), ext.getInt(), 2.0f ); );
		_onCLICK( "lane_GUI::moveScope",		m_laneGUI.movescope( val.getInt() ); );
		_onCLICK( "lane_GUI::setTotalScore",	int L = val.getInt() - m_LaneZero;   if ((L>=0) && (L<m_NumLanes)) m_laneGUI.setTotalScore( L, hlp.getInt() ); );

	}
	_onCLICK( "exit",				m_pScene->rpc_call_onExit();	Sleep(50);		m_pScene->Quit();	);			// Broadcast quit, and call Scene Quit
	
	//_onCLICK( "moveInfoTop",		m_laneGUI.movescope(1);	/*(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig("spotting", L"top" );*/);
	//_onCLICK( "moveInfoBottom",		m_laneGUI.movescope(3);	/*(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig("spotting", L"bottom" );*/);
	_onCLICK( "setup_layout",		;	);
	_onCLICK( "PaperAdvance",		m_numShotsSinceLastPaperAdvance +=val.getInt(); paperAdvance();	);
	_onCLICK( "setup_camera",		/*goMenu();*/ gui_LoadImage( L"panzoomfocus.png" );	m_pCam_ZoomPanFocus->show();	m_mode = mode_CAMERA_PZF;			);
	_onCLICK( "Cam_IR1",			if (m_mode != mode_CAMERA_IR1) {m_mode = mode_CAMERA_IR1; m_pCam_IR->show(); m_pScene->setRefFrame( true, 4 ); buildDots(); }
									else {m_mode = mode_CAMERA_TEST; m_pCam_IR->hide();});
	_onCLICK( "Cam_IR2",			if (m_mode != mode_CAMERA_IR2) {m_mode = mode_CAMERA_IR2; m_pCam_IR->show(); m_pScene->setRefFrame( true, 4 );  buildDots(); }
									else {m_mode = mode_CAMERA_TEST; m_pCam_IR->hide();});
	_onCLICK( "toggle_cam_info",	if (m_pCam_Info->isHidden()) m_pCam_Info->show(); else m_pCam_Info->hide();  );
	_onCLICK( "Cam_Distortion",  	m_CamDistortionPRE_blank = 0;	buildDots(); m_pCam_Distortion->show(); m_pCam_DistortionBLACK->show();	m_pCam_DistortionWHITE->hide(); 
									m_mode = mode_CAMERA_DISTORTION;	SetCursorPos( (int)m_rect_Menu.bottomRight.x, (int)m_rect_Menu.bottomRight.y);	);
	_onCLICK( "Cam_Intensity",		m_pCam_Distortion->hide(); m_pCam_IR->hide();  m_pCam_Intensity->show();	m_mode = mode_CAMERA_TEST;	m_pScene->setRefFrame( true, 4 );	
									DmString cp = L"camparam," + m_pCam_Thresh->getText() + L"," + m_pCam_Min->getText() + L"," +
									m_pCam_Max->getText() + L"," + m_pCam_Gain->getText() + L"," + m_pCam_Gamma->getText() + L"," +
									m_pCam_Framerate->getText() + L"," + m_pCam_Resolution->getText() + L"," + m_pCam_Serial->getText();
									ewstring *cps = new ewstring( cp );
									SEND( *cps );
									m_pLogger->Log( LOG_ERROR, cp.c_str() ); 
									);
	_onCLICK( "IR1",	
									float dX = val.getFloat() * 0.1;
									float dY = hlp.getFloat() * 0.1;

									IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X].x += m_IR_Weights.x * dX;
									IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X].y += m_IR_Weights.x * dY;

									IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X+1].x += m_IR_Weights.y * dX;
									IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X+1].y += m_IR_Weights.y * dY;

									IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X].x += m_IR_Weights.z * dX;
									IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X].y += m_IR_Weights.z * dY;

									IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X+1].x += m_IR_Weights.w * dX;
									IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X+1].y += m_IR_Weights.w * dY;

									saveIRgrid();
										);
	_onCLICK( "Cam_param_Threshold",	m_pCam_Thresh->setText( DmString( val.getWString().c_str() ) );
										m_pScene->setDots( m_pCam_Thresh->getText().toFloat(), m_pCam_Min->getText().toFloat(), m_pCam_Max->getText().toFloat(), m_pScene->m_PG_dot_position );
										);
	_onCLICK( "Cam_param_DotMin",		m_pCam_Min->setText( DmString( val.getWString().c_str() ) );
										m_pScene->setDots( m_pCam_Thresh->getText().toFloat(), m_pCam_Min->getText().toFloat(), m_pCam_Max->getText().toFloat(), m_pScene->m_PG_dot_position );
										);
	_onCLICK( "Cam_param_DotMax",		m_pCam_Max->setText( DmString( val.getWString().c_str() ) );
										m_pScene->setDots( m_pCam_Thresh->getText().toFloat(), m_pCam_Min->getText().toFloat(), m_pCam_Max->getText().toFloat(), m_pScene->m_PG_dot_position );
										);
	_onCLICK( "Cam_param_Gain",			m_pCam_Gain->setText( DmString( val.getWString().c_str() ) );
										m_pScene->setPointGreyParams( m_pCam_Gamma->getText().toFloat(), m_pCam_Framerate->getText().toFloat(), m_pCam_Gain->getText().toFloat() );	
										 );
	_onCLICK( "Cam_param_Gamma",		m_pCam_Gamma->setText( DmString( val.getWString().c_str() ) );
										m_pScene->setPointGreyParams( m_pCam_Gamma->getText().toFloat(), m_pCam_Framerate->getText().toFloat(), m_pCam_Gain->getText().toFloat() );	
										);
	_onCLICK( "camera_refFrame",	m_pScene->setRefFrame( true, 4 );	);
	_onCLICK( "setup_weapons",		;	);
	_onCLICK( "setup_screen",		gui_LoadImage(L"ScreenSetup.png"); m_pScreenSetup->show();	);
	_onCLICK( "demo_mode",			m_bDemoMode = true;		gui_LoadImage( L"basicMenu.png" );	goMenu();	m_pScene->setRefFrame( true, 4 );   m_SlideShowTimer = 0;  m_bSlideShow = false;);
	_onCLICK( "exit_demo_mode",		m_bDemoMode = false;	goMenu();	);
	_onCLICK( "menu",				goMenu();	);
	_onCLICK( "spacebar",			if (m_NextDelay < 0) { m_Scenario.next(); m_NextDelay=0.5f; });
	_onCLICK( "PRONE",				setEyePosition( eye_PRONE );	 );
	_onCLICK( "SIT",				setEyePosition( eye_SIT );	 );
	_onCLICK( "KNEEL",				setEyePosition( eye_KNEEL );	 );
	_onCLICK( "STAND",				setEyePosition( eye_STAND );	 );

	_onCLICK( "SPOT_TOPLEFT",		m_laneGUI.movescope( spotting_TOPLEFT );	 );
	_onCLICK( "SPOT_TOPMIDDLE",		m_laneGUI.movescope( spotting_TOP );	 );
	_onCLICK( "SPOT_BOTTOMLEFT",	m_laneGUI.movescope( spotting_BOTTOMLEFT );	 );
	_onCLICK( "SPOT_BOTTOMMIDDLE",	m_laneGUI.movescope( spotting_BOTTOM );	 );
	_onCLICK( "SPOT_HIDE",			m_laneGUI.movescope( spotting_HIDE );	 );

	_onCLICK( "select_lane",	
								int L = val.getInt();
								if (m_b3DTerminal)
								{
									//L += m_LaneZero;
									//if ((L>=0) && (L<m_NumLanes))  {}
									//else L -= m_LaneZero;	   
									if ( L>=m_NumLanes )  L -= m_NumLanes;
								}
								m_pLogger->Log( LOG_ERROR, L"_onCLICK select_lane  %d", L );
								m_laneGUI.setActiveLane( L );			// Selects this lane for single lane shooting - Hide other show this one
								m_laneGUI.showIntro_Lanes( );	
								m_laneGUI.liveLayout(); 
								m_bWaitingForLaneSelect=false;
								);	

	_onCLICK( "VID_REPLAY",			m_pScene->videoPlay()	 );
	_onCLICK( "PRINT",				int lane = val.getInt() - m_LaneZero; 
									if (m_mode == mode_IVID_DEBRIEF)	m_Scenario.PrintVideo( );  
									if (m_mode == mode_3D)				m_Scenario.pdf_Print( lane )	 );		// FIXME add lane number in here

	_onCLICK( "rescanSQL",			loadUsers();	calculateSearch( m_SearchText );	);
	_onCLICK( "printLane",			int L = val.getInt();  m_bFirstPrint = !m_Scenario.Print() );
	_onCLICK( "resetWeaponOffset",	int L = val.getInt(); m_lanes[L].weaponOffset = f_Vector(0, -50, 0, 0); 	);
	
	
	_onCLICK( "I-Video",		//m_pLogger->Log( LOG_INFO, L"A" );
								SEND( "showVideo" );
								m_Scenario.create_videoScenario( L"Video" );
								m_Scenario.maxNumLanes( 1 );
								m_Scenario.add_exercise( L"Video", hlp.getWString().c_str(), 50 );	
								m_Scenario.add_scene( WAIT_INSTRUCTOR );
								m_Scenario.act_Loadrounds( 50 );
								m_Scenario.tr_time( 1000.0f );		// net baie groot
								m_Scenario.start();	
								m_Scenario.set_SpottingScope( spotting_BOTTOMLEFT );
								m_VideoTime = 0.0f;
								//m_pLogger->Log( LOG_INFO, L"B" );
								m_laneGUI.showIntro( guitype_TARGET, hlp.getWString(), m_Scenario.get_numLanes() );
								//m_pLogger->Log( LOG_INFO, L"C" );
								goLive( mode_INTERACTIVE_VIDEO );
								m_pScene->PlayVideo( hlp.getWString().c_str(), true );
								m_pLogger->Log( LOG_INFO, L"I-Video (%s)", hlp.getWString().c_str() );
								m_bWaitingForLaneSelect = true;
								m_pScene->videoPause();		// start paused
								//m_pLogger->Log( LOG_INFO, L"D" );
								);

	


	// Speel standaar videos terug
	if ( (cmd.find(".avi") != wstring::npos) ||
		 (cmd.find(".wmv") != wstring::npos) ||
		 (cmd.find(".mov") != wstring::npos) )
	{
		WCHAR filePath[MAX_PATH];
		if ( m_pScene->getResourceManager()->findFile( cmd.getWString().c_str(), filePath, MAX_PATH ) )
		m_pScene->PlayVideo( filePath, true );
		goLive( mode_VIDEO );
		m_laneGUI.showVideo();
		return;
	}
}

#define _onCLICK_2(x,y) if (sender.getCommandText() == x) { y; return; }
void	ctrl_DirectWeapons::toolClick(DmPanel& sender, const DmVec2& cursorPosition, DmMouseButton button)
{
	m_pLogger->Log( LOG_INFO, L"toolClick()  ... %s", sender.getCommandText().c_str() );

	if (!m_bAuthorized && !checkLicense())
	{
		m_pLogger->Log( LOG_ERROR, L"toolClick()  ... FAILED lisence check" );
		return;
	}

	if (m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		ewstring cmd = sender.getCommandText();
		ewstring val = sender.getValue();
		ewstring hlp = sender.getHelpText();
		Bs.Write( cmd.c_str() );
		Bs.Write( val.c_str() );
		Bs.Write( hlp.c_str() );
		Bs.Write( "" );
		m_pScene->rpc_broadcast( "rpc_CALL", &Bs );
	}

	

	clickProcess( ewstring(sender.getCommandText()), ewstring(sender.getValue()), ewstring(sender.getHelpText()), "" );

	if (m_ballowMouseFire)
		_onCLICK_2( L"mouseShot",		fire( cursorPosition.x, cursorPosition.y );   );

	_onCLICK_2( L"laser_live",	m_bLiveFire = !m_bLiveFire; 
								if (m_bLiveFire)	{sender.getParent()->setValue(L"live");	(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig(L"live_fire", "1" );} 
								else				{sender.getParent()->setValue(L"laser");	(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig(L"live_fire", "0" );}	);
	_onCLICK_2( L"returnName",	m_lanes[m_activeAmmoLane].userID = sender.getValue();	
								m_lanes[m_activeAmmoLane].userNAME = sender.getHelpText();
								populateAmmo( m_activeAmmoLane ) );
	_onCLICK_2( L"clearName",	m_lanes[m_activeAmmoLane].userID = L"";	
								m_lanes[m_activeAmmoLane].userNAME = L"";
								populateAmmo( m_activeAmmoLane ) );

	_onCLICK_2( L"Playlist",			m_pPlaylist->show(); m_pPlaylistSearch->setText(L""); m_SearchText=L""; calculateSearch( L"" );  );
	_onCLICK_2( L"playlistLoad",		playlistLoad(); );
	_onCLICK_2( L"playlistSave",		playlistSave(); );
	_onCLICK_2( L"playlistClear",		playlistClear(); );
	_onCLICK_2( L"playlistAdd",			playlistAdd(); );
	_onCLICK_2( L"playlistTogglePrint", playlistTogglePrint(); );
	_onCLICK_2( L"playlistPrint",		m_Scenario.playlistPrint(); );
	_onCLICK_2( L"playlistClickRow",	playlistClickRow( sender.getHelpText().toInt() ); );
	_onCLICK_2( L"playlistClickX",		m_PlaylistUsers.erase( m_PlaylistUsers.begin() + sender.getValue().toInt() ); playlistUpdateWidget(); );


	_onCLICK_2( L"QuickScenarioHandgun",		QuickRange.setDefault( QR_HANDGUN ); QuickRange.show();	);
	_onCLICK_2( L"QuickScenarioAssaultRifle",	QuickRange.setDefault( QR_ASSAULT ); QuickRange.show();	);
	_onCLICK_2( L"QuickScenarioHunting",		QuickRange.setDefault( QR_HUNTING ); QuickRange.show();	);
	_onCLICK_2( L"QuickScenario_start",			goQuickScenario();   );

	if ( sender.getCommandText().find(L".lua") != wstring::npos )
	{
		m_pLogger->Log( LOG_INFO, L"sender.getCommandText().find( .lua )");
		goLive( mode_3D );
		m_LUA_delayLoad = 2;
		m_LUA_name = sender.getCommandText();
		m_SQL_group = sender.getValue();
		m_SQL_name = sender.getHelpText();
		//m_pLogger->Log( LOG_INFO, L"H");
		m_laneGUI.showIntro( guitype_TARGET, DmString( L"" ), this->m_NumLanes );
		//m_pLogger->Log( LOG_INFO, L"I");
		
		return;
	}
	
	
}

// gebruik hierdie net op eye level te stel
void ctrl_DirectWeapons::onMouseDrag3D(DmPanel& sender, const DmVec2& dragDisplacement, const DmVec2& cursorPosition, DmMouseButton mouseButton)
{
	if(sender.getName() == L"menuSlide")
	{
		m_AnimatedPosition += dragDisplacement.x;
		m_AnimatedPosition = __min(m_AnimatedPosition, 0);
		sender.setPosition( m_AnimatedPosition, 96 );
		//m_bMouseIsDragging = true;
	}
	else
	{
		if(sender.getName() == L"center_height1")	m_eye_Position = eye_PRONE;
		if(sender.getName() == L"center_height2")	m_eye_Position = eye_KNEEL;
		if(sender.getName() == L"center_height3")	m_eye_Position = eye_STAND;
		
		float EYE_LEVEL = cursorPosition.y - 15;
		float level = 1.0f - EYE_LEVEL / m_rect_3D.height();

		switch(m_eye_Position)
		{
		case eye_PRONE:
			(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig("eye_level_1", DmString(EYE_LEVEL).c_str() );
			m_eye_level[eye_PRONE] = level;
			break;
		case eye_KNEEL:
			(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig("eye_level_2", DmString(EYE_LEVEL).c_str() );
			m_eye_level[eye_SIT] = level;
			m_eye_level[eye_KNEEL] = level;
			break;
		case eye_STAND:
			(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig("eye_level_3", DmString(EYE_LEVEL).c_str() );
			m_eye_level[eye_STAND] = level;
			break;
		
		}
		setEyePosition( m_eye_Position );
		sender.setPosition( 0, EYE_LEVEL );

		if (m_bInstructorStation)
		{
			RakNet::BitStream Bs;
			Bs.Write( (unsigned short)RPC_TOOLCLICK );
			Bs.Write( "eye_level" );
			Bs.Write( ewstring( level ).c_str() );
			Bs.Write( ewstring( m_eye_Position ).c_str() );
			Bs.Write( "" );
			m_pScene->rpc_broadcast( "rpc_CALL", &Bs );
		}
		
	}
}


void ctrl_DirectWeapons::onMouseDragCameraAdjust(DmPanel& sender, const DmVec2& dragDisplacement, const DmVec2& cursorPosition, DmMouseButton mouseButton)
{
	if (m_mode == mode_CAMERA_IR1)
	{
		float dX = dragDisplacement.x * 0.1;
		float dY = dragDisplacement.y * 0.1;

		IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X].x += m_IR_Weights.x * dX;
		IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X].y += m_IR_Weights.x * dY;

		IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X+1].x += m_IR_Weights.y * dX;
		IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X+1].y += m_IR_Weights.y * dY;

		IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X].x += m_IR_Weights.z * dX;
		IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X].y += m_IR_Weights.z * dY;

		IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X+1].x += m_IR_Weights.w * dX;
		IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X+1].y += m_IR_Weights.w * dY;

		saveIRgrid();
	}
	else if  (m_mode == mode_CAMERA_IR2)
	{
	}
}

void ctrl_DirectWeapons::eye_onMouseOver(DmPanel& sender)
{
	sender.setPosition( m_rect_3D.bottomRight.x-150, 0 );
	sender.setWidth( 150 );
}


void ctrl_DirectWeapons::eye_onMouseOut(DmPanel& sender)
{
	sender.setPosition(  m_rect_3D.bottomRight.x-1, 0 );
	sender.setWidth( 1 );
}


void	ctrl_DirectWeapons::camera_onMouseOver(DmPanel& sender)
{
	m_pCamView->show();
}
void	ctrl_DirectWeapons::camera_onMouseOut(DmPanel& sender)
{
	m_pCamView->hide();
}



void ctrl_DirectWeapons::onKeyPress(DmInput& sender, int key)
{
	if (m_mode == mode_CAMERA_TEST)
	{
		if (key == DIK_C)
		{
			m_ActiveCamera = ( m_ActiveCamera + 1 ) & 0x1;
		}
	}

	if( m_mode == mode_CAMERA_IR1  ||  m_mode == mode_CAMERA_IR2  )
	{
		float dX = 0;
		float dY = 0;
		if (key == DIK_LEFT)	dX = -0.33333333333;
		if (key == DIK_RIGHT)	dX =  0.33333333333;
		if (key == DIK_UP)		dY = -0.33333333333;
		if (key == DIK_DOWN)	dY =  0.33333333333;

		IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X].x += m_IR_Weights.x * dX;
		IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X].y += m_IR_Weights.x * dY;

		IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X+1].x += m_IR_Weights.y * dX;
		IR_dist[m_ActiveCamera][m_IR_Y][m_IR_X+1].y += m_IR_Weights.y * dY;

		IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X].x += m_IR_Weights.z * dX;
		IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X].y += m_IR_Weights.z * dY;

		IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X+1].x += m_IR_Weights.w * dX;
		IR_dist[m_ActiveCamera][m_IR_Y+1][m_IR_X+1].y += m_IR_Weights.w * dY;

		saveIRgrid();

		if (key == DIK_TAB)
		{
			m_IR_adjust_currentX ++;
			if (m_IR_adjust_currentX >= 3)
			{
				m_IR_adjust_currentX = 0;
				m_IR_adjust_currentY ++;
			}
			if (m_IR_adjust_currentY >= 3)
			{
				m_IR_adjust_currentX = 0;
				m_IR_adjust_currentY = 0;
			}
			m_IR_X = __min(1, m_IR_adjust_currentX);
			m_IR_Y = __min(1, m_IR_adjust_currentY);
			m_IR_Weights = f_Vector( 1, 0, 0, 0);
			if (m_IR_adjust_currentX == 2 && m_IR_adjust_currentY < 2)	m_IR_Weights = f_Vector( 0, 1, 0, 0);
			if (m_IR_adjust_currentX < 2 && m_IR_adjust_currentY == 2)	m_IR_Weights = f_Vector( 0, 0, 1, 0);
			if (m_IR_adjust_currentX == 2 && m_IR_adjust_currentY == 2)	m_IR_Weights = f_Vector( 0, 0, 0, 1);
		}
	}


	if (m_mode == mode_MENU)
	{
		if (key == DIK_LEFT)			// search left		
		{
			for (int i=m_MaxPage; i>=0; i-- )
			{
				if (m_PageOffsets[i] < (-m_AnimatedPosition))	{ m_MenuNewPage = i; m_MenuPage = i+1; return;}
			}
			//m_MenuNewPage = __max( 0, m_MenuPage -1 );
		}
		if (key == DIK_RIGHT)			
		{
			for (int i=1; i<=m_MaxPage; i++ )
			{
				if (m_PageOffsets[i] > (-m_AnimatedPosition))	{m_MenuNewPage = i; m_MenuPage = i+1; return;}
			}
			//m_MenuNewPage = __min( m_MaxPage, m_MenuPage + 1 );
		}
		if (key == DIK_F)			
		{
			m_bShowFPS = !m_bShowFPS;
		}
		
	}

	if (key == DIK_ESCAPE)			// usually takes us back to the menu
		{
			m_bDemoMode = false;
			goMenu();
		}


	if (key == DIK_F10)			// usually takes us back to the menu
		{
			loadIRgrid();
		}
	


	if (m_mode == mode_3D)
	{
		if ((key == DIK_SPACE) || (key == DIK_PGDN))			// ecape cancels effectively
		{
			if (m_NextDelay < 0) { m_Scenario.next(); m_NextDelay=0.5f; };
			return;
		}
		if (key == DIK_RETURN)			// ecape cancels effectively
		{
			m_lastVoiceCommand = L"pull";
			return;
		}

		switch (key)
		{
			case DIK_UPARROW:
			case DIK_DOWNARROW:
			case DIK_LEFTARROW:
			case DIK_RIGHTARROW:
			case DIK_PGUP:
			case DIK_PGDN:
			{
				m_pScene->rpc_KeyPress(m_activeGUID, key);
				break;
			}
		}
		
		return;
	}


	

	if (m_mode == mode_MENU && (!m_pAmmo->isHidden() || !m_pPlaylist->isHidden())  )
	{ 
		switch (key)
		{

		case DIK_LEFT:			m_ammoAnimNewLane = __max(0,						   m_activeAmmoLane - 1 );	  m_animTime = 0; break;
		case DIK_RIGHT:			m_ammoAnimNewLane = __min(m_Scenario.get_numLanes()-1, m_activeAmmoLane + 1 );	  m_animTime = 0; break;

		case DIK_BACKSPACE:		if(m_SearchText.size()>0) m_SearchText.erase(m_SearchText.size()-1);	break;

		case DIK_0:				m_SearchText += L"0";	break;
		case DIK_1:				m_SearchText += L"1";	break;
		case DIK_2:				m_SearchText += L"2";	break;
		case DIK_3:				m_SearchText += L"3";	break;
		case DIK_4:				m_SearchText += L"4";	break;
		case DIK_5:				m_SearchText += L"5";	break;
		case DIK_6:				m_SearchText += L"6";	break;
		case DIK_7:				m_SearchText += L"7";	break;
		case DIK_8:				m_SearchText += L"8";	break;
		case DIK_9:				m_SearchText += L"9";	break;

		case DIK_SPACE:			m_SearchText += L" ";	break;

		case DIK_A:				m_SearchText += L"a";	break;
		case DIK_B:				m_SearchText += L"b";	break;
		case DIK_C:				m_SearchText += L"c";	break;
		case DIK_D:				m_SearchText += L"d";	break;
		case DIK_E:				m_SearchText += L"e";	break;
		case DIK_F:				m_SearchText += L"f";	break;
		case DIK_G:				m_SearchText += L"g";	break;
		case DIK_H:				m_SearchText += L"h";	break;
		case DIK_I:				m_SearchText += L"i";	break;
		case DIK_J:				m_SearchText += L"j";	break;
		case DIK_K:				m_SearchText += L"k";	break;
		case DIK_L:				m_SearchText += L"l";	break;
		case DIK_M:				m_SearchText += L"m";	break;
		case DIK_N:				m_SearchText += L"n";	break;
		case DIK_O:				m_SearchText += L"o";	break;
		case DIK_P:				m_SearchText += L"p";	break;
		case DIK_Q:				m_SearchText += L"q";	break;
		case DIK_R:				m_SearchText += L"r";	break;
		case DIK_S:				m_SearchText += L"s";	break;
		case DIK_T:				m_SearchText += L"t";	break;
		case DIK_U:				m_SearchText += L"u";	break;
		case DIK_V:				m_SearchText += L"v";	break;
		case DIK_W:				m_SearchText += L"w";	break;
		case DIK_X:				m_SearchText += L"x";	break;
		case DIK_Y:				m_SearchText += L"y";	break;
		case DIK_Z:				m_SearchText += L"z";	break;
		}

		m_pName_Search->setText( m_SearchText );
		m_pPlaylistSearch->setText( m_SearchText );
		calculateSearch( m_SearchText );
	}

}


void ctrl_DirectWeapons::onKeyUp(DmInput& sender, int key)
{
	if (m_mode == mode_3D)
	{
		switch (key)
		{
			case DIK_LEFTARROW:
			case DIK_RIGHTARROW:
			{
				m_pScene->rpc_KeyUp( m_activeGUID, key);
				break;
			}
		}
	}
}
void ctrl_DirectWeapons::onKeyDown(DmInput& sender, int key)
{
	if (m_mode == mode_3D)
	{
		switch (key)
		{
			case DIK_LEFTARROW:
			case DIK_RIGHTARROW:
			{
				m_pScene->rpc_KeyDown( m_activeGUID, key);
				break;
			}
		}
	}
}


void ctrl_DirectWeapons::playSound( DmString name, bool bMute )
{
//	WCHAR filePath[MAX_PATH];
//	if ( m_pScene->getResourceManager()->findFile( name.c_str(), filePath, MAX_PATH ) )
	{
		SoundSource *m_pSoundSource = NULL;
		m_pSoundSource = SoundManager::GetSingleton()->createSoundSourceFromFile( name.c_str() );
		if (m_pSoundSource) m_pSoundSource->playImmediate();
	}
}







void ctrl_DirectWeapons::hideGUI()
{
	m_pMenu->hide();
	m_pMenu3D->hide();
	//m_pLanes->hide();
	//m_pSelectName->hide();
//	m_pIntro->hide();
	m_pLive->hide();
//	m_pFeedback->hide();
	m_pAmmo->hide();
	m_pAmmo->findWidget( L"boreSight" )->hide(); 
	m_pCam_ZoomPanFocus->hide();
	m_pCam_Distortion->hide();
	m_pCam_Intensity->hide();
	m_p_ImageOverlay->hide();
	m_p_IntroOverlay->hide();
	m_p_BloodSplatter->hide();
	//m_pScenarioBuilder->hide();
	m_pScreenSetup->hide();
	QuickRange.hide();

	
}

void ctrl_DirectWeapons::goMenu()
{
	m_NextDelay = 0.0f;
	m_MENU_loadDelay = 5.0;	// so we wait 5 seconds at least
	//if (m_mode == mode_CAMERA_TEST)
	{
		DmString logname = L"../../camera_logs/";
		m_pScene->PGdump( (LPWSTR)logname.c_str() );
	}

	if ( m_b3DTerminal ) SetCursorPos( m_rect_3D.width()/2, m_rect_3D.height() );
	m_pScene->getGUI()->setRenderThrottle(dmNoThrottling);

	if (m_mode == mode_INTERACTIVE_VIDEO || (m_mode == mode_IVID_DEBRIEF) && m_bAutoPrint)		//????
	{
		m_Scenario.PrintVideo( );
	}

/*	if (m_mode == mode_3D)
	{
		if (m_bAutoPrint)
		{
			m_pLogger->Log( LOG_ERROR, L"goMenu() ... m_bAutoPrint" );
			m_Scenario.Print( );
		}
	}*/
		
	m_LUA_delayLoad = -100;

	hideGUI();
	m_pMenu->show();
	m_pMenu3D->show();
	m_pScene->fastload_enableRender( false );
	m_Scenario.stop();
	m_Scenario.m_max_lanes = m_Scenario.m_pc_lanes;	// reset to maximum
	m_mode = mode_MENU;
	//m_bAutoPrint = false;
	//m_bAutoPrint = m_bPlaylistAutoPrint; // TYDELIK
	m_laneGUI.movescope( 0 );		// hide the spottign scope
	m_bWaitingForLaneSelect = false;

	// FIXME aanvaar ons kom van Screen Setup af terug en doen dit hier die herhaling is nie te sleg nie
	// Some debug lanes ----------------------------------------------------------------------------------------
	(*m_pCfgMng)["controller_DirectWeapons"].setConfig("lanes", m_pScreenSetup->findWidget( L"SS_lanes" )->getText().c_str() );
	(*m_pCfgMng)["controller_DirectWeapons"].setConfig("screen_width", m_pScreenSetup->findWidget( L"SS_width" )->getText().c_str() );
	(*m_pCfgMng)["controller_DirectWeapons"].setConfig("shooting_distance", m_pScreenSetup->findWidget( L"SS_distance" )->getText().c_str() );
	float DST = m_pScreenSetup->findWidget( L"SS_distance" )->getText().toFloat() / 1000.0f;
	m_pMenu->findWidget( L"Distance" ) ->setText( DmString(DST, 1) + L" m" );
	for (unsigned int i=0; i< m_lanes.size(); i++)
	{
		ewstring lane_str = "lane_" + ewstring( i );
		if (!m_pCfgMng->hasGroup(lane_str))				m_pCfgMng->addGroup(lane_str);
		(*m_pCfgMng)[lane_str].setConfig( "active_ammo", m_lanes[i].ammo.ammoIndex );
		(*m_pCfgMng)[lane_str].setConfig( "active_ammo_X", m_lanes[i].weaponOffset.x );
		(*m_pCfgMng)[lane_str].setConfig( "active_ammo_Y", m_lanes[i].weaponOffset.y );
	}
	(*m_pCfgMng)[L"controller_DirectWeapons"].setConfig(L"menuPosition", ewstring(m_AnimatedPosition).c_str() );
	if( !m_pCfgMng->saveConfig() ) m_pLogger->Log( LOG_ERROR, L"Saving Config file failed" );


	int L = m_pScreenSetup->findWidget( L"SS_lanes" )->getText().toInt();
	float W = m_pScreenSetup->findWidget( L"SS_width" )->getText().toFloat();
	float D = m_pScreenSetup->findWidget( L"SS_distance" )->getText().toFloat();

	float fov = 2.0f * atan2(W/2, D) * 57.296f / m_FOV_scale;
	m_Camera.setFOV( fov );

	if (m_pLUAscript)
	{
		m_pLUAscript->terminate();
		m_pLUAscript = NULL;
	}
	m_Scenario.stop();

	if (m_bDemoMode)
	{
		hideGUI();
		m_pLive->show();
		gui_LoadImage( L"basicMenu.png" );
	}

	if(m_pScene->videoIsPlaying()) { m_pScene->videoStop(); }

	if(!m_b3DTerminal )
	{
		m_pScene->rpc_call_ClearAll();	// maak die server scene skoon
		//m_pLogger->Log( LOG_ERROR, L"goMenu() ... m_pScene->rpc_call_ClearAll();" );
	}

	bBoresight = false;
	zigbeeRounds( 0, 0, true );		// turn all air off
	m_laneGUI.cancelReplay();		// FIXME only in 3D

	set_MOI_to_Camera( 0 );

	// play;ists
	playlistGoMenu( m_pPlaylist->isHidden() );
	m_pPlaylist->hide();



	if (!m_bAuthorized)		m_pMenuSlide->hide();
	if ( m_b3DTerminal )	m_pMenuSlide->hide();
	else					m_pMenuSlide->show();

	if (m_bInstructorStation)
	{
		m_pMenuSlide->show();

		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "go_Menu" );
		Bs.Write( "" );
		Bs.Write( "" );
		Bs.Write( "" );
		m_pScene->rpc_call( "rpc_CALL", &Bs );
	}

}
/*
void ctrl_DirectWeapons::goLanes()
{
	hideGUI();
	resetNames();
	updateLanes();
	//m_pLanes->show();
}
*/
//void ctrl_DirectWeapons::goSelectName()
//{
	//m_pSelectName->show();
//}

void ctrl_DirectWeapons::goIntro()
{
	m_IntroTimer = 5.0f;  // 5 second wait to live
	hideGUI();
	m_pIntro->show();

	if (m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "go_Intro" );
		Bs.Write( ewstring( m_mode ).c_str() );
		Bs.Write( "" );
		Bs.Write( "" );
		m_pScene->rpc_call( "rpc_CALL", &Bs );
	}
}

void ctrl_DirectWeapons::goLive(int mode)
{
	if ( m_b3DTerminal ) SetCursorPos( m_rect_3D.width()/2, m_rect_3D.height() );
	m_mode = mode;
	hideGUI();	
	m_pLive->show();
	m_pScene->fastload_enableRender( true );
	m_pScene->getGUI()->setRenderThrottle(dmHighThrottling);
	m_pScene->rpc_call_loadComplete();			// call this to start te wait for scene complete.

	if (m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "go_Live" );
		Bs.Write( ewstring( m_mode ).c_str() );
		Bs.Write( "" );
		Bs.Write( "" );
		m_pScene->rpc_call( "rpc_CALL", &Bs );
	}
}

void ctrl_DirectWeapons::goFeedback()
{
	hideGUI();
	m_pFeedback->show();
}

void ctrl_DirectWeapons::showRanking(unsigned int lane)
{
	if( m_lanes.size() <= lane ) return;

	int userIdx = -1;

	_user *pU[5];
	memset(pU, 0, sizeof(pU));
	
	list<_user>::iterator it;
	int idx = 0;
	for (it=m_users.begin(); it!= m_users.end(); it++)
	{
		if (!it->id.compare(m_lanes[lane].userID))
		{
			userIdx = idx;
			break;
		}
		idx++;
	}

	int fromIdx = std::max(0, 2 - idx);
	it = m_users.begin();
	std::advance(it, std::max(idx - 2, 0));
	
	for (int i = fromIdx; i < 5; i++)
	{
		if (it == m_users.end())
			break;
		pU[i] = &(*it++);
	}

	if (pU[2])
		dw_scenario::s_pCTRL->m_laneGUI.showRanking( lane, pU, (idx)+1 );

}

void ctrl_DirectWeapons::updateBestScore(DmString ID, int score)
{
	list<_user>::iterator it;
	for (it=m_users.begin(); it!= m_users.end(); it++)
	{
		if (it->id.find(ID) == 0)
		{
			it->bestScore = std::max(it->bestScore, score);
		}
	}
}

/*
int ctrl_DirectWeapons::ln_numActive()
{
	
	int cnt = 0;
	for (unsigned int i=0; i<m_lanes.size(); i++)
	{
		if (m_lanes[i].bActive) cnt ++;
	}
	return cnt;

}

int ctrl_DirectWeapons::ln_leastRecent()
{

	int ln = 0;
	int cnt = 0;
	for (unsigned int i=0; i<m_lanes.size(); i++)
	{
		if (m_lanes[i].bActive && m_lanes[i].cnt > cnt)
		{
			cnt = m_lanes[i].cnt;
			ln = i;
		}
	}
	return ln;

}

void ctrl_DirectWeapons::updateLanes()
{

	// maak eers seker dat ons nie te veel lane aan het nie
	while (ln_numActive() > m_Scenario.get_numLanes() )
	{
		m_lanes[ ln_leastRecent() ].bActive = false;
	}

	// now display them
	bool bActive = false;
	for (unsigned int i=0; i<m_lanes.size(); i++)
	{
		if (m_lanes[i].bActive) bActive = true;

		if (m_lanes[i].userNAME != L"")
		{
			dynamic_cast<DmPanel*>(m_pLanes->findWidget(  L"name" + DmString(i) ))->setText(m_lanes[i].userNAME);
			dynamic_cast<DmPanel*>(m_pLanes->findWidget(  L"name" + DmString(i) ))->setStyleName( L"metro_darkblue" );
		}
		else 
		{
			dynamic_cast<DmPanel*>(m_pLanes->findWidget(  L"name" + DmString(i) ))->setText( L"Click to select a name" );
			dynamic_cast<DmPanel*>(m_pLanes->findWidget(  L"name" + DmString(i) ))->setStyleName( L"metro_yellow" );
		}
		if (m_lanes[i].bActive)
		{
			dynamic_cast<DmPanel*>(m_pLanes->findWidget(  L"B_" + DmString(i) ))->setText( L"" );
			m_lanes[i].info->show();
		}
		else
		{
			dynamic_cast<DmPanel*>(m_pLanes->findWidget( L"B_" + DmString(i) ))->setText( L"Click to select" );
			m_lanes[i].info->hide();
		}
	}

	if (bActive)	dynamic_cast<DmPanel*>(m_pLanes->findWidget(  L"next" ))->show();
	else			dynamic_cast<DmPanel*>(m_pLanes->findWidget(  L"next" ))->hide();

}

void ctrl_DirectWeapons::toggleLane( unsigned int lane )
{
	if (lane >= m_lanes.size()) return;
	m_lanes[ lane ].bActive = !m_lanes[ lane ].bActive;
	for (unsigned int i=0; i<m_lanes.size(); i++)
	{
		if( !m_lanes[i].bActive ) 
		{
			m_lanes[i].userNAME = L"";
			m_lanes[i].userID = L"";
		}
		m_lanes[i].cnt ++;
	}
	m_lanes[lane].cnt = 0;

	// Automatically Opens up if we have users in our datatabase - and we go active
	if (m_lanes[lane].bActive && m_users.size()>0)
	{
		m_SearchText = L"";
		m_pName_Search->setText( L"" );	
		calculateSearch( m_SearchText );	
		m_lane_for_name=lane;	
		//m_pSelectName->show();

	}

	updateLanes();
}
*/
void ctrl_DirectWeapons::resetNames()
{
	return;
	/*
	for (int i=0; i<m_lanes.size(); i++)
	{
		m_lanes[ i].bActive = false;
		m_lanes[i].userNAME = L"";
		m_lanes[i].userID = L"";
		dynamic_cast<DmPanel*>(m_pLanes->findWidget( L"name"+DmString(i) ))->setText(L"");
		dynamic_cast<DmPanel*>(m_pLanes->findWidget( L"name"+DmString(i) ))->setStyleName(L"metro_yellow");
	}
	*/
}

void ctrl_DirectWeapons::calculateSearch(DmString TXT)
{
	/*	SKOU FIXME 
		Hirdie werk nie 100% nie
		dit gaan beter wees om die input strign in 'n stel los woorde in te breek en elkeen appart te toets teen alle mootnlikhede - maar vir nou gaan ek aan
		ook - backspace moet op keypress wees sodat it kan herhaal
		Ons kort ook nog mouse highligth po die antwoorde wat wys mens mag dit click
		*/
	//m_pLogger->Log( LOG_INFO, L"calculateSearch( %s ) with %d names", TXT.c_str(), m_users.size() );

	list<_user>::iterator it;
	for (it=m_users.begin(); it != m_users.end(); it++)		// calc all the scores
	{
		it->score = 0.0;
		if (TXT.size() == 0) it->score = 0.1;

		if (it->surname.length()>0)
		{
			if( TXT == it->surname)			it->score += 2;							// surname
			if( TXT.find( it->surname ) !=wstring::npos )		it->score += 1;		
			if( it->surname.find( TXT ) !=wstring::npos )		it->score += 1;
			if( it->surname.find( TXT ) ==0 )		it->score += 1;
		}

		if (it->initials.length()>0)
		{
			if( TXT == it->initials)			it->score += 2;							// initials
			if( TXT.find( it->initials ) !=wstring::npos )		it->score += 1;		
			if( it->initials.find( TXT ) !=wstring::npos )		it->score += 1;
			if( it->initials.find( TXT ) ==0 )					it->score += 1;
		}

		if (it->rank.length()>0)
		{
			if( TXT == it->rank)			it->score += 2;							// rank
			if( TXT.find( it->rank ) !=wstring::npos )		it->score += 1;		
			if( it->rank.find( TXT ) !=wstring::npos )		it->score += 1;
			if( it->rank.find( TXT ) ==0 )					it->score += 1;
		}

		if (it->userID.length()>0)
		{
			if( TXT == it->userID)			it->score += 2;							// id
			if( TXT.find( it->userID ) !=wstring::npos )		it->score += 1;		
			if( it->userID.find( TXT ) !=wstring::npos )		it->score += 1;
			if( it->userID.find( TXT ) ==0 )					it->score += 1;
		}

		if (it->unit.length()>0)
		{
			if( TXT == it->unit)			it->score += 2;							// unit
			if( TXT.find( it->unit ) !=wstring::npos )		it->score += 1;		
			if( it->unit.find( TXT ) !=wstring::npos )		it->score += 1;
			if( it->unit.find( TXT ) ==0 )					it->score += 1;
		}

		if (it->subunit.length()>0)
		{
			if( TXT == it->subunit)			it->score += 2;							// subunit
			if( TXT.find( it->subunit ) !=wstring::npos )		it->score += 1;		
			if( it->subunit.find( TXT ) !=wstring::npos )		it->score += 1;
			if( it->subunit.find( TXT ) ==0 )					it->score += 1;
		}

		if (TXT.length() == 0) it->score = 0;
	}

	m_users.sort(cmp_Users);

	
	if (!m_pAmmo->isHidden() )
	{
		//m_pLogger->Log( LOG_INFO, L"calculateSearch .. if (!m_pAmmo->isHidden() )" );
		it=m_users.begin();
		int maxUsers = std::min(8, (int)m_users.size());
		for (int i=0; i<maxUsers; i++)
		{
			if (it->score > 0)
			{
				m_pName_Results[i]->show();
				dynamic_cast<DmPanel*>(m_pName_Results[i]->findWidget(L"name"))->setText( it->displayName );
				dynamic_cast<DmPanel*>(m_pName_Results[i]->findWidget(L"name"))->setHorizontalTextAlignment( dmAlignLeft );
				dynamic_cast<DmPanel*>(m_pName_Results[i]->findWidget(L"id"))->setText( it->userID );
				dynamic_cast<DmPanel*>(m_pName_Results[i]->findWidget(L"id"))->setHorizontalTextAlignment( dmAlignLeft );
				dynamic_cast<DmPanel*>(m_pName_Results[i]->findWidget(L"unit"))->setText( it->unit );
				dynamic_cast<DmPanel*>(m_pName_Results[i]->findWidget(L"unit"))->setHorizontalTextAlignment( dmAlignLeft );
				dynamic_cast<DmPanel*>(m_pName_Results[i]->findWidget(L"sub"))->setText( it->subunit );
				dynamic_cast<DmPanel*>(m_pName_Results[i]->findWidget(L"sub"))->setHorizontalTextAlignment( dmAlignLeft );

				dynamic_cast<DmPanel*>(m_pName_Results[i]->findWidget(L"result"))->setValue(it->id);
				dynamic_cast<DmPanel*>(m_pName_Results[i]->findWidget(L"result"))->setHelpText(it->displayName);
			}
			else
			{
				m_pName_Results[i]->hide();
			}

			it ++;
		}
	}

	if(!m_pPlaylist->isHidden())
	{
		//m_pLogger->Log( LOG_INFO, L"calculateSearch .. if(!m_pPlaylist->isHidden())" );
		m_PlaylistNewUsers.clear();
		it=m_users.begin();
		for (int i=0; i<m_users.size(); i++)
		{
			if (it->score > 0)
			{
				int ID = it->id.toInt();
				if (m_PlaylistUsers.size() >0)
				{
					if(std::find(m_PlaylistUsers.begin(), m_PlaylistUsers.end(), ID)  ==  m_PlaylistUsers.end()){
						m_PlaylistNewUsers.push_back( ID );
					}
				}
				else
				{
					m_PlaylistNewUsers.push_back( ID );
				}
			}
			it ++;
		}
		//m_pLogger->Log( LOG_INFO, L"calculateSearch .. about to playlistUpdateWidget()" );
		playlistUpdateWidget();
	}
}




	

/*	Fires a single shot
	1) determines lane
	2) stop if blocked
	3) stop if no ammo left
	4) call scene for coordinates , and rpc bullet
	5) play sound, and exit particles, and flash
	*/
void ctrl_DirectWeapons::fire(float x, float y)
{
	int lane = m_laneGUI.getLanefromCursor( x ) ;
	//m_pLogger->Log( LOG_ERROR, L"fire(%d, %d) - lane( %d ) -----------------------------------------", (int)x, (int)y, lane);
	if (lane < 0 ) return;
	if (lane > m_lanes.size() )
	{
		//m_pLogger->Log( LOG_ERROR, L"BAD LANE %d pix in laan %d", x, lane);
		return;
	}

	int shotNumber = m_laneGUI.addShot( lane );
	if (shotNumber < 0) return;							// not a valif shot - probably to soon after previous shot - return

	//m_pLogger->Log( LOG_ERROR, L"fire  shotNumber( %d ) ", shotNumber );
	_ammo Ammo = m_lanes[lane].ammo;
	zigbeeFire( lane );
	m_Scenario.onFire( lane );

	// add offsets in --------------------------------------------------------------------------------------------------------
	x += m_lanes[lane].weaponOffset.x;
	y += m_lanes[lane].weaponOffset.y;
	//m_pLogger->Log( LOG_ERROR, L"fire  offsets( %d, %d ) ", m_lanes[lane].weaponOffset.x, m_lanes[lane].weaponOffset.y );

	// vector-----------------------------------------------------------------------------------------------------------------
	d_Vector dir, orig;
	m_pScene->calcRayFromClick( x, y, &dir, &orig );
		
	// sounds particles flash--------------------------------------------------------------------------------------------
	SoundSource *m_pSoundSource = SoundManager::GetSingleton()->createSoundSourceFromFile( L"Beretta_shot.wav" );
	if (m_pSoundSource)			m_pSoundSource->playImmediate();
	//else m_pLogger->Log( 0, L"failed Beretta_shot.wav" );

	float shotFOV = 0.040f;														
	if ( Ammo.shot_spread > 0 ) shotFOV = atan( Ammo.shot_spread ) * 4.0f;		// shotgun
	if (Ammo.overlay == L"shot_overlay_pistol") shotFOV = 0.090f;
	if (Ammo.overlay == L"shot_overlay_rifle") shotFOV = 0.007f;
	if (Ammo.overlay == L"shot_overlay_scope") shotFOV = 0.005f;
	


	LPDIRECT3DSURFACE9 pIMG;
	if ( m_mode == mode_INTERACTIVE_VIDEO)
	{
		shotFOV = 0.050f;
		pIMG = m_laneGUI.onFire( lane, shotNumber, Ammo.name, ew_Scene::GetSingleton()->getVidTime(), 0.0f, Ammo.overlay, orig, dir, shotFOV );
	}
	else
	{
		//m_pLogger->Log( LOG_ERROR, L"m_laneGUI.onFire( %d, %d, %s,  ", lane, shotNumber, Ammo.name.c_str() );
		if (m_b3DTerminal)		pIMG = m_laneGUI.onFire( lane, shotNumber, Ammo.name, 1.0f, 0.0f, Ammo.overlay, orig, dir, shotFOV );
		else					pIMG = m_laneGUI.onFire( lane, shotNumber, Ammo.name, m_Scenario.getExTime(), 0.0f, Ammo.overlay, orig, dir, shotFOV );
		//pIMG = m_laneGUI.onFire( lane, shotNumber, Ammo.name, 1.0f, 0.0f, Ammo.overlay, orig, dir, shotFOV );
	}


	//m_pLogger->Log( LOG_ERROR, L"fire  renderShot ( %d, %d, %d, (%d, %d, %d), (%d, %d, %d), %d, %d ) ", (int)Ammo.m_muzzleVelocity, (int)x, (int)y, (int)orig.x, (int)orig.y, (int)orig.z, (int)(dir.x*1000), (int)(dir.y*1000), (int)(dir.z*1000), pIMG, (int)(shotFOV*1000)  );
	m_pScene->renderShot( Ammo.m_muzzleVelocity, x, y, orig, dir, pIMG, shotFOV );

	//D3DXSaveSurfaceToFile( L"renderShot.jpg", D3DXIFF_JPG, pIMG, NULL, NULL );
	if ( m_bLiveFire ) m_pScene->setRefFrame( false, 4 );

	if ( m_mode == mode_3D)
	{	
		orig.y -= m_lanes[lane].ammo.m_barrelDrop;
		dir.y += m_lanes[lane].ammo.m_barrelRotation;
		dir.normalize();
		//m_pLogger->Log( LOG_ERROR, L"fire  rpc_Fire  "  );
		rpc_Fire( orig+(dir*0.3), dir, Ammo.m_muzzleVelocity, Ammo.m_Cd, Ammo.m_Weight, Ammo.shot_count, Ammo.shot_spread );
	}

}


void ctrl_DirectWeapons::rpc_Fire( d_Vector start, d_Vector direction, float velocity, float cd, float weight, unsigned short shotCount, float shotSpread )
{
	RakNet::BitStream Bs;
	writeVector( Bs, start );
	writeVector( Bs, direction );
	Bs.Write( velocity );
	Bs.Write( cd );
	Bs.Write( weight );
	Bs.Write( shotCount );
	Bs.Write( shotSpread );
	m_pScene->rpc_call( "fireDirectBullet", &Bs );
}




void	ctrl_DirectWeapons::hideOverlay()	
{	
	// FIXME ADD RAKNET
	if (m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "hide_overlay" );
		Bs.Write( "" );
		Bs.Write( "" );
		Bs.Write( "" );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}

	m_p_ImageOverlay->hide(); 
	m_p_IntroOverlay->hide(); 
}


void ctrl_DirectWeapons::gui_showIntro( int ex, DmString name, int shootingposition, int numrounds, DmString target, DmString desc, int distance )
{
	// FIXME ADD RAKNET
	if (m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_SHOWINTRO );
		Bs.Write( (short)ex );
		Bs.Write( ewstring(name).c_str() );
		Bs.Write( (short)shootingposition );
		Bs.Write( (short)numrounds );
		Bs.Write( ewstring(target).c_str() );
		Bs.Write( ewstring(desc).c_str() );
		Bs.Write( (short)distance );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
		return;
	}

	m_p_IntroOverlay->show();

	m_p_IntroExercise->setText( DmString(L"exercise ") + DmString(ex) + DmString(L"  :  ") );
	m_p_IntroExercise->hide();

	m_p_IntroTarget->setValue( L"target_" + target );
	m_p_IntroDistance->setText(  DmString( distance ) + L" m"  );

	switch (shootingposition)
	{
	case eye_PRONE:	m_p_IntroPosition->setValue(  L"icon_prone"  );	break;
	case eye_SIT:	m_p_IntroPosition->setValue(  L"icon_sitting"  );	break;
	case eye_KNEEL:	m_p_IntroPosition->setValue(  L"icon_kneeling"  );	break;
	case eye_STAND:	m_p_IntroPosition->setValue(  L"icon_standing"  );	break;
	}

	m_p_IntroBullet[0]->setText( DmString( numrounds ) + L" rounds" );
//	m_p_IntroBullet[1]->setText( DmString( distance ) + L" m" );
//	m_p_IntroBullet[2]->setText( desc );
//	if (desc.length() == 0) m_p_IntroBullet[2]->hide();
//	else					m_p_IntroBullet[2]->show();
//	m_p_IntroBullet[3]->hide();

	m_p_IntroName->setText( name );

	// Tokenise description
	for (int i=1; i<5; i++)	m_p_IntroBullet[i]->hide();
	if ( desc.length() > 0 )
	{
		int IDX = 1;
		DmString TOK;
		size_t start = 0;
		size_t found = -2;
		do
		{
			start = found + 2;
			found = desc.find( L",", start);
			TOK = desc.substr( start, found-start );
			m_p_IntroBullet[IDX]->setText( TOK );
			m_p_IntroBullet[IDX]->show();
			IDX ++;
		} while ( (found != wstring::npos) && (IDX<5) );
	}
}

void	ctrl_DirectWeapons::gui_LoadImage( LPCWSTR image )
{
	LPDIRECT3DTEXTURE9 pTex;

	DmSpriteManager& sprites = *m_pScene->getSpriteManager();
	pTex = sprites.getSprites().find( L"blank_image" )->second->getColourMap();

	if (pTex)
	{
		D3DLOCKED_RECT lr;
		pTex->LockRect( 0, &lr, NULL, 0 );
		pTex->UnlockRect( 0 );
		WCHAR filePath[MAX_PATH];
		if ( m_pScene->getResourceManager()->findFile( image, filePath, MAX_PATH ) )
		{
			LPDIRECT3DSURFACE9 pSurf;
			for (int i=0; i<5; i++)
			{
				pTex->GetSurfaceLevel( i, &pSurf );
				if (pSurf)
				{
					HRESULT hr = D3DXLoadSurfaceFromFile(  pSurf, NULL, NULL, filePath , NULL, D3DX_DEFAULT, 0, NULL );
					pSurf->Release();
				}
			}
	
		}
		
	}
	m_p_ImageOverlay->setStyleName( L"blank_image" );		//.. forcses screen refresh
	m_p_ImageOverlay->show();

}

#define IDX(x, y) ((y*m_pScene->m_PG_height/480)*m_pScene->m_PG_width +(x*m_pScene->m_PG_width/752))

void	ctrl_DirectWeapons::calculateWARP()
{
	for (int y=0; y<480; y++)
	{
		for (int x=0; x<752; x++)
		{
			m_pScene->m_pwarpX[y][x] = 0;
			m_pScene->m_pwarpY[y][x] = 0;
		}
	}

	float scaleX = this->m_rect_3D.width() / 752;
	float scaleY = this->m_rect_3D.height() / 480;
	for (int y=0; y<480; y++)
	{
		for (int x=0; x<752; x++)
		{
			f_Vector SCR = video_to_Screen( f_Vector(x, y, 0, 0), 0 );
			int dX = (int)(SCR.x / scaleX / 2);
			int dY = (int)(SCR.y / scaleY);
			dX = __min( dX, 751 );
			dX = __max( dX, 0 );
			dY = __min( dY, 479 );
			dY = __max( dY, 0 );
			m_pScene->m_pwarpX[dY][dX] = x;
			m_pScene->m_pwarpY[dY][dX] = y;
		}
	}
}

void	ctrl_DirectWeapons::WARPImage()
{
	calculateWARP();
	for (int y=0; y<480; y++)
	{
		for (int x=0; x<752; x++)
		{
			m_pScene->m_pwarpFrame[y][x] = m_pScene->m_pvidFrame[m_ActiveCamera][IDX(m_pScene->m_pwarpX[y][x], m_pScene->m_pwarpY[y][x])];
		}
	}
}


void	ctrl_DirectWeapons::copyImage( bool bColour, LPWSTR filename, D3DXIMAGE_FILEFORMAT filetype )
{
	if ( this->m_mode == mode_MENU ) return;
	if( m_pScene->m_pvidFrame[m_ActiveCamera] == NULL ) return;
	if( m_pScene->m_pvidThresh[m_ActiveCamera] == NULL ) return;
	if( m_pScene->m_pvidRef[m_ActiveCamera] == NULL ) return;

	//WARPImage();

	//m_pCamView->setSize( m_pScene->m_PG_width, m_pScene->m_PG_height );

	static int FrameCount = 0;
	FrameCount ++;
	if (!m_pScene->isPointGreyConnected())		return;

	DmSpriteManager& sprites = *m_pScene->getSpriteManager();
	LPDIRECT3DTEXTURE9 pTex = sprites.getSprites().find( L"video_placeholder" )->second->getColourMap();
	int threshold = m_pCam_Thresh->getText().toInt();
	if (pTex)
	{
		
		D3DLOCKED_RECT lr;
		pTex->LockRect( 0, &lr, NULL, 0 );
		D3DSURFACE_DESC S;
		pTex->GetLevelDesc( 0, &S );
		memset( lr.pBits, 255, S.Width * S.Height * 4 );

		unsigned char *data = (unsigned char*)lr.pBits;
		{
			for(int y=479; y>=0;y--)
			{
				for (int x=0; x<752; x++)
				{
					bool bthresh = false;
					//unsigned char val = m_pScene->m_pwarpFrame[y][x];//m_pScene->m_pvidFrame[m_ActiveCamera][IDX(x,y)];
					int idx = IDX(x,y);
					unsigned char val = m_pScene->m_pvidFrame[m_ActiveCamera][idx];
					if (val >= m_pScene->m_pvidRef[m_ActiveCamera][idx] +  threshold) bthresh = true;  // gebruik hier PG threshold as ek kan
						
					*(data + y*752*4 + x*4 + 3) = 255;

					if (bColour)
					{
						if (bthresh)
						{
							*(data + y*752*4 + x*4 + 0) = val >> 1;
							*(data + y*752*4 + x*4 + 1) = __min( 255, val << 1);
							*(data + y*752*4 + x*4 + 2) = __min( 255, val << 2);
						}
						else
						{
							*(data + y*752*4 + x*4 + 0) = val >> 1;
							*(data + y*752*4 + x*4 + 1) = 0;
							*(data + y*752*4 + x*4 + 2) = 0;
						}

						/*
						if (m_pScene->m_pvidThresh[m_ActiveCamera][IDX(x,y)] >= 254)
						{
							//*(data + y*752*4 + x*4 + 0) = *(data + y*752*4 + x*4 + 0) + 128;
							*(data + y*752*4 + x*4 + 1) = *(data + y*752*4 + x*4 + 1) + 128;
							*(data + y*752*4 + x*4 + 2) = *(data + y*752*4 + x*4 + 2) + 128;
						}*/
					}


					if (m_mode == mode_CAMERA_DISTORTION && m_bSplitScreen)
					{
					}
					else if (m_mode == mode_CAMERA_DISTORTION )
					{
						*(data + y*752*4 + x*4 + 0) = 0;
						*(data + y*752*4 + x*4 + 1) = 0;
						*(data + y*752*4 + x*4 + 2) = 0;
						*(data + y*752*4 + x*4 + 3) = 255;
					}
					else if (m_mode == mode_CAMERA_PZF)
					{
						*(data + y*752*4 + x*4 + 0) = val;
						*(data + y*752*4 + x*4 + 1) = val;
						*(data + y*752*4 + x*4 + 2) = val;
						*(data + y*752*4 + x*4 + 3) = 255;
					}
				}
			}
		}
		pTex->UnlockRect( 0 );
		RECT r;
		r.left = 0;
		r.right = 751;
		r.top = 0;
		r.bottom = 479;
		pTex->AddDirtyRect( &r );
		pTex->GenerateMipSubLevels();

			
		pTex->LockRect( 1, &lr, NULL, 0 );
		pTex->GetLevelDesc( 1, &S );
		memset( lr.pBits, 255, S.Width * S.Height * 4 );
			
		data = (unsigned char*)lr.pBits;
		{
			for(int y=239; y>=0;y--)
			{
				for (int x=0; x<376; x++)
				{
					unsigned char val = m_pScene->m_pvidFrame[m_ActiveCamera][IDX(x*2,y*2)];
						
					*(data + y*376*4 + x*4 + 0) = val;
					*(data + y*376*4 + x*4 + 1) = val;
					*(data + y*376*4 + x*4 + 2) = val;
					*(data + y*376*4 + x*4 + 3) = 255;
				}
			}
		}
		pTex->UnlockRect( 1 );
	}
	if (filename)
	{
		D3DXSaveTextureToFile( filename, filetype, pTex, NULL );
		//D3DXSaveTextureToFile( L"../../config/calibration.tga", D3DXIFF_TGA, pTex, NULL );
	}
}



void	ctrl_DirectWeapons::gui_LoadLabviewImage()
{
	static int SHOTCNT = 0;
	

	static int PZF_CNT = 100;
	if ((m_mode == mode_CAMERA_TEST)  || (m_mode == mode_MENU))
	{
		PZF_CNT --;
		if (PZF_CNT<=0)
		{
			PZF_CNT = 100;
			m_pScene->setRefFrame( true, 4 );
		}
	}
	
	
	
	if (m_mode == mode_CAMERA_DISTORTION ) m_pScene->bAccumulateDots = false;
	int okDOTS = m_pScene->numDots[m_ActiveCamera];
	int DOT_cnt = 0;
	int row[9];
	if (m_mode == mode_CAMERA_DISTORTION && okDOTS==45)
	{
		m_pLogger->Log( LOG_INFO, L"mode_CAMERA_DISTORTION 45 dots activeCam(%d)  ", m_ActiveCamera );

		m_pScene->bAccumulateDots = false;
		static int WAIT = 2;
		WAIT --;
		if (WAIT <= 0)
		{
			// stoor hier die inligting - gaan automaties voort
			for (int y=4; y>=0; y--)
			{
				for (int kk=0; kk<9; kk++)
				{
					row[kk] = DOT_cnt;
					DOT_cnt ++;
				}

				// sorteer nou die ry links na regs - somemr bubble
				int tmp;
				for (int j=0; j<8; j++)
				{
					for (int k=j+1; k<9; k++)
					{
						if ( m_pScene->dots[m_ActiveCamera][ row[j] ].x > m_pScene->dots[m_ActiveCamera][ row[k] ].x )
						{
							tmp = row[j];
							row[j] = row[k];
							row[k] = tmp;
						}
					}
				}


				for (int x=0; x<9; x++)
				{
					distortion_grid[y][x].video = f_Vector( m_pScene->dots[m_ActiveCamera][ row[x] ].x, m_pScene->dots[m_ActiveCamera][ row[x] ].y, 0 );
				}
			}


			m_pLogger->Log( LOG_INFO, L"mode_CAMERA_DISTORTION now save to disk  " );

			FILE *pDfile;
			FILE *pDfile2;

			if ( m_ActiveCamera == 0)
			{
				pDfile = fopen("../../config/camera_distortion.config", "wb");
				pDfile2 = fopen("../../config/camera_distortion.txt", "w");
			}
			else
			{
				pDfile = fopen("../../config/camera_distortion_B.config", "wb");
				pDfile2 = fopen("../../config/camera_distortion_B.txt", "w");
			}

			fprintf(pDfile2, "%f\n", m_rect_3D.topLeft.x );
			for (int y=0; y<5; y++)
			{
				for (int x=0; x<9; x++)
				{
					fprintf(pDfile2, "%f, %f \n", distortion_grid[y][x].video.x, distortion_grid[y][x].video.y);
				}
			}
			fprintf(pDfile2, "\n\n");


			fwrite( &m_rect_3D.topLeft.x, 1, sizeof(float), pDfile );	// write the X offset in here
			for (int y=0; y<4; y++)
			{
				for (int x=0; x<8; x++)
				{
					distortion_blocks[m_ActiveCamera][y][x].corner[0] = distortion_grid[y][x];
					distortion_blocks[m_ActiveCamera][y][x].corner[1] = distortion_grid[y+1][x];
					distortion_blocks[m_ActiveCamera][y][x].corner[2] = distortion_grid[y+1][x+1];
					distortion_blocks[m_ActiveCamera][y][x].corner[3] = distortion_grid[y][x+1];

					distortion_blocks[m_ActiveCamera][y][x].edge[0] = distortion_blocks[m_ActiveCamera][y][x].corner[1].video - distortion_blocks[m_ActiveCamera][y][x].corner[0].video;
					distortion_blocks[m_ActiveCamera][y][x].edge[1] = distortion_blocks[m_ActiveCamera][y][x].corner[2].video - distortion_blocks[m_ActiveCamera][y][x].corner[1].video;
					distortion_blocks[m_ActiveCamera][y][x].edge[2] = distortion_blocks[m_ActiveCamera][y][x].corner[3].video - distortion_blocks[m_ActiveCamera][y][x].corner[2].video;
					distortion_blocks[m_ActiveCamera][y][x].edge[3] = distortion_blocks[m_ActiveCamera][y][x].corner[0].video - distortion_blocks[m_ActiveCamera][y][x].corner[3].video;

					fwrite( distortion_blocks[m_ActiveCamera][y][x].corner, 2*4, sizeof(f_Vector), pDfile );
					fwrite( distortion_blocks[m_ActiveCamera][y][x].edge, 4, sizeof(f_Vector), pDfile );

					fprintf(pDfile2, "%f, %f \n", distortion_blocks[m_ActiveCamera][y][x].corner->screen.x, distortion_blocks[m_ActiveCamera][y][x].corner->screen.y);
				}
			}
			fclose(pDfile);
			fclose(pDfile2);

			WAIT = 5;
			m_mode = mode_CAMERA_TEST;
			m_pCam_Intensity->show();
			m_pCam_Distortion->hide();
			m_pCam_ZoomPanFocus->hide();
			 
			m_pLogger->Log( LOG_INFO, L"mode_CAMERA_DISTORTION now save jpg  " );

			copyImage( true, L"../../config/calibration.jpg" );
			m_pScene->setDots( m_pCam_Thresh->getText().toFloat(), m_pCam_Min->getText().toFloat(), m_pCam_Max->getText().toFloat(), m_pScene->m_PG_dot_position );		// reset this all
		}

		
	}



	static int BORESIGHT_DELAY = 3;
	BORESIGHT_DELAY --;
	if( m_mode == mode_CAMERA_TEST && okDOTS > 0 )
	{
		for (int i=0; i<20; i++)
		{
			m_pCam_Skoot[i]->hide();;
			m_pCam_SkootInfo[i]->hide();
		}
		m_pCam_Skoot[0]->show();;
		m_pCam_SkootInfo[0]->show();
		
		if (BORESIGHT_DELAY <= 0)
		{
			
			m_pScene->bAccumulateDots = true;
			f_Vector screen = video_to_Screen( m_pScene->dots[m_ActiveCamera][0], m_ActiveCamera );
			//SHOTCNT = 40;
			m_pCam_Skoot[0]->setPosition( screen.x - 32, screen.y - 32);
			m_pCam_Skoot[0]->setSize( 64, 64 );

			m_pCam_SkootInfo[0]->setPosition( screen.x + 30, screen.y - 10);
			m_pCam_SkootInfo[0]->setSize( 150, 30 );
			m_pCam_SkootInfo[0]->setText( DmString( (int)m_pScene->dots[m_ActiveCamera][0].z ) + L" pix, (" + DmString( (int)m_pScene->dots[m_ActiveCamera][0].w ) + L") of " + DmString( m_pScene->numDots[m_ActiveCamera] ) );
			m_pCam_SkootInfo[0]->setHorizontalTextAlignment( dmAlignLeft );


			zigbeeFire( this->m_activeAmmoLane );

			BORESIGHT_DELAY = 0;
			//DmString logname = L"../../camera_logs/" + DmString( m_pScene->m_PG_frameCNT ) + L".jpg";
			//copyImage( true, (LPWSTR)logname.c_str(), D3DXIFF_JPG );
			m_pScene->numDots[m_ActiveCamera] = 0;
		}
	}


	if( m_mode == mode_CAMERA_IR1 && okDOTS > 0 )
	{
		for (int i=0; i<20; i++)
		{
			m_pCam_Skoot[i]->hide();;
			m_pCam_SkootInfo[i]->hide();
		}
		m_pCam_Skoot[0]->show();;
		m_pCam_SkootInfo[0]->show();
				
		m_pScene->bAccumulateDots = true;
		f_Vector screen = video_to_Screen( m_pScene->dots[m_ActiveCamera][0], m_ActiveCamera );
		m_pCam_Skoot[0]->setPosition( screen.x - 32, screen.y - 32);
		m_pCam_Skoot[0]->setSize( 64, 64 );
		m_pCam_SkootInfo[0]->setPosition( screen.x + 30, screen.y - 10);
		m_pCam_SkootInfo[0]->setSize( 150, 30 );
		m_pCam_SkootInfo[0]->setText( DmString( (int)m_pScene->dots[m_ActiveCamera][0].z ) + L" pix, (" + DmString( (int)m_pScene->dots[m_ActiveCamera][0].w ) + L") of " + DmString( m_pScene->numDots[m_ActiveCamera] ) );
		m_pCam_SkootInfo[0]->setHorizontalTextAlignment( dmAlignLeft );
		m_pScene->numDots[m_ActiveCamera] = 0;

		// FIXME thsi is duplicate vir screen to video put in own fucntion or have screen to video retun this somehow
		float W = this->m_rect_3D.width();
		float H = this->m_rect_3D.height();
		int X = floor((screen.x - m_rect_3D.topLeft.x) / (W/2));
		int Y = floor(screen.y / (H/2));
		float dX = ((screen.x - m_rect_3D.topLeft.x) - (X*W/2)) / (W/2);
		float dY = (screen.y - (Y*H/2)) / (H/2);

		float W1 = (1.0 - dY)	* (1.0 - dX);
		float W2 = (1.0 - dY)	* (dX);
		float W3 = (dY)			* (1.0 - dX);
		float W4 = (dY)			* (dX);

		m_IR_X = X;
		m_IR_Y = Y;
		m_IR_Weights = f_Vector(W1, W2, W3, W4);

		for (int y=0; y<3; y++)
		{
			for (int x=0; x<3; x++)
			{
				m_pCam_IR_grid[y][x]->setText( DmString( IR_dist[m_ActiveCamera][y][x].x, 1 ) + L", " + DmString( IR_dist[m_ActiveCamera][y][x].y, 1 ) );
			}
		}
		
		m_pCam_IR_info->setText( DmString(X) + L", " + DmString(Y) + L"   (" + DmString(W1) + L", " + DmString(W2) + L", " + DmString(W3) + L", " + DmString(W4) + L", " + L" )" );
	}

	if( m_mode == mode_CAMERA_IR2  )
	{
		for (int i=0; i<20; i++)
		{
			m_pCam_Skoot[i]->hide();;
			m_pCam_SkootInfo[i]->hide();
		}

		for (int i=0; i<__min(20, okDOTS); i++)
		{
			m_pScene->bAccumulateDots = true;
			f_Vector screen = video_to_Screen( m_pScene->dots[m_ActiveCamera][i], m_ActiveCamera );
			m_pCam_Skoot[i]->setPosition( screen.x - 32, screen.y - 32);
			m_pCam_Skoot[i]->setSize( 64, 64 );
			m_pCam_SkootInfo[i]->setPosition( screen.x + 30, screen.y - 10);
			m_pCam_SkootInfo[i]->setSize( 150, 30 );
			m_pCam_SkootInfo[i]->setText( DmString( (int)m_pScene->dots[m_ActiveCamera][0].z ) + L" pix, (" + DmString( (int)m_pScene->dots[m_ActiveCamera][0].w ) + L") of " + DmString( m_pScene->numDots[m_ActiveCamera] ) );
			m_pCam_SkootInfo[i]->setHorizontalTextAlignment( dmAlignLeft );
			m_pCam_Skoot[i]->show();;
			m_pCam_SkootInfo[i]->show();
		}
		m_pScene->numDots[m_ActiveCamera] = 0;


		for (int y=0; y<3; y++)
		{
			for (int x=0; x<3; x++)
			{
				m_pCam_IR_grid[y][x]->setText( DmString( IR_dist[m_ActiveCamera][y][x].x, 1 ) + L", " + DmString( IR_dist[m_ActiveCamera][y][x].y, 1 ) );
				m_pCam_IR_grid[y][x]->setStyleName( L"Edit_30" );
			}
		}
		m_pCam_IR_grid[m_IR_adjust_currentY][m_IR_adjust_currentX]->setStyleName( L"metro_yellow" );
		
	}
	





	if ( m_mode == mode_3D ||  m_mode == mode_INTERACTIVE_VIDEO )
	{
		m_pScene->bAccumulateDots = true;
		SHOTCNT = 50;	// blok hierdie permanane tom nog die rpetn ook op te dateer

		// Do both cameras
		for (int cam = 0; cam < 2; cam ++)
		{
			for (int i=0; i<m_pScene->numDots[cam]; i++)
			{
				f_Vector screen = video_to_Screen( m_pScene->dots[cam][i], cam );	
				if ( !(screen.x == 0 && screen.y==0) ) 
				{
					if  (m_bLiveFire)	m_numShotsSinceLastPaperAdvance ++;
					fire( screen.x, screen.y );
				}
			}
		}
	}
/*
	if (bBoresight)		// we are in teh boresight Screen ------------------------------------------------------------------------------
	{
		zigbeeRounds( m_activeAmmoLane, 100 );
		m_pScene->bAccumulateDots = true;
		SHOTCNT = 50;	// blok hierdie permanane tom nog die rpetn ook op te dateer
		static int BORESIGHT_DELAY = 6;
		BORESIGHT_DELAY --;
		if (BORESIGHT_DELAY <= 0)
		{
			for (int i=0; i<okDOTS; i++)		// Only uses the first boresight
			{
				f_Vector screen = video_to_Screen( m_pScene->dots[i] );	
				if ( !(screen.x == 0 && screen.y==0) ) 
				{
					fireBoresight( screen.x, screen.y );
					BORESIGHT_DELAY = 16;
					return;
				}
			}
		}
	}
	*/

	
	if (m_pScene->bAccumulateDots)
	{
		m_pScene->numDots[0] = 0;
		m_pScene->numDots[1] = 0;
	}


	
	SHOTCNT --;
	if (SHOTCNT <= 0 )	// moet beeld opdateer
	{ 
		copyImage( true );
		m_pCam_ZoomPanFocus->setStyleName( L"metro_black" );
		m_pCam_Intensity->setStyleName( L"metro_black" );
	}
}


void labViewRectify()
{
	
}


f_Vector ctrl_DirectWeapons::video_to_Screen(f_Vector pixel, int cam)
{

	f_Vector screen = f_Vector(0, 0, 0, 0 );

	pixel.z = 0;
	pixel.w = 0;	// supress silly data
	for (int y=0; y<4; y++ )
	{
		for (int x=0; x<8; x++)
		{
			f_Vector vA = pixel - distortion_blocks[cam][y][x].corner[0].video;
			f_Vector vB = pixel - distortion_blocks[cam][y][x].corner[1].video;
			f_Vector vC = pixel - distortion_blocks[cam][y][x].corner[2].video;
			f_Vector vD = pixel - distortion_blocks[cam][y][x].corner[3].video;
			float wA = (vA ^ distortion_blocks[cam][y][x].edge[0]).z;
			float wB = (vB ^ distortion_blocks[cam][y][x].edge[1]).z;
			float wC = (vC ^ distortion_blocks[cam][y][x].edge[2]).z;
			float wD = (vD ^ distortion_blocks[cam][y][x].edge[3]).z;

			if (wA>=0 && wB>=0 && wC>=0 && wD>=0)		// ons het die regte blok
			{
				float wX = wA/(wA + wC);
				float wY = wD/(wB + wD);
				screen =	distortion_blocks[cam][y][x].corner[0].screen * (1.0f - wX) * (1.0f - wY)  +
							distortion_blocks[cam][y][x].corner[1].screen * (1.0f - wX) * (wY       )  +
							distortion_blocks[cam][y][x].corner[2].screen * (wX       ) * (wY       )  +
							distortion_blocks[cam][y][x].corner[3].screen * (wX       ) * (1.0f - wY) ;


			}
		}
	}

	//if (this->m_bLiveFire)
	{
		float W = this->m_rect_3D.width();
		float H = this->m_rect_3D.height();
		int X = floor((screen.x - m_rect_3D.topLeft.x) / (W/2));
		int Y = floor(screen.y / (H/2));
		float dX = ((screen.x - m_rect_3D.topLeft.x) - (X*W/2)) / (W/2);
		float dY = (screen.y - (Y*H/2)) / (H/2);

		f_Vector IR =	IR_dist[cam][Y][X]		* (1.0 - dY)	* (1.0 - dX) +
						IR_dist[cam][Y+1][X]		* (dY)			* (1.0 - dX) +
						IR_dist[cam][Y][X+1]		* (1.0 - dY)	* (dX) +
						IR_dist[cam][Y+1][X+1]	* (dY)			* (dX);
		screen = screen + IR;

		//m_pLogger->Log( LOG_INFO, L"video_to_Screen IR  pixel(%d, %d)  YX(%d, %d), dXdY(%d, %d) shift(%d, %d)", (int)screen.x, (int)screen.y, X, Y, (int)(dX*100), (int)(dY*100), (int)IR.x, (int)IR.y );

	}

	// Hier moet ons error log - ons kon nie die blok kry nie
	return screen;
}


void ctrl_DirectWeapons::loadIRgrid()
{
	//m_pLogger->Log( LOG_INFO, L"loadIRgrid");
	FILE *pDfile = fopen("../../config/IR_distortion.txt", "r");
	if (pDfile)
	{
		for (int y=0; y<3; y++)
		{
			for (int x=0; x<3; x++)
			{
				fscanf( pDfile, "%f %f", &IR_dist[0][y][x].x, &IR_dist[0][y][x].y );
				//m_pLogger->Log( LOG_ERROR, L"		X %d, Y %d   value(%d, %d)", x, y, (int)IR_dist[y][x].x, (int)IR_dist[y][x].y  );
			}
		}
		fclose(pDfile);
	}
	else
	{
		m_pLogger->Log( LOG_ERROR, L"failed to open   ../../config/IR_distortion.txt" );
	}



	pDfile = fopen("../../config/IR_distortion_B.txt", "r");
	if (pDfile)
	{
		for (int y=0; y<3; y++)
		{
			for (int x=0; x<3; x++)
			{
				fscanf( pDfile, "%f %f", &IR_dist[1][y][x].x, &IR_dist[1][y][x].y );
				//m_pLogger->Log( LOG_ERROR, L"		X %d, Y %d   value(%d, %d)", x, y, (int)IR_dist[y][x].x, (int)IR_dist[y][x].y  );
			}
		}
		fclose(pDfile);
	}
	else
	{
		m_pLogger->Log( LOG_ERROR, L"failed to open   ../../config/IR_distortion_B.txt" );
	}
	
}

void ctrl_DirectWeapons::saveIRgrid()
{
	FILE *pDfile;
	if (m_ActiveCamera == 0)
	{
		pDfile = fopen("../../config/IR_distortion.txt", "w");
	}
	else
	{
		pDfile = fopen("../../config/IR_distortion_B.txt", "w");
	}
	if (pDfile)
	{
		for (int y=0; y<3; y++)
		{
			for (int x=0; x<3; x++)
			{
				fprintf( pDfile, "%f %f\n", IR_dist[m_ActiveCamera][y][x].x, IR_dist[m_ActiveCamera][y][x].y );
			}
		}
		fclose(pDfile);
	}
	else
	{
		m_pLogger->Log( LOG_ERROR, L"failed to open for saving  ../../config/IR_distortion.txt" );
	}
	
}

void   ctrl_DirectWeapons::zigbeePaperMove( )
{
	if ( !ZIGBEE.IsOpen() ) return;
	unsigned char channels =  1 << 4;		// set this to true

	unsigned char STR[13];			// veelvoude van 10
	unsigned long bytes = ZIGBEE.Read( STR, 13 );	// read   ??? Hoekom moet on sbuffer skoonmaak  FIXME
	Sleep(30);

	unsigned char ANS[13];
	switch ( m_ZigbeePacketVersion )
	{
	case 0:
		ANS[0] = 0xff;
		ANS[1] = 0xff;
		ANS[2] = 0x00;	// device ID
		ANS[3] = 0x00;
		ANS[4] = 0x00;	//own id
		ANS[5] = 0x01;
		ANS[6] = 0x0d;	//lenght - all bytes
		ANS[7] = 0x16;	//command
		ANS[8] = 0x00;	// body
		ANS[9] = 0x00;
		ANS[10] = channels;
		ANS[11] = 0x00; // status
		ANS[12] = 0xDE - channels;
		ZIGBEE.Write( ANS, 13 );
		break;
	case 1:
		ANS[0] = 0xff;
		ANS[1] = 0xff;
		ANS[2] = 0x0b;	// device ID
		ANS[3] = 0x00;
		ANS[4] = zigbeeID_h;	//own id
		ANS[5] = zigbeeID_l;
		ANS[6] = 0x16;	//lenght - all bytes
		ANS[7] = 0x00;	//command
		ANS[8] = channels;
		ANS[9] = 0x00; // status
		int checksum = 0x100;
		for ( int i=0; i<10; i++ )  checksum -= ANS[i];
		ANS[10] = checksum & 0xff;  //0xB6 - (1 << lane);
		ZIGBEE.Write( ANS, 11 );
		break;
	}
}

void   ctrl_DirectWeapons::zigbeePaperStop( )
{
	if ( !ZIGBEE.IsOpen() ) return;
	unsigned char channels =  0;	

	unsigned char STR[13];			// veelvoude van 10
	unsigned long bytes = ZIGBEE.Read( STR, 13 );	// read   ??? Hoekom moet on sbuffer skoonmaak  FIXME
	Sleep(30);

	unsigned char ANS[13];
	switch ( m_ZigbeePacketVersion )
	{
	case 0:
		ANS[0] = 0xff;
		ANS[1] = 0xff;
		ANS[2] = 0x00;	// device ID
		ANS[3] = 0x00;
		ANS[4] = 0x00;	//own id
		ANS[5] = 0x01;
		ANS[6] = 0x0d;	//lenght - all bytes
		ANS[7] = 0x16;	//command
		ANS[8] = 0x00;	// body
		ANS[9] = 0x00;
		ANS[10] = channels;
		ANS[11] = 0x00; // status
		ANS[12] = 0xDE - channels;
		ZIGBEE.Write( ANS, 13 );
		break;
	case 1: 
		ANS[0] = 0xff;
		ANS[1] = 0xff;
		ANS[2] = 0x0b;	// device ID
		ANS[3] = 0x00;
		ANS[4] = zigbeeID_h;	//own id
		ANS[5] = zigbeeID_l;
		ANS[6] = 0x16;	//lenght - all bytes
		ANS[7] = 0x00;	//command
		ANS[8] = channels;
		ANS[9] = 0x00; // status
		int checksum = 0x100;
		for ( int i=0; i<10; i++ )  checksum -= ANS[i];
		ANS[10] = checksum & 0xff;  //0xB6 - (1 << lane);
		ZIGBEE.Write( ANS, 11 );
		break;
	}
}


void  ctrl_DirectWeapons::zigbeeRounds( unsigned int lane, int R, bool bStop )
{
	if ( !ZIGBEE.IsOpen() ) return;

	// test zigbee bool and return
	if (m_bLiveFire) return;

	// test ammo for air cycled and return
	if( (!bStop) && m_lanes[lane].ammo.type == ammo_AIRCYCLED ) return;

	static unsigned char channels = 0;
	unsigned char newchannels = channels;
	if (R > 0)	newchannels |= 1 << lane;		// set this to true
	else		newchannels &= ~(1 << lane );	// clear this bit

	if(bStop) newchannels = 0;		// on stop command clear it all

	if (bStop || (channels != newchannels) )
	{
		channels = newchannels;		

		unsigned char STR[13];			// veelvoude van 10
		unsigned long bytes = ZIGBEE.Read( STR, 13 );	// read   ??? Hoekom moet on sbuffer skoonmaak  FIXME
		Sleep(30);


		unsigned char ANS[13];

		switch ( m_ZigbeePacketVersion )
		{
		case 0:	
			ANS[0] = 0xff;
			ANS[1] = 0xff;
			ANS[2] = 0x00;	// device ID
			ANS[3] = 0x00;
			ANS[4] = 0x00;	//own id
			ANS[5] = 0x01;
			ANS[6] = 0x0d;	//lenght - all bytes
			ANS[7] = 0x16;	//command
			ANS[8] = 0x00;	// body
			ANS[9] = 0x00;
			ANS[10] = channels;
			ANS[11] = 0x00; // status
			ANS[12] = 0xDE - channels;

			ZIGBEE.Write( ANS, 13 );
			break;

		case 1:
			ANS[0] = 0xff;
			ANS[1] = 0xff;
			ANS[2] = 0x0b;	// device ID
			ANS[3] = 0x00;
			ANS[4] = zigbeeID_h;	//own id
			ANS[5] = zigbeeID_l;
			ANS[6] = 0x16;	//lenght - all bytes
			ANS[7] = 0x00;	//command
			ANS[8] = channels;
			ANS[9] = 0x00; // status

			int checksum = 0x100;
			for ( int i=0; i<10; i++ )  checksum -= ANS[i];

			ANS[10] = checksum & 0xff;  //0xB6 - (1 << lane);
			ZIGBEE.Write( ANS, 11 );

			//ANS[10] = 0xE3 - channels;

			ZIGBEE.Write( ANS, 11 );
			break;
		}


		//m_pLogger->Log( LOG_INFO, L"zigbee (%d, %d, %d) - %.2X%.2X %.2X%.2X %.2X%.2X %.2X%.2X %.2X%.2X %.2X%.2X %.2X", lane, R, channels, ANS[0], ANS[1], ANS[2], ANS[3], ANS[4], ANS[5], ANS[6], ANS[7], ANS[8], ANS[9], ANS[10], ANS[11], ANS[12] );
	}
}

void  ctrl_DirectWeapons::zigbeeFire( unsigned int lane )
{
	if ( !ZIGBEE.IsOpen() ) return;

	// test zigbee bool and return
	if (m_bLiveFire) return;

	// test ammo for air cycled and return
	if( m_lanes[lane].ammo.type == ammo_STD ) return;

	// test R for change and return - write to zigbee

	unsigned char STR[50];			// veelvoude van 10
	memset( STR, 0, 50 );			// clear
	unsigned long bytes = ZIGBEE.Read( STR, 13 );	// read   ??? Hoekom moet on sbuffer skoonmaak  FIXME

	unsigned char ANS[13];

	switch ( m_ZigbeePacketVersion )
	{
	case 0:	
		ANS[0] = 0xff;
		ANS[1] = 0xff;
		ANS[2] = 0x00;	// device ID
		ANS[3] = 0x00;
		ANS[4] = 0x00;	//own id
		ANS[5] = 0x01;
		ANS[6] = 0x0d;	//lenght - all bytes
		ANS[7] = 0x18;	//command
		ANS[8] = 0x00;	// body
		ANS[9] = 0x00;
		ANS[10] = 1 << lane;
		ANS[11] = 0x00; // status
		ANS[12] = 0xDC - (1 << lane);
		ZIGBEE.Write( ANS, 13 );
		break;

	case 1:
		ANS[0] = 0xff;
		ANS[1] = 0xff;
		ANS[2] = 0x0b;	// lengte van oakkie
		ANS[3] = 0x00;
		ANS[4] = zigbeeID_h;//0x69;	//ID from config file  FIXME
		ANS[5] = zigbeeID_l;//0xc0;
		ANS[6] = 0x18;	//cammand 
		ANS[7] = 0x00;	//command
		ANS[8] = 1 << lane;
		ANS[9] = 0x00; // status

		int checksum = 0x100;
		for ( int i=0; i<10; i++ )  checksum -= ANS[i];

		ANS[10] = checksum & 0xff;  //0xB6 - (1 << lane);
		ZIGBEE.Write( ANS, 11 );

		//m_pLogger->Log( LOG_INFO, L"zigbee R4  - %.2X%.2X %.2X%.2X %.2X%.2X %.2X%.2X %.2X%.2X  chk  %.2X", ANS[0], ANS[1], ANS[2], ANS[3], ANS[4], ANS[5], ANS[6], ANS[7], ANS[8], ANS[9], ANS[10] );

		break;
	}

	//m_pLogger->Log( LOG_INFO, L"zigbee R4 - %.2X%.2X %.2X%.2X %.2X%.2X %.2X%.2X %.2X%.2X %.2X%.2X %.2X", ANS[0], ANS[1], ANS[2], ANS[3], ANS[4], ANS[5], ANS[6], ANS[7], ANS[8], ANS[9], ANS[10], ANS[11], ANS[12] );
}









// Voice recognition ------------------------------------------------------------------------------------------------------
void ctrl_DirectWeapons::voiceRecognitionCallback(LPCWSTR phrase, signed char confidence)
{
	if (confidence > 0)	// good confidence - sday it
	{
		//MessageBox(NULL, L"VOICE", phrase, MB_OK | MB_ICONERROR);
		//m_Scenario->
		m_lastVoiceCommand = phrase;

	}
}


void ctrl_DirectWeapons::voiceNoRecCallback()
{
}


void ctrl_DirectWeapons::voiceStartStopCallback(bool bSS)
{
}


void ctrl_DirectWeapons::voiceVolumeCallback(int level)
{
	/*
	static int max = 0;
	static int cnt = 0;
	max = __max( max, level );
	cnt ++;

	if (cnt == 3)
	{
		cnt = 0;
		if (level < 5 )			m_Tools.changeStyle( L"tool_mic", L"tool_mic" );
		else if (level < 25 )	m_Tools.changeStyle( L"tool_mic", L"tool_mic_V1" );
		else if (level < 50 )	m_Tools.changeStyle( L"tool_mic", L"tool_mic_V2" );
		else					m_Tools.changeStyle( L"tool_mic", L"tool_mic_V3" );
	}
	*/
}




unsigned short ctrl_DirectWeapons::GUID_lookup(LPCWSTR callsign)
{
	for (int i=0; i<1024; i++)
	{
		 if( m_pScene->m_pMOI[ i ].callsign == callsign )	
		 {
			 return i;
		 }
	}
	return 0;
}

unsigned short ctrl_DirectWeapons::GUID_lookupname(LPCWSTR name)
{
	for (int i=0; i<1024; i++)
	{
		 if( m_pScene->m_pMOI[ i ].name == name )	
		 {
			 return i;
		 }
	}
	return 0;
}

void ctrl_DirectWeapons::adjustWeaponOffset( int lane, float x, float y , float distance )
{
	if (m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_ADJUST );
		Bs.Write( (unsigned short) (lane) );
		Bs.Write( x ); 
		Bs.Write( y ); 
		Bs.Write( distance ); 
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}

	float fov = this->m_pScene->getCamera()->getFOV() / 57.295779513082320876798154814105f;
	float screenW = tan(fov/2) * 2 * distance;
	float pixW = screenW / m_rect_3D.width();

	
	 float laser_offset = tan( m_lanes[lane].ammo.m_barrelRotation )*distance - m_lanes[lane].ammo.m_barrelDrop + (m_lanes[lane].ammo.laser_offset_mm/1000.0f);
	y -= laser_offset;
	

	x/=pixW;
	y/=pixW;

	m_lanes[lane].weaponOffset.x += x;
	m_lanes[lane].weaponOffset.y += y;

	for (unsigned int i=0; i< m_lanes.size(); i++)			// save it all in case of crash ------------------------------------
	{
		ewstring lane_str = "lane_" + ewstring( i );
		if (!m_pCfgMng->hasGroup(lane_str))				m_pCfgMng->addGroup(lane_str);
		(*m_pCfgMng)[lane_str].setConfig( "active_ammo", m_lanes[i].ammo.ammoIndex );
		(*m_pCfgMng)[lane_str].setConfig( "active_ammo_X", m_lanes[i].weaponOffset.x );
		(*m_pCfgMng)[lane_str].setConfig( "active_ammo_Y", m_lanes[i].weaponOffset.y );
	}
}

void	ctrl_DirectWeapons::set_MOI_to_Camera(unsigned short GUID)
{
	m_activeGUID = GUID;
	if (GUID > 0)	m_Camera.setMOI( m_pScene->m_pMOI[ GUID ].pMOI, "Bip01 Head" );
	else			m_Camera.setMOI( NULL, "" );
}



void ctrl_DirectWeapons::playlistLoad()
{
	WCHAR path[MAX_PATH];
	WCHAR name[MAX_PATH];
	ewstring prntDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "\\playlists\\");
	if (!DialogUtil::OpenDialog(path, MAX_PATH, name, L"Playlists (*.playlist)\0*.playlist\0\0", L"ext", prntDIR.getWString().c_str()))
	{
		//MessageBox(NULL, L"No valid license found - please contact Dimension Software Engineering, www.kolskoot.net", L"Error", MB_OK);
	}
	else
	{
		m_PlaylistUsers.clear();
		m_PlaylistNewUsers.clear();
		m_PlaylistCurrentRow = 0;

		FILE *file = _wfopen( path, L"r" );
		if (file)
		{
			int ret = 0;
			int user;
			char p[256];
			ret = fscanf( file, "%s\n", p );
			m_bPlaylistAutoPrint = false;
			if (strstr( p, "AUTOPRINT" ) )	m_bPlaylistAutoPrint = true;

			ret = fscanf( file, "%d\n", &user );
			while ( ret != EOF )
			{
				m_PlaylistUsers.push_back( user );
				ret = fscanf( file, "%d\n", &user );
			}
			fclose(file);
		}
	}

	m_bPlaylistActive = true;

	playlistUpdateWidget();
}

void ctrl_DirectWeapons::playlistSave()
{
	WCHAR path[MAX_PATH];
	WCHAR name[MAX_PATH];
	ewstring prntDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "\\playlists\\");
	if (!DialogUtil::SaveDialog(path, MAX_PATH, L"Playlists (*.playlist)\0*.playlist\0\0", L"ext", prntDIR.getWString().c_str()))
	{
	}
	else
	{
		FILE *file = _wfopen( path, L"w" );
		if (file)
		{
			if (m_bPlaylistAutoPrint)	fprintf(file, "AUTOPRINT\n");
			else						fprintf(file, "NOPRINT\n");
	
			for (int i=0; i<m_PlaylistUsers.size(); i++)
			{
				fprintf(file, "%d\n", m_PlaylistUsers[i]);
			}
			fclose(file);
		}
	}

	m_bPlaylistActive = true;
}

void ctrl_DirectWeapons::playlistClear()
{
	m_bPlaylistAutoPrint = false;
	m_bPlaylistActive = false;
	m_PlaylistUsers.clear();
	m_PlaylistNewUsers.clear();
	m_SearchText = L"";
	calculateSearch( m_SearchText );
	m_pPlaylistSearch->setText( L"" );
	m_PlaylistCurrentRow = 0;
	playlistUpdateWidget();
}

void ctrl_DirectWeapons::playlistAdd()
{
	for (int i=0; i<m_PlaylistNewUsers.size(); i++)
	{
		m_PlaylistUsers.push_back( m_PlaylistNewUsers[i] );
	}
	m_PlaylistNewUsers.clear();
	m_SearchText = L"";
	calculateSearch( m_SearchText );
	m_pPlaylistSearch->setText( L"" );
	m_PlaylistCurrentRow = 0;
	playlistUpdateWidget();
	m_bPlaylistActive = true;
}

void ctrl_DirectWeapons::playlistGoMenu(  bool bAdvance )
{
	if (!m_bPlaylistActive) return;

	// remove names
/*	m_bPlaylistActive = false; // in case no names laetf
	for (int i=0; i<m_PLaylistNumLanes; i++)
	{
		int CNT = m_PlaylistCurrentRow *  m_PLaylistNumLanes + i;
		m_lanes[i].userID = L"";	
		m_lanes[i].userNAME = L"";
		populateAmmo( i, false );
	}*/

	// add new ones
	for (int i=0; i<m_PLaylistNumLanes; i++)
	{
		int CNT = m_PlaylistCurrentRow *  m_PLaylistNumLanes + i;
		if (CNT < m_PlaylistUsers.size())
		{
			for (std::list<_user>::iterator it = m_users.begin(); it != m_users.end(); it++)
			{
				if (it->id.toInt() == m_PlaylistUsers[CNT])
				{
					m_lanes[i].userID = it->id;	
					m_lanes[i].userNAME = it->displayName;
					populateAmmo( i, false );

					m_bPlaylistActive = true;		// we still have names left
					// set autoprint
					m_bAutoPrint = m_bPlaylistAutoPrint;
				}
			}
		}
		else
		{
			m_lanes[i].userID = L"";	
			m_lanes[i].userNAME = L"";
			populateAmmo( i, false );
		}
	}

	if (bAdvance) m_PlaylistCurrentRow ++;
	
}

void ctrl_DirectWeapons::playlistTogglePrint()
{
	m_bPlaylistAutoPrint = !m_bPlaylistAutoPrint;
	playlistUpdateWidget();
}

void ctrl_DirectWeapons::playlistClickRow( int R )
{
	m_PlaylistCurrentRow = R;
	m_bPlaylistActive = true;
	playlistUpdateWidget();
}

void ctrl_DirectWeapons::playlistClickX()
{
}

void ctrl_DirectWeapons::playlistUpdateWidget()
{
	if (m_bPlaylistAutoPrint)
	{
		m_pPlaylist->findWidget( L"print" )->setValue( L"icon_autoprint" );	//->setText( L"Print" );
		//m_pPlaylist->findWidget( L"print" )->setText( L"Print results" );
	}
	else
	{
		m_pPlaylist->findWidget( L"print" )->setValue( L"icon_autoprintOFF" );	//->setText( L"X" );
		//m_pPlaylist->findWidget( L"print" )->setText( L"Do not print" );
	}

	int CNT = 0;
	for (int l=0; l<10; l++)
	{
		((DmPanel*)m_pPlaylist->findWidget( L"currentLane"+DmString(l) ))->setStyleName( "metro_border" );
		for (int k=0; k<m_PLaylistNumLanes; k++)
		{
			if (CNT < m_PlaylistUsers.size())
			{
				for (std::list<_user>::iterator it = m_users.begin(); it != m_users.end(); it++)
				{
					//m_pLogger->Log( LOG_INFO, L"playlist (%d), userID(%s)", m_PlaylistUsers[CNT], it->id.c_str() );
					if (it->id.toInt() == m_PlaylistUsers[CNT])
					{
						m_pPlaylist->findWidget( L"user"+DmString(l)+L"_"+DmString(k) )->setText( it->displayName );
						m_pPlaylist->findWidget( L"X"+DmString(l)+L"_"+DmString(k) )->show();
					}
				}
			}
			else if (CNT < m_PlaylistUsers.size()+m_PlaylistNewUsers.size())
			{
				int CNT2 = CNT - m_PlaylistUsers.size();
				for (std::list<_user>::iterator it = m_users.begin(); it != m_users.end(); it++)
				{
					if (it->id.toInt() == m_PlaylistNewUsers[CNT2])
					{
						m_pPlaylist->findWidget( L"user"+DmString(l)+L"_"+DmString(k) )->setText( L"( " + it->displayName + L" )" );
						m_pPlaylist->findWidget( L"X"+DmString(l)+L"_"+DmString(k) )->hide();
					}
				}
			}
			else
			{
				m_pPlaylist->findWidget( L"user"+DmString(l)+L"_"+DmString(k) )->setText( L"." );
				m_pPlaylist->findWidget( L"X"+DmString(l)+L"_"+DmString(k) )->hide();
			}
			CNT ++;
		}
	}

	((DmPanel*)m_pPlaylist->findWidget( L"currentLane"+DmString(m_PlaylistCurrentRow) ))->setStyleName( "block_green" );

}


