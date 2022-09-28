
#include "laneGUI.h"
#include "Controllers/ctrl_DirectWeapons/ctrl_DirectWeapons.h"
#include "ew_Scene.h"
#include "RAKNET/WindowsIncludes.h"
#include "earthworks_Raknet.h"
#include "server_defines.h"



void guiSHOT::onFire( int num, DmString ammo,float time, float dT, float wind, DmString overlay, d_Vector pos, d_Vector dir, float fov, LPDIRECT3DTEXTURE9	pTex )
{
	m_shotNum = num;
	m_time = time;
	m_dT = dT;
	m_ammo = ammo;
	m_wind = wind;
	m_overlay = overlay;
	m_Position = pos;
	m_Dir = dir;
	m_bHIT = false;
	m_pTexture_FOV = fov;
	m_pTexture = pTex;
	m_score = 0;
	m_grouping = 0;
}

void guiSHOT::onHit( d_Vector hitPos, float tof, float tX, float tY, int score, int totalscore, int grp )
{
	m_bHIT = true;
	m_hitPos = hitPos;
	if (tof > 0)	m_tof = tof;					// this is so that gyeh target callbackl does not overrire TOF for cl;ay targets
	m_Distance = (hitPos - m_Position).length();
	m_ProjectedHit = m_Position+(m_Dir*m_Distance);
	d_Vector diff = hitPos - m_ProjectedHit;
	d_Vector U( 0, 1, 0 );
	d_Vector R = U ^ m_Dir;
	R.normalize();
	U = m_Dir ^ R;
	U.normalize();

	m_drop = diff * U;
	m_drift = diff * R;
	m_imageY = -atan2( m_drop, m_Distance ) * 1/m_pTexture_FOV;
	m_imageX = -atan2( m_drift, m_Distance )* 1/m_pTexture_FOV;
	m_targetX = tX;
	m_targetY = tY;
	m_score = score;
	m_totalscore = totalscore;
	m_grouping = grp;	//mm
}



// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void shotVIEW::buildGUI( DmPanel *pRoot )
{
	m_Isize = 512.0f;
	DmRect2D r = pRoot->getRectangle();
	DmPanel *pP = &pRoot->addWidget<DmPanel>( L"shotview", L"", L"empty_36" );
	&pP->addWidget<DmPanel>( L"x", L"", L"metro_darkblue_semi" );
	m_pIMG = &pP->addWidget<DmPanel>( L"image", L"", 0, 0, m_Isize, m_Isize );
	DmConnect( m_pIMG->drawCustomBackground, *this, &shotVIEW::drawCustomShot );
	for(int j=0; j<100; j++)
	{
		m_pHit[j] = &m_pIMG->addWidget<DmPanel>( L"X"+DmString(j), L"", 0, 0, m_Isize/10.0f, m_Isize/10.0f, L"bullet" );
		m_pHit[j]->hide();
	}
	m_pTexture = NULL;
	for (int y=0; y<10; y++ )
	{
		m_Info[0][y] = &pP->addWidget<DmPanel>( L"lbl"+DmString(y), L"", 0, 0, m_Isize, m_Isize, L"metro_info" );
		m_Info[0][y]->setHorizontalTextAlignment( dmAlignRight );
		m_Info[0][y]->setVerticalTextAlignment( dmAlignBottom );
		m_Info[1][y] = &pP->addWidget<DmPanel>( L"val"+DmString(y), L"", 0, 0, m_Isize, m_Isize, L"metro_title" );
		m_Info[1][y]->setHorizontalTextAlignment( dmAlignRight );
		m_Info[1][y]->setHorizontalTextMargin( 6 );
		m_Info[1][y]->setVerticalTextAlignment( dmAlignBottom );
		m_Info[2][y] = &pP->addWidget<DmPanel>( L"unt"+DmString(y), L"", 0, 0, m_Isize, m_Isize, L"metro_info" );
		m_Info[2][y]->setHorizontalTextAlignment( dmAlignLeft );
		m_Info[2][y]->setVerticalTextAlignment( dmAlignBottom );
		//m_Info[2][y]->setHorizontalTextMargin( 20 );
	}

	m_Errors = &pP->addWidget<DmPanel>( L"error_feedback", L"", 0, 0, m_Isize, m_Isize );
	DmConnect( m_Errors->drawCustomBackground, *this, &shotVIEW::drawCustomError );
	
}
void shotVIEW::layoutNarrow( float width, int numL, float oX, float oY )
{
	m_Isize = width;
	m_pIMG->setSize( width, width );
	m_pIMG->setPosition( oX, oY );
	for(int j=0; j<100; j++ ) m_pHit[j]->setSize( width/25, width/25 );
	float sx = 0;
	float sy = width + 5;
	if (numL == 1)
	{
//		sx = width;
//		sy = 0;
//		width = width/2;
	}
	for (int y=0; y<10; y++ )
	{
		m_Info[0][y]->setPosition(sx+oX, sy+y*40+oY);
		m_Info[0][y]->setSize(width/2, 40);
		m_Info[1][y]->setPosition(sx+width/2+oX, sy+y*40+oY+10);
		m_Info[1][y]->setSize(width/3, 40);
		m_Info[1][y]->setText( L"" );
		m_Info[2][y]->setPosition(sx+width*9/10+oX, sy+y*40+oY);
		m_Info[2][y]->setSize(width/6, 40);
	}

	m_Errors->setPosition( oX, sy+150+oY );
	m_Errors->setSize( width, width/2 );
}

void shotVIEW::layoutSpotting( float w, float h )
{
	m_Isize = h*0.9;
	m_pIMG->setSize( m_Isize, m_Isize );
	for(int j=0; j<100; j++ ) m_pHit[j]->setSize( m_Isize/10, m_Isize/10 );
	float sx = h;
	float sy = h*0.05;
	m_pIMG->setPosition(sy, sy);

	float W = __max( w-h, 120);
	float S = w - W;

	for (int y=0; y<10; y++ )
	{
		m_Info[0][y]->setPosition(h*0.95, sy+y*40);
		m_Info[0][y]->setSize(W*0.2, 40);
		m_Info[1][y]->setPosition(S+W*0.30, sy+y*40);
		m_Info[1][y]->setSize(W*0.5, 40);
		m_Info[1][y]->setText( L"" );
		m_Info[2][y]->setPosition(S+W*0.8, sy+y*40);
		m_Info[2][y]->setSize(W*0.2, 40);
		if (y>3)
		{
			m_Info[0][y]->setPosition(sx+m_Isize*0.35, 5000);
			m_Info[1][y]->setPosition(sx+m_Isize*0.35, 5000);
			m_Info[2][y]->setPosition(sx+m_Isize*0.35, 5000);

			m_Info[0][y]->hide();
			m_Info[1][y]->hide();
			m_Info[2][y]->hide();
		}
	}
}

void shotVIEW::layoutDebrief( float w, float h )
{
	m_Isize = h*0.9;
	m_pIMG->setSize( m_Isize, m_Isize );
	for(int j=0; j<100; j++ ) m_pHit[j]->setSize( m_Isize/10, m_Isize/10 );
	float sx = h;
	float sy = h*0.05;
	m_pIMG->setPosition(sy, sy);

	for (int y=0; y<10; y++ )
	{
		m_Info[0][y]->setPosition(sx, sy+y*40);
		m_Info[0][y]->setSize(m_Isize*0.3, 40);
		m_Info[1][y]->setPosition(sx+m_Isize*0.3, sy+y*40);
		m_Info[1][y]->setSize(m_Isize*0.5, 40);
		m_Info[1][y]->setText( L"" );
		m_Info[2][y]->setPosition(sx+m_Isize*0.8, sy+y*40);
		m_Info[2][y]->setSize(m_Isize*0.2, 40);
		if (y>3)
		{
			m_Info[0][y]->setPosition(-5000, 0);	// effective hide
			m_Info[1][y]->setPosition(-5000, 0);	// effective hide
			m_Info[2][y]->setPosition(-5000, 0);	// effective hide
		}
	}
}

void shotVIEW::analiseShots( vector<guiSHOT> *pS )
{
	if ( pS->size() == 0)
	{
		m_ShotsOnTarget = 0;
		m_ERROR = error_OK;
		return;
	}

	// Elipse ---------------------------------------------------------------------
	d_Vector xform[200];
	d_Matrix mat;
	int s = pS->size();
	m_Aspect = -1;
	
	for(int i=0; i<180; i++)
	{
		mat.identity();
		mat.concatRotZ( i );
		RECT aabb;
		m_ShotsOnTarget = 0;
		for (int y=0; y<s; y++)
		{
			xform[y] = d_Vector((*pS)[y].m_targetX * 1000, (*pS)[y].m_targetY*1000, 0, 0).transformPnt( mat );
			if ( (*pS)[y].m_bHIT )
			{
				m_ShotsOnTarget ++;
				aabb.left =		xform[y].x;
				aabb.right =	xform[y].x;
				aabb.bottom =	xform[y].y;
				aabb.top =		xform[y].y;
			}
		}
		
		
		for (int y=0; y<s; y++)
		{
			if ( (*pS)[y].m_bHIT )
			{
				aabb.left =		__min(aabb.left,	xform[y].x);
				aabb.right =	__max(aabb.right,	xform[y].x);
				aabb.bottom =	__min(aabb.bottom,	xform[y].y);
				aabb.top =		__max(aabb.top,		xform[y].y);
			}
			//m_AABB = aabb;
		}
		float a = 0;
		if ( aabb.top > aabb.bottom )		a = (aabb.right - aabb.left) / (aabb.top - aabb.bottom);
		if (a > m_Aspect)
		{
			m_Aspect = a;
			m_Angle = -((float)i/57.29577951308f);
			m_AABB = aabb;
			d_Vector C = d_Vector( (aabb.left+aabb.right)/2, (aabb.bottom + aabb.top)/2, 0, 0 );
			mat.identity();
			mat.concatRotZ( -i );
			m_AABB_center = C.transformPnt( mat );
			//m_Centerpos = DmVec2(m_AABB_center.x, m_AABB_center.y);
			//m_WH = DmVec2( (m_AABB.right - m_AABB.left)/3 + 20,  (m_AABB.top - m_AABB.bottom) / 3 + 20 );
		}
	}

	m_ERROR = error_OK;

	// Rifle errors ---------------------------------------------------------------
	if ( m_Overlay == L"shot_overlay_rifle"  )
	{
		if (m_ShotsOnTarget >=3)
		{
			if ( (m_AABB.top - m_AABB.bottom) > 150 )		// Small axis is also bad
			{
				m_ERROR = error_Rifle_Poor;	
			}
			else if ( ((m_AABB.right - m_AABB.left) > 100) && (m_Aspect > 3) )						// Oblong
			{
				int A = - m_Angle * 57.29577951308;
				if ( (A < 20) || (A > 160))		m_ERROR = error_Rifle_Horizontal;
				else if ( (A>70) && (A<110) )	m_ERROR = error_Rifle_Vertical;
				else							m_ERROR = error_Rifle_Split;	
			}
			else if ( m_AABB_center.length() > __max(200, m_AABB.top - m_AABB.bottom) )		// Small and round, check position
			{
				m_ERROR = error_Rifle_Position;
			}
		}

	}

	// Pistol errors --------------------------------------------------------------
	if ( m_Overlay.find( L"shot_overlay_pistol" )!=wstring::npos )
	{
		if ( m_AABB_center.length() > 50 )
		{
			float A = atan2( m_AABB_center.y, m_AABB_center.x ) * 57.29577951308f;
			if (A<0) A += 360;

			if (A < 25)			m_ERROR = error_Pistol_Thumbing;
			else if (A < 66)	m_ERROR = error_Pistol_Healing;
			else if (A < 110)	m_ERROR = error_Pistol_Up;
			else if (A < 155)	m_ERROR = error_Pistol_Pushing;
			else if (A < 200)	m_ERROR = error_Pistol_Trigger;
			else if (A < 225)	m_ERROR = error_Pistol_Fingers;
			else if (A < 250)	m_ERROR = error_Pistol_Jerking;
			else if (A < 290)	m_ERROR = error_Pistol_Down;
			else if (A < 335)	m_ERROR = error_Pistol_Tightning;
			else				m_ERROR = error_Pistol_Thumbing;
		}
	}
}


void shotVIEW::displayShot( vector<guiSHOT> *pS, bool bLIVEFIRE, int numR )
{
	if (m_type == guitype_IMAGE)
	{
		if (pS->size() ==0) return;
		
		guiSHOT S = (*pS)[pS->size()-1];
		m_pTexture = S.m_pTexture;
		m_Overlay = S.m_overlay;

		m_Info[0][0]->setText( L"sht" );
		m_Info[0][1]->setText( L"scr" );
		m_Info[0][2]->setText( L"" );
		m_Info[0][3]->setText( L"" );
		m_Info[0][4]->setText( L"distance" );
		m_Info[0][5]->setText( L"time of flight" );
		m_Info[0][6]->setText( L"drop" );
		m_Info[0][7]->setText( L"MOA" );
		m_Info[0][8]->setText( L"drift" );
		m_Info[0][9]->setText( L"MOA" );

		m_Info[2][0]->setText( L"" );
		m_Info[2][1]->setText( L"" );
		m_Info[2][2]->setText( L"s" );
		m_Info[2][3]->setText( L"s" );
		m_Info[2][4]->setText( L"m" );
		m_Info[2][5]->setText( L"ms" );
		m_Info[2][6]->setText( L"mm" );
		m_Info[2][7]->setText( L"" );
		m_Info[2][8]->setText( L"mm" );
		m_Info[2][9]->setText( L"" );

		m_Info[1][0]->setText( DmString( S.m_shotNum ) );		// #
		m_Info[1][2]->setText( DmString( S.m_time, 1 ) );	// time
		m_Info[1][3]->setText( DmString( S.m_dT, 1 ) );		// dtime

		m_Info[1][1]->setText( L"" );		// score
		m_Info[1][4]->setText( L"" );		// distance
		m_Info[1][5]->setText( L"" );		// tof
		m_Info[1][6]->setText( L"" );		// drop
		m_Info[1][6]->setText( L"" );		// drop
		m_Info[1][7]->setText( L"" );		// drop
		m_Info[1][8]->setText( L"" );		// drop
		m_Info[1][9]->setText( L"" );		// drop
	
		if ( S.m_bHIT ) 
		{
			m_Info[1][1]->setText( DmString((int)S.m_score) );		// score
			m_Info[1][4]->setText( DmString( S.m_Distance, 0)  );	// distance
			m_Info[1][5]->setText( DmString((int)(S.m_tof*1000)) );		// tof
			if(abs(S.m_drop)>0.005)	m_Info[1][6]->setText( DmString((int)(abs(S.m_drop)*1000))  );		// drop
			else					m_Info[1][6]->setText( L"-" );
			if(S.m_drop>0.005)	m_Info[0][6]->setText( L"rise" );
			else				m_Info[0][6]->setText( L"drop" );
			m_Info[1][7]->setText( DmString( atan2(S.m_drop, S.m_Distance)/0.000290888, 1 )  );		// MOA DROP
			if(abs(S.m_drift)>0.005)	m_Info[1][8]->setText( DmString((int)(abs(S.m_drift)*1000)) );		// drift
			m_Info[1][9]->setText( DmString( atan2(S.m_drift, S.m_Distance)/0.000290888, 1 )  );		// MOA drift


			m_pHit[0]->show();
			m_pHit[0]->setPosition( (S.m_imageX + 0.5f - 0.05f)*m_Isize, (S.m_imageY + 0.5f - 0.05f)*m_Isize );
		}
		else m_pHit[0]->hide();
		
	}


	if (m_type == guitype_TARGET)
	{
		if (pS->size() > 0)
		{
			guiSHOT S = (*pS)[pS->size()-1];
			m_Overlay = S.m_overlay;
			analiseShots( pS );
		}
		m_Centerpos = DmVec2(m_AABB_center.x*m_TargetScale/2*m_Isize/1000, -m_AABB_center.y*m_TargetScale/2*m_Isize/1000);
		m_WH = DmVec2( (m_AABB.right - m_AABB.left)*m_TargetScale*m_Isize/2000 +m_Isize/10,  (m_AABB.top - m_AABB.bottom) *m_TargetScale*m_Isize/ 2000  +m_Isize/10);

		m_Info[0][0]->setText( L"shots on target" );
		m_Info[0][1]->setText( L"score" );
		m_Info[0][2]->setText( L"grouping" );
		m_Info[0][3]->setText( L"" );
		m_Info[0][4]->setText( L"" );
		m_Info[0][5]->setText( L"" );
		m_Info[0][6]->setText( L"" );
		m_Info[0][7]->setText( L"" );
		m_Info[0][8]->setText( L"" );
		m_Info[0][9]->setText( L"" );

		m_Info[2][0]->setText( L"" );
		m_Info[2][1]->setText( L"" );
		m_Info[2][2]->setText( L"cm" );
		m_Info[2][3]->setText( L"" );
		m_Info[2][4]->setText( L"" );
		m_Info[2][5]->setText( L"" );
		m_Info[2][6]->setText( L"" );
		m_Info[2][7]->setText( L"" );
		m_Info[2][8]->setText( L"" );
		m_Info[2][9]->setText( L"" );

		int score = 0;
		float grp = 0.0f;
		int numS = 0;
		for (int s=0; s<__min(100, pS->size()); s++)
		{
			guiSHOT S = (*pS)[s];
			//score += S.m_score;
			if (S.m_bHIT) score = S.m_totalscore;
			grp = __max(grp, S.m_grouping);

			if (S.m_bHIT)
			{
				m_pHit[s]->show();
				m_pHit[s]->setPosition( ((S.m_targetX)*m_TargetScale/2 + 0.5f - 0.02f)*m_Isize, (0.5f-(S.m_targetY)*m_TargetScale/2 - 0.02f)*m_Isize );
			}
			numS++;
		}

		// Live fire normalise scores  ----------------------------------------------------------
		int newScore = score;
		int R = pS->size();
		int newShotsOnTarget = m_ShotsOnTarget;
		int newR = R;
		if (bLIVEFIRE && (R > 0))
		{
			int A = numR / 2;
			int B = numR * 2;
			if ( (R > A) && (R < B) )
			{
				newScore = (int)(( score * numR / (float)R ));
				newR = numR;
				newShotsOnTarget = m_ShotsOnTarget * numR / R;
				//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print...SHOT CORRECTTION (%d, %d) score (%d, %d)", ex.m_shotsFired, ex.m_numRounds, ex.m_targets[k]->m_score, newScore );
			}
		}

		m_Info[1][0]->setText( DmString( newShotsOnTarget ) + L" / " + DmString( newR ) );
		m_Info[1][1]->setText( DmString( newScore ) );
		m_Info[1][2]->setText( DmString( grp * 0.1f, 1 ) );


		switch (m_ERROR)
		{
		case error_OK: break;
		case error_Rifle_Poor: break;
		case error_Rifle_Horizontal: break;
		case error_Rifle_Vertical: break;
		case error_Rifle_Split: break;
		case error_Rifle_Position: break;
		case error_Pistol_Up: break;
		case error_Pistol_Healing: break;
		case error_Pistol_Thumbing: break;
		case error_Pistol_Tightning: break;
		case error_Pistol_Down: break;
		case error_Pistol_Jerking: break;
		case error_Pistol_Fingers: break;
		case error_Pistol_Trigger: break;
		case error_Pistol_Pushing: break;
		}
	}
}

void shotVIEW::displayDebrief( guiSHOT *pS )
{
		m_pTexture = pS->m_pTexture;
		m_Overlay = pS->m_overlay;

		m_Info[0][0]->setText( L"shot" );
		m_Info[0][1]->setText( L"score" );
		m_Info[0][2]->setText( L"time" );
		m_Info[0][3]->setText( L"" );
		m_Info[0][4]->setText( L"distance" );
		m_Info[0][5]->setText( L"time of flight" );
		m_Info[0][6]->setText( L"drop" );
		m_Info[0][7]->setText( L"MOA" );
		m_Info[0][8]->setText( L"drift" );
		m_Info[0][9]->setText( L"MOA" );

		m_Info[2][0]->setText( L"" );
		m_Info[2][1]->setText( L"" );
		m_Info[2][2]->setText( L"sec" );
		m_Info[2][3]->setText( L"sec" );
		m_Info[2][4]->setText( L"m" );
		m_Info[2][5]->setText( L"ms" );
		m_Info[2][6]->setText( L"mm" );
		m_Info[2][7]->setText( L"" );
		m_Info[2][8]->setText( L"mm" );
		m_Info[2][9]->setText( L"" );

		m_Info[1][0]->setText( DmString( pS->m_shotNum ) );		// #
		m_Info[1][2]->setText( DmString( pS->m_time, 1 ) );	// time
		m_Info[1][3]->setText( DmString( pS->m_dT, 1 ) );		// dtime

		m_Info[1][1]->setText( L"" );		// score
		m_Info[1][4]->setText( L"" );		// distance
		m_Info[1][5]->setText( L"" );		// tof
		m_Info[1][6]->setText( L"" );		// drop
		m_Info[1][6]->setText( L"" );		// drop
		m_Info[1][7]->setText( L"" );		// drop
		m_Info[1][8]->setText( L"" );		// drop
		m_Info[1][9]->setText( L"" );		// drop
	
		if ( pS->m_bHIT ) 
		{
			m_Info[1][1]->setText( DmString((int)pS->m_score) );		// score
			m_Info[1][4]->setText( DmString( pS->m_Distance, 0)  );	// distance
			m_Info[1][5]->setText( DmString((int)(pS->m_tof*1000)) );		// tof
			if(pS->m_drop > 0)	m_Info[0][6]->setText( L"rise" );
			else				m_Info[0][6]->setText( L"drop" );
			if(abs(pS->m_drop)>0.005)
			{
				m_Info[1][6]->setText( DmString((int)(abs(pS->m_drop)*1000))  );		// drop
				m_Info[1][7]->setText( DmString( atan2(pS->m_drop, pS->m_Distance)/0.000290888, 1 )  );		// MOA DROP
			}
			else
			{
				m_Info[1][6]->setText( L"-" );
				m_Info[1][7]->setText( L"-" );
			}
			
			if(abs(pS->m_drift)>0.005)
			{
				m_Info[1][8]->setText( DmString((int)(abs(pS->m_drift)*1000)) );		// drift
				m_Info[1][9]->setText( DmString( atan2(pS->m_drift, pS->m_Distance)/0.000290888, 1 )  );		// MOA drift
			}


			m_pHit[0]->show();
			m_pHit[0]->setPosition( (pS->m_imageX + 0.5f - 0.05f)*m_Isize, (pS->m_imageY + 0.5f - 0.05f)*m_Isize );
		}
		else m_pHit[0]->hide();

}

void shotVIEW::clear( )
{
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"e_1"  );
	m_type = guitype_IMAGE;
	m_TargetImage = L"";
	m_pTexture = NULL;
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"e_2"  );
	//for(int j=0; j<100; j++ ) m_pHit[j]->hide();
	for(int j=0; j<100; j++ ) m_pHit[j]->setPosition( -1000, 0 );
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"e_3"  );
	for (int y=0; y<10; y++ )
	{
		m_Info[1][y]->setText( L"" );
	}
	
	m_ShotsOnTarget = 0;
	m_ERROR = error_OK;

	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"e_4  .. clear"  );
}


void shotVIEW::setTarget ( DmString target, float scale)
{
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"d_1"  );
	//clear( );
	m_TargetImage = target;
	m_TargetScale = scale;
	m_type = guitype_TARGET;
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"d_2"  );
}


void shotVIEW::drawCustomShot(DmPanel& sender, const DmRect2D& rect )
{
	ew_Scene *pScene = ew_Scene::GetSingleton();
	DmSpriteManager& pSM = *pScene->getSpriteManager();
	DmRect2D R = rect;

	if( pSM.containsSprite( m_TargetImage ) )
	{
		DmSprite& spr2 = pSM[ m_TargetImage ];
		spr2.drawBestFit( R );

		if (m_ShotsOnTarget > 0)
		{
			DmSprite& sprE = pSM[ L"Elipse" ];
			sprE.draw( DmVec2(0,0), m_Angle, DmVec2( rect.midX() + m_Centerpos.x, rect.midY()+m_Centerpos.y), m_WH );
		}
	}
	else if (m_pTexture)
	{
		DmSprite& spr = pSM[L"tool_target"];
		spr.overrideColourMap( m_pTexture );
		spr.draw( R );

		if( pSM.containsSprite( m_Overlay ) )
		{
			DmSprite& spr2 = pSM[ m_Overlay ];
			spr2.draw( R );
		}
	}


}



void shotVIEW::drawCustomError(DmPanel& sender, const DmRect2D& rect )
{
	ew_Scene *pScene = ew_Scene::GetSingleton();
	DmSpriteManager& pSM = *pScene->getSpriteManager();
	DmRect2D R = rect;
	DmString e_Img = "";
	switch (m_ERROR)
	{
	case error_OK: return;

	case error_Rifle_Poor:			e_Img = L"e_Poor";		break;
	case error_Rifle_Horizontal:	e_Img = L"e_Horizontal";	break;
	case error_Rifle_Vertical:		e_Img = L"e_Vertical";	break;
	case error_Rifle_Split:			e_Img = L"e_Split";		break;
	case error_Rifle_Position:		e_Img = L"e_Sights";		break;

	case error_Pistol_Up:			e_Img = L"E_1";	break;
	case error_Pistol_Healing:		e_Img = L"E_2";	break;
	case error_Pistol_Thumbing:		e_Img = L"E_3";	break;
	case error_Pistol_Tightning:	e_Img = L"E_4";	break;
	case error_Pistol_Down:			e_Img = L"E_5";	break;
	case error_Pistol_Jerking:		e_Img = L"E_6";	break;
	case error_Pistol_Fingers:		e_Img = L"E_7";	break;
	case error_Pistol_Trigger:		e_Img = L"E_8";	break;
	case error_Pistol_Pushing:		e_Img = L"E_9";	break;
	}

	if( pSM.containsSprite( e_Img ) )
	{
		DmSprite& spr2 = pSM[ e_Img ];
		spr2.drawBestFit( R );
	}
}





void guiLANE::replayUpdate( float dTime )
{

	if ( m_Shots.size() == 0 )
	{
		return;
	}

	static double shotTime = 0;

	if ((shotTime>0) && ((shotTime-dTime)<=0))
	{
		ew_Scene::GetSingleton()->videoPlay();
	}

	if ((shotTime>-0.5) && ((shotTime-dTime)<=-0.5))
	{
		m_SV_Spotting.clear();
		m_SV_Inst.clear();
	}

	shotTime -= dTime;
	double vidTime = ew_Scene::GetSingleton()->getVidTime();
	
	if ( (shotTime < 0.0) && (m_CurrentShot < m_Shots.size()) && (m_Shots[m_CurrentShot].m_time  < vidTime ))	
	{
		shotTime = 4.0;
		ew_Scene::GetSingleton()->videoPause();
		m_SV_Spotting.displayDebrief( &m_Shots[m_CurrentShot] );
		m_SV_Inst.displayDebrief( &m_Shots[m_CurrentShot] );
		SoundSource *m_pSoundSource = SoundManager::GetSingleton()->createSoundSourceFromFile( L"Beretta_shot.wav" );
					if (m_pSoundSource)			m_pSoundSource->playImmediate();
		m_CurrentShot ++;
	}


}


void guiLANE::update( float dTime )
{
	m_shotTimer += dTime;


	// Check and Do magazine Change -----------------------------------------------------------------------
		
	if (((m_numRounds - m_numRoundsFired)<=0) && (m_magazines>0))
	{
		if (m_reloadCounter == 0.0f)	
		{
			dw_scenario::s_pCTRL->gui_LoadImage( L"MAG.png" );
		}

		m_reloadCounter+=dTime;

		if (m_reloadCounter > m_reloadTime)
		{
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"guiLANE::update() loading %d rounds from %d magazines",  m_numMAGrounds, m_magazines  );
			dw_scenario::s_pCTRL->hideOverlay();
			m_magazines --;
			m_numRounds = m_numMAGrounds;
			m_reloadCounter = 0;
			m_numRoundsFired = 0;
		}
	}

}


int guiLANE::addShot()
{
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"guiLANE::addShot()  m_shotDelay( %d ms )", (int)(m_shotDelay * 1000 )  );
	if ( m_shotTimer>m_shotDelay )	// we pass the timing delay
	{
		if (m_numRounds > 0)
		{
			if ( m_numRoundsFired<m_numRounds ||  dw_scenario::s_pCTRL->isLiveFire() )		// If live fire we do not block, we allow more shots
			{
				m_shotTimer = 0.0f;
				m_numRoundsFired ++; 
				return (m_numRoundsFired-1);
			}
			else
			{
				//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"guiLANE::addShot() - BLOCKED - we have fired %d of our %d rounds", m_numRoundsFired,  m_numRounds );
			}
		}
		else
		{
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"guiLANE::addShot() - BLOCKED - no ammunition loaded"  );
		}
	}
	else
	{
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"guiLANE::addShot() - BLOCKED - 100ms havent passed since previos shot"  );
		return -2;	// -2 beteken dat dit die times is wat die skoot geblok het
	}

	return -1;  // negatief beteken die skoot tel nie
}





// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
lane_GUI::lane_GUI()
{
}

lane_GUI::~lane_GUI()
{
	for (int i=0; i<NUM_SHOT_TEX; i++ )	
	{
		SAFE_RELEASE( m_pSurface[i] );//->Release();
		SAFE_RELEASE( m_pTexture[i] );//->Release();
	}
}

void  lane_GUI::setLive(bool bLive)
{
	if (bLive)
	{
	}
	else
	{
	}
}

#define _onCLICK(x,y) if (sender.getCommandText() == x) { y; return; }
void lane_GUI::toolClick( DmPanel& sender, const DmVec2& cursorPosition, DmMouseButton button )
{
		
}


void lane_GUI::clear()
{
	for (unsigned int i=0; i<m_NumLanes; i++)
	{
		m_LANES[i].clear();
		m_pLive3D->findWidget( i )->findWidget( L"content" )->hide();
	}
	m_pVideoTime->setText( L"" );
}

void  lane_GUI::setAmmunition(int lane, float distance )
{
	if (m_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "lane_GUI::setAmmunition" );
		Bs.Write( ewstring(lane).c_str() );
		Bs.Write( ewstring(distance).c_str() );
		Bs.Write( "" );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}

	_laneInfo LI = m_pCTRL->getLane( lane );
	int D = (int)distance;


	m_pLive3D->findWidget( lane )->findWidget( L"dmm" )->setText( L"-" );
	m_pLive3D->findWidget( lane )->findWidget( L"dmoa" )->setText( L"-" );

	m_pDistanceOverlay->hide();
	m_DOdistance = 0;
	if (distance > 0)
	{
		//m_pLive3D->findWidget( lane )->findWidget( L"ammoInfo" )->show();

		float moa = atan2(LI.ammo.m_bulletDrop[ D ], distance)/0.000290888;
		m_pLive3D->findWidget( lane )->findWidget( L"dist" )->setText( (int)distance );
		m_pDistanceOverlay->show();
		{
			m_pDistanceOverlay->findWidget( L"links" )->setText( DmString((int)distance) + L" m" );
			m_pDistanceOverlay->findWidget( L"regs" )->setText( DmString((int)distance) + L" m" );
			m_DOdistance = 100 / distance;
			//m_pDistanceOverlay->setPosition( 0, (1.0 - m_pCTRL->m_eye_level) * m_rect_3D.height() + 40 + m_DOdistance * 20);
			
		}
		if (distance > 100)
		{
			if (LI.ammo.m_bulletDrop[ D ] > 0.01)
			{
				m_pLive3D->findWidget( lane )->findWidget( L"dmm" )->setText( DmString( fabs(100 * LI.ammo.m_bulletDrop[ D ]), 1) );
				m_pLive3D->findWidget( lane )->findWidget( L"dmoa" )->setText( DmString( fabs(moa), 1 ) );
				//if(LI.ammo.m_bulletDrop[ D ] > 0 )	m_pLive3D->findWidget( lane )->findWidget( L"drop" )->setText( L"rise" );
				//else								m_pLive3D->findWidget( lane )->findWidget( L"drop" )->setText( L"drop" );
			}
		}

	}
	else
	{
		m_pLive3D->findWidget( lane )->findWidget( L"dist" )->setText( L"-" );
	}
}

void  lane_GUI::setWeather(float wind, float drift )
{
	for (unsigned int i=0; i<m_NumLanes; i++)
	{
		m_LANES[i].clear();
		m_pLive3D->findWidget( i )->findWidget( L"content" )->hide();
	}
}

void lane_GUI::showIntro( unsigned int type, DmString title, int numLanes )
{
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"_1"  );
	m_pDistanceOverlay->hide();

	if (m_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "lane_GUI::showIntro" );
		Bs.Write( ewstring(numLanes).c_str() );
		Bs.Write( ewstring( title ).c_str() );
		Bs.Write( "" );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"_2"  );
	m_type = type;
	m_pIntro3D->show();
	m_pIntroMenu->show();
	m_pLoadWaitMenu->show();
	//m_pLoadWait3D->show();

	// populate names, ammo from _laneInfo  -----------------------------------------------------------------------------------------
	for (unsigned int l=0; l<m_NumLanes; l++)
	{
		_laneInfo LI = m_pCTRL->getLane( l );
		if (LI.userNAME == L"")
		{
			m_pIntroMenu->getWidget( l )->getWidget( L"name" )->setText( "..." );
			m_pIntro3D->getWidget( l )->getWidget( L"name" )->setText( "..." );
			m_pLiveMenu->getWidget( l )->findWidget( L"name" )->setText( "..." );
			m_pLive3D->getWidget( l )->findWidget( L"name" )->setText( "" );
		}
		else
		{
			m_pIntroMenu->getWidget( l )->getWidget( L"name" )->setText( LI.userNAME );
			m_pIntro3D->getWidget( l )->getWidget( L"name" )->setText( LI.userNAME );
			m_pLiveMenu->getWidget( l )->findWidget( L"name" )->setText( LI.userNAME );
			m_pLive3D->getWidget( l )->findWidget( L"name" )->setText( LI.userNAME );
		}
		m_pIntroMenu->getWidget( l )->getWidget( L"ammoMenu" )->setText( LI.ammo.name );
		m_pIntro3D->getWidget( l )->getWidget( L"ammoIntro" )->setText( LI.ammo.name );
		//m_pLiveMenu->getWidget( l )->findWidget( L"ammoLive" )->setText( LI.ammo.name );
		m_pLive3D->getWidget( l )->findWidget( L"ammo" )->setText( LI.ammo.name );
	}
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"_3"  );
	// populate title from scenarion ??? push or pull ------------------------------------------------------------------------------
	m_pIntroMenu->getWidget( L"title" )->setText( title );
	m_pIntro3D->getWidget( L"title" )->setText( title );
	m_pLiveMenu->getWidget( L"title" )->setText( title );
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"_4"  );
	m_NumActiveLanes = numLanes;
	showIntro_Lanes( );
	
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"_5"  );
	liveLayout();
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"_6"  );
}

void lane_GUI::showIntro_Lanes( )
{
	// disable some lanes if only 1 lane  ------------------------------------------------------------------------------------------
	if (m_NumActiveLanes == 1)
	{
		for (unsigned int l=0; l<m_NumLanes; l++)
		{
			m_pIntroMenu->getWidget( l )->show();
			m_pIntro3D->getWidget( l )->show();
			/*
			if (l!=m_ActiveLane)
			{
				m_pIntroMenu->getWidget( l )->hide();
				m_pIntro3D->getWidget( l )->hide();
			}
			else
			{
				m_pIntroMenu->getWidget( l )->show();
				m_pIntro3D->getWidget( l )->show();
			}
			*/
		}
	}
	else
	{
		for (unsigned int l=0; l<m_NumLanes; l++)
		{
			m_pIntroMenu->getWidget( l )->show();
			m_pIntro3D->getWidget( l )->show();
		}
	}

	for (unsigned int i=0; i<m_NumLanes; i++)
	{
		m_LANES[i].m_SV_Inst.clear();
		m_LANES[i].m_SV_Spotting.clear();
		m_LANES[i].m_SV_Popup.clear();
	}

	// Hier moet ons ook die lewendige lane spasiering doen -----------------------------------------------------------------------
}

void lane_GUI::showLaneSelect( )
{
	m_pCTRL->SEND( "showLaneSelect" );
	if (m_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "lane_GUI::showLaneSelect" );
		Bs.Write( "" );
		Bs.Write( "" );
		Bs.Write( "" );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}

	m_pLoadWaitMenu->hide();
	//m_pLoadWait3D->hide();
}

void lane_GUI::showLive( )
{
	m_pCTRL->SEND( "showLive" );
	if (m_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "lane_GUI::showLive" );
		Bs.Write( "" );
		Bs.Write( "" );
		Bs.Write( "" );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}

	for (int i=0; i<m_NumLanes; i++)
	{
		DmPanel *pLane = (DmPanel *)m_pLive3D->getWidget( i );
		if(pLane) 
		{
			( (DmPanel*)pLane->findWidget( L"ammoInfo" ) )->hide();
			( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->hide();
		}
	}

	clear();
	m_pIntroMenu->hide();
	m_pIntro3D->hide();

	m_pLive3D->show();
	m_pLiveMenu->show();
	if ((m_rect_Menu.topLeft.x == m_rect_3D.topLeft.x) && (!m_pCTRL->m_bInstructorStation)) m_pLiveMenu->hide();

	//m_pDistanceOverlay->setPosition( 0, (1.0 - m_pCTRL->m_eye_level) * m_rect_3D.height() + 40 + m_DOdistance * 20);
}

void lane_GUI::showFinal( )
{
	m_pCTRL->SEND( "showFinal" );
	if (m_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "lane_GUI::showFinal" );
		Bs.Write( "" );
		Bs.Write( "" );
		Bs.Write( "" );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}

	for (int i=0; i<m_NumLanes; i++)
	{
		DmPanel *pLane = (DmPanel *)m_pLiveMenu->getWidget( i );
		if(pLane) 
		{
			( (DmPanel*)pLane->findWidget( L"print" ) )->show();
		}
	}
}


// For instructional videos
void lane_GUI::showVideo( )
{
	m_pCTRL->SEND( "showVideo" );
	if (m_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "lane_GUI::showVideo" );
		Bs.Write( "" );
		Bs.Write( "" );
		Bs.Write( "" );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}

	m_pIntroMenu->hide();
	m_pIntro3D->hide();
	m_pLive3D->hide();
	m_pLiveMenu->hide();
}

void lane_GUI::showFeedback( unsigned int lane, bool bLive, int numR )
{ 
	m_pCTRL->SEND( "showFeedback" );
	if (m_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "lane_GUI::showFeedback" );
		Bs.Write( ewstring(lane).c_str() );
		Bs.Write( ewstring(numR).c_str() );
		Bs.Write( "" );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}

	m_pLive3D->findWidget( lane )->findWidget( L"spottingscope" )->hide();
	m_pLive3D->findWidget( lane )->findWidget( L"content" )->show();
	m_LANES[lane].m_SV_Popup.displayShot( &m_LANES[lane].m_Shots, bLive, numR );
	m_LANES[lane].m_SV_Inst.displayShot( &m_LANES[lane].m_Shots, bLive, numR );		// redo this to do score adjustmetn in live fire
}


void lane_GUI::showDebrief( )
{
}


void lane_GUI::loadRounds( int lane, int r, int m, float t ) 
{ 
	if (m_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "lane_GUI::loadRounds" );
		Bs.Write( ewstring(lane).c_str() );
		Bs.Write( ewstring(r).c_str() );
		Bs.Write( ewstring(m).c_str() );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}

	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"lane_GUI::loadRounds() lane %d, rounds %d, magazines %d",  lane, r, m  );
	m_LANES[lane].loadRounds(r, m, t);
}

void lane_GUI::movescope(int position)
{
	// FIXME ADD RAKNET
	if (m_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "lane_GUI::movescope" );
		Bs.Write( ewstring(position).c_str() );
		Bs.Write( "" );
		Bs.Write( "" );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}


	m_bScopePosition = position;
	m_pSpottingSelect->hide();
	float h = m_rect_3D.height();
	float wL = m_rect_3D.width() / m_NumActiveLanes;
	float b= h/24;
	float wVis = __min( wL * 0.95, b*10);
	float wSpace = (wL - wVis) / 2.0f;

	for (unsigned int i=0; i<m_NumLanes; i++)
	{
		DmPanel *pLane = (DmPanel *)m_pLive3D->getWidget( i );
		switch (position)
		{
		case spotting_HIDE:
			( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setPosition( wSpace, -2000 );
			m_pSpotting->setValue( L"icon_spotting_hide" );
			break;
		case spotting_TOP:
			( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setPosition( wSpace, h/24*3 );
			m_pSpotting->setValue( L"icon_spotting_topmiddle" );
			break;
		case spotting_TOPLEFT:
			( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setPosition( __min(wSpace, b), h/24*3 );
			m_pSpotting->setValue( L"icon_spotting_topleft" );
			break;
		case spotting_BOTTOM:
			( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setPosition( wSpace, h*9/12 );
			m_pSpotting->setValue( L"icon_spotting_bottommiddle" );
			break;
		case spotting_BOTTOMLEFT:
			( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setPosition( __min(wSpace, b), h*9/12 );
			m_pSpotting->setValue( L"icon_spotting_bottomleft" );
			break;
		}
		//if(btop)	( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setPosition( wSpace, h/24*3 );
		//else		( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setPosition( wSpace, h*9/12 );

		//if(btop)	( (DmPanel*)pLane->findWidget( L"content" ) )->setPosition( wSpace, h/24*9 );
		//else		( (DmPanel*)pLane->findWidget( L"content" ) )->setPosition( wSpace, h/24*3 );
		( (DmPanel*)pLane->findWidget( L"content" ) )->setPosition( wSpace, h/24*3 );		//????
	}
}

void lane_GUI::showRoundsCompleted( unsigned int lane )
{
}

 void lane_GUI::showTimeBonus( unsigned int lane, int bonusScore ) 
 { 
 }


 void lane_GUI::showRanking(unsigned int l, _user *pR[5], unsigned int rank )
 { /*
	 m_Lanes[l].pRanking->show();
	 for (int i=0; i<5; i++)
	 {
		 if (pR[i])
		 {
			m_Lanes[l].pR[i][1]->setText(DmString(rank-2+i));
			m_Lanes[l].pR[i][2]->setText( pR[i]->displayName );
			m_Lanes[l].pR[i][3]->setText( DmString(pR[i]->bestScore) );
		 }
		 else
		 {
			 m_Lanes[l].pR[i][1]->setText(L"");
			m_Lanes[l].pR[i][2]->setText( L"" );
			m_Lanes[l].pR[i][3]->setText( L"" );
		 }
	 }
	 */
 }


 


void lane_GUI::replayUpdate( float dTime )
{
	m_Time += dTime;

	

	for (int i=0; i<m_NumLanes; i++)
	{
		m_LANES[i].replayUpdate( dTime );
	}

	double vTime = ew_Scene::GetSingleton()->getVidTime();
	int m;
	float s;
	m = (int)floor(vTime / 60);
	s = (vTime - (m*60));

	if (s < 10)
	{
		m_pVideoTime->setText( DmString( m ) + L" : 0" + DmString(s, 0) );
	}
	else
	{
		m_pVideoTime->setText( DmString( m ) + L" : " + DmString(s, 0) );
	}
	
}


void lane_GUI::setTotalScore( int lane, int score )
{
	

	DmPanel *pLane = (DmPanel *)m_pLive3D->getWidget( lane );
	int oldscore = ( (DmPanel*)pLane->findWidget( L"totalScore" ) )->getText().toInt();

	if (oldscore != score)
	{
		( (DmPanel*)pLane->findWidget( L"totalScore" ) )->setText( DmString(score) );

		// FIXME ADD RAKNET
		if (m_pCTRL->m_bInstructorStation)
		{
			RakNet::BitStream Bs;
			Bs.Write( (unsigned short)RPC_TOOLCLICK );
			Bs.Write( "lane_GUI::setTotalScore" );
			Bs.Write( ewstring(lane).c_str() );
			Bs.Write( ewstring(score).c_str() );
			Bs.Write( "" );
			ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
		}
	}
}

void lane_GUI::update( float dTime )
{
	for (int i=0; i<m_NumLanes; i++)
	{
		m_LANES[i].update( dTime );
	}
}

int lane_GUI::getLanefromCursor( float x )
{
	float w = m_rect_3D.width();
	float sx = m_rect_3D.topLeft.x;
	float wL = w / m_NumActiveLanes;

	if (m_pCTRL->m_bLiveFire)
	{
		sx += w*dw_scenario::s_pCTRL->m_ScreenSideBuffer;
		w = w * (1.0 - 2.0*dw_scenario::s_pCTRL->m_ScreenSideBuffer);
		wL = w / m_NumActiveLanes;
	}

	if (m_NumActiveLanes == 1) return m_ActiveLane;
	else return( (int)(floor( (x-sx)/wL )) );

}



void lane_GUI::onMouseOver(DmPanel& sender)
{
	if ( m_pSpottingSelect->isHidden() ) m_pPositionSelect->show();
}

void lane_GUI::onMouseOverSpot(DmPanel& sender)
{
	if ( m_pPositionSelect->isHidden() )	m_pSpottingSelect->show();
}


void lane_GUI::onMouseOut(DmPanel& sender)
{
	sender.hide();
	
	/*
	int lane = sender.getCommandText().toInt();
	if ( m_Lanes[lane].m_currentMouseOverShot == sender.getValue().toInt() )	// this prevents out of order calls from hiding it
	{
		m_Lanes[lane].pScreenshotTarget->hide();
	}
	*/
}



void lane_GUI::buildGUI( DmPanel *pRoot, int numLanes, bool bSplitScreen, DmRect2D rect_Menu, DmRect2D rect_3D, float block, float space, ctrl_DirectWeapons *pCTRL )
{
	guiLANE GL;
	m_LANES.clear();
	for (unsigned int i=0l; i<numLanes; i++)	m_LANES.push_back( GL );

	m_rect_3D = rect_3D;
	m_rect_Menu = rect_Menu;
	m_NumLanes = numLanes;
	m_pCTRL = pCTRL;
	// Live  ---------------------------------------------------------------------------------------------------------------------------------------------------------
	m_pLive3D = &pRoot->addWidget<DmPanel>( L"l3D", L"", rect_3D.topLeft.x, rect_3D.topLeft.y, rect_3D.bottomRight.x, rect_3D.bottomRight.y );
	m_pLive3D->setSize(1, 1 );
	m_pLive3D->setClipChildren( false );
	m_pLive3D->hide();
	{
		m_pDistanceOverlay = &m_pLive3D->addWidget<DmPanel>( L"DistanceOverlay", L"" );
		m_pDistanceOverlay->setSize(1, 1 );
		m_pDistanceOverlay->setPosition( 0, this->m_rect_3D.height() - 80 );
		m_pDistanceOverlay->setClipChildren( false );
		{
			DmPanel *pL = &m_pDistanceOverlay->addWidget<DmPanel>( L"links", L"", L"DistanceNumber" );
			pL->setSize(140, 60);
			pL->setPosition( 20, 0 );
					pL = &m_pDistanceOverlay->addWidget<DmPanel>( L"regs", L"", L"DistanceNumber" );
			pL->setSize(140, 60);
			pL->setPosition( m_rect_3D.width() - 160, 0 );
		}

		m_pVideoTime = &m_pLive3D->addWidget<DmPanel>( L"vidTime", L"1:30:23", rect_3D.width() - 200, rect_3D.height()-80, rect_3D.width(), rect_3D.height()-40, L"Edit_60" );
		DmPanel *pP;
		for (int l=0; l<numLanes; l++)
		{
			DmPanel *pL = &m_pLive3D->addWidget<DmPanel>( l, L"" );
			pL->setSize( 1, 1 );
			pL->setClipChildren( false );
			{
				DmPanel *pH = &pL->addWidget<DmPanel>( L"header", L"", L"metro_darkblue" );
				pP = &pH->addWidget<DmPanel>( L"name", L"clear name", L"Edit_30_bold" );

				pP = &pH->addWidget<DmPanel>( L"number", DmString(1+l+m_pCTRL->m_LaneZero), L"metro_yellow_number" );
				pP = &pH->addWidget<DmPanel>( L"totalScore", L"0", L"Edit_48" );
				pP = &pL->addWidget<DmPanel>( L"spottingscope", L"", 0, 40, 100, 100, L"empty_36" );
				m_LANES[l].m_SV_Spotting.buildGUI( pP );
				DmPanel *pA = &pP->addWidget<DmPanel>( "ammoInfo", L"", L"metro_darkblue"  );
				{
					float P = DmWidgetLayoutSpecialValues::dmSubtractIfPercentage;
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "ammo", L"clear ammo", DmWidgetLayout( 0, 0, 100, 25, true), L"Edit_48") );
					pP->setHorizontalTextAlignment( dmAlignLeft );
					pP->setHorizontalTextMargin( 10 );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "dist", L"200", DmWidgetLayout( 0, 15, 30, 30, true), L"Edit_60") );
					pP->setHorizontalTextAlignment( dmAlignRight );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "m", L"m", DmWidgetLayout( 32, 25, 18, 20, true), L"Edit_30") );
					pP->setHorizontalTextAlignment( dmAlignLeft );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "drop", L"drop", DmWidgetLayout( 3, 45, 50, 10, true), L"metro_info") );
					pP->setHorizontalTextAlignment( dmAlignLeft );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "dmm", L"12", DmWidgetLayout( 0, 55, 30, 20, true), L"Edit_48") );
					pP->setHorizontalTextAlignment( dmAlignRight );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "mm", L"cm", DmWidgetLayout( 32, 55, 18, 20, true), L"metro_info") );
					pP->setHorizontalTextAlignment( dmAlignLeft );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "dmoa", L"2.5", DmWidgetLayout( 0, 75, 30, 20, true), L"Edit_48") );
					pP->setHorizontalTextAlignment( dmAlignRight );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "moa", L"MOA", DmWidgetLayout( 32, 75, 18, 20, true), L"metro_info") );
					pP->setHorizontalTextAlignment( dmAlignLeft );
					pP->setVerticalTextAlignment( dmAlignBottom );
					/*
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "wind", L"-", DmWidgetLayout( 50, 25, 20, 20, true), L"Edit_48") );
					pP->setHorizontalTextAlignment( dmAlignRight );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "kmh", L"km/h", DmWidgetLayout( 72, 25, 18, 20, true), L"metro_info") );
					pP->setHorizontalTextAlignment( dmAlignLeft );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "drift", L"drift", DmWidgetLayout( 53, 45, 50, 10, true), L"metro_info") );
					pP->setHorizontalTextAlignment( dmAlignLeft );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "drmm", L"-", DmWidgetLayout( 50, 55, 20, 20, true), L"Edit_48") );
					pP->setHorizontalTextAlignment( dmAlignRight );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "mm2", L"mm", DmWidgetLayout( 72, 55, 18, 20, true), L"metro_info") );
					pP->setHorizontalTextAlignment( dmAlignLeft );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "drmoa", L"-", DmWidgetLayout( 50, 75, 20, 20, true), L"Edit_48") );
					pP->setHorizontalTextAlignment( dmAlignRight );
					pP->setVerticalTextAlignment( dmAlignBottom );
					pP = &pA->addWidget<DmPanel>( DmWidgetCreationValues( "moa2", L"MOA", DmWidgetLayout( 72, 75, 18, 20, true), L"metro_info") );
					pP->setHorizontalTextAlignment( dmAlignLeft );
					pP->setVerticalTextAlignment( dmAlignBottom );*/
				}
				pP = &pL->addWidget<DmPanel>( "content", L"", L"metro_darkblue" );
				m_LANES[l].m_SV_Popup.buildGUI( pP );
				//pL->addWidget<DmPanel>( L"left", L"", L"white" );
				//pL->hide();
				//pL->addWidget<DmPanel>( L"right", L"", L"white" );
				//pL->hide();
			}
		}
	}

	m_pLiveMenu = &pRoot->addWidget<DmPanel>( L"lMenu", L"", rect_Menu.topLeft.x, rect_Menu.topLeft.y, rect_Menu.bottomRight.x, rect_Menu.bottomRight.y, L"metro_darkblue" );
	m_pLiveMenu->hide();
	{
		DmPanel *pP;
		pP = &m_pLiveMenu->addWidget<DmPanel>( L"title", L"Testing", L"metro_yellow" );
		pP->setHeight( block/2 );
		pP->setHorizontalTextAlignment( dmAlignLeft );
		pP->setHorizontalTextMargin( block/2 );

		for (int l=0; l<numLanes; l++)
		{
			DmPanel *pL = &m_pLiveMenu->addWidget<DmPanel>( l, L"", "metro_darkblue" );
			{
				pP = &pL->addWidget<DmPanel>( "name", L"Hendrik van der Merwe", L"Edit_60" );
				pP->setHorizontalTextAlignment( dmAlignCenter );
				//pP = &pL->addWidget<DmPanel>( "ammoLive", L"MP5 test", L"Edit_48" );
				//pP->setHorizontalTextAlignment( dmAlignRight );
				DmPanel *pCnt = &pL->addWidget<DmPanel>( "content", L"", L"metro_darkblue" );
				m_LANES[l].m_SV_Inst.buildGUI( pCnt );

				pP = &pL->addWidget<DmPanel>( "print", L"", L"empty_24" );
				pP->setValue( L"icon_print" );
				DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
				pP->setSize( block*0.5,  block*0.5 );
				pP->setPosition( m_rect_Menu.width()-block, block*0.25);
				pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
				pP->setCommandText( L"PRINT" );
				pP->setValue( DmString( l ) );
				DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );

				pP = &pL->addWidget<DmPanel>( "replay", L"Debrief", L"empty_24" );
				pP->setVerticalTextAlignment( dmAlignBottom );
				pP->setValue( L"icon_next" );
				DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
				pP->setSize( block*0.5,  block*0.5 );
				pP->setPosition( m_rect_Menu.width()-block, block*0.25);
				pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
				pP->setCommandText( L"VID_REPLAY" );
				DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );

				//pL->addWidget<DmPanel>( L"left", L"", L"white" );
				//pL->hide();
				//pL->addWidget<DmPanel>( L"right", L"", L"white" );
				//pL->hide();
			}
		}

		DmPanel *pC;
		pC = &m_pLiveMenu->addWidget<DmPanel>( L"controls", L"", L"metro_darkblue" );
		pC->setHeight( block );
		pC->setPosition( 0, rect_Menu.height() - (block));
		//pC->setHorizontalTextAlignment( dmAlignLeft );
		//pC->setHorizontalTextMargin( block );

		pP = &pC->addWidget<DmPanel>( L"next", L"Next", L"" );
		pP->setValue( L"icon_next" );
		DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
		pP->setSize( block*2.0,  block*1.0 );
		pP->setPosition( 0, 0);
		pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
		pP->setCommandText( L"spacebar" );
		DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );
		

		pP = &pC->addWidget<DmPanel>( L"menu", L"", L"" );
		pP->setValue( L"icon_menu" );
		DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
		pP->setSize( block*0.5,  block*0.5 );
		pP->setPosition( m_rect_Menu.width()-block, block*0.25);
		pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
		pP->setCommandText( L"menu" );
		DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );


		m_pPosition = &pC->addWidget<DmPanel>( L"prone", L"Prone", L"" );
		m_pPosition->setValue( L"icon_sitting" );
		DmConnect( m_pPosition->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
		m_pPosition->setSize( block,  block );
		m_pPosition->setPosition( m_rect_Menu.width()/2 - 1.5*block, 0);
		//m_pPosition->setClipChildren( false );
		DmConnect( m_pPosition->mouseOver, *this, &lane_GUI::onMouseOver );
		pC->setClipChildren( false );

		pP = &pC->addWidget<DmPanel>( L"spot1", L"Prone", L"" );
		pP->setValue( L"icon_spotting" );
		DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
		pP->setSize( block*0.5,  block*0.5 );
		pP->setPosition( m_rect_Menu.width()/2 + 0.5*block, 0.25*block);

		m_pSpotting = &pC->addWidget<DmPanel>( L"spot", L"Prone", L"" );
		m_pSpotting->setValue( L"icon_spotting_topleft" );
		DmConnect( m_pSpotting->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
		m_pSpotting->setSize( block*0.5,  block*0.5 );
		m_pSpotting->setPosition( m_rect_Menu.width()/2 + 1.0*block, 0.25*block);
		DmConnect( m_pSpotting->mouseOver, *this, &lane_GUI::onMouseOverSpot );



		m_pPositionSelect = &pC->addWidget<DmPanel>( L"posselect", L"", L"metro_darkblue" );
		m_pPositionSelect->setSize( block*4,  block*1 );
		m_pPositionSelect->setPosition( m_rect_Menu.width()/2 - 2.5*block, 0);
		DmConnect( m_pPositionSelect->mouseOut, *this, &lane_GUI::onMouseOut );
		m_pPositionSelect->hide();

		{
			pP = &m_pPositionSelect->addWidget<DmPanel>( L"prone", L"", L"metro_darkblue" );
			pP->setValue( L"icon_prone" );
			DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
			pP->setSize(  block,  block );
			pP->setPosition( 0,  0);
			pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
			pP->setCommandText( L"PRONE" );
			DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );

			pP = &m_pPositionSelect->addWidget<DmPanel>( L"sitting", L"", L"metro_darkblue" );
			pP->setValue( L"icon_sitting" );
			DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
			pP->setSize(  block,  block );
			pP->setPosition( block*1,  0);
			pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
			pP->setCommandText( L"SIT" );
			DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );

			pP = &m_pPositionSelect->addWidget<DmPanel>( L"kneeling", L"", L"metro_darkblue" );
			pP->setValue( L"icon_kneeling" );
			DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
			pP->setSize(  block,  block );
			pP->setPosition( block*2,  0);
			pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
			pP->setCommandText( L"KNEEL" );
			DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );

			pP = &m_pPositionSelect->addWidget<DmPanel>( L"standing", L"", L"metro_darkblue" );
			pP->setValue( L"icon_standing" );
			DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
			pP->setSize(  block,  block );
			pP->setPosition( block*3,  0);
			pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
			pP->setCommandText( L"STAND" );
			DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );
		}



		m_pSpottingSelect = &pC->addWidget<DmPanel>( L"spotselect", L"", L"metro_darkblue" );
		m_pSpottingSelect->setSize( block*5,  block*1 );
		m_pSpottingSelect->setPosition( m_rect_Menu.width()/2 - 1*block, 0);
		DmConnect( m_pSpottingSelect->mouseOut, *this, &lane_GUI::onMouseOut );
		m_pSpottingSelect->hide();

		{
			pP = &m_pSpottingSelect->addWidget<DmPanel>( L"prone", L"", L"metro_darkblue" );
			pP->setValue( L"icon_spotting_topleft" );
			DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
			pP->setSize(  block,  block );
			pP->setPosition( 0,  0);
			pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
			pP->setCommandText( L"SPOT_TOPLEFT" );
			DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );

			pP = &m_pSpottingSelect->addWidget<DmPanel>( L"sitting", L"", L"metro_darkblue" );
			pP->setValue( L"icon_spotting_topmiddle" );
			DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
			pP->setSize(  block,  block );
			pP->setPosition( block*1,  0);
			pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
			pP->setCommandText( L"SPOT_TOPMIDDLE" );
			DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );

			pP = &m_pSpottingSelect->addWidget<DmPanel>( L"kneeling", L"", L"metro_darkblue" );
			pP->setValue( L"icon_spotting_bottomleft" );
			DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
			pP->setSize(  block,  block );
			pP->setPosition( block*2,  0);
			pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
			pP->setCommandText( L"SPOT_BOTTOMLEFT" );
			DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );

			pP = &m_pSpottingSelect->addWidget<DmPanel>( L"standing", L"", L"metro_darkblue" );
			pP->setValue( L"icon_spotting_bottommiddle" );
			DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
			pP->setSize(  block,  block );
			pP->setPosition( block*3,  0);
			pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
			pP->setCommandText( L"SPOT_BOTTOMMIDDLE" );
			DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );

			pP = &m_pSpottingSelect->addWidget<DmPanel>( L"5", L"", L"metro_darkblue" );
			pP->setValue( L"icon_spotting_hide" );
			DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
			pP->setSize(  block,  block );
			pP->setPosition( block*4,  0);
			pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
			pP->setCommandText( L"SPOT_HIDE" );
			DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );
		}


		

		/*pP = &pP->addWidget<DmPanel>( L"params", L"", L"metro_border" );
		pP->setCommandText( L"PRONE" );
		DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );
		*/
		/*
		pP = &pC->addWidget<DmPanel>( L"kneel", L"Kneel / Sit", L"metro_green" );
		pP->setValue( L"icon_kneeling" );
		DmConnect( pP->drawCustomBackground, m_pCTRL, &ctrl_DirectWeapons::drawCustomSprite );
		pP->setSize( block,  block );
		pP->setPosition( 6*block, 0);
		pP->setCommandText( L"KNEEL" );
		DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );

		pP = &pC->addWidget<DmPanel>( L"stand", L"Standing", L"metro_green" );
		pP->setSize( block*1.2,  block*0.5 );
		pP->setPosition( 8*block, 0);
		pP->setCommandText( L"STAND" );
		DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );
		*/
	}

	// Debrief -------------------------------------------------------------------------------------------------------------------------------------------------------
	
	// Bou Intro skerm, laaste want dis bo-oor alles -----------------------------------------------------------------------------------------------------------------
	m_pIntro3D = &pRoot->addWidget<DmPanel>( L"i3D", L"", rect_3D.topLeft.x, rect_3D.topLeft.y, rect_3D.bottomRight.x, rect_3D.bottomRight.y, L"metro_darkblue" );
	m_pIntro3D->hide();
	{
		float w = rect_3D.width();
		float h = rect_3D.height();
		float wL = w / numLanes;
		DmPanel *pP;
		pP = &m_pIntro3D->addWidget<DmPanel>( L"title", L"Testing", L"metro_yellow" );
		pP->setHeight( block/2 );
		pP->setTopMargin( 0 );
		pP->setHorizontalTextAlignment( dmAlignLeft );
		pP->setHorizontalTextMargin( block/2 );

		for (int l=0; l<numLanes; l++)
		{
			float h2 = h-block*2;
			DmPanel *pL = &m_pIntro3D->addWidget<DmPanel>( l, L"", l*wL, block/2, l*wL+wL, h-block );
			{
				pP = &pL->addWidget<DmPanel>( "lane", DmString(l+1+pCTRL->m_LaneZero), L"Edit_48" );
				pP->setSize( block/2, block/2 );
				pP->setPosition( (wL-block/2)/2, h2*2/7 );

				pP = &pL->addWidget<DmPanel>( "name", L"Hendrik van der Merwe", L"Edit_60" );
				pP->setSize( wL*0.9f, h2/7 );
				pP->setPosition( wL*0.05f, h2*3/7 );

				pP = &pL->addWidget<DmPanel>( "ammoIntro", L"MP5 test", L"Edit_60" );
				pP->setSize( wL*0.9f, h2/7 );
				pP->setPosition( wL*0.05f, h2*4/7 );
			}
			if (l> 0) m_pIntro3D->addWidget<DmPanel>( L"L1" + DmString(l+1), L"", l*wL-2, block/2, l*wL+2, h-block, L"white" );
			//if (l> 0) m_pIntro3D->addWidget<DmPanel>( L"L2" + DmString(l+1), L"", l*wL+wL-2, block, l*wL+wL+2, h-block*1.5, L"white" );
			//m_pIntro3D->addWidget<DmPanel>( L"L3" + DmString(l+1), L"", l*wL, block/2, l*wL+wL, block/2+4, L"white" );
			//m_pIntro3D->addWidget<DmPanel>( L"L4" + DmString(l+1), L"", l*wL, h-block-4, l*wL+wL, h-block, L"white" );
		}
	}

	m_pIntroMenu = &pRoot->addWidget<DmPanel>( L"iMenu", L"", rect_Menu.topLeft.x, rect_Menu.topLeft.y, rect_Menu.bottomRight.x, rect_Menu.bottomRight.y, L"metro_darkblue" );
	m_pIntroMenu->hide();
	{
		float w = rect_Menu.width();
		float h = rect_Menu.height();
		float wL = w / numLanes;
		DmPanel *pP;
		pP = &m_pIntroMenu->addWidget<DmPanel>( L"title", L"Testing", L"metro_yellow" );
		pP->setHeight( block/2 );
		pP->setTopMargin( 0 );
		pP->setHorizontalTextAlignment( dmAlignLeft );
		pP->setHorizontalTextMargin( block/2 );

		

		for (int l=0; l<numLanes; l++)
		{
			float h2 = h-block*2;
			/*
			pP = &m_pIntroMenu->addWidget<DmPanel>( L"click" + DmString(l+1), L"Click to select", l*wL, block, l*wL+wL, h-block, L"Edit_48" );
			pP = &pP->addWidget<DmPanel>( L"click", L"", L"metro_border" );
			pP->setCommandText( L"select_lane" );
			pP->setValue( l );
			DmConnect( pP->mouseClick, *this, &lane_GUI::toolClick );
			*/

			DmPanel *pL = &m_pIntroMenu->addWidget<DmPanel>( l, L"", l*wL, block/2, l*wL+wL, h-block, "metro_darkblue" );
			{
				pP = &pL->addWidget<DmPanel>( "lane", DmString(l+1+pCTRL->m_LaneZero), L"metro_yellow" );
				pP->setSize( block/2, block/2 );
				pP->setPosition( (wL-block/2)/2, h2*2/7 );

				pP = &pL->addWidget<DmPanel>( "name", L"Hendrik van der Merwe", L"Edit_60" );
				pP->setSize( wL*0.9f, h2/7 );
				pP->setPosition( wL*0.05f, h2*3/7 );
				if (wL < 200) pL->setStyleName(  L"Edit_30" );

				pP = &pL->addWidget<DmPanel>( "ammoMenu", L"MP5 test", L"Edit_60" );
				pP->setSize( wL*0.9f, h2/7 );
				pP->setPosition( wL*0.05f, h2*4/7 );
				if (wL < 200) pL->setStyleName(  L"Edit_30" );
			}

			pP = &m_pIntroMenu->addWidget<DmPanel>( L"click" + DmString(l+1), L"Click to select", l*wL, block/2, l*wL+wL, h-block, L"empty" );
			pP->setVerticalTextAlignment( dmAlignBottom );
			pP->setHorizontalTextAlignment( dmAlignLeft );
			pP = &pP->addWidget<DmPanel>( L"click", L"", L"metro_border" );
			pP->setCommandText( L"select_lane" );
			pP->setValue( l );
			DmConnect( pP->mouseClick, m_pCTRL, &ctrl_DirectWeapons::toolClick );

			if (l> 0) m_pIntroMenu->addWidget<DmPanel>( L"L1" + DmString(l+1), L"", l*wL-2, block/2, l*wL+2, h-block, L"white" );
			//if (l>0) m_pIntroMenu->addWidget<DmPanel>( L"L2" + DmString(l+1), L"", l*wL+wL-2, block, l*wL+wL, h-block*1.5, L"white" );
			//m_pIntroMenu->addWidget<DmPanel>( L"L3" + DmString(l+1), L"", l*wL, block/2, l*wL+wL, block/2+4, L"white" );
			//m_pIntroMenu->addWidget<DmPanel>( L"L4" + DmString(l+1), L"", l*wL, h-block-4, l*wL+wL, h-block, L"white" );
		}

		m_pLoadWaitMenu = &m_pIntroMenu->addWidget<DmPanel>( L"info", L"Loading, please wait ...", L"metro_darkblue" );
	}

	
	ew_Scene *pScene = ew_Scene::GetSingleton();
	m_CurrentTexture = 0;
	for (int i=0; i<NUM_SHOT_TEX; i++ )	
	{
		pScene->getD3DDevice()->CreateTexture( SHOT_TEX_SIZE, SHOT_TEX_SIZE, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &m_pTexture[i], NULL );
		m_pTexture[i]->GetSurfaceLevel( 0, &m_pSurface[i] );
	}

	pScene->getD3DDevice()->CreateOffscreenPlainSurface( SHOT_TEX_SIZE, SHOT_TEX_SIZE, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &m_pOffscreenSurface, NULL );
}

void lane_GUI::copytoOffsceenandRPC( LPDIRECT3DSURFACE9 from )
{
	HRESULT hr = D3DXLoadSurfaceFromSurface( m_pOffscreenSurface, NULL, NULL, from, NULL, NULL, D3DX_FILTER_NONE, NULL );

	//if (m_pCTRL->m_bInstructorStation)
	/*
	{
		char *x;
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_SHOTIMAGE );
		Bs.Write( (unsigned short) (lane + m_pCTRL->m_LaneZero) );
		Bs.Write( (unsigned short)shotnumber );

		D3DLOCKED_RECT r;
		m_pOffscreenSurface->LockRect( &r, NULL, NULL );
		Bs.Write( (char*)r.pBits,  SHOT_TEX_SIZE*SHOT_TEX_SIZE*4 );
		m_pOffscreenSurface->UnlockRect();

		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}
	*/
}


void  lane_GUI::debriefLayout()

{
	float w = m_rect_3D.width();
	float h = m_rect_3D.height();
	float wL = w / m_NumActiveLanes;
	if (m_NumActiveLanes == 1) wL = w * 0.6;
	float wC = __min( 512, __min(wL-40, h*0.4) );
	if (wL > h) wC = h*0.8;			// spesiale geval vir een laan
	float x = 0;
	float b= h/24;
	float space = (wL - wC) / 2;
	

	for (unsigned int i=0; i<m_NumLanes; i++)
	{
		DmPanel *pLane = (DmPanel *)m_pLive3D->getWidget( i );
		if ((m_NumActiveLanes==1) && i!=m_ActiveLane ) pLane->hide();
		else
		{
			pLane->setPosition( x, 0 );
			x+= wL;
			pLane->show();
			( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setPosition( 0, b*3 );
			( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setSize( wL, h-b*4 );
			//( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setStyleName( L"empty" );

			m_LANES[i].m_SV_Spotting.layoutDebrief( wL, h-b*3 );
		}
	}
}

void  lane_GUI::liveLayout()
{
	
	float w = m_rect_3D.width();
	float h = m_rect_3D.height();
	float wL = w / m_NumActiveLanes;
	float x = 0;

	if (m_pCTRL->m_bLiveFire)
	{
		x += w*dw_scenario::s_pCTRL->m_ScreenSideBuffer;
		w = w * (1.0 - 2.0*dw_scenario::s_pCTRL->m_ScreenSideBuffer);
		wL = w / m_NumActiveLanes;
	}

	float wC = __min( 512, __min(wL-40, h*0.4) );
	//if (wL > h) wC = h*0.8;			// spesiale geval vir een laan
	
	float b= h/24;
	float space = (wL - wC) / 2;
	float wVis = __min( wL * 0.95, b*10);
	float wSpace = (wL - wVis) / 2.0f;
//	float wC = wSpace - 20;
	float N= h/30;

	

	for (unsigned int i=0; i<m_NumLanes; i++)
	{
		DmPanel *pLane = (DmPanel *)m_pLive3D->getWidget( i );
		if ((m_NumActiveLanes==1) && i!=m_ActiveLane ) pLane->hide();
		else
		{
			pLane->setPosition( x, 0 );
			x+= wL;
			//pLane->setSize( wL, h );
			pLane->show();
			( (DmPanel*)pLane->findWidget( L"number" ) )->setPosition( 0, 0 );
			( (DmPanel*)pLane->findWidget( L"number" ) )->setSize( b, b );
			( (DmPanel*)pLane->findWidget( L"totalScore" ) )->setPosition( wVis-2*b, 0 );
			( (DmPanel*)pLane->findWidget( L"totalScore" ) )->setSize( b*2, b );
			( (DmPanel*)pLane->findWidget( L"totalScore" ) )->setHorizontalTextAlignment( dmAlignRight );
			( (DmPanel*)pLane->findWidget( L"totalScore" ) )->setHorizontalTextMargin( 5 );
			( (DmPanel*)pLane->findWidget( L"header" ) )->setPosition( wSpace, N );
			( (DmPanel*)pLane->findWidget( L"header" ) )->setSize( wVis, b );
			//( (DmPanel*)pLane->findWidget( L"content" ) )->setPosition( 0, h/12 );
			//( (DmPanel*)pLane->findWidget( L"content" ) )->setSize( wL, h*11/12 );
			( (DmPanel*)pLane->findWidget( L"content" ) )->hide();
			//( (DmPanel*)pLane->findWidget( L"content" ) )->setPosition( space, b*2 );
			//( (DmPanel*)pLane->findWidget( L"content" ) )->setSize( wC, h*9/12-b*2 );
			( (DmPanel*)pLane->findWidget( L"content" ) )->setPosition( wSpace, b*4 );
			( (DmPanel*)pLane->findWidget( L"content" ) )->setSize( wVis, h-b*4 );
			( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setPosition( wSpace, h*9/12 );
			( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setSize( wVis, h*3/12 );
			if (m_NumActiveLanes==1)	( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->setSize( h*5/12, h*3/12 );
			( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->hide();
			( (DmPanel*)pLane->findWidget( L"ammoInfo" ) )->hide();
			//( (DmPanel*)pLane->findWidget( L"left" ) )->setPosition( 0, 0 );
			//( (DmPanel*)pLane->findWidget( L"left" ) )->setSize( 1, 1 );
			//( (DmPanel*)pLane->findWidget( L"right" ) )->setPosition( wL-2, 0 );
			//( (DmPanel*)pLane->findWidget( L"right" ) )->setSize( 1, 1 );
			m_LANES[i].m_SV_Spotting.layoutSpotting( wVis, h*3/12 );
			m_LANES[i].m_SV_Popup.layoutNarrow( wVis - 20, m_NumActiveLanes, 10, 10 );
		}
	}
	movescope( m_bScopePosition );

	w = m_rect_Menu.width();
	h = m_rect_Menu.height();
	wL = w / m_NumActiveLanes;
	wC = __min( 512, __min(wL-4, h*0.4) );
	if (m_NumActiveLanes == 1) wC = w * 0.8;			// spesiale geval vir een laan
	space = (wL - wC) / 2;

	b= h/24;
	wVis = __min( wL * 0.95, b*10);
	if (m_NumActiveLanes == 1) wVis = wL * 0.95;
	wSpace = (wL - wVis) / 2.0f;

	
	x = 0;


	for (unsigned int i=0; i<m_NumLanes; i++)
	{
		DmPanel *pLane = (DmPanel *)m_pLiveMenu->getWidget( i );
		if ((m_NumActiveLanes==1) && i!=m_ActiveLane ) pLane->hide();
		else
		{
			pLane->setPosition( x, b*2 );
			x+= wL;
			pLane->setSize( wL, h-b*2 );
			pLane->show();
			( (DmPanel*)pLane->findWidget( L"name" ) )->setStyleName( L"Edit_48" );
			( (DmPanel*)pLane->findWidget( L"name" ) )->setPosition( space+b, 0 );				//  JOHAN
			( (DmPanel*)pLane->findWidget( L"name" ) )->setSize( wVis-3*b, b*1 );

			//( (DmPanel*)pLane->findWidget( L"number" ) )->setStyleName( L"Edit_48" );
			//( (DmPanel*)pLane->findWidget( L"number" ) )->setPosition( space, 0 );				//  JOHAN
			//( (DmPanel*)pLane->findWidget( L"number" ) )->setSize( b, b );

			//( (DmPanel*)pLane->findWidget( L"totalScore" ) )->setStyleName( L"Edit_48" );
			//( (DmPanel*)pLane->findWidget( L"totalScore" ) )->setPosition( space+wVis-b*2, 0 );				//  JOHAN
			//( (DmPanel*)pLane->findWidget( L"totalScore" ) )->setSize( 2*b, b*1 );

			( (DmPanel*)pLane->findWidget( L"content" ) )->setPosition( space, b*1 );
			( (DmPanel*)pLane->findWidget( L"content" ) )->setSize( wVis, h-b*3 );

			( (DmPanel*)pLane->findWidget( L"print" ) )->setPosition( wL/2 - b, h-b*7 );
			( (DmPanel*)pLane->findWidget( L"print" ) )->setSize( b*2, b*2 );
			( (DmPanel*)pLane->findWidget( L"print" ) )->hide();
			

			m_LANES[i].m_SV_Inst.layoutNarrow( wVis - 20, m_NumActiveLanes, 10, 10 );
			if (m_NumActiveLanes == 1)
			{
				( (DmPanel*)pLane->findWidget( L"content" ) )->setSize( wC, h * 0.75 );
				wC = h * 0.75;			// spesiale geval vir een laan
				m_LANES[i].m_SV_Inst.layoutNarrow( wC-20, m_NumActiveLanes, 10, 10 );

				( (DmPanel*)pLane->findWidget( L"print" ) )->setPosition( wVis - b*8, h-b*7 );
				( (DmPanel*)pLane->findWidget( L"print" ) )->setSize( b*2, b*2 );
				( (DmPanel*)pLane->findWidget( L"print" ) )->show();

				( (DmPanel*)pLane->findWidget( L"replay" ) )->setPosition( wVis - b*5, h-b*7 );
				( (DmPanel*)pLane->findWidget( L"replay" ) )->setSize( b*2, b*2 );
			}
			
		}
	}

	if (m_rect_Menu.topLeft.x == m_rect_3D.topLeft.x) m_pLiveMenu->hide();
}

LPDIRECT3DSURFACE9 lane_GUI::onFire( int lane, int shotNumber, DmString ammo, float time, float wind, DmString overlay, d_Vector pos, d_Vector dir, float fov )
{
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"lane_GUI::onFire( %d, %d, %s, .. %s  .. )",  lane, shotNumber, ammo.c_str(), overlay.c_str()  );
	
	if (m_pCTRL->m_b3DTerminal)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_ON_FIRE );
		Bs.Write( (unsigned short) (lane + m_pCTRL->m_LaneZero) );
		Bs.Write( (unsigned short) shotNumber );
		Bs.Write( ammo.getMBString().c_str() );
		Bs.Write( time ); 
		Bs.Write( wind );
		Bs.Write( overlay.getMBString().c_str() );
		writeVector(Bs, pos);
		writeVector(Bs, dir);
		Bs.Write( fov );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}

	
	LPDIRECT3DTEXTURE9 pTEX = m_pTexture[ m_CurrentTexture ];
	LPDIRECT3DSURFACE9 pSRF = m_pSurface[ m_CurrentTexture ];
	m_CurrentTexture ++;
	if( m_CurrentTexture >= NUM_SHOT_TEX) m_CurrentTexture=0;
	
	guiSHOT S;
	m_LANES[lane].m_Shots.push_back( S );
	unsigned int N = m_LANES[lane].m_Shots.size()-1;
	float dT = 0;
	if(N>0) dT = time -  m_LANES[lane].m_Shots[N-1].m_time;
	m_LANES[lane].m_Shots[N].onFire( N+1, ammo, time, dT, wind, overlay, pos, dir, fov, pTEX );
	m_LANES[lane].m_SV_Inst.displayShot( &m_LANES[lane].m_Shots );
	m_LANES[lane].m_SV_Spotting.displayShot( &m_LANES[lane].m_Shots );

	DmPanel *pLane = (DmPanel *)m_pLive3D->getWidget( lane );
	if(pLane) ( (DmPanel*)pLane->findWidget( L"ammoInfo" ) )->hide();
	( (DmPanel*)pLane->findWidget( L"spottingscope" ) )->show();
	
	return pSRF;
}
void lane_GUI::onHit( int lane, d_Vector hitPos, float tof, float tX, float tY, int score, int totalscore, int grp )
{
	if (m_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_ON_HIT );
		Bs.Write( (unsigned short) (lane + m_pCTRL->m_LaneZero) );
		writeVector(Bs, hitPos);
		Bs.Write( tof );
		Bs.Write( tX );
		Bs.Write( tY );
		Bs.Write( (unsigned short)score );
		Bs.Write( (unsigned short)totalscore );
		Bs.Write( (float)grp );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}


	if (m_NumActiveLanes == 1)
	{
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"lane_GUI::onHit   - oveeride lane to   %d  ",  lane  );
		lane = m_ActiveLane;
	}
	if (lane>=m_LANES.size()) return;
	// add to the last shot
	//m_pCTRL->getLane( lane ).ammo.m_muzzleVelocity;
	int N = m_LANES[lane].m_Shots.size()-1;
	if (N >= 0)
	{
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"lane_GUI::onHit   - N = %d  ",  N  );
		m_LANES[lane].m_Shots[N].onHit( hitPos, tof, tX, tY, score, totalscore, grp );
		m_LANES[lane].m_SV_Inst.displayShot( &m_LANES[lane].m_Shots );
		m_LANES[lane].m_SV_Spotting.displayShot( &m_LANES[lane].m_Shots );
	}
	else
	{
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_ERROR, L"lane_GUI::onHit   - shots was empty , HIT not added "  );
	}
}

void lane_GUI::setTargetType(int lane, DmString target, float scale)
{ 
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"b_1"  );


	if (m_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "lane_GUI::setTargetType" );
		Bs.Write( ewstring(lane).c_str() );
		Bs.Write( ewstring(target).c_str() );
		Bs.Write( ewstring(scale).c_str() );
		ew_Scene::GetSingleton()->rpc_call( "rpc_CALL", &Bs );
	}

	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"b_2"  );

	if (lane>=m_LANES.size()) return;
	m_LANES[lane].m_SV_Popup.setTarget( L"target_" + target, scale );

	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"b_3"  );
	//if (m_pCTRL->m_bInstructorStation)  m_LANES[lane].m_SV_Inst.setTarget( L"target_" + target, scale );
	m_LANES[lane].m_SV_Inst.setTarget( L"target_" + target, scale );
	// FIXEME kyk of dit my popups verbeter vir twee skerm - sal later toets

	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"b_4"  );
}










// void lane_GUI::setShot(int l , float X, float Y, int score, int grouping )
// {
	 /*
	 float tX = X * m_TargetScale * m_TargetWidth/2 + m_TargetWidth/2;
	 float tY = m_TargetWidth/2 - Y * m_TargetScale * m_TargetWidth/2;
	 static int CNT=0;
	 CNT++;
	 m_Lanes[l].pTargetImage->addWidget<DmPanel>( L"B1" + DmString(CNT), L"", tX-15, tY-15, tX+15, tY+15, L"bulletX" );

	 m_Lanes[l].pTargetScore->setText( DmString( score ) );
	 m_Lanes[l].pTargetGrouping->setText( DmString( grouping ) + L" mm" );
	 */
// }





 //void lane_GUI::showShotImage(int lane, int shotNumber, bool bRifle, float time)
 //{ 
 /*
	 if (lane > 4) return;
	 if (shotNumber >23) return;

	 m_Lanes[lane].shotTime[shotNumber] = time;			// save the time of this shot
	 m_Lanes[lane].shotTime[shotNumber+1] = 99999;			// save the time of this shot
	 
	 m_Lanes[lane].pTimelineImage[shotNumber]->show();
	 m_Lanes[lane].pTimelineTxt[shotNumber]->show();
	 if (bRifle)	m_Lanes[lane].pTimelineImage[shotNumber]->setHelpText( L"shot_overlay_rifle" );
	 else			m_Lanes[lane].pTimelineImage[shotNumber]->setHelpText( L"shot_overlay" );

	 m_Lanes[lane].pScreenshotTarget->setValue( DmString(shotNumber) );
	 if (bRifle)	m_Lanes[lane].pScreenshotTarget->setHelpText( L"shot_overlay_rifle" );
	 else			m_Lanes[lane].pScreenshotTarget->setHelpText( L"shot_overlay" );
	 m_Lanes[lane].pScreenshotTarget->show(); m_Lanes[lane].m_ShotTimer = 5.0f;
	 m_Lanes[lane].pScreenshotTarget->setSize( m_block*2, m_block*2 );
	 m_Lanes[lane].pScreenshotTarget->setTopMargin(m_screenHeight - m_block*2 - m_block/2);
	 */
 //}

 void lane_GUI::displayShotImage(int lane, int shotNumber)
 { /*
	 if (lane > 4) return;
	 if (shotNumber >24) return;

	 m_Lanes[lane].pScreenshotTarget->setValue( DmString(shotNumber) );
	 m_Lanes[lane].pScreenshotTarget->show(); 
	 m_Lanes[lane].pScreenshotTarget->setSize( m_screenHeight - m_block, m_screenHeight - m_block );
	 m_Lanes[lane].pScreenshotTarget->setTopMargin(m_block/2);
	 m_Lanes[lane].m_ShotTimer = 5.0f;*/
 }

 int lane_GUI::getCurrentShot(int lane, float dTime)
 {/*
	 if (lane > 4) return -1;
	 
	 for (int i=0; i<24; i++)
	 {
		 if( (m_Lanes[lane].shotTime[i] < dTime) && (m_Lanes[lane].shotTime[i+1] > dTime)) return i;			// save the time of this shot
	 }
	 */
	 return -1;
 }



