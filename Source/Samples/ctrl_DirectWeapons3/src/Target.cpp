#include "ew_Scene.h"
#include "Controllers/ctrl_DirectWeapons/Target.h"
#include "TinyXML/tinyxml.h"
#include "Util/ImageUtil.h"

Target::~Target()
{
	SAFE_DELETE_ARRAY(m_pScoreImageData);
}

bool Target::loadTarget(ewstring path, ewstring filename, ew_Scene *pScene)
{
	ewstring fullPath = path + "/" + filename;
	//MessageBox(NULL, path.getWString().c_str(), L"loadTarget", MB_OK);

	//MessageBox(NULL, filename.getWString().c_str(), L"loadTarget", MB_OK);

	//MessageBox(NULL, fullPath.getWString().c_str(), L"loadTarget", MB_OK);

	TiXmlDocument doc;
	if (!doc.LoadFile(fullPath.getWString().c_str()))
		return false;

	//MessageBox(NULL, L"loadTarget B", L"Error", MB_OK);

	TiXmlNode *targetNode = doc.FirstChild(L"target");
	m_name = ewstring(targetNode->ToElement()->Attribute(L"name"));
	m_meshFile = ewstring(targetNode->FirstChild(L"meshFile")->FirstChild()->Value());
	m_spriteFile = ewstring(targetNode->FirstChild(L"sprite")->FirstChild()->Value());
	m_scale = (float)_wtof(targetNode->FirstChild(L"scale")->FirstChild()->Value());
	m_fCenterHeight = (float)_wtof(targetNode->FirstChild(L"centerHeight")->FirstChild()->Value());
	m_scoreImage = ewstring(targetNode->FirstChild(L"scoreImage")->FirstChild()->Value());

	DmSpriteManager &spriteManager = *pScene->getSpriteManager();
	ewstring spriteName = ewstring(L"target_" + m_spriteFile.getWString());
	if (spriteManager.containsSprite(spriteName.getWString().c_str()))
	{
		DmSprite &sprite = spriteManager[spriteName.c_str()];

		m_width = (int)sprite.getColourMapWidth();
		m_height = (int)sprite.getColourMapHeight();

		m_pScoreImageData = new unsigned char[m_width * m_height];
		if (!ImageUtil::LoadRAW(path + "/scoreImages/" + m_scoreImage + ".raw", m_pScoreImageData, m_width * m_height * sizeof(unsigned char)))
		{
			MessageBox(NULL, L"Failed to load scoreimmage", L"Error", MB_OK);
			SAFE_DELETE_ARRAY(m_pScoreImageData);
			return false;
		}
		int HIST[256];
		for (int i=0; i<256; i++) HIST[i] = 0;
		for (int y=0; y<m_height; y++)
		{
			for (int x=0; x<m_width; x++)
			{
				HIST[ m_pScoreImageData[y*m_width+x] ] ++;
			}
		}
		//for (int i=0; i<256; i++) log
		//ew_Scene::GetSingleton()->getLogger()->Log(LOG_INFO, L"%s, %d, %d, %d, %d, %d, %d", m_scoreImage.getWString().c_str(), HIST[0], HIST[1], HIST[2], HIST[3], HIST[4], HIST[5]  );
	}

	//MessageBox(NULL, L"loadTarget sucseed", L"Error", MB_OK);
	return true;
}

unsigned int Target::getScoreAt(int x, int y)
{
	if (!m_pScoreImageData)
		return 0;

	if (x < 0 || y < 0)
		return 0;
	if (x > m_width - 1 || y > m_height -1 )
		return 0;

	return m_pScoreImageData[(m_height - y - 1) * m_width + x];
}

unsigned int Target::getScoreAtNormalizedPos(float x, float y)
{
	return getScoreAt((int)(m_width * (x + 1.0f) * 0.5f + 0.5f), (int)(m_height * (y + 1.0f) * 0.5f + 0.5f));
}