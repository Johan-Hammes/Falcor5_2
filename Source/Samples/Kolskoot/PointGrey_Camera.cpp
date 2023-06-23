#define WIN32_LEAN_AND_MEAN

#include "PointGrey_Camera.h"
#include <process.h>
#include "stdio.h"
#include "windows.h"



#pragma optimize( "", off )



bool			bthreadRunning;

PointGrey_Camera *PointGrey_Camera::s_instance = NULL;





// Circular buffer for backup


_buffer_element BUFFER[2][BUFFER_SIZE];
int bufferPos[2];
int bufferCnt[2];
bool b_incBuffer;		// OFF for live fire


int m_width = 0;
int m_height = 0;
unsigned char*	vidFrame[2] = {NULL, NULL };	// 5 die buffer
unsigned char*	vidReference[2] = {NULL, NULL};	
bool bRefClearing;
unsigned char	referenceCNT[2] = {0, 0};
unsigned short*	vidBlur = NULL;
unsigned char*	vidThresh = NULL;
//unsigned char*	vidMASK = NULL;
int	copyPTR = 0;
int processPTR = 0;
int	frameCNT[2] = {0,0};
int	lastProcessedFrame = 0;
int	slowFrames = 0;
int NumDots;
_blob DOTS[MAX_BLOBS_TO_TRACK];
int	m_CurrentCamera;
bool b_SwapCamera;


int DetectionType;				// top, middle or bottom
unsigned char THRESHHOLD=32;
int MIN_PIX = 10;
int MAX_PIX = 500;

int okDOTS;
glm::vec4 v_dots[100];
float tB, tT, tD, tCam;

int debug_PixelsProcessed;
int debug_SkippedFrames;

//ew_Scene *pScene = NULL;
bool	m_bBusyProcessing = false;

void allocateBuffers(int w, int h)
{
	m_width = w;
	m_height = h;

	for (int i=0; i<BUFFER_SIZE; i++)
	{
		if(BUFFER[0][i].data) delete BUFFER[0][i].data;
		BUFFER[0][i].data = new unsigned char[w*h];

		if(BUFFER[1][i].data) delete BUFFER[1][i].data;
		BUFFER[1][i].data = new unsigned char[w*h];
	}

	vidFrame[0] = BUFFER[0][0].data;
	vidFrame[1] = BUFFER[1][0].data;
	bufferPos[0] = 0;
	bufferCnt[0] = 0;
	bufferPos[1] = 0;
	bufferCnt[1] = 0;
	
	if(vidReference[0]) delete vidReference[0];
	if(vidReference[1]) delete vidReference[1];
	if(vidBlur) delete vidBlur;
	if(vidThresh) delete vidThresh;
	//if(vidMASK) delete vidMASK;

	
	vidReference[0] =  new unsigned char[w*h];
	vidReference[1] =  new unsigned char[w*h];
	vidBlur =  new unsigned short[w*h];
	vidThresh =  new unsigned char[w*h];
	//vidMASK =  new unsigned char[w*h];

	//memset( vidMASK, 1, m_width*m_height );

    PointGrey_Camera::GetSingleton()->bufferSize = glm::vec2(m_width, m_height);
    PointGrey_Camera::GetSingleton()->bufferData = BUFFER[0][0].data;
    PointGrey_Camera::GetSingleton()->bufferReference = vidFrame[0];
    //PointGrey_Camera::GetSingleton()->bufferBlur = vidBlur;
    PointGrey_Camera::GetSingleton()->bufferThreshold = vidThresh;
}



/*	Test id the point in tersect with a current blob, and add it, or return false to create a new blob*/
void addToBlob( unsigned short x, unsigned short y )
{
	for (int i=0; i<NumDots; i++)
	{
		if ( (x+BLOCK_SIZE>=DOTS[i].xmin) && (x-BLOCK_SIZE<=DOTS[i].xmax) && (y+BLOCK_SIZE>=DOTS[i].ymin) && (y-BLOCK_SIZE<=DOTS[i].ymax) )
		{
			DOTS[i].xmin = __min( DOTS[i].xmin, x-BLOCK_SIZE );
			DOTS[i].xmax = __max( DOTS[i].xmax, x+BLOCK_SIZE );
			DOTS[i].ymin = __min( DOTS[i].ymin, y-BLOCK_SIZE );
			DOTS[i].ymax = __max( DOTS[i].ymax, y+BLOCK_SIZE );
			return;
		}
	}

	// if we get here add a new blob
	if ( NumDots < MAX_BLOBS_TO_TRACK-1 )
	{
		DOTS[ NumDots ].xmin = x-BLOCK_SIZE;
		DOTS[ NumDots ].xmax = x+BLOCK_SIZE;
		DOTS[ NumDots ].ymin = y-BLOCK_SIZE;
		DOTS[ NumDots ].ymax = y+BLOCK_SIZE;
		NumDots ++;
		return;
	}
}

int IDX(int x, int y) {return y*m_width + x;}
void threshold()	
{
	int idx;
	int y;

	for (unsigned short k=BLOCK_SIZE; k<m_height-BLOCK_SIZE*2; k+=2)
	{
		for (unsigned short x=BLOCK_SIZE; x<m_width-BLOCK_SIZE*2; x+=2)
		{
			if ( DetectionType == POS_TOP_EDGE )	y = k;
			else									y = m_height - k;
			idx = IDX(x,y);
			//if ( (vidFrame[ idx ] * vidMASK[ idx ]) > (vidReference[m_CurrentCamera][ idx ] + THRESHHOLD) )
			if ( vidFrame[m_CurrentCamera][ idx ] > (vidReference[m_CurrentCamera][ idx ] + THRESHHOLD) )
			{	
				addToBlob( x, y );
			}
		}
	}
}

void blobSolve(int i)
{
	int countdownPixels;
	
	if (DetectionType == POS_MIDDLE)	countdownPixels = 100000;		// do all the pixels
	else								countdownPixels = 10;

	// restrict min max rectanlge to valid area
	DOTS[i].xmin = __max( 0,			DOTS[i].xmin );
	DOTS[i].xmax = __min( m_width-1,	DOTS[i].xmax );
	DOTS[i].ymin = __max( 0,			DOTS[i].ymin );
	DOTS[i].ymax = __min( m_height-1,	DOTS[i].ymax );

	for ( unsigned short k=DOTS[i].ymin; k<DOTS[i].ymax; k++ )
	{
		for ( unsigned short x=DOTS[i].xmin; x<DOTS[i].xmax; x++ )
		{
			int y = k;
			if (DetectionType == POS_BOTTOM_EDGE) y = __min( m_height - 2, DOTS[i].ymax + DOTS[i].ymin - k );

			debug_PixelsProcessed ++; 
			if ( debug_PixelsProcessed > 40000 ) return;	// its a mess no decent reference, dont stall

			if ( vidFrame[m_CurrentCamera][IDX(x,y)] > ( vidReference[m_CurrentCamera][IDX(x,y)] + THRESHHOLD ) )
			{
				int val = vidFrame[m_CurrentCamera][IDX(x,y)] - vidReference[m_CurrentCamera][IDX(x,y)];
				{
					DOTS[i].numPix ++; 
					if (countdownPixels > 0)
					{
						DOTS[i].value += val;
						DOTS[i].x += x*val;
						DOTS[i].y += y*val;
						vidThresh[IDX(x,y)] = 128;
					}
					countdownPixels --;
				}			
			}
		}
	}
}


void blobMark(int i)
{
	if ( (DOTS[i].x > 6) && (DOTS[i].x < m_width - 6) && (DOTS[i].y > 6) && (DOTS[i].y < m_height - 6) )
	{
		for (int j=-5; j<=5; j++)
		{
			vidThresh[IDX((int)DOTS[i].x-j, (int)DOTS[i].y-j)] = 254;
			vidThresh[IDX((int)DOTS[i].x+j, (int)DOTS[i].y-j)] = 254;
		}
	}
	
	for (int x=DOTS[i].xmin; x<DOTS[i].xmax; x++)
	{
		vidThresh[IDX(x, DOTS[i].ymin)] = 255;
		vidThresh[IDX(x, DOTS[i].ymax)] = 255;
	}
	for (int y=DOTS[i].ymin; y<DOTS[i].ymax; y++)
	{
		vidThresh[IDX(DOTS[i].xmin, y)] = 255;
		vidThresh[IDX(DOTS[i].xmax, y)] = 255;
	}
	
}



void dots()
{
	okDOTS = 0;
	debug_PixelsProcessed = 0;

	for (int i=0; i<NumDots; i++)
	{
		blobSolve( i );

		DOTS[i].x /= DOTS[i].value;
		DOTS[i].y /= DOTS[i].value;		// This is for middle average position

		DOTS[i].bOK = false;
		if ( (DOTS[i].numPix>MIN_PIX) && (DOTS[i].numPix<MAX_PIX) )		DOTS[i].bOK = true;
				
		if ( DOTS[i].bOK )
		{
			v_dots[okDOTS] = glm::vec4( DOTS[i].x, DOTS[i].y, (float)DOTS[i].numPix, DOTS[i].value/DOTS[i].numPix );
			okDOTS ++;
			blobMark( i );
		}
	}
}


void ProcessDots( int frameNumber )
{
	//if (pScene)
	{
		NumDots = 0;				// clear all dots
		memset( vidThresh, 0, m_width*m_height );
		for (int d=0; d<MAX_BLOBS_TO_TRACK; d++)	
		{
			DOTS[d].numPix = 0;
			DOTS[d].xmin = 10000;
			DOTS[d].xmax = 0;
			DOTS[d].ymin = 10000;
			DOTS[d].ymax = 0;
			DOTS[d].value = 0;
			DOTS[d].x = 0;
			DOTS[d].y = 0;
		}

		//timerBlur.begin();
			threshold();
		//tT = timerBlur.end();

		//timerBlur.begin();
			dots();
		//tD = timerBlur.end();


            //PointGrey_Camera::GetSingleton()->dotQueue.empty();

            for (int i = 0; i < okDOTS; i++)
            {
                PointGrey_Camera::GetSingleton()->dotQueue.push(v_dots[i]);
            }

            /*
		// Pust the data to scene
		if( !pScene->bAccumulateDots )	pScene->numDots[m_CurrentCamera] = 0;	// we are in clear mode
	
		for (int i=0; i<okDOTS; i++ )
		{
			if ( pScene->numDots[m_CurrentCamera] > 90 )	pScene->numDots[m_CurrentCamera] = 0;		// if it gts too big, just clear
			pScene->dots[m_CurrentCamera][ pScene->numDots[m_CurrentCamera] ] = v_dots[i];
			pScene->numDots[m_CurrentCamera] ++;
		}
	
		pScene->timeDots = tT + tD;
		pScene->m_pvidFrame[m_CurrentCamera] = vidFrame[m_CurrentCamera];
		pScene->m_pvidThresh[m_CurrentCamera] = vidThresh;
		pScene->m_pvidRef[m_CurrentCamera] = vidReference[m_CurrentCamera];
		pScene->m_PG_width = m_width;
		pScene->m_PG_height = m_height;
		pScene->m_PG_frameCNT[m_CurrentCamera]  = frameNumber;
		pScene->m_PG_debug_PixelsProcessed = debug_PixelsProcessed;
		pScene->m_PG_debug_SkippedFrames = debug_SkippedFrames;
        */
		lastProcessedFrame = frameNumber;

		if (okDOTS > 0)
		{
			BUFFER[m_CurrentCamera][bufferPos[m_CurrentCamera]].numDots = NumDots;
			BUFFER[m_CurrentCamera][bufferPos[m_CurrentCamera]].frameNumber = frameNumber;
			for (int j=0; j<NumDots; j++)
			{
				BUFFER[m_CurrentCamera][bufferPos[m_CurrentCamera]].dots[j] = DOTS[j];
			}
			
			if (b_incBuffer)
			{
				vidFrame[m_CurrentCamera] = BUFFER[m_CurrentCamera][bufferPos[m_CurrentCamera]].data;
				bufferPos[m_CurrentCamera] ++;
				bufferPos[m_CurrentCamera] = bufferPos[m_CurrentCamera] % BUFFER_SIZE;
				bufferCnt[m_CurrentCamera] ++;
				vidFrame[m_CurrentCamera] = BUFFER[m_CurrentCamera][bufferPos[m_CurrentCamera]].data;
			}
		}
	
		bool bCm = true;
	}
	//else
	{
		 //pScene = ew_Scene::GetSingleton();		// we are not ready yet -----------------------------						
	}
	
	return;
}


void calculateReference( int cam )
{
	referenceCNT[cam]--;

	for (int y=0; y<m_height; y++)
	{
		for (int x=0; x<m_width; x++)
		{
			vidReference[cam][IDX(x, y)] = __max(  vidReference[cam][IDX(x, y)],    __min( 255 - THRESHHOLD, vidFrame[m_CurrentCamera][IDX(x,y)] ) );		// take the bigger one
		}
	}
}

// sover ek kan sien is alles wat hier binne gebeur in 'n apparte thread omdat dit van 'n ander thread af regoep word
void OnImageGrabbed(Image* pImage, const void* pCallbackData)
{
	

	int timeMS = 0;
	while ( m_bBusyProcessing )
	{
		Sleep( 1 );
		timeMS ++;
		if (timeMS > 15)
		{
			debug_SkippedFrames ++;
			return;
		}
	}

	m_bBusyProcessing = true;
	{

		if( (m_width==0) &&  ((pImage->GetCols() != m_width)  || 	(pImage->GetRows() != m_height)))
		{
			allocateBuffers(pImage->GetCols(), pImage->GetRows());		
		}

		unsigned int Camera =  *(unsigned int*)pCallbackData;
		if ( b_SwapCamera )  Camera = (Camera+1) & 0x1;
		m_CurrentCamera = Camera;
		frameCNT[Camera] ++;

		if ( referenceCNT[m_CurrentCamera] > 0 )			
		{
			calculateReference( m_CurrentCamera );
		}
		else
		{
			if( (pImage->GetCols() == m_width)  &&
				(pImage->GetRows() == m_height)  &&
				(pImage->GetCols() == pImage->GetStride())  &&
				(pImage->GetBitsPerPixel() == 8) )
			{
				memcpy( vidFrame[m_CurrentCamera], pImage->GetData(), m_width*m_height );
				ProcessDots( frameCNT[Camera] );
			}
		}
	}
	m_bBusyProcessing = false;
	
	return;
}


// sover ek kan sien is alles wat hier binne gebeur in 'n apparte thread omdat dit van 'n ander thread af regoep word
void OnImageGrabbed_B(Image* pImage, const void* pCallbackData)
{
	

	int timeMS = 0;
	while ( m_bBusyProcessing )
	{
		Sleep( 1 );
		timeMS ++;
		if (timeMS > 15)
		{
			debug_SkippedFrames ++;
			return;
		}
	}

	m_bBusyProcessing = true;
	{
		if( (m_width==0) &&  ((pImage->GetCols() != m_width)  || 	(pImage->GetRows() != m_height)))
		{
			allocateBuffers(pImage->GetCols(), pImage->GetRows());		
		}

		unsigned int Camera =  *(unsigned int*)pCallbackData;
		if ( b_SwapCamera )  Camera = (Camera+1) & 0x1;
		m_CurrentCamera = Camera;
		frameCNT[Camera] ++;

		if ( referenceCNT[m_CurrentCamera] > 0 )			
		{
			calculateReference( m_CurrentCamera );
		}
		else
		{
			if( (pImage->GetCols() == m_width)  &&
				(pImage->GetRows() == m_height)  &&
				(pImage->GetCols() == pImage->GetStride())  &&
				(pImage->GetBitsPerPixel() == 8) )
			{
				memcpy( vidFrame[m_CurrentCamera], pImage->GetData(), m_width*m_height );
				ProcessDots( frameCNT[Camera] );
			}
		}
	}
	m_bBusyProcessing = false;
	
	return;
}


PointGrey_Camera *PointGrey_Camera::GetSingleton()
{
	if (!s_instance)
		s_instance = new PointGrey_Camera;

	return s_instance;
}

void PointGrey_Camera::FreeSingleton()
{
	if (s_instance)
		delete s_instance;
	s_instance = NULL;
}

PointGrey_Camera::PointGrey_Camera()
{
	for (int i=0; i<BUFFER_SIZE; i++)	BUFFER[0][i].data = NULL;
	for (int i=0; i<BUFFER_SIZE; i++)	BUFFER[1][i].data = NULL;
	bufferPos[0] = 0;
	bufferCnt[0] = 0;
	bufferPos[1] = 0;
	bufferCnt[1] = 0;
	b_incBuffer = false;
	
	A = 0;
	B = 1;
	m_bSwapCameras = false;
	BusManager	busMgr;
    m_CamError = busMgr.GetNumOfCameras( &m_NumCameras );
	m_NumCameras = __min( m_NumCameras, 2 );
	for (int i=0; i<m_NumCameras; i++)
	{
		m_CamError = busMgr.GetCameraFromIndex( i, &m_Guid[i] );
        
		debug_SkippedFrames = 0;

        CameraInfo pCameraInfo;
        FlyCapture2::Error E3 = m_VideoCamera[i].GetCameraInfo(&pCameraInfo);

        FlyCapture2::Error E2 = m_VideoCamera[i].Connect();
        m_bConnected = (E2.GetType() == PGRERROR_OK);

        E3 = m_VideoCamera[i].GetCameraInfo(&pCameraInfo);
/*
        FlyCapture2::VideoMode pVideoMode;
        FlyCapture2::FrameRate pFrameRate;
        Error E = m_VideoCamera[i].GetVideoModeAndFrameRate(&pVideoMode, &pFrameRate);

        {
            

            Format7ImageSettings F7;
            Format7Info Finfo;
            bool support;
            FlyCapture2::Error E5 = m_VideoCamera[i].GetFormat7Info(&Finfo, & support);

            FlyCapture2::Error E6 = m_VideoCamera[i].SetVideoModeAndFrameRate(VIDEOMODE_FORMAT7, FRAMERATE_60);

            FlyCapture2::VideoMode pVideoMode;
            FlyCapture2::FrameRate pFrameRate;
            Error E = m_VideoCamera[i].GetVideoModeAndFrameRate(&pVideoMode, &pFrameRate);
            
            F7.height = 480;
            F7.mode = MODE_0;
            F7.offsetX = 0;
            F7.offsetY = 0;
            F7.pixelFormat = PIXEL_FORMAT_MONO8;
            F7.width = 752;
            m_VideoCamera[i].SetFormat7Configuration(&F7, (unsigned int)3008);
        }
        */
		if (i==0) m_CamError = m_VideoCamera[i].StartCapture( OnImageGrabbed, &A );
		if (i==1) m_CamError = m_VideoCamera[i].StartCapture( OnImageGrabbed, &B );
	}
	 m_bInit = true;



	 /*
	 Format7ImageSettings F7;
	 F7.height = 480;
	 F7.mode = MODE_0;
	 F7.offsetX = 0;
	 F7.offsetY = 0;
	 F7.pixelFormat = PIXEL_FORMAT_MONO8;
	 F7.width = 752;
	 m_VideoCamera.SetFormat7Configuration( &F7, (unsigned int)3008 );
	 */
}



PointGrey_Camera::~PointGrey_Camera()
{
	for (int i=0; i<m_NumCameras; i++)
	{
		if (m_VideoCamera[i].IsConnected())
		{
			m_CamError = m_VideoCamera[i].StopCapture();
			m_CamError = m_VideoCamera[i].Disconnect();
		}
	}
}


bool PointGrey_Camera::setFirstCamera( unsigned int serial )
{
	if (m_NumCameras > 1)
	{
		if ( getSerialNumber( 0 ) == serial )	m_bSwapCameras = false;
		if ( getSerialNumber( 1 ) == serial )	m_bSwapCameras = true;
	}
	b_SwapCamera = m_bSwapCameras;
	return m_bSwapCameras;
}


void PointGrey_Camera::restartCamera( int w, int h)
{
	for (int i=0; i<m_NumCameras; i++)
	{
			m_CamError = m_VideoCamera[i].StopCapture();
			m_CamError = m_VideoCamera[i].Disconnect();

			PGRGuid guid;
			BusManager	busMgr;
			unsigned int numCameras;
			m_CamError = busMgr.GetNumOfCameras(&numCameras);
			m_CamError = busMgr.GetCameraFromIndex(0, &guid);
			m_CamError = m_VideoCamera[i].Connect(&guid);
			{
				m_VideoCamera[i].SetVideoModeAndFrameRate( VIDEOMODE_FORMAT7, FRAMERATE_60 );
				Format7ImageSettings F7;
				F7.height = h;
				F7.mode = MODE_0;
				F7.offsetX = 0;
				F7.offsetY = 0;
				F7.pixelFormat = PIXEL_FORMAT_MONO8;
				F7.width = w;
				m_VideoCamera[i].SetFormat7Configuration( &F7, (unsigned int)3008 );
			}
			if (i==0) m_CamError = m_VideoCamera[i].StartCapture( OnImageGrabbed, &A );
			if (i==1) m_CamError = m_VideoCamera[i].StartCapture( OnImageGrabbed, &B );
	}
}

void PointGrey_Camera::setIncrementBuffer( bool bInc )
{
	b_incBuffer = bInc;
}

void PointGrey_Camera::setReferenceFrame( bool bClear, int numFrames )
{
	if (vidReference[0]==NULL) return;

	referenceCNT[0] = numFrames;
	referenceCNT[1] = numFrames;
	bRefClearing = bClear;

	if ( bClear )
	{
		for (int y=0; y<m_height; y++)
		{
			for (int x=0; x<m_width; x++)
			{
				vidReference[0][IDX(x, y)] = __min( 255 - THRESHHOLD, vidFrame[0][IDX(x,y)] );
				vidReference[1][IDX(x, y)] = __min( 255 - THRESHHOLD, vidFrame[1][IDX(x,y)] );
			}
		}
	}
	
}

// call this on a white fraem after a set Reference on a black Frame
/*void PointGrey_Camera::setMaskFrame()
{
	memset( vidMASK, 0, m_width*m_height );

	for (int y=1; y<m_height-1; y++)
	{
		for (int x=1; x<m_width-1; x++)
		{
			if ( vidFrame[IDX(x,y)] > vidReference[0][IDX(x,y)] + THRESHHOLD ) vidMASK[IDX(x,y)] = 1;	// pass test
		}
	}
	
	for (int y=1; y<m_height-1; y++)
	{
		for (int x=1; x<m_width-1; x++)
		{
			if ( vidMASK[IDX(x+1,y)] == 1 )		vidMASK[IDX(x,y)] = 1;		// dilute
			if ( vidMASK[IDX(x-1,y+1)] == 1 )	vidMASK[IDX(x,y)] = 1;		// dilute
			if ( vidMASK[IDX(x,y+1)] == 1 )		vidMASK[IDX(x,y)] = 1;		// dilute
			if ( vidMASK[IDX(x+1,y+1)] == 1 )	vidMASK[IDX(x,y)] = 1;		// dilute
		}
	}
	
}*/

void PointGrey_Camera::clearReferenceFrame( )
{
	memset( vidReference[0], 0, m_width*m_height);
	memset( vidReference[1], 0, m_width*m_height);
}




void PointGrey_Camera::setBrightness( float S )	// Set the shutter property of the camera
{
    Property prop;
    prop.type = BRIGHTNESS;
    m_CamError = m_VideoCamera[0].GetProperty( &prop );
	m_CamError = m_VideoCamera[1].GetProperty( &prop );

    prop.autoManualMode = false;
    prop.absControl = true;
	prop.onOff = true;

    prop.absValue = S;
    m_CamError = m_VideoCamera[0].SetProperty( &prop );
	m_CamError = m_VideoCamera[1].SetProperty( &prop );
}

void PointGrey_Camera::setExposure( float S )	// Set the shutter property of the camera
{
    Property prop;
    prop.type = AUTO_EXPOSURE;
    m_CamError = m_VideoCamera[0].GetProperty( &prop );
	m_CamError = m_VideoCamera[1].GetProperty( &prop );

    prop.autoManualMode = false;
    prop.absControl = true;
	prop.onOff = true;

    prop.absValue = S;
    m_CamError = m_VideoCamera[0].SetProperty( &prop );
	m_CamError = m_VideoCamera[1].SetProperty( &prop );
}


void PointGrey_Camera::setGamma( float S )	// Set the shutter property of the camera
{
    Property prop;
    prop.type = GAMMA;
    m_CamError = m_VideoCamera[0].GetProperty( &prop );
	m_CamError = m_VideoCamera[1].GetProperty( &prop );

    prop.autoManualMode = true;
    prop.absControl = true;
    prop.absValue = S;
    m_CamError = m_VideoCamera[0].SetProperty( &prop );
	m_CamError = m_VideoCamera[1].GetProperty( &prop );
}

void PointGrey_Camera::setShutter( float S )	// Set the shutter property of the camera
{
    Property prop;
    prop.type = SHUTTER;
    m_CamError = m_VideoCamera[0].GetProperty( &prop );
	m_CamError = m_VideoCamera[1].GetProperty( &prop );

    prop.autoManualMode = false;
    prop.absControl = true;

    prop.absValue = S;
    m_CamError = m_VideoCamera[0].SetProperty( &prop );
	m_CamError = m_VideoCamera[1].GetProperty( &prop );
}

void PointGrey_Camera::setGain( float S )	// Set the shutter property of the camera
{
    Property prop;
    prop.type = GAIN;
    m_CamError = m_VideoCamera[0].GetProperty( &prop );
	m_CamError = m_VideoCamera[1].GetProperty( &prop );

    prop.autoManualMode = false;
    prop.absControl = true;

    prop.absValue = S;
    m_CamError = m_VideoCamera[0].SetProperty( &prop );
	m_CamError = m_VideoCamera[1].SetProperty( &prop );
}


void PointGrey_Camera::setFramerate( float S )	// Set the shutter property of the camera
{
    Property prop;
    prop.type = FRAME_RATE;
    m_CamError = m_VideoCamera[0].GetProperty( &prop );
	m_CamError = m_VideoCamera[1].GetProperty( &prop );

    prop.autoManualMode = false;
    prop.absControl = true;

    prop.absValue = S;
    m_CamError = m_VideoCamera[0].SetProperty( &prop );
	m_CamError = m_VideoCamera[1].SetProperty( &prop );
}


void PointGrey_Camera::setMode7( int width, int height)
{
	restartCamera( width, height );
}

void PointGrey_Camera::setDots( int min, int max, int T, int detectionType )	// Set the shutter property of the camera
{
	THRESHHOLD=T;
	MIN_PIX = min;
	MAX_PIX = max;
	DetectionType = detectionType;
}


void PointGrey_Camera::setThreshold( int T )
{
	THRESHHOLD=T;
}






unsigned int PointGrey_Camera::getSerialNumber(int cam)
{
	
	FlyCapture2::CameraInfo info;
	m_VideoCamera[cam].GetCameraInfo(&info);
	return info.serialNumber;
	
}

unsigned int PointGrey_Camera::getFrameCount(int cam)
{
    return frameCNT[cam];
}






