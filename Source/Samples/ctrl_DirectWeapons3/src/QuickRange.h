#pragma once 

#define WIN32_LEAN_AND_MEAN

#include "DM\DmFoundation.h"
#include "Dm/DmGUI.h"
#include "d3d9.h"


class ctrl_DirectWeapons;
class ew_Scene;
class TargetManager;

enum {QR_HANDGUN, QR_ASSAULT, QR_HUNTING };
enum {QR_STATIC, QR_SNAP, QR_RAPID, QR_MOVINGLR, QR_MOVINGRL, QR_CHARGE, QR_ZIGZAG, QR_ATTACK, QR_RANDOM  };

struct _exercise
{
	DmString	target;
	DmString	action;
	int			mode;
	//float		distance;
	//int			rounds;

	DmPanel		*pEx;	
	DmPanel		*pTarget;	
	DmPanel		*pTargetImg;	
	DmPanel		*pMode, *pModeText;	
	DmPanel		*pDistance;	
	DmPanel		*pRounds, *pRepeats;	
	DmNumeric	*pRoundsText, *pDistanceText, *pRepeatText;
	
};



// scenario -----------------------------------------------------------------------------------------------------------------------
class quickRange
{
public :
			 quickRange();
	virtual	~quickRange();

	void buildButton( DmPanel *pP, int colour, DmString cmd, DmString help );
	void buildButtonTgt( DmPanel *pP, DmString img, DmString cmd, DmString help );
	void buildButtonImg( DmPanel *pP, DmString img, DmString cmd, DmString help, int size );
	void buildGUI( DmPanel *pRoot, DmRect2D rectMenu, float block, float space, ctrl_DirectWeapons *pCTRL  );
	void drawCustomIcon(DmPanel& sender, const DmRect2D& rect );
	void drawCustomColour(DmPanel& sender, const DmRect2D& rect );

	
	void setDefault( int mode );
	void setRange( DmString Range );
	void setSky( DmString Sky );
	void setTarget( int ex, DmString Target, int mode, float distance, int rounds );
	void setExercise( int ex, DmString target, DmString acion, int distance, int rounds );
	void start();
	void toolClickQR(DmPanel& sender, const DmVec2& cursorPosition, DmMouseButton button);
	void show() {m_pQuickRange->show();}
	void hide() {m_pQuickRange->hide();}

	DmString getRange() { return m_pRange->getHelpText(); }
	DmString getSky() { return m_pTime->getHelpText(); }
	DmString getTarget(int ex) { return m_Exercises[ex].pTarget->getHelpText(); }
	int getNumEx() { return m_NumExercises; }
	bool getSingleLane() {return m_bSingleLane;}
	//DmString getBackdrop(int ex) { return m_Exercises[ex].pBackdrop->getHelpText(); }
	int getMode(int ex) { return m_Exercises[ex].mode; }
	float getDistance(int ex) { return m_Exercises[ex].pDistanceText->getText().toFloat(); }
	int getRouds(int ex) { return m_Exercises[ex].pRoundsText->getText().toInt(); }
	int getRepeats(int ex) { return m_Exercises[ex].pRepeatText->getText().toInt(); }
	DmString getSQLgroup() { return m_SQLGroup; }
	void updateDisplay();


	void load();
	void save();
	void lua_export();

	int getShootingPosition() {return m_TEMP_shootingposition;}

private:
	TargetManager	*pTM;
	ew_Scene		*m_pScene;
	DmPanel			*m_pQuickRange;	

	DmString		m_SQLGroup;
	DmPanel			*m_pRange;	
	DmPanel			*m_pTime;
	DmPanel			*m_pLoad;
	DmPanel			*m_pSave;
	DmPanel			*m_pExport;
	DmPanel			*m_pPlus;
	DmPanel			*m_pMinus;
	DmPanel			*m_pNumLanes;	
	DmPanel			*m_pTemperature;	
	DmPanel			*m_pWind;	

	float		m_RepeatX, m_RepeatY, m_RepeatScale;
	int			m_NumExercises;
	bool		m_bSingleLane;

	DmPanel			*m_pPopupTargets;	
	DmPanel			*m_pPopupMode;
	DmPanel			*m_pPopupDistance;
	DmPanel			*m_pPopupRounds;
	DmPanel			*m_pPopupAreas;

	_exercise		m_Exercises[6];
	int				m_Ex;
	int				m_lastMode;

	int				m_TEMP_shootingposition;

	ctrl_DirectWeapons  *m_pCTRL;

};