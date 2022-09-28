#pragma once 

#define WIN32_LEAN_AND_MEAN

#include "DM\DmFoundation.h"
#include "Dm/DmGUI.h"
#include "d3d9.h"
#include "Maths/Maths.h"

class ctrl_DirectWeapons;

enum {LAYOUT_NARROW, LAYOUT_WIDE, LAYOUT_ADJUST};


#define NUM_SHOT_TEX 100
#define SHOT_TEX_SIZE 512
//#define RUBBER_BUFFER 0.1

// -------------------------------------------------------------------------------------------------------------------------------
class guiSHOT
{
public:
	guiSHOT(){;}
	virtual ~guiSHOT(){;}

	void onFire( int num, DmString ammo, float time, float dT, float wind, DmString overlay, d_Vector pos, d_Vector dir, float fov, LPDIRECT3DTEXTURE9	pTex );
	void onHit( d_Vector hitPos, float tof, float tX, float tY, int score, int totalscore, int grp );

public:
	LPDIRECT3DTEXTURE9		m_pTexture;
	float					m_pTexture_FOV;
	unsigned int			m_shotNum;
	float					m_time;
	float					m_dT;		// diff time from previous shot.
	DmString				m_ammo;
	float					m_wind;
	DmString				m_overlay;
	d_Vector				m_Position;
	d_Vector				m_Dir;

	bool					m_bHIT;
	d_Vector				m_hitPos;
	float					m_tof;
	double					m_Distance;
	d_Vector				m_ProjectedHit;
	double					m_drop;
	double					m_drift;
	float					m_imageX;
	float					m_imageY;
	float					m_targetX;
	float					m_targetY;
	float					m_score;
	float					m_totalscore;
	float					m_grouping;


};


enum {	guitype_TARGET, guitype_MULTITARGET, guitype_BOOLTARGET, guitype_IMAGE, guitype_VIDEO };
enum {	error_OK,
		error_Rifle_Poor, error_Rifle_Horizontal, error_Rifle_Vertical, error_Rifle_Split, error_Rifle_Position,
		error_Pistol_Up, error_Pistol_Healing, error_Pistol_Thumbing, error_Pistol_Tightning, 
		error_Pistol_Down, error_Pistol_Jerking, error_Pistol_Fingers, error_Pistol_Trigger, error_Pistol_Pushing };
// -------------------------------------------------------------------------------------------------------------------------------
class shotVIEW
{
public:
	shotVIEW(){;}
	virtual ~shotVIEW(){;}

	void buildGUI( DmPanel *pRoot );
	void layoutNarrow( float width, int numL, float x=0.0f, float y=0.0f );
	void layoutSpotting( float w, float h );
	void layoutDebrief( float w, float h );
	void displayShot( vector<guiSHOT> *pS, bool bLIVEFIRE = false, int numR = 0 );
	void displayDebrief( guiSHOT *pS );
	void drawCustomShot(DmPanel& sender, const DmRect2D& rect );
	void drawCustomError(DmPanel& sender, const DmRect2D& rect );
	void clear();
	void setTarget ( DmString target, float scale);
	void analiseShots( vector<guiSHOT> *pS ); 
	

private:
public:
	int						m_type;
	DmPanel					*m_pIMG;
	DmPanel					*m_pHit[100];
	LPDIRECT3DTEXTURE9		m_pTexture;
	DmString				m_Overlay;
	DmString				m_TargetImage;
	float					m_TargetScale;
	float					m_Isize;
	DmPanel					*m_Info[3][10];
	DmPanel					*m_Errors;
	
		// analise
	RECT					m_AABB;
	float					m_Angle;
	float					m_Aspect;
	d_Vector				m_AABB_center;
	DmVec2					m_Centerpos;
	DmVec2					m_WH;
	int						m_ERROR;
	int						m_ShotsOnTarget;
};


// -------------------------------------------------------------------------------------------------------------------------------
class guiLANE
{
public:
	guiLANE(){;}
	virtual ~guiLANE(){;}
	void clear() {m_Shots.clear(); m_SV_Inst.clear(); m_SV_Spotting.clear(); m_SV_Popup.clear(); }
	void setShot( guiSHOT S ) {m_Shots.push_back( S ); m_CurrentShot=m_Shots.size();}
	void replayUpdate( float dTime );
	void update( float dTime );
	void restart( ) {m_CurrentShot = 0;  m_SV_Spotting.clear(); m_SV_Inst.clear(); /*clear();*/}
	void cancelreplay( ) {	m_CurrentShot = m_Shots.size();	}

	float	m_shotDelay;
	void setShotDelay( int delay ) { m_shotDelay = (float)delay / 1000.0f ;	/*	ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"setShotDelay  m_shotDelay( %d ms )", (int)(m_shotDelay * 1000 )  );*/}

	int addShot();
	void loadRounds( int r, int m, float t ) { m_numRounds = r; m_numMAGrounds = r; m_magazines=m; m_reloadTime=t; if (m_numRounds>0) m_numRoundsFired = 0; m_shotTimer = 0.0f; }
	int getRoundsLeft() { return m_numRounds - m_numRoundsFired;}



public:
	int				m_CurrentShot;
	vector<guiSHOT> m_Shots;
	shotVIEW		m_SV_Inst;
	shotVIEW		m_SV_Spotting;
	shotVIEW		m_SV_Popup;


	float			m_shotTimer;
	int				m_numRounds;
	int				m_numRoundsFired;
	int								m_numMAGrounds;
	int								m_magazines;
	float							m_reloadTime;
	float							m_reloadCounter;

};


struct _user
{
	DmString	rank;
	DmString	initials;
	DmString	surname;
	DmString	id;
	DmString	userID;
	DmString	unit;
	DmString	subunit;
	DmString	displayName;
	float		score;
	int			bestScore;
};
/*
struct _panels
{
	DmPanel		*pBase;
	DmPanel		*pLane;

	DmPanel		*pName;
	DmPanel		*pScore;
	DmPanel		*pMaxScore;
	DmPanel		*pShotcount;
	DmPanel		*pMaxShotcount;
	DmPanel		*pShotsLeft;

	// weapon setup
	DmPanel		*pOffsetTxt;
	DmPanel		*pOffset;
	DmPanel		*pOffsetReset;


	DmPanel		*pTarget;
	DmPanel		*pTargetImage;
	DmPanel		*pTargetName;
	DmPanel		*pTargetScore;
	DmPanel		*pTargetGrouping;
	DmPanel		*pAdjust;

	DmPanel		*pPrintButton;

	DmPanel		*pNoAmmo;
	DmPanel		*pAllTargetsShot;
	DmPanel		*pBonusScore;
	DmPanel		*pFeedback;
	DmPanel		*pRanking;
	DmPanel		*pR[5][4];
	

	DmPanel		*pScreenshotTarget;
	float		m_ShotTimer;
	int			m_currentMouseOverShot;
	//DmPanel		*pShotOverlay;
	//DmPanel		*pShotOverlayRifle;

	DmPanel		*pSpacer;

	DmPanel					*pTimeline;
	DmPanel					*pTimelineTxt[25];
	DmPanel					*pTimelineImage[25];
	float					shotTime[25];
	LPDIRECT3DTEXTURE9		g_pTimelineTexture[25];
};
*/

// scenario -----------------------------------------------------------------------------------------------------------------------
class lane_GUI
{
public :
			 lane_GUI();
	virtual	~lane_GUI();

	void setLive(bool bLive);
	void toolClick( DmPanel& sender, const DmVec2& cursorPosition, DmMouseButton button );
	void buildGUI( DmPanel *pRoot, int numLanes, bool bSplitScreen, DmRect2D rect_Menu, DmRect2D rect_3D, float block, float space, ctrl_DirectWeapons *pCTRL  );
	void liveLayout();
	void debriefLayout();
	
	void setTime( float time ) {m_Time=time;	for (int i=0; i<m_NumLanes; i++) m_LANES[i].restart();	debriefLayout();	}
	void cancelReplay () { for (int i=0; i<m_NumLanes; i++) m_LANES[i].cancelreplay(); }

	int getLanefromCursor( float x );
	int addShot( int lane ) { return m_LANES[lane].addShot(); }
	void clear();
	void showIntro( unsigned int type, DmString title, int numLanes );
	void setAmmunition( int lane, float distance );
	void setWeather(float wind, float drift );
	void showIntro_Lanes( );
	void showLive( );
	void showLaneSelect( );
	void showFinal( );
	void showVideo( );
	void showDebrief( );
	void showRoundsCompleted( unsigned int lane );// { m_Lanes[l].pNoAmmo->show();}   THEN AUTO HIDE after 5 seconds
	void showTimeBonus( unsigned int lane, int bonusScore=0 );						// THEN autohide after 3seconds
	void showRanking(unsigned int l, _user *pR[5], unsigned int rank );
	void replayUpdate( float dTime );
	void update( float dTime );
	

	void setTotalScore( int lane, int score );
	
	// Timeline functions -------------------------------------------------------------------------------------------------------
	void	onMouseOver(DmPanel& sender);
	void	onMouseOverSpot(DmPanel& sender);
	void	onMouseOut(DmPanel& sender);

	void setShotDelay( int delay)  { for (int i=0; i<m_NumLanes; i++) m_LANES[i].setShotDelay(delay); }

	void setScore(int lane, int score){;}//					{ m_Lanes[lane].pScore->setText( DmString( score ) ); }				// GLOBAL SCORE vanaf DW_lane update
	void setShotcount(int lane, int shotcount){;}//			{ m_Lanes[lane].pShotcount->setText( DmString( shotcount ) ); }

	
	void displayShotImage(int lane, int shotNumber);							// used fpr debreif pl;aybnack
	int getCurrentShot(int lane, float dTime);									// used for debrief playback - wquery shot VS time   WRONG PLACE

	void showFeedback( unsigned int lane, bool bLive, int numR );
	bool is_showFeedback( unsigned int lane ){ return !m_pLive3D->findWidget( lane )->findWidget( L"content" )->isHidden();}
	
	void setEyePosition(int pos)
	{
		switch( pos )
		{
		case 0: m_pPosition->setValue( L"icon_prone" ); break;
		case 1: m_pPosition->setValue( L"icon_sitting" ); break;
		case 2: m_pPosition->setValue( L"icon_kneeling" ); break;
		case 3: m_pPosition->setValue( L"icon_standing" ); break;
		}
		m_pPositionSelect->hide();
	}



	// Per esxercise --------------------
	void setTargetType(int l, DmString target, float scale);					// maybe multipler types?, or multiple calls if an exercise hasmutiple targets
	//void setShot(int l , float X, float Y, int score, int grouping );			// from dw_target::bullet_hit()
	//void showShotImage(int lane, int shotNumber, bool bRifle, float time);     // van ctrl_DirectWeapons::fire()
	LPDIRECT3DSURFACE9 onFire( int lane, int shotNumber, DmString ammo, float time, float wind, DmString overlay, d_Vector pos, d_Vector dir, float fov );
	void onHit( int lane, d_Vector hitPos, float tof, float tX, float tY, int score, int totalscore, int grp );

	void movescope( int position );
	unsigned int getActiveLane() {return m_ActiveLane;}
	void setActiveLane(int L) {m_ActiveLane = L;}

	void loadRounds( int lane, int r, int m, float t );// { m_LANES[lane].loadRounds(r, m, t);}
	int getRoundsLeft( int l ) { return m_LANES[l].getRoundsLeft(); }
	
	void copytoOffsceenandRPC( LPDIRECT3DSURFACE9 from );

private:
	unsigned int	m_type;

	unsigned int		m_NumLanes;	
	unsigned int		m_NumActiveLanes;	
	ctrl_DirectWeapons	*m_pCTRL;
	DmRect2D			m_rect_3D;
	DmRect2D			m_rect_Menu;
	int					m_bScopePosition;

	DmPanel				*m_pLive3D;
	DmPanel				*m_pDistanceOverlay;
	float				m_DOdistance;
	DmPanel				*m_pVideoTime;
	DmPanel				*m_pLiveMenu;
	DmPanel				*m_pPositionSelect;
	DmPanel				*m_pSpottingSelect;
	DmPanel				*m_pPosition;
	DmPanel				*m_pSpotting;

	DmPanel				*m_pIntro3D;
	DmPanel				*m_pIntroMenu;
	DmPanel				*m_pLoadWaitMenu;
	DmPanel				*m_pLoadWait3D;
	unsigned int		m_ActiveLane;	// for 1 lane operations

	LPDIRECT3DTEXTURE9	m_pTexture[NUM_SHOT_TEX];
	LPDIRECT3DSURFACE9	m_pSurface[NUM_SHOT_TEX];
	LPDIRECT3DSURFACE9	m_pOffscreenSurface;
	unsigned int		m_CurrentTexture;
public:
	vector<guiLANE>		m_LANES;
	float				m_Time;

};