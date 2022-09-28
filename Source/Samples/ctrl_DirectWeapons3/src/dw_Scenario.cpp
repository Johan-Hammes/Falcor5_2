
#include "dw_Scenario.h"
#include "ew_Scene.h"
#include "Controllers/ctrl_DirectWeapons/ctrl_DirectWeapons.h"
#include "earthworks_Raknet.h"
#include "server_defines.h"
#include "Logger/CSVFile.h"
#include "VoiceRecognition/VoiceRecognition.h"
#include "Controllers/ctrl_DirectWeapons/TargetManager.h"
#include "Controllers/ctrl_DirectWeapons/Target.h"
#include "Shellapi.h"


#include <comdef.h>

dw_scenario			*dw_scenario::s_pSCENARIO;
cameraAnim			dw_scenario::s_Cameras;
ctrl_DirectWeapons	*dw_scenario::s_pCTRL;
ew_Scene			*dw_scenario::s_pSCENE;
vector<dw_target*>	dw_scenario::s_targets;
dw_target*			dw_scenario::s_targetGUIDs[1024];


static int NUM_MOI =  0;


// target -----------------------------------------------------------------------------------------------------------------------
dw_target::dw_target( int lane, LPCWSTR type, LPCWSTR mesh, LPCWSTR startCam, int AI, bool bGRP )
{
	m_bScoreOnGrouping = bGRP;

	TargetManager *targetManager = dw_scenario::s_pCTRL->getTargetManager();
	m_target = targetManager->getTarget(mesh);

	m_lane = lane;
	//D3DXMATRIX mat;
	//D3DXMatrixIdentity( &mat );
	wstring N = m_target ? m_target->getMeshFile() : mesh;
	string sN = string( N.begin(), N.end() );
	wstring T = type;
	string sT = string( T.begin(), T.end() );		// fixme use ewStering if we have to conevr or get rid of conevrsuions
	char CH_name[256];
	sprintf(CH_name, "target_%d_%d", lane, dw_scenario::s_targets.size() );
	m_name = L"target_" + DmString(lane) + L"_"+ DmString( dw_scenario::s_targets.size() );
	m_ImageName = DmString(mesh);// + L".png";
	///m_SpriteName = m_target->m_spriteFile.getWString();
	if (m_target)
	{
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"   m_target exists"  );
		m_SpriteName = m_target->m_spriteFile.getWString();
	}
	
	
	d_Matrix M;
	M.identity();
	if (startCam)	M = dw_scenario::s_Cameras.getMatrix( startCam );
	else			M.elements[3][0] = 1000 + lane*10;					// spread tehm far off
	dw_scenario::s_pSCENE->rpc_call_LoadModel( 1, sN.c_str(), sT.c_str(), CH_name, "",  M.toD3DX(), AI);
	pMOI = NULL;

	m_Type = TGT_STATIC;
	m_AI_type = L"";
	
	m_score = 0;
	m_Scale = m_target ? m_target->getScale() : 0;
}

dw_target::~dw_target()
{
}


void dw_target::start()
{
	m_score = 0;
	m_shotInfo.clear();
}

void dw_target::popup( unsigned short repeat, float tDn, float tUp )
{
	m_Type = TGT_POPUP;
	m_AI_type = L"AI_Popup";

	RakNet::BitStream Bs;
	Bs.Write( m_GUID );
	Bs.Write( (unsigned short)AI_TARGET_POPUP );
	Bs.Write( repeat );
	Bs.Write( tDn );
	Bs.Write( tUp );
	dw_scenario::s_pSCENE->rpc_call( "set_AI", &Bs );	// FIXME ons misbruik hierdie nog en dis vreeslik haw demo spesefiek
}

#define RND_1_1		((float)rand() / (float)RAND_MAX - 0.5f) * 2.0f		// minus 1 to plus 1
void dw_target::move( unsigned short repeat, int TYPE, float speed )
{
	m_Type = TGT_MOVING;
	m_AI_type = L"AI_Moving";
	f_Vector start, stop;
	
//	float dist = 2.0f;	// werk uit vanaf scenario laan hoe en aftsand

	int numL = dw_scenario::s_pSCENARIO->get_numLanes();
	float fov = dw_scenario::s_pSCENE->getCamera()->getFOV();
	if ( dw_scenario::s_pCTRL->m_bLiveFire ) fov *= (1.0 - 2.0*dw_scenario::s_pCTRL->m_ScreenSideBuffer);
	d_Matrix M = *dw_scenario::s_pSCENE->getCamera()->getMatrix();
	d_Vector pos = M.pos();
	d_Vector dir = M.dir();
	d_Vector right = M.right();
	d_Vector newPos = pos - dir * m_Distance;
	newPos.y = 0;

	float W1 = tan(fov*0.01745329251994329576923690768489/2)*2;
	float W = W1 * m_Distance / numL;
	newPos = newPos + right * (W*m_lane + W/2 - (W1*m_Distance/2));

	dir = pos - newPos;
	dir.y = 0;
	dir.normalize();

	float speedMin = speed;
	float speedMax = speed;
	unsigned short segments = 1;
	start.w = 0;
	stop.w = 0;
	float r1, r2;

	switch (TYPE )
	{
	case move_LR:		start = newPos - (right * W/2.2);
						stop = newPos + (right * W/2.2);	
						break;
	case move_RL:		start = newPos + (right * W/2.2);
						stop = newPos - (right * W/2.2);	
						break;
	case move_CHARGE:	speedMin*=0.8;	
						speedMax*=1.25;	
						r1 = __min( W*0.45, m_Distance/6 );
						r2 = __min( W*0.20, m_Distance/6 );
						start = newPos + (dir * r1);
						start.w = r1;
						stop = newPos + (dir * m_Distance*0.66);	
						stop.w = r2;
						break;
	case move_ATTACK:	segments = 5;	
						speedMin*=0.8;	
						speedMax*=-1.25;				// negative speed means we drop inbetween
						r1 = __min( W*0.45, m_Distance/6 );
						r2 = __min( W*0.20, m_Distance/6 );
						start = newPos + (dir * r1);
						start.w = r1;
						stop = newPos + (dir * m_Distance*0.66);	
						stop.w = r2;
						break;
	case move_RANDOM:	speedMin*=0.6;	
						speedMax*=1.5;	
						r1 = __min( W*0.45, m_Distance/4 );
						start = newPos + (dir * r1);
						start.w = r1;
						stop = start;
						break;
	case move_ZIGZAG:	segments = 7;	
						r1 = __min( W*0.45, m_Distance/6 );
						r2 = __min( W*0.20, m_Distance/6 );
						start = newPos + (dir * r1);
						start.w = r1;
						stop = newPos + (dir * m_Distance*0.66);	
						stop.w = r2;
						break;
	}

	/*
	RakNet::BitStream Bs;
	Bs.Write( m_GUID );
	Bs.Write( (unsigned short)AI_TARGET_MOVE );
	Bs.Write( repeat );
	writeVector( Bs, start );
	writeVector( Bs, stop );
	Bs.Write( speed );
	dw_scenario::s_pSCENE->rpc_call( "set_AI", &Bs );	
	*/

	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"   AI_TARGET_MOVE2 type(%d) start(%d, %d, %d, %d) stop(%d, %d, %d, %d)", TYPE, (int)start.x, (int)start.y, (int)start.z, (int)start.w, (int)stop.x, (int)stop.y, (int)stop.z, (int)stop.w  );
	
	RakNet::BitStream Bs;
	Bs.Write( m_GUID );
	if ( wcsstr( pMOI->getFileName(), L"man") != NULL)
	{
		Bs.Write( (unsigned short)AI_HUMAN_MOVE2 );
	}
	else if ( wcsstr( pMOI->getFileName(), L"antelope") != NULL)
	{
		Bs.Write( (unsigned short)AI_ANTELOPE_MOVE2 );
	}
	else
	{
		Bs.Write( (unsigned short)AI_TARGET_MOVE2 );
	}
	Bs.Write( repeat );
	Bs.Write( segments );
	writeVector( Bs, start );
	writeVector( Bs, stop );
	Bs.Write( start.w );
	Bs.Write( stop.w );
	Bs.Write( speedMin );
	Bs.Write( speedMax );
	dw_scenario::s_pSCENE->rpc_call( "set_AI", &Bs );	
}


void dw_target::hide( )
{
	d_Matrix M;
	M.identity();
	M.elements[3][0] = -5000 + RND_1_1 * 100;
	M.elements[3][3] = -5000 + RND_1_1 * 100;

	RakNet::BitStream Bs;
	Bs.Write( m_GUID );
	writedMatrix( Bs, M );
	dw_scenario::s_pSCENE->rpc_call( "move_Model", &Bs );
}


void dw_target::show( float x, float y )
{
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_target::show   %s  (%d, %d) {%X}", this->m_name.c_str(), (int)x, (int)y, pMOI );

	dw_scenario::s_pCTRL->m_laneGUI.setAmmunition( m_lane, y );

	if  ( wcsstr( pMOI->getFileName(), L"antelope") != NULL)		
	{ 
		m_Scale = 1.0;	

		// FIXME - this should be fixed ... AI should be handled somewhere else... not sure where... but not here :)
		RakNet::BitStream Bs2;
		Bs2.Write( m_GUID );
		Bs2.Write( (unsigned short)AI_ANTELOPE );
		dw_scenario::s_pSCENE->rpc_call( "set_AI", &Bs2 );	// FIXME ons misbruik hierdie nog en dis vreeslik haw demo spesefiek
	}
	else if (m_target)
		dw_scenario::s_pCTRL->m_laneGUI.setTargetType( m_lane, m_target->getSpriteFile().c_str(), m_Scale );

	

	if ( y>=0 ) m_Distance = y;	
	//m_shotInfo.clear();
	int numL = dw_scenario::s_pSCENARIO->get_numLanes();

	float fov = dw_scenario::s_pSCENE->getCamera()->getFOV();
	if ( dw_scenario::s_pCTRL->m_bLiveFire ) fov *= (1.0 - 2.0*dw_scenario::s_pCTRL->m_ScreenSideBuffer);
	d_Matrix M = *dw_scenario::s_pSCENE->getCamera()->getMatrix();
	d_Vector pos = M.pos();
	d_Vector dir = M.dir();
	d_Vector right = M.right();
	d_Vector up = d_Vector(0, 1, 0);
	d_Vector newPos = pos - dir * y + right * x;
	newPos.y = 0;

	float W1 = tan(fov*0.01745329251994329576923690768489/2)*2;
	float W = W1 * y / numL;
	newPos = newPos + right * (W*m_lane + W/2 - (W1*y/2));

	//dir = pos - newPos;
	//dir.normalize();
	//right = up ^ dir;
	//M.setRight(right);
	//M.setDir(dir);
	M.setPos( newPos );

	RakNet::BitStream Bs;
	Bs.Write( m_GUID );
	writedMatrix( Bs, M );
	dw_scenario::s_pSCENE->rpc_call( "move_Model", &Bs );

	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_target::show   done}" );
	
}


void dw_target::show( wstring cam, float speed )
{
	

	d_Matrix M = dw_scenario::s_Cameras.getMatrix( cam );

	{
		RakNet::BitStream Bs;
		Bs.Write( m_GUID );
		writedMatrix( Bs, M );
		Bs.Write( speed );
		dw_scenario::s_pSCENE->rpc_call( "move_Model", &Bs );


	}
	if (speed >0) // FIXME verkeerd ons meot toets vr kleiduif
	{
		RakNet::BitStream Bs;
		Bs.Write( m_GUID );
		Bs.Write( (unsigned short)AI_KLEIDUIF );
		dw_scenario::s_pSCENE->rpc_call( "set_AI", &Bs );	
	}
}


void dw_target::bullet_hit( f_Vector worldPos, float tof )
{
	// we have been hit on the server
	// translate to target coordinates, do scoring and save in hits vector of sorts
	
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_target::bullet_hit");

	D3DXMATRIX M, W;
	W = *pMOI->getWorldMatrix();
	float det;
	D3DXMatrixInverse( &M, &det, &W );
	D3DXVECTOR3 in(worldPos.x, worldPos.y, worldPos.z);
	D3DXVECTOR4 out;
	D3DXVec3Transform( &out, &in, &M );

	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"   - a W(%d, %d, %d) det(%d), %s {%X}", (int)(W._11*1000), (int)(W._12*1000), (int)(W._13*1000), (int)(det*1000), pMOI->getFileName(), pMOI );
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"   - a W(%d, %d, %d) ", (int)(W._21*1000), (int)(W._22*1000), (int)(W._23*1000) );
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"   - a W(%d, %d, %d) ", (int)(W._31*1000), (int)(W._32*1000), (int)(W._33*1000) );
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"   - a W(%d, %d, %d) ", (int)(W._41*1000), (int)(W._42*1000), (int)(W._43*1000) );
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"   - out(%d, %d, %d) ", (int)(out.x*1000), (int)(out.y*1000), (int)(out.z*1000) );
	

	// FIXME skuif al die later na 'n XML file
	// FIXME save ook elke skoot se individuele inligting
	float centerHeight = m_target ? m_target->getCenterHeight() : 1.7f;
	f_Vector pos(out.x, out.y-centerHeight, 0); 
	f_Vector abs(fabs(out.x), fabs(out.y-centerHeight), 0); 
	float dist = pos.length();	
	float shotScore = 0;
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"   - a world(%d, %d, %d) center(%d) det(%d)", (int)(worldPos.x*1000), (int)(worldPos.y*1000), (int)(worldPos.z*1000), (int)(centerHeight*1000), (int)(det*1000) );
	if (m_target)
	{
		float scale = m_target->getScale();
		shotScore = m_target->getScoreAtNormalizedPos(pos.x * scale, pos.y * scale);
		m_score += shotScore;
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"   - %s  b score(%d) scale(%d) (%d, %d)", this->m_name.c_str(), (int)shotScore, (int)(scale*1000), (int)(pos.x * scale*1000), (int)(pos.y * scale*1000) );
	}
	else if ( wcsstr( pMOI->getFileName(), L"man") != NULL)
	{
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"   - c");
		if (m_shotInfo.size() == 0) shotScore = 50;
		m_score += shotScore;
	}
	else if ( wcsstr( pMOI->getFileName(), L"man") != NULL)
	{
		if (m_shotInfo.size() == 0) shotScore = 50;
		m_score += shotScore;
	}
	else if ( wcsstr( pMOI->getFileName(), L"Jeromy") != NULL)
	{
		if (m_shotInfo.size() == 0) shotScore = 50;
		m_score += shotScore;
	}
	else if ( wcsstr( pMOI->getFileName(), L"Koos") != NULL)
	{
		if (m_shotInfo.size() == 0) shotScore = 50;
		m_score += shotScore;
	}
	else if ( wcsstr( pMOI->getFileName(), L"Koos_military") != NULL)
	{
		if (m_shotInfo.size() == 0) shotScore = 50;
		m_score += shotScore;
	}
	else if ( wcsstr( pMOI->getFileName(), L"Sipho") != NULL)
	{
		if (m_shotInfo.size() == 0) shotScore = 50;
		m_score += shotScore;
	}
	else if ( wcsstr( pMOI->getFileName(), L"Thabo") != NULL)
	{
		if (m_shotInfo.size() == 0) shotScore = 50;
		m_score += shotScore;
	}
	else if ( wcsstr( pMOI->getFileName(), L"Tom") != NULL)
	{
		if (m_shotInfo.size() == 0) shotScore = 50;
		m_score += shotScore;
	}
	else if ( wcsstr( pMOI->getFileName(), L"kleiduif") != NULL)
	{
		m_score = 1;
		show( L"hide", 0.0f );
	}


	m_grouping = 0;
	for (int i=0; i<m_shotInfo.size(); i++)
	{
		for (int j=1; j<m_shotInfo.size(); j++)
		{
			float dist = f_Vector( m_shotInfo[i].x - m_shotInfo[j].x, m_shotInfo[i].y - m_shotInfo[j].y, 0, 0).length();
			if (dist >m_grouping)m_grouping = dist;
		}
	}


	


	_shotHitInfo S;
	S.x = pos.x;
	S.y = pos.y;
	S.score = shotScore;
	m_shotInfo.push_back( S );
	
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"   - d");
	// calc grouping -----------------------------------------------------------
	m_grouping = 0;
	for (int i=0; i<m_shotInfo.size(); i++)
	{
		for (int j=1; j<m_shotInfo.size(); j++)
		{
			float dist = f_Vector( m_shotInfo[i].x - m_shotInfo[j].x, m_shotInfo[i].y - m_shotInfo[j].y, 0, 0).length();
			if (dist >m_grouping)m_grouping = dist;
		}
	}


	if (m_bScoreOnGrouping)
	{
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_target::bullet_hit    m_bScoreOnGrouping  m_score = 0 grp(%d)", (int)(m_grouping*1000) );
		shotScore = 0;
		m_score = 0;
		if ( m_grouping <= 0.2 )  m_score = 60;
		if ( m_grouping <= 0.15 )  m_score = 80;
		if ( m_grouping <= 0.1 )  m_score = 100;
	}

	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_target::bullet_hit    m_laneGUI.onHit   %s (%d) (%d, %d)score(%d), grouping(%dmm) ", this->m_name, m_lane, (int)(S.x*1000), (int)(S.y*1000), (int)S.score, (int)(m_grouping * 1000));
	dw_scenario::s_pCTRL->m_laneGUI.onHit( m_lane, worldPos, tof, S.x, S.y, (int)S.score, m_score, m_grouping * 1000 );


	bool bCM = true;
}

void dw_target::adjustWeapon()
{
	adjust();
}

void dw_target::adjust()
{
	m_adjustX = 0;
	m_adjustY = 0;

	if (m_shotInfo.size() > 0)
	{
		for (int i=0; i<m_shotInfo.size(); i++)
		{
			m_adjustX -= m_shotInfo[i].x;
			m_adjustY += m_shotInfo[i].y;
		}
		m_adjustX /= m_shotInfo.size();
		m_adjustY /= m_shotInfo.size();
	}

	dw_scenario::s_pCTRL->adjustWeaponOffset( m_lane, m_adjustX, m_adjustY , m_Distance );	
}



// action -----------------------------------------------------------------------------------------------------------------------
dw_action::dw_action( int lane, int type, float F )
{
	m_lane = lane;
	m_type = type;
	switch(m_type)
	{
	case	ACT_LOAD:		m_rounds=(int)F;	break;
	case	ACT_BASECAM:	m_baseCamDistance=F;	break;
	}
}

dw_action::dw_action( int lane, int type, dw_target *T, float x, float y, LPCWSTR cam )
{
	m_lane = lane;
	m_type = type;
	m_pTarget = T;
	switch(m_type)
	{
	case	ACT_SHOWTARGET:	m_x=x;  m_y=y;	m_TargetCam = L"";	break;
	case	ACT_SHOWTARGET3D:	m_x=0;  m_y=0;	m_TargetCam = cam;	break;
	case	ACT_LAUNCHTARGET3D:	m_x=0;  m_y=0;	m_TargetCam = cam;  m_launchSpeed= x;	break;
	case	ACT_ADJUSTWEAPON: break;
	}
}


dw_action::dw_action( int lane, int type, dw_target *T, LPCWSTR ai, int A, float B, float C  )
{
	m_lane = lane;
	m_type = type;
	m_pTarget = T;
	m_AI = ai;
	switch(m_type)
	{
	case	ACT_TARGETPOPUP:	m_repeat=A;	m_tUp=C; m_tDn=B;	break;
	case	ACT_TARGETMOVE:		m_repeat=A; m_dir=B; m_speed=C;				break;
	}
}


dw_action::dw_action( int lane, int type, LPCWSTR S )
{
	//if (type == ACT_ADJUSTWEAPON) 	ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_action( ACT_ADJUSTWEAPON )");

	m_lane = lane;
	m_type = type;
	m_name = S;
}

dw_action::dw_action( int lane, int type, LPCWSTR tgt, LPCWSTR name, int eye, int distance, int rnd, LPCWSTR desc )
{
	m_lane = lane;
	m_type = type;
	m_tgt = tgt;
	m_name = name;
	m_eye = eye;
	m_distance = distance;
	m_rnd = rnd;
	m_desc = desc;
}

dw_action::~dw_action()
{
}


void dw_action::start()
{
	switch(m_type)
	{
	case	ACT_LOAD:		dw_scenario::s_pSCENARIO->loadRounds(m_lane, m_rounds, 0, 0 );		 	break;
	case	ACT_VIDEO:		if (m_lane == 0)dw_scenario::s_pSCENE->PlayVideo( m_name.c_str(), true );	break;
	case	ACT_SOUND:		if (m_lane == 0)
								{
									//dw_scenario::s_pCTRL->playSound( m_name, false );	
									SoundSource *m_pSoundSource = NULL;
									m_pSoundSource = SoundManager::GetSingleton()->createSoundSourceFromFile( m_name.c_str() );
									if (m_pSoundSource) m_pSoundSource->playImmediate();
							}
							break;
	case	ACT_IMAGE:		if (m_lane == 0)dw_scenario::s_pCTRL->gui_LoadImage( m_name.c_str() );	break;
	case	ACT_INTRO:		if (m_lane == 0)dw_scenario::s_pCTRL->gui_showIntro( 0, m_name.c_str(), m_eye, m_rnd, m_tgt.c_str(), m_desc.c_str(), m_distance );	break;
	case	ACT_LANEVIDEO:	;	break;
	case	ACT_CAMERAANIM:	if (m_lane == 0)dw_scenario::s_Cameras.play( m_name );				break;
	case	ACT_BASECAM:	if (m_lane == 0)dw_scenario::s_Cameras.play( m_baseCamDistance );	break;
	case	ACT_SHOWTARGET:	m_pTarget->show( m_x, m_y );									break;
	case	ACT_SHOWTARGET3D:	m_pTarget->show( m_TargetCam, 0.0f );						break;
	case	ACT_LAUNCHTARGET3D:	m_pTarget->show( m_TargetCam, m_launchSpeed );				break;
	case	ACT_SHOWFEEDBACK:	dw_scenario::s_pCTRL->m_laneGUI.showFeedback( m_lane, dw_scenario::s_pCTRL->isLiveFire(), dw_scenario::s_pSCENARIO->getNumRoundsInEx(m_lane) );	break;
	case	ACT_SHOWRANKING:	dw_scenario::s_pCTRL->showRanking( m_lane )	;			break;
	case	ACT_SAVETOSQL:		if (m_lane == 0) dw_scenario::s_pSCENARIO->saveToSQL();	break;

	case ACT_TARGETPOPUP:	m_pTarget->popup( m_repeat, m_tDn, m_tUp );	break;
	case ACT_TARGETMOVE:	m_pTarget->move( m_repeat, m_dir, m_speed );break;

	case ACT_ADJUSTWEAPON:	
		ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"case ACT_ADJUSTWEAPON:");	
		m_pTarget->adjustWeapon( );	
		break;
	}
}


bool dw_action::update( double dTime )
{
	switch(m_type)
	{
	case	ACT_LOAD:		return false;										break;		// dis onmiddelik klaar
	case	ACT_VIDEO:		return dw_scenario::s_pSCENE->videoIsPlaying();		break;
	case	ACT_LANEVIDEO:	;	break;
	case	ACT_CAMERAANIM:	return dw_scenario::s_Cameras.isPlaying();			break;
	case	ACT_BASECAM:	return dw_scenario::s_Cameras.isPlaying();			break;
	}
	return false;
}



// trigger -----------------------------------------------------------------------------------------------------------------------
dw_trigger::dw_trigger( int lane, int type, float time)
{
	m_lane = lane;
	m_type = type;
	m_delay = time;	// fie stoor so dat ek kan restart sonder om dit te verloor
	b_Trigger = false;
}

 dw_trigger::dw_trigger( int lane, int type, float time, float tS, float Score)
 {
	 m_lane = lane;
	m_type = type;
	m_delay = time;	// fie stoor so dat ek kan restart sonder om dit te verloor
	m_bonusTime = tS;
	m_bonusScore = Score;
	b_Trigger = false;
 }

dw_trigger::dw_trigger( int lane, int type, float time, int target)
{
	m_lane = lane;
	m_type = type;
	m_delay = time;	// fie stoor so dat ek kan restart sonder om dit te verloor
	b_Trigger = false;
}
dw_trigger::dw_trigger( int lane, int type, LPCWSTR str)
{
	m_type = type;
	m_str = str;
	b_Trigger = false;
}

dw_trigger::~dw_trigger()
{
}

bool dw_trigger::update( double dTime )
{
	if ( !b_Trigger )
	{
		switch(m_type)
		{
		case TR_TIME: 
			m_delay-= dTime;
			if (m_delay <=0) b_Trigger = true;
			break;
		case TR_ROUNDSCOMPLETED:
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"case TR_ROUNDSCOMPLETED:	 lane(%d)  %d", dw_scenario::s_pSCENARIO->getNumRoundsLeft(m_lane), m_lane );
			if( dw_scenario::s_pSCENARIO->getNumRoundsLeft(m_lane) == 0 )  m_delay-= dTime;
			if((m_delay<=0) && !(dw_scenario::s_pCTRL->isLiveFire()))			// Rounds Completed can only happen in Laser mode, in Live fire we wait for the instructor to go on
			//if((m_delay<=0))			// Rounds Completed can only happen in Laser mode, in Live fire we wait for the instructor to go on
			{
				//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"case TR_ROUNDSCOMPLETED:" );
				b_Trigger = true;
				dw_scenario::s_pCTRL->m_laneGUI.showRoundsCompleted(m_lane);	// FIXME this does nothong				
			}
			break;
		case TR_ALLTARGETSSHOT:
			m_bonusTime  -= dTime;

			if(m_delay>0)
			{
				if( dw_scenario::s_pSCENARIO->getAllTargetsShot(m_lane) == true )  m_delay-= dTime;
				if(m_delay<=0)
				{
					int bonusScore = __max( m_bonusTime, 0) * m_bonusScore;
					dw_scenario::s_pSCENARIO->addBonusScore( m_lane, bonusScore );
					dw_scenario::s_pCTRL->m_laneGUI.showTimeBonus(m_lane, bonusScore);
				}
			}
			else
			{
				m_delay-= dTime;
				if(m_delay<=-2)
				{
					b_Trigger = true;
				}
			}
			break;
		case TR_TARGETSHOT: 
			break;
		case TR_VOICECMD: 
			// gloabl staring sumewhere
			// cheak it, then clear
			//m_delay-= dTime;
			//if (m_delay <=0) b_Trigger = true;
			if (dw_scenario::s_pCTRL->m_lastVoiceCommand.find( L"pull" ) == 0 )
			{
				//MessageBox(NULL, L"VOICE", dw_scenario::s_pCTRL->m_lastVoiceCommand.c_str(), MB_OK | MB_ICONERROR);
				dw_scenario::s_pCTRL->m_lastVoiceCommand = L"";
				b_Trigger = true;
			}
			break;
		}

		if ( b_Trigger )
		{
			foreach( m_actions, start() );
		}
		return true;	// moet nog trigger
	}
	else
	{
		bool b = false;
		for(unsigned int i=0; i<m_actions.size(); i++)		b |= m_actions[i].update( dTime );
		return b;
	}
	return false;
}



// scene -----------------------------------------------------------------------------------------------------------------------
dw_scene::dw_scene( int lane, int wait)
{
	m_lane = lane;
	m_bPauseWhenDone = wait;
	//if (m_bPauseWhenDone == WAIT_NONE) 			ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"scene created with WAIT_NONE" );
	dw_trigger *pT = new dw_trigger( m_lane, TR_TIME, 0.0f);
	m_triggers.push_back( *pT );
}

dw_scene::~dw_scene()
{
}

int dw_scene::update( double dTime )
{
	bool b = false;
	for(unsigned int i=0; i<m_triggers.size(); i++)		b |= m_triggers[i].update( dTime );

	if (b) return SCE_RUNNING;
	else if (m_bPauseWhenDone==WAIT_LANES)		return SCE_WAIT;
	else if (m_bPauseWhenDone==WAIT_INSTRUCTOR) return SCE_WAITNEXT;
	else										return SCE_DONE;
}




// exercise -----------------------------------------------------------------------------------------------------------------------
dw_exercise::dw_exercise( int lane, LPCWSTR name, LPCWSTR description, int maxscore)
{
	m_name = name;
	m_lane = lane;
	m_maxscore = maxscore;
	m_description = description;
	m_shotsFired = 0;
	m_numRounds = 0;
	m_bPrintPictures = false;
	m_eyePosition = -1;
	m_scopePosition = -1;

	m_score = 0;
	m_scoreBonusPoints = 0;
}

dw_exercise::~dw_exercise()
{
}

void dw_exercise::set_Eye( int eyePosition )
{
	m_eyePosition = eyePosition;
}

void dw_exercise::set_SpottingScope( int scope )
{
	m_scopePosition = scope;
}

int dw_exercise::add_target( LPCWSTR type, bool bGRP )	
{	
	dw_target *pT = new dw_target( m_lane, L"stuff", type, NULL, 0, bGRP );
	m_targets.push_back( pT );		
	dw_scenario::s_targets.push_back( pT );	
	return m_targets.size()-1;	
}

int dw_exercise::add_AI_target( LPCWSTR type, LPCWSTR mesh, LPCWSTR startCam, int AI )
{
	dw_target *pT = new dw_target( m_lane, type, mesh, startCam, AI );
	m_targets.push_back( pT );		
	dw_scenario::s_targets.push_back( pT );	
	return m_targets.size()-1;	
}

void dw_exercise::start()
{
	for(unsigned int i=0; i<m_targets.size(); i++) m_targets[i]->start();
	m_currentScene = m_scenes.begin();
	m_score = 0;
	m_scoreBonusPoints = 0;
	m_dTime = 0;

	//dw_scenario::s_pCTRL->m_laneGUI.showLaneSelect();	// aways do this in case its a 1 lane exrcisde
	if (this->m_lane == dw_scenario::s_pSCENARIO->m_pc_lanes-1) dw_scenario::s_pCTRL->m_laneGUI.showLive();
	if ( m_eyePosition >= 0 ) dw_scenario::s_pCTRL->setEyePosition( m_eyePosition );
	if ( m_scopePosition >= 0 ) dw_scenario::s_pCTRL->m_laneGUI.movescope(m_scopePosition);
	
}

void dw_exercise::stop()
{
	for(unsigned int i=0; i<m_targets.size(); i++)
	{
		RakNet::BitStream Bs;
		Bs.Write( m_targets[i]->m_GUID );
		Bs.Write( (unsigned short)AI_CLEAR );
		dw_scenario::s_pSCENE->rpc_call( "set_AI", &Bs );	// FIXME ons misbruik hierdie nog en dis vreeslik haw demo spesefiek

		m_targets[i]->hide();//show(RND_1_1*100, -1000 + RND_1_1*100);		// steek alle teuikens weg
	}
}

int dw_exercise::update( double dTime, bool bNext )
{
	m_dTime += dTime;
	// acumulate all scores and display ------------------------
	m_score = 0;
	if (m_maxscore > 0)
	{
		for (unsigned int i=0; i<m_targets.size(); i++)		m_score += m_targets[i]->getScore();
	}
//	else
//	{
//		ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_exercise::update() m_maxscore == 0 - SCORE diditn update"  );
//	}
	m_score += m_scoreBonusPoints;


	int status = m_currentScene->update( dTime );
	//if ( (status == SCE_DONE) || (bNext && (status & SCE_WAIT)) )
	if ( (status == SCE_DONE) || (bNext) )
	{
		dw_scenario::s_pCTRL->hideOverlay();
		m_currentScene ++;
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_exercise::update() new scene lane %   %X",  m_lane  );
		if (m_currentScene==m_scenes.end()) return EX_DONE;	// we have reached the end
	}
	return status;
}

bool dw_exercise::getAllTargetsShot() 
{
	bool A = true; 
	for (unsigned int i=0; i<m_targets.size(); i++)
	{
		A &= m_targets[i]->hasbeenShot();
	}
	return A;
}




// lane -----------------------------------------------------------------------------------------------------------------------
dw_lane::dw_lane( int lane )
{
	m_lane = lane;
	m_shotTimer = 0;
	m_numRounds = 0;
	m_roundsFired = 0;
	m_magazines = 0;
	m_reloadTime = 0;
	m_reloadCounter = 0;
}

dw_lane::~dw_lane()
{
}

void dw_lane::start()
{
	m_numRounds = 0;
	if (m_exercises.size() > 0) m_exercises[0].start();
	pdf_Start();
}

int dw_lane::update( double dTime, bool bNext )
{
	m_shotTimer += dTime;
	if( m_currentExercise!=m_exercises.end() )
	{
		// Check and Do magazine Change -----------------------------------------------------------------------
		
		if (((m_numRounds - m_roundsFired)<=0) && (m_magazines>0))
		{
			if (m_reloadCounter == 0.0f)	
			{
				dw_scenario::s_pCTRL->gui_LoadImage( L"MAG.png" );
			}

			m_reloadCounter+=dTime;

			if (m_reloadCounter > m_reloadTime)
			{
				dw_scenario::s_pCTRL->hideOverlay();
				m_magazines --;
				m_numRounds = m_numMAGrounds;
				m_reloadCounter = 0;
				m_roundsFired = 0;
			}
		}

		//dw_scenario::s_pCTRL->zigbeeRounds(m_lane, m_numRounds - m_roundsFired);
		//dw_scenario::s_pCTRL->m_laneGUI.setShotcount( m_lane, m_numRounds - m_roundsFired );

		int status = m_currentExercise->update( dTime, bNext );
		m_bSceneRunning = status&SCE_RUNNING;

		if ( status==EX_DONE )
		{
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_lane::update()  lane %d, EX_DONE", m_lane);
			dw_scenario::s_pCTRL->paperAdvance();
			
			m_currentExercise->stop();
			pdf_Exercise( *m_currentExercise );
			m_currentExercise ++;

			if (m_currentExercise==m_exercises.end())	
			{
				pdf_Header();
				pdf_Stop();
				return EX_DONE;	// we have reached the end
			}
			else										m_currentExercise->start();	return SCE_RUNNING;
		}

		int score = 0;
		for (int i=0; i< m_exercises.size(); i++)	score += m_exercises[i].m_score;
		dw_scenario::s_pCTRL->m_laneGUI.setScore( m_lane, score );

		if ( m_currentExercise->m_shotsFired != this->m_roundsFired )
		{
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_lane::update()  lane %d, m_roundsFired %d", m_lane, m_roundsFired);
			m_currentExercise->m_shotsFired = this->m_roundsFired;
		}


		return status;
	}
	return EX_DONE;
}
/*
int dw_lane::addShot( int x, int y )
{ 
	if (m_bSceneRunning)
	{
		float shotDelay = 0.1;		// vir galil - 0.2 is vir skou geweer

		if ( m_shotTimer>shotDelay )	// we pass the timing delay
		{
			if (m_numRounds > 0)
			{
				if ( m_roundsFired<m_numRounds || dw_scenario::s_pCTRL->isLiveFire() )		// If live fire we do not block, we allow more shots
				{
					m_shotTimer = 0.0f;
					m_roundsFired ++; 
					//dw_scenario::s_pCTRL->m_laneGUI.setShotcount( m_lane, m_roundsFired );
					return (m_roundsFired-1);
				}
			}
		}
		else
		{
			return -2;	// -2 beteken dat dit die times is wat die skoot geblok het
		}
	}
	else
	{
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"shot but not in a scene  XXXXXX  lane %d", m_lane);
	}
	return -1;  // negatief beteken die skoot tel nie
}
*/

#ifdef HPDF_DLL
void  __stdcall
#else
void
#endif
error_handler (HPDF_STATUS   error_no,
               HPDF_STATUS   detail_no,
               void         *user_data)
{
	//printf ("ERROR: error_no=%04X, detail_no=%u\n",
	//  (HPDF_UINT)error_no, (HPDF_UINT)detail_no);
	// throw std::exception ();
	ew_Scene::GetSingleton()->getLogger()->Log(LOG_ERROR, L"PDF error" );
}

int pdf_Line(HPDF_Page page, int x, int y, int x2, int y2)
{
	HPDF_Page_SetRGBStroke( page, 0.0, 0.0, 0.0 );
	HPDF_Page_SetLineWidth (page, 0.3);
	HPDF_Page_MoveTo (page, x, HPDF_Page_GetHeight (page) - y);
	HPDF_Page_LineTo (page, x2, HPDF_Page_GetHeight (page) - y2);
	HPDF_Page_Stroke (page);
	return 0;
}


enum { align_LEFT, align_MIDDLE, align_RIGHT };
int pdf_Text(HPDF_Page page, HPDF_Font font, int x, int y, string txt, float size, int alignment )
{
	HPDF_Page_SetFontAndSize (page, font, size);
	HPDF_Page_SetWordSpace( page, 2 );
	int w = HPDF_Page_TextWidth (page, txt.c_str());
	switch (alignment)
	{
	case align_LEFT:
		break;
	case align_MIDDLE:
		x = x - (w/2);
		break;
	case align_RIGHT:
		x = x - w;
		break;
	}

	HPDF_Page_BeginText (page);
		HPDF_Page_MoveTextPos (page, x, HPDF_Page_GetHeight (page) - y);
		HPDF_Page_ShowText (page, txt.c_str());
	HPDF_Page_EndText (page);
	
	return 	w;	// return width
}

void pdf_PNG( HPDF_Doc  pdf, HPDF_Page page, string name, int x, int y, int w, int h)
{
	HPDF_Image image = HPDF_LoadPngImageFromFile (pdf, name.c_str());
	if (image)
	{
		HPDF_Page_DrawImage (page, image, x, HPDF_Page_GetHeight (page) - y, w, h);
	}
}

void pdf_JPG( HPDF_Doc  pdf, HPDF_Page page, string name, int x, int y, int w, int h)
{
	HPDF_Image image = HPDF_LoadJpegImageFromFile (pdf, name.c_str());
	if (image)
	{
		HPDF_Page_DrawImage (page, image, x, HPDF_Page_GetHeight (page) - y, w, h);
	}
}

int pdf_Circle(HPDF_Page page, float x, float y, float r)
{
	HPDF_Page_SetRGBStroke( page, 0.0, 0.0, 0.0 );
	HPDF_Page_SetLineWidth (page, 1.0);
	HPDF_Page_Circle (page, x , HPDF_Page_GetHeight (page) - y , r);
	HPDF_Page_Stroke (page);
	return 0;
}

int pdf_CircleWhite(HPDF_Page page, float x, float y, float r)
{
	HPDF_Page_SetRGBStroke( page, 1.0, 1.0, 1.0 );
	HPDF_Page_SetLineWidth (page, 2.0);
	HPDF_Page_Circle (page, x , HPDF_Page_GetHeight (page) - y , r);
	HPDF_Page_Stroke (page);
	return 0;
}


void dw_lane::pdf_Start()
{
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"pdf_Start(  )");

	m_PDF = HPDF_New ( error_handler, NULL );
	m_pdf_Font = HPDF_GetFont ( m_PDF, "Helvetica", NULL );
	m_pdf_Page = HPDF_AddPage ( m_PDF );
	m_pdf_Height = HPDF_Page_GetHeight ( m_pdf_Page );
	m_pdf_Width = HPDF_Page_GetWidth ( m_pdf_Page );

	m_pdf_totalMaxScore = 0;
	m_pdf_totalScore = 0;
	m_pdf_X = 20;
	m_pdf_Y = 80;
}

static int PRINT_CNT = 0;
void dw_lane::pdf_Stop()
{
	//return;
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"pdf_Stop(  )");
	ConfigManager		*m_pCfgMng = ew_Scene::GetSingleton()->getConfigManager();
	ewstring imgDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "/media/controllers/DirectWeapons3/targets/printImages/");
	ewstring prntDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "\\printing\\");

	if (this->getNumRoundsFired())
	//if ( m_pdf_totalScore > 0 )
	{
		_laneInfo laneInfo = dw_scenario::s_pCTRL->getLane( m_lane );

		m_PrintCommand = prntDIR;
		m_PrintCommand += ewstring( laneInfo.userNAME.c_str() );
		m_PrintCommand += "_";
		m_PrintCommand += ewstring( PRINT_CNT );
		m_PrintCommand += ".pdf";
		HPDF_SaveToFile ( m_PDF, m_PrintCommand.c_str() );

		HPDF_Free( m_PDF );

		PRINT_CNT ++;


		ewstring sys = "del " + prntDIR + "*.jpg";
		system( sys.c_str() );

		if (dw_scenario::s_pCTRL->m_bAutoPrint)
		{
			ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_lane::pdf_Stop() Print( )  ... autoprint is TRUE");

			pdf_Print();
		}
	}

}

void dw_lane::pdf_Print()
{
	ConfigManager		*m_pCfgMng = ew_Scene::GetSingleton()->getConfigManager();
	ewstring prntDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "\\printing\\");
	ewstring sys = prntDIR + "printme.bat";
	FILE *file = fopen( sys.c_str(), "w" );
	if (file)
	{
					
		fprintf( file, "\"%s\" /p \"%s\" ", ((*m_pCfgMng)[L"Earthworks_3"][L"ACROBAT_DIR"]).c_str(), m_PrintCommand.c_str() );
		fclose( file );
	}
	system( sys.c_str() );
}

void dw_lane::pdf_Header()
{
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"pdf_Header(  )");
	_laneInfo laneInfo = dw_scenario::s_pCTRL->getLane( m_lane );

	// header --------------------------------------------------------
	pdf_Line( m_pdf_Page, m_pdf_Width/2, 90, m_pdf_Width/2, m_pdf_Height - 70);
	pdf_Line( m_pdf_Page, 0, 60, m_pdf_Width, 60);
	pdf_Text( m_pdf_Page, m_pdf_Font, 20, 30, ewstring( laneInfo.userNAME.c_str() ), 18, align_LEFT );
	//if (laneInfo.userNAME.length() == 0)  pdf_Text( m_pdf_Page, m_pdf_Font, 20, 20, "Lane 1", 10, align_LEFT );
			
	ewstring txt = ewstring( dw_scenario::s_pSCENARIO->getName().c_str() );
	pdf_Text( m_pdf_Page, m_pdf_Font, 20, 50, txt, 10, align_LEFT );

	txt = ewstring( laneInfo.ammo.name );
	pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_Width/2+0, 35, txt, 10, align_LEFT );
	if ( dw_scenario::s_pCTRL->isLiveFire() )	pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_Width/2+0, 50, "LIVE FIRE", 10, align_LEFT );
	else										pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_Width/2+0, 50, "LASER", 10, align_LEFT );

	

	if (m_pdf_totalMaxScore>0)		txt = ewstring( m_pdf_totalScore ) + " / " + ewstring( m_pdf_totalMaxScore );
	else							txt = ewstring( m_pdf_totalScore );
	pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_Width-20, 35, txt, 20, align_RIGHT );
			
			

	// footer --------------------------------------------------------
	//pdf_Line( page, 0, pageHeight-50, pageWidth, pageHeight-50);
	pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_Width*0.65, m_pdf_Height-20, "Instructor ", 10, align_RIGHT );
	pdf_Line( m_pdf_Page, m_pdf_Width*0.65, m_pdf_Height-20, m_pdf_Width*0.95, m_pdf_Height-20);

	time_t currentTime;
	tm locTime;
	memset(&currentTime, 0, sizeof(time_t));
	time(&currentTime);
	errno_t err = localtime_s( &locTime, &currentTime );
	txt = ewstring(locTime.tm_year + 1900) + ewstring(" / ");
	if ((locTime.tm_mon + 1) < 10)	txt += "0" + ewstring(locTime.tm_mon + 1) + ewstring(" / ");
	else							txt += ewstring(locTime.tm_mon + 1) + ewstring(" / ");
	if (locTime.tm_mday < 10)		txt += "0" + ewstring(locTime.tm_mday);	
	else							txt += ewstring(locTime.tm_mday);	
	pdf_Text( m_pdf_Page, m_pdf_Font, 20, m_pdf_Height-20, txt, 10, align_LEFT );
}

void dw_lane::pdf_Exercise( dw_exercise ex )
{
	if (ex.m_maxscore == 0) return;
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"pdf_Exercise(  )");
	ConfigManager		*m_pCfgMng = ew_Scene::GetSingleton()->getConfigManager();
	ewstring imgDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "/media/controllers/DirectWeapons3/targets/printImages/");
	ewstring prntDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "\\printing\\");

	long exerciseID;
	ewstring txt = ewstring(ex.m_name);
	pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_X, m_pdf_Y, txt.c_str(), 10, align_LEFT );

	
	if ( ex.m_bPrintPictures )
	{
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L" this in is PRINT AS PICTURES lane (%d)  shots(%d)", m_lane, dw_scenario::s_pCTRL->m_laneGUI.m_LANES[m_lane].m_Shots.size() );
		int dX = m_pdf_X;
		int totalscore = 0;
		for (int m=0; m<dw_scenario::s_pCTRL->m_laneGUI.m_LANES[m_lane].m_Shots.size(); m++)
		{
			ewstring shotIMG = prntDIR + ewstring(PRINT_CNT) + "_" + ewstring(m_lane) + "_"  + ewstring(m) + ".jpg";
			D3DXSaveTextureToFile( shotIMG.getWString().c_str(), D3DXIFF_JPG, dw_scenario::s_pCTRL->m_laneGUI.m_LANES[m_lane].m_Shots[m].m_pTexture, NULL );
			pdf_JPG( m_PDF, m_pdf_Page, shotIMG, dX, m_pdf_Y + 50, 40, 40);
			//pdf_CircleWhite( m_pdf_Page, dX+20, m_pdf_Y -20, 2);
			pdf_Circle( m_pdf_Page, dX+20, m_pdf_Y -20 + 50, 2);
			//txt = ewstring( dw_scenario::s_pCTRL->m_laneGUI.m_LANES[m_lane].m_Shots[m].m_bHIT );
			//pdf_Text( m_pdf_Page, m_pdf_Font, dX+40, m_pdf_Y + 60, txt.c_str(), 10, align_MIDDLE );

			totalscore += dw_scenario::s_pCTRL->m_laneGUI.m_LANES[m_lane].m_Shots[m].m_score;
			if (dw_scenario::s_pCTRL->m_laneGUI.m_LANES[m_lane].m_Shots[m].m_bHIT )
			{
				pdf_Text( m_pdf_Page, m_pdf_Font, dX+20, m_pdf_Y + 60, "hit", 10, align_MIDDLE );
			}
			else
			{
				pdf_Text( m_pdf_Page, m_pdf_Font, dX+20, m_pdf_Y + 60, "miss", 10, align_MIDDLE );
			}
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"pdf_JPG( %s, %d, %d );", shotIMG.getWString().c_str(), dX, m_pdf_Y  );

			//txt = ewstring(m+1) + " : " + ewstring( s_pCTRL->m_laneGUI.m_LANES[i].m_Shots[m].m_time, 1 ) + "s (" + ewstring( s_pCTRL->m_laneGUI.m_LANES[i].m_Shots[m].m_dT, 1  ) + "s)";
			//pdf_Text( page, x, y+15, txt, 10, align_LEFT );

			
			if ( (dX-m_pdf_X) > (m_pdf_Width/2 - 70))
			{
				dX = m_pdf_X;
				m_pdf_Y += 60;
			}

			if (m_pdf_Y>(m_pdf_Height-50-80))
			{
				m_pdf_Y = 80;
				m_pdf_X = m_pdf_Width/2 + 20;
				dX = m_pdf_X;
			}

			dX += 45;
		}

		if ( ex.m_maxscore > 0 )
		{
			m_pdf_totalScore += totalscore;
			m_pdf_totalMaxScore += ex.m_maxscore;
		}

		if (ex.m_maxscore>0)	txt = ewstring( totalscore ) + " / " + ewstring( ex.m_maxscore );
		else					txt = ewstring( totalscore );
		pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_X + 20, m_pdf_Y + 80, txt, 12, align_MIDDLE );

		m_pdf_Y -= 30;
	}
	else 
	{
		for (int k = 0; k < ex.m_targets.size(); k++)
		{
			if (ex.m_targets[k]->m_target)
			{
				ewstring target = imgDIR + "target_" + ewstring( ex.m_targets[k]->m_target->getSpriteFile() ) + ".png";
				//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L" %s", target.getWString().c_str());
				pdf_PNG( m_PDF, m_pdf_Page, target, m_pdf_X, m_pdf_Y + 10 + 100, 100, 100);
				pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_X+150, m_pdf_Y+20, "distance : ", 6, align_RIGHT );
				pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_X+150, m_pdf_Y+40, "shots : ", 6, align_RIGHT );
				pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_X+150, m_pdf_Y+60, "score : ", 6, align_RIGHT );
				pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_X+150, m_pdf_Y+80, "grouping : ", 6, align_RIGHT );
				

				// Live fire normalise scores  ----------------------------------------------------------
				int newScore = (int)ex.m_targets[k]->m_score;
				int newShots = ex.m_shotsFired;
				//if ( dw_scenario::s_pCTRL->isLiveFire() && (ex.m_targets.size() == 1) )		// only live fire single target exercises
				if ( dw_scenario::s_pCTRL->isLiveFire() )
				{
					int A = ex.m_numRounds / 2;
					int B = ex.m_numRounds * 2;
					if ( (ex.m_shotsFired > A) && (ex.m_shotsFired < B) )
					{
						newScore = (int)( ex.m_targets[k]->m_score * ex.m_numRounds / ex.m_shotsFired);
						newShots = ex.m_numRounds;
						//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print...SHOT CORRECTTION (%d, %d) score (%d, %d)", ex.m_shotsFired, ex.m_numRounds, ex.m_targets[k]->m_score, newScore );
					}
					/*
					ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print...SHOT CORRECTTION (%d, %d)", ex.m_shotsFired, ex.m_numRounds );
					if ((ex.m_shotsFired - ex.m_numRounds) > 3)  ;
					else if ((ex.m_shotsFired - ex.m_numRounds) > 0) 
					{
						newScore = (int)( ex.m_targets[k]->m_score * ex.m_numRounds / ex.m_shotsFired);
						newShots = ex.m_numRounds;
						dw_scenario::s_pCTRL->m_correctionsDOWN ++;
					}
					else if ((ex.m_shotsFired - ex.m_numRounds) < -5);
					else if ((ex.m_shotsFired - ex.m_numRounds) < 0) 
					{
						newScore = (int)( ex.m_targets[k]->m_score * ex.m_numRounds / ex.m_shotsFired);
						newShots = ex.m_numRounds;
						dw_scenario::s_pCTRL->m_correctionsUP ++;
					}
					else
					{
						dw_scenario::s_pCTRL->m_correctionsNONE ++;
					} */
				}

				if ( dw_scenario::s_pCTRL->isLiveFire() )	dw_scenario::s_pCTRL->m_TotalShotsLIVE += ex.m_shotsFired;
				else										dw_scenario::s_pCTRL->m_TotalShotsLASER += ex.m_shotsFired;

				txt = ewstring( (int)ex.m_targets[k]->m_Distance ) + " m";
				pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_X+150, m_pdf_Y+20, txt, 12, align_LEFT );
				txt = ewstring( newShots );
				pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_X+150, m_pdf_Y+40, txt, 12, align_LEFT );
				if (ex.m_maxscore>0)	txt = ewstring( newScore ) + " / " + ewstring( ex.m_maxscore );
				else					txt = ewstring( newScore );
				pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_X+150, m_pdf_Y+60, txt, 12, align_LEFT );
				txt = ewstring( ex.m_targets[k]->m_grouping*100, 1 ) + " cm";
				pdf_Text( m_pdf_Page, m_pdf_Font, m_pdf_X+150, m_pdf_Y+80, txt, 12, align_LEFT );

				if ( ex.m_maxscore > 0 )
				{
					m_pdf_totalScore += newScore;
					m_pdf_totalMaxScore += ex.m_maxscore;
				}

				for (int l = 0; l < ex.m_targets[k]->m_shotInfo.size(); l++)
				{
					float scale = ex.m_targets[k]->m_Scale * 50;
					_shotHitInfo shot = ex.m_targets[k]->m_shotInfo[l];
					pdf_Circle( m_pdf_Page, m_pdf_X+50 + shot.x*scale, m_pdf_Y + 10 + 50 - shot.y*scale, 2);
						
				}
			}
		}
	}

	m_pdf_Y+= 140;
	if (m_pdf_Y>(m_pdf_Height-50-140))
	{
		m_pdf_Y = 80;
		m_pdf_X = m_pdf_Width/2 + 20;
	}
}




// scenario -----------------------------------------------------------------------------------------------------------------------
dw_scenario::dw_scenario()
{
	m_pScene = ew_Scene::GetSingleton();
	dw_scenario::s_pSCENARIO = this;
	m_maxScore = 10000;							// FIXME ?
}


dw_scenario::~dw_scenario()
{
	m_lanes.clear();
}


void  dw_scenario::init(int numLanes)
{
	m_pSoundSource = NULL;

	m_pc_lanes = numLanes;
	for (int i=0; i<m_pc_lanes; i++) 
	{
		dw_lane L(i);
		m_lanes.push_back( L );
	}

	LUAManager *pLUAManager = LUAManager::GetSingleton();
	pLUAManager->registerClassMethod( "create_terrainScenario",		*this,	&dw_scenario::create_terrainScenario );
	pLUAManager->registerClassMethod( "create_modelScenario",		*this,	&dw_scenario::create_modelScenario );
	pLUAManager->registerClassMethod( "create_speculativeScenario",	*this,	&dw_scenario::create_speculativeScenario );
	pLUAManager->registerClassMethod( "load_Sky",					*this,	&dw_scenario::load_Sky );
	pLUAManager->registerClassMethod( "load_Cameras",				*this,	&dw_scenario::load_Cameras );
	pLUAManager->registerClassMethod( "load_Soundtrack",			*this,	&dw_scenario::load_Soundtrack );
	pLUAManager->registerClassMethod( "load_Soundtrack",			*this,	&dw_scenario::load_Soundtrack );
	pLUAManager->registerClassMethod( "maxLanes",					*this,	&dw_scenario::maxNumLanes);
	pLUAManager->registerClassMethod( "createMyself",				*this,	&dw_scenario::createMyself);

	pLUAManager->registerClassMethod( "exercise",					*this,	&dw_scenario::add_exercise );
	pLUAManager->registerClassMethod( "backdrop",					*this,	&dw_scenario::add_backdrop );
	pLUAManager->registerClassMethod( "target",						*this,	&dw_scenario::add_target );
	pLUAManager->registerClassMethod( "targetGrouping",				*this,	&dw_scenario::add_targetGRP );
	pLUAManager->registerClassMethod( "target_L",					*this,	&dw_scenario::add_lane_target );
	pLUAManager->registerClassMethod( "target_AI",					*this,	&dw_scenario::add_AI_target );
	pLUAManager->registerClassMethod( "target_lane_AI",				*this,	&dw_scenario::add_lane_AI_target );
	pLUAManager->registerClassMethod( "scene",						*this,	&dw_scenario::add_scene );
	pLUAManager->setGlobal( "WAIT_NONE",							(int)WAIT_NONE );
	pLUAManager->setGlobal( "WAIT_LANES",							(int)WAIT_LANES );
	pLUAManager->setGlobal( "WAIT_INSTRUCTOR",						(int)WAIT_INSTRUCTOR );

	pLUAManager->setGlobal( "EYE_PRONE",							(int)eye_PRONE );
	pLUAManager->setGlobal( "EYE_SIT",								(int)eye_SIT );
	pLUAManager->setGlobal( "EYE_KNEEL",							(int)eye_KNEEL );
	pLUAManager->setGlobal( "EYE_STAND",							(int)eye_STAND );
	pLUAManager->registerClassMethod( "set_Eye",					*this,	&dw_scenario::set_Eye );

	pLUAManager->setGlobal( "SPOT_HIDE",							(int)spotting_HIDE );
	pLUAManager->setGlobal( "SPOT_TOP",								(int)spotting_TOP );
	pLUAManager->setGlobal( "SPOT_TOPLEFT",							(int)spotting_TOPLEFT );
	pLUAManager->setGlobal( "SPOT_BOTTOM",							(int)spotting_BOTTOM );
	pLUAManager->setGlobal( "SPOT_BOTTOMLEFT",						(int)spotting_BOTTOMLEFT );
	pLUAManager->registerClassMethod( "set_SpottingScope",			*this,	&dw_scenario::set_SpottingScope );


	pLUAManager->setGlobal( "MOVE_LEFT",							(int)MOVE_LEFT );
	pLUAManager->setGlobal( "MOVE_RIGHT",							(int)MOVE_RIGHT );
	pLUAManager->setGlobal( "MOVE_FRONT",							(int)MOVE_FRONT );
	pLUAManager->setGlobal( "MOVE_BACK",							(int)MOVE_BACK );

	pLUAManager->registerClassMethod( "tr_time",					*this,	&dw_scenario::tr_time );
	pLUAManager->registerClassMethod( "tr_roundsCompleted",			*this,	&dw_scenario::tr_roundsCompleted );
	pLUAManager->registerClassMethod( "tr_allTargetsShot",			*this,	&dw_scenario::tr_allTargetsShot );
	pLUAManager->registerClassMethod( "tr_targetShot",				*this,	&dw_scenario::tr_targetShot );
	pLUAManager->registerClassMethod( "tr_voiceCMD",				*this,	&dw_scenario::tr_voiceCMD );

	pLUAManager->registerClassMethod( "act_Video",					*this,	&dw_scenario::act_Video );
	pLUAManager->registerClassMethod( "act_Sound",					*this,	&dw_scenario::act_Sound );
	pLUAManager->registerClassMethod( "act_Image",					*this,	&dw_scenario::act_Image );
	pLUAManager->registerClassMethod( "act_Intro",					*this,	&dw_scenario::act_Intro );
	pLUAManager->registerClassMethod( "act_Camera",					*this,	&dw_scenario::act_Camera );
	pLUAManager->registerClassMethod( "act_BasicCamera",			*this,	&dw_scenario::act_BasicCamera );
	pLUAManager->registerClassMethod( "act_Loadrounds",				*this,	&dw_scenario::act_Loadrounds );
	pLUAManager->registerClassMethod( "printAsPictures",			*this,	&dw_scenario::printAsPictures );
	pLUAManager->registerClassMethod( "LoadMagazines",				*this,	&dw_scenario::LoadMagazines );
	pLUAManager->registerClassMethod( "act_ShowTarget",				*this,	&dw_scenario::act_ShowTarget );
	pLUAManager->registerClassMethod( "act_ShowTarget3D",			*this,	&dw_scenario::act_ShowTarget3D );
	pLUAManager->registerClassMethod( "act_LaunchTarget3D",			*this,	&dw_scenario::act_LaunchTarget3D );
	pLUAManager->registerClassMethod( "act_ShowFeedback",			*this,	&dw_scenario::act_ShowFeedback );
	pLUAManager->registerClassMethod( "act_ShowRanking",			*this,	&dw_scenario::act_ShowRanking );
	pLUAManager->registerClassMethod( "act_SavetoSQL",				*this,	&dw_scenario::act_SavetoSQL );
	pLUAManager->registerClassMethod( "act_TargetPopup",			*this,	&dw_scenario::act_TargetPopup );
	pLUAManager->registerClassMethod( "act_TargetMove",				*this,	&dw_scenario::act_TargetMove );
	pLUAManager->registerClassMethod( "act_AdjustWeapon",			*this,	&dw_scenario::act_AdjustWeapon );
	//pLUAManager->registerClassMethod( "act_LoadSpeculative",		*this,	&dw_scenario::act_LoadSpeculative );

	pLUAManager->setGlobal( "force_Own",				(int)force_Own );
	pLUAManager->setGlobal( "force_Enemy",				(int)force_Enemy );
	pLUAManager->setGlobal( "force_Neutral",			(int)force_Neutral );

	pLUAManager->setGlobal( "AI_CLEAR",					(int)AI_CLEAR );
	pLUAManager->setGlobal( "AI_TARGET_POPUP",			(int)AI_TARGET_POPUP );
	pLUAManager->setGlobal( "AI_TARGET_MOVE",			(int)AI_TARGET_MOVE );
	pLUAManager->setGlobal( "AI_HAWE_DEMO",				(int)AI_HAWE_DEMO );
	pLUAManager->setGlobal( "AI_ANTELOPE",				(int)AI_ANTELOPE );
	pLUAManager->setGlobal( "AI_KLEIDUIF",				(int)AI_KLEIDUIF );
	pLUAManager->setGlobal( "AI_GUNFIGHT",				(int)AI_GUNFIGHT );
	pLUAManager->setGlobal( "AI_GUNFIGHT_WIDE_KNEEL",	(int)AI_GUNFIGHT_WIDE_KNEEL );
	pLUAManager->setGlobal( "AI_GUNFIGHT_SMALL_STAND",	(int)AI_GUNFIGHT_SMALL_STAND );
	pLUAManager->setGlobal( "AI_GUNFIGHT_SMALL_KNEEL",	(int)AI_GUNFIGHT_SMALL_KNEEL );
	pLUAManager->setGlobal( "AI_GUNFIGHT_SMALL_HIDE",	(int)AI_GUNFIGHT_SMALL_HIDE );

	// moving target types
	pLUAManager->setGlobal( "MOVING_LR",					(int)QR_MOVINGLR );
	pLUAManager->setGlobal( "MOVING_RL",					(int)QR_MOVINGRL );
	pLUAManager->setGlobal( "MOVING_CHARGE",				(int)QR_CHARGE );
	pLUAManager->setGlobal( "MOVING_ZIGZAG",				(int)QR_ZIGZAG );
	pLUAManager->setGlobal( "MOVING_ATTACK",				(int)QR_ATTACK );
	pLUAManager->setGlobal( "MOVING_RANDOM",				(int)QR_RANDOM );

}


bool  dw_scenario::testTargetsLoaded()
{
	// scan MOI for targets coming back over the network
	// FIXME AL HIER DIE moet mooi oorgeskryf word
	int cnt = 0;
	for (int i=0; i<1024; i++)	{ if (s_pSCENE->m_pMOI[i].pMOI) cnt++; }

	for (int i=0; i<1024; i++)	
	{ 
		if ( s_pSCENE->m_pMOI[i].pMOI && s_pSCENE->m_pMOI[i].pMOI->getWorldMatrix()->m[2][0] > 1500 )
		{
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L" ----->   guid %d   %d, {%X}",  i, s_pSCENE->m_pMOI[i].pMOI->getWorldMatrix()->m[2][0], s_pSCENE->m_pMOI[i].pMOI  );
		}
	}
	
	if (cnt != NUM_MOI)
	{	
		// we hae new ones
		NUM_MOI = cnt;
		for (int i=0; i<1024; i++)
		{
			s_targetGUIDs[i] = NULL;
			if (s_pSCENE->m_pMOI[i].pMOI)
			{
				for (int j=0; j<s_targets.size(); j++ )
				{
					if (s_pSCENE->m_pMOI[i].name == s_targets[j]->m_name)
					{
						s_targetGUIDs[i] = s_targets[j];
						s_targets[j]->m_GUID = i;
						s_targets[j]->pMOI = s_pSCENE->m_pMOI[i].pMOI;
						//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L".... found a target   guid %d   %s, %d, {%X}",  i, s_targets[j]->m_name.c_str(), cnt, s_targets[j]->pMOI  );
					}
				}
			}
		}
	}

	// Toets of die teikens klaar gelaai het
	for (int j=0; j<s_targets.size(); j++ )
	{
		if ( ! s_targets[j]->pMOI ) return false;
	}
	return true;
}


void dw_scenario::next() 
{	
	if ( m_LoadDelay <= 0 ) foreach( m_lanes, update( 0, true ) ); 
}

bool dw_scenario::update( double dTime )
{
	if ( !testTargetsLoaded() )  return m_bPlaying;
	else if (m_LoadDelay > 0)
	{
		m_LoadDelay -= dTime;
		if (m_LoadDelay <= 0)	foreach( m_lanes, start() );	// FIXME ons wil heirdie he om triggers te reset, en dalk ook as ons RND erns gebruik om weer te RND
		return m_bPlaying;
	}
	

	for(unsigned int i=0; i<m_pc_lanes; i++)		
	{		
		dw_scenario::s_pCTRL->m_laneGUI.setTotalScore( i, getTotalScore( i ) );
	}

	if (dTime >0.033) dTime = 0.033;
	int status = 0;
	if (m_bPlaying)
	{
		for(unsigned int i=0; i<m_pc_lanes; i++)		
		{
			status |= m_lanes[i].update( dTime, false );
		}
		if ( !(status&SCE_RUNNING) )
		{
			// none of the lanes are still running their current scene
			if ( status&EX_DONE )
			{
				//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"if ( status&EX_DONE )" );
				m_bPlaying=false;	// all are done
				// Insert hier kode waty print opsies vertoon, ens - d.w.s laneGUI->showFinal
				dw_scenario::s_pCTRL->m_laneGUI.showFinal();
			}
			if ( status & SCE_WAIT )
			{
				// so all are waiting
				for(unsigned int i=0; i<m_lanes.size(); i++)		m_lanes[i].update( 0, true );
			}
		}
	
		if( s_Cameras.update( dTime ) )
		{
			m_pScene->getCamera()->setMatrix( *s_Cameras.getMatrix() );
			if( s_Cameras.getFOV() > 0 )
			{
				m_pScene->getCamera()->setFOV( s_Cameras.getFOV() );		// so FOV==0 makes it determined by the lane
			}
			
			if (s_pCTRL->m_bInstructorStation)
			{
				BitStream BS;
				BS.Write( (unsigned short)RPC_CAMERA );
				writedMatrix( BS, (*s_Cameras.getMatrix()) );
				BS.Write( (float)s_Cameras.getFOV() );
				m_pScene->rpc_broadcast( "rpc_CALL", &BS );
			}
		}		
	}
	else //if(m_bLoopingScenario)
	{
		//stop();
		//dw_scenario::s_pCTRL->goMenu();
		//::this->start();
	}
	m_bLoopingScenario = true;

	return m_bPlaying;
}

void dw_scenario::SQLError(const char *msg)
{
	MessageBoxA(NULL, msg, "Error", MB_OK | MB_ICONERROR);
}

bool compareUser(_user u1, _user u2)
{
	return u1.bestScore > u2.bestScore;
}

void dw_scenario::saveToSQL()
{
	//m_SQLName  is the exercise session name to use
	if (!m_SQLName.length())
		return;
	//FIXME gebruik die Scenario SQL manager eerder as om weer te conenct
	MySQLManager *mysql = s_pCTRL->getMySQLManager();
	bool dbConnect;
	if (dbConnect = mysql->connect())
	{
		//MessageBox(NULL, L"Starting save to SQL", L"Error", MB_OK);
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Starting save to SQL, %s, %s", m_SQLName.c_str(), m_SQLGroup.c_str() );
		string query = "";
		MySQLInput input;
		vector<mysqlpp::Row> result;
		for (int i = 0; i < get_numLanes(); i++)
		{
			dw_lane lane = m_lanes[i];
			_laneInfo laneInfo = s_pCTRL->getLane(i);

			if (!laneInfo.userID.length())
				continue;

			query = "SELECT id FROM exerciseSessionDefinition WHERE groupID IN (SELECT id FROM exerciseSessionDefinitionGroup WHERE name = %2q:groupName AND applicationID = (SELECT id FROM applications WHERE name = %3q:appName)) AND name = %1q:name;";
			input.clear();
			result.clear();
			input["name"] = m_SQLName.getMBString().c_str();
			input["groupName"] = m_SQLGroup.getMBString().c_str();
			input["appName"] = APP_NAME;
			if (!mysql->getObjects(query.c_str(), input, &result))
			{
				SQLError(mysql->getLastError());
				return;
			}
			if (result.size() == 0 )
			{
				ew_Scene::GetSingleton()->getLogger()->Log(LOG_ERROR, L"SQL - Could not find the exercise in the database" );
				SQLError( "Could not find the exercise in the database" );
				return;
			}
			long defID = result[0]["id"];

			//MessageBox(NULL, L"SQL id for user", L"Error", MB_OK);
			//query = "SELECT id FROM exerciseSession WHERE userID = %1q:userID AND exerciseSessionDefinitionID = %2q:definitionID AND totalScore = 0 ORDER BY sessionDateTime LIMIT 1;";
			query = "SELECT id FROM exerciseSession WHERE userID = %1q:userID AND exerciseSessionDefinitionID = %2q:definitionID AND totalScore = -1 ORDER BY sessionDateTime DESC LIMIT 1;";
			input.clear();
			result.clear();
			input["userID"] = laneInfo.userID.getMBString().c_str();
			input["definitionID"] = (int)defID;
			if (!mysql->getObjects(query.c_str(), input, &result))
			{
				SQLError(mysql->getLastError());
				return;
			}
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"SELECT id FROM exerciseSession WHERE userID = %s:userID AND exerciseSessionDefinitionID = %d:definitionID AND totalScore = 0 ORDER BY sessionDateTime LIMIT 1;", laneInfo.userID.c_str(), defID  );

			long sessionID = 0;

			int totalScore = 0;
			for (int j = 0; j < lane.getNumExercises(); j++)
			{
				// Live fire normalise scores  ----------------------------------------------------------
				dw_exercise ex = lane.getExercise(j);
				dw_target *pTgt = ex.m_targets[ ex.m_targets.size()-1 ];
				int newScore = ex.m_score;
				int R = pTgt->m_shotInfo.size();
				int numR = ex.m_numRounds;
				int newShotsOnTarget = ex.m_shotsFired;
				int newR = R;
				if (s_pCTRL->m_bLiveFire && (R > 0))
				{
					int A = numR / 2;
					int B = numR * 2;
					if ( (R > A) && (R < B) )
					{
						newScore = (int)(( newScore * numR / (float)R ));
						newR = numR;
						newShotsOnTarget = numR;
						//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print...SHOT CORRECTTION (%d, %d) score (%d, %d)", ex.m_shotsFired, ex.m_numRounds, ex.m_targets[k]->m_score, newScore );
					}
				}
				if (ex.m_maxscore > 0)	totalScore += newScore;
				//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"SQL - lane( %d )  exercise( %d )   score( %d )  totalscore( %d ) ", i, j, lane.getExercise(j).m_score, totalScore );
			}

			// FIXME GRANT
			// I am updating teh memory copy here - but also save this to CSV in case of netweork outage
			// FIXME GRANT ek soek vir AAD 2lane - maar diot moet van scenario af kom waarvoor ek soek
			//MessageBox(NULL, L"SQL update Best scores", L"Error", MB_OK);
			dw_scenario::s_pCTRL->updateBestScore(laneInfo.userID, totalScore);

			if (result.size())
			{
				//MessageBox(NULL, L"SQL - A", L"Error", MB_OK);
				ew_Scene::GetSingleton()->getLogger()->Log(LOG_ERROR, L"SQL - Error    -   %s", ewstring( mysql->getLastError() ).getWString().c_str() );
				sessionID = result[0]["id"];
				ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"SQL - sessionID    -   %d", sessionID );
				query = "UPDATE exerciseSession SET totalScore = %1q:totalScore WHERE id = %2q:sessionID;";
				input.clear();
				input["totalScore"] = totalScore;
				input["sessionID"] = (int)sessionID;
				if (!mysql->saveObject(query.c_str(), input))
				{
					ew_Scene::GetSingleton()->getLogger()->Log(LOG_ERROR, L"SQL - Error    -   %s", ewstring( mysql->getLastError() ).getWString().c_str() );
					//SQLError(mysql->getLastError());
					return;
				}
			}
			else
			{
				//MessageBox(NULL, L"SQL -B", L"Error", MB_OK);
				ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"SQL - B" );
				query = "INSERT INTO exerciseSession(userID, exerciseSessionDefinitionID, weaponTypeID, liveFire, totalScore, maxScore, comments) VALUES(%1q:userID, %2q:definitionID, (SELECT id FROM weapontype WHERE companyID = %4q:companyID AND applicationID = (SELECT id FROM applications WHERE name = %7q:appName) AND name = %5q:weaponTypeName), %6q:liveFire, %3q:totalScore, %8q:maxScore, 'Manual Entry');";
				input.clear();
				input["userID"] = laneInfo.userID.getMBString().c_str();
				input["definitionID"] = (int)defID;
				input["totalScore"] = totalScore;
				input["companyID"] = s_pCTRL->getCompanyID();
				input["weaponTypeName"] = laneInfo.ammo.name.getMBString().c_str();
				input["liveFire"] = s_pCTRL->isLiveFire();
				input["appName"] = APP_NAME;
				input["maxScore"] = m_maxScore;
				if (!mysql->createObject(query.c_str(), input, sessionID))
				{
					SQLError(mysql->getLastError());
					return;
				}
			
			}

			//MessageBox(NULL, L"SQL now it seems insert individual shots", L"Error", MB_OK);
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"SQL now it seems insert individual shots" );
			for (int j = 0; j < lane.getNumExercises(); j++)
			{
				long exerciseID;
				dw_exercise ex = lane.getExercise(j);

				// Live fire normalise scores  ----------------------------------------------------------
				dw_target *pTgt = ex.m_targets[ ex.m_targets.size()-1 ];
				int newScore = ex.m_score;
				int R = pTgt->m_shotInfo.size();
				int numR = ex.m_numRounds;
				int newShotsOnTarget = ex.m_shotsFired;
				int newR = R;
				if (s_pCTRL->m_bLiveFire && (R > 0))
				{
					int A = numR / 2;
					int B = numR * 2;
					if ( (R > A) && (R < B) )
					{
						newScore = (int)(( newScore * numR / (float)R ));
						newR = numR;
						newShotsOnTarget = numR;
						//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print...SHOT CORRECTTION (%d, %d) score (%d, %d)", ex.m_shotsFired, ex.m_numRounds, ex.m_targets[k]->m_score, newScore );
					}
				}

				
				query = "INSERT INTO exercise(exerciseSessionID, exerciseNo, shotsFired, score, grouping, targetType, exerciseName) VALUES(%1q:sessionID, %2q:exerciseNo, %3q:shotsFired, %4q:score, %7q:grouping, %6q:targetType, %5q:exerciseName);";
				input.clear();
				input["sessionID"] = (int)sessionID;
				input["exerciseNo"] = j;
				input["shotsFired"] = newShotsOnTarget;
				input["score"] = newScore;
				input["grouping"] = ex.m_targets.size() == 0 ? 0 : ex.m_targets[ ex.m_targets.size()-1 ]->m_grouping;
				input["exerciseName"] = ex.m_name.getMBString().c_str();
				input["targetType"] = DmString(ex.m_targets[ ex.m_targets.size()-1 ]->m_ImageName).getMBString().c_str();
//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"SQL", m_SQLName.c_str(), m_SQLGroup.c_str() );

				if (!mysql->createObject(query.c_str(), input, exerciseID))
				{
					SQLError(mysql->getLastError());
					return;
				}

				for (int k = 0; k < ex.m_targets.size(); k++)
				{
					for (int l = 0; l < ex.m_targets[k]->m_shotInfo.size(); l++)
					{
						_shotHitInfo shot = ex.m_targets[k]->m_shotInfo[l];

						query = "INSERT INTO shots(exerciseID, targetNo, shotNo, x, y) VALUES(%1q:exerciseID, %2q:targetNo, %3q:shotNo, %4q:x, %5q:y);";
						input.clear();
						input["exerciseID"] = (int)exerciseID;
						input["targetNo"] = k;
						input["shotNo"] = l;
						input["x"] = shot.x;
						input["y"] = shot.y;
						if (!mysql->saveObject(query.c_str(), input))
						{
							SQLError(mysql->getLastError());
							return;
						}
					}
				}
			}
		}

	//	mysql->disconnect();
	}

//	if (!dbConnect && !s_pCTRL->getMySQLError())
//	{
//		MessageBox(NULL, L"Error saving to database", L"Error", MB_OK);
//		s_pCTRL->setMySQLError();
//	}
	
	const char *csvFiles[] = { "scores_all.csv", "scores.csv" };
	for (int i = 0; i < 2; i++)
	{
		if (i && dbConnect)
			continue;

		CSVFile csv;
		if (!csv.openForWrite(csvFiles[i], true))
		{
			MessageBox(NULL, L"Error loading scores file from local disk", L"Error", MB_OK | MB_ICONERROR);
			return;
		}

		for (int i = 0; i < get_numLanes(); i++)
		{
			int csvIdx = 0;
			csv[csvIdx++] = m_SQLName.getMBString().c_str();

			dw_lane lane = m_lanes[i];
			_laneInfo laneInfo = s_pCTRL->getLane(i);

			if (!laneInfo.userID.length())
				continue;

			int totalScore = 0;
			for (int j = 0; j < lane.getNumExercises(); j++)
				totalScore += lane.getExercise(j).m_score;

			csv[csvIdx++] = laneInfo.userID.getMBString().c_str();

			time_t t = time(NULL);
			tm *now = localtime(&t);
			char tmp[256];
			sprintf(tmp, "%d/%d/%d %d:%d", 1900 + now->tm_year, 1 + now->tm_mon, now->tm_mday, now->tm_hour, now->tm_min);
			csv[csvIdx++] = tmp;

			itoa(totalScore, tmp, 10);
			csv[csvIdx++] = tmp;

			for (int j = 0; j < lane.getNumExercises(); j++)
			{
				itoa(lane.getExercise(j).m_score, tmp, 10);
				csv[csvIdx++] = tmp;
			}

			csv.write();
		}

		csv.close();
	}

	// Save to CSV
	list<_user> *users = s_pCTRL->getUsers();
	for (int i = 0; i < get_numLanes(); i++)
	{
		dw_lane lane = m_lanes[i];
		_laneInfo laneInfo = s_pCTRL->getLane(i);

		if (laneInfo.userID.length())
		{
			for (list<_user>::iterator it = users->begin(); it != users->end(); it++)
			{
				if (!it->id.compare(laneInfo.userID))
				{
					int totalScore = 0;
					for (int j = 0; j < lane.getNumExercises(); j++)
						totalScore += lane.getExercise(j).m_score;
					it->bestScore = std::max(it->bestScore, totalScore);
					break;
				}
			}
		}
	}
	users->sort(compareUser);
	s_pCTRL->saveUsersCSV();
	//s_pCTRL->loadUsers();
}



void dw_scenario::clear()
{
	if (m_pSoundSource)
	{
		m_pSoundSource->stopAndDestroy();
		m_pSoundSource = NULL;
	}

	for (int i=0; i<s_targets.size(); i++) delete s_targets[i];
	dw_scenario::s_targets.clear();

	foreach( m_lanes, clear() );
	//m_lanes.clear();

	m_Terrain = L"";
	m_Building = L"";
	m_Sky = L"";
	m_SQLName = L"";
}


void dw_scenario::start()
{
	ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_scenario::start()" );
	ew_Scene::GetSingleton()->getLogger()->Tab();
	m_LoadDelay = 1.0f;
	m_bPlaying = true;
	if (m_pSoundSource)	
	{
		m_pSoundSource->playImmediate();
		m_pSoundSource->setGain( 0.3 );
	}
	
	dw_scenario::s_pCTRL->paperAdvance();

	dw_scenario::s_pCTRL->m_lastVoiceCommand = L"";
	/*
	if (dw_scenario::s_pSCENE->getVoiceRecognition())
	{
		dw_scenario::s_pSCENE->getVoiceRecognition()->loadGrammar( L"grammar_Skeet.xml" );
		dw_scenario::s_pSCENE->getVoiceRecognition()->enableRecognition( true );
	}
	*/

}



void dw_scenario::stop()
{
	if (m_bPlaying)
	{
		ew_Scene::GetSingleton()->getLogger()->UnTab();
		ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_scenario::stop()" );
		if( m_pSoundSource ) m_pSoundSource->stop();
		//m_pBackgroundMusic->stopAndDestroy();
		//m_pBackgroundMusic = NULL;
		m_bPlaying = false; 
		for(int i=0; i<m_lanes.size(); i++) m_lanes[i].loadRounds(0, 0, 0); 
		NUM_MOI =  0;	// forseer dat ons weer vir tiekens soek
	}
}


void dw_scenario::create_terrainScenario( LPCWSTR name, LPCWSTR terrain )
{
	clear();
	m_Name = name;
	wstring A = wstring(terrain);
	m_pScene->rpc_call_LoadTerrain( WStoLPCS(A) );
}


void dw_scenario::create_modelScenario( LPCWSTR name, LPCWSTR mesh, LPCWSTR props )
{
	clear();
	m_Name = name;
	wstring A = wstring(mesh);
	wstring B = wstring(props);
	m_pScene->rpc_call_LoadWorld( WStoLPCS(A), WStoLPCS(B) );
	load_Sky( L"Oudtshoorn_midday" );
}

void dw_scenario::create_speculativeScenario( LPCWSTR name )
{
	clear();
	m_Name = name;
//	wstring A = wstring(mesh);
//	wstring B = wstring(props);
//	m_pScene->rpc_call_LoadWorld( WStoLPCS(A), WStoLPCS(B) );
//	load_Sky( L"outdoor_shoot_B" );
}

void dw_scenario::create_videoScenario( LPCWSTR name )
{
	clear();
	m_Name = name;
}

void dw_scenario::load_Sky( LPCWSTR sky )
{
	ew_Scene::GetSingleton()->loadSky( sky );
	if (s_pCTRL->m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		Bs.Write( "loadSky" );
		Bs.Write( ewstring(sky).c_str() );
		Bs.Write( "" );
		Bs.Write( "" );
		m_pScene->rpc_call( "rpc_CALL", &Bs );
	}
}

void dw_scenario::createMyself( LPCWSTR cam  )
{
	d_Matrix M = dw_scenario::s_Cameras.getMatrix( cam );
	dw_scenario::s_pSCENE->rpc_call_LoadModel( force_Own, "Tom", "human", "myself", "myself",  M.toD3DX(), AI_CLEAR );
}

void dw_scenario::LoadMagazines( int R, int M, float T )				
{ 
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"lw_scenario::LoadMagazines, rounds %d, magazines %d",  R, M  );
	//foreach( m_lanes, loadRounds(R,M,T));  
	{
		for (int i=0; i<m_lanes.size(); i++)
		{
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"lw_scenario::    loadRounds(%d)",  i  );
			dw_scenario::s_pSCENARIO->loadRounds( i, R, M, T );
		}
	}
}

void dw_scenario::load_Cameras( LPCWSTR cam )
{
	WCHAR filePath[MAX_PATH];
	if ( m_pScene->getResourceManager()->findFile( cam, filePath, MAX_PATH ) )
	{
		s_Cameras.load( filePath );
	}
}


void dw_scenario::load_Soundtrack( LPCWSTR sound )
{
	WCHAR filePath[MAX_PATH];
	if ( m_pScene->getResourceManager()->findFile( sound, filePath, MAX_PATH ) )
	{
		m_pSoundSource = m_pScene->getSoundManager()->createSoundSourceFromFile( filePath );
	}
}


void dw_scenario::bullet_hit( unsigned short GUID, f_Vector worldPos, float tof )
{
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_scenario::bullet_hit  GUID   (%d) ", GUID );
	if (s_targetGUIDs[GUID])
	{
		s_targetGUIDs[GUID]->bullet_hit( worldPos, tof );
	}
}

/*
unsigned int dw_scenario::getLiveChannels()
{
	unsigned int ans = 0;
	for (int i=0; i< m_lanes.size(); i++)
	{
		if ( (m_lanes[i].getNumRoudnsLeft() > 0) )				// Vir cut air op pistool
		{
			ans += 1 << i;
		}

		if ( (m_lanes[i].getNumRoudnsLeft() > 0) || (m_lanes[i].getNumRounds()==0) )		// Vir rounds com,peted popup
		{
			ans += 1 << (i+8);	
		}
	}
	for (int i=m_lanes.size(); i< 5; i++)
	{
		ans += 1 << (i+8);	// merk net die donnerse ongebruikkte lane
	}
	return ans;
}
*/




void dw_scenario::loadRounds( int lane, int r, int m, float t )		
{ 
	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"dw_scenario::loadRounds lane %d rounds  %d,  magazines %d", lane, r, m  );
			
	m_lanes[lane].loadRounds( r, m, t ); 
	this->s_pCTRL->m_laneGUI.loadRounds( lane, r, m, t ); 
}







bool dw_scenario::Print( )
{
	return true;
	
	HPDF_Doc  pdf;
	HPDF_Page page;
	HPDF_Font pdf_font;
	float pageWidth, pageHeight;
	int x, y;
	ewstring txt;
	ConfigManager		*m_pCfgMng = m_pScene->getConfigManager();
	ewstring imgDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "/media/controllers/DirectWeapons3/targets/printImages/");
	ewstring prntDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "\\printing\\");

	

	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print( )");

	for (int i = 0; i < m_pc_lanes; i++)
	{
		dw_lane lane = m_lanes[i];
		_laneInfo laneInfo = s_pCTRL->getLane(i);
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print( ) lane %d", i);
		int numShots = 0;
		for (int j = 0; j < lane.getNumExercises(); j++)
			numShots += lane.getExercise(j).m_shotsFired;


		if (numShots > 0)
		{
			
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print( ) ... we have shots in lane %d", i);
			pdf = HPDF_New (error_handler, NULL);
			pdf_font = HPDF_GetFont (pdf, "Helvetica", NULL);
			page = HPDF_AddPage (pdf);
			pageHeight = HPDF_Page_GetHeight (page);
			pageWidth = HPDF_Page_GetWidth (page);

			int totalScore = 0;
			int totalMaxScore = 0;
			
			x = 20;
			y = 60;

			for (int j=0; j<lane.getNumExercises(); j++)
			{
				long exerciseID;
				dw_exercise ex = lane.getExercise(j);
				
			
				if ( ex.m_bPrintPictures )
				{
					txt = ewstring(ex.m_name);
					pdf_Text( page, pdf_font, x, y, txt.c_str(), 10, align_LEFT );

					for (int m=0; m<s_pCTRL->m_laneGUI.m_LANES[i].m_Shots.size(); m++)
					{
						int dX = x;
						ewstring shotIMG = prntDIR + ewstring(PRINT_CNT) + "_" + ewstring(i) + "_" + ewstring(j) + "_" + ewstring(m) + ".jpg";
						D3DXSaveTextureToFile( shotIMG.getWString().c_str(), D3DXIFF_JPG, s_pCTRL->m_laneGUI.m_LANES[i].m_Shots[m].m_pTexture, NULL );
						pdf_JPG( pdf, page, shotIMG, dX, y, 40, 40);
						pdf_CircleWhite( page, dX+20, y -20, 2);
						pdf_Circle( page, dX+20, y -20, 2);

						dX += 45;
					}

					y+= 140;
					if (y>(pageHeight-50-140))
					{
						y = 60;
						x = pageWidth/2 + 20;
					}

				}
				else if (ex.m_maxscore > 0)
				{
					txt = ewstring(ex.m_name);
					pdf_Text( page, pdf_font, x, y, txt.c_str(), 10, align_LEFT );

					for (int k = 0; k < ex.m_targets.size(); k++)
					{
						//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print( ) ...		target %d, %s, %s", k, imgDIR.getWString().c_str(), ex.m_targets[k]->m_ImageName.c_str() );
						if (ex.m_targets[k]->m_target)
						{
							ewstring target = imgDIR + "target_" + ewstring( ex.m_targets[k]->m_target->getSpriteFile() ) + ".png";
							//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L" %s", target.getWString().c_str());
							pdf_PNG( pdf, page, target, x, y + 10 + 100, 100, 100);
							pdf_Text( page, pdf_font, x+150, y+20, "shots : ", 6, align_RIGHT );
							pdf_Text( page, pdf_font, x+150, y+40, "score : ", 6, align_RIGHT );
							pdf_Text( page, pdf_font, x+150, y+60, "grouping : ", 6, align_RIGHT );

							// SCORE CORRECTION  ----------------------------------
							int newScore = (int)ex.m_targets[k]->m_score;
							int newShots = ex.m_shotsFired;//(int)ex.m_targets[k]->m_shotInfo.size();
							//if ( s_pCTRL->isLiveFire() && (ex.m_targets.size() == 1) )		// only live fire single target exercises
							if ( s_pCTRL->isLiveFire() )		// only live fire single target exercises
							{
								/*
								ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print...SHOT CORRECTTION (%d, %d)", ex.m_shotsFired, ex.m_numRounds );
								if ((ex.m_shotsFired - ex.m_numRounds) > 3);  // DO NOTHING - too big
								else if ((ex.m_shotsFired - ex.m_numRounds) > 0) 
								{
									newScore = (int)( ex.m_targets[k]->m_score * ex.m_numRounds / ex.m_shotsFired);
									newShots = ex.m_numRounds;
									s_pCTRL->m_correctionsDOWN ++;
								}
								else if ((ex.m_shotsFired - ex.m_numRounds) < -2) ;  // DO NOTHING - too small
								else if ((ex.m_shotsFired - ex.m_numRounds) < 0) 
								{
									newScore = (int)( ex.m_targets[k]->m_score * ex.m_numRounds / ex.m_shotsFired);
									newShots = ex.m_numRounds;
									s_pCTRL->m_correctionsUP ++;
								}
								else
								{
									s_pCTRL->m_correctionsNONE ++;
								}
								*/
								if (ex.m_shotsFired > 0)
								{
									newScore = (int)( ex.m_targets[k]->m_score * ex.m_numRounds / ex.m_shotsFired);
									newShots = ex.m_numRounds;
								}
							}

							if ( s_pCTRL->isLiveFire() )	s_pCTRL->m_TotalShotsLIVE += ex.m_shotsFired;
							else							s_pCTRL->m_TotalShotsLASER += ex.m_shotsFired;

							txt = ewstring( newShots );
							pdf_Text( page, pdf_font, x+150, y+20, txt, 12, align_LEFT );
							if (ex.m_maxscore>0)	txt = ewstring( newScore ) + " / " + ewstring( ex.m_maxscore );
							else					txt = ewstring( newScore );
							pdf_Text( page, pdf_font, x+150, y+40, txt, 12, align_LEFT );
							txt = ewstring( ex.m_targets[k]->m_grouping*100, 1 ) + " cm";
							pdf_Text( page, pdf_font, x+150, y+60, txt, 12, align_LEFT );

							if ( ex.m_maxscore > 0 )
							{
								totalScore += newScore;
								totalMaxScore += ex.m_maxscore;
							}

							for (int l = 0; l < ex.m_targets[k]->m_shotInfo.size(); l++)
							{
								float scale = ex.m_targets[k]->m_Scale * 50;
								_shotHitInfo shot = ex.m_targets[k]->m_shotInfo[l];
								pdf_Circle( page, x+50 + shot.x*scale, y + 10 + 50 - shot.y*scale, 2);
						
							}
						}
					}

					y+= 140;
					if (y>(pageHeight-50-140))
					{
						y = 60;
						x = pageWidth/2 + 20;
					}
				}

				
			}


			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print( ) ... header");
			// header --------------------------------------------------------
			pdf_Line( page, pageWidth/2, 70, pageWidth/2, pageHeight - 70);
			pdf_Line( page, 0, 40, pageWidth, 40);
			pdf_Text( page, pdf_font, 20, 30, ewstring( laneInfo.userNAME.c_str() ), 12, align_LEFT );
			
			txt = ewstring( this->m_Name );
			pdf_Text( page, pdf_font, pageWidth*0.33, 30, txt, 10, align_MIDDLE );

			txt = ewstring( laneInfo.ammo.name );
			if ( s_pCTRL->isLiveFire() )	txt += " - LIVE";
			else							txt += " - LASER";

			pdf_Text( page, pdf_font, pageWidth*0.66, 30, txt, 10, align_MIDDLE );

			if (totalMaxScore>0)		txt = ewstring(totalScore) + " / " + ewstring(totalMaxScore);
			else						txt = ewstring(totalScore);
			pdf_Text( page, pdf_font, pageWidth-20, 30, txt, 16, align_RIGHT );
			
			

			// footer --------------------------------------------------------
			//pdf_Line( page, 0, pageHeight-50, pageWidth, pageHeight-50);
			pdf_Text( page, pdf_font, pageWidth*0.65, pageHeight-20, "Instructor ", 10, align_RIGHT );
			pdf_Line( page, pageWidth*0.65, pageHeight-20, pageWidth*0.95, pageHeight-20);

			time_t currentTime;
			tm locTime;
			memset(&currentTime, 0, sizeof(time_t));
			time(&currentTime);
			errno_t err = localtime_s( &locTime, &currentTime );
			txt = ewstring(locTime.tm_year + 1900) + ewstring(" / ");
			if ((locTime.tm_mon + 1) < 10)	txt += "0" + ewstring(locTime.tm_mon + 1) + ewstring(" / ");
			else							txt += ewstring(locTime.tm_mon + 1) + ewstring(" / ");
			if (locTime.tm_mday < 10)		txt += "0" + ewstring(locTime.tm_mday);	
			else							txt += ewstring(locTime.tm_mday);	
			pdf_Text( page, pdf_font, 20, pageHeight-20, txt, 10, align_LEFT );



			
			ewstring printName = prntDIR;
			printName += ewstring( laneInfo.userNAME.c_str() );
			printName += "_";
			printName += ewstring( this->m_Name );
			printName += "_";
			printName += ewstring( PRINT_CNT );
			printName += ".pdf";
			HPDF_SaveToFile ( pdf, printName.c_str() );
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print( )  saving %s", printName.getWString().c_str() );
			PRINT_CNT ++;

			if (s_pCTRL->m_bAutoPrint)
			{
				//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print( )  ... autoprint is TRUE");


				ewstring sys = prntDIR + "printme.bat";
				FILE *file = fopen( sys.c_str(), "w" );
				if (file)
				{
					
					fprintf( file, "\"%s\" /p \"%s\" ", ((*m_pCfgMng)[L"Earthworks_3"][L"ACROBAT_DIR"]).c_str(), printName.c_str() );
					fclose( file );
				}
				system( sys.c_str() );
				//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"system( %s )", sys.getWString().c_str() );
			}
			else
			{
				//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print( )  ... autoprint is FALSE - save only");
			}
		}
	}

	return true;
}




bool dw_scenario::PrintVideo( )
{
	HPDF_Doc  pdf;
	HPDF_Page page;
	HPDF_Font pdf_font;
	float pageWidth, pageHeight;
	int x, y;
	ewstring txt;
	ConfigManager		*m_pCfgMng = m_pScene->getConfigManager();
	ewstring imgDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "/media/controllers/DirectWeapons3/targets/printImages/");
	ewstring prntDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "\\printing\\");

	//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"PrintVideo( )");


	for (int i = 0; i < m_pc_lanes; i++)
	{
		dw_lane lane = m_lanes[i];
		_laneInfo laneInfo = s_pCTRL->getLane(i);

		if (lane.getNumRoundsFired() > 0)
		{
			pdf = HPDF_New (error_handler, NULL);
			pdf_font = HPDF_GetFont (pdf, "Helvetica", NULL);
			page = HPDF_AddPage (pdf);
			pageHeight = HPDF_Page_GetHeight (page);
			pageWidth = HPDF_Page_GetWidth (page);

			// header --------------------------------------------------------
			pdf_Line( page, 0, 40, pageWidth, 40);
			pdf_Text( page, pdf_font, 20, 30, ewstring( laneInfo.userNAME.c_str() ), 12, align_LEFT );
			
			txt = ewstring( this->m_Name );
			pdf_Text( page, pdf_font, pageWidth*0.33, 30, txt, 10, align_MIDDLE );

			txt = ewstring( laneInfo.ammo.name );
			if ( s_pCTRL->isLiveFire() )	txt += " - LIVE";
			else							txt += " - LASER";

			pdf_Text( page, pdf_font, pageWidth*0.66, 30, txt, 10, align_MIDDLE );
			
			y = 60;
			x = 20;

			// footer --------------------------------------------------------
			//pdf_Line( page, 0, pageHeight-50, pageWidth, pageHeight-50);
			pdf_Text( page, pdf_font, pageWidth*0.65, pageHeight-20, "Instructor ", 10, align_RIGHT );
			pdf_Line( page, pageWidth*0.65, pageHeight-20, pageWidth*0.95, pageHeight-20);

			time_t currentTime;
			tm locTime;
			memset(&currentTime, 0, sizeof(time_t));
			time(&currentTime);
			errno_t err = localtime_s( &locTime, &currentTime );
			txt = ewstring(locTime.tm_year + 1900) + ewstring(" / ");
			if ((locTime.tm_mon + 1) < 10)	txt += "0" + ewstring(locTime.tm_mon + 1) + ewstring(" / ");
			else							txt += ewstring(locTime.tm_mon + 1) + ewstring(" / ");
			if (locTime.tm_mday < 10)		txt += "0" + ewstring(locTime.tm_mday);	
			else							txt += ewstring(locTime.tm_mday);	
			pdf_Text( page, pdf_font, 20, pageHeight-20, txt, 10, align_LEFT );

			
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"header and footer");

			for (int j=0; j<lane.getNumExercises(); j++)
			{
				long exerciseID;
				dw_exercise ex = lane.getExercise(j);

				if ( s_pCTRL->isLiveFire() )	s_pCTRL->m_TotalShotsLIVE += ex.m_shotsFired;
				else							s_pCTRL->m_TotalShotsLASER += ex.m_shotsFired;

				y += 100;
				x = 20;
				for (int m=0; m<s_pCTRL->m_laneGUI.m_LANES[i].m_Shots.size(); m++)
				{
					ewstring shotIMG = prntDIR + ewstring(PRINT_CNT) + "_" + ewstring(i) + "_" + ewstring(j) + "_" + ewstring(m) + ".jpg";
					D3DXSaveTextureToFile( shotIMG.getWString().c_str(), D3DXIFF_JPG, s_pCTRL->m_laneGUI.m_LANES[i].m_Shots[m].m_pTexture, NULL );
					pdf_JPG( pdf, page, shotIMG, x, y, 100, 100);
					pdf_CircleWhite( page, x+50, y -50, 2);
					pdf_Circle( page, x+50, y -50, 2);

					txt = ewstring(m+1) + " : " + ewstring( s_pCTRL->m_laneGUI.m_LANES[i].m_Shots[m].m_time, 1 ) + "s (" + ewstring( s_pCTRL->m_laneGUI.m_LANES[i].m_Shots[m].m_dT, 1  ) + "s)";
					pdf_Text( page, pdf_font, x, y+15, txt, 10, align_LEFT );

					x += 110;
					if ( x > pageWidth-120)
					{
						y += 120;
						x = 20;
					}
				}

			}

			
			ewstring printName = prntDIR;
			printName += ewstring( laneInfo.userNAME.c_str() );
			printName += "_";
			printName += ewstring( this->m_Name );
			printName += "_";
			printName += ewstring( PRINT_CNT );
			printName += ".pdf";
			HPDF_SaveToFile ( pdf, printName.c_str() );
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"Print( )  saving %s", printName.getWString().c_str() );
			PRINT_CNT ++;

			ewstring sys = "del " + prntDIR + "*.jpg";
			system( sys.c_str() );
			//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"system( %s )", sys.getWString().c_str() );

			//if (s_pCTRL->m_bAutoPrint)
			{
				ewstring sys = prntDIR + "printme.bat";
				FILE *file = fopen( sys.c_str(), "w" );
				if (file)
				{
					
					fprintf( file, "\"%s\" /p \"%s\" ", ((*m_pCfgMng)[L"Earthworks_3"][L"ACROBAT_DIR"]).c_str(), printName.c_str() );
					fclose( file );
				}
				system( sys.c_str() );
			}
		}
	}

	return true;
}



bool dw_scenario::playlistPrint( )
{
	HPDF_Doc  pdf;
	HPDF_Page page;
	HPDF_Font pdf_font;
	float pageWidth, pageHeight;
	int x, y;
	ewstring txt;
	ConfigManager		*m_pCfgMng = m_pScene->getConfigManager();
	ewstring prntDIR = ((*m_pCfgMng)[L"Earthworks_3"][L"INSTALL_DIR"] + "\\printing\\");


	pdf = HPDF_New (error_handler, NULL);
	
	pdf_font = HPDF_GetFont (pdf, "Helvetica", NULL);
	page = HPDF_AddPage (pdf);
	HPDF_Page_SetSize  ( page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_LANDSCAPE);
	pageHeight = HPDF_Page_GetHeight (page);
	pageWidth = HPDF_Page_GetWidth (page);

	// header --------------------------------------------------------
	pdf_Line( page, 0, 40, pageWidth, 40);
	pdf_Text( page, pdf_font, 20, 30, ewstring( "Shooting order" ), 12, align_LEFT );
			

	// footer --------------------------------------------------------
	time_t currentTime;
	tm locTime;
	memset(&currentTime, 0, sizeof(time_t));
	time(&currentTime);
	errno_t err = localtime_s( &locTime, &currentTime );
	txt = ewstring(locTime.tm_year + 1900) + ewstring(" / ");
	if ((locTime.tm_mon + 1) < 10)	txt += "0" + ewstring(locTime.tm_mon + 1) + ewstring(" / ");
	else							txt += ewstring(locTime.tm_mon + 1) + ewstring(" / ");
	if (locTime.tm_mday < 10)		txt += "0" + ewstring(locTime.tm_mday);	
	else							txt += ewstring(locTime.tm_mday);	
	pdf_Text( page, pdf_font, 20, pageHeight-20, txt, 10, align_LEFT );

	int numLanes = get_numLanes();
	int LW = (pageWidth - 40) / numLanes;
	for (int i=0; i<numLanes; i++)
	{
		pdf_Text( page, pdf_font, 20+(i+0.5)*LW, 100, ewstring( "Lane " ) + ewstring(i+1), 14, align_MIDDLE );
		if (i>0) pdf_Line( page, 20+i*LW, 70, 20+i*LW, pageHeight - 70);
	}

	for (int i=0; i<s_pCTRL->m_PlaylistUsers.size(); i++)
	{
		int dx = i % numLanes;
		int dy = i / numLanes;

		for (std::list<_user>::iterator it = s_pCTRL->m_users.begin(); it != s_pCTRL->m_users.end(); it++)
		{
			if (it->id.toInt() == s_pCTRL->m_PlaylistUsers[i])
			{
				pdf_Text( page, pdf_font, 20+5+(dx)*LW, 150 + dy*70, ewstring( it->displayName ), 10, align_LEFT );
				pdf_Text( page, pdf_font, 20+5+(dx)*LW, 150 + dy*70+14, ewstring( it->userID ), 9, align_LEFT );
			}
		}
	}


			
	ewstring printName = prntDIR;
	printName += "playlist.pdf";
	HPDF_SaveToFile ( pdf, printName.c_str() );

	ewstring sys = prntDIR + "printme.bat";
	FILE *file = fopen( sys.c_str(), "w" );
	if (file)
	{
					
		fprintf( file, "\"%s\" /p \"%s\" ", ((*m_pCfgMng)[L"Earthworks_3"][L"ACROBAT_DIR"]).c_str(), printName.c_str() );
		fclose( file );
	}
	system( sys.c_str() );


	return true;
}