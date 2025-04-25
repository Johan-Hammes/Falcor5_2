#include "vegetationBuilder.h"
#include "imgui.h"

//#pragma optimize("", off)

#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}


materialCache_plants _plantMaterial::static_materials_veg;
std::string materialCache_plants::lastFile;


_plantBuilder *_rootPlant::selectedPart = nullptr;
_plantMaterial *_rootPlant::selectedMaterial = nullptr;

std::mt19937 _rootPlant::generator(100);
std::uniform_real_distribution<> _rootPlant::rand_1(-1.f, 1.f);
std::uniform_real_distribution<> _rootPlant::rand_01(0.f, 1.f);
std::uniform_int_distribution<> _rootPlant::rand_int(0, 65535);


float ribbonVertex::objectScale = 0.002f;  //0.002 for trees  // 32meter block 2mm presision
float ribbonVertex::radiusScale = 0.20f;//  so biggest radius now objectScale / 2.0f;
float ribbonVertex::O = 16384.0f * ribbonVertex::objectScale * 0.5f;
float3 ribbonVertex::objectOffset = float3(O, O * 0.5f, O);






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




    ribbonVertex8 p;

    p.a = ((type & 0x1) << 31) + (startBit << 30) + (x14 << 16) + y16;
    p.b = (z14 << 18) + ((material & 0x3ff) << 8) + anim8;
    p.c = (up_yaw9 << 23) + (up_pitch8 << 15) + v15;
    p.d = (left_yaw9 << 23) + (left_pitch8 << 15) + (u7 << 8) + radius8;
    p.e = (coneYaw9 << 23) + (conePitch8 << 15) + (cone7 << 8) + depth8;
    p.f = (ao << 24) + (shadow << 16) + (albedoScale << 8) + translucencyScale;
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

int materialCache_plants::find_insert_material(const std::string _path, const std::string _name)
{
    std::filesystem::path fullPath = _path + _name + ".vegetationMaterial";
    if (std::filesystem::exists(fullPath))
    {
        return find_insert_material(fullPath);
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
                return find_insert_material(subPath);
            }
        }
    }

    fprintf(terrafectorSystem::_logfile, "error : vegetation material - %s does not exist\n", _name.c_str());
    return -1;
}


// force a new one? copy last and force a save ?
int materialCache_plants::find_insert_material(const std::filesystem::path _path)
{
    for (uint i = 0; i < materialVector.size(); i++)
    {
        if (materialVector[i].fullPath.compare(_path) == 0)
        {
            return i;
        }
    }

    // else add new
    if (std::filesystem::exists(_path))
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



int materialCache_plants::find_insert_texture(const std::filesystem::path _path, bool isSRGB)
{
    modified = true;

    for (uint i = 0; i < textureVector.size(); i++)
    {
        if (textureVector[i]->getSourcePath().compare(_path) == 0)
        {
            return i;
        }
    }

    Texture::SharedPtr tex = Texture::createFromFile(_path.string(), true, isSRGB);
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
    _constData.albedoTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + albedoPath, true);
    _constData.alphaTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + alphaPath, true);
    _constData.normalTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + normalPath, false);
    _constData.translucencyTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + translucencyPath, false);
}



void replaceAllVEG(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
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
    index = _plantMaterial::static_materials_veg.find_insert_material(terrafectorEditorMaterial::rootFolder + path);
}

bool _vegMaterial::renderGui()
{
    static int ID = 139999;
    if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed))
    {
        std::string tool = path + "\n{" + std::to_string(index) + "}";
        TOOLTIP(tool.c_str());
        if (ImGui::IsItemClicked())  loadFromFile();
        ImGui::PushID(ID);
        {
            static float2 TST;
            ImGui::DragFloat2("albedo", &TST.x, 0.1f, 0.1f, 2.0f);
            ImGui::DragFloat2("translucency", &translucencyScale.x, 0.1f, 0.1f, 2.0f);
            ImGui::PopID();
            ID++;
        }        
        ImGui::TreePop();
    }
    

    return false;
}





template <class T>     T randomVector<T>::get()
{
    static int rnd_idx = 0;
    rnd_idx += _rootPlant::rand_int(_rootPlant::generator);
    rnd_idx %= data.size();
    return data[rnd_idx];
}

template <class T>     void randomVector<T>::renderGui(char* name)
{
    bool first = true;
    if (ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
    {
        if (ImGui::Button(".  +  .")) { data.emplace_back(); }
        ImGui::SameLine(0, 50);
        if (ImGui::Button(".  -  .")) { if(data.size() > 1) data.pop_back(); }

        for (auto& D : data) D.renderGui();
        
        ImGui::TreePop();
    }
}




ImVec4 select_color = ImVec4(0.2f, 0.2f, 0.0f, 1);
ImVec4 twig_color = ImVec4(0.1f, 0.07f, 0.0f, 1);
ImVec4 leaf_color = ImVec4(0.0f, 0.1f, 0.01f, 1);
ImVec4 mat_color = ImVec4(0.0f, 0.03f, 0.09f, 1);

#define CLICK_PART if (ImGui::IsItemClicked()) { _rootPlant::selectedPart = this; _rootPlant::selectedMaterial = nullptr; }
#define CLICK_MATERIAL if (ImGui::IsItemClicked()) { _rootPlant::selectedPart = nullptr; _rootPlant::selectedMaterial = &mat; }
#define TOOLTIP_PART(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}
#define TOOLTIP_MATERIAL(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}
#define TREE_MAT(x)  style.Colors[ImGuiCol_HeaderActive] = mat_color; ImGui::TreeNodeEx(x, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed);
#define TREE_LEAF(x)  style.Colors[ImGuiCol_HeaderActive] = leaf_color; if(ImGui::TreeNodeEx(x, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed)) 
#define TREE_TWIG(x)  style.Colors[ImGuiCol_HeaderActive] = twig_color; if(ImGui::TreeNodeEx(x, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed))

#define R_LENGTH(name,data,t1,t2)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(80, 0); \
                    ImGui::SetNextItemWidth(80);    \
                    if (ImGui::DragFloat("##X", &data.x, 1.f, 0, 300, "%2.f")) changed = true; \
                    TOOLTIP(t1); \
                    ImGui::SameLine(0, 10); \
                    ImGui::SetNextItemWidth(80);    \
                    if (ImGui::DragFloat("##Y", &data.y, 0.1f, 0, 1, "%1.3f")) changed = true; \
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



// _leafBuilder
// -----------------------------------------------------------------------------------------------------------------------------------

void _leafBuilder::renderGui(Gui* _gui)
{
    ImGui::NewLine();
    ImGui::Text("stem");
    unsigned int gui_id = 1001;
    R_LENGTH("length", stem_length, "stem length in mm", "random");
    R_LENGTH("width", stem_width, "stem width in mm", "random");
    if (stem_length.x > 0 && stem_width.x > 0)
    {
        R_LENGTH("curve", stem_curve, "stem curvature", "random");
        R_LENGTH("angle", stem_to_leaf, "stem to leaf angle in radians", "random");
        R_VERTS("num verts", stemVerts);
        stem_Material.renderGui();
    }

    ImGui::NewLine();
    ImGui::Text("leaf");
    if (ImGui::Checkbox("face camera", &cameraFacing)) changed = true;
    R_LENGTH("length", leaf_length, "mm", "random");
    R_LENGTH("width", leaf_width, "mm", "random");
    R_LENGTH("offset", width_offset, "", "random");
    R_LENGTH("curve", leaf_curve, "radians", "random");
    R_LENGTH("twist", leaf_twist, "radians", "random");
    R_LENGTH("gavity", gravitrophy, "rotate towards the ground or sky", "random");
    R_VERTS("num verts", numVerts);
    ImGui::NewLine();
    materials.renderGui("materials");
}


void _leafBuilder::treeView()
{
    auto& style = ImGui::GetStyle();

    TREE_LEAF("leaf")
    {
        CLICK_PART;
        std::string tool = name + "\n" + path;
        TOOLTIP(tool.c_str());
        ImGui::TreePop();
    }
}


void _leafBuilder::build(buildSetting& _settings)
{
    std::mt19937 gen(_settings.seed);
    std::uniform_real_distribution<> dist(-1.f, 1.f);
    std::uniform_int_distribution<> distAlbedo(-50, 50);
    glm::mat4 temp[200];   // always build all 200
    glm::mat4 node = _settings.root;
    ribbonVertex R;

    if (stem_length.x > 0 && stem_width.x > 0)
    {
        float length = stem_length.x * (1.f + stem_length.y * dist(gen)) / 100.f * 0.001f;   // to meters and age
        float width = stem_width.x * 0.001f;
        float curve = stem_curve.x * (1.f + stem_curve.y * dist(gen)) / 100.f;
        for (int i = 0; i < 100; i++)
        {
            node = glm::rotate(node, curve, (glm::vec3)node[0]);
            node = glm::translate(node, (glm::vec3)node[2] * length);
            temp[i] = node;
        }

        // rotation from stem to leaf
        node = glm::rotate(node, curve, (glm::vec3)node[0]);

        bool firstVertex = true;
        _vegMaterial mat = materials.get();
        unsigned char albedoScale = (unsigned char)(glm::lerp(mat.albedoScale.x, mat.albedoScale.y, 1.f - _settings.age) * 127.f);// +distAlbedo(gen);
        unsigned char translucentScale = (unsigned char)(glm::lerp(mat.translucencyScale.x, mat.translucencyScale.y, 1.f - _settings.age) * 127.f);
        {
            float length = leaf_length.x * (1.f + leaf_length.y * dist(gen)) / 100.f * 0.001f;   // to meters and age
            float width = leaf_width.x * (1.f + leaf_width.y * dist(gen)) / 100.f * 0.001f;
            float gravi = gravitrophy.x * (1.f + gravitrophy.y * dist(gen)) / 100.f;
            float curve = leaf_curve.x / 100.f;
            float twist = leaf_twist.x / 100.f;
            for (int i = 0; i < 100; i++)
            {
                float t = (float)i / 100.f;
                float du = __min(1.f, sin(pow(t, width_offset.x) * 3.1415f) + width_offset.y);
                // NUMVERTS so if 2 just make it big, this can happen especialyl petals if (leafCNT < 3) du = 1;

                curve += (leaf_curve.y * dist(gen)) / 100.f;
                twist += (leaf_twist.y * dist(gen)) / 100.f;
                node = glm::rotate(node, curve, (glm::vec3)node[0]);
                node = glm::rotate(node, twist, (glm::vec3)node[2]);
                node = glm::translate(node, (glm::vec3)node[2] * length);
                temp[100 + i] = node;

                //R.startSet(, cameraFacing)  // for first vertex
                // R.set(node, radius, material, uv, A_scale, T_scale);
                R.type = !cameraFacing;
                R.startBit = !firstVertex;
                R.position = node[3];
                R.radius = width * 0.5f * du;
                R.bitangent = node[2];
                R.tangent = node[0];
                R.material = mat.index;
                R.albedoScale = albedoScale;
                R.translucencyScale = translucentScale;
                R.uv = float2(du, 1.f + t);

                firstVertex = false;
            }
        }
    }
}






// _twigBuilder
// -----------------------------------------------------------------------------------------------------------------------------------


void _twigBuilder::renderGui(Gui* _gui)
{
    ImGui::Text("_twigBuilder");

}

void _twigBuilder::treeView()
{
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, twig_color);

    ImGui::PushStyleColor(ImGuiCol_Header, twig_color);
    if (ImGui::TreeNodeEx("twig", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed))
    {
        if (ImGui::IsItemClicked()) { _rootPlant::selectedPart = this; _rootPlant::selectedMaterial = nullptr; }
        TOOLTIP("twig tipe - NAME here\nmaybe full path vegetation/bushes/grass/grass_03");

        ImGui::PushStyleColor(ImGuiCol_Header, mat_color);
        ImGui::TreeNodeEx("mat-stem", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed);
        if (ImGui::IsItemClicked()) { ; }
        TOOLTIP("material tipe");


        ImGui::PopStyleColor();

        for (_plantBuilder& l : leaves.data)
        {
            l.treeView();
        }
        for (_plantBuilder& l : tip.data)
        {
            l.treeView();
        }

        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}

void _twigBuilder::build(buildSetting& _settings)
{
}







void _rootPlant::renderGui(Gui* _gui)
{
    Gui::Window builderPanel(_gui, "vegetation builder", { 900, 900 }, { 100, 100 });
    {
        ImGui::PushFont(_gui->getFont("roboto_18"));

        ImGui::Columns(2);
        if (ImGui::Button("new Leaf")) { if (root) delete root; root = new _leafBuilder;  _rootPlant::selectedPart = root; _rootPlant::selectedMaterial = nullptr; }
        ImGui::SameLine();
        if (ImGui::Button("new Twig")) { if (root) delete root; root = new _twigBuilder;  _rootPlant::selectedPart = root; _rootPlant::selectedMaterial = nullptr; }

        ImGui::NewLine();
        if (root) root->treeView();

        ImGui::NextColumn();
        // now render the selected item.
        if (selectedPart)
        {
            ImGui::PushFont(_gui->getFont("roboto_20"));
            {
                ImGui::Text(selectedPart->name.c_str());  // move to root lcass
                TOOLTIP(selectedPart->path.c_str());
                if (ImGui::Button("Load ..."))
                {
                    std::filesystem::path filepath;
                    FileDialogFilterVec filters = {{selectedPart->ext()}};
                    if (openFileDialog(filters, filepath))
                    {
                        std::ifstream is(filepath.string());
                        cereal::JSONInputArchive archive(is);
                        _leafBuilder* leaf = dynamic_cast<_leafBuilder*>(selectedPart);
                        if (leaf) archive(*leaf);
                        _twigBuilder* twig = dynamic_cast<_twigBuilder*>(selectedPart);
                        if (twig) archive(*twig);

                        selectedPart->path = materialCache::getRelative(filepath.string());
                        selectedPart->name = filepath.filename().string();
                    }
                }
                ImGui::SameLine();
                if (selectedPart->changed) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.0f, 0.5f));
                    ImGui::SameLine(0, 20);
                    if (ImGui::Button("Save ..."))
                    {
                        std::ofstream os(terrafectorEditorMaterial::rootFolder + selectedPart->path);
                        cereal::JSONOutputArchive archive(os);
                        _leafBuilder* leaf = dynamic_cast<_leafBuilder*>(selectedPart);
                        if (leaf) archive(*leaf);
                        _twigBuilder* twig = dynamic_cast<_twigBuilder*>(selectedPart);
                        if (twig) archive(*twig);
                        selectedPart->changed = false;
                    }
                    ImGui::PopStyleColor();
                }
                
                ImGui::SameLine(0, 20);
                if (ImGui::Button("Save as"))
                {
                    std::filesystem::path filepath;
                    FileDialogFilterVec filters = { {selectedPart->ext()} };
                    if (saveFileDialog(filters, filepath))
                    {
                        std::ofstream os(filepath.string());
                        cereal::JSONOutputArchive archive(os);
                        _leafBuilder* leaf = dynamic_cast<_leafBuilder*>(selectedPart);
                        if (leaf) archive(*leaf);
                        _twigBuilder* twig = dynamic_cast<_twigBuilder*>(selectedPart);
                        if (twig) archive(*twig);

                        selectedPart->path = materialCache::getRelative(filepath.string());
                        selectedPart->name = filepath.filename().string();
                        selectedPart->changed = false;
                    }
                }
                ImGui::PopFont();
            }
            selectedPart->renderGui(_gui);
        }
        if (selectedMaterial)
        {
        }

        ImGui::PopFont();
    }
    builderPanel.release();

}



void _rootPlant::build(glm::mat4 root, float _age, float _lodPixelsize, int _seed)
{
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



void _rootPlant::bake(RenderContext* _renderContext)
{
}
