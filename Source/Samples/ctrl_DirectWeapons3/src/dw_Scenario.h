#pragma once 

#define WIN32_LEAN_AND_MEAN

#include "LUAManager/LUAManager.h"
#include "DM\DmFoundation.h"
#include "cameraAnim.h"
#include "Scene/ew_CameraFollow.h"
#include <vector>
//#include <gdiplus.h>
#include <commdlg.h>
#include "hpdf.h"
//#include "server_defines.h"
#include "Util/ewstring.h"

using namespace Util_LUAManager;
using namespace std;
//using namespace Gdiplus;

class ew_Scene;
class ctrl_DirectWeapons;
class Target;


enum {	TR_TIME,				// {time} do this after X seconds
		TR_ROUNDSCOMPLETED,		// {time} no rounds left in magazine, after X seconds do
		TR_ALLTARGETSSHOT,		// {time} if all targets have been shot
		TR_TARGETSHOT,			// {time, target} if specific target have been shot
		TR_VOICECMD,			// A voice cmd have been reached
	};

enum {	TGT_STATIC,
		TGT_POPUP,	
		TGT_MOVING,	
		TGT_AI		
	};

enum {	ACT_NEXT,				// go to the next exercise
		ACT_VIDEO,				// {name} play a FullScreen Video
		ACT_IMAGE,				// {name} Show image overlay
		ACT_INTRO,				// {name} Show intro
		ACT_SOUND,				// {name} Play a sound
		ACT_LANEVIDEO,			// {name} Play a video thats resticted to your lane - we are already assigned to a lane so no need to specify wich one
		ACT_CAMERAANIM,			// {name} plays a specific camera animation name
		ACT_BASECAM,			// {name} plays a basic camera X meters from origin
		ACT_LOAD,				// {nr} loads number of rounds onto weapon
		ACT_SHOWTARGET,			// {nr} pops us a lane target
		ACT_SHOWTARGET3D,		// {nr} pops up a target at a camera position
		ACT_LAUNCHTARGET3D,		// {nr} pops up a target at a camera position
		ACT_SHOWFEEDBACK,		// 
		ACT_SHOWRANKING,		// 
		ACT_SAVETOSQL,			// 
		ACT_TARGETPOPUP,
		ACT_TARGETMOVE,
		ACT_TARGETAI,
		ACT_ADJUSTWEAPON,		// this will do laser boresight on this lane
		MOVE_LEFT,
		MOVE_RIGHT,
		MOVE_FRONT,
		MOVE_BACK
	};

enum {	WAIT_NONE, 
		WAIT_LANES, 
		WAIT_INSTRUCTOR 	};

enum {	spotting_HIDE, 
		spotting_TOP, 
		spotting_TOPLEFT,
		spotting_BOTTOM,
		spotting_BOTTOMLEFT };

#define SCE_RUNNING		1<<1
#define SCE_DONE		1<<2
#define SCE_WAIT		1<<3
#define SCE_WAITNEXT	1<<4 + SCE_WAIT
#define EX_DONE			1<<5


#define foreach( vector, function) {for(unsigned int i=0; i<vector.size(); i++) vector[i].function;}

struct _shotHitInfo	// shots that have hit the target
{
	float x;
	float y;
	float score;
};
enum { p_STATIC, p_SNAP, p_RAPID, move_LR, move_RL, move_CHARGE, move_ZIGZAG, move_ATTACK, move_RANDOM };
// target -----------------------------------------------------------------------------------------------------------------------
class dw_target
{
public:
			 dw_target( int lane, LPCWSTR type, LPCWSTR mesh, LPCWSTR startCam = NULL, int AI = 0, bool bGRP = false );
	virtual	~dw_target();

	bool update( double dTime );
	void start();

	void popup( unsigned short repeat, float tDn, float tUp );
	void move( unsigned short repeat, int TYPE, float speed  );
	void hide( );
	void show( float x, float y );
	void show( wstring cam, float speed );
	void bullet_hit( f_Vector worldPos, float tof );
	float getScore() { return m_score; }
	void adjust();
	float getAdjustX() {return m_adjustX;}
	float getAdjustY() {return m_adjustY;}
	bool hasbeenShot() {if (m_shotInfo.size() > 0) return true; else return false;}

	void adjustWeapon();

public:
	int				m_lane;
	wstring			m_name;
	wstring			m_ImageName;
	wstring			m_SpriteName;
	unsigned short	m_GUID;
	MeshObjectInstance *pMOI;
	Target			*m_target;

	bool			m_bScoreOnGrouping;
	float			m_score;
	float			m_grouping;

	float		m_adjustX;
	float		m_adjustY;
	float		m_Distance;
	vector<_shotHitInfo>	m_shotInfo;
	float		m_Scale;

	int			m_Type;
	wstring		m_AI_type;
};

// action -----------------------------------------------------------------------------------------------------------------------
class dw_action
{
public:
			 dw_action( int lane, int type, float F );
			 dw_action( int lane, int type, dw_target *T, float x, float y, LPCWSTR cam );
			 dw_action( int lane, int type, dw_target *T, LPCWSTR ai, int A, float B, float C  );
			 dw_action( int lane, int type, LPCWSTR S );
			 dw_action( int lane, int type, LPCWSTR tgt, LPCWSTR name, int eye, int distance, int rnd, LPCWSTR desc );
	virtual	~dw_action();

	bool	update( double dTime );
	void	start();

private:
	unsigned int	m_lane;
	int				m_type;
	wstring			m_name;
	int				m_rounds;
	float			m_x, m_y;
	dw_target*		m_pTarget;
	wstring			m_TargetCam;
	float			m_launchSpeed;		// vir trap and skeet
	float			m_baseCamDistance;
	wstring			m_AI;
	int				m_repeat;
	float			m_tUp, m_tDn;
	int				m_dir;
	float			m_speed;

	wstring			m_tgt;
	int				m_eye;
	int				m_distance;
	int				m_rnd;
	wstring			m_desc;

};

// trigger -----------------------------------------------------------------------------------------------------------------------
class dw_trigger
{
public:
			 dw_trigger( int lane, int type, float time);
			 dw_trigger( int lane, int type, float time, float tS, float Score);
			 dw_trigger( int lane, int type, float time, int target);
			 dw_trigger( int lane, int type, LPCWSTR str);
	virtual	~dw_trigger();

	void clear()	{ m_actions.clear(); }
	bool update( double dTime );

	void pushAction( dw_action *pA )			{  m_actions.push_back(*pA); }

private:
	int		m_lane;
	int		m_type;
	float	m_delay;				// maak seker dit kan reset op next
	bool	b_Trigger;				// maak seker dit kan resert op next
	vector<dw_action> m_actions;

	float	m_bonusTime;
	float	m_bonusScore;
	DmString m_str;
};

// scene -----------------------------------------------------------------------------------------------------------------------
class dw_scene
{
public:
			 dw_scene( int lane, int wait);
	virtual	~dw_scene();

	void clear()	{ m_triggers.clear(); }
	int update( double dTime );

	void tr_time( float time )					{ m_triggers.push_back(dw_trigger( m_lane, TR_TIME, time ) ); }
	void tr_roundsCompleted(float time)			{ m_triggers.push_back(dw_trigger( m_lane, TR_ROUNDSCOMPLETED, time ) ); }
	void tr_allTargetsShot(float time, float tS, float Score)			{ m_triggers.push_back(dw_trigger( m_lane, TR_ALLTARGETSSHOT, time, tS, Score ) ); }
	void tr_targetShot( float time, int target ){ m_triggers.push_back(dw_trigger( m_lane, TR_TARGETSHOT, time, target ) ); }
	void tr_voiceCMD( LPCWSTR cmd){ m_triggers.push_back(dw_trigger( m_lane, TR_VOICECMD, cmd ) ); }

	void pushAction( dw_action *pA )			{ m_triggers.back().pushAction(pA); }

private:
	int					m_lane;
	int					m_bPauseWhenDone;		// after this one all lanes must sync
	vector<dw_trigger>	m_triggers;
};

// exercise -----------------------------------------------------------------------------------------------------------------------
class dw_exercise
{
public:
			 dw_exercise(  int lane, LPCWSTR name, LPCWSTR description, int maxscore );
	virtual	~dw_exercise();

	void clear()	{ m_targets.clear();	m_scenes.clear(); }
	int update( double dTime, bool bNext );
	void start();
	void stop();

	int add_target( LPCWSTR type, bool bGRP=false );
	int add_AI_target( LPCWSTR type, LPCWSTR mesh, LPCWSTR startCam, int AI );

	void set_Eye( int eyePosition );
	void set_SpottingScope(int SCOPE );
	

	void add_scene( int wait )		{	m_scenes.push_back(dw_scene( m_lane, wait )); m_currentScene = m_scenes.begin();	}

	void tr_time( float time )				{ m_scenes.back().tr_time( time ); }
	void tr_roundsCompleted( float time )	{ m_scenes.back().tr_roundsCompleted( time ); }
	void tr_allTargetsShot( float time, float tS, float Score )	{ m_scenes.back().tr_allTargetsShot( time, tS, Score ); }
	void tr_targetShot(  float time, int target )	{ m_scenes.back().tr_targetShot( time, target ); }
	void tr_voiceCMD(  LPCWSTR cmd )	{ m_scenes.back().tr_voiceCMD( cmd ); }

	void pushAction( dw_action *pA )				{  m_scenes.back().pushAction(pA); }
	void setNumRounds( int R ) { if (R>0) m_numRounds = R;}
	void printAsPictures( ) { m_bPrintPictures = true; }
	bool getAllTargetsShot();
	void addBonusScore( int bonusScore )	{ m_scoreBonusPoints = bonusScore; }
	//int getScore() {return m_score + m_scoreBonusPoints;}
	int getScore() {return m_score;}
	double getTime() {return m_dTime;}

	//void getTargets( vector<_target_lead> T );
	
public:
	int							m_lane;
	int							m_maxscore;
	int							m_score;
	int							m_scoreBonusPoints;
	int							m_shotsFired;
	int							m_numRounds;
	vector<dw_target*>			m_targets;
	vector<dw_scene>			m_scenes;
	vector<dw_scene>::iterator	m_currentScene;
	DmString					m_name;
	DmString					m_description;
	double						m_dTime;
	double						m_ShootingDistance;
	bool						m_bPrintPictures;

	int m_eyePosition;
	int m_scopePosition;
};


// lane -----------------------------------------------------------------------------------------------------------------------
class dw_lane
{
public:
			 dw_lane( int lane );
	virtual	~dw_lane();

	void clear()	{ m_exercises.clear(); }
	int update( double dTime, bool bNext );
	void start();
	double getExTime(){return m_currentExercise->getTime();}

	// lua -------------------------------------------------------------------------------
	void add_exercise( LPCWSTR name, LPCWSTR desc, int maxscore )	{ m_exercises.push_back( dw_exercise( m_lane, name, desc, maxscore ) ); m_currentExercise=m_exercises.begin();	}
	int add_target( LPCWSTR name, bool bGRP=false )		{ return m_exercises.back().add_target( name, bGRP ); }
	int add_AI_target( LPCWSTR type, LPCWSTR mesh, LPCWSTR startCam, int AI )		{ return m_exercises.back().add_AI_target( type, mesh, startCam, AI ); }
	void add_scene( int wait )			{ m_exercises.back().add_scene( wait ); }

	void set_Eye( int eyePosition )		{ m_exercises.back().set_Eye( eyePosition ); }
	void set_SpottingScope( int scope )		{ m_exercises.back().set_SpottingScope( scope ); }
	
	

	void tr_time( float time )						{ m_exercises.back().tr_time( time ); }
	void tr_roundsCompleted( float time )			{ m_exercises.back().tr_roundsCompleted( time ); }
	void tr_allTargetsShot( float time, float tS, float Score )			{ m_exercises.back().tr_allTargetsShot( time, tS, Score ); }
	void tr_targetShot(  float time, int target )	{ m_exercises.back().tr_targetShot( time, target ); }
	void tr_voiceCMD(  LPCWSTR cmd )	{ m_exercises.back().tr_voiceCMD( cmd ); }

	dw_target *resolveTarget( int T )				{ return m_exercises.back().m_targets[ T ]; }	// returns dw_target * from int - FOR the LAST exercise added to a specific lane
	void pushAction( dw_action *pA )				{ m_exercises.back().pushAction(pA); }

	void loadRounds( int r, int m, float t )	{ m_numRounds = r; m_numMAGrounds = r; m_magazines=m; m_reloadTime=t; if (m_numRounds>0) m_roundsFired = 0;  m_currentExercise->setNumRounds( __max(r, r*m) );}
	void printAsPictures( )						{ m_exercises.back().printAsPictures();}
	//int addShot( int x, int y );
	int getNumRoudnsLeft() {return m_numRounds - m_roundsFired;}
	int getNumRounds() {return m_numRounds;}
	int getNumRoundsFired() {return m_roundsFired;}
	int getNumRoundsInEx() {return m_currentExercise->m_numRounds;}
	bool getAllTargetsShot( ) {return m_currentExercise->getAllTargetsShot();}
	int getTotalScore( ) { int score = 0; for(unsigned int i=0; i<m_exercises.size(); i++) score += m_exercises[i].getScore(); return score;}
	void addBonusScore( int bonusScore )	{ m_currentExercise->addBonusScore( bonusScore ); }

	int getNumExercises() const { return m_exercises.size(); }
	dw_exercise getExercise(int idx) const { return m_exercises[idx]; }

	void onFire() {m_roundsFired++;}
	//void getTargets( vector<_target_lead> T )	{ m_currentExercise->getTargets( T ); }

private:
	unsigned int					m_lane;
	int								m_numRounds;
	int								m_numMAGrounds;
	int								m_magazines;
	float							m_reloadTime;
	float							m_reloadCounter;
	int								m_roundsFired;
	vector<dw_exercise>::iterator	m_currentExercise;
	vector<dw_exercise>				m_exercises;
	float							m_shotTimer;
	bool							m_bSceneRunning;


	MeshObjectInstance				*m_p_laneMOI;

	//Nuwe printing kode - moet dalk later 'n klas word
	HPDF_Doc		m_PDF;
	HPDF_Page		m_pdf_Page;
	HPDF_Font		m_pdf_Font;
	float			m_pdf_Width;
	float			m_pdf_Height;
	int				m_pdf_Columns;
	int				m_pdf_totalMaxScore;
	int				m_pdf_totalScore;
	int				m_pdf_X;
	int				m_pdf_Y;
	void			pdf_Start();
	void			pdf_Stop();
	void			pdf_Header();
	void			pdf_Exercise( dw_exercise ex );
	ewstring		m_PrintCommand;
public:
	void			pdf_Print();
};


// scenario -----------------------------------------------------------------------------------------------------------------------
class dw_scenario
{
public :
			 dw_scenario();
	virtual	~dw_scenario();

	void init(int numLanes);
	void clear();
	void start();
	void stop();

	double getExTime() {return m_lanes[0].getExTime();}
	// lua binding to create it all ----------------------------------------------------------
	void create_terrainScenario( LPCWSTR name, LPCWSTR terrain );
	void create_modelScenario( LPCWSTR name, LPCWSTR mesh, LPCWSTR props );
	void create_speculativeScenario( LPCWSTR name );
	void create_videoScenario( LPCWSTR name  );
	void load_Sky( LPCWSTR sky );
	void load_Cameras( LPCWSTR cam );
	void load_Soundtrack( LPCWSTR sound );
	void setSQLname( DmString group, DmString name ) {m_SQLGroup = group; m_SQLName = name; }
	void SQLError(const char *msg);
	void saveToSQL();
	void maxNumLanes( int lanes ) {m_max_lanes = lanes;}
	void createMyself(LPCWSTR cam);

	void add_exercise( LPCWSTR name, LPCWSTR desc, int maxscore )	{ foreach( m_lanes, add_exercise( name, desc, maxscore ) ); }

	int	 add_backdrop( LPCWSTR name )	{ int T; for( unsigned int i=0; i<m_lanes.size(); i++) T = m_lanes[i].add_target( (name + DmString(i+1)).c_str() ) ; return T; }
	int	 add_target( LPCWSTR name )		{ int T; for( unsigned int i=0; i<m_lanes.size(); i++) T = m_lanes[i].add_target( name ) ; return T; }
	int	 add_targetGRP( LPCWSTR name )		{ int T; for( unsigned int i=0; i<m_lanes.size(); i++) T = m_lanes[i].add_target( name, true ) ; return T; }
	int	 add_lane_target( int lane, LPCWSTR name )		{ int T; T = m_lanes[lane].add_target( name ) ; return T; }
	int	 add_AI_target( LPCWSTR type, LPCWSTR mesh, LPCWSTR startCam, int AI )		{ int T; T = m_lanes[0].add_AI_target( type, mesh, startCam, AI ) ; return T; }
	int	 add_lane_AI_target( int lane, LPCWSTR type, LPCWSTR mesh, LPCWSTR startCam, int AI )		{ int T; T = m_lanes[lane].add_AI_target( type, mesh, startCam, AI ) ; return T; }

	void	 set_Eye( int eyePosition )		{ m_lanes[0].set_Eye( eyePosition ) ; }
	void	 set_SpottingScope( int scope )		{ m_lanes[0].set_SpottingScope( scope ) ; }

	void add_scene( int wait )									{ foreach( m_lanes, add_scene( wait ) ); }

	void tr_time( float time )									{ foreach( m_lanes, tr_time( time ) ); }
	void tr_roundsCompleted( float time )						{ foreach( m_lanes, tr_roundsCompleted( time ) ); }
	void tr_allTargetsShot( float time, float tS, float Score )	{ foreach( m_lanes, tr_allTargetsShot( time, tS, Score ) ); }
	void tr_targetShot(  float time, int target )				{ foreach( m_lanes, tr_targetShot( time, target ) ); }
	void tr_voiceCMD(  LPCWSTR cmd )							{ foreach( m_lanes, tr_voiceCMD( cmd ) ); }
	
	dw_target *resolveTarget( int lane, int T )				{ return m_lanes[lane].resolveTarget( T ); }	// returns dw_target * from int - FOR the LAST exercise added to a specific lane
	void act_Video( LPCWSTR video )							{ foreach( m_lanes, pushAction( &dw_action( i, ACT_VIDEO, video  ))); }
	void act_Sound( LPCWSTR sound )							{ foreach( m_lanes, pushAction( &dw_action( i, ACT_SOUND, sound  ))); }
	void act_Image( LPCWSTR image )							{ foreach( m_lanes, pushAction( &dw_action( i, ACT_IMAGE, image  )));  }
	void act_Intro( int Tgt, LPCWSTR name, int eye, int distance, int rnd, LPCWSTR desc )		{ foreach( m_lanes, pushAction( &dw_action( i, ACT_INTRO, resolveTarget(i, Tgt)->m_SpriteName.c_str(), name, eye, distance, rnd, desc  )));  }
	void act_Camera( LPCWSTR cam )							{ foreach( m_lanes, pushAction( &dw_action( i, ACT_CAMERAANIM, cam  ))); }
	void act_BasicCamera( float Distance )					{ foreach( m_lanes, pushAction( &dw_action( i, ACT_BASECAM, Distance  ))); }
	void act_Loadrounds( int r )							{ foreach( m_lanes, pushAction( &dw_action( i, ACT_LOAD, (float)r  ))); }
	void printAsPictures( )									{ foreach( m_lanes, printAsPictures() ); }
	void LoadMagazines( int R, int M, float T );
	void act_ShowFeedback()									{ foreach( m_lanes, pushAction( &dw_action( i, ACT_SHOWFEEDBACK, L"" ))); }
	void act_ShowRanking()									{ foreach( m_lanes, pushAction( &dw_action( i, ACT_SHOWRANKING, L"" ))); }
	void act_SavetoSQL()									{ foreach( m_lanes, pushAction( &dw_action( i, ACT_SAVETOSQL, L"" ))); }
	void act_ShowTarget( int Tgt, float x, float y )		{ foreach( m_lanes, pushAction( &dw_action( i, ACT_SHOWTARGET, resolveTarget(i, Tgt), x, y, L""  ))); }
	void act_ShowTarget3D( int ln, int Tgt, LPCWSTR cam )	{ m_lanes[ln].pushAction( &dw_action( ln,  ACT_SHOWTARGET3D, resolveTarget(ln, Tgt), 0, 0, cam )); }
	void act_LaunchTarget3D( int ln, int Tgt, LPCWSTR cam, float speed )	{ m_lanes[ln].pushAction( &dw_action( ln,  ACT_LAUNCHTARGET3D, resolveTarget(ln, Tgt), speed, 0, cam )); }
	//void act_LoadSpeculative( );

	void act_TargetPopup( int Tgt, int numR, float tDn, float tUp )	{ foreach( m_lanes, pushAction( &dw_action( i, ACT_TARGETPOPUP, resolveTarget(i, Tgt), L"AI_popup", numR, tDn, tUp )));	}
	void act_TargetMove( int Tgt, int numR, int dir, float speed )	{ foreach( m_lanes, pushAction( &dw_action( i, ACT_TARGETMOVE, resolveTarget(i, Tgt), L"AI_move", numR, (float)dir, speed  ))); }
	void act_TargetAI( int Tgt, LPCWSTR AI )						{ foreach( m_lanes, pushAction( &dw_action( i, ACT_TARGETAI, resolveTarget(i, Tgt), AI, 0, 0.0f, 0.0f  ))); }

	void act_AdjustWeapon( int Tgt )	{	foreach( m_lanes, pushAction( &dw_action( i, ACT_ADJUSTWEAPON, resolveTarget(i, Tgt), 0, 0, L""  )));  }

	bool testTargetsLoaded();
	bool update( double dTime );
	void next();
	bool Print( );
	bool PrintVideo( );
	bool playlistPrint();
	
	void onFire( int lane ) {m_lanes[lane].onFire();}

	int get_numLanes()						{ return __min(m_pc_lanes, m_max_lanes); }
	void loadRounds( int lane, int r, int m, float t );
	//int addShot( int lane, int x, int y )	{ return m_lanes[lane].addShot( x, y );     } 
	int getNumRoundsLeft( int lane )		{ return m_lanes[lane].getNumRoudnsLeft(); }
	int getNumRoundsInEx( int lane )		{ return m_lanes[lane].getNumRoundsInEx(); }
	bool getAllTargetsShot( int lane )		{ if(m_max_lanes==1) return m_lanes[0].getAllTargetsShot(); else return m_lanes[lane].getAllTargetsShot(); }
	void addBonusScore( int lane, int bonusScore )	{ m_lanes[lane].addBonusScore( bonusScore ); }
	int getTotalScore(int lane)				{ return m_lanes[lane].getTotalScore(); }
	DmString getName() {return m_Name;}

	void bullet_hit( unsigned short GUID, f_Vector worldPos, float tof );
	//unsigned int getLiveChannels();

	void pdf_Print( int lane ) { m_lanes[lane].pdf_Print(); }
	//void getTargets( vector<_target_lead> T )	{ foreach( m_lanes, getTargets( T )); }

private:
	ew_Scene		*m_pScene;
	int				m_Type;
	bool			m_bPlaying;
	float			m_LoadDelay;
	bool			m_bLoopingScenario;	// if false we go back to teh main menu, wehn done, otherwise restart
	bool			m_bWaitingForTargetsToLoad;

	bool			m_bSplitScreen;		// vir skou scenario
	DmString		m_Terrain;
	DmString		m_Building;
	DmString		m_Sky;

	DmString		m_Name;
	DmString		m_SQLName;
	DmString		m_SQLGroup;
	DmString		m_Description;

	SoundSource		*m_pSoundSource;

	vector<dw_lane>	m_lanes;

	//SoundSource*	m_pBackgroundMusic;

public:
	int				m_max_lanes;		// maximum number of lanses that this scenario can handle simultaniously
	int				m_pc_lanes;			// maximum number of lanses that this scenario can handle simultaniously
	int				m_maxScore;
	static dw_scenario			*s_pSCENARIO;
	static cameraAnim			s_Cameras;
	static ctrl_DirectWeapons	*s_pCTRL;
	static ew_Scene				*s_pSCENE;
	static vector<dw_target*>	s_targets;
	static dw_target*			s_targetGUIDs[1024];
};