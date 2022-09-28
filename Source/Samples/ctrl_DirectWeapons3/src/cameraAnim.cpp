
#include "cameraAnim.h"
#include "Dm/DmFoundation.h"
#include "ew_Scene.h"


f_Vector toVector( LPCWSTR str )
{
	f_Vector V;
	WCHAR in[512];
	wsprintf( in, L"%s", str );
	WCHAR *tok = wcstok(in, L" ");		V.x = _wtof( tok );
	tok = wcstok(0, L" " );				V.y = _wtof( tok );
	tok = wcstok(0, L" " );				V.z = _wtof( tok );
	return V;
}




void _path::solve_CR()		// CATMUL ROM
{
	if (keys.size()==0) return;

	spline.clear();
	_matSpline S;
	
	for (unsigned int i=0; i<keys.size()-1; i++)
	{
		S.t0 = keys[i].time;
		S.t1 = keys[i+1].time;
		S.dt = keys[i+1].time - keys[i].time;

		S.fovP1 = keys[i].FOV;
		S.fovP2 = keys[i+1].FOV;

		S.posP1 = keys[i].pos;
		S.posP2 = keys[i+1].pos;
		
		S.dirP1 = keys[i].dir;
		S.dirP2 = keys[i+1].dir;
		
		S.upP1 = keys[i].up;
		S.upP2 = keys[i+1].up;
		
		if (keys.size() == 2)
		{
			S.fovT1 = keys[i+1].FOV - keys[i].FOV;
			S.posT1 = keys[i+1].pos - keys[i].pos;
			S.dirT1 = keys[i+1].dir - keys[i].dir;
			S.upT1 = keys[i+1].up - keys[i].up;

			S.fovT2 = keys[i+1].FOV - keys[i].FOV;
			S.posT2 = keys[i+1].pos - keys[i].pos;
			S.dirT2 = keys[i+1].dir - keys[i].dir;
			S.upT2 = keys[i+1].up - keys[i].up;
		}
		else if (i==0)
		{
			S.fovT1 = keys[i+1].FOV - keys[i].FOV;
			S.posT1 = keys[i+1].pos - keys[i].pos;
			S.dirT1 = keys[i+1].dir - keys[i].dir;
			S.upT1 = keys[i+1].up - keys[i].up;

			S.fovT2 = (keys[i+2].FOV - keys[i].FOV)*0.5f;
			S.posT2 = (keys[i+2].pos - keys[i].pos)*0.5f;
			S.dirT2 = (keys[i+2].dir - keys[i].dir)*0.5f;
			S.upT2 = (keys[i+2].up - keys[i].up)*0.5f;
		}
		else if (i==keys.size()-2)
		{
			S.fovT1 = (keys[i+1].FOV - keys[i-1].FOV)*0.5f;
			S.posT1 = (keys[i+1].pos - keys[i-1].pos)*0.5f;
			S.dirT1 = (keys[i+1].dir - keys[i-1].dir)*0.5f;
			S.upT1 = (keys[i+1].up - keys[i-1].up)*0.5f;

			S.fovT2 = keys[i].FOV - keys[i-1].FOV;
			S.posT2 = keys[i].pos - keys[i-1].pos;
			S.dirT2 = keys[i].dir - keys[i-1].dir;
			S.upT2 = keys[i].up - keys[i-1].up;
		}
		else
		{
			S.fovT1 = (keys[i+1].FOV - keys[i-1].FOV)*0.5f;
			S.posT1 = (keys[i+1].pos - keys[i-1].pos)*0.5f;
			S.dirT1 = (keys[i+1].dir - keys[i-1].dir)*0.5f;
			S.upT1 = (keys[i+1].up - keys[i-1].up)*0.5f;

			S.fovT2 = (keys[i+2].FOV - keys[i].FOV)*0.5f;
			S.posT2 = (keys[i+2].pos - keys[i].pos)*0.5f;
			S.dirT2 = (keys[i+2].dir - keys[i].dir)*0.5f;
			S.upT2 = (keys[i+2].up - keys[i].up)*0.5f;
		}

		spline.push_back( S );
	}

}


// -----------------------------------------------------------------------------------------------------------------------
cameraAnim::cameraAnim()
{
	m_bPlaying = false;
}

cameraAnim::~cameraAnim()
{
}


bool cameraAnim::update( double dTime )
{
	_matSpline *pS;
	if ( m_bPlaying )
	{
		//ew_Scene::GetSingleton()->getLogger()->Log( LOG_INFO, L"   camera update - start" );
		_path *pP = &(it_active->second);
		pP->playtime += dTime;
		if (pP->playtime > pP->it->t1 )	pP->it ++;
		if ( pP->it != pP->spline.end())
		{
			pS = pP->it._Ptr;
				
			float s = __min(1, __max(0, (pP->playtime - pS->t0) / pS->dt));
			float s2 = s*s;
			float s3 = s*s*s;

			float h1 =    2*s3 - 3*s2 + 1;
			float h2 = (-2)*s3 + 3*s2;
			float h3 =      s3 - 2*s2 + s;
			float h4 =      s3 -   s2;

			d_Vector R, U, D, P;
			P = pS->posP1*h1 + pS->posP2*h2 + pS->posT1*h3 + pS->posT2*h4;
			D = pS->dirP1*h1 + pS->dirP2*h2 + pS->dirT1*h3 + pS->dirT2*h4;
			U =  pS->upP1*h1 +  pS->upP2*h2 +  pS->upT1*h3 +  pS->upT2*h4;
			R = U ^ D;
			U = D ^ R;
			R.normalize();
			U.normalize();
			D.normalize();
			m_Matrix.setRight( R );
			m_Matrix.setDir( D );
			m_Matrix.setUp( U );
			m_Matrix.setPos( P + D*m_OffsetDistance );

			m_fov = pS->fovP1*h1 + pS->fovP2*h2 + pS->fovT1*h3 + pS->fovT2*h4;
		}
		else
		{
			_cam_key K = pP->keys[ pP->keys.size()-1 ];		// laaste een
			d_Vector R = K.up ^ K.dir;
			m_Matrix.setRight( R );
			m_Matrix.setDir( K.dir );
			m_Matrix.setUp( K.up );
			m_Matrix.setPos( K.pos + K.dir*m_OffsetDistance );

			m_fov = K.FOV;

			//ew_Scene::GetSingleton()->getLogger()->Log( LOG_INFO, L"   camera update - last one" );
			m_bPlaying = false;
		}
	}
	
	return m_bPlaying;
}


void cameraAnim::play( float Distance )
{
	

	m_OffsetDistance = Distance;
	it_active = m_cameras.find( L"ROOT" );
	if (it_active != m_cameras.end())	
	{
		m_bPlaying = true;
		it_active->second.playtime = 0;
		it_active->second.it = it_active->second.spline.begin();
	}
}


void cameraAnim::play( wstring name )
{
	ew_Scene::GetSingleton()->getLogger()->Log( LOG_INFO, L"play camera : %s", name.c_str() );

	m_OffsetDistance = 0.0f;
	it_active = m_cameras.find( name );
	if (it_active != m_cameras.end())	
	{
		m_bPlaying = true;
		it_active->second.playtime = 0;
		it_active->second.it = it_active->second.spline.begin();
	}
	else
	{
		ew_Scene::GetSingleton()->getLogger()->Log( LOG_INFO, L"could not find camera %s", name.c_str() );
	}
}

d_Matrix cameraAnim::getMatrix(  wstring name )
{
	d_Matrix M;
	M.identity();
	map<wstring, _path>::iterator	it = m_cameras.find( name );
	if (it != m_cameras.end())	
	{
		d_Vector R = it->second.keys[0].up ^ it->second.keys[0].dir;
		M.setRight( R );
		M.setDir( it->second.keys[0].dir );
		M.setUp( it->second.keys[0].up );
		M.setPos( it->second.keys[0].pos );
	}
	return M;
}


void cameraAnim::load( LPCWSTR name )
{
	m_cameras.clear();

	TiXmlDocument document;
	if (!document.LoadFile( name ))	return;

	TiXmlNode *cameraNode = document.FirstChild( L"camera" );
	while(cameraNode)
	{
		_path P;
		P.name = cameraNode->ToElement()->Attribute(L"name");
		//ew_Scene::GetSingleton()->getLogger()->Log( LOG_INFO, L"Loading camera : %s", P.name.c_str() );

			TiXmlNode *keyNode = cameraNode->FirstChild( L"key" );
			while (keyNode)
			{
				_cam_key K;
				K.FOV = DmString( keyNode->ToElement()->Attribute(L"fov") ).toFloat();
				K.time = DmString( keyNode->ToElement()->Attribute(L"time") ).toFloat();
				K.pos = toVector( keyNode->ToElement()->Attribute(L"pos") );
				K.dir = toVector( keyNode->ToElement()->Attribute(L"dir")  );
				K.up = toVector( keyNode->ToElement()->Attribute(L"up")  );
				P.keys.push_back( K );

				keyNode = keyNode->NextSibling( L"key" );
			}

		
		P.solve_CR();
		P.it = P.spline.end();
		m_cameras[P.name] = P;
		cameraNode=cameraNode->NextSibling( L"camera" );
	}

	cameraNode = document.FirstChild( L"camera_B" );
	while(cameraNode)
	{
		_path P;
		P.name = cameraNode->ToElement()->Attribute(L"name");
		//ew_Scene::GetSingleton()->getLogger()->Log( LOG_INFO, L"Loading camera : %s", P.name.c_str() );

			TiXmlNode *keyNode = cameraNode->FirstChild( L"key" );
			while (keyNode)
			{
				f_Matrix M;
				_cam_key K;
				K.FOV = DmString( keyNode->ToElement()->Attribute(L"fov") ).toFloat();
				K.time = DmString( keyNode->ToElement()->Attribute(L"time") ).toFloat();
				K.pos = toVector( keyNode->ToElement()->Attribute(L"pos") );
				float R = _wtof( keyNode->ToElement()->Attribute(L"rot") );
				M.identity();
				M.concatRotY( R );
				K.dir = M.dir();
				K.up = f_Vector(0,1,0);
				P.keys.push_back( K );

				keyNode = keyNode->NextSibling( L"key" );
			}

		
		P.solve_CR();
		P.it = P.spline.end();
		m_cameras[P.name] = P;
		cameraNode=cameraNode->NextSibling( L"camera_B" );
	}

	// insert a BASE camera at the end --------------------------------
	{
		_path P;
		_cam_key K;
		P.name = L"ROOT";
				
		K.FOV = 0;
		K.time = 0;
		K.pos = f_Vector(0, 1.7, 0);
		K.dir = f_Vector(0, 0, -1);
		K.up = f_Vector(0,1,0);
		P.keys.push_back( K );

		K.time = 0.1;
		P.keys.push_back( K );
		
		P.solve_CR();
		P.it = P.spline.end();
		m_cameras[P.name] = P;
	}

	it_active = m_cameras.end();
}