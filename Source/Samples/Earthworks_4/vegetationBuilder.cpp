#include "vegetationBuilder.h"
#include "imgui.h"
#include "PerlinNoise.hpp"      //https://github.com/Reputeless/PerlinNoise/blob/master/PerlinNoise.hpp

#pragma optimize("", off)

#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}


materialCache_plants _plantMaterial::static_materials_veg;
std::string materialCache_plants::lastFile;


_plantBuilder* _rootPlant::selectedPart = nullptr;
_plantMaterial* _rootPlant::selectedMaterial = nullptr;

std::mt19937 _rootPlant::generator(100);
std::uniform_real_distribution<> _rootPlant::rand_1(-1.f, 1.f);
std::uniform_real_distribution<> _rootPlant::rand_01(0.f, 1.f);
std::uniform_int_distribution<> _rootPlant::rand_int(0, 65535);


float ribbonVertex::objectScale = 0.002f;  //0.002 for trees  // 32meter block 2mm presision
float ribbonVertex::radiusScale = 0.20f;//  so biggest radius now objectScale / 2.0f;
float ribbonVertex::O = 16384.0f * ribbonVertex::objectScale * 0.5f;
float3 ribbonVertex::objectOffset = float3(O, O * 0.5f, O);

std::vector<ribbonVertex>    ribbonVertex::ribbons;
std::vector<ribbonVertex8>    ribbonVertex::packed;
bool ribbonVertex::pushStart = false;



void replaceAllVEG(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}


void ribbonVertex::setup(float scale, float radius, float3 offset)
{
    objectScale = scale;
    radiusScale = radius;
    objectOffset = offset;
}

ribbonVertex8 ribbonVertex::pack()
{
    int x14 = ((int)((position.x + objectOffset.x) / objectScale)) & 0x3fff;
    int y16 = ((int)((position.y + objectOffset.y) / objectScale)) & 0xffff;
    int z14 = ((int)((position.z + objectOffset.z) / objectScale)) & 0x3fff;

    int anim8 = (int)floor(__min(1.f, __max(0.f, anim_blend)) * 255.f);

    int u7 = ((int)(uv.x * 127.f)) & 0x7f;
    int v15 = ((int)(uv.y * 255.f)) & 0x7fff;
    //int radius8 = (__min(255, (int)(radius  / radiusScale))) & 0xff;        // FIXME POW for better distribution
    //floar Rtemp = radius / radiusScale;
    int radius8 = (int)(pow(__min(1.0f, radius / radiusScale), 0.5f) * 255.f) & 0xff;        // square

    float up_yaw = atan2(bitangent.z, bitangent.x) + 3.1415926535897932;
    float up_pitch = atan2(bitangent.y, length(float2(bitangent.x, bitangent.z))) + 1.570796326794;
    int up_yaw9 = ((int)(up_yaw * 81.487)) & 0x1ff;
    int up_pitch8 = ((int)(up_pitch * 81.487)) & 0xff;

    float left_yaw = atan2(tangent.z, tangent.x) + 3.1415926535897932;
    float left_pitch = atan2(tangent.y, length(float2(tangent.x, tangent.z))) + 1.570796326794;
    int left_yaw9 = ((int)(left_yaw * 81.487)) & 0x1ff;
    int left_pitch8 = ((int)(left_pitch * 81.487)) & 0xff;
    {
        float plane, x, y, z;
        z = sin((left_yaw9 - 256) * 0.01227);
        x = cos((left_yaw9 - 256) * 0.01227);
        y = sin((left_pitch8 - 128) * 0.01227);
        plane = cos((left_pitch8 - 128) * 0.01227);
        float3 reconstruct = float3(plane * x, y, plane * z);
        if (dot(reconstruct, tangent) < 0.98)
        {
            bool bCm = true;
        }
    }


    float coneyaw = atan2(lightCone.z, lightCone.x) + 3.1415926535897932;
    float conepitch = atan2(lightCone.y, length(float2(lightCone.x, lightCone.z))) + 1.570796326794;
    int coneYaw9 = ((int)(coneyaw * 81.17)) & 0x1ff;
    int conePitch8 = ((int)(conepitch * 81.17)) & 0xff;
    int cone7 = (int)((lightCone.w + 1.f) * 63.5f);
    int depth8 = (int)(clamp(lightDepth, 0.f, 2.0f) * 127.5f);


    uint albedo = (int)clamp((albedoScale - 0.1f) / 0.008f, 0.f, 255.f);
    uint translucency = (int)clamp((translucencyScale - 0.1f) / 0.008f, 0.f, 255.f);
    


    ribbonVertex8 p;

    p.a = ((type & 0x1) << 31) + (startBit << 30) + (x14 << 16) + y16;
    p.b = (z14 << 18) + ((material & 0x3ff) << 8) + anim8;
    p.c = (up_yaw9 << 23) + (up_pitch8 << 15) + v15;
    p.d = (left_yaw9 << 23) + (left_pitch8 << 15) + (u7 << 8) + radius8;
    p.e = (coneYaw9 << 23) + (conePitch8 << 15) + (cone7 << 8) + depth8;
    p.f = ((int)(ambientOcclusion * 255) << 24) + (shadow << 16) + (albedo << 8) + translucency;
    return p;
}





std::string materialCache_plants::clean(const std::string _s)
{
    std::string str = _s;
    size_t start_pos = 0;
    while ((start_pos = str.find("\\", start_pos)) != std::string::npos) {
        str.replace(start_pos, 2, "/");
        start_pos += 1;
    }
    return str;
}

int materialCache_plants::find_insert_material(const std::string _path, const std::string _name, bool _forceReload)
{
    std::filesystem::path fullPath = _path + _name + ".vegetationMaterial";
    if (std::filesystem::exists(fullPath))
    {
        return find_insert_material(fullPath, _forceReload);
    }
    else
    {
        // Now we have to search, but use the first one we find
        std::string fullName = _name + ".vegetationMaterial";
        fullPath = terrafectorEditorMaterial::rootFolder + "/vegetationMaterials";
        for (const auto& entry : std::filesystem::recursive_directory_iterator(fullPath))
        {
            std::string subPath = clean(entry.path().string());
            if (subPath.find(fullName) != std::string::npos)
            {
                return find_insert_material(subPath, _forceReload);
            }
        }
    }

    fprintf(terrafectorSystem::_logfile, "error : vegetation material - %s does not exist\n", _name.c_str());
    return -1;
}


// force a new one? copy last and force a save ?
int materialCache_plants::find_insert_material(const std::filesystem::path _path, bool _forceReload)
{
    for (uint i = 0; i < materialVector.size(); i++)
    {
        if (materialVector[i].fullPath.compare(_path) == 0)
        {
            if (_forceReload)
            {
                materialVector[i].import(_path);
                materialVector[i].makeRelative(_path);       // not sure f needed but its below, fols down deeper
            }
            return i;
        }
    }

    // else add new
    if (_path.string().length() > 5 && std::filesystem::exists(_path))
    {
        uint materialIndex = (uint)materialVector.size();
        _plantMaterial& material = materialVector.emplace_back();
        material.import(_path);
        material.makeRelative(_path);
        fprintf(terrafectorSystem::_logfile, "add vegeatation material[%d] - %s\n", materialIndex, _path.filename().string().c_str());
        return materialIndex;
    }

    return -1;
}



int materialCache_plants::find_insert_texture(const std::filesystem::path _path, bool isSRGB, bool _forceReload)
{
    if (std::filesystem::is_directory(_path)) return -1;     // Its a directory not a file, ignore

    modified = true;

    for (uint i = 0; i < textureVector.size(); i++)
    {
        if (textureVector[i]->getSourcePath().compare(_path) == 0)
        {
            if (_forceReload)
            {
                textureVector[i] = Texture::createFromFile(textureVector[i]->getSourcePath(), true, isSRGB);
                textureVector[i]->setSourcePath(_path);
                textureVector[i]->setName(_path.filename().string());
                // FIXME can we save a timestamp and only relaod if that has changed
            }
            return i;
        }
    }


    std::string ddsFilename = _path.string();
    if (_path.string().find(".dds") == std::string::npos)
    {
        ddsFilename = _path.string() + ".earthworks.dds";
    }
    if (!std::filesystem::exists(ddsFilename))
    {
        std::string resource = terrafectorEditorMaterial::rootFolder;
        replaceAllVEG(resource, "/", "\\");
        std::string temp = resource + "Compressonator\\temp_mip.dds ";
        std::string comprs = resource + "Compressonator\\CompressonatorCLI  ";
        std::string pathOnly = ddsFilename.substr(0, ddsFilename.find_last_of("\\/") + 1);

        std::string cmdExp = comprs + " -miplevels 6  \"" + _path.string() + "\"  " + temp;
        replaceAllVEG(cmdExp, "/", "\\");
        fprintf(terrafectorSystem::_logfile, "%s\n", cmdExp.c_str());
        system(cmdExp.c_str());
        if (isSRGB)
        {
            std::string cmdExp2 = comprs + " -fd BC7 -Quality 0.01 " + temp + ddsFilename;
            replaceAllVEG(cmdExp2, "/", "\\");
            fprintf(terrafectorSystem::_logfile, "%s\n", cmdExp2.c_str());
            system(cmdExp2.c_str());
        }
        else
        {
            std::string cmdExp2 = comprs + " -fd BC6H " + temp + ddsFilename;
            replaceAllVEG(cmdExp2, "/", "\\");
            fprintf(terrafectorSystem::_logfile, "%s\n", cmdExp2.c_str());
            system(cmdExp2.c_str());

        }
    }
    Texture::SharedPtr tex = Texture::createFromFile(ddsFilename, true, isSRGB);
    //Texture::SharedPtr tex = Texture::createFromFile(_path.string(), true, isSRGB);
    if (tex)
    {
        tex->setSourcePath(_path);
        tex->setName(_path.filename().string());
        textureVector.emplace_back(tex);

        float compression = 4.0f;
        if (isSRGB) compression = 4.0f;

        texMb += (float)(tex->getWidth() * tex->getHeight() * 4.0f * 1.333f) / 1024.0f / 1024.0f / compression;	// for 4:1 compression + MIPS

        fprintf(terrafectorSystem::_logfile, "%s\n", tex->getName().c_str());

        return (uint)(textureVector.size() - 1);
    }
    else
    {
        fprintf(terrafectorSystem::_logfile, "failed %s \n", _path.string().c_str());
        return -1;
    }


}


Texture::SharedPtr materialCache_plants::getDisplayTexture()
{
    if (dispTexIndex >= 0) {
        return textureVector.at(dispTexIndex);
    }
    return nullptr;
}



void materialCache_plants::setTextures(ShaderVar& _var)
{
    for (size_t i = 0; i < textureVector.size(); i++)
    {
        _var[i] = textureVector[i];
    }

    modified = false;
}


// FIXME could also just do the individual one
//terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials = Buffer::createStructured(sizeof(TF_material), 2048); // FIXME hardcoded
void materialCache_plants::rebuildStructuredBuffer()
{
    size_t offset = 0;
    for (auto& mat : materialVector)
    {
        sb_vegetation_Materials->setBlob(&mat._constData, offset, sizeof(sprite_material));
        offset += sizeof(sprite_material);
    }
}






void materialCache_plants::renderGui(Gui* mpGui, Gui::Window& _window)
{
    auto& style = ImGui::GetStyle();
    ImGuiStyle oldStyle = style;
    float width = ImGui::GetWindowWidth();
    int numColumns = __max(2, (int)floor(width / 140));

    ImGui::PushItemWidth(128);

    if (ImGui::Button("import")) {
        std::filesystem::path path = lastFile;
        FileDialogFilterVec filters = { {"vegetationMaterial"} };
        if (openFileDialog(filters, path))
        {
            find_insert_material(path);
            lastFile = path.string();
        }
    }

    if (ImGui::Button("rebuild")) {
        rebuildStructuredBuffer();
    }

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
    int cnt = 0;
    for (auto& material : materialVector)
    {
        S.name = material.fullPath.string().substr(terrafectorEditorMaterial::rootFolder.length() - 1);
        S.index = cnt;
        displaySortMap.push_back(S);
        cnt++;
    }
    std::sort(displaySortMap.begin(), displaySortMap.end());


    {
        std::string path = "";

        int subCount = 0;
        for (cnt = 0; cnt < materialVector.size(); cnt++)
        {
            std::string  thisPath = "other";
            if (displaySortMap[cnt].name.find("vegetationMaterials") != std::string::npos)
            {
                thisPath = displaySortMap[cnt].name.substr(21, displaySortMap[cnt].name.find_last_of("\\/") - 22);
            }
            if (thisPath != path) {
                ImGui::PushFont(mpGui->getFont("roboto_32"));
                ImGui::NewLine();
                ImGui::Text(thisPath.c_str());
                ImGui::PopFont();
                path = thisPath;
                subCount = 0;
                rootPos = ImGui::GetCursorPos();
            }
            _plantMaterial& material = materialVector[displaySortMap[cnt].index];

            ImGui::PushID(777 + cnt);
            {
                if (ImGui::Button(material.displayName.c_str()))
                {
                    selectedMaterial = displaySortMap[cnt].index;
                }
                /*
                uint x = subCount % numColumns;
                uint y = (int)floor(subCount / numColumns);
                ImGui::SetCursorPos(ImVec2(x * 140 + rootPos.x, y * 160 + rootPos.y));

                int size = material.displayName.size() - 19;
                if (size >= 0)
                {
                    ImGui::Text((material.displayName.substr(0, 19) + "..").c_str());
                }
                else
                {
                    ImGui::Text(material.displayName.c_str());
                }

                ImGui::SetCursorPos(ImVec2(x * 140 + rootPos.x, y * 160 + 20 + rootPos.y));
                if (material.thumbnail) {
                    if (_window.imageButton("testImage", material.thumbnail, float2(128, 128)))
                    {
                        selectedMaterial = displaySortMap[cnt].index;
                    }
                }
                else
                {
                    style.Colors[ImGuiCol_Button] = ImVec4(material._constData.albedoScale.x, material._constData.albedoScale.y, material._constData.albedoScale.z, 1.f);
                    if (ImGui::Button("##test", ImVec2(128, 128)))
                    {
                        selectedMaterial = displaySortMap[cnt].index;
                    }
                }
                style.Colors[ImGuiCol_Button] = ImVec4(0.01f, 0.01f, 0.01f, 0.7f);

                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::Selectable("Explorer")) {
                        std::string cmdExp = "explorer " + material.fullPath.string();  //
                        cmdExp = cmdExp.substr(0, cmdExp.find_last_of("\\/") + 1);
                        replaceAll(cmdExp, "//", "/");
                        replaceAll(cmdExp, "/", "\\");
                        system(cmdExp.c_str());
                    }

                    ImGui::EndPopup();
                }

                if (material._constData.materialType == 1)
                {
                    ImGui::SetCursorPos(ImVec2(x * 140 + rootPos.x, y * 160 + 20 + 10 + rootPos.y));
                    ImGui::Selectable("MULTI");
                    ImGui::LabelText("m1", "MULTI");
                }

                */
            }
            ImGui::PopID();
            subCount++;
        }
    }




    style = oldStyle;
}







void materialCache_plants::renderGuiTextures(Gui* mpGui, Gui::Window& _window)
{
    auto& style = ImGui::GetStyle();
    ImGuiStyle oldStyle = style;
    float width = ImGui::GetWindowWidth();
    int numColumns = __max(2, (int)floor(width / 140));


    char text[1024];
    ImGui::PushFont(mpGui->getFont("roboto_26"));
    ImGui::Text("Tex [%d]   %3.1fMb", (int)textureVector.size(), texMb);
    ImGui::PopFont();


    ImGui::PushFont(mpGui->getFont("roboto_20"));
    {
        for (uint i = 0; i < textureVector.size(); i++)
        {
            ImGui::NewLine();
            ImGui::SameLine(40);

            Texture* pT = textureVector[i].get();
            sprintf(text, "%s##%d", pT->getName().c_str(), i);

            if (ImGui::Selectable(text)) {}

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%d  :  %s\n%d x %d", i, pT->getSourcePath().c_str(), pT->getWidth(), pT->getHeight());
                _plantMaterial::static_materials_veg.dispTexIndex = i;
            }

            //if (ImGui::BeginPopup(text)) {
            if (ImGui::BeginPopupContextItem())
            {
                std::string cmdExp = "explorer " + pT->getSourcePath().string();
                std::string cmdPs = "\"C:\\Program Files\\Adobe\\Adobe Photoshop 2022\\Photoshop.exe\" " + pT->getSourcePath().string();
                if (ImGui::Selectable("Explorer")) { system(cmdExp.c_str()); }
                if (ImGui::Selectable("Photoshop")) { system(cmdPs.c_str()); }
                ImGui::Separator();

                /*
                std::string texPath = pT->getSourcePath().string();
                texPath = texPath.substr(terrafectorEditorMaterial::rootFolder.size());
                int matCnt = 0;
                for (auto material : materialVector)
                {
                    for (int j = 0; j < 9; j++)
                    {
                        if (material.textureNames[j].size() > 0)
                        {
                            if (texPath.find(material.textureNames[j]) != std::string::npos)
                            {
                                if (ImGui::Selectable(material.displayName.c_str())) { selectedMaterial = matCnt; }
                            }
                        }
                    }
                    matCnt++;
                }
                */
                ImGui::EndPopup();
            }

        }
    }
    ImGui::PopFont();

    style = oldStyle;
}



bool materialCache_plants::renderGuiSelect(Gui* mpGui)
{
    if (selectedMaterial >= 0 && selectedMaterial < materialVector.size())
    {
        materialVector[selectedMaterial].renderGui(mpGui);
        return true;
    }
    else
    {
        ImGui::Text("default material");
        static _plantMaterial M;
        M.renderGui(mpGui);
    }

    return false;
}









void _plantMaterial::renderGui(Gui* _gui)
{
    ImGui::PushFont(_gui->getFont("roboto_20"));
    {

        ImGui::Text(displayName.c_str());

        ImGui::SetNextItemWidth(100);
        if (ImGui::Button("load")) { import(); }

        ImGui::SameLine(0, 30);
        ImGui::SetNextItemWidth(100);
        if (ImGui::Button("save")) { eXport(); }

        ImGui::PushID(9990);
        if (ImGui::Selectable(albedoName.c_str())) { loadTexture(0); }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("ALBEDO");
        }
        ImGui::PopID();


        ImGui::PushID(9991);
        if (ImGui::Selectable(alphaName.c_str())) { loadTexture(1); }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("ALPHA");
        }
        ImGui::PopID();


        ImGui::PushID(9992);
        if (ImGui::Selectable(translucencyName.c_str())) { loadTexture(2); }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("TRANSLUCENCY");
        }
        ImGui::PopID();


        ImGui::PushID(9993);
        if (ImGui::Selectable(normalName.c_str())) { loadTexture(3); }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("NORMAL");
        }
        ImGui::PopID();

        ImGui::DragFloat("Translucency", &_constData.translucency, 0.01f, 0, 1);
        ImGui::DragFloat("alphaPow", &_constData.alphaPow, 0.01f, 0.1f, 1);

        ImGui::NewLine();
        ImGui::Text("%d rgb", _constData.albedoTexture);
        ImGui::Text("%d a", _constData.alphaTexture);
        ImGui::Text("%d t", _constData.translucencyTexture);
        ImGui::Text("%d N", _constData.normalTexture);
    }
    ImGui::PopFont();
}


void _plantMaterial::makeRelative(std::filesystem::path _path)
{
    relativePath = materialCache::getRelative(_path.string());
}

void _plantMaterial::import(std::filesystem::path _path, bool _replacePath)
{
    std::ifstream is(_path);
    if (is.fail()) {
        displayName = "failed to load";
        fullPath = _path;
    }
    else
    {
        cereal::JSONInputArchive archive(is);
        serialize(archive, 100);

        if (_replacePath) fullPath = _path;
        displayName = _path.filename().string();
        reloadTextures();
        isModified = false;
    }
}
void _plantMaterial::import(bool _replacePath)
{
    std::filesystem::path path = materialCache_plants::lastFile;
    FileDialogFilterVec filters = { {"vegetationMaterial"} };
    if (openFileDialog(filters, path))
    {
import(path, _replacePath);
        materialCache_plants::lastFile = path.string();
    }
}
void _plantMaterial::save()
{
    std::ofstream os(fullPath);
    cereal::JSONOutputArchive archive(os);
    serialize(archive, 100);
}
void _plantMaterial::eXport(std::filesystem::path _path)
{
    std::ofstream os(_path);
    cereal::JSONOutputArchive archive(os);
    serialize(archive, 100);
    isModified = false;
}
void _plantMaterial::eXport()
{
    std::filesystem::path path = materialCache_plants::lastFile;
    FileDialogFilterVec filters = { {"vegetationMaterial"} };
    if (saveFileDialog(filters, path))
    {
        eXport(path);
        materialCache_plants::lastFile = path.string();
    }
}
void _plantMaterial::reloadTextures()
{
    bool FORCE_RELOAD = true;
    _constData.albedoTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + albedoPath, true, FORCE_RELOAD);
    _constData.alphaTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + alphaPath, true, FORCE_RELOAD);
    _constData.normalTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + normalPath, false, FORCE_RELOAD);
    _constData.translucencyTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + translucencyPath, false, FORCE_RELOAD);
}





void _plantMaterial::loadTexture(int idx)
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"png"}, {"jpg"}, {"tga"}, {"exr"}, {"dds"}, {"tif"} }; //??? only DDS
    if (openFileDialog(filters, path))
    {
        std::string P = path.string();
        replaceAllVEG(P, "\\", "/");
        if (P.find(terrafectorEditorMaterial::rootFolder) == 0) {
            std::string relative = P.substr(terrafectorEditorMaterial::rootFolder.length());
            switch (idx)
            {
            case 0:
                albedoPath = relative;
                albedoName = path.filename().string();
                _constData.albedoTexture = static_materials_veg.find_insert_texture(P, true);
                break;
            case 1:
                alphaPath = relative;
                alphaName = path.filename().string();
                _constData.alphaTexture = static_materials_veg.find_insert_texture(P, false);
                break;
            case 2:
                translucencyPath = relative;
                translucencyName = path.filename().string();
                _constData.translucencyTexture = static_materials_veg.find_insert_texture(P, false);
                break;
            case 3:
                normalPath = relative;
                normalName = path.filename().string();
                _constData.normalTexture = static_materials_veg.find_insert_texture(P, false);
                break;
            }
        }
    }
}







void _vegMaterial::loadFromFile()
{
    std::filesystem::path filepath;
    FileDialogFilterVec filters = { {"vegetationMaterial"} };
    if (openFileDialog(filters, filepath))
    {
        index = _plantMaterial::static_materials_veg.find_insert_material(filepath);
        path = _plantMaterial::static_materials_veg.materialVector[index].relativePath;
        name = filepath.filename().string().substr(0, filepath.filename().string().length() - 19);
    }
}

void _vegMaterial::reload()
{
    // force a reload
    bool FORCE_RELOAD = true;
    index = _plantMaterial::static_materials_veg.find_insert_material(terrafectorEditorMaterial::rootFolder + path, FORCE_RELOAD);
}

bool _vegMaterial::renderGui(uint& gui_id)
{
    ImGui::PushID(gui_id);
    if (ImGui::Selectable(name.c_str())) loadFromFile();
    std::string tool = path + "\n{" + std::to_string(index) + "}";
    TOOLTIP(tool.c_str());
    ImGui::DragFloat2("albedo", &albedoScale.x, 0.01f, 0.1f, 2.0f);
    ImGui::DragFloat2("translucency", &translucencyScale.x, 0.01f, 0.1f, 2.0f);
    ImGui::PopID();
    gui_id++;

    return false;
}


// FXME can I amstarct this away mnore, and combine it with the load at the bottom of the file that is amlmost identical
void _plantRND::loadFromFile()
{
    std::filesystem::path filepath;
    FileDialogFilterVec filters = { {"leaf"}, {"twig"} };
    if (openFileDialog(filters, filepath))
    {
        if (filepath.string().find("leaf") != std::string::npos) { plantPtr.reset(new _leafBuilder); type = P_LEAF; }
        if (filepath.string().find("twig") != std::string::npos) { plantPtr.reset(new _twigBuilder);  type = P_TWIG; }

        path = materialCache::getRelative(filepath.string());
        name = filepath.filename().string();
        plantPtr->path = path;
        plantPtr->name = name;
        plantPtr->loadPath();
    }
}


void _plantRND::reload()
{
    switch (type)
    {
    case P_LEAF:   plantPtr.reset(new _leafBuilder);   break;
    case P_TWIG:   plantPtr.reset(new _twigBuilder);   break;
    }

    plantPtr->path = path;
    plantPtr->name = name;
    plantPtr->loadPath();
}


void _plantRND::renderGui(uint& gui_id)
{
    if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed))
    {
        TOOLTIP(path.c_str());
        if (ImGui::IsItemClicked())  loadFromFile();
        ImGui::TreePop();
    }
}


Gui* _plantBuilder::_gui;



template <class T>     T randomVector<T>::get()
{
    static int rnd_idx = 0;
    rnd_idx += _rootPlant::rand_int(_rootPlant::generator);
    rnd_idx %= data.size();
    return data[rnd_idx];
}

template <class T>     void randomVector<T>::renderGui(char* name, uint& gui_id)
{
    bool first = true;
    if (ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
    {
        if (ImGui::Button(".  +  .")) { data.emplace_back(); }
        ImGui::SameLine(0, 50);
        if (ImGui::Button(".  -  .")) { if (data.size() > 1) data.pop_back(); }

        for (auto& D : data) D.renderGui(gui_id);

        ImGui::TreePop();
    }
}




bool anyChange = true;

ImVec4 twig_color = ImVec4(0.07f, 0.03f, 0.0f, 1);
ImVec4 leaf_color = ImVec4(0.02f, 0.07f, 0.01f, 1);
ImVec4 mat_color = ImVec4(0.0f, 0.01f, 0.07f, 1);

#define CLICK_PART if (ImGui::IsItemClicked()) { _rootPlant::selectedPart = this; _rootPlant::selectedMaterial = nullptr; }
#define CLICK_MATERIAL if (ImGui::IsItemClicked()) { _rootPlant::selectedPart = nullptr; _rootPlant::selectedMaterial = &mat; }
#define TOOLTIP_PART(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}
#define TOOLTIP_MATERIAL(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}
//#define TREE_MAT(x)  style.Colors[ImGuiCol_HeaderActive] = mat_color; ImGui::TreeNodeEx(x, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed);
//#define TREE_LEAF(x)  style.Colors[ImGuiCol_HeaderActive] = leaf_color; if(ImGui::TreeNodeEx(x, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed)) 
//#define TREE_TWIG(x)  style.Colors[ImGuiCol_HeaderActive] = twig_color; if(ImGui::TreeNodeEx(x, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed))

#define TREE_LINE(x,t)  if(ImGui::TreeNodeEx(x, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed)) 
//                        TOOLTIP(t);


#define R_LENGTH(name,data,t1,t2)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(80, 0); \
                    ImGui::SetNextItemWidth(80);    \
                    if (ImGui::DragFloat("##X", &data.x, 1.f, 0, 300, "%2.2fmm")) changed = true; \
                    TOOLTIP(t1); \
                    ImGui::SameLine(0, 10); \
                    ImGui::SetNextItemWidth(80);    \
                    if (ImGui::DragFloat("##Y", &data.y, 0.01f, 0, 1, "%1.3f")) changed = true; \
                    TOOLTIP(t2); \
                    ImGui::PopID(); \
                    gui_id ++;

#define R_CURVE(name,data,t1,t2)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(80, 0); \
                    ImGui::SetNextItemWidth(80);    \
                    if (ImGui::DragFloat("##X", &data.x, 0.01f, -5, 5, "%1.2f rad")) changed = true; \
                    TOOLTIP(t1); \
                    ImGui::SameLine(0, 10); \
                    ImGui::SetNextItemWidth(80);    \
                    if (ImGui::DragFloat("##Y", &data.y, 0.01f, 0, 5, "%1.3f rad")) changed = true; \
                    TOOLTIP(t2); \
                    ImGui::PopID(); \
                    gui_id ++;


#define R_VERTS(name,data)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(80, 0); \
                    ImGui::SetNextItemWidth(80);    \
                    if (ImGui::DragInt("##X", &data.x, 0.1f, 2, 10)) changed = true; \
                    ImGui::SameLine(0, 10); \
                    ImGui::SetNextItemWidth(80);    \
                    if (ImGui::DragInt("##Y", &data.y, 0.1f, 3, 32)) changed = true; \
                    ImGui::PopID(); \
                    gui_id ++;

#define R_INT(name,data,min,max,tooltip)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(80, 0); \
                    ImGui::SetNextItemWidth(80);    \
                    if (ImGui::DragInt("##X", &data, 0.1f, min, max)) changed = true; \
                    TOOLTIP(tooltip); \
                    ImGui::PopID(); \
                    gui_id ++;

#define R_FLOAT(name,data,min,max,tooltip)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(80, 0); \
                    ImGui::SetNextItemWidth(80);    \
                    if (ImGui::DragFloat("##X", &data, 0.1f, min, max)) changed = true; \
                    TOOLTIP(tooltip); \
                    ImGui::PopID(); \
                    gui_id ++;




// _leafBuilder
// -----------------------------------------------------------------------------------------------------------------------------------


void _leafBuilder::loadPath()
{
    std::ifstream is(terrafectorEditorMaterial::rootFolder + path);
    cereal::JSONInputArchive archive(is);
    archive(*this);
    changed = false;
}

void _leafBuilder::savePath()
{
    std::ofstream os(terrafectorEditorMaterial::rootFolder + path);
    cereal::JSONOutputArchive archive(os);
    archive(*this);
    changed = false;
}

void _leafBuilder::load()
{
    if (ImGui::Button("Load##_leafBuilder", ImVec2(60, 0)))
    {
        std::filesystem::path filepath;
        if (openFileDialog(filters, filepath))
        {
            path = materialCache::getRelative(filepath.string());
            name = filepath.filename().string();
            loadPath();
        }
    }
}


void _leafBuilder::save()
{
    if (changedForSave) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.0f, 0.8f));
        if (ImGui::Button("Save##_leafBuilder", ImVec2(60, 0)))
        {
            savePath();
        }
        ImGui::PopStyleColor();
    }
}


void _leafBuilder::saveas()
{
    if (ImGui::Button("Save as##_leafBuilder", ImVec2(60, 0)))
    {
        std::filesystem::path filepath;
        if (saveFileDialog(filters, filepath))
        {
            path = materialCache::getRelative(filepath.string());
            name = filepath.filename().string();
            savePath();
        }
    }
}


void _leafBuilder::renderGui()
{
    ImGui::Text(name.c_str());  // move to root lcass
    TOOLTIP(path.c_str());

    ImGui::PushFont(_gui->getFont("roboto_20"));
    {
        load();
        ImGui::SameLine(70, 0);
        save();
        ImGui::SameLine(140, 0);
        saveas();
    }
    ImGui::PopFont();



    ImGui::NewLine();
    ImGui::Text("stem");
    unsigned int gui_id = 1001;
    R_LENGTH("length", stem_length, "stem length in mm", "random");
    R_LENGTH("width", stem_width, "stem width in mm", "random");
    if (stem_length.x > 0 && stem_width.x > 0)
    {
        R_CURVE("curve", stem_curve, "stem curvature", "random");
        R_CURVE("angle", stem_to_leaf, "stem to leaf angle in radians", "random");
        R_VERTS("num verts", stemVerts);
        stem_Material.renderGui(gui_id);
    }

    ImGui::NewLine();
    ImGui::Text("leaf");
    if (ImGui::Checkbox("face camera", &cameraFacing)) changed = true;
    R_LENGTH("length", leaf_length, "mm", "random");
    R_LENGTH("width", leaf_width, "mm", "random");
    R_LENGTH("offset", width_offset, "", "random");
    R_CURVE("curve", leaf_curve, "radians", "random");
    R_CURVE("perlin C", perlinCurve, "amount", "repeats");
    R_CURVE("twist", leaf_twist, "radians", "random");
    R_CURVE("perlin T", perlinTwist, "amount", "repeats");
    R_CURVE("gavity", gravitrophy, "rotate towards the ground or sky", "random");
    R_VERTS("num verts", numVerts);
    ImGui::NewLine();
    ImGui::GetStyle().Colors[ImGuiCol_Header] = mat_color;
    materials.renderGui("materials", gui_id);

    changedForSave |= changed;
    anyChange |= changed;
    changed = false;
}


void _leafBuilder::treeView()
{
    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Header] = leaf_color;
    if (_rootPlant::selectedPart == this)   style.FrameBorderSize = 2;

    TREE_LINE(name.c_str(), path.c_str())
    {
        CLICK_PART;
        ImGui::TreePop();
    }
}

#define RND_ALBEDO(data) (data * (1.f + 0.3f * (float)dist(_rootPlant::generator)))
#define RND_B(data) (data.x * (1.f + data.y * (float)dist(_rootPlant::generator)))
#define RND_CRV(data) (data.x + (data.y * (float)dist(_rootPlant::generator)))

//#define CURVE(_mat,_ang)  glm::rotate(glm::mat4(1.0), _ang, (glm::vec3)_mat[0])     // pitch
//#define TWIST(_mat,_ang)  glm::rotate(glm::mat4(1.0), _ang, (glm::vec3)_mat[1])     // roll
//#define CURVE_Z(_mat,_ang)  glm::rotate(glm::mat4(1.0), _ang, (glm::vec3)_mat[2])   // yaw

#define GROW(_mat,_length)   _mat[3] += _mat[1] * _length

#define ROLL(_mat,_ang)  _mat = glm::rotate(_mat, _ang, glm::vec3(0, 1, 0))
#define PITCH(_mat,_ang)  _mat = glm::rotate(_mat, _ang, glm::vec3(1, 0, 0))
#define YAW(_mat,_ang)  _mat = glm::rotate(_mat, _ang, glm::vec3(0, 0, 1))
//#define RPY(_mat,_r,_p,_y)  TWIST(_mat,_r) * CURVE(_mat,_p) * CURVE_Z(_mat,_y)



glm::mat4 _leafBuilder::build(buildSetting& _settings)
{
    std::uniform_real_distribution<> dist(-1.f, 1.f);
    std::uniform_int_distribution<> distAlbedo(-50, 50);
    std::uniform_int_distribution<> distPerlin(1, 50000);

    const siv::PerlinNoise::seed_type seed = distPerlin(_rootPlant::generator);
    const siv::PerlinNoise perlin{ seed };
    const siv::PerlinNoise::seed_type seedT = distPerlin(_rootPlant::generator);
    const siv::PerlinNoise perlinTWST{ seedT };

    glm::mat4 node = _settings.root;
    ribbonVertex R_verts;
    float age = pow(_settings.age, 1.f);
    //age = saturate(_settings.age);

    // stem
    if (stem_length.x > 0 && stem_width.x > 0)
    {
        float length = RND_B(stem_length) / 100.f * 0.001f * age;   // to meters and numSegments
        float width = stem_width.x * 0.001f * age;
        float curve = RND_CRV(stem_curve) / 100.f * age;

        // Lodding stem, but use length instead............................................................
        bool showStem = width > _settings.pixelSize;
        int numStem = glm::clamp((int)((length / _settings.pixelSize) / 8.f * 100.f), 1, stemVerts.y);     // 1 for every 8 pixels, clampped
        float step = 99.f / (numStem);
        float cnt = 0.f;

        if (showStem)
        {
            R_verts.startRibbon(true);
            R_verts.set(node, width * 0.5f, stem_Material.index, float2(1.f, 0.f), 1.f, 1.f);
        }

        for (int i = 0; i < 100; i++)
        {
            PITCH(node, curve);
            GROW(node, length);

            cnt++;
            if (showStem && cnt >= step)
            {
                R_verts.set(node, width * 0.5f, stem_Material.index, float2(1.f, (float)i / 99.f), 1.f, 1.f);
                cnt -= step;
            }
        }

        GROW(node, -width * 0.5f);  // Now move ever so slghtly backwards for better penetration of stem to leaf
    }



    // rotation from stem to leaf
    PITCH(node, RND_CRV(stem_to_leaf));


    // build the leaf
    // WILL BE significantly better if we calc start and end positions
    {
        _vegMaterial mat = materials.get();
        float albedoScale = RND_ALBEDO(glm::lerp(mat.albedoScale.y, mat.albedoScale.x, age));
        float translucentScale = glm::lerp(mat.translucencyScale.y, mat.translucencyScale.x, age);

        float length = RND_B(leaf_length) / 100.f * 0.001f * age;   // to meters and numSegments  // FIXME scale to neter built into macro, rename macro for distamce only
        float width = RND_B(leaf_width) * 0.001f * age;
        float gravi = RND_CRV(gravitrophy) / 100.f * age;
        float curve = RND_CRV(leaf_curve) / 100.f * age;
        float twist = RND_CRV(leaf_twist) / 100.f * age;

        // Lodding leaf............................................................
        bool showLeaf = width > _settings.pixelSize;
        int numLeaf = glm::clamp((int)((length / _settings.pixelSize) / 8.f * 100.f), 1, numVerts.y);     // 1 for every 8 pixels, clampped
        bool forceFacing = false;// numLeaf == 1;
        uint firstVis = 0;
        uint lastVis = 99;
        bool first = true;
        
        for (int i = 0; i < 100; i++)
        {
            float t = (float)i / 100.f;
            float du = __min(1.f, sin(pow(t, width_offset.x) * 3.1415f) + width_offset.y);
            if (first)
            {
                if (width * du <= _settings.pixelSize)
                {
                    firstVis = i;
                    lastVis = i;
                }
                else first = false;
            }
            else
            {
                if (width * du > _settings.pixelSize)
                {
                    lastVis = i;
                }
            }
            
        } 
        float step = (float)(lastVis - firstVis) / numLeaf;
        float cnt = 0;

        // Fixme search for first and last vertex on size
        showLeaf = lastVis > firstVis;
        if (showLeaf)
        {
            R_verts.startRibbon(cameraFacing || forceFacing);
            float du = width_offset.y;
            if (numLeaf == 1) du = 1.f;
            R_verts.set(node, width * 0.5f * du, mat.index, float2(du, 0.f), albedoScale, translucentScale);
        }
        else
        {
            bool bCM = true;
        }

        for (int i = 0; i < 100; i++)
        {
            float t = (float)i / 100.f;
            float du = __min(1.f, sin(pow(t, width_offset.x) * 3.1415f) + width_offset.y);
            if (numLeaf == 1) du = 1.f;  //??? use 3 mqybe, 2 still curts dcorners

            float perlinScale = glm::smoothstep(0.f, 0.3f, t) * age;
            float noise = (float)perlin.normalizedOctave1D(perlinCurve.y * t, 4);
            PITCH(node, noise * perlinCurve.x * perlinScale);

            noise = (float)perlinTWST.normalizedOctave1D(perlinTwist.y * t, 4) * age;
            ROLL(node, noise * perlinTwist.x * perlinScale);

            ROLL(node, twist);
            PITCH(node, curve);
            GROW(node, length);


            if (i >= firstVis)
            {
                cnt++;
                if ((i <= lastVis) && showLeaf && cnt >= step)
                {
                    R_verts.set(node, width * 0.5f * du, mat.index, float2(du, t), albedoScale, translucentScale);
                    cnt -= step;
                }
            }
        }

    }

    changedForSave |= changed;
    return node;
}






// _twigBuilder
// -----------------------------------------------------------------------------------------------------------------------------------

void _twigBuilder::loadPath()
{
    std::ifstream is(terrafectorEditorMaterial::rootFolder + path);
    cereal::JSONInputArchive archive(is);
    archive(*this);
    changed = false;
}

void _twigBuilder::savePath()
{
    std::ofstream os(terrafectorEditorMaterial::rootFolder + path);
    cereal::JSONOutputArchive archive(os);
    archive(*this);
    changed = false;
}

void _twigBuilder::load()
{
    if (ImGui::Button("Load##_twigBuilder", ImVec2(60, 0)))
    {
        std::filesystem::path filepath;
        if (openFileDialog(filters, filepath))
        {
            path = materialCache::getRelative(filepath.string());
            name = filepath.filename().string();
            loadPath();
        }
    }
}


void _twigBuilder::save()
{
    if (changedForSave) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.0f, 0.8f));
        if (ImGui::Button("Save##_twigBuilder", ImVec2(60, 0)))
        {
            savePath();
        }
        ImGui::PopStyleColor();
    }
}


void _twigBuilder::saveas()
{
    if (ImGui::Button("Save as##_twigBuilder", ImVec2(60, 0)))
    {
        std::filesystem::path filepath;
        if (saveFileDialog(filters, filepath))
        {
            path = materialCache::getRelative(filepath.string());
            name = filepath.filename().string();
            savePath();
        }
    }
}

void _twigBuilder::renderGui()
{
    ImGui::Text(name.c_str());
    TOOLTIP(path.c_str());

    ImGui::PushFont(_gui->getFont("roboto_20"));
    {
        load();
        ImGui::SameLine(70, 0);
        save();
        ImGui::SameLine(140, 0);
        saveas();
    }
    ImGui::PopFont();


    ImGui::NewLine();
    ImGui::Text("stem");
    unsigned int gui_id = 1001;
    R_LENGTH("age", numSegments, "total number of nodes", "random is used for automatic variations");
    if (numSegments.x < 0.5f) numSegments.x = 0.5f;
    R_INT("start", startSegment, 0, 20, "first segment with leaves\nbelow this is bare stalk");
    R_LENGTH("length", stem_length, "stem length in mm", "random");
    R_LENGTH("width", stem_width, "stem width in mm", "random");
    R_CURVE("curve", stem_curve, "curve in a single segment", "random");
    R_CURVE("phototropism", stem_phototropism, "", "random");
    R_CURVE("node twist", node_rotation, "twist of the stalk", "random");
    R_CURVE("node bend", node_angle, "stem bend at a node", "random");
    ImGui::GetStyle().Colors[ImGuiCol_Header] = mat_color;
    stem_Material.renderGui(gui_id);

    // leaves
    ImGui::NewLine();
    ImGui::Text("stem");
    R_LENGTH("numLeaves", numLeaves, "stem length in mm", "random");
    R_CURVE("angle", leaf_angle, "angle of teh stalk t the ", "change of angle as it ages, usually drooping");
    R_CURVE("random", leaf_rnd, "randomness in the angles", "random");
    R_FLOAT("age_power", leaf_age_power, 1.f, 5.f, "");
    if (ImGui::Checkbox("twistAway", &twistAway)) changed = true; TOOLTIP("does trh stalk try t0 twis awat from a single leaf");
    ImGui::GetStyle().Colors[ImGuiCol_Header] = leaf_color;
    leaves.renderGui("leaves", gui_id);

    // tip
    ImGui::NewLine();
    ImGui::Text("stem");
    if (ImGui::Checkbox("unique_tip", &unique_tip)) changed = true;
    if (unique_tip)
    {
        ImGui::GetStyle().Colors[ImGuiCol_Header] = leaf_color;
        tip.renderGui("tip", gui_id);
    }

    ImGui::Text("numLeavesBuilt : %d", numLeavesBuilt);
}

void _twigBuilder::treeView()
{
    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Header] = twig_color;
    if (_rootPlant::selectedPart == this)   style.FrameBorderSize = 2;

    TREE_LINE(name.c_str(), path.c_str())
    {
        style.FrameBorderSize = 0;
        CLICK_PART;
        ImGui::Text("leaves");
        for (auto& L : leaves.data) { if (L.plantPtr) L.plantPtr->treeView(); }
        if (unique_tip)
        {
            ImGui::Text("tip");
            for (auto& L : tip.data) { if (L.plantPtr) L.plantPtr->treeView(); }
        }
        ImGui::TreePop();
    }

    changedForSave |= changed;
    anyChange |= changed;
}


lodBake* _twigBuilder::getBakeInfo(uint i)
{
    if (i < lod_bakeInfo.size()) return &lod_bakeInfo[i];

    return nullptr;
}

levelOfDetail* _twigBuilder::getLodInfo(uint i)
{
    if (i < lodInfo.size()) return &lodInfo[i];
    return nullptr;
}


void _twigBuilder::build_lod_0(buildSetting& _settings)
{
    ribbonVertex R_verts;
    float w = lod_bakeInfo[0].extents.x;
    uint mat = lod_bakeInfo[0].material.index;
    int sz = NODES_PREV.size();

    if (sz < 2) return;

    // Angle this towards the last STEM node, but with teh lengths of the total    
    
    glm::mat4 node = NODES_PREV[0];
    glm::mat4 last = NODES_PREV[sz - 2];
    last[1] = glm::normalize(last[3] - node[3]);     // point in general stem direction
    float tipLength = glm::dot(NODES_PREV[sz - 1][3] - NODES_PREV[sz - 2][3], last[1]);
    GROW(last, tipLength);


    // 2 verts quad version
    /*
    R_verts.startRibbon(true);
    R_verts.set(node, w, mat, float2(1, 0.f), 1.f, 1.f);
    node[3] = NODES_PREV.back()[3];
    R_verts.set(node, w, mat, float2(1, 1.f), 1.f, 1.f);
    */

    // 4 Verts fitted version
    R_verts.startRibbon(true);
    glm::vec4 step = (last[3] - node[3]) / 3.f;
    node[1] = glm::normalize(step); // point this towards the end
    for (int i = 0; i < 4; i++)
    {
        R_verts.set(node, w * lod_bakeInfo[0].dU[i] * 0.6f, mat, float2(lod_bakeInfo[0].dU[i] * 0.6f, 0.f + (0.3333333f * i)), 1.f, 1.f);
        node[3] += step;
    }
    
}


void _twigBuilder::build_lod_1(buildSetting& _settings)
{
    ribbonVertex R_verts;
    float w = lod_bakeInfo[1].extents.x * lod_bakeInfo[1].bakeWidth;
    uint mat = lod_bakeInfo[1].material.index;
    int sz = NODES_PREV.size();

    if (sz < 2) return;

    // Angle this towards the last STEM node, but with teh lengths of the total    
    //float tipLength = glm::length(NODES_PREV[sz - 1][3] - NODES_PREV[sz - 2][3]);
    glm::mat4 node = NODES_PREV[0];
    glm::mat4 last = NODES_PREV[sz - 2];
    last[1] = glm::normalize(last[3] - node[3]);     // point in general stem direction
    float tipLength = glm::dot(NODES_PREV[sz - 1][3] - NODES_PREV[sz - 2][3], last[1]);
    GROW(last, tipLength);

    
    R_verts.startRibbon(true);
    glm::vec4 step = (last[3] - node[3]) / 3.f;
    for (int i = 0; i < 4; i++)
    {
        R_verts.set(node, w, mat, float2(1.f, 0.f + (0.3333333f * i)), 1.f, 1.f);
        node[3] += step;
    }

    uint numSides = (32 - 4) / 2;
    // tip is in teh core
    //_settings.pixelSize = 0.1f;
    build_leaves(_settings, numSides);
}


void _twigBuilder::build_lod_2(buildSetting& _settings)
{
    if (NODES_PREV.size() < 2) return;

    ribbonVertex R_verts;
    float w = lod_bakeInfo[2].extents.x * lod_bakeInfo[2].bakeWidth;
    uint mat = lod_bakeInfo[2].material.index;

    
    // THIS version follwos every node, but really nto that good
    R_verts.startRibbon(true);
    float vScale = 1.f / (NODES_PREV.size() - 2);
    int step = 4;   // FIXME - need to be width over heigth ratio
    for (int i = 0; i < NODES_PREV.size() - 2; i+= step)
    {
        R_verts.set(NODES_PREV[i], w, mat, float2(1.f, i * vScale), 1.f, 1.f);
    }
    R_verts.set(NODES_PREV[NODES_PREV.size() - 2], w, mat, float2(1.f, 1.f), 1.f, 1.f);
    


    build_leaves(_settings, 100000);

    NODES_PREV.pop_back();

    build_tip(_settings);

}


void _twigBuilder::build_tip(buildSetting& _settings)
{
    // reset teh seed so all lods build teh same here
    _rootPlant::generator.seed(_settings.seed + 1999);
    if (unique_tip)
    {
        // walk the tip back ever so slightly
        glm::mat4 node = NODES_PREV.back();
        GROW(node, -tipWidth * 0.5f);
        _settings.root = node;
        _settings.age = 1.f;
        glm::mat4 lastNode = tip.get().plantPtr->build(_settings);
        NODES_PREV.push_back(lastNode);
    }
}


void _twigBuilder::build_leaves(buildSetting& _settings, uint _max)
{
    numLeavesBuilt = 0;
    std::uniform_real_distribution<> dist(-1.f, 1.f);
    // reset teh seed so all lods build teh same here
    _rootPlant::generator.seed(_settings.seed + 999);

    // side nodes
    uint end = NODES_PREV.size();
    if (unique_tip) end--;
    for (int i = 0; i < end; i++)
    {
        if (i >= startSegment)
        {
            float leafAge = 1.f - pow(i / age, leaf_age_power);
            int numL = (int)RND_B(numLeaves);


            for (int j = 0; j < numL; j++)
            {
                numLeavesBuilt++;
                glm::mat4 node = NODES_PREV[i];
                float A = leaf_angle.x + leaf_angle.y * leafAge + dist(_rootPlant::generator) * 0.3f;
                float nodeTwist = 6.283185307f / (float)numL * (float)j;

                ROLL(node, nodeTwist);
                PITCH(node, -A);
                _settings.root = node;
                _settings.age = leafAge;
                _plantRND LEAF = leaves.get();
                if (LEAF.plantPtr) LEAF.plantPtr->build(_settings);
            }
        }
    }
}


glm::mat4 _twigBuilder::build(buildSetting& _settings)
{
    //std::mt19937 gen(_settings.seed);
    std::uniform_real_distribution<> dist(-1.f, 1.f);
    std::uniform_int_distribution<> distAlbedo(-50, 50);
    glm::mat4 node = _settings.root;
    ribbonVertex R_verts;

    glm::mat4 lastNode;
    NODES.clear();
    NODES.push_back(node);

    if (_settings.pixelSize == lodInfo[0].pixelSize) { build_lod_0(_settings); return NODES_PREV.back(); }
    if (_settings.pixelSize == lodInfo[1].pixelSize) { build_lod_1(_settings); return NODES_PREV.back(); }
    if (_settings.pixelSize == lodInfo[2].pixelSize) { build_lod_2(_settings); return NODES_PREV.back(); }



    // stem
    // build the whole stem as one, writing to NODES
    float nodePixels = (stem_length.x * 0.001f) / _settings.pixelSize;
    int nodeNumSegments = __max(1, (int)pow(nodePixels / 4.f, 0.5f));
    float segStep = 99.f / (float)nodeNumSegments;


    R_verts.startRibbon(true);
    age = RND_B(numSegments);
    int iAge = __max(1, (int)age);
    for (int i = 0; i < iAge; i++)
    {
        float leafAge = 1.f - pow((float)i / age, leaf_age_power);
        float L = RND_B(stem_length) * 0.001f / 100.f;
        float W = RND_B(stem_width) * 0.001f * pow(leafAge, 0.4f);
        tipWidth = W;
        float C = RND_CRV(stem_curve) / 100.f;
        float P = RND_CRV(stem_phototropism) / 100.f;

        bool visible = W > _settings.pixelSize;
        if (visible && i == 0) {
            R_verts.set(node, W * 0.5f, stem_Material.index, float2(1.f, 0.f), 1.f, 1.f);   // set very first one
        }
        float cnt = 0;
        for (int j = 0; j < 100; j++)
        {
            PITCH(node, C);
            // Phototropy
            float cos_dX = glm::dot((glm::vec3)node[1], glm::vec3(1, 0, 0));
            float cos_dZ = glm::dot((glm::vec3)node[1], glm::vec3(0, 0, 1));
            PITCH(node, -P * asin(cos_dZ));
            YAW(node, P * asin(cos_dX));

            GROW(node, L);
            cnt++;
            if (visible && cnt >= segStep)
            {
                R_verts.set(node, W * 0.5f, stem_Material.index, float2(1.f, i + (float)j / 99.f), 1.f, 1.f);
                cnt -= segStep;
            }
        }
        NODES.push_back(node);

        // now rotate for teh next segment
        ROLL(node, RND_CRV(node_rotation));
        PITCH(node, RND_CRV(node_angle));
        YAW(node, RND_CRV(node_angle));
    }






    // save NODES for loddign later
    NODES_PREV.resize(NODES.size());
    NODES_PREV = NODES;
    lastNode = NODES.back();

    build_leaves(_settings, 100000);
    build_tip(_settings);


    changedForSave |= changed;
    changed = false;




    // Now set all the lodding info
    float2 extents = float2(0, 0);
    for (auto& R : ribbonVertex::ribbons)
    {
        float2 XZ = R.position.xz;
        float r = glm::length(XZ);
        extents.x = __max(extents.x, r + R.radius);
        extents.y = __max(extents.y, abs(R.position.y) + R.radius);
    }

    // du
    float du6[6] = { 0, 0, 0, 0, 0, 0 };
    float step = extents.y / 6.f;
    for (auto& R : ribbonVertex::ribbons)
    {
        float2 XZ = R.position.xz;
        float r = glm::length(XZ);
        float y = __max(0, R.position.y);   // only positive
        uint bucket = (int)(y / step);
        du6[bucket] = __max(r, du6[bucket]);
    }
    std::array<float, 4> du;
    du[0] = du6[0];
    du[1] = __max(du6[1], du6[2]);
    du[2] = __max(du6[3], du6[4]);
    du[3] = du6[5];

    std::filesystem::path full_path = path;

    // lod 0
    lod_bakeInfo[0].extents = extents;
    lod_bakeInfo[0].dU[0] = du[0] / extents.x;
    lod_bakeInfo[0].dU[1] = du[1] / extents.x;
    lod_bakeInfo[0].dU[2] = du[2] / extents.x;
    lod_bakeInfo[0].dU[3] = du[3] / extents.x;
    //lod_bakeInfo[0].material.name = full_path.stem().string() + "_billboard_lod0";
    //lod_bakeInfo[0].material.path = full_path.parent_path().string() + "\\" + lod_bakeInfo[0].material.name + ".vegMaterial";

    lod_bakeInfo[0].material.name = std::to_string(_settings.seed) + "_lod0";
    lod_bakeInfo[0].material.path = full_path.parent_path().string() + "\\bake_" + full_path.stem().string() + "\\" + lod_bakeInfo[0].material.name + ".vegetationMaterial";;

    // lod 1
    lod_bakeInfo[1].extents = extents;
    float dw = extents.x * lod_bakeInfo[1].bakeWidth;
    lod_bakeInfo[1].dU[0] = __min(1.f, du[0] / dw);
    lod_bakeInfo[1].dU[1] = __min(1.f, du[1] / dw);
    lod_bakeInfo[1].dU[2] = __min(1.f, du[2] / dw);
    lod_bakeInfo[1].dU[3] = __min(1.f, du[3] / dw);
    //lod_bakeInfo[1].material.name = full_path.stem().string() + "_billboard_lod1";
    //lod_bakeInfo[1].material.path = full_path.parent_path().string() + "\\" + lod_bakeInfo[1].material.name + ".vegMaterial";
    lod_bakeInfo[1].material.name = std::to_string(_settings.seed) + "_lod1";
    lod_bakeInfo[1].material.path = full_path.parent_path().string() + "\\bake_" + full_path.stem().string() + "\\" + lod_bakeInfo[1].material.name + ".vegetationMaterial";;

    // lod 2
    extents.y = NODES.back()[3].y;   // last lod use NDOE end
    lod_bakeInfo[2].extents = extents;
    float dw2 = extents.x * lod_bakeInfo[2].bakeWidth;
    lod_bakeInfo[2].dU[0] = __min(1.f, du[0] / dw2);
    lod_bakeInfo[2].dU[1] = __min(1.f, du[1] / dw2);
    lod_bakeInfo[2].dU[2] = __min(1.f, du[2] / dw2);
    lod_bakeInfo[2].dU[3] = __min(1.f, du[3] / dw2);
    //lod_bakeInfo[2].material.name = full_path.stem().string() + "_billboard_lod2";
    //lod_bakeInfo[2].material.path = full_path.parent_path().string() + "\\" + lod_bakeInfo[2].material.name + ".vegMaterial";
    lod_bakeInfo[2].material.name = std::to_string(_settings.seed) + "_lod2";
    lod_bakeInfo[2].material.path = full_path.parent_path().string() + "\\bake_" + full_path.stem().string() + "\\" + lod_bakeInfo[2].material.name + ".vegetationMaterial";;


    return lastNode;
}







void _rootPlant::onLoad()
{
    plantData = Buffer::createStructured(sizeof(plant), 256);
    instanceData = Buffer::createStructured(sizeof(plant_instance), 16384);
    instanceData_Billboards = Buffer::createStructured(sizeof(plant_instance), 16384, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
    blockData = Buffer::createStructured(sizeof(block_data), 16384 * 32, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);        // big enough to house inatnces * blocks per instance   8 Mb for now
    vertexData = Buffer::createStructured(sizeof(ribbonVertex8), 256 * 128);
    drawArgs_vegetation = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
    drawArgs_billboards = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);

    // compute
    //split.compute_tileClear.Vars()->setBuffer("DrawArgs_ClippedLoddedPlants", split.drawArgs_clippedloddedplants);


    compute_clearBuffers.load("Samples/Earthworks_4/hlsl/terrain/compute_vegetation_clear.hlsl");
    compute_clearBuffers.Vars()->setBuffer("DrawArgs_Quads", drawArgs_billboards);
    compute_clearBuffers.Vars()->setBuffer("DrawArgs_Plants", drawArgs_vegetation);

    compute_calulate_lod.load("Samples/Earthworks_4/hlsl/terrain/compute_vegetation_lod.hlsl");
    compute_calulate_lod.Vars()->setBuffer("DrawArgs_Quads", drawArgs_billboards);
    compute_calulate_lod.Vars()->setBuffer("DrawArgs_Plants", drawArgs_vegetation);
    compute_calulate_lod.Vars()->setBuffer("plant_buffer", plantData);
    compute_calulate_lod.Vars()->setBuffer("instance_buffer", instanceData);
    compute_calulate_lod.Vars()->setBuffer("instance_buffer_billboard", instanceData_Billboards);    
    compute_calulate_lod.Vars()->setBuffer("block_buffer", blockData);

    std::uniform_real_distribution<> RND(-1.f, 1.f);

    for (int i = 0; i < 16384; i++)
    {
        instanceBuf[i].plant_idx = 0;
        instanceBuf[i].position = { RND(generator) * 55, 0, RND(generator) * 55 };
        instanceBuf[i].scale = 1.f + RND(generator) * 0.5f;
        instanceBuf[i].rotation = RND(generator) * 3.14f;
        instanceBuf[i].time_offset = RND(generator) * 100;
    }
    instanceBuf[0].position = { 0, 0, 0 };
    instanceBuf[0].scale = 1.f;
    instanceBuf[0].rotation = 0;
    instanceData->setBlob(instanceBuf.data(), 0, 16384 * sizeof(plant_instance));

    for (int i = 0; i < 16384; i++)
    {
        blockBuf[i].vertex_offset = 0;
        blockBuf[i].instance_idx = i;
        blockBuf[i].section_idx = 0;
        blockBuf[i].plant_idx = 0;
    }

    Sampler::Desc samplerDesc;
    samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp).setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setMaxAnisotropy(4);
    sampler_ClampAnisotropic = Sampler::create(samplerDesc);

    samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap).setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setMaxAnisotropy(1);
    sampler_Ribbons = Sampler::create(samplerDesc);


    vegetationShader.load("Samples/Earthworks_4/hlsl/terrain/render_vegetation_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
    vegetationShader.Vars()->setBuffer("plant_buffer", plantData);
    vegetationShader.Vars()->setBuffer("instance_buffer", instanceData);
    vegetationShader.Vars()->setBuffer("block_buffer", blockData);
    vegetationShader.Vars()->setBuffer("vertex_buffer", vertexData);
    vegetationShader.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
    vegetationShader.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX
    vegetationShader.Vars()->setSampler("gSamplerClamp", sampler_ClampAnisotropic);              // fixme only cvlamlX
    auto& block = vegetationShader.Vars()->getParameterBlock("textures");
    varVegTextures = block->findMember("T");

    billboardShader.add("_BILLBOARD", "");
    billboardShader.load("Samples/Earthworks_4/hlsl/terrain/render_vegetation_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::PointList, "gsMain");
    billboardShader.Vars()->setBuffer("plant_buffer", plantData);
    billboardShader.Vars()->setBuffer("instance_buffer", instanceData_Billboards);
    billboardShader.Vars()->setBuffer("block_buffer", blockData);
    billboardShader.Vars()->setBuffer("vertex_buffer", vertexData);
    billboardShader.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
    billboardShader.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX
    billboardShader.Vars()->setSampler("gSamplerClamp", sampler_ClampAnisotropic);              // fixme only cvlamlX
    auto& blockBB = billboardShader.Vars()->getParameterBlock("textures");
    varBBTextures = blockBB->findMember("T");

    bakeShader.add("_BAKE", "");
    bakeShader.load("Samples/Earthworks_4/hlsl/terrain/render_vegetation_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
    bakeShader.Vars()->setBuffer("plant_buffer", plantData);
    bakeShader.Vars()->setBuffer("instance_buffer", instanceData);
    bakeShader.Vars()->setBuffer("block_buffer", blockData);
    bakeShader.Vars()->setBuffer("vertex_buffer", vertexData);
    bakeShader.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
    bakeShader.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX
    auto& blockB = bakeShader.Vars()->getParameterBlock("textures");
    varBakeTextures = blockB->findMember("T");


    RasterizerState::Desc rsDesc;
    rsDesc.setFillMode(RasterizerState::FillMode::Solid).setCullMode(RasterizerState::CullMode::None);
    rasterstate = RasterizerState::create(rsDesc);

    BlendState::Desc blendDesc;
    blendDesc.setRtBlend(0, true);
    blendDesc.setRtParams(0, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::Zero, BlendState::BlendFunc::Zero);
    blendstate = BlendState::create(blendDesc);

    blendDesc.setIndependentBlend(true);
    for (int i = 0; i < 8; i++)
    {
        // clear all
        blendDesc.setRenderTargetWriteMask(i, true, true, true, true);
        blendDesc.setRtBlend(i, true);
        blendDesc.setRtParams(i, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::SrcAlphaSaturate, BlendState::BlendFunc::One);
    }
    blendDesc.setRtParams(3, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::SrcAlphaSaturate, BlendState::BlendFunc::One);
    blendstateBake = BlendState::create(blendDesc);

    compute_bakeFloodfill.load("Samples/Earthworks_4/hlsl/terrain/compute_bakeFloodfill.hlsl");
}


void _rootPlant::renderGui(Gui* _gui)
{
    Gui::Window builderPanel(_gui, "vegetation builder", { 900, 900 }, { 100, 100 });
    {
        ImGui::PushFont(_gui->getFont("roboto_32"));
        {
            static float timeAvs = 0;
            timeAvs *= 0.99f;
            timeAvs += 0.01f * gpFramework->getFrameRate().getAverageFrameTime();
            //ImGui::SetCursorPos(ImVec2(10, 10));
            ImGui::Text("%3.1f fps", 1000.0 / timeAvs);
        }

        ImGui::PushFont(_gui->getFont("roboto_18"));
        _plantBuilder::_gui = _gui;

        ImGui::Columns(2);
        if (ImGui::Button("new Leaf")) { if (root) delete root; root = new _leafBuilder;  _rootPlant::selectedPart = root; _rootPlant::selectedMaterial = nullptr; }
        ImGui::SameLine();
        if (ImGui::Button("new Twig")) { if (root) delete root; root = new _twigBuilder;  _rootPlant::selectedPart = root; _rootPlant::selectedMaterial = nullptr; }

        if (ImGui::Button("Load")) {
            std::filesystem::path filepath;
            FileDialogFilterVec filters = { {"leaf"}, {"twig"} };
            if (openFileDialog(filters, filepath))
            {
                _rootPlant::selectedMaterial = nullptr;
                if (root) delete root;
                if (filepath.string().find(".leaf") != std::string::npos) { root = new _leafBuilder;  _rootPlant::selectedPart = root; }
                if (filepath.string().find(".twig") != std::string::npos) { root = new _twigBuilder;  _rootPlant::selectedPart = root; }

                if (root)
                {
                    root->path = materialCache::getRelative(filepath.string());
                    root->name = filepath.filename().string();
                    root->loadPath();
                }
            }
        }

        ImGui::NewLine();
        static float numPix = 100;
        numPix = bakeSettings.objectSize / settings.pixelSize;
        if (ImGui::DragFloat("size", &bakeSettings.objectSize, 0.01f, 1.f, 64.f, "%3.2fm")) build();
        if (ImGui::DragFloat("radius", &bakeSettings.radiusScale, 0.001f, 0.1f, 8.f, "%3.2fm")) build();
        if (ImGui::DragFloat("num pix", &numPix, 1.f, 1.f, 1000.f, "%3.4f")) { settings.pixelSize = bakeSettings.objectSize / numPix; build(); }
        ImGui::Text("pixel is %2.1f mm", settings.pixelSize * 1000.f);
        if (ImGui::DragFloat("pix SZ", &settings.pixelSize, 0.001f, 0.001f, 1.f, "%3.2fm")) { numPix = bakeSettings.objectSize / settings.pixelSize; build(); }

        if (ImGui::DragInt("seed", &settings.seed, 1, 0, 1000)) { build(); }
        ImGui::Text("%d verts", totalBlocksToRender * VEG_BLOCK_SIZE);
        ImGui::Text("%2.2f M tris", (float)totalBlocksToRender * VEG_BLOCK_SIZE * 2.f / 1000000.f);
        //if (ImGui::Button("BUILD")) build();

        ImGui::NewLine();


        if (root)
        {
            if (ImGui::Button("BuildAllLODS"))
            {
                tempUpdateRender = true;

                ribbonVertex::packed.clear();
                ribbonVertex::packed.reserve(4096); // just fairly large
                uint start = 0;
                settings.pixelSize = 0.001f;    // 1mm just tiny
                build();
                float Y = extents.y;

                ribbonVertex::packed.clear();

                uint startBlock[16][16];
                uint numBlocks[16][16];
                for (int pIndex = 0; pIndex < 4; pIndex++)
                {
                    plantBuf[pIndex].radiusScale = ribbonVertex::radiusScale;
                    plantBuf[pIndex].scale = ribbonVertex::objectScale;
                    plantBuf[pIndex].offset = ribbonVertex::objectOffset;
                    plantBuf[pIndex].Ao_depthScale = 0.3f;
                    plantBuf[pIndex].bDepth = 1;
                    plantBuf[pIndex].bScale = 1;
                    plantBuf[pIndex].sunTilt = -0.2f;
                    plantBuf[pIndex].size = extents;

                    lodBake* lodZero = root->getBakeInfo(0);
                    if (lodZero)
                    {
                        plantBuf[pIndex].billboardMaterialIndex = lodZero->material.index;
                    }

                    for (uint lod = 1; lod < 100; lod++)
                    {
                        levelOfDetail* lodInfo = root->getLodInfo(lod);
                        if (lodInfo)
                        {
                            lodInfo->pixelSize = Y / lodInfo->numPixels;
                            settings.pixelSize = lodInfo->pixelSize;
                            settings.seed = 1000 + pIndex;
                                build();
                            lodInfo->numVerts = (int)ribbonVertex::ribbons.size();
                            lodInfo->numBlocks = ribbonVertex::packed.size() / VEG_BLOCK_SIZE - start;
                            lodInfo->unused = ribbonVertex::packed.size() - ribbonVertex::ribbons.size();
                            lodInfo->startBlock = start;

                            startBlock[pIndex][lod] = start;
                            numBlocks[pIndex][lod] = lodInfo->numBlocks;

                            


                            plantBuf[pIndex].numLods = lod - 1;
                            plantBuf[pIndex].lods[lod - 1].pixSize = (float)lodInfo->numPixels;
                            plantBuf[pIndex].lods[lod - 1].numBlocks = lodInfo->numBlocks;
                            plantBuf[pIndex].lods[lod - 1].startVertex = start * VEG_BLOCK_SIZE;

                            

                            start += lodInfo->numBlocks;
                        }
                    }
                }


                levelOfDetail* display_lodInfo = root->getLodInfo(4);
                totalBlocksToRender = 0;

                for (int i = 0; i < 16384; i++)
                {
                    int plantIdx = 0;// i % 4;
                    uint RENDERME = 4;
                    uint numB = numBlocks[plantIdx][RENDERME];
                    totalBlocksToRender += numB;
                    for (int j = 0; j < numB; j++)
                    {
                        blockBuf[i * numB + j].vertex_offset = VEG_BLOCK_SIZE * (startBlock[plantIdx][RENDERME] + j);
                        blockBuf[i * numB + j].instance_idx = i;
                        blockBuf[i * numB + j].section_idx = 0;
                        blockBuf[i * numB + j].plant_idx = plantIdx;
                    }
                }
                
                blockData->setBlob(blockBuf.data(), 0, totalBlocksToRender * sizeof(block_data));

                


                plantData->setBlob(plantBuf.data(), 0, 4 * sizeof(plant));
                //instanceData->setBlob(instanceBuf.data(), 0, 16384 * sizeof(plant_instance));
                //blockData->setBlob(blockBuf.data(), 0, 16384 * sizeof(block_data));
                vertexData->setBlob(ribbonVertex::packed.data(), 0, ribbonVertex::packed.size() * sizeof(ribbonVertex8));                // FIXME uploads should be smaller

            }

            for (uint lod = 0; lod < 100; lod++)
            {
                levelOfDetail* lodInfo = root->getLodInfo(lod);
                if (lodInfo)
                {
                    ImGui::PushID(5678 + lod);
                    {
                        ImGui::Text("%d", lod);
                        ImGui::SameLine(0, 10);
                        ImGui::SetNextItemWidth(60);
                        if (ImGui::DragInt("##numPix", &(lodInfo->numPixels), 1, 8, 2000))
                        {
                            lodInfo->pixelSize = extents.y / lodInfo->numPixels;
                            settings.pixelSize = lodInfo->pixelSize;
                            build();
                            lodInfo->numVerts = (int)ribbonVertex::packed.size();
                            lodInfo->numBlocks = lodInfo->numVerts / VEG_BLOCK_SIZE + 1;
                            lodInfo->unused = lodInfo->numBlocks * VEG_BLOCK_SIZE - lodInfo->numVerts;
                        }
                        ImGui::SetTooltip("[v %d, b %d, u %d]", lodInfo->numVerts, lodInfo->numBlocks, lodInfo->unused);

                        ImGui::SameLine(0, 10);
                        ImGui::Text("px, %2.1f mm", lodInfo->pixelSize * 1000.f);
                        //ImGui::SameLine(0, 10);
                        ImGui::Text("[v %d, b %d, u %d, startB %d]", lodInfo->numVerts, lodInfo->numBlocks, lodInfo->unused, lodInfo->startBlock);
                    }
                    ImGui::PopID();
                }
            }
        }

        ImGui::NewLine();
        ImGui::Text("render information");
        ImGui::Text("blocks %d   {%d}", totalBlocksToRender, totalBlocksToRender / 16384);

        if (root && anyChange)
        {
            //ribbonVertex::packed.clear();
            //ribbonVertex::packed.reserve(4096); // just fairly large
            //build();
        }


        ImGui::NewLine();
        if (ImGui::Button("BUILD lod-0"))
        {
            build();
            float Y = extents.y;

            levelOfDetail* lodInfo = root->getLodInfo(0);
            if (lodInfo)
            {
                lodInfo->pixelSize = Y / lodInfo->numPixels;
                settings.pixelSize = lodInfo->pixelSize;
                ribbonVertex::packed.clear();
                build();


                vertexData->setBlob(ribbonVertex::packed.data(), 0, ribbonVertex::packed.size() * sizeof(ribbonVertex8));
                uint numB = ribbonVertex::packed.size() / VEG_BLOCK_SIZE;
                for (int j = 0; j < numB; j++)
                {
                    blockBuf[j].vertex_offset = VEG_BLOCK_SIZE * j;
                    blockBuf[j].instance_idx = 0;
                    blockBuf[j].section_idx = 0;
                    blockBuf[j].plant_idx = 0;
                }
                totalBlocksToRender = numB;
                blockData->setBlob(blockBuf.data(), 0, totalBlocksToRender * sizeof(block_data));

                plantBuf[0].radiusScale = ribbonVertex::radiusScale;
                plantBuf[0].scale = ribbonVertex::objectScale;
                plantBuf[0].offset = ribbonVertex::objectOffset;
                plantBuf[0].Ao_depthScale = 0.3f;
                plantBuf[0].bDepth = 1;
                plantBuf[0].bScale = 1;
                plantBuf[0].sunTilt = -0.2f;
                plantData->setBlob(plantBuf.data(), 0, 1 * sizeof(plant));

                tempUpdateRender = true;
            }
        }
        if (ImGui::Button("BUILD lod-1"))
        {
            build();
            float Y = extents.y;

            levelOfDetail* lodInfo = root->getLodInfo(1);
            if (lodInfo)
            {
                lodInfo->pixelSize = Y / lodInfo->numPixels;
                settings.pixelSize = lodInfo->pixelSize;
                ribbonVertex::packed.clear();
                build();


                vertexData->setBlob(ribbonVertex::packed.data(), 0, ribbonVertex::packed.size() * sizeof(ribbonVertex8));
                uint numB = ribbonVertex::packed.size() / VEG_BLOCK_SIZE;
                for (int j = 0; j < numB; j++)
                {
                    blockBuf[j].vertex_offset = VEG_BLOCK_SIZE * j;
                    blockBuf[j].instance_idx = 0;
                    blockBuf[j].section_idx = 0;
                    blockBuf[j].plant_idx = 0;
                }
                totalBlocksToRender = numB;
                blockData->setBlob(blockBuf.data(), 0, totalBlocksToRender * sizeof(block_data));

                plantBuf[0].radiusScale = ribbonVertex::radiusScale;
                plantBuf[0].scale = ribbonVertex::objectScale;
                plantBuf[0].offset = ribbonVertex::objectOffset;
                plantBuf[0].Ao_depthScale = 0.3f;
                plantBuf[0].bDepth = 1;
                plantBuf[0].bScale = 1;
                plantBuf[0].sunTilt = -0.2f;
                plantData->setBlob(plantBuf.data(), 0, 1 * sizeof(plant));

                tempUpdateRender = true;
            }
        }
        if (ImGui::Button("BUILD lod-2"))
        {
            build();
            float Y = extents.y;

            levelOfDetail* lodInfo = root->getLodInfo(2);
            if (lodInfo)
            {
                lodInfo->pixelSize = Y / lodInfo->numPixels;
                settings.pixelSize = lodInfo->pixelSize;
                ribbonVertex::packed.clear();
                build();


                vertexData->setBlob(ribbonVertex::packed.data(), 0, ribbonVertex::packed.size() * sizeof(ribbonVertex8));
                uint numB = ribbonVertex::packed.size() / VEG_BLOCK_SIZE;
                for (int j = 0; j < numB; j++)
                {
                    blockBuf[j].vertex_offset = VEG_BLOCK_SIZE * j;
                    blockBuf[j].instance_idx = 0;
                    blockBuf[j].section_idx = 0;
                    blockBuf[j].plant_idx = 0;
                }
                totalBlocksToRender = numB;
                blockData->setBlob(blockBuf.data(), 0, totalBlocksToRender * sizeof(block_data));

                plantBuf[0].radiusScale = ribbonVertex::radiusScale;
                plantBuf[0].scale = ribbonVertex::objectScale;
                plantBuf[0].offset = ribbonVertex::objectOffset;
                plantBuf[0].Ao_depthScale = 0.3f;
                plantBuf[0].bDepth = 1;
                plantBuf[0].bScale = 1;
                plantBuf[0].sunTilt = -0.2f;
                plantData->setBlob(plantBuf.data(), 0, 1 * sizeof(plant));

                tempUpdateRender = true;
            }
        }
        if (ImGui::Button("BUILD"))
        {
            settings.pixelSize = 0.001f;
            ribbonVertex::packed.clear();
            build();
            vertexData->setBlob(ribbonVertex::packed.data(), 0, ribbonVertex::packed.size() * sizeof(ribbonVertex8));
            uint numB = ribbonVertex::packed.size() / VEG_BLOCK_SIZE;
            for (int j = 0; j < numB; j++)
            {
                blockBuf[j].vertex_offset = VEG_BLOCK_SIZE * j;
                blockBuf[j].instance_idx = 0;
                blockBuf[j].section_idx = 0;
                blockBuf[j].plant_idx = 0;
            }
            totalBlocksToRender = numB;
            blockData->setBlob(blockBuf.data(), 0, totalBlocksToRender * sizeof(block_data));

            plantBuf[0].radiusScale = ribbonVertex::radiusScale;
            plantBuf[0].scale = ribbonVertex::objectScale;
            plantBuf[0].offset = ribbonVertex::objectOffset;
            plantBuf[0].Ao_depthScale = 0.3f;
            plantBuf[0].bDepth = 1;
            plantBuf[0].bScale = 1;
            plantBuf[0].sunTilt = -0.2f;
            plantData->setBlob(plantBuf.data(), 0, 1 * sizeof(plant));

            tempUpdateRender = true;
        }

        ImGui::NewLine();
        if (ImGui::Button("BAKE 0, 1, 2"))
        {
            if (root)
            {


                for (int seed = 1000; seed < 1001; seed++)
                {
                    // FIXME this has to chnage the settings
                    // straiten it
                    // and detail settings - quite high for bakogn at least 16x the highest ixel settibng - just high for now
                    // build must set all teh extents and du
                    settings.seed = seed;
                    settings.pixelSize = 0.001f;
                    ribbonVertex::packed.clear();
                    build();
                    vertexData->setBlob(ribbonVertex::packed.data(), 0, ribbonVertex::packed.size() * sizeof(ribbonVertex8));

                    uint numB = ribbonVertex::packed.size() / VEG_BLOCK_SIZE;
                    for (int j = 0; j < numB; j++)
                    {
                        blockBuf[j].vertex_offset = VEG_BLOCK_SIZE * j;
                        blockBuf[j].instance_idx = 0;
                        blockBuf[j].section_idx = 0;
                        blockBuf[j].plant_idx = 0;
                    }
                    totalBlocksToRender = numB;
                    blockData->setBlob(blockBuf.data(), 0, totalBlocksToRender * sizeof(block_data));

                    plantBuf[0].radiusScale = ribbonVertex::radiusScale;
                    plantBuf[0].scale = ribbonVertex::objectScale;
                    plantBuf[0].offset = ribbonVertex::objectOffset;
                    plantBuf[0].Ao_depthScale = 0.3f;
                    plantBuf[0].bDepth = 1;
                    plantBuf[0].bScale = 1;
                    plantBuf[0].sunTilt = -0.2f;
                    plantData->setBlob(plantBuf.data(), 0, 1 * sizeof(plant));

                    for (int i = 0; i < 10; i++)
                    {
                        if (root->getBakeInfo(i))
                        {
                            bake(root->path, std::to_string(settings.seed), root->getBakeInfo(i));
                            root->getBakeInfo(i)->material.reload();
                        }
                    }
                }
            }
        }

        ImGui::NewLine();
        if (root) root->treeView();

        ImGui::NextColumn();
        // now render the selected item.
        if (selectedPart)       selectedPart->renderGui();
        if (selectedMaterial)
        {
        }

        ImGui::PopFont();
    }
    builderPanel.release();
}



void _rootPlant::build()
{
    if (root)
    {
        ribbonVertex::objectScale = bakeSettings.getScale();
        ribbonVertex::radiusScale = bakeSettings.radiusScale;
        ribbonVertex::objectOffset = bakeSettings.getOffset();

        settings.parentStemDir = { 0, 1, 0 };
        settings.root = glm::mat4(1.0);

        ribbonVertex::ribbons.clear();
        ribbonVertex::ribbons.reserve(2000);

        generator.seed(settings.seed);
        root->build(settings);

        // Now light the plant

        // now pack it
        extents = float2(0, 0);
        for (auto& R : ribbonVertex::ribbons)
        {
            // can we do this earlier during set - to a global extents?
            float2 XZ = R.position.xz;
            float r = glm::length(XZ);
            extents.x = __max(extents.x, r + R.radius);
            extents.y = __max(extents.y, abs(R.position.y) + R.radius);
        }

        int verts = ribbonVertex::ribbons.size();
        int last = verts % VEG_BLOCK_SIZE;
        int unused = 0;
        if (last > 0) unused = VEG_BLOCK_SIZE - last;
        //ribbonVertex::packed.clear();
        //ribbonVertex::packed.reserve(verts + unused);
        for (auto& R : ribbonVertex::ribbons)
        {
            R.lightBasic(extents);
            ribbonVertex8 P = R.pack();
            ribbonVertex::packed.push_back(P);
        }

        // fill up the rest with th efirst vertex
        for (int i = 0; i < unused; i++)
        {
            ribbonVertex::packed.push_back(ribbonVertex::packed.front());
        }

        anyChange = false;
    }
}





void _rootPlant::reloadMaterials()
{
}


void _rootPlant::remapMaterials()
{
}



void _rootPlant::import()
{
}



void _rootPlant::eXport()
{
}



void _rootPlant::bake(std::string _path, std::string _seed, lodBake* _info)
{
    if (!root) return;

    std::filesystem::path PT = _path;
    //lod_bakeInfo[0].material.path = full_path.parent_path().string() + "\\" + full_path.stem().string() + "_bake\\" + lod_bakeInfo[0].material.name + ".vegMaterial";;


    std::string resource = terrafectorEditorMaterial::rootFolder;

    std::string newDir = "rmdir " + resource + PT.parent_path().string() + "\\bake_" + PT.stem().string();
    replaceAllVEG(newDir, "/", "\\");
    system(newDir.c_str());

    newDir = "mkdir " + resource + PT.parent_path().string() + "\\bake_" + PT.stem().string();
    replaceAllVEG(newDir, "/", "\\");
    system(newDir.c_str());

    std::string newRelative = PT.parent_path().string() + "\\bake_" + PT.stem().string() + "\\";
    newDir = resource + newRelative;


    int superSample = 8;
    float W = _info->extents.x * _info->bakeWidth;      // this is half width
    float H = _info->extents.y;
    int iW = 4 * (int)(_info->pixHeight * W * 2.f / H / 4) * superSample;
    int iH = _info->pixHeight * superSample;

    Fbo::Desc desc;
    desc.setDepthStencilTarget(ResourceFormat::D24UnormS8);			// keep for now, not sure why, but maybe usefult for cuts
    desc.setColorTarget(0u, ResourceFormat::RGBA8Unorm, true);		// albedo
    desc.setColorTarget(1u, ResourceFormat::RGBA16Float, true);	    // normal_16
    desc.setColorTarget(2u, ResourceFormat::RGBA8Unorm, true);		// normal_8
    desc.setColorTarget(3u, ResourceFormat::RGBA8Unorm, true);	    // pbr
    desc.setColorTarget(4u, ResourceFormat::RGBA8Unorm, true);	    // extra
    Fbo::SharedPtr fbo = Fbo::create2D(iW, iH, desc, 1, 4);
    const glm::vec4 clearColor(0.5, 0.5f, 1.0f, 0.0f);
    renderContext->clearFbo(fbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    renderContext->clearRtv(fbo->getRenderTargetView(0).get(), glm::vec4(0.0, 0.0f, 0.0f, 0.0f));
    renderContext->clearRtv(fbo->getRenderTargetView(1).get(), glm::vec4(0.5, 0.5f, 1.0f, 0.0f));
    renderContext->clearRtv(fbo->getRenderTargetView(2).get(), glm::vec4(0.5, 0.5f, 1.0f, 0.0f));
    renderContext->clearRtv(fbo->getRenderTargetView(3).get(), glm::vec4(0.0, 0.0f, 0.0f, 0.0f));
    renderContext->clearRtv(fbo->getRenderTargetView(4).get(), glm::vec4(1.0, 1.0f, 1.0f, 1.0f));

    bakeShader.State()->setFbo(fbo);
    bakeShader.State()->setViewport(0, GraphicsState::Viewport(0, 0, (float)iW, (float)iH, 0, 1), true);
    bakeShader.State()->setRasterizerState(rasterstate);
    bakeShader.State()->setBlendState(blendstateBake);

    glm::mat4 V, VP;
    V = { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, W, 1} };
    VP = glm::orthoLH(-W, W, 0.f, H, -1000.0f, 1000.0f) * V;
    rmcv::mat4 viewproj;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            viewproj[j][i] = VP[j][i];
        }
    }

    bakeShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
    bakeShader.Vars()["gConstantBuffer"]["eyePos"] = float3(0, 0, -100000);  // just very far sort of parallel
    bakeShader.Vars()["gConstantBuffer"]["bake_radius_alpha"] = W;  // just very far sort of parallel


    _plantMaterial::static_materials_veg.setTextures(varBakeTextures);
    _plantMaterial::static_materials_veg.rebuildStructuredBuffer();

    //plantData->setBlob(plantBuf.data(), 0, 1 * sizeof(plant));
    //vertexData->setBlob(ribbonVertex::packed.data(), 0, ribbonVertex::packed.size() * sizeof(ribbonVertex8));                // FIXME uploads should be smaller

    //bakeShader.drawInstanced(renderContext, ribbonVertex::packed.size(), 1);
    bakeShader.drawInstanced(renderContext, 32, totalBlocksToRender);

    renderContext->flush(true);


    compute_bakeFloodfill.Vars()->setTexture("gAlbedo", fbo->getColorTexture(0));
    compute_bakeFloodfill.Vars()->setTexture("gNormal", fbo->getColorTexture(2));
    compute_bakeFloodfill.Vars()->setTexture("gTranslucency", fbo->getColorTexture(4));
    compute_bakeFloodfill.Vars()->setTexture("gpbr", fbo->getColorTexture(3));

    int numBuffer = 32;
    for (int i = 0; i < numBuffer; i++)
    {
        compute_bakeFloodfill.dispatch(renderContext, iW / 4, iH / 4);
    }



    //std::filesystem::path full_path = root->path;
    //std::string name = full_path.stem().string() + "_billboard";
    //std::string newPath = full_path.parent_path().string() + "\\" + name;

    replaceAllVEG(resource, "/", "\\");
    {
        _plantMaterial Mat;
        Mat._constData.translucency = 1;
        Mat.albedoPath = newRelative + _info->material.name + "_albedo.dds";
        Mat.albedoName = _info->material.name + "_albedo.dds";
        Mat.normalPath = newRelative + _info->material.name + "_normal.dds";
        Mat.normalName = _info->material.name + "_normal.dds";
        Mat.translucencyPath = newRelative + _info->material.name + "_translucency.dds";
        Mat.translucencyName = _info->material.name + "_translucency.dds";

        fbo->getColorTexture(0).get()->generateMips(renderContext);
        fbo->getColorTexture(1).get()->generateMips(renderContext);
        fbo->getColorTexture(2).get()->generateMips(renderContext);
        fbo->getColorTexture(3).get()->generateMips(renderContext);
        fbo->getColorTexture(4).get()->generateMips(renderContext);

        fbo->getColorTexture(0).get()->captureToFile(3, 0, newDir + "_albedo.png", Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::ExportAlpha);
        fbo->getColorTexture(2).get()->captureToFile(3, 0, newDir + "_normal.png", Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);
        fbo->getColorTexture(4).get()->captureToFile(3, 0, newDir + "_translucency.png", Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);

        {
            std::string png = newDir + "_albedo.png";
            std::string cmdExp = resource + "Compressonator\\CompressonatorCLI -miplevels 6 \"" + png + "\" " + resource + "Compressonator\\temp_mip.dds";
            replaceAllVEG(cmdExp, "/", "\\");
            system(cmdExp.c_str());
            std::string cmdExp2 = resource + "Compressonator\\CompressonatorCLI -fd BC7 " + resource + "Compressonator\\temp_mip.dds \"" + resource + Mat.albedoPath + "\"";
            replaceAllVEG(cmdExp2, "/", "\\");
            system(cmdExp2.c_str());
            //??? Shall I not rather use my compute compressor here?
        }

        {
            std::string png = newDir + "_normal.png";
            std::string cmdExp = resource + "Compressonator\\CompressonatorCLI -miplevels 6 \"" + png + "\" " + resource + "Compressonator\\temp_mip.dds";
            replaceAllVEG(cmdExp, "/", "\\");
            system(cmdExp.c_str());
            std::string cmdExp2 = resource + "Compressonator\\CompressonatorCLI -fd BC6H " + resource + "Compressonator\\temp_mip.dds \"" + resource + Mat.normalPath + "\"";
            replaceAllVEG(cmdExp2, "/", "\\");
            system(cmdExp2.c_str());
            //??? Shall I not rather use my compute compressor here?
        }

        {
            std::string png = newDir + "_translucency.png";
            std::string cmdExp = resource + "Compressonator\\CompressonatorCLI -miplevels 6 \"" + png + "\" " + resource + "Compressonator\\temp_mip.dds";
            replaceAllVEG(cmdExp, "/", "\\");
            system(cmdExp.c_str());
            std::string cmdExp2 = resource + "Compressonator\\CompressonatorCLI -fd BC7 " + resource + "Compressonator\\temp_mip.dds \"" + resource + Mat.translucencyPath + "\"";
            replaceAllVEG(cmdExp2, "/", "\\");
            system(cmdExp2.c_str());
            //??? Shall I not rather use my compute compressor here?
        }

        Mat._constData.translucency = 1.f;
        Mat._constData.alphaPow = 1.4f;

        std::ofstream os(resource + _info->material.path);
        cereal::JSONOutputArchive archive(os);
        Mat.serialize(archive, _PLANTMATERIALVERSION);

        //_info->material.reload();
    }
}


void _rootPlant::render(RenderContext* _renderContext, const Fbo::SharedPtr& _fbo, GraphicsState::Viewport _viewport, Texture::SharedPtr _hdrHalfCopy,
    rmcv::mat4  _viewproj, float3 camPos, rmcv::mat4  _view, rmcv::mat4  _clipFrustum)
{
    FALCOR_PROFILE("vegetation");
    renderContext = _renderContext;

    //if (_plantMaterial::static_materials_veg.modified || _plantMaterial::static_materials_veg.modifiedData)
    //if (ribbonVertex::packed.size() > 1)
    if (tempUpdateRender)
    {
        tempUpdateRender = false;
        _plantMaterial::static_materials_veg.modified = false;
        _plantMaterial::static_materials_veg.modifiedData = false;
        _plantMaterial::static_materials_veg.setTextures(varVegTextures);
        _plantMaterial::static_materials_veg.rebuildStructuredBuffer();


        vegetationShader.State()->setRasterizerState(rasterstate);
        vegetationShader.State()->setBlendState(blendstate);

        billboardShader.State()->setRasterizerState(rasterstate);
        billboardShader.State()->setBlendState(blendstate);
        _plantMaterial::static_materials_veg.setTextures(varBBTextures);
        
        
    }

    if (ribbonVertex::packed.size() > 1 && totalBlocksToRender >= 100)
    {
        FALCOR_PROFILE("compute_veg_lods");
        compute_clearBuffers.dispatch(_renderContext, 1, 1);

        compute_calulate_lod.Vars()["gConstantBuffer"]["view"] = _view;
        compute_calulate_lod.Vars()["gConstantBuffer"]["frustum"] = _clipFrustum;
        compute_calulate_lod.dispatch(_renderContext, 16384 / 256, 1);
    }
    

    if (ribbonVertex::packed.size() > 1 && totalBlocksToRender >= 100)
    {
        FALCOR_PROFILE("billboards");

        billboardShader.State()->setFbo(_fbo);
        billboardShader.State()->setViewport(0, _viewport, true);
        billboardShader.State()->setRasterizerState(rasterstate);
        billboardShader.State()->setBlendState(blendstate);

        billboardShader.Vars()["gConstantBuffer"]["viewproj"] = _viewproj;
        billboardShader.Vars()["gConstantBuffer"]["eyePos"] = camPos;
        billboardShader.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);   // it chences every time we loose focus
        
        if (totalBlocksToRender >= 100)
        {
            billboardShader.renderIndirect(_renderContext, drawArgs_billboards);
        }
    }

    if (ribbonVertex::packed.size() > 1)
    {
        FALCOR_PROFILE("ribbonShader");

        vegetationShader.State()->setFbo(_fbo);
        vegetationShader.State()->setViewport(0, _viewport, true);
        vegetationShader.State()->setRasterizerState(rasterstate);
        vegetationShader.State()->setBlendState(blendstate);

        vegetationShader.Vars()["gConstantBuffer"]["viewproj"] = _viewproj;
        vegetationShader.Vars()["gConstantBuffer"]["eyePos"] = camPos;
        static float time = 0.0f;
        time += 0.01f;  // FIXME I NEED A Timer
        vegetationShader.Vars()["gConstantBuffer"]["time"] = time;

        vegetationShader.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);   // it chences every time we loose focus

        //vegetationShader.drawInstanced(_renderContext, ribbonVertex::packed.size(), 1);
        if (totalBlocksToRender < 100)
        {
            vegetationShader.drawInstanced(_renderContext, 32, totalBlocksToRender);    // single plant
        }
        else if (totalBlocksToRender >= 100)
        {
            vegetationShader.renderIndirect(_renderContext, drawArgs_vegetation);
        }
    }

}
