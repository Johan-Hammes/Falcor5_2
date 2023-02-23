
#include"roads_materials.h"
#include "imgui.h"
#include "imgui_internal.h"


#pragma optimize( "", off )


/*  roadMaterialGroup
    --------------------------------------------------------------------------------------------------------------------*/
void roadMaterialGroup::import(std::string _relativepath)
{
    std::ifstream is(terrafectorEditorMaterial::rootFolder + _relativepath );
    if (is.fail()) {
        displayName = "failed to load";
        relativePath = _relativepath;
    }
    else
    {
        cereal::JSONInputArchive archive(is);
        serialize(archive, 0);

        relativePath = _relativepath;
    }
}



/*  roadMaterialCache
    --------------------------------------------------------------------------------------------------------------------*/
uint roadMaterialCache::find_insert_material(const std::string _path)
{
    if (_path.find(terrafectorEditorMaterial::rootFolder) == 0)
    {
        std::string relative = _path.substr(terrafectorEditorMaterial::rootFolder.length());
        for (uint i = 0; i < materialVector.size(); i++)
        {
            if (materialVector[i].relativePath.compare(relative) == 0)
            {
                fprintf(terrafectorSystem::_logfile, "	ROAD MATERIAL CACHE - found %s \n", _path.substr(_path.find_last_of("\\/") + 1, _path.size()).c_str());
                fprintf(terrafectorSystem::_logfile, "		%d layers\n", (int)materialVector[i].layers.size());
                fflush(terrafectorSystem::_logfile);

                // try to load the thumbnail
                if (!materialVector[i].thumbnail) {
                    materialVector[i].thumbnail = Texture::createFromFile(_path + ".jpg", false, true);
                }
                return i;
            }
        }

        // else add new
        fprintf(terrafectorSystem::_logfile, "	ROAD MATERIAL CACHE - add %s\n", _path.c_str());
        fflush(terrafectorSystem::_logfile);

        materialVector.emplace_back();
        materialVector.back().import(relative);
        materialVector.back().thumbnail = Texture::createFromFile(_path + ".jpg", false, true);

        // load all the terrafector Materials and set
        roadMaterialGroup& current = materialVector.back();
        current.import(relative);
        current.thumbnail = Texture::createFromFile(_path + ".jpg", false, true);

        fprintf(terrafectorSystem::_logfile, "		  roadMaterialCache (%s)  %d layers\n", _path.c_str(), (int)current.layers.size());
        fflush(terrafectorSystem::_logfile);
        for (uint i = 0; i < current.layers.size(); i++)
        {
            current.layers[i].materialIndex = terrafectorEditorMaterial::static_materials.find_insert_material(terrafectorEditorMaterial::rootFolder + current.layers[i].material);
        }

        return (uint)(materialVector.size() - 1);
    }
    return 0;	//?? is this right
}


void roadMaterialCache::reloadMaterials()
{
    for (auto& mat : materialVector)
    {
        // reload from disk - account for changes
        mat.import(mat.relativePath);

        roadMaterialCache::getInstance().find_insert_material(terrafectorEditorMaterial::rootFolder + mat.relativePath);
        for (auto& layer : mat.layers)
        {
            layer.materialIndex = terrafectorEditorMaterial::static_materials.find_insert_material(terrafectorEditorMaterial::rootFolder + layer.material);
        }
    }
}



void roadMaterialCache::renderGui(Gui* _gui)
{
    ImGui::PushFont(_gui->getFont("roboto_32"));
    ImGui::Text("Road materials [%d]", materialVector.size());
    ImGui::PopFont();
}


