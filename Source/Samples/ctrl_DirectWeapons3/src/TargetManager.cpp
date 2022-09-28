#include "ew_Scene.h"
#include "Controllers/ctrl_DirectWeapons/TargetManager.h"
#include "Controllers/ctrl_DirectWeapons/Target.h"
#include "Util/ewstring.h"
#include "Util/MemoryUtil.h"
#include "Logger/Logger.h"

TargetManager::TargetManager(ew_Scene *pScene)
	: m_pScene(pScene)
{

}


TargetManager::~TargetManager()
{
	for (int i=0; i<m_targets.size(); i++)
	{
		SAFE_DELETE(m_targets[i]);
	}
	//for (TargetMap::iterator i = m_targets.begin(); i != m_targets.end(); i++)
	//	SAFE_DELETE(i->second);
}

bool TargetManager::loadTargets(ewstring path)
{
	WIN32_FIND_DATAA findData;

	//MessageBox(NULL, L"Entering loadTargets", L"Error", MB_OK);
	HANDLE hFind = FindFirstFileA((path + "/*.xml").c_str(), &findData );
	if (hFind)
	{
		//MessageBox(NULL, L"found some target XML", L"Error", MB_OK);
		m_pScene->getLogger()->Log( LOG_INFO, L"load target -  %s", path.getWString().c_str() );

		DmSpriteManager *spriteManager = m_pScene->getSpriteManager();
		spriteManager->load((path + "/targetSprites").c_str());

		do
		{
			//MessageBoxA(NULL, findData.cFileName, "cFilename", MB_OK);
			ewstring targetFile = findData.cFileName;

			Target *t = new Target();
			if (!t->loadTarget(path, targetFile, m_pScene))
			{
				SAFE_DELETE(t);
				m_pScene->getLogger()->Log( LOG_ERROR, L"ctrl_DirectWeapons::loadTargets - error loading target %s", targetFile.getWString().c_str() );
			}
			else
			{
				m_targets.push_back(t);
				//m_targets.insert(TargetMap::value_type(t->getName(), t));
			}

		} while (FindNextFileA(hFind, &findData));

		FindClose(hFind);
	}
	else
		return false;

	return true;
}
/*
 ewstring TargetManager::getTargetName(int index)
 { 
	 TargetMap::iterator i = m_targets.begin(); 
	 for (int j = 0; j < index; j++)
		 i++;
	 return i->first; 
 }
 */
 Target *TargetManager::getTarget(ewstring name) 
 { 
	 for (int i=0; i<m_targets.size(); i++)
	 {
		 if ( m_targets[i]->m_name.compare(name) == 0) return m_targets[i];
	 }
	 return NULL;
	// return m_targets.find(name) == m_targets.end() ? NULL : m_targets.find(name)->second; 
 }
