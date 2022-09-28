// skietbaan Labview interface
// just a nice c++ wrapper with some error logging and some nice clean structures
// for me to use. Isolates Labview Changes from the main code.

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// This is all from Labview
// ------------------------------------------------------------------------------------------------
#pragma pack(push)
#pragma pack(1)

	#ifdef __cplusplus
	extern "C" {
	#endif


		typedef struct {
			float x;
			float y;
			} TD3;


	

		typedef void (* LPFNDLLFUNC1)( int Rounds, bool *bFan, bool *bStop, bool *bHandgun, bool *brecAssoc, bool *bsetChannel, bool *bDoor, bool *bDimmer, int RecoilNos, int *Shotcount, TD3 BulletCoords[], int len, unsigned char Image[], int len2 );
		//typedef void (* LPFNDLLFUNC1)( int Rounds, bool *bFan, bool *bStop, bool *bHandgun, bool *brecAssoc, bool *bsetChannel, bool *bDoor, bool *bDimmer, int RecoilNos, int *Shotcount, TD3 BulletCoords[], int len );

		long __cdecl LVDLLStatus(char *errStr, int errStrLen, void *module);

	#ifdef __cplusplus
	} // extern "C"
	#endif

#pragma pack(pop)



typedef struct
{
	int RECOIL;
	int Rounds;
	int shotcount;
	unsigned int readcount;
	TD3		shots[32];		// max 32 backard shots
	unsigned char pImage[752*480*5];	// die *5 is net om dit nnou groetr te maak teots dit

	bool bError_TooLittleData;
	bool bError_TooMuchData;
	int	debug_SHOT;
	int debug_ARRAY;
} _Lasershot;



class ew_Labview_lasershot
{
public :
	static ew_Labview_lasershot *GetSingleton();
	static void FreeSingleton();

	void openDLL();
	void closeDLL();
	void restartDLL();

	bool isRunning() {return m_bInit;}

	_Lasershot *getData() {return &m_Data;}
	_Lasershot *read();
	void setValves( int i  ){m_Data.RECOIL = i;}


	void setFan(bool b ) {m_bFan = b;}
	void toggleFan() {m_bFan = !m_bFan;}
	void setHandgun(bool b ) {m_bHandgun = b;}
	void setRequestChannel(bool b ) {m_bsetChannel = b;}
	void setRounds( int r ) {m_Data.Rounds = r; m_ShotLoadCounter = 100;}
	void setLiveFire( bool b ) {m_bDoor = b;}	// misuse door for laser / live   TRUE live, FALSE laser
	void setGrabVideo( bool b ) {m_bDimmer = b;}	// misuse dimemr for Gran Video

protected:
	bool			m_bInit;
	HINSTANCE		m_hDLL;				// Handle to DLL
	LPFNDLLFUNC1	m_lpfnDllFunc1;		// Function pointer
	_Lasershot		m_Data;

	bool			m_bFan;

	bool			m_bHandgun;
	bool			m_bRecAssoc;
	bool			m_bsetChannel;
	bool			m_bDoor;
	bool			m_bDimmer;

	int				m_ShotLoadCounter;

	//Logger			*m_pLogger;

private:
	ew_Labview_lasershot();
	virtual ~ew_Labview_lasershot();

	static ew_Labview_lasershot *s_instance;

public:
	char *m_p_Cam;
};


