
#include"roads_materials.h"
#include "imgui.h"
#include "imgui_internal.h"


#pragma optimize( "", off )


/*  roadMaterialGroup
    --------------------------------------------------------------------------------------------------------------------*/
void roadMaterialGroup::import(std::string _relativepath)
{
    std::ifstream is(terrafectorEditorMaterial::rootFolder + _relativepath);
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


uint roadMaterialCache::find_insert_material(std::string _path)
{
    replaceAllrm(_path, "\\", "/");

    if (_path.find(terrafectorEditorMaterial::rootFolder) == 0)
    {
        std::string relative = _path.substr(terrafectorEditorMaterial::rootFolder.length());
        for (uint i = 0; i < materialVector.size(); i++)
        {
            if (materialVector[i].relativePath.compare(relative) == 0)
            {
                //fprintf(terrafectorSystem::_logfile, "	ROAD MATERIAL CACHE - found %s \n", _path.substr(_path.find_last_of("\\/") + 1, _path.size()).c_str());
                //fprintf(terrafectorSystem::_logfile, "		%d layers\n", (int)materialVector[i].layers.size());
                //fflush(terrafectorSystem::_logfile);

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
    else
    {
        fprintf(terrafectorSystem::_logfile, "		  failed to find rootpath  (%s)  in   %s\n", terrafectorEditorMaterial::rootFolder.c_str(), _path.c_str());
        fflush(terrafectorSystem::_logfile);
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



void roadMaterialCache::renderGui(Gui* _gui, Gui::Window& _window)
{
    float width = ImGui::GetWindowWidth();
    int numColumns = __max(2, (int)floor(width / 140));

    ImGui::PushFont(_gui->getFont("roboto_32"));
    ImGui::Text("Road materials", materialVector);
    ImGui::PopFont();

    int cnt = 0;
    ImVec2 rootPos = ImGui::GetCursorPos();

    for (auto& material : materialVector)
    {
        ImGui::PushID(777 + cnt);

        uint x = cnt % numColumns;
        uint y = (int)floor(cnt / numColumns);
        ImGui::SetCursorPos(ImVec2(x * 140 + rootPos.x, y * 160 + rootPos.y));
        ImGui::Text(material.displayName.c_str());
        ImGui::SetCursorPos(ImVec2(x * 140 + rootPos.x, y * 160 + 20 + rootPos.y));
        if (material.thumbnail) {
            if (_window.imageButton("testImage", material.thumbnail, float2(128, 128)))
            {
            }
            if (ImGui::IsItemHovered() && (ImGui::IsMouseClicked(0)))
            {
                ImGui::SetWindowFocus();
                bool b = ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID);
                if (b)
                {
                    ImGui::SetDragDropPayload("ROADMATERIAL", &cnt, sizeof(int), ImGuiCond_Once);
                    ImGui::EndDragDropSource();
                }
            }
            else
            {
                if (ImGui::Button("##test", ImVec2(128, 128)))
                {
                    //selectedMaterial = cnt;
                }
            }
            if (ImGui::BeginPopupContextWindow())
            {
                if (ImGui::Selectable("Do Stuff Here")) { ; }
                ImGui::EndPopup();
            }
            
        }

        ImGui::PopID();
        cnt++;
    }

}


