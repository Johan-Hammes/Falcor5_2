
#include"roads_materials.h"
#include "imgui.h"
#include "imgui_internal.h"


#pragma optimize( "", off )

#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}

/*  roadMaterialGroup
    --------------------------------------------------------------------------------------------------------------------*/
bool roadMaterialGroup::import(std::string _relativepath)
{
    std::ifstream is(terrafectorEditorMaterial::rootFolder + _relativepath);
    if (is.fail()) {
        displayName = "failed to load";
        relativePath = _relativepath;
        return false;
    }
    else
    {
        cereal::JSONInputArchive archive(is);
        serialize(archive, 0);
        relativePath = _relativepath;
        displayName = relativePath.substr(relativePath.find_last_of("\\/") + 1);
        displayName = displayName.substr(0, displayName.find_last_of("."));
        return true;
    }
}

void roadMaterialGroup::save()
{
    std::ofstream os(terrafectorEditorMaterial::rootFolder + relativePath);
    cereal::JSONOutputArchive archive(os);
    serialize(archive, 0);
}



/*  roadMaterialCache
    --------------------------------------------------------------------------------------------------------------------*/


uint roadMaterialCache::find_insert_material(std::string _path)
{
    replaceAllrm(_path, "\\", "/");
    replaceAllrm(_path, "//", "/");     // remove double

    if (_path.find(terrafectorEditorMaterial::rootFolder) == 0)
    {
        std::string relative = _path.substr(terrafectorEditorMaterial::rootFolder.length());

        for (uint i = 0; i < materialVector.size(); i++)
        {
            if (materialVector[i].relativePath.compare(relative) == 0)
            {
                // try to load the thumbnail
                if (!materialVector[i].thumbnail) {
                    materialVector[i].thumbnail = Texture::createFromFile(_path + ".jpg", false, true);
                    if (!materialVector[i].thumbnail) fprintf(terrafectorSystem::_logfile, "		Road material - failed to load %s.jpg\n", _path.c_str());
                }
                return i;
            }
        }

        // not found - add new
        fprintf(terrafectorSystem::_logfile, "	roadMaterialCache - add %s\n", _path.c_str());
        materialVector.emplace_back();
        materialVector.back().import(relative);
        materialVector.back().thumbnail = Texture::createFromFile(_path + ".jpg", false, true);

        // load all the terrafector Materials and set
        roadMaterialGroup& current = materialVector.back();

        fprintf(terrafectorSystem::_logfile, "		  roadMaterialCache (%s)  %d layers\n", _path.c_str(), (int)current.layers.size());
        fflush(terrafectorSystem::_logfile);
        for (uint i = 0; i < current.layers.size(); i++)
        {
            std::string file = terrafectorEditorMaterial::rootFolder + current.layers[i].material;
            replaceAllrm(file, "//", "/");     // remove double
            current.layers[i].materialIndex = terrafectorEditorMaterial::static_materials.find_insert_material(file);
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


std::string roadMaterialCache::checkPath(std::string _root, std::string _file)
{
    for (const auto& entry : std::filesystem::recursive_directory_iterator(_root))
    {
        std::string newPath = entry.path().string();
        replaceAllrm(newPath, "\\", "/");

        if (!entry.is_directory() && (newPath.find(_file) != std::string::npos))
        {
            return newPath;
        }
    }
    return "";
}



void roadMaterialCache::reloadMaterials()
{
    for (auto& mat : materialVector)
    {
        if (!std::filesystem::exists(terrafectorEditorMaterial::rootFolder + mat.relativePath))
        {
            std::string filename = mat.relativePath.substr(mat.relativePath.find_last_of("/\\") + 1);
            std::string returnName = checkPath(terrafectorEditorMaterial::rootFolder + "roadMaterials/", filename);
            if (returnName.find(terrafectorEditorMaterial::rootFolder) == 0)
            {
                std::string relative = returnName.substr(terrafectorEditorMaterial::rootFolder.length());

                fprintf(terrafectorSystem::_logfile, "	ROAD MATERIAL CACHE - FILE RELOCATE from  %s     to    %s\n", mat.relativePath.c_str(), relative.c_str());
                fflush(terrafectorSystem::_logfile);

                mat.relativePath = relative;
                mat.save();
            }
        }


        fprintf(terrafectorSystem::_logfile, "	roadMaterialCache - load %s\n", mat.relativePath.c_str());
        fflush(terrafectorSystem::_logfile);
        if (!mat.import(mat.relativePath))
        {
            reFindMaterial(mat);
        }




        roadMaterialCache::getInstance().find_insert_material(terrafectorEditorMaterial::rootFolder + mat.relativePath);
        for (auto& layer : mat.layers)
        {
            std::string file = terrafectorEditorMaterial::rootFolder + layer.material;
            replaceAllrm(file, "//", "/");
            layer.materialIndex = terrafectorEditorMaterial::static_materials.find_insert_material(file);
        }
    }
}


void roadMaterialCache::reFindMaterial(roadMaterialGroup& _material)
{

    std::filesystem::path path;
    FileDialogFilterVec filters = { {"roadMaterial", _material.relativePath.c_str()} };
    if (openFileDialog(filters, path))
    {
        std::string pathstring = path.string();
        replaceAllrm(pathstring, "\\", "/");
        if (pathstring.find(terrafectorEditorMaterial::rootFolder) == 0)
        {
            _material.relativePath = pathstring.substr(terrafectorEditorMaterial::rootFolder.length());
            _material.import(_material.relativePath);
            _material.thumbnail = Texture::createFromFile(pathstring + ".jpg", false, true);
            _material.save();
        }
    }
}


void roadMaterialCache::renameMoveMaterial(roadMaterialGroup& _material)
{
    std::string windowspath = terrafectorEditorMaterial::rootFolder + _material.relativePath;     // to open in that directory
    //replaceAllrm(windowspath, "/", "\\");
    std::filesystem::path path = windowspath;
    

    FileDialogFilterVec filters = { {"roadMaterial"} };
    if (saveFileDialog(filters, path))
    {
        std::string filepath = path.string();
        replaceAllrm(filepath, "\\", "/");

        if (filepath.find(terrafectorEditorMaterial::rootFolder) == 0)
        {
            std::filesystem::rename(terrafectorEditorMaterial::rootFolder + _material.relativePath, filepath);
            std::filesystem::rename(terrafectorEditorMaterial::rootFolder + _material.relativePath + ".jpg", filepath + ".jpg");

            _material.relativePath = filepath.substr(terrafectorEditorMaterial::rootFolder.length());
            _material.save();
        }
    }
}



void roadMaterialCache::renderGui(Gui* _gui, Gui::Window& _window)
{
    static float ICON_SIZE = 64.f;
    static int GRID_SIZE = 70;
    float width = ImGui::GetWindowWidth();
    int numColumns = __max(2, (int)floor(width / GRID_SIZE));

    if (ImGui::Button("small")) { ICON_SIZE = 64.f; GRID_SIZE = 70; }
    ImGui::SameLine();
    if (ImGui::Button("medium")) { ICON_SIZE = 128.f; GRID_SIZE = 135; }
    ImGui::SameLine();
    if (ImGui::Button("large")) { ICON_SIZE = 200.f; GRID_SIZE = 210; }
    ImGui::SameLine();


    int cnt = 0;
    ImVec2 rootPos = ImGui::GetCursorPos();


    struct sortDisplay
    {
        bool operator < (const sortDisplay& str) const
        {
            return (name < str.name);
        }
        std::string name;
        int index;
    };

    std::vector<sortDisplay> displaySortMap;

    sortDisplay S;
    for (auto& material : materialVector)
    {
        S.name = material.relativePath;
        S.index = cnt;
        displaySortMap.push_back(S);
        cnt++;
    }
    std::sort(displaySortMap.begin(), displaySortMap.end());


    std::string path = "";

    int subCount = 0;
    for (cnt = 0; cnt < materialVector.size(); cnt++)
    {
        std::string  thisPath = displaySortMap[cnt].name.substr(14, displaySortMap[cnt].name.find_last_of("\\/") - 14);
        if (thisPath != path) {
            ImGui::PushFont(_gui->getFont("roboto_20"));
            ImGui::NewLine();
            ImGui::Text(thisPath.c_str());
            ImGui::PopFont();
            path = thisPath;
            subCount = 0;
            rootPos = ImGui::GetCursorPos();
        }
        roadMaterialGroup& material = materialVector[displaySortMap[cnt].index];
        ImGui::PushID(777 + cnt);

        uint x = subCount % numColumns;
        uint y = (int)floor(subCount / numColumns);
        ImGui::SetCursorPos(ImVec2(x * GRID_SIZE + rootPos.x, y * GRID_SIZE + rootPos.y));
        if (material.thumbnail) {
            if (_window.imageButton("testImage", material.thumbnail, float2(ICON_SIZE, ICON_SIZE))) {}
        }
        else
        {
            if (ImGui::Button("##test", ImVec2(ICON_SIZE, ICON_SIZE))) {}
        }
        TOOLTIP(material.displayName.c_str());

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::Selectable("rename / move")) { renameMoveMaterial(material); }

            
            if (ImGui::Selectable("Explorer")) {
                std::string cmdExp = "explorer " + terrafectorEditorMaterial::rootFolder + material.relativePath;
                cmdExp = cmdExp.substr(0, cmdExp.find_last_of("\\/") + 1);
                replaceAllrm(cmdExp, "/", "\\");
                system(cmdExp.c_str());
            }
            //TOOLTIP()

            ImGui::EndPopup();
        }

        ImGui::PopID();
        subCount++;
    }

}


