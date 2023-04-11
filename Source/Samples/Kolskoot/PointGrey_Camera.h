#pragma once

#include "Falcor.h"	
#include "windows.h"
#include <queue>
#include "FlyCapture/FlyCapture2.h"
using namespace FlyCapture2;



enum { POS_TOP_EDGE, POS_MIDDLE, POS_BOTTOM_EDGE };


struct _blob
{
	int	numPix;
	//int	numRectPix;		// number of pixels in its rectangle - I want ot early out for large
	float value;
	float x;
	float y;
	unsigned short xmin, xmax;
	unsigned short ymin, ymax;
	//unsigned short w, h;
	bool bOK;
};

#define BLOCK_SIZE 3
#define MAX_BLOBS_TO_TRACK 60
#define BUFFER_SIZE 32

struct _buffer_element
{
	unsigned char*	data;
	int				frameNumber;
	int				numDots;
	_blob			dots[MAX_BLOBS_TO_TRACK];
};

class PointGrey_Camera
{
public :

	static PointGrey_Camera *GetSingleton();
	static void FreeSingleton();

	void setReferenceFrame( bool bClear, int numFrames );
	//void setMaskFrame( );
	void clearReferenceFrame( );
	void restartCamera( int w, int h);
	bool setFirstCamera( unsigned int serial );

	void setIncrementBuffer( bool bInc );

	void setBrightness( float S );
	void setExposure( float S );
	void setGamma( float S );
	void setShutter( float S );
	void setGain( float S );
	void setFramerate( float S );
	void setDots( int min, int max, int T, int position );
	void setThreshold( int T );
	void setMode7( int width, int height);

	bool isConnected() { return m_bConnected; }

	unsigned int getSerialNumber( int cam = 0 );


	
protected :
	bool			m_bInit;

private :
	PointGrey_Camera();
	virtual ~PointGrey_Camera();
	static PointGrey_Camera *s_instance;
	bool		m_bConnected;

public:
	unsigned int	m_NumCameras;
	PGRGuid			m_Guid[2];
	Camera			m_VideoCamera[2];
	Error			m_CamError;
	unsigned int	A;
	unsigned int	B;
	bool			m_bSwapCameras;

    std::queue<glm::vec4> dotQueue;
    glm::vec2 bufferSize;
    unsigned char* bufferData;
    unsigned char* bufferReference;
    unsigned char* bufferBlur;
    unsigned char* bufferThreshold;
};
