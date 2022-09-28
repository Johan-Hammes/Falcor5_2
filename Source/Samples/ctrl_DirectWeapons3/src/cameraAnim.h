#pragma once 

#define WIN32_LEAN_AND_MEAN

#include <vector>
#include <map>
#include "Maths/Maths.h"
#include "TinyXML/tinyxml.h"

using namespace std;


struct _cam_key
{
	float FOV;
	float time;
	f_Vector pos;
	f_Vector dir;
	f_Vector up;
};

struct _matSpline
{
	float t0, t1, dt;
	float fovP1, fovP2, fovT1, fovT2;
	f_Vector posP1, posP2, posT1, posT2;
	f_Vector dirP1, dirP2, dirT1, dirT2;
	f_Vector upP1, upP2, upT1, upT2;

};

class _path
{
public:
	_path()			{;}
	virtual ~_path() {;}
	void solve_CR();		// CATMUL ROM

	wstring						name;
	float						playtime;
	vector<_matSpline>::iterator	it;
	vector<_cam_key>			keys;
	vector<_matSpline>			spline;
};



class cameraAnim
{
public:
			 cameraAnim();
	virtual	~cameraAnim();

	bool update( double dTime );
	void load( LPCWSTR name );
	void play( wstring name);
	void play( float Distance );
	d_Matrix *getMatrix() { return &m_Matrix; }
	float getFOV() { return m_fov; }
	bool isPlaying() {return m_bPlaying;}

	d_Matrix getMatrix(  wstring name );

private:
	map<wstring, _path>				m_cameras;
	map<wstring, _path>::iterator	it_active;
	d_Matrix						m_Matrix;
	float							m_fov;
	bool							m_bPlaying;
	float							m_OffsetDistance;
};
