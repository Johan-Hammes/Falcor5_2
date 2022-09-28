
#include "quickRange.h"
#include "Controllers/ctrl_DirectWeapons/ctrl_DirectWeapons.h"
#include "ew_Scene.h"
#include "Controllers/ctrl_DirectWeapons/Target.h"
#include "Controllers/ctrl_DirectWeapons/TargetManager.h"
#include "Util/DialogUtil.h"

quickRange::quickRange()
{
	m_pScene = ew_Scene::GetSingleton();
	m_lastMode = -1;
	m_SQLGroup = L"";
}

quickRange::~quickRange()
{
}


void quickRange::drawCustomColour(DmPanel& sender, const DmRect2D& rect )
{
	if (sender.getValue() == L"") return;

	DmSpriteManager& pSM = *m_pScene->getSpriteManager();
	DmRect2D R = rect;

	// Background colour- ---------------------------------------------------
	DmSprite& spr = pSM[L"menu_white"];
	spr.setColourAlphaMultiplier( sender.getValue().toUInt() );
	spr.draw( R );
}

void quickRange::drawCustomIcon(DmPanel& sender, const DmRect2D& rect )
{
	DmSpriteManager& pSM = *m_pScene->getSpriteManager();
	if ( pSM.containsSprite( sender.getValue() ) )
	{
		DmSprite& spr = pSM[ sender.getValue() ];
		DmRect2D R = rect;
		spr.draw( R );
	}
}


void quickRange::buildButton( DmPanel *pP, int colour, DmString cmd, DmString help )
{
	pP->setValue( DmString( colour ) ); 
	pP->setVerticalTextAlignment(dmAlignBottom); 
	pP->setHorizontalTextAlignment( dmAlignLeft ); 
	DmConnect( pP->drawCustomBackground, *this, &quickRange::drawCustomColour ); 
	
	DmPanel *pO = &pP->addWidget<DmPanel>( L"k", L"", L"metro_border" ); 	
	pO->setCommandText( cmd ); 	
	pO->setHelpText( help );
	DmConnect( pO->mouseClick, this, &quickRange::toolClickQR );
}

void quickRange::buildButtonTgt( DmPanel *pP, DmString img, DmString cmd, DmString help )
{
	pP->setValue( img ); 
	pP->setVerticalTextAlignment(dmAlignBottom); 
	pP->setHorizontalTextAlignment( dmAlignLeft ); 
	DmConnect( pP->drawCustomBackground, *this, &quickRange::drawCustomIcon ); 
	
	DmPanel *pO = &pP->addWidget<DmPanel>( L"k", L"", L"metro_border" ); 	
	pO->setCommandText( cmd ); 	
	DmConnect( pO->mouseClick, this, &quickRange::toolClickQR );
}

void quickRange::buildButtonImg( DmPanel *pP, DmString img, DmString cmd, DmString help, int size )
{
	DmPanel *pImg = &pP->addWidget<DmPanel>( img, L"", 10, 10, size-20, size-20, L"empty" ); 	
	pImg->setValue( img ); 
	DmConnect( pImg->drawCustomBackground, *this, &quickRange::drawCustomIcon ); 
	
	DmPanel *pO = &pP->addWidget<DmPanel>( L"k", L"", L"metro_border" ); 	
	pO->setCommandText( cmd ); 	
	pO->setValue( img );
	pO->setHelpText( help );
	DmConnect( pO->mouseClick, this, &quickRange::toolClickQR );
}

void quickRange::buildGUI( DmPanel *pRoot, DmRect2D rectMenu, float block, float space, ctrl_DirectWeapons *pCTRL  )
{
	m_pCTRL = pCTRL;
	m_pQuickRange = &pRoot->addWidget<DmPanel>( L"QuickScenario", L"", L"metro_darkblue" );
	m_pQuickRange->setPosition( rectMenu.topLeft.x, rectMenu.topLeft.y  );
	m_pQuickRange->setSize( rectMenu.width(), rectMenu.height() );
	
	int dH = (block + space) / 4; //1 spacer grootte
	int dW = block*2 + space;
	int X = (block + space) / 2;
	int Y = 90 + dH;

	DmPanel *pTXT = &m_pQuickRange->addWidget<DmPanel>( L"QR", L"Quick Range", X, 40, X+300, 40+40, L"metro_title" );
	pTXT->setHorizontalTextAlignment( dmAlignLeft );

	m_pRange = &m_pQuickRange->addWidget<DmPanel>( L"range", L"", X, Y, X+dW, Y+dH*3, L"metro_cyan" );
	m_pRange->setValue( L"area_skietbaan" );
	m_pRange->setCommandText( L"area" );
	m_pRange->setVerticalTextAlignment(dmAlignBottom); 
	m_pRange->setHorizontalTextAlignment( dmAlignLeft ); 
	DmConnect( m_pRange->drawCustomBackground, *this, &quickRange::drawCustomIcon );
	DmConnect( m_pRange->mouseClick, this, &quickRange::toolClickQR );

	m_pTime = &m_pQuickRange->addWidget<DmPanel>( L"time", L"time", X, Y+dH*4, X+dW, Y+dH*7, L"metro_cyan" );
	m_pTime->setValue( L"area_skietbaan" );
	m_pTime->setCommandText( L"QuickScenario_start" );
	m_pTime->setVerticalTextAlignment(dmAlignBottom); 
	m_pTime->setHorizontalTextAlignment( dmAlignLeft ); 
	DmConnect( m_pTime->drawCustomBackground, *this, &quickRange::drawCustomIcon );
	DmConnect( m_pTime->mouseClick, this, &quickRange::toolClickQR );
	m_pTime->hide();
	
	m_pNumLanes = &m_pQuickRange->addWidget<DmPanel>( L"lanes", L"", X, Y+dH*4, X+dW, Y+dH*7, L"metro_cyan" );
	m_pNumLanes->setValue( L"QR_lanes_multi" );
	m_pNumLanes->setCommandText( L"lanes" );
	DmConnect( m_pNumLanes->drawCustomBackground, *this, &quickRange::drawCustomIcon );
	DmConnect( m_pNumLanes->mouseClick, this, &quickRange::toolClickQR );
	m_bSingleLane = false;  

	m_pLoad = &m_pQuickRange->addWidget<DmPanel>( L"load", L"", X, Y+dH*8, X+dH*2, Y+dH*10, L"" );
	m_pLoad->setValue( L"icon_load" );
	m_pLoad->setCommandText( L"load" );
	DmConnect( m_pLoad->drawCustomBackground, *this, &quickRange::drawCustomIcon );
	DmConnect( m_pLoad->mouseClick, this, &quickRange::toolClickQR );
	//buildButton( m_pLoad, 0x00001933, L"QuickScenario_load", L"");

	m_pSave = &m_pQuickRange->addWidget<DmPanel>( L"save", L"", X+dW/2-dH, Y+dH*8, X+dW/2+dH, Y+dH*10, L"" );
	m_pSave->setValue( L"icon_save" );
	m_pSave->setCommandText( L"save" );
	DmConnect( m_pSave->drawCustomBackground, *this, &quickRange::drawCustomIcon );
	DmConnect( m_pSave->mouseClick, this, &quickRange::toolClickQR );
	//buildButton( m_pLoad, 0x00001933, L"QuickScenario_save", L"");

	m_pExport = &m_pQuickRange->addWidget<DmPanel>( L"export", L"", X+dW-dH*2, Y+dH*8, X+dW, Y+dH*10, L"" );
	m_pExport->setValue( L"icon_export" );
	m_pExport->setCommandText( L"export" );
	DmConnect( m_pExport->drawCustomBackground, *this, &quickRange::drawCustomIcon );
	DmConnect( m_pExport->mouseClick, this, &quickRange::toolClickQR );
	//buildButton( m_pLoad, 0x00001933, L"QuickScenario_export", L"");


	// Start button - link back to main ctrl -------------------------------------------------------------------------------------------------
	DmPanel *pStart = &m_pQuickRange->addWidget<DmPanel>( L"start", L"START",  X, Y+dH*20, X+dW, Y+dH*23, L"metro_cyan" );
	pStart->setValue( DmString( 0xffCA7E0E ) );
	pStart->setCommandText( L"QuickScenario_start" );
	DmConnect( pStart->drawCustomBackground, *this, &quickRange::drawCustomColour );
	DmPanel *pO = &pStart->addWidget<DmPanel>( L"k", L"", L"metro_border" );
	pO->setCommandText( L"QuickScenario_start" );
	DmConnect( pO->mouseClick, pCTRL, &ctrl_DirectWeapons::toolClick );

	float w = rectMenu.width();
	float h = rectMenu.height();
	float ico = block * 0.5;
	DmPanel *pP = &m_pQuickRange->addWidget<DmPanel>( L"menuico", L"", w-ico*2, h-ico*2, w-ico, h-ico, L"" );
	pP->setValue( L"icon_menu" );			
	DmConnect( pP->drawCustomBackground, pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
	pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
	pP->setCommandText( L"menu" );
	DmConnect( pP->mouseClick, pCTRL, &ctrl_DirectWeapons::toolClick );

	X = (block + space) / 2 + dW + dH;
	int B = dH*3;
	int modePosition;
	for (int i=0; i<6; i++)
	{
		X = (block + space) / 2 + dW + B;

		m_Exercises[i].pEx = &m_pQuickRange->addWidget<DmPanel>( DmString(i) + L"ex", DmString(i+1), X, Y, X+B*7.5, Y+B, L"Edit_30" );
		m_Exercises[i].pEx->setHorizontalTextAlignment( dmAlignLeft );
		m_Exercises[i].pEx->setHorizontalTextMargin( 10 );
		m_Exercises[i].pEx->setVerticalTextAlignment( dmAlignMiddle );
		DmPanel *pO = &m_Exercises[i].pEx->addWidget<DmPanel>( L"k", L"", L"metro_border" ); 	
		X = B/2;

		m_Exercises[i].pTarget = &m_Exercises[i].pEx->addWidget<DmPanel>( L"tgt", L"", X, 0, X+B, B, L"empty_18" );
		m_Exercises[i].pTargetImg = &m_Exercises[i].pTarget->addWidget<DmPanel>( L"tgtImg", L"", L"empty_18" );
		m_Exercises[i].pTargetImg->setSize(B*0.8, B*0.8);
		m_Exercises[i].pTargetImg->setPosition( B*0.1, B*0.1 );
		DmConnect( m_Exercises[i].pTargetImg->drawCustomBackground, *this, &quickRange::drawCustomIcon ); 
		buildButton( m_Exercises[i].pTarget, 0x00001933, L"target", DmString(i));
		X+=B;

		m_Exercises[i].pRounds =	&m_Exercises[i].pEx->addWidget<DmPanel>( L"rnd", L" rounds", X+50, 0, X+130, B, L"Edit_30" );
		m_Exercises[i].pRounds->setHorizontalTextAlignment( dmAlignLeft );
		m_Exercises[i].pRoundsText = &m_Exercises[i].pEx->addWidget<DmNumeric>( L"rnds", L"5", X, 0, X+100, B, L"metro_title" );
		m_Exercises[i].pRoundsText->setHorizontalTextAlignment( dmAlignRight );
		m_Exercises[i].pRoundsText->setHorizontalTextMargin( 80 );
		m_Exercises[i].pRoundsText->setMinVal(1);
		m_Exercises[i].pRoundsText->setMaxVal(50);
		m_Exercises[i].pRoundsText->setPrecision( 0 );
		X+=130;

		m_Exercises[i].pDistance =	&m_Exercises[i].pEx->addWidget<DmPanel>( DmString(i) + L"dst", L" m", X+70, 0, X+100, B, L"Edit_30" );	
		m_Exercises[i].pDistance->setHorizontalTextAlignment( dmAlignLeft );
		m_Exercises[i].pDistanceText =		&m_Exercises[i].pEx->addWidget<DmNumeric>( L"dst", L"100", X, 0, X+100, B, L"metro_title" );
		m_Exercises[i].pDistanceText->setHorizontalTextAlignment( dmAlignRight );
		m_Exercises[i].pDistanceText->setHorizontalTextMargin( 30 );
		m_Exercises[i].pDistanceText->setMinVal(1);
		m_Exercises[i].pDistanceText->setMaxVal(300);
		m_Exercises[i].pDistanceText->setPrecision( 0 );
		
		//buildButton( m_Exercises[i].pDistance, 0x00001933, L"distance", DmString(i));
		X+=100;

		m_Exercises[i].pMode =		&m_Exercises[i].pEx->addWidget<DmPanel>( DmString(i) + L"md", L"", X, 0, X+100, B,  L"empty_18" );	
		m_Exercises[i].pModeText =		&m_Exercises[i].pEx->addWidget<DmPanel>( L"md", L"Static", X, 0, X+100, B, L"Edit_30" );
		m_Exercises[i].pModeText->setHorizontalTextAlignment( dmAlignRight );
		//buildButton( m_Exercises[i].pMode, 0x00001933, L"mode", DmString(i));
		m_Exercises[i].pModeText->setCommandText( L"mode" ); 	
		m_Exercises[i].pModeText->setHelpText( DmString(i) );
		DmConnect( m_Exercises[i].pModeText->mouseClick, this, &quickRange::toolClickQR );
		modePosition = (block + space) / 2 + dW + dH    + X  - 100;
	
		X+=100;

		m_Exercises[i].pRepeats =	&m_Exercises[i].pEx->addWidget<DmPanel>( L"rpt", L" x", X+30, 0, X+60, B, L"Edit_30" );	
		m_Exercises[i].pRepeats->setHorizontalTextAlignment( dmAlignLeft );
		m_Exercises[i].pRepeatText =		&m_Exercises[i].pEx->addWidget<DmNumeric>( L"rpttxt", L"100", X, 0, X+60, B, L"metro_title" );
		m_Exercises[i].pRepeatText->setHorizontalTextAlignment( dmAlignRight );
		m_Exercises[i].pRepeatText->setHorizontalTextMargin( 30 );
		m_Exercises[i].pRepeatText->setMinVal(1);
		m_Exercises[i].pRepeatText->setMaxVal(10);
		m_Exercises[i].pRepeatText->setPrecision( 0 );
		
	
		Y += dH *3;
	}

	
	m_RepeatX = (block + space) / 2 + dW + B;
	m_RepeatY = 90 + dH;
	m_RepeatScale = dH * 3;
	m_pPlus = &m_pQuickRange->addWidget<DmPanel>( L"plus", L"", m_RepeatX, m_RepeatY, m_RepeatX+dW, m_RepeatY+dW, L"" );
	m_pPlus->setValue( L"icon_plus" );
	m_pPlus->setCommandText( L"plus" );
	DmConnect( m_pPlus->drawCustomBackground, *this, &quickRange::drawCustomIcon );
	DmConnect( m_pPlus->mouseClick, this, &quickRange::toolClickQR );

	m_pMinus = &m_pQuickRange->addWidget<DmPanel>( L"minus", L"", m_RepeatX+dW, m_RepeatY, m_RepeatX+dW*2, m_RepeatY+dW, L"" );
	m_pMinus->setValue( L"icon_minus" );
	m_pMinus->setCommandText( L"minus" );
	DmConnect( m_pMinus->drawCustomBackground, *this, &quickRange::drawCustomIcon );
	DmConnect( m_pMinus->mouseClick, this, &quickRange::toolClickQR );

	m_pPlus->setSize( dH*2, dH*2 );
	m_pPlus->setPosition( m_RepeatX, m_RepeatY + m_RepeatScale * 6);
	m_pMinus->setSize( dH*2, dH*2 );
	m_pMinus->setPosition( m_RepeatX + dH*2, m_RepeatY + m_RepeatScale * 6);
	


	// Popup Targets ------------------------------------------------------------------------------------------------------------------
	pTM = pCTRL->getTargetManager();
	int numTargets = pTM->getNumTargets();
	int cnt = 0;
	int nH = 0, nW=0;
	float aspect = rectMenu.width() / (rectMenu.height()-120);
	while (cnt<numTargets )
	{
		nH ++;
		nW = (int) floor( nH * aspect );
		cnt = nH * nW;
	}
	//pCTRL->m_pLogger->Log( LOG_INFO, L"Target chooser: (%d, %d) cnt(%d) tgts(%d)", nW, nH, cnt, numTargets);
	
	int W = (rectMenu.width()) / nW;
	int H = W + 20; 
	int tX=0;
	int tY=0;
	//pCTRL->m_pLogger->Log( LOG_INFO, L"popupTargets: (%d, %d) (%d, %d) ", W, H, W*nW, H*nH);
	m_pPopupTargets = &m_pQuickRange->addWidget<DmPanel>( L"popupTargets", L"", L"metro_darkblue" );
	//m_pPopupTargets->setPosition( (block + space) / 2, 90 + dH );
	//m_pPopupTargets->setPosition( (block + space) / 2, 0  );
	m_pPopupTargets->hide();
	
	
	for (int i = 0; i < pTM->getNumTargets(); i++)
	{
		//ewstring name = pTM->getTargetName(i);
		//Target *pT = pTM->getTarget( name );
		Target *pT = pTM->getTarget( i );

		DmPanel *pP = &m_pPopupTargets->addWidget<DmPanel>( L"T" + DmString(i), pT->m_name.getWString(), tX, tY, tX+W, tY+H, L"empty_18" );
		pP->setVerticalTextAlignment(dmAlignBottom); 
		pP->setHorizontalTextAlignment( dmAlignCenter ); 
		buildButtonImg( pP, L"target_" + pT->getSpriteFile().getWString(), L"select_target", pT->getName().getWString(), W );

		tX += W;
		if (tX >= W*nW)
		{
			tX = 0;
			tY += H;
		}
	}

	// Popup Mode ------------------------------------------------------------------------------------------------------------------
	m_pPopupMode = &m_pQuickRange->addWidget<DmPanel>( L"popupMode", L"", L"metro_darkblue" );
	//m_pPopupMode->setPosition( (block + space) / 2, 90 + dH );
	m_pPopupMode->hide();

	DmPanel *pBG = & m_pPopupMode->addWidget<DmPanel>( L"popupMode", L"", L"metro_darkblue" );
	pBG->setPosition( modePosition + 100, 90 + dH  );
	pBG->setSize( 150, 300 );
	for (int i = 0; i < 9; i++)
	{
		DmPanel *pP = &pBG->addWidget<DmPanel>( DmString(i), L"Static", 0, 30*i, 150, 30*(i+1), L"Edit_30" );
		DmPanel *pO = &pP->addWidget<DmPanel>( L"x", L"", L"metro_border" );
		pO->setCommandText( L"select_mode" ); 	
		pO->setHelpText( DmString(i) );
		DmConnect( pO->mouseClick, this, &quickRange::toolClickQR );
		switch(i)
		{
		case 0: pP->setText( L"Static" );		pO->setHelpText(L"0");	pO->setValue(L"Static");		break;
		case 1: pP->setText( L"Snap" );			pO->setHelpText(L"1");	pO->setValue(L"Snap");			break;
		case 2: pP->setText( L"Rapid" );		pO->setHelpText(L"2");	pO->setValue(L"Rapid");			break;
		case 3: pP->setText( L"Moving l-r" );	pO->setHelpText(L"3");	pO->setValue(L"Moving l-r");	break;
		case 4: pP->setText( L"Moving r-l" );	pO->setHelpText(L"4");	pO->setValue(L"Moving r-l");	break;
		case 5: pP->setText( L"Charge" );		pO->setHelpText(L"5");	pO->setValue(L"Charge");		break;
		case 6: pP->setText( L"Zigzag" );		pO->setHelpText(L"6");	pO->setValue(L"Zigzag");		break;
		case 7: pP->setText( L"Attack" );		pO->setHelpText(L"7");	pO->setValue(L"Attack");		break;
		case 8: pP->setText( L"Random" );		pO->setHelpText(L"8");	pO->setValue(L"Random");		break;
		}
	}


	// Popup m_pPopupAreas ------------------------------------------------------------------------------------------------------------------
	m_pPopupAreas = &m_pQuickRange->addWidget<DmPanel>( L"popupAreas", L"", L"metro_darkblue" );
	m_pPopupAreas->hide();

	pBG = & m_pPopupAreas->addWidget<DmPanel>( L"popupAreas", L"", L"metro_darkblue" );
	pBG->setPosition( 100, 100  );
	pBG->setSize( 150, 300 );
	for (int i = 0; i < 4; i++)
	{
		DmPanel *pP = &pBG->addWidget<DmPanel>( DmString(i), L"Static", 0, 30*i, 150, 30*(i+1), L"Edit_30" );
		DmPanel *pO = &pP->addWidget<DmPanel>( L"x", L"", L"metro_border" );
		pO->setCommandText( L"select_area" ); 	
		pO->setHelpText( DmString(i) );
		DmConnect( pO->mouseClick, this, &quickRange::toolClickQR );
		switch(i)
		{
		case 0: pP->setText( L"Indoor" );			pO->setHelpText(L"0");		break;
		case 1: pP->setText( L"Outdoor - grass" );	pO->setHelpText(L"1");		break;
		case 2: pP->setText( L"Outdoor - dirt" );	pO->setHelpText(L"2");		break;
		case 3: pP->setText( L"Savannah" );			pO->setHelpText(L"3");		break;
		}
	}

/*
	// Popup Distance ------------------------------------------------------------------------------------------------------------------
	m_pPopupDistance = &m_pQuickRange->addWidget<DmPanel>( L"popupDistance", L"", 0, 0, B*2*5, B*6, L"metro_cyan" );
	m_pPopupDistance->setPosition( (block + space) / 2, 90 + dH );
	m_pPopupDistance->hide();
	for (int y = 0; y < 6; y++)
	{
		for (int x=0; x<5; x++)
		{
			float distance;
			if (y<2) distance = y*5 + x + 1;
			else if (y==2) distance = 12.5f + 2.5f*x;
			else if (y==3) distance = 30 + 5*x;
			else if (y==4) distance = 60 + 10*x;
			else if (y==5) distance = 125 + 25*x;
			DmPanel *pP = &m_pPopupDistance->addWidget<DmPanel>( DmString(y)+DmString(x), DmString(distance, 1) + L" m", B*2*x, B*y, B*2*(x+1), B*(y+1), L"metro_title" );
			DmPanel *pO = &pP->addWidget<DmPanel>( L"x", L"", L"metro_border" );
			pO->setCommandText( L"select_distance" ); 
			pO->setValue(DmString(distance, 1) + L" m");
			pO->setHelpText(DmString(distance, 1) + L" m");
			//pO->setHelpText( DmString(i) );
			DmConnect( pO->mouseClick, this, &quickRange::toolClickQR );
		}
	}

	// Popup Rounds ------------------------------------------------------------------------------------------------------------------
	m_pPopupRounds = &m_pQuickRange->addWidget<DmPanel>( L"popupRounds", L"", 0, 0, B*2*5, B*4, L"metro_cyan" );
	m_pPopupRounds->setPosition( (block + space) / 2, 90 + dH );
	m_pPopupRounds->hide();
	for (int y = 0; y < 4; y++)
	{
		for (int x=0; x<5; x++)
		{
			int R = y*5 + x + 1;
			DmPanel *pP = &m_pPopupRounds->addWidget<DmPanel>( DmString(y)+DmString(x), DmString(R), B*2*x, B*y, B*2*(x+1), B*(y+1), L"metro_title" );
			DmPanel *pO = &pP->addWidget<DmPanel>( L"x", L"", L"metro_border" );
			pO->setCommandText( L"select_rounds" ); 
			pO->setValue(DmString(R));
			pO->setHelpText(DmString(R));
			DmConnect( pO->mouseClick, this, &quickRange::toolClickQR );
		}
	}
	*/
}


void quickRange::setDefault( int mode )
{
	if (m_lastMode == mode) return;		// keep old values if we stick on a type
	m_lastMode = mode;

	switch( mode )
	{
		case QR_HANDGUN:
			m_SQLGroup = L"Handgun";
			setRange( L"indoor_01" );
			setSky( L"indoor" );
			setTarget( 0, L"ST1", QR_STATIC, 5, 5 );
			setTarget( 1, L"ST1", QR_STATIC, 7, 5 );
			setTarget( 2, L"ST1", QR_STATIC, 10, 5 );
			setTarget( 3, L"ST1", QR_STATIC, 10, 5 );
			setTarget( 4, L"ST1", QR_STATIC, 10, 5 );
			setTarget( 5, L"ST1", QR_STATIC, 10, 5 );
			m_NumExercises = 3;
			m_TEMP_shootingposition = eye_STAND;
			break;
		case QR_ASSAULT:
			m_SQLGroup = L"Assault Rifle";
			setRange( L"outdoor_grass" );
			setSky( L"outdoor_shoot" );
			setTarget( 0, L"Fig 11", QR_STATIC, 50, 5 );
			setTarget( 1, L"Fig 11", QR_STATIC, 100, 5 );
			setTarget( 2, L"Fig 11", QR_STATIC, 150, 5 );
			setTarget( 3, L"Fig 11", QR_STATIC, 150, 5 );
			setTarget( 4, L"Fig 11", QR_STATIC, 150, 5 );
			setTarget( 5, L"Fig 11", QR_STATIC, 150, 5 );
			m_NumExercises = 3;
			m_TEMP_shootingposition = eye_PRONE;
			break;
		case QR_HUNTING:
			m_SQLGroup = L"Hunting";
			setRange( L"" );
			setSky( L"outdoor_shoot" );
			setTarget( 0, L"Springbuck", QR_STATIC, 100, 3 );
			setTarget( 1, L"Wildebeest", QR_STATIC, 150, 3 );
			setTarget( 2, L"Hunting Cape Buffalo", QR_STATIC, 200, 3 );
			setTarget( 3, L"Hunting Cape Buffalo", QR_STATIC, 200, 3 );
			setTarget( 4, L"Hunting Cape Buffalo", QR_STATIC, 200, 3 );
			setTarget( 5, L"Hunting Cape Buffalo", QR_STATIC, 200, 3 );
			m_NumExercises = 3;
			m_TEMP_shootingposition = eye_PRONE;
			break;
	}

	updateDisplay();
}

void quickRange::setRange( DmString Range )
{
	m_pRange->setValue( L"area_" + Range );
	m_pRange->setHelpText( Range );
}

void quickRange::setSky( DmString Sky )
{
	m_pTime->setValue( L"sky_" + Sky );
	m_pTime->setHelpText( Sky );
}

void quickRange::setTarget( int ex, DmString TargetName, int mode, float distance, int rounds )
{
	ewstring name = TargetName;
	Target *pT = pTM->getTarget( name );
	m_Exercises[ex].pTarget->setHelpText( pT->getName().getWString() );
	m_Exercises[ex].pTargetImg->setValue( L"target_" + pT->getSpriteFile().getWString() );

	m_Exercises[ex].mode = mode;
	m_Exercises[ex].pModeText->setValue( DmString( mode ) );
	switch(mode)
	{
	case 0: m_Exercises[ex].pModeText->setText( L"Static" );		break;
	case 1: m_Exercises[ex].pModeText->setText( L"Snap" );			break;
	case 2: m_Exercises[ex].pModeText->setText( L"Rapid" );			break;
	case 3: m_Exercises[ex].pModeText->setText( L"Moving l-r" );	break;
	case 4: m_Exercises[ex].pModeText->setText( L"Moving r-l" );	break;
	case 5: m_Exercises[ex].pModeText->setText( L"Charge" );		break;
	}

	//m_Exercises[ex].distance = distance;
	m_Exercises[ex].pDistanceText->setText( DmString((int)distance)  );

	//m_Exercises[ex].rounds = rounds;
	m_Exercises[ex].pRoundsText->setText( DmString(rounds) );
}

void quickRange::setExercise( int ex, DmString target, DmString acion, int distance, int rounds )
{
}

void quickRange::start()
{
}


#define _onCLICK(x,y) if (sender.getCommandText() == x) { y; return; }
void quickRange::toolClickQR(DmPanel& sender, const DmVec2& cursorPosition, DmMouseButton button)
{
	
	_onCLICK( L"target",	 m_Ex = sender.getHelpText().toInt(); m_pPopupTargets->show();	);
	_onCLICK( L"select_target",	m_pPopupTargets->hide(); m_Exercises[m_Ex].pTargetImg->setValue( sender.getValue() ); m_Exercises[m_Ex].pTarget->setHelpText( sender.getHelpText() );	);
	_onCLICK( L"mode",	 m_Ex = sender.getHelpText().toInt(); 	m_pPopupMode->show();	);
	_onCLICK( L"select_mode",	m_pPopupMode->hide(); m_Exercises[m_Ex].mode = sender.getHelpText().toInt();	m_Exercises[m_Ex].pModeText->setText(sender.getValue()); );
	_onCLICK( L"area",	 m_pPopupAreas->show();	);
	_onCLICK( L"select_area",	m_pPopupAreas->hide();  );
	_onCLICK( L"distance",	 m_Ex = sender.getHelpText().toInt(); m_pPopupDistance->show();	);
	_onCLICK( L"rounds",	 m_Ex = sender.getHelpText().toInt(); m_pPopupRounds->show();	);
	_onCLICK( L"select_distance",	m_pPopupDistance->hide(); m_Exercises[m_Ex].pDistanceText->setText(sender.getValue()); );
	_onCLICK( L"select_rounds",		m_pPopupRounds->hide(); m_Exercises[m_Ex].pRoundsText->setText(sender.getValue()); );
	_onCLICK( L"lanes",	 m_bSingleLane = !m_bSingleLane;   if (m_bSingleLane) m_pNumLanes->setValue( L"QR_lanes_one" ); else m_pNumLanes->setValue( L"QR_lanes_multi" );	);
	_onCLICK( L"plus",	 m_NumExercises = __min( 6, m_NumExercises+1 );	updateDisplay(); );
	_onCLICK( L"minus",	 m_NumExercises = __max( 1, m_NumExercises-1 );	updateDisplay(); );
	_onCLICK( L"load",		load() );
	_onCLICK( L"save",		save() );
	_onCLICK( L"export",	lua_export() );
}

void quickRange::updateDisplay()
{
	for (int i=0; i<6; i++)
	{
		if (i<m_NumExercises)	m_Exercises[i].pEx->show();
		else					m_Exercises[i].pEx->hide();
	}

	m_pPlus->setPosition( m_RepeatX, m_RepeatY + m_RepeatScale * m_NumExercises);
	m_pMinus->setPosition( m_RepeatX + m_RepeatScale, m_RepeatY + m_RepeatScale * m_NumExercises);
}


void quickRange::load()
{
}


void quickRange::save()
{
	WCHAR path[MAX_PATH];
	WCHAR name[MAX_PATH];
	ewstring luaDIR = ((*ew_Scene::GetSingleton()->getConfigManager())[L"Earthworks_3"][L"INSTALL_DIR"]).getWString() + L"/quickRange";
	switch( m_lastMode )
	{
	case QR_HANDGUN: luaDIR = luaDIR + "/HANDGUN";			break;
	case QR_ASSAULT: luaDIR = luaDIR + "/ASSAULTRIFLE";	break;
	case QR_HUNTING: luaDIR = luaDIR + "/HUNTING";			break;
	}

	if (!DialogUtil::SaveDialog(path, MAX_PATH, L"Quick Range (*.xml)\0*.xml\0\0", L"xml", luaDIR.getWString().c_str()) )
	{
	}
	else
	{
		FILE *file = _wfopen( path, L"w");
		{
			fwprintf( file, L"dofile(tostring(GetCurrentPath() .. L\"commonFunctions.lua\"))\n\n" );

			fwprintf( file, L"create_modelScenario( L\"Quick Range\", L\"%s\", L\"\" )\n", getRange().c_str() );
			fwprintf( file, L"load_Cameras( L\"IndoorRange_cameras.xml\" )\n" );
			fwprintf( file, L"load_Sky( L\"%s\" )\n\n", getSky().c_str() );
			if ( getSingleLane() )	fwprintf( file, L"maxLanes( 1 )\n" );

			for (int i=0; i<m_NumExercises; i++)
			{
				switch( m_Exercises[i].mode )
				{
				case QR_STATIC:		fwprintf( file, L"standardExercise( L\"\", L\"\", 0, L\"%s\", %d, %2.2f )\n", getTarget(i).c_str(), getRouds(i), getDistance(i) );	break;
				case QR_SNAP:		fwprintf( file, L"snapExercise( L\"\", L\"\", 0, L\"%s\", %d, %d, %2.2f )\n", getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_RAPID:		fwprintf( file, L"snapExercise( L\"\", L\"\", 0, L\"%s\", %d, 1, %2.2f )\n", getTarget(i).c_str(), getRouds(i), getDistance(i) );	break;
				case QR_MOVINGLR:	fwprintf( file, L"movingExercise( L\"\", L\"\", 0, L\"%s\", MOVING_LR, 1.0, %d, %d, %2.2f )\n", getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_MOVINGRL:	fwprintf( file, L"movingExercise( L\"\", L\"\", 0, L\"%s\", MOVING_RL, 1.0, %d, %d, %2.2f )\n", getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_CHARGE:		fwprintf( file, L"movingExercise( L\"\", L\"\", 0, L\"%s\", MOVING_CHARGE, 7.0, %d, %d, %2.2f )\n", getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_ZIGZAG:		fwprintf( file, L"movingExercise( L\"\", L\"\", 0, L\"%s\", MOVING_ZIGZAG, 4.0, %d, %d, %2.2f )\n", getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_ATTACK:		fwprintf( file, L"movingExercise( L\"\", L\"\", 0, L\"%s\", MOVING_ATTACK, 3.0, %d, %d, %2.2f )\n", getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_RANDOM:		fwprintf( file, L"movingExercise( L\"\", L\"\", 0, L\"%s\", MOVING_RANDOM, 1.0, %d, %d, %2.2f )\n", getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				}
			}

			fwprintf( file, L"\n\nsuspend()\n" );
		
		}
		fclose ( file );
	}
}


void quickRange::lua_export()
{
	WCHAR path[MAX_PATH];
	WCHAR name[MAX_PATH];
	ewstring luaDIR = ((*ew_Scene::GetSingleton()->getConfigManager())[L"Earthworks_3"][L"INSTALL_DIR"]).getWString() + L"/user_Scenes";
	/*switch( m_lastMode )
	{
		swprintf( path, L"%s/HANDGUN/default.lua", luaDIR.getWString().c_str() );
	case QR_HANDGUN: luaDIR = luaDIR + "/HANDGUN";			break;
	case QR_ASSAULT: luaDIR = luaDIR + "/ASSAULTRIFLE";	break;
	case QR_HUNTING: luaDIR = luaDIR + "/HUNTING";			break;
	}*/
	switch( m_lastMode )
	{
	case QR_HANDGUN: swprintf( path, L"%s/HANDGUN/default.lua", luaDIR.getWString().c_str() );;			break;
	case QR_ASSAULT: swprintf( path, L"%s/ASSAULTRIFLE/default.lua", luaDIR.getWString().c_str() );	break;
	case QR_HUNTING: swprintf( path, L"%s/HUNTING/default.lua", luaDIR.getWString().c_str() );			break;
	}

	//ew_Scene::GetSingleton()->getLogger()->Log( LOG_INFO, L"lua_export( %s )", luaDIR.getWString().c_str() );

	if (!DialogUtil::SaveDialog(path, MAX_PATH, L"Lua exercise (*.lua)\0*.lua\0\0", L"lua", L"") )
	{
	}
	else
	{
		FILE *file = _wfopen( path, L"w");
		{
			fwprintf( file, L"-- Quick Range - Export -----------------------------------\n\n");
			fwprintf( file, L"dofile(tostring(GetCurrentPath() .. L\"commonFunctions.lua\"))\n\n" );

			fwprintf( file, L"create_modelScenario( L\"Quick Range\", L\"%s\", L\"\" )\n", getRange().c_str() );
			fwprintf( file, L"load_Cameras( L\"IndoorRange_cameras.xml\" )\n" );
			fwprintf( file, L"load_Sky( L\"%s\" )\n\n", getSky().c_str() );
			if ( getSingleLane() )	fwprintf( file, L"maxLanes( 1 )\n" );

			for (int i=0; i<m_NumExercises; i++)
			{
				switch( m_Exercises[i].mode )
				{
				case QR_STATIC:		fwprintf( file, L"standardExercise( L\"\", L\"\", %d, L\"%s\", %d, %2.2f )\n", getRouds(i)*5, getTarget(i).c_str(), getRouds(i), getDistance(i) );	break;
				case QR_SNAP:		fwprintf( file, L"snapExercise( L\"\", L\"\", %d, L\"%s\", %d, %d, %2.2f )\n", getRouds(i)*5, getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_RAPID:		fwprintf( file, L"snapExercise( L\"\", L\"\", %d, L\"%s\", %d, 1, %2.2f )\n", getRouds(i)*5, getTarget(i).c_str(), getRouds(i), getDistance(i) );	break;
				case QR_MOVINGLR:	fwprintf( file, L"movingExercise( L\"\", L\"\", %d, L\"%s\", MOVING_LR, 1.0, %d, %d, %2.2f )\n", getRouds(i)*5, getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_MOVINGRL:	fwprintf( file, L"movingExercise( L\"\", L\"\", %d, L\"%s\", MOVING_RL, 1.0, %d, %d, %2.2f )\n", getRouds(i)*5, getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_CHARGE:		fwprintf( file, L"movingExercise( L\"\", L\"\", %d, L\"%s\", MOVING_CHARGE, 7.0, %d, %d, %2.2f )\n", getRouds(i)*5, getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_ZIGZAG:		fwprintf( file, L"movingExercise( L\"\", L\"\", %d, L\"%s\", MOVING_ZIGZAG, 4.0, %d, %d, %2.2f )\n", getRouds(i)*5, getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_ATTACK:		fwprintf( file, L"movingExercise( L\"\", L\"\", %d, L\"%s\", MOVING_ATTACK, 3.0, %d, %d, %2.2f )\n", getRouds(i)*5, getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				case QR_RANDOM:		fwprintf( file, L"movingExercise( L\"\", L\"\", %d, L\"%s\", MOVING_RANDOM, 1.0, %d, %d, %2.2f )\n", getRouds(i)*5, getTarget(i).c_str(), getRouds(i), getRepeats(i), getDistance(i) );	break;
				}
			}


			fwprintf( file, L"\nscene( WAIT_LANES )\n");
			fwprintf( file, L"act_SavetoSQL()\n");

			fwprintf( file, L"\n\nsuspend()\n" );
		
		}
		fclose ( file );

		m_pCTRL->loadConfig( L"controllers/kolskoot.xml" );		// relaod the menu
	}


	
}


