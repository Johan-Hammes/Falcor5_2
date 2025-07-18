#include "vegetationBuilder.h"
#include "imgui.h"
#include "PerlinNoise.hpp"      //https://github.com/Reputeless/PerlinNoise/blob/master/PerlinNoise.hpp



#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}



std::array<std::string, 77> resourceString =
{
    "Unknown",
    "R8Unorm",
    "R8Snorm",
    "R16Unorm",
    "R16Snorm",
    "RG8Unorm",
    "RG8Snorm",
    "RG16Unorm",
    "RG16Snorm",
    "RGB16Unorm",
    "RGB16Snorm",
    "R24UnormX8",
    "RGB5A1Unorm",
    "RGBA8Unorm",
    "RGBA8Snorm",
    "RGB10A2Unorm",
    "RGB10A2Uint",
    "RGBA16Unorm",
    "RGBA8UnormSrgb",
    "R16Float",
    "RG16Float",
    "RGB16Float",
    "RGBA16Float",
    "R32Float",
    "R32FloatX32",
    "RG32Float",
    "RGB32Float",
    "RGBA32Float",
    "R11G11B10Float",
    "RGB9E5Float",
    "R8Int",
    "R8Uint",
    "R16Int",
    "R16Uint",
    "R32Int",
    "R32Uint",
    "RG8Int",
    "RG8Uint",
    "RG16Int",
    "RG16Uint",
    "RG32Int",
    "RG32Uint",
    "RGB16Int",
    "RGB16Uint",
    "RGB32Int",
    "RGB32Uint",
    "RGBA8Int",
    "RGBA8Uint",
    "RGBA16Int",
    "RGBA16Uint",
    "RGBA32Int",
    "RGBA32Uint",
    "BGRA8Unorm",
    "BGRA8UnormSrgb",
    "BGRX8Unorm",
    "BGRX8UnormSrgb",
    "Alpha8Unorm",
    "Alpha32Float",
    "R5G6B5Unorm",
    "D32Float",
    "D16Unorm",
    "D32FloatS8X24",
    "D24UnormS8",
    "BC1Unorm",
    "BC1UnormSrgb",
    "BC2Unorm",
    "BC2UnormSrgb",
    "BC3Unorm",
    "BC3UnormSrgb",
    "BC4Unorm",
    "BC4Snorm",
    "BC5Unorm",
    "BC5Snorm",
    "BC6HS16",
    "BC6HU16",
    "BC7Unorm",
    "BC7UnormSrgb",
};



materialCache_plants _plantMaterial::static_materials_veg;
std::string materialCache_plants::lastFile;


_plantBuilder* _rootPlant::selectedPart = nullptr;
_plantMaterial* _rootPlant::selectedMaterial = nullptr;

std::mt19937 _rootPlant::generator(100);
std::uniform_real_distribution<> _rootPlant::rand_1(-1.f, 1.f);
std::uniform_real_distribution<> _rootPlant::rand_01(0.f, 1.f);
std::uniform_int_distribution<> _rootPlant::rand_int(0, 65535);


float ribbonVertex::objectScale = 0.002f;  //0.002 for trees  // 32meter block 2mm presision
float ribbonVertex::radiusScale = 5.0f;//  so biggest radius now objectScale / 2.0f;
float ribbonVertex::O = 16384.0f * ribbonVertex::objectScale * 0.5f;
float3 ribbonVertex::objectOffset = float3(O, O * 0.5f, O);

std::vector<ribbonVertex>    ribbonVertex::ribbons;
std::vector<ribbonVertex8>    ribbonVertex::packed;
std::vector <_plant_anim_pivot> ribbonVertex::pivotPoints;
bool ribbonVertex::pushStart = false;
int ribbonVertex::lod_startBlock;   // This is the blok this lod started on
int ribbonVertex::maxBlocks;   // this will not accept more verts once we push past ? But how to handle when pushing lods
int ribbonVertex::totalRejectedVerts;



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

uint ribbonVertex::S_root = 0;

void ribbonVertex::pushPivot(float3 _root, float3 _extent, float _frequency, float _stiffness, float _shift, unsigned char _offset)
{
    // in future this function will do the packing when we pack _plant_anim_pivot tighter
    _plant_anim_pivot p;
    p.root = _root;
    p.extent = _extent;
    p.frequency = _frequency;
    p.stiffness = _stiffness;
    p.shift = _shift;
    p.offset = _offset;
    pivotPoints.push_back(p);
}

#pragma optimize("", off)
ribbonVertex8 ribbonVertex::pack()
{
    int x14 = ((int)((position.x + objectOffset.x) / objectScale)) & 0x3fff;
    int y16 = ((int)((position.y + objectOffset.y) / objectScale)) & 0xffff;
    int z14 = ((int)((position.z + objectOffset.z) / objectScale)) & 0x3fff;

    //int anim8 = (int)floor(__min(1.f, __max(0.f, anim_blend)) * 255.f);
    int DiamondFlag = (int)diamond;

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

    uint AoA = (int)clamp(ambientOcclusion * 255, 0.f, 255.f);

    uint L_stiff = (int)clamp((leafStiffness - 0.1f) / 0.004f, 0.f, 255.f);
    uint L_freq = (int)clamp((leafFrequency) / 0.004f, 0.f, 255.f);
    uint L_index = (int)clamp((leafIndex) / 0.004f, 0.f, 255.f);



    ribbonVertex8 p;
    // type is used for cameraFacing - rename    
    p.a = ((type & 0x1) << 31) + (startBit << 30) + (x14 << 16) + y16;
    p.b = (z14 << 18) + ((material & 0x3ff) << 8) + (DiamondFlag & 0x1);  // This leaves 7 bits free here
    p.c = (up_yaw9 << 23) + (up_pitch8 << 15) + v15;
    p.d = (left_yaw9 << 23) + (left_pitch8 << 15) + (u7 << 8) + radius8;
    p.e = (coneYaw9 << 23) + (conePitch8 << 15) + (cone7 << 8) + depth8;
    p.f = (AoA << 24) + (shadow << 16) + (albedo << 8) + translucency;
    p.g = ((root[0] & 0xff) << 24) + ((root[1] & 0xff) << 16) + ((root[2] & 0xff) << 8) + (root[3] & 0xff);
    p.h = (L_index << 24) + (L_stiff << 16) + (L_freq << 8) + leafRoot; // add stiffnes etc later
    return p;
}
#pragma optimize("", on)




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
        fullPath = terrafectorEditorMaterial::rootFolder + "/vegetationMaterial";
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
    if ((_path.string().find("vegetationMaterial") != std::string::npos) && std::filesystem::exists(_path))
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
    if (!std::filesystem::exists(_path)) return -1;             // it doesmt ecists - common when baking and a texture fails return -1


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

    Texture::SharedPtr tempTexture = Texture::createFromFile(_path, false, false);
    int totalPixels = tempTexture->getWidth() * tempTexture->getHeight();
    bool pleaseCompress = totalPixels > (64 * 64);  // so dont compress teh small ones

    std::string ddsFilename = _path.string();
    if (pleaseCompress)
    {
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
            if (displaySortMap[cnt].name.find("vegetationMaterial") != std::string::npos)
            {
                thisPath = displaySortMap[cnt].name.substr(12, displaySortMap[cnt].name.find_last_of("\\/") - 12);  // -12 removes the first vegetation
            }
            if (thisPath != path) {
                ImGui::NewLine();
                ImGui::Text(thisPath.c_str());
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
                TOOLTIP((material.fullPath.string() + "\n" + std::to_string(displaySortMap[cnt].index)).c_str());
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
    ImGui::PushFont(mpGui->getFont("header2"));
    ImGui::Text("Tex [%d]   %3.1fMb", (int)textureVector.size(), texMb);
    ImGui::PopFont();



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
                uint32_t format = (int)pT->getFormat();
                //ImGui::PushFont(mpGui->getFont("header2"));
                ImGui::SetTooltip("%d  :  %s \n%d x %d {%d mips} \n%s", i, pT->getSourcePath().string().c_str(), pT->getWidth(), pT->getHeight(), pT->getMipCount(), resourceString[format].c_str());
                //ImGui::PopFont();

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
    bool changed = false;

    ImGui::PushFont(_gui->getFont("small"));
    {

        ImGui::Text(displayName.c_str());

        ImGui::SetNextItemWidth(100);
        if (ImGui::Button("load")) { import(); }


        if (changedForSave) {
            ImGui::SameLine(0, 30);
            ImGui::SetNextItemWidth(100);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.0f, 0.5f));
            ImGui::SameLine(0, 20);
            if (ImGui::Button("Save"))
            {
                save();
                changedForSave = false;
            }
            ImGui::PopStyleColor();
        }

        ImGui::SameLine(0, 30);
        ImGui::SetNextItemWidth(100);
        ImGui::SameLine(0, 20);
        if (ImGui::Button("Save as"))
        {
            eXport();
        }

        ImGui::PushID(9990);
        if (ImGui::Selectable(albedoName.c_str())) { loadTexture(0); changed = true; }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("ALBEDO");
            _plantMaterial::static_materials_veg.dispTexIndex = _constData.albedoTexture;
        }
        ImGui::PopID();


        ImGui::PushID(9991);
        if (ImGui::Selectable(alphaName.c_str())) { loadTexture(1); changed = true; }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("ALPHA");
            _plantMaterial::static_materials_veg.dispTexIndex = _constData.alphaTexture;
        }
        ImGui::PopID();


        ImGui::PushID(9992);
        if (ImGui::Selectable(translucencyName.c_str())) { loadTexture(2); changed = true; }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("TRANSLUCENCY");
            _plantMaterial::static_materials_veg.dispTexIndex = _constData.translucencyTexture;
        }
        ImGui::PopID();


        ImGui::PushID(9993);
        if (ImGui::Selectable(normalName.c_str())) { loadTexture(3); changed = true; }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("NORMAL");
            _plantMaterial::static_materials_veg.dispTexIndex = _constData.normalTexture;
        }
        ImGui::PopID();

        if (ImGui::DragFloat("Translucency", &_constData.translucency, 0.01f, 0, 10)) changed = true;
        if (ImGui::DragFloat("alphaPow", &_constData.alphaPow, 0.01f, 0.1f, 3.f)) changed = true;

        ImGui::NewLine();

        if (ImGui::ColorEdit3("albedo front", &_constData.albedoScale[0].x)) changed = true;
        if (ImGui::DragFloat("roughness front", &_constData.roughness[0], 0.01f, 0, 1)) changed = true;
        if (ImGui::ColorEdit3("albedo back", &_constData.albedoScale[1].x)) changed = true;
        if (ImGui::DragFloat("roughness back", &_constData.roughness[1], 0.01f, 0, 1)) changed = true;

        ImGui::NewLine();
        ImGui::Text("index a, alpha, trans, norm(%d, %d, %d, %d) rgb", _constData.albedoTexture, _constData.alphaTexture, _constData.translucencyTexture, _constData.normalTexture);
    }
    ImGui::PopFont();

    if (changed)
    {
        changedForSave = true;
        _plantMaterial::static_materials_veg.rebuildStructuredBuffer();
    }
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
        //serialize(archive, 100);
        archive(*this);

        if (_replacePath) fullPath = _path;
        displayName = _path.stem().string();
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
    //serialize(archive, 100);
    archive(*this);
}
void _plantMaterial::eXport(std::filesystem::path _path)
{
    std::ofstream os(_path);
    cereal::JSONOutputArchive archive(os);
    //serialize(archive, 100);
    archive(*this);
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
    FileDialogFilterVec filters = { {"leaf"}, {"stem"}, {"clump"} };
    if (openFileDialog(filters, filepath))
    {
        if (filepath.string().find("leaf") != std::string::npos) { plantPtr.reset(new _leafBuilder); type = P_LEAF; }
        if (filepath.string().find("stem") != std::string::npos) { plantPtr.reset(new _stemBuilder);  type = P_STEM; }
        if (filepath.string().find("clump") != std::string::npos) { plantPtr.reset(new _clumpBuilder);  type = P_CLUMP; }

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
    case P_STEM:   plantPtr.reset(new _stemBuilder);   break;
    case P_CLUMP:   plantPtr.reset(new _clumpBuilder);   break;
    default:  plantPtr.reset();  break;
    }

    if (plantPtr)
    {
        plantPtr->path = path;
        plantPtr->name = name;
        plantPtr->loadPath();
    }
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
        if (ImGui::Button(".  -  .")) { if (data.size() > 1) data.pop_back(); }
        ImGui::SameLine(0, 50);
        if (ImGui::Button(".  +  .")) { data.emplace_back(); }


        for (auto& D : data) D.renderGui(gui_id);

        ImGui::TreePop();
    }
}




bool anyChange = true;

ImVec4 selected_color = ImVec4(0.4f, 0.1f, 0.0f, 1);
ImVec4 stem_color = ImVec4(0.05f, 0.02f, 0.0f, 1);
ImVec4 leaf_color = ImVec4(0.01f, 0.04f, 0.01f, 1);
ImVec4 mat_color = ImVec4(0.0f, 0.01f, 0.07f, 1);

#define CLICK_PART if (ImGui::IsItemClicked()) { _rootPlant::selectedPart = this; _rootPlant::selectedMaterial = nullptr; }
#define CLICK_MATERIAL if (ImGui::IsItemClicked()) { _rootPlant::selectedPart = nullptr; _rootPlant::selectedMaterial = &mat; }
#define TOOLTIP_PART(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}
#define TOOLTIP_MATERIAL(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}
//#define TREE_MAT(x)  style.Colors[ImGuiCol_HeaderActive] = mat_color; ImGui::TreeNodeEx(x, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed);
//#define TREE_LEAF(x)  style.Colors[ImGuiCol_HeaderActive] = leaf_color; if(ImGui::TreeNodeEx(x, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Framed)) 

#define TREE_LINE(x,t,f)  if(ImGui::TreeNodeEx(x, f))

#define textWIDTH 120
#define itemWIDTH 80

#define R_LENGTH(name,data,t1,t2)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(textWIDTH, 0); \
                    ImGui::SetNextItemWidth(itemWIDTH);    \
                    if (ImGui::DragFloat("##X", &data.x, 1.f, 0, 2000, "%2.2fmm")) changed = true; \
                    TOOLTIP(t1); \
                    ImGui::SameLine(0, 10); \
                    ImGui::SetNextItemWidth(itemWIDTH);    \
                    if (ImGui::DragFloat("##Y", &data.y, 0.01f, 0, 1, "%1.3f")) changed = true; \
                    TOOLTIP(t2); \
                    ImGui::PopID(); \
                    gui_id ++;

#define R_LENGTH_EX(name,data, scl, min, max, t1, t2)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(textWIDTH, 0); \
                    ImGui::SetNextItemWidth(itemWIDTH);    \
                    if (ImGui::DragFloat("##X", &data.x, scl, min, max, "%2.2fmm")) changed = true; \
                    TOOLTIP(t1); \
                    ImGui::SameLine(0, 10); \
                    ImGui::SetNextItemWidth(itemWIDTH);    \
                    if (ImGui::DragFloat("##Y", &data.y, 0.01f, 0, 1, "%1.3f")) changed = true; \
                    TOOLTIP(t2); \
                    ImGui::PopID(); \
                    gui_id ++;

#define R_CURVE(name,data,t1,t2)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(textWIDTH, 0); \
                    ImGui::SetNextItemWidth(itemWIDTH);    \
                    if (ImGui::DragFloat("##X", &data.x, 0.01f, -5, 5, "%1.2f rad")) changed = true; \
                    TOOLTIP(t1); \
                    ImGui::SameLine(0, 10); \
                    ImGui::SetNextItemWidth(itemWIDTH);    \
                    if (ImGui::DragFloat("##Y", &data.y, 0.01f, 0, 5, "%1.3f rad")) changed = true; \
                    TOOLTIP(t2); \
                    ImGui::PopID(); \
                    gui_id ++;


#define R_VERTS(name,data)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(textWIDTH, 0); \
                    ImGui::SetNextItemWidth(itemWIDTH);    \
                    if (ImGui::DragInt("##X", &data.x, 0.1f, 2, 10)) changed = true; \
                    ImGui::SameLine(0, 10); \
                    ImGui::SetNextItemWidth(itemWIDTH);    \
                    if (ImGui::DragInt("##Y", &data.y, 0.1f, 3, 32)) changed = true; \
                    ImGui::PopID(); \
                    gui_id ++;

#define R_INT(name,data,min,max,tooltip)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(textWIDTH, 0); \
                    ImGui::SetNextItemWidth(itemWIDTH);    \
                    if (ImGui::DragInt("##d", &data, 0.1f, min, max)) changed = true; \
                    TOOLTIP(tooltip); \
                    ImGui::PopID(); \
                    gui_id ++;

#define R_FLOAT(name,data,speed, min,max,tooltip)    ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(textWIDTH, 0); \
                    ImGui::SetNextItemWidth(itemWIDTH);    \
                    if (ImGui::DragFloat("##X", &data, speed, min, max)) changed = true; \
                    TOOLTIP(tooltip); \
                    ImGui::PopID(); \
                    gui_id ++;


#define FONT_TEXT(f, s) ImGui::PushFont(_gui->getFont(f));   ImGui::Text(s);   ImGui::PopFont();

#define CHECKBOX(name, val, tooltip)  ImGui::PushID(gui_id); \
                    ImGui::Text(name);    \
                    ImGui::SameLine(textWIDTH + itemWIDTH, 0); \
                    style.FrameBorderSize = 1; \
                    if (ImGui::Checkbox("", val)) changed = true;    \
                    style.FrameBorderSize = 0; \
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
            changedForSave = false;
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
    auto& style = ImGui::GetStyle();
    int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_OpenOnArrow;
    unsigned int gui_id = 1001;

    ImGui::PushFont(_gui->getFont("header1"));
    {
        ImGui::Text(name.c_str());
    }
    ImGui::PopFont();
    TOOLTIP(path.c_str());

    ImGui::PushFont(_gui->getFont("header2"));
    {
        load();
        ImGui::SameLine(70, 0);
        save();
        ImGui::SameLine(140, 0);
        saveas();
    }
    ImGui::PopFont();



    ImGui::NewLine();
    style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
    if (ImGui::TreeNodeEx("animation", flags))
    {
        if (ImGui::Combo("pivot type", (int*)&pivotType, "off\0leaf\0full 1\0full 2\0")) { changed = true; }

        R_FLOAT("stiffness", ossilation_stiffness, 0.1f, 0.8f, 20.f, "");
        R_FLOAT("sqrt(sway)", ossilation_constant_sqrt, 0.1f, 1.01f, 100.f, "");
        ImGui::SameLine(0, 20);
        ImGui::Text("%2.3fHz", 1.f / (rootFrequency() * sqrt(leaf_length.x * 0.001f) / sqrt(leaf_width.x * 0.001f)));
        R_FLOAT("root-tip", ossilation_power, 0.01f, 0.02f, 1.8f, "");
        ImGui::TreePop();
    }

    ImGui::NewLine();
    style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
    if (ImGui::TreeNodeEx("stem", flags))
    {

        R_LENGTH("length", stem_length, "stem length in mm", "random");
        R_LENGTH("width", stem_width, "stem width in mm", "random");
        if (stem_length.x > 0 && stem_width.x > 0)
        {
            R_CURVE("curve", stem_curve, "stem curvature", "random");
            R_CURVE("angle", stem_to_leaf, "stem to leaf angle in radians", "random");
            R_VERTS("num verts", stemVerts);
            stem_Material.renderGui(gui_id);
        }

        ImGui::TreePop();
    }

    ImGui::NewLine();
    style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
    if (ImGui::TreeNodeEx("leaf", flags))
    {
        CHECKBOX("face camera", &cameraFacing, "");
        //if (ImGui::Checkbox("face camera", &cameraFacing)) changed = true;
        R_LENGTH("length", leaf_length, "mm", "random");
        R_LENGTH("width", leaf_width, "mm", "random");
        R_LENGTH("offset", width_offset, "", "random");
        R_CURVE("curve", leaf_curve, "radians", "random");
        R_CURVE("perlin C", perlinCurve, "amount", "repeats");
        R_CURVE("twist", leaf_twist, "radians", "random");
        R_CURVE("perlin T", perlinTwist, "amount", "repeats");
        R_CURVE("gavity", gravitrophy, "rotate towards the ground or sky", "random");
        R_VERTS("num verts", numVerts);
        CHECKBOX("use diamond", &useTwoVertDiamond, "use diamond pattern when the leaf uses only 2 vertices");
        R_FLOAT("length split", leafLengthSplit, 0.1f, 4.f, 100.f, "Number of pixels to insert a new node in this leaf, pusg it higher for very curved leaves \nWill still be clamped by min and max above");


        ImGui::NewLine();
        style.Colors[ImGuiCol_Header] = ImVec4(0.03f, 0.03f, 0.03f, 1.f);
        materials.renderGui("materials", gui_id);
        ImGui::TreePop();
    }

    changedForSave |= changed;
    anyChange |= changed;
    changed = false;
}


void _leafBuilder::treeView()
{
    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Header] = leaf_color;
    int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_OpenOnArrow;
    if (_rootPlant::selectedPart == this) {
        style.FrameBorderSize = 3;
        flags = flags | ImGuiTreeNodeFlags_Selected;
        //style.Colors[ImGuiCol_Header] = selected_color;
        style.Colors[ImGuiCol_Border] = ImVec4(0.7f, 0.7f, 0.7f, 1);
    }

    if (changedForSave) style.Colors[ImGuiCol_Header] = selected_color;

    TREE_LINE(name.c_str(), path.c_str(), flags)
    {
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%d instances \n%d verts", numInstancePacked, numVertsPacked);
        }

        style.FrameBorderSize = 0;
        CLICK_PART;
        ImGui::TreePop();
    }

}

#define RND_ALBEDO(data) (data * (1.f + 0.3f * (float)distrib(_rootPlant::generator)))
#define RND_B(data) (data.x * (1.f + data.y * (float)distrib(_rootPlant::generator)))
#define RND_CRV(data) (data.x + (data.y * (float)distrib(_rootPlant::generator)))

//#define CURVE(_mat,_ang)  glm::rotate(glm::mat4(1.0), _ang, (glm::vec3)_mat[0])     // pitch
//#define TWIST(_mat,_ang)  glm::rotate(glm::mat4(1.0), _ang, (glm::vec3)_mat[1])     // roll
//#define CURVE_Z(_mat,_ang)  glm::rotate(glm::mat4(1.0), _ang, (glm::vec3)_mat[2])   // yaw

#define GROW(_mat,_length)   _mat[3] += _mat[1] * _length

#define ROLL(_mat,_ang)  _mat = glm::rotate(_mat, _ang, glm::vec3(0, 1, 0))
#define PITCH(_mat,_ang)  _mat = glm::rotate(_mat, _ang, glm::vec3(1, 0, 0))
#define YAW(_mat,_ang)  _mat = glm::rotate(_mat, _ang, glm::vec3(0, 0, 1))
//#define RPY(_mat,_r,_p,_y)  TWIST(_mat,_r) * CURVE(_mat,_p) * CURVE_Z(_mat,_y)

void _leafBuilder::clear_build_info()
{
    numInstancePacked = 0;
    numVertsPacked = 0;
    numPivots = 0;
}


#pragma optimize("", off)
glm::mat4 _leafBuilder::build(buildSetting _settings, bool _addVerts)
{
    uint startVerts = ribbonVertex::ribbons.size();

    std::uniform_real_distribution<> distrib(-1.f, 1.f);
    std::uniform_real_distribution<> d50(0.5f, 1.5f);
    std::uniform_int_distribution<> distAlbedo(-50, 50);
    std::uniform_int_distribution<> distPerlin(1, 50000);

    const siv::PerlinNoise::seed_type seed = distPerlin(_rootPlant::generator);
    const siv::PerlinNoise perlin{ seed };
    const siv::PerlinNoise::seed_type seedT = distPerlin(_rootPlant::generator);
    const siv::PerlinNoise perlinTWST{ seedT };

    glm::mat4 node = _settings.root;
    ribbonVertex R_verts;
    float age = pow(_settings.normalized_age, 1.f);
    //age = saturate(_settings.age);
    bool stemVisible = false;


    // freq and stiffness needs to app,y to both stem and leaf
    float lengthS = RND_B(leaf_length) * 0.001f * age;   // to meters and numSegments  // FIXME scale to neter built into macro, rename macro for distamce only
    float widthS = RND_B(leaf_width) * 0.001f * age;
    float freq = rootFrequency() * sqrt(lengthS) / sqrt(widthS);
    float stiffness = 1.f / ossilation_stiffness;

    // stem
    if (stem_length.x > 0)
    {
        float length = RND_B(stem_length) / 100.f * 0.001f * age;   // to meters and numSegments
        float width = stem_width.x * 0.001f * age;
        float curve = RND_CRV(stem_curve) / 100.f * age;

        // Lodding stem, but use length instead............................................................
        bool showStem = width > _settings.pixelSize;
        int numStem = glm::clamp((int)((length / _settings.pixelSize) / 8.f * 100.f), 1, stemVerts.y);     // 1 for every 8 pixels, clampped
        float step = 99.f / (numStem);
        float cnt = 0.f;

        if (_addVerts && showStem)
        {
            R_verts.startRibbon(true, _settings.pivotIndex);
            R_verts.set(node, width * 0.5f, stem_Material.index, float2(1.f, 0.f), 1.f, 1.f, !(pivotType == pivot_leaf), stiffness, freq);
            stemVisible = true;
        }

        for (int i = 0; i < 100; i++)
        {
            PITCH(node, curve);
            GROW(node, length);

            cnt++;
            if (_addVerts && showStem && cnt >= step)
            {
                R_verts.set(node, width * 0.5f, stem_Material.index, float2(1.f, (float)i / 99.f), 1.f, 1.f, !(pivotType == pivot_leaf), stiffness, freq);
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

        // some of this detail needs to go into the left data and user set
        //float freq = rootFrequency() * sqrt(length * 100) / sqrt(width * 100);
        //float stiffness = 1.f / ossilation_stiffness;

        // Lodding leaf............................................................
        // random width to test
        bool showLeaf = (width * d50(_rootPlant::generator)) > _settings.pixelSize;
        int numLeaf = glm::clamp((int)((length / _settings.pixelSize) / leafLengthSplit * 100.f), numVerts.x - 1, numVerts.y - 1);     // 1 for every 8 pixels, clampped
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
        //showLeaf = lastVis > firstVis;
        bool useDiamond = (useTwoVertDiamond && (numLeaf == 1));
        if (useDiamond)
        {
            firstVis = 0;
            lastVis = 99;
            step = 99;
        }



        for (int i = 0; i < 100; i++)
        {
            float t = (float)i / 99.f;
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


            if (_addVerts && i == firstVis && showLeaf)
            {
                int oldRoot = R_verts.S_root;
                R_verts.startRibbon(cameraFacing || forceFacing, _settings.pivotIndex);
                if (stemVisible)
                {
                    R_verts.S_root = oldRoot;   // uglu but means that the two ribbons share the one root
                }

                R_verts.set(node, width * 0.5f * du, mat.index, float2(du, 1.f - t), albedoScale, translucentScale, !(pivotType == pivot_leaf), stiffness, freq, pow((float)i / 99.f, ossilation_power), useDiamond);
                cnt = 0;
            }
            else if (i > firstVis)
            {
                cnt++;
                if (_addVerts && (i <= lastVis) && showLeaf && cnt >= step)
                {
                    R_verts.set(node, width * 0.5f * du, mat.index, float2(du, 1.f - t), albedoScale, translucentScale, !(pivotType == pivot_leaf), stiffness, freq, pow((float)i / 99.f, ossilation_power), useDiamond);
                    cnt -= step;
                }
            }
        }

    }

    uint numVerts = ribbonVertex::ribbons.size() - startVerts;
    if (numVerts > 0) numInstancePacked++;
    numVertsPacked += numVerts;

    changedForSave |= changed;

    return node;
}
#pragma optimize("", on)






// _stemBuilder
// -----------------------------------------------------------------------------------------------------------------------------------

void _stemBuilder::loadPath()
{
    std::ifstream is(terrafectorEditorMaterial::rootFolder + path);
    cereal::JSONInputArchive archive(is);
    archive(*this);
    changed = false;
}

void _stemBuilder::savePath()
{
    std::ofstream os(terrafectorEditorMaterial::rootFolder + path);
    cereal::JSONOutputArchive archive(os);
    archive(*this);
    changed = false;
}

void _stemBuilder::load()
{
    if (ImGui::Button("Load##_stemBuilder", ImVec2(80, 0)))
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


void _stemBuilder::save()
{
    if (changedForSave) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.0f, 0.8f));
        if (ImGui::Button("Save##_stemBuilder", ImVec2(80, 0)))
        {
            savePath();
            changedForSave = false;
        }
        ImGui::PopStyleColor();

    }
}


void _stemBuilder::saveas()
{
    if (ImGui::Button("Save as##_stemBuilder", ImVec2(80, 0)))
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

void _stemBuilder::renderGui()
{
    auto& style = ImGui::GetStyle();
    int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_OpenOnArrow;

    ImGui::PushFont(_gui->getFont("header1"));
    {
        ImGui::Text(name.c_str());
    }
    ImGui::PopFont();
    TOOLTIP(path.c_str());

    ImGui::PushFont(_gui->getFont("header2"));
    {
        load();
        ImGui::SameLine(90, 0);
        save();
        ImGui::SameLine(180, 0);
        saveas();
    }
    ImGui::PopFont();


    unsigned int gui_id = 1001;

    ImGui::NewLine();
    style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
    if (ImGui::TreeNodeEx("light", flags))
    {

        ImGui::PushFont(_gui->getFont("italic"));
        {
            R_FLOAT("uv scale", shadowUVScale, 0.001f, 0.001f, 10.f, "");
            R_FLOAT("softness", shadowSoftness, 0.01f, 0.01f, 0.3f, "");
            R_FLOAT("depth", shadowDepth, 0.01f, 0.01f, 10.f, "");
            R_FLOAT("hgt", shadowPenetationHeight, 0.01f, 0.05f, 0.95f, "");
        }
        ImGui::PopFont();

        ImGui::TreePop();
    }


    ImGui::NewLine();
    style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
    if (ImGui::TreeNodeEx("animation", flags))
    {
        ImGui::SameLine(0, 50);
        float l = 1;
        if (NODES.size() >= 2)
        {
            l = glm::length(NODES.back()[3] - NODES.front()[3]);
        }
        if (l == 0) l = 1; // avoid devide by zero
        ImGui::Text("%2.3fHz", 1.f / (rootFrequency() / sqrt(l)));

        R_FLOAT("stiffness", ossilation_stiffness, 0.1f, 0.8f, 20.f, "");
        R_FLOAT("sqrt(sway)", ossilation_constant_sqrt, 0.1f, 1.01f, 100.f, "");
        R_FLOAT("root-tip", ossilation_power, 0.01f, 0.02f, 1.8f, "");

        ImGui::TreePop();
    }



    ImGui::NewLine();
    style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
    if (ImGui::TreeNodeEx("stem / trunk", flags))
    {

        ImGui::SameLine(0, 10);
        ImGui::Text("%2.1f - %2.1f)mm, [%d, %d]", root_width * 1000.f, tip_width * 1000.f, (int)age, firstLiveSegment);

        R_LENGTH("age", numSegments, "total number of nodes", "random is used for automatic variations");
        if (numSegments.x < 0.5f) numSegments.x = 0.5f;
        R_LENGTH("living", max_live_segments, "how many of these support growth, can be bigger than totalm then all except very first one do", "random is used for automatic variations");

        R_LENGTH("length", stem_length, "stem length in mm", "random");
        R_LENGTH_EX("width", stem_width, 0.002f, 0.5f, 20.f, "stem width in mm", "random");
        R_LENGTH_EX("growth", stem_d_width, 0.001f, 0.1f, 20.f, "amount to grow for each node", "random");
        R_LENGTH_EX("pow", stem_pow_width, 0.01f, 0.1f, 10.f, "scale growth towards tip or root", "random");
        R_CURVE("curve", stem_curve, "curve in a single segment", "random");
        R_CURVE("phototropism", stem_phototropism, "", "random");
        R_CURVE("node twist", node_rotation, "twist of the stalk", "random");
        R_CURVE("node bend", node_angle, "stem bend at a node", "random");
        //ImGui::GetStyle().Colors[ImGuiCol_Header] = mat_color;
        style.Colors[ImGuiCol_Header] = ImVec4(0.03f, 0.03f, 0.03f, 1.f);
        stem_Material.renderGui(gui_id);
        //style.FrameBorderSize = 1;
        //if (ImGui::Checkbox("bake-2 replaces stem", &bake2_replaces_stem)) changed = true;
        //style.FrameBorderSize = 0;
        //CHECKBOX("bake-2 replaces stem", &bake2_replaces_stem, "Stem will only be used in baking but replaced by bake-2 in final detail");
        R_FLOAT("length split", nodeLengthSplit, 0.1f, 4.f, 100.f, "Number of pixels to insert a new node in this leaf, pusg it higher for very curved leaves");

        ImGui::TreePop();
    }


    // leaves
    ImGui::NewLine();
    style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
    if (ImGui::TreeNodeEx("branches / leaves", flags))
    {

        ImGui::SameLine(0, 10);
        ImGui::Text("%d added", numLeavesBuilt);

        R_LENGTH("numLeaves", numLeaves, "stem length in mm", "random");
        numLeaves.x = __max(0.5f, numLeaves.x);
        R_CURVE("angle", leaf_angle, "angle of teh stalk t the ", "change of angle as it ages, usually drooping");
        R_CURVE("random", leaf_rnd, "randomness in the angles", "random");
        R_FLOAT("age_power", leaf_age_power, 0.1f, 1.f, 25.f, "");
        //style.FrameBorderSize = 1;
        //if (ImGui::Checkbox("twistAway", &twistAway)) changed = true; TOOLTIP("does trh stalk try t0 twis awat from a single leaf");
        //style.FrameBorderSize = 0;
        CHECKBOX("twistAway", &twistAway, "does the stalk try to twis away from a single leaf");
        //ImGui::GetStyle().Colors[ImGuiCol_Header] = leaf_color;
        style.Colors[ImGuiCol_Header] = ImVec4(0.03f, 0.03f, 0.03f, 1.f);
        leaves.renderGui("leaves", gui_id);

        ImGui::TreePop();
    }

    // tip
    ImGui::NewLine();
    style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
    if (ImGui::TreeNodeEx("tip", flags))
    {
        CHECKBOX("unique tip", &unique_tip, "load a different part for the tip \nif off it will reuse one of the breanch / leaf parts");
        //style.FrameBorderSize = 1;
        //if (ImGui::Checkbox("unique_tip", &unique_tip)) changed = true;
        //style.FrameBorderSize = 0;

        if (unique_tip)
        {
            R_LENGTH("tip age", tip_age, "age of the tip\n-1 means that the tip plant will decide", "random");

            //ImGui::GetStyle().Colors[ImGuiCol_Header] = leaf_color;
            style.Colors[ImGuiCol_Header] = ImVec4(0.03f, 0.03f, 0.03f, 1.f);
            tip.renderGui("tip", gui_id);
        }

        ImGui::TreePop();
    }

    changedForSave |= changed;
    anyChange |= changed;
    changed = false;
}

void _stemBuilder::treeView()
{
    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Header] = stem_color;
    int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_OpenOnArrow;
    if (_rootPlant::selectedPart == this) {
        style.FrameBorderSize = 3;
        flags = flags | ImGuiTreeNodeFlags_Selected;
        //style.Colors[ImGuiCol_Header] = selected_color;
        style.Colors[ImGuiCol_Border] = ImVec4(0.7f, 0.7f, 0.7f, 1);
    }


    if (changedForSave) style.Colors[ImGuiCol_Header] = selected_color;

    TREE_LINE(name.c_str(), path.c_str(), flags)
    {
        if (ImGui::IsItemHovered()) {
            if (ribbonVertex::pivotPoints.size() > 256)     ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.f));
            ImGui::SetTooltip("%d instances \n%d verts \n%d / %d pivots ", numInstancePacked, numVertsPacked, numPivots, ribbonVertex::pivotPoints.size());
            if (ribbonVertex::pivotPoints.size() > 256)     ImGui::PopStyleColor();
        }



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
}


lodBake* _stemBuilder::getBakeInfo(uint i)
{
    if (i < lod_bakeInfo.size()) return &lod_bakeInfo[i];

    return nullptr;
}

levelOfDetail* _stemBuilder::getLodInfo(uint i)
{
    if (i < lodInfo.size()) return &lodInfo[i];
    return nullptr;
}


#pragma optimize("", off)
glm::mat4  _stemBuilder::build_2(buildSetting _settings, uint _bakeIndex, bool _faceCamera)
{
    lodBake& lB = lod_bakeInfo[_bakeIndex];
    glm::mat4 node = NODES.front();
    glm::mat4 last = NODES.back();
    if (lB.pixHeight > 0)
    {
        float w = lB.extents.x * lB.bakeWidth;
        uint mat = lB.material.index;


        //float tipLength = glm::dot(tip_NODE[3] - last[3], last[1]);
        //float tipLength = glm::length(tip_NODE[3] - last[3]);
        float tipLength = glm::dot(tip_NODE[3] - last[3], last[1]);
        GROW(last, tipLength);

        // adjust start end
        float totalLenght = glm::length(last[3] - node[3]);
        GROW(node, totalLenght * lB.bake_V.x);
        GROW(last, -totalLenght * (1 - lB.bake_V.y));

        ribbonVertex R_verts;
        _faceCamera |= lB.faceCamera;
        R_verts.startRibbon(_faceCamera, _settings.pivotIndex);
        R_verts.set(node, w * 0.99f, mat, float2(0.99f, 0.f), 1.f, 1.f, true, 0.5f, 0.1f, 0.0f, true);  // only 60% width, save pixels but test
        R_verts.set(last, w * 0.99f, mat, float2(0.99f, 1.f), 1.f, 1.f, true, 0.5f, 0.1f, 0.0f, true);
    }
    return last;
}


glm::mat4  _stemBuilder::build_4(buildSetting _settings, uint _bakeIndex, bool _faceCamera)
{
    lodBake& lB = lod_bakeInfo[_bakeIndex];
    float w = lB.extents.x * lB.bakeWidth;
    uint mat = lB.material.index;

    glm::mat4 node = NODES.front();
    glm::mat4 last = NODES.back();
    //float tipLength = glm::length(tip_NODE[3] - last[3]);
    float tipLength = glm::dot(tip_NODE[3] - last[3], last[1]);
    GROW(last, tipLength);

    glm::vec4 step = (last[3] - node[3]);
    float4 binorm_step = (last[1] - node[1]);

    node[3] += step * lB.bake_V.x;
    node[1] += binorm_step * lB.bake_V.x;
    step *= (lB.bake_V.y - lB.bake_V.x);
    binorm_step *= (lB.bake_V.y - lB.bake_V.x);
    step /= 3.f;
    binorm_step /= 3.f;

    ribbonVertex R_verts;
    _faceCamera |= lB.faceCamera;
    R_verts.startRibbon(_faceCamera, _settings.pivotIndex);

    std::vector<glm::mat4> all = NODES;
    all.push_back(tip_NODE);

    for (int i = 0; i < 4; i++)
    {
        glm::mat4 CURRENT = node;

        if (all.size() > 4)
        {
            int currentIdx = 0;
            for (int iN = 0; iN < NODES.size(); iN++)   // dont test tip so use NODES.size
            {
                float3 rel = all[iN][3] - node[3];
                float d = glm::dot(rel, (float3)node[1]);
                if (d >= 0)
                {
                    d = fabs(d);
                    float3 rel2 = all[iN + 1][3] - node[3];
                    float d2 = fabs(glm::dot(rel2, (float3)node[1]));
                    float total = d + d2;

                    CURRENT[3] = glm::lerp(all[iN][3], all[iN + 1][3], d / total);
                    break;
                }
            }
        }

        R_verts.set(CURRENT, w * lod_bakeInfo[_bakeIndex].dU[i] * 0.99f, mat, float2(lod_bakeInfo[_bakeIndex].dU[i] * 0.99f, 1.f - (0.3333333f * i)), 1.f, 1.f);
        node[3] += step;
        node[1] += binorm_step;
    }

    //R_verts.set(last, w * lod_bakeInfo[_bakeIndex].dU[3] * 0.6f, mat, float2(lod_bakeInfo[_bakeIndex].dU[3] * 0.6f, 1.f), 1.f, 1.f);

    return last;
}


glm::mat4  _stemBuilder::build_n(buildSetting _settings, uint _bakeIndex, bool _faceCamera)
{
    return NODES.front();
}


glm::mat4 _stemBuilder::build_lod_0(buildSetting _settings)
{
    if (NODES.size() < 2) return NODES.front();

    float numPixX = lod_bakeInfo[0].extents.x / _settings.pixelSize;
    float L = glm::length(tip_NODE[3] - NODES.front()[3]);
    float numPixY = L / _settings.pixelSize;
    // if (numPixX > 0.5f)
    {
        if (numPixY < 4)
        {
            return NODES.back();
        }
        if (numPixY < 8 || lod_bakeInfo[0].forceDiamond)
        {
            return build_2(_settings, 0, true);
        }
        else
        {
            return build_4(_settings, 0, true);
        }
    }
    return NODES.back();
}


glm::mat4   _stemBuilder::build_lod_1(buildSetting _settings, bool _buildLeaves)
{
    if (NODES.size() < 2) return NODES.front();

    float numPixX = lod_bakeInfo[1].extents.x / _settings.pixelSize;
    float L = glm::length(tip_NODE[3] - NODES.front()[3]);
    float numPixY = L / _settings.pixelSize;
    //if (numPixX > 0.5f)
    {
        if (numPixY < 4)
        {
            return NODES.back();
        }
        if (numPixY < 8 || lod_bakeInfo[1].forceDiamond)
        {
            return build_2(_settings, 1, true);
        }
        else
        {
            return build_4(_settings, 1, true);
        }
    }
    return NODES.back();
}


glm::mat4   _stemBuilder::build_lod_2(buildSetting _settings, bool _buildLeaves)
{
    if (NODES.size() < 2) return NODES.front();
    return build_4(_settings, 2, true);
}
#pragma optimize("", on)

void _stemBuilder::build_tip(buildSetting _settings, bool _addVerts)
{
    _settings.forcePhototropy = false;
    // reset teh seed so all lods build teh same here
    _rootPlant::generator.seed(_settings.seed + 1999);
    //if ()
    {
        // walk the tip back ever so slightly
        glm::mat4 node = NODES.back();
        GROW(node, -tip_width * 0.5f);  //???
        _settings.root = node;
        _settings.node_age = 1.f;
        _settings.normalized_age = 1;
        // add new force tip age, and set that here instead

        if (unique_tip && tip.get().plantPtr)
        {
            std::uniform_real_distribution<> distrib(-1.f, 1.f);
            _settings.node_age = RND_B(tip_age);
            tip_NODE = tip.get().plantPtr->build(_settings, _addVerts);
        }
        else
        {
            _plantRND LEAF = leaves.get();
            if (LEAF.plantPtr) tip_NODE = LEAF.plantPtr->build(_settings, _addVerts);
        }
    }
}


void _stemBuilder::build_leaves(buildSetting _settings, uint _max, bool _addVerts)
{
    _settings.forcePhototropy = false;
    numLeavesBuilt = 0;
    std::uniform_real_distribution<> distrib(-1.f, 1.f);
    // reset teh seed so all lods build teh same here
    //_rootPlant::generator.seed(_settings.seed + 999);


    int oldSeed = _settings.seed;
    std::mt19937 MT(_settings.seed + 999);
    std::uniform_real_distribution<> DDD(-1.f, 1.f);

    // side nodes
    uint end = NODES.size() - 1; // - WE NEVER INCLUDE THE TIP HERE, always just add that in build_tip()


    for (int i = 0; i < end; i++)
    {
        if (i >= firstLiveSegment)
        {
            float leafAge = 1.f - pow(i / age, leaf_age_power);
            int numL = (int)RND_B(numLeaves);

            float rndRoll = 6.28f * DDD(MT);

            float t = (float)i / age;
            float t_live = glm::clamp((i - firstLiveSegment) / (age - firstLiveSegment), 0.f, 1.f);
            t_live = 1.f - pow(t_live, leaf_age_power);
            float W = root_width - (root_width - tip_width) * pow(t, stem_pow_width.x);

            for (int j = 0; j < numL; j++)
            {
                numLeavesBuilt++;
                glm::mat4 node = NODES[i];
                float A = leaf_angle.x + leaf_angle.y * leafAge + DDD(MT) * RND_B(leaf_rnd);
                float nodeTwist = rndRoll + 6.283185307f / (float)numL * (float)j;

                ROLL(node, nodeTwist);
                PITCH(node, -A);
                GROW(node, W * 0.35f);   // 70% out, has to make up for alpha etcDman I want to grow here to some % of branch width

                _settings.seed = oldSeed + i;
                _settings.root = node;
                _settings.node_age = age - i + 1;
                _settings.normalized_age = t_live;
                _plantRND LEAF = leaves.get();
                if (LEAF.plantPtr) LEAF.plantPtr->build(_settings, _addVerts);
            }
        }
    }
}

#pragma optimize("", off)
void _stemBuilder::build_NODES(buildSetting _settings, bool _addVerts)
{
    //std::mt19937 gen(_settings.seed);
    //std::mt19937 MT();
    _rootPlant::generator.seed(_settings.seed + 33);
    std::uniform_real_distribution<> distrib(-1.f, 1.f);
    std::uniform_int_distribution<> distAlbedo(-50, 50);
    glm::mat4 node = _settings.root;
    ribbonVertex R_verts;

    NODES.clear();
    NODES.push_back(node);

    R_verts.startRibbon(true, _settings.pivotIndex);
    age = RND_B(numSegments);
    if (_settings.node_age > 0.f)      // if passed in from root use that
    {
        age = _settings.node_age;
    }
    int iAge = __max(1, (int)age);



    tip_width = RND_B(stem_width) * 0.001f;                  // tip radius
    root_width = tip_width + RND_B(stem_d_width) * 0.001f * age;    // root radius  //?? iAge
    float rootPow = stem_pow_width.x;
    float dR = (root_width - tip_width);

    int numLiveNodes = (int)RND_B(max_live_segments);
    firstLiveSegment = __max(1, iAge - numLiveNodes);

    std::uniform_real_distribution<> d50(0.5f, 1.5f);
    float pixRandFoViz = _settings.pixelSize * d50(_rootPlant::generator);

    bool visible = root_width > pixRandFoViz;
    if (_addVerts && visible) {
        R_verts.set(node, root_width * 0.5f, stem_Material.index, float2(1.f, 0.f), 1.f, 1.f);   // set very first one
    }

    for (int i = 0; i < iAge; i++)
    {
        float nodeAge = glm::clamp((age - i) / numLiveNodes, 0.f, 1.f);
        float L = RND_B(stem_length) * 0.001f / 100.f;
        float C = RND_CRV(stem_curve) / 100.f;
        float P = RND_CRV(stem_phototropism) / 100.f;
        if (_settings.forcePhototropy) {
            P = 5.0f / 100;
        }

        float t = (float)i / age;
        float W = root_width - dR * pow(t, rootPow);
        float nodePixels = (L * 100) / _settings.pixelSize;
        int nodeNumSegments = glm::clamp((int)(nodePixels / nodeLengthSplit), 1, 20);     // 1 for every 8 pixels, clampped
        float segStep = 99.f / (float)nodeNumSegments;

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

            float t = (float)i / age + ((float)j / 100.f * (1.f / age));
            float W = root_width - dR * pow(t, rootPow);
            visible = W > pixRandFoViz;
            if (_addVerts && visible && cnt >= segStep)
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
}
#pragma optimize("", on)

void _stemBuilder::clear_build_info()
{
    numInstancePacked = 0;
    numVertsPacked = 0;
    numPivots = 0;

    for (auto& L : leaves.data)
    {
        if (L.plantPtr)  L.plantPtr->clear_build_info();
    }

    for (auto& L : tip.data)
    {
        if (L.plantPtr)  L.plantPtr->clear_build_info();
    }
}

glm::mat4 _stemBuilder::build(buildSetting _settings, bool _addVerts)
{
    uint startVerts = ribbonVertex::ribbons.size();
    uint leafVerts = ribbonVertex::ribbons.size();
    uint tipVerts = ribbonVertex::ribbons.size();

    build_NODES(_settings, false);
    tip_NODE = NODES.back();    // just in case build tip adds nothing
    build_tip(_settings, false);

    if (_addVerts)
    {
        std::uniform_int_distribution<> DDD(0, 255);
        ribbonVertex R;
        float3 extent = (float3)tip_NODE[3] - (float3)NODES.front()[3];
        float ext_L = glm::length(extent);
        extent = glm::normalize(extent) / ext_L;
        _settings.pivotIndex[_settings.pivotDepth] = (int)ribbonVertex::pivotPoints.size();
        _settings.pivotDepth += 1;
        R.pushPivot((float3)NODES.front()[3], extent, rootFrequency() * sqrt(ext_L), ossilation_stiffness, ossilation_power, DDD(_rootPlant::generator));

        // FIXME, Shoul dnot happen on bluid-2 and build4 - They should set leaf pivots if at all

        numPivots++;
    }

    if (_addVerts)
    {
        levelOfDetail lod = lodInfo.back();             // no parent can push us past this, similar when I do sun Parts, there will be a lod set for that add
        for (int i = lodInfo.size() - 1; i >= 0; i--)
        {
            if (_settings.pixelSize >= lodInfo[i].pixelSize)
            {
                lod = lodInfo[i];
            }
        }

        if (lod.bakeType == BAKE_DIAMOND)       build_2(_settings, lod.bakeIndex, true);
        if (lod.bakeType == BAKE_4)             build_4(_settings, lod.bakeIndex, true);
        if (lod.bakeType == BAKE_N)             build_n(_settings, lod.bakeIndex, true);    // FIXME

        if (lod.useGeometry || _settings.forcePhototropy)
        {
            _settings.pixelSize *= lod.geometryPixelScale;

            build_tip(_settings, true);
            tipVerts = ribbonVertex::ribbons.size();

            build_NODES(_settings, true);

            leafVerts = ribbonVertex::ribbons.size();
            build_leaves(_settings, 100000, true);
        }

        if ((ribbonVertex::ribbons.size() - startVerts) > 0) numInstancePacked++;
        numVertsPacked += leafVerts - tipVerts;
    }
    /*
    if (_addVerts)
    {
        // it is possible if I forgot to save that this is zero and a first full build should fix it
        if (lodInfo[2].pixelSize > 0)
        {
            // clamp to out last lod in this tree, dont let parents oveeride it
            if (_settings.node_age != -1)
            {
                _settings.pixelSize = __max(_settings.pixelSize, lodInfo.back().pixelSize);
            }

            if (_settings.pixelSize >= lodInfo[0].pixelSize ) {
                //_settings.pixelSize = lodInfo[0].pixelSize;

                glm::mat4 R = tip_NODE;
                if (lod_bakeInfo[0].pixHeight > 0)
                {
                    R = build_lod_0(_settings);
                    uint numVerts = ribbonVertex::ribbons.size() - startVerts;
                    if (numVerts > 0) numInstancePacked++;
                    numVertsPacked += numVerts;
                }
                return R;
            }

            else if (_settings.pixelSize >= lodInfo[1].pixelSize) {
                //_settings.pixelSize = lodInfo[1].pixelSize;
                if (lod_bakeInfo[2].pixHeight == 0 && (lod_bakeInfo[1].pixHeight > 0))
                {
                    glm::mat4 R = build_lod_1(_settings, false);

                    uint numVerts = ribbonVertex::ribbons.size() - startVerts;
                    if (numVerts > 0) numInstancePacked++;
                    numVertsPacked += numVerts;

                    return R;
                }
                else
                {
                    glm::mat4 R = tip_NODE;
                    if (lod_bakeInfo[1].pixHeight > 0)
                    {
                        R = build_lod_1(_settings, true);
                    }
                    return R;
                }
            }


            else if (_settings.pixelSize >= lodInfo[2].pixelSize)
            {
                //_settings.pixelSize = lodInfo[2].pixelSize;
                if (lod_bakeInfo[2].pixHeight == 0 && (lod_bakeInfo[1].pixHeight > 0))  // This ine is just billboard
                {
                    _settings.pixelSize = lodInfo[1].pixelSize;
                    glm::mat4 R = build_lod_1(_settings, false);

                    uint numVerts = ribbonVertex::ribbons.size() - startVerts;
                    if (numVerts > 0) numInstancePacked++;
                    numVertsPacked += numVerts;

                    return R;
                }   // if lod3 doesnt bake at all, this is a billboard onlt
                else
                {
                    build_lod_2(_settings, true);
                }
            }
        }
    }

    if (_addVerts)
    {

        build_tip(_settings, true);
        tipVerts = ribbonVertex::ribbons.size();

        bool Viz = !bake2_replaces_stem || _settings.forcePhototropy;
        if (!Viz)
        {
            build_lod_2(_settings, true);
        }
        else
        {
            build_NODES(_settings, true);
        }

        leafVerts = ribbonVertex::ribbons.size();
        build_leaves(_settings, 100000, true);
    }

    uint numVerts = ribbonVertex::ribbons.size() - startVerts;
    if (numVerts > 0) numInstancePacked++;
    uint numUnique = leafVerts - tipVerts;    // the rest is in choildren
    numVertsPacked += numUnique;

 //   changedForSave |= changed;
 //   changed = false;
    */

    return tip_NODE;
}


void _stemBuilder::calculate_extents(buildSetting _settings)
{
    float2 extents = float2(0, 0);
    for (auto& R : ribbonVertex::ribbons)
    {
        float2 XZ = R.position.xz;
        float r = glm::length(XZ);
        //extents.x = __max(extents.x, r);
        //extents.y = __max(extents.y, abs(R.position.y));    // addign radius breaks down when we start to use lod-0 for sub elements
        extents.x = __max(extents.x, r + R.radius);
        extents.y = __max(extents.y, abs(R.position.y));    // + R.radius
    }

    // du
    float du6[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int cnt[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    float step = extents.y / 9.f;
    for (auto& R : ribbonVertex::ribbons)
    {
        float2 XZ = R.position.xz;
        float r = glm::length(XZ) + R.radius;
        float y = __max(0, R.position.y);   // only positive
        uint bucket = (uint)glm::clamp(y / step, 0.f, 8.f);
        du6[bucket] = __max(r, du6[bucket]);
        cnt[bucket]++;
    }
    std::array<float, 4> du;
    du[0] = du6[0];
    du[1] = __max(du6[2], du6[3]);
    du[2] = __max(du6[5], du6[6]);
    du[3] = du6[8];

    du[0] /= extents.x;
    du[1] /= extents.x;
    du[2] /= extents.x;
    du[3] /= extents.x;

    du[0] += 0.05f;
    du[1] += 0.05f;
    du[2] += 0.05f;
    du[3] += 0.05f;

    du[0] = __min(1.f, du[0]);
    du[1] = __min(1.f, du[1]);
    du[2] = __min(1.f, du[2]);
    du[3] = __min(1.f, du[3]);

    std::filesystem::path full_path = path;

    // lod 0
    float dw = lod_bakeInfo[0].bakeWidth;
    lod_bakeInfo[0].extents = extents;
    lod_bakeInfo[0].dU[0] = __min(1.f, du[0] / dw);
    lod_bakeInfo[0].dU[1] = __min(1.f, du[1] / dw);
    lod_bakeInfo[0].dU[2] = __min(1.f, du[2] / dw);
    lod_bakeInfo[0].dU[3] = __min(1.f, du[3] / dw);
    //lod_bakeInfo[0].material.name = full_path.stem().string() + "_billboard_lod0";
    //lod_bakeInfo[0].material.path = full_path.parent_path().string() + "\\" + lod_bakeInfo[0].material.name + ".vegMaterial";

    lod_bakeInfo[0].material.name = "bake_0";
    lod_bakeInfo[0].material.path = full_path.parent_path().string() + "\\bake_" + full_path.stem().string() + "\\" + lod_bakeInfo[0].material.name + ".vegetationMaterial";;

    // lod 1
    lod_bakeInfo[1].extents = extents;
    dw = lod_bakeInfo[1].bakeWidth;
    lod_bakeInfo[1].dU[0] = __min(1.f, du[0] / dw);
    lod_bakeInfo[1].dU[1] = __min(1.f, du[1] / dw);
    lod_bakeInfo[1].dU[2] = __min(1.f, du[2] / dw);
    lod_bakeInfo[1].dU[3] = __min(1.f, du[3] / dw);
    //lod_bakeInfo[1].material.name = full_path.stem().string() + "_billboard_lod1";
    //lod_bakeInfo[1].material.path = full_path.parent_path().string() + "\\" + lod_bakeInfo[1].material.name + ".vegMaterial";
    lod_bakeInfo[1].material.name = "bake_1";
    lod_bakeInfo[1].material.path = full_path.parent_path().string() + "\\bake_" + full_path.stem().string() + "\\" + lod_bakeInfo[1].material.name + ".vegetationMaterial";;

    // lod 2
    //extents.y = NODES.back()[3].y;   // last lod use NDOE end
    //No, stop doing that bake it all
    lod_bakeInfo[2].extents = extents;
    float dw2 = lod_bakeInfo[2].bakeWidth;
    lod_bakeInfo[2].dU[0] = __min(1.f, du[0] / dw2);
    lod_bakeInfo[2].dU[1] = __min(1.f, du[1] / dw2);
    lod_bakeInfo[2].dU[2] = __min(1.f, du[2] / dw2);
    lod_bakeInfo[2].dU[3] = __min(1.f, du[3] / dw2);
    //lod_bakeInfo[2].material.name = full_path.stem().string() + "_billboard_lod2";
    //lod_bakeInfo[2].material.path = full_path.parent_path().string() + "\\" + lod_bakeInfo[2].material.name + ".vegMaterial";
    lod_bakeInfo[2].material.name = "bake_2";
    lod_bakeInfo[2].material.path = full_path.parent_path().string() + "\\bake_" + full_path.stem().string() + "\\" + lod_bakeInfo[2].material.name + ".vegetationMaterial";;

}








// _clumpBuilder
// -----------------------------------------------------------------------------------------------------------------------------------

void _clumpBuilder::loadPath()
{
    std::ifstream is(terrafectorEditorMaterial::rootFolder + path);
    cereal::JSONInputArchive archive(is);
    archive(*this);
    changed = false;
}

void _clumpBuilder::savePath()
{
    std::ofstream os(terrafectorEditorMaterial::rootFolder + path);
    cereal::JSONOutputArchive archive(os);
    archive(*this);
    changed = false;
}

void _clumpBuilder::load()
{
    if (ImGui::Button("Load##_clumpBuilder", ImVec2(80, 0)))
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


void _clumpBuilder::save()
{
    if (changedForSave) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.0f, 0.8f));
        if (ImGui::Button("Save##_clumpBuilder", ImVec2(80, 0)))
        {
            savePath();
            changedForSave = false;
        }
        ImGui::PopStyleColor();
    }
}


void _clumpBuilder::saveas()
{
    if (ImGui::Button("Save as##_clumpBuilder", ImVec2(80, 0)))
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


void _clumpBuilder::renderGui()
{
    unsigned int gui_id = 1001;

    ImGui::PushFont(_gui->getFont("header1"));
    {
        ImGui::Text(name.c_str());
    }
    ImGui::PopFont();
    TOOLTIP(path.c_str());

    ImGui::PushFont(_gui->getFont("header2"));
    {
        load();
        ImGui::SameLine(90, 0);
        save();
        ImGui::SameLine(180, 0);
        saveas();
    }
    ImGui::PopFont();


    ImGui::NewLine();
    ImGui::PushFont(_gui->getFont("bold"));
    {
        ImGui::Text("light");
    }
    ImGui::PopFont();

    ImGui::PushFont(_gui->getFont("italic"));
    {
        R_FLOAT("uv scale", shadowUVScale, 0.001f, 0.001f, 10.f, "");
        R_FLOAT("softness", shadowSoftness, 0.01f, 0.01f, 0.3f, "");
        R_FLOAT("depth", shadowDepth, 0.01f, 0.01f, 10.f, "");
        R_FLOAT("hgt", shadowPenetationHeight, 0.01f, 0.05f, 0.95f, "");
    }
    ImGui::PopFont();

    ImGui::NewLine();
    ImGui::PushFont(_gui->getFont("bold"));
    {
        ImGui::Text("sway");
    }
    ImGui::PopFont();

    ImGui::SameLine(0, 20);
    ImGui::NewLine();
    /*
    float l = 1;
    if (NODES.size() >= 2)
    {
        l = glm::length(NODES.back()[3] - NODES.front()[3]);
    }
    if (l == 0) l = 1; // avoid devide by zero
    ImGui::Text("%2.3fHz", 1.f / (rootFrequency() / sqrt(l)));
    */

    R_FLOAT("stiffness", ossilation_stiffness, 0.1f, 0.8f, 20.f, "");
    R_FLOAT("sqrt(sway)", ossilation_constant_sqrt, 0.1f, 1.01f, 100.f, "");
    R_FLOAT("root-tip", ossilation_power, 0.01f, 0.02f, 1.8f, "");

    ImGui::NewLine();
    ImGui::PushFont(_gui->getFont("bold"));
    {
        ImGui::Text("root");
    }
    ImGui::PopFont();
    {
        ImGui::SameLine(0, 10);
        float a = sqrt(aspect.x);
        ImGui::Text("%2.3f, %2.3fm", size.x * a, size.x / a);

        R_LENGTH_EX("size", size, 0.001f, 0.0001f, 1.f, "circle radius in meters", "random");
        R_LENGTH_EX("aspect", aspect, 0.01f, 0.01f, 10.f, "xz aspect ratio", "random");
    }

    // leaves
    ImGui::NewLine();
    ImGui::PushFont(_gui->getFont("bold"));
    {
        ImGui::Text("children");
    }
    ImGui::PopFont();
    {
        ImGui::SameLine(0, 10);
        ImGui::Text("%d added", ROOTS.size());

        if (ImGui::Checkbox("radial", &radial)) changed = true;
        TOOLTIP("radial will rotate elements to the sides, and set age from the inside out\noff is linear, will set age and angle in the X direction");
        R_LENGTH("numChildren", numChildren, "stem length in mm", "random");
        R_CURVE("age", child_age, "age of the children", "random");
        R_CURVE("child_angle", child_angle, "angle of the sub element relative to middle of clump", "change of angle as it ages, usually drooping");
        R_CURVE("random", child_rnd, "randomness in the angles", "random");
        R_FLOAT("age_power", child_age_power, 0.1f, 1.f, 25.f, "");
        ImGui::GetStyle().Colors[ImGuiCol_Header] = leaf_color;
        children.renderGui("children", gui_id);
    }
}


void _clumpBuilder::treeView()
{
    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Header] = stem_color;
    int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_OpenOnArrow;

    if (_rootPlant::selectedPart == this) {
        style.FrameBorderSize = 3;
        flags = flags | ImGuiTreeNodeFlags_Selected;
        style.Colors[ImGuiCol_Border] = ImVec4(0.7f, 0.7f, 0.7f, 1);
    }

    if (changedForSave) style.Colors[ImGuiCol_Header] = selected_color;

    TREE_LINE(name.c_str(), path.c_str(), flags)
    {
        if (ImGui::IsItemHovered()) {
            if (ribbonVertex::pivotPoints.size() > 256)     ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.f));
            ImGui::SetTooltip("%d instances \n%d verts \n%d / %d pivots ", numInstancePacked, numVertsPacked, numPivots, ribbonVertex::pivotPoints.size());
            if (ribbonVertex::pivotPoints.size() > 256)     ImGui::PopStyleColor();
        }

        style.FrameBorderSize = 0;
        CLICK_PART;
        ImGui::Text("children");
        for (auto& L : children.data) { if (L.plantPtr) L.plantPtr->treeView(); }
        ImGui::TreePop();
    }

    changedForSave |= changed;
    anyChange |= changed;
    changed = false;
}


lodBake* _clumpBuilder::getBakeInfo(uint i)
{
    if (i < lod_bakeInfo.size()) return &lod_bakeInfo[i];

    return nullptr;
}

levelOfDetail* _clumpBuilder::getLodInfo(uint i)
{
    if (i < lodInfo.size()) return &lodInfo[i];
    return nullptr;
}


void _clumpBuilder::clear_build_info()
{
    numInstancePacked = 0;
    numVertsPacked = 0;
    numPivots = 0;

    for (auto& L : children.data)
    {
        if (L.plantPtr)  L.plantPtr->clear_build_info();
    }
}

void _clumpBuilder::calculate_extents(buildSetting _settings)
{
    float2 extents = float2(0, 0);
    for (auto& R : ribbonVertex::ribbons)
    {
        float2 XZ = R.position.xz;
        float r = glm::length(XZ);
        //extents.x = __max(extents.x, r);
        //extents.y = __max(extents.y, abs(R.position.y));    // addign radius breaks down when we start to use lod-0 for sub elements
        extents.x = __max(extents.x, r + R.radius);
        extents.y = __max(extents.y, abs(R.position.y));    // + R.radius
    }

    // du
    float du6[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int cnt[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    float step = extents.y / 9.f;
    for (auto& R : ribbonVertex::ribbons)
    {
        float2 XZ = R.position.xz;
        float r = glm::length(XZ) + R.radius;
        float y = __max(0, R.position.y);   // only positive
        uint bucket = (uint)glm::clamp(y / step, 0.f, 8.f);
        du6[bucket] = __max(r, du6[bucket]);
        cnt[bucket]++;
    }
    std::array<float, 4> du;
    du[0] = du6[0];
    du[1] = __max(du6[2], du6[3]);
    du[2] = __max(du6[5], du6[6]);
    du[3] = du6[8];

    du[0] /= extents.x;
    du[1] /= extents.x;
    du[2] /= extents.x;
    du[3] /= extents.x;

    du[0] += 0.05f;
    du[1] += 0.05f;
    du[2] += 0.05f;
    du[3] += 0.05f;

    du[0] = __min(1.f, du[0]);
    du[1] = __min(1.f, du[1]);
    du[2] = __min(1.f, du[2]);
    du[3] = __min(1.f, du[3]);

    std::filesystem::path full_path = path;

    // lod 0
    float dw = lod_bakeInfo[0].bakeWidth;
    lod_bakeInfo[0].extents = extents;
    lod_bakeInfo[0].dU[0] = __min(1.f, du[0] / dw);
    lod_bakeInfo[0].dU[1] = __min(1.f, du[1] / dw);
    lod_bakeInfo[0].dU[2] = __min(1.f, du[2] / dw);
    lod_bakeInfo[0].dU[3] = __min(1.f, du[3] / dw);
    lod_bakeInfo[0].material.name = "bake_0";
    lod_bakeInfo[0].material.path = full_path.parent_path().string() + "\\bake_" + full_path.stem().string() + "\\" + lod_bakeInfo[0].material.name + ".vegetationMaterial";;

    // lod 1
    lod_bakeInfo[1].extents = extents;
    dw = lod_bakeInfo[1].bakeWidth;
    lod_bakeInfo[1].dU[0] = __min(1.f, du[0] / dw);
    lod_bakeInfo[1].dU[1] = __min(1.f, du[1] / dw);
    lod_bakeInfo[1].dU[2] = __min(1.f, du[2] / dw);
    lod_bakeInfo[1].dU[3] = __min(1.f, du[3] / dw);
    lod_bakeInfo[1].material.name = "bake_1";
    lod_bakeInfo[1].material.path = full_path.parent_path().string() + "\\bake_" + full_path.stem().string() + "\\" + lod_bakeInfo[1].material.name + ".vegetationMaterial";;

    // lod 2
    //extents.y = NODES.back()[3].y;   // last lod use NDOE end
    lod_bakeInfo[2].extents = extents;
    float dw2 = lod_bakeInfo[2].bakeWidth;
    lod_bakeInfo[2].dU[0] = __min(1.f, du[0] / dw2);
    lod_bakeInfo[2].dU[1] = __min(1.f, du[1] / dw2);
    lod_bakeInfo[2].dU[2] = __min(1.f, du[2] / dw2);
    lod_bakeInfo[2].dU[3] = __min(1.f, du[3] / dw2);
    lod_bakeInfo[2].material.name = "bake_2";
    lod_bakeInfo[2].material.path = full_path.parent_path().string() + "\\bake_" + full_path.stem().string() + "\\" + lod_bakeInfo[2].material.name + ".vegetationMaterial";;
}

glm::mat4 _clumpBuilder::buildChildren(buildSetting _settings, bool _addVerts)
{
    bool forcePHOTOTROPY = _settings.forcePhototropy;
    glm::mat4 ORIGINAL = _settings.root;
    std::uniform_real_distribution<> distrib(-1.f, 1.f);

    int oldSeed = _settings.seed;
    std::mt19937 MT(_settings.seed + 999);
    std::uniform_real_distribution<> DDD(-1.f, 1.f);
    _rootPlant::generator.seed(_settings.seed + 333);

    ribbonVertex R_verts;
    R_verts.startRibbon(true, _settings.pivotIndex);

    glm::mat4 nodeCENTERLINE = ORIGINAL;
    if (!radial)
    {
        ROLL(nodeCENTERLINE, 3.14f);
        if (!forcePHOTOTROPY)              // for clumps, gettign rid of this part makes tehm upright
        {
            PITCH(nodeCENTERLINE, child_angle.x);  // add rnd
        }
    }
    float3 CENTER_DIR = glm::normalize((float3)nodeCENTERLINE[1]);
    float tipDistance = 0;

    float centerTest = 1.5;

    ROOTS.clear();
    int numC = (int)RND_B(numChildren);
    float s = RND_B(size);
    float a = sqrt(RND_B(aspect));
    float w = s * a;
    float h = s / a;

    for (int i = 0; i < numC; i++)
    {
        // generate random points in a unit circle
        float l;
        float2 rndPos;
        do
        {
            rndPos.x = DDD(MT);
            rndPos.y = DDD(MT);
            l = glm::length(rndPos);
        } while (l >= 1.f);

        glm::mat4 node = ORIGINAL;

        node[3] += node[0] * rndPos.x * w;
        node[3] += node[2] * rndPos.y * h;

        //float c_angle = DDD(MT) * child_angle.x;
        //float c_angle_age = DDD(MT) * child_angle.y;
        float c_rnd = DDD(MT) * child_rnd.x;
        float c_rndp = DDD(MT) * child_rnd.x;
        float c_age = RND_B(child_age);
        float c_rot = atan2(rndPos.x, rndPos.y);
        float t = l;     // center to edge
        float t_live = 1.f - pow(t, child_age_power);

        if (radial)
        {
            ROLL(node, c_rot + 3.14f);
            PITCH(node, child_angle.x + (child_angle.y * l));  // add rnd
            //PITCH(node, c_angle * t_live + c_rndp);  // add rnd
            YAW(node, c_rnd);
        }
        else
        {
            t = rndPos.x * 0.5f + 0.5f;     // left to right  but needs controls how much it changes
            t_live = 1.f - pow(t, child_age_power);

            ROLL(node, 3.14f);

            if (!forcePHOTOTROPY)              // for clumps, gettign rid of this part makes tehm upright
            {
                PITCH(node, child_angle.x);  // add rnd
            }
            YAW(node, RND_B(child_rnd));
        }



        ROOTS.push_back(node);

        _settings.forcePhototropy = false;      // MAKE A COOPY this is just messign em up
        _settings.seed = oldSeed + i;
        _settings.root = node;
        _settings.node_age = c_age;
        _settings.normalized_age = t_live;
        _plantRND CHILD = children.get();
        glm::mat4 TIP;
        if (CHILD.plantPtr) TIP = CHILD.plantPtr->build(_settings, _addVerts);

        tipDistance = __max(tipDistance, glm::dot((float3)TIP[3] - (float3)ORIGINAL[3], CENTER_DIR));
    }

    //CENTER_DIR

    START = nodeCENTERLINE;
    TIP_CENTER = nodeCENTERLINE;

    GROW(TIP_CENTER, tipDistance);

    return TIP_CENTER;
}

glm::mat4 _clumpBuilder::build(buildSetting _settings, bool _addVerts)
{
    uint startVerts = ribbonVertex::ribbons.size();
    buildChildren(_settings, false);

    if (_addVerts)
    {
        std::uniform_int_distribution<> DDD(0, 255);

        ribbonVertex R;
        float3 extent = (float3)TIP_CENTER[3] - (float3)START[3];
        float ext_L = glm::length(extent);
        extent = glm::normalize(extent) / ext_L;

        _settings.pivotIndex[_settings.pivotDepth] = (int)ribbonVertex::pivotPoints.size();
        //_settings.pivotDepth += 1;
        R.pushPivot((float3)START[3], extent, rootFrequency() * sqrt(ext_L), ossilation_stiffness, ossilation_power, DDD(_rootPlant::generator));
        numPivots++;
        // clumps do not indent whne addign a pivot, all children are at the same level, saves us a silly level
    }


    if (_addVerts)
    {
        levelOfDetail lod = lodInfo.back();             // no parent can push us past this, similar when I do sun Parts, there will be a lod set for that add
        for (int i = lodInfo.size() - 1; i >= 0; i--)
        {
            if (_settings.pixelSize >= lodInfo[i].pixelSize)
            {
                lod = lodInfo[i];
            }
        }

        if (lod.bakeType == BAKE_DIAMOND)       build_2(_settings, lod.bakeIndex, true);
        if (lod.bakeType == BAKE_4)             build_4(_settings, lod.bakeIndex, true);
        //if (lod.bakeType == BAKE_N)             build_n(_settings, lod.bakeIndex, true);    // FIXME

        if (lod.useGeometry || _settings.forcePhototropy)
        {
            _settings.pixelSize *= lod.geometryPixelScale;

            buildChildren(_settings, true);
        }

        uint numVerts = ribbonVertex::ribbons.size() - startVerts;
        if (numVerts > 0) numInstancePacked++;
        numVertsPacked += numVerts;
    }

    /*
    if (_addVerts)
    {
        int max_bake = 0;
        for (int i = 0; i < 3; i++)
        {
            if (lod_bakeInfo[i].pixHeight > 0) max_bake = i;
        }

        if (_settings.node_age != -1)       // clamp to out last lod in this tree, dont let parents oveeride it
        {
            _settings.pixelSize = __max(_settings.pixelSize, lodInfo.back().pixelSize);
        }

        if (_settings.pixelSize >= lodInfo[0].pixelSize)
        {
            glm::mat4 R = build_2(_settings, 0, true);
            uint numVerts = ribbonVertex::ribbons.size() - startVerts;      // UGLY
            if (numVerts > 0) numInstancePacked++;
            numVertsPacked += numVerts;
            return R;
        }
        else if (_settings.pixelSize >= lodInfo[1].pixelSize)
        {
            glm::mat4 R = build_2(_settings, __min(1, max_bake), true);
            //uint numVerts = ribbonVertex::ribbons.size() - startVerts;
            //if (numVerts > 0) numInstancePacked++;
            //numVertsPacked += numVerts;
            //return R;
        }
        else if (_settings.pixelSize >= lodInfo[2].pixelSize)
        {
            glm::mat4 R = build_2(_settings, __min(2, max_bake), true);
            //uint numVerts = ribbonVertex::ribbons.size() - startVerts;
            //if (numVerts > 0) numInstancePacked++;
            //numVertsPacked += numVerts;
            //return R;
        }

    }

    if (_addVerts)
    {
        buildChildren(_settings, true);
    }

    uint numVerts = ribbonVertex::ribbons.size() - startVerts;
    if (numVerts > 0) numInstancePacked++;
    numVertsPacked += numVerts;
    */
    return TIP_CENTER;
}

glm::mat4 _clumpBuilder::build_lod_0(buildSetting _settings)
{
    glm::mat4 M(1);
    return M;
}

glm::mat4 _clumpBuilder::build_lod_1(buildSetting _settings)
{
    glm::mat4 M(1);
    return M;
}

glm::mat4 _clumpBuilder::build_lod_2(buildSetting _settings)
{
    glm::mat4 M(1);
    return M;
}

#pragma optimize("", off)
glm::mat4  _clumpBuilder::build_2(buildSetting _settings, uint _bakeIndex, bool _faceCamera)
{
    lodBake& lB = lod_bakeInfo[_bakeIndex];
    if (lB.pixHeight > 0)
    {
        float w = lB.extents.x * lB.bakeWidth;
        uint mat = lB.material.index;

        glm::mat4 node = START;
        glm::mat4 last = TIP_CENTER;

        // adjust start end
        float totalLenght = glm::length(last[3] - node[3]);
        GROW(node, totalLenght * lB.bake_V.x);
        GROW(last, -totalLenght * (1 - lB.bake_V.y));

        ribbonVertex R_verts;
        R_verts.startRibbon(_faceCamera, _settings.pivotIndex);
        R_verts.set(node, w * 0.8f, mat, float2(0.8f, 0.f), 1.f, 1.f, true, 0.5f, 0.1f, 0.0f, true);  // only 60% width, save pixels but test
        R_verts.set(last, w * 0.8f, mat, float2(0.8f, 1.f), 1.f, 1.f, true, 0.5f, 0.1f, 0.0f, true);
    }
    return TIP_CENTER;
}


glm::mat4  _clumpBuilder::build_4(buildSetting _settings, uint _bakeIndex, bool _faceCamera)
{
    lodBake& lB = lod_bakeInfo[_bakeIndex];
    if (lB.pixHeight > 0)
    {
        float w = lB.extents.x * lB.bakeWidth;
        uint mat = lB.material.index;

        glm::mat4 node = START;
        glm::mat4 last = TIP_CENTER;

        // adjust start end
        float totalLenght = glm::length(last[3] - node[3]);
        GROW(node, totalLenght * lB.bake_V.x);
        GROW(last, -totalLenght * (1 - lB.bake_V.y));

        glm::vec4 step = (last[3] - node[3]);
        float4 binorm_step = (last[1] - node[1]);

        node[3] += step * lB.bake_V.x;
        node[1] += binorm_step * lB.bake_V.x;
        step *= (lB.bake_V.y - lB.bake_V.x);
        binorm_step *= (lB.bake_V.y - lB.bake_V.x);
        step /= 3.f;
        binorm_step /= 3.f;

        ribbonVertex R_verts;
        R_verts.startRibbon(_faceCamera, _settings.pivotIndex);

        for (int i = 0; i < 4; i++)
        {
            R_verts.set(node, w * lod_bakeInfo[_bakeIndex].dU[i] * 0.99f, mat, float2(lod_bakeInfo[_bakeIndex].dU[i] * 0.99f, 1.f - (0.3333333f * i)), 1.f, 1.f);
            node[3] += step;
            node[1] += binorm_step;
        }
    }
    return TIP_CENTER;



}
#pragma optimize("", on)






void _rootPlant::onLoad()
{
    plantData = Buffer::createStructured(sizeof(plant), 256);
    plantpivotData = Buffer::createStructured(sizeof(_plant_anim_pivot), 256 * 256);
    instanceData = Buffer::createStructured(sizeof(plant_instance), 16384);
    instanceData_Billboards = Buffer::createStructured(sizeof(plant_instance), 16384, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
    blockData = Buffer::createStructured(sizeof(block_data), 16384 * 32, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);        // big enough to house inatnces * blocks per instance   8 Mb for now
    vertexData = Buffer::createStructured(sizeof(ribbonVertex8), 65536 * 8);
    drawArgs_vegetation = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
    drawArgs_billboards = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);

    buffer_feedback = Buffer::createStructured(sizeof(vegetation_feedback), 1);
    buffer_feedback_read = Buffer::createStructured(sizeof(vegetation_feedback), 1, Resource::BindFlags::None, Buffer::CpuAccess::Read);
    // compute
    //split.compute_tileClear.Vars()->setBuffer("DrawArgs_ClippedLoddedPlants", split.drawArgs_clippedloddedplants);


    compute_clearBuffers.load("Samples/Earthworks_4/hlsl/terrain/compute_vegetation_clear.hlsl");
    compute_clearBuffers.Vars()->setBuffer("DrawArgs_Quads", drawArgs_billboards);
    compute_clearBuffers.Vars()->setBuffer("DrawArgs_Plants", drawArgs_vegetation);
    compute_clearBuffers.Vars()->setBuffer("feedback", buffer_feedback);

    compute_calulate_lod.load("Samples/Earthworks_4/hlsl/terrain/compute_vegetation_lod.hlsl");
    compute_calulate_lod.Vars()->setBuffer("DrawArgs_Quads", drawArgs_billboards);
    compute_calulate_lod.Vars()->setBuffer("DrawArgs_Plants", drawArgs_vegetation);
    compute_calulate_lod.Vars()->setBuffer("plant_buffer", plantData);
    compute_calulate_lod.Vars()->setBuffer("instance_buffer", instanceData);
    compute_calulate_lod.Vars()->setBuffer("instance_buffer_billboard", instanceData_Billboards);
    compute_calulate_lod.Vars()->setBuffer("block_buffer", blockData);
    compute_calulate_lod.Vars()->setBuffer("feedback", buffer_feedback);

    std::uniform_real_distribution<> RND(-1.f, 1.f);

    for (int i = 0; i < 16384; i++)
    {
        instanceBuf[i].plant_idx = 0;
        instanceBuf[i].position = { RND(generator) * 25, 1000.f, RND(generator) * 25 };
        instanceBuf[i].scale = 1.f + RND(generator) * 0.2f;
        instanceBuf[i].rotation = RND(generator) * 3.14f;
        instanceBuf[i].time_offset = RND(generator) * 100;
    }
    instanceBuf[0].position = { 0, 1000, 0 };
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
    auto& starts = vegetationShader.Program()->getGlobalCompilationStats();
    vegetationShader.Vars()->setBuffer("plant_buffer", plantData);
    vegetationShader.Vars()->setBuffer("plant_pivot_buffer", plantpivotData);
    vegetationShader.Vars()->setBuffer("instance_buffer", instanceData);
    vegetationShader.Vars()->setBuffer("block_buffer", blockData);
    vegetationShader.Vars()->setBuffer("vertex_buffer", vertexData);
    vegetationShader.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
    vegetationShader.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX
    vegetationShader.Vars()->setSampler("gSamplerClamp", sampler_ClampAnisotropic);              // fixme only cvlamlX
    auto& block = vegetationShader.Vars()->getParameterBlock("textures");
    varVegTextures = block->findMember("T");


    vegetationShader_GOURAUD.add("_GOURAUD_SHADING", "");
    vegetationShader_GOURAUD.load("Samples/Earthworks_4/hlsl/terrain/render_vegetation_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
    vegetationShader_GOURAUD.Vars()->setBuffer("plant_buffer", plantData);
    vegetationShader_GOURAUD.Vars()->setBuffer("plant_pivot_buffer", plantpivotData);
    vegetationShader_GOURAUD.Vars()->setBuffer("instance_buffer", instanceData);
    vegetationShader_GOURAUD.Vars()->setBuffer("block_buffer", blockData);
    vegetationShader_GOURAUD.Vars()->setBuffer("vertex_buffer", vertexData);
    vegetationShader_GOURAUD.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
    vegetationShader_GOURAUD.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX
    vegetationShader_GOURAUD.Vars()->setSampler("gSamplerClamp", sampler_ClampAnisotropic);              // fixme only cvlamlX
    auto& blockGouraud = vegetationShader_GOURAUD.Vars()->getParameterBlock("textures");
    varTextures_Gauraud = blockGouraud->findMember("T");

    vegetationShader_DEBUG_PIVOTS.add("_DEBUG_PIVOTS", "");
    vegetationShader_DEBUG_PIVOTS.load("Samples/Earthworks_4/hlsl/terrain/render_vegetation_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
    vegetationShader_DEBUG_PIVOTS.Vars()->setBuffer("plant_buffer", plantData);
    vegetationShader_DEBUG_PIVOTS.Vars()->setBuffer("plant_pivot_buffer", plantpivotData);
    vegetationShader_DEBUG_PIVOTS.Vars()->setBuffer("instance_buffer", instanceData);
    vegetationShader_DEBUG_PIVOTS.Vars()->setBuffer("block_buffer", blockData);
    vegetationShader_DEBUG_PIVOTS.Vars()->setBuffer("vertex_buffer", vertexData);
    vegetationShader_DEBUG_PIVOTS.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
    vegetationShader_DEBUG_PIVOTS.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX
    vegetationShader_DEBUG_PIVOTS.Vars()->setSampler("gSamplerClamp", sampler_ClampAnisotropic);              // fixme only cvlamlX
    auto& blockPivots = vegetationShader_DEBUG_PIVOTS.Vars()->getParameterBlock("textures");
    varTextures_Debug_Pivots = blockPivots->findMember("T");

    vegetationShader_DEBUG_PIXELS.add("_DEBUG_PIXELS", "");
    vegetationShader_DEBUG_PIXELS.load("Samples/Earthworks_4/hlsl/terrain/render_vegetation_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
    vegetationShader_DEBUG_PIXELS.Vars()->setBuffer("plant_buffer", plantData);
    vegetationShader_DEBUG_PIXELS.Vars()->setBuffer("plant_pivot_buffer", plantpivotData);
    vegetationShader_DEBUG_PIXELS.Vars()->setBuffer("instance_buffer", instanceData);
    vegetationShader_DEBUG_PIXELS.Vars()->setBuffer("block_buffer", blockData);
    vegetationShader_DEBUG_PIXELS.Vars()->setBuffer("vertex_buffer", vertexData);
    vegetationShader_DEBUG_PIXELS.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
    vegetationShader_DEBUG_PIXELS.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX
    vegetationShader_DEBUG_PIXELS.Vars()->setSampler("gSamplerClamp", sampler_ClampAnisotropic);              // fixme only cvlamlX
    auto& blockPixels = vegetationShader_DEBUG_PIXELS.Vars()->getParameterBlock("textures");
    varTextures_Debug_Pixels = blockPixels->findMember("T");


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
#pragma optimize("", off)


void _rootPlant::renderGui_perf(Gui* _gui)
{
    uint gui_id = 14;
    bool changed;

    ImGui::PushFont(_gui->getFont("header2"));
    {
        //            timeAvs += 0.01f * gpFramework->getFrameRate().getAverageFrameTime();
        ImGui::Text("gpu %1.3fms + %1.3fms", gputime, gputimeBB);

        float numTri = (float)feedback.numBlocks * VEG_BLOCK_SIZE * 2.f / 1000000.f;
        if (displayModeSinglePlant)
        {
            numTri = (float)totalBlocksToRender * VEG_BLOCK_SIZE * 2.f / 1000000.f;
        }

        ImGui::Text("%2.2f M tri ", numTri);

        if (gputime > 0)
        {
            ImGui::SameLine(0, 10);
            ImGui::Text("%2.2f tri / ms", numTri / gputime);
        }
    }
    ImGui::PopFont();

    if (displayModeSinglePlant)
    {

        //ImGui::Text("%2.2f M tris", (float)totalBlocksToRender * VEG_BLOCK_SIZE * 2.f / 1000000.f);
    }
    else if (root)
    {
        R_FLOAT("lod Bias", loddingBias, 0.01f, 0.1f, 10.f, "");
        ImGui::Text("plantZero, %2.2fpix - lod %d", feedback.plantZero_pixeSize, feedback.plantZeroLod);

        ImGui::PushFont(_gui->getFont("small"));
        {

            //ImGui::PushFont(_gui->getFont("small"));
            {
                ImGui::NewLine();
                for (int i = 1; i < 10; i++)
                {
                    ImGui::SameLine((float)(i * 40), 0);
                    ImGui::Text("%d, ", feedback.numLod[i]);
                }

                ImGui::NewLine();
                for (int i = 1; i < 10; i++)
                {
                    if (root->getLodInfo(i))
                    {
                        ImGui::SameLine((float)(i * 40), 0);
                        ImGui::Text("%d, ", root->getLodInfo(i)->numBlocks);
                    }
                }

                ImGui::NewLine();
                for (int i = 1; i < 10; i++)
                {
                    if (root->getLodInfo(i))
                    {
                        ImGui::SameLine((float)(i * 40), 0);
                        ImGui::Text("%d, ", root->getLodInfo(i)->numVerts);
                    }
                }


                ImGui::NewLine();
                for (int i = 1; i < 10; i++)
                {
                    if (root->getLodInfo(i))
                    {
                        ImGui::SameLine((float)(i * 40), 0);
                        ImGui::Text("%1.2f, ", feedback.numLod[i] * root->getLodInfo(i)->numVerts * 2 / 1000000.f);
                    }
                }
                ImGui::SameLine(0, 5);
                ImGui::Text(" M tris");
            }
        }
        ImGui::PopFont();


    }
    auto& style = ImGui::GetStyle();
    style.FrameBorderSize = 1;
    ImGui::Checkbox("show debug colours", &showDebugInShader);
    ImGui::Checkbox("show pivots", &showNumPivots);
    style.FrameBorderSize = 0;

    ImGui::DragInt2("plants", &firstPlant, 0.1f, 0, 1000);
    ImGui::DragInt("lods", &firstLod, 0.1f, -1, 1000);
}

void _rootPlant::renderGui_lodbake(Gui* _gui)
{
}

void _rootPlant::renderGui_other(Gui* _gui)
{
}


void _rootPlant::renderGui(Gui* _gui)
{
    uint gui_id = 99994;
    _plantBuilder::_gui = _gui;

    Gui::Window builderPanel(_gui, "vegetation builder", { 900, 900 }, { 100, 100 });
    {
        ImGui::Columns(2);
        float columnWidth = ImGui::GetWindowWidth() / 2 - 10;
        auto& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.05f, 0.0f, 0.0f);
        int flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_OpenOnArrow;

        style.FrameBorderSize = 0;

        ImGui::PushFont(_gui->getFont("default"));
        {
            ImGui::PushFont(_gui->getFont("header2"));
            {
                if (ImGui::Button("Load")) {
                    std::filesystem::path filepath;
                    FileDialogFilterVec filters = { {"leaf"}, {"stem"}, {"clump"} };
                    if (openFileDialog(filters, filepath))
                    {
                        _rootPlant::selectedMaterial = nullptr;
                        if (root) delete root;
                        if (filepath.string().find(".leaf") != std::string::npos) { root = new _leafBuilder;  _rootPlant::selectedPart = root; }
                        if (filepath.string().find(".stem") != std::string::npos) { root = new _stemBuilder;  _rootPlant::selectedPart = root; }
                        if (filepath.string().find(".clump") != std::string::npos) { root = new _clumpBuilder;  _rootPlant::selectedPart = root; }

                        if (root)
                        {
                            root->path = materialCache::getRelative(filepath.string());
                            root->name = filepath.filename().string();
                            root->loadPath();
                            build(true);    //to get extetns
                        }
                    }
                }
                if (ImGui::Button("import"))
                {
                    importBinary();
                }
                ImGui::SameLine(0, 30);
                if (ImGui::Button("clearBinary"))
                {
                    numBinaryPlants = 0;
                    binVertexOffset = 0;
                    binPlantOffset = 0;
                    binPivotOffset = 0;
                }
            }
            ImGui::PopFont();

            if (ImGui::Button("new Stem")) { if (root) delete root; root = new _stemBuilder;  _rootPlant::selectedPart = root; _rootPlant::selectedMaterial = nullptr; }
            ImGui::SameLine(0, 15);
            if (ImGui::Button("new Clump")) { if (root) delete root; root = new _clumpBuilder;  _rootPlant::selectedPart = root; _rootPlant::selectedMaterial = nullptr; }
            ImGui::SameLine(0, 15);
            if (ImGui::Button("new Leaf")) { if (root) delete root; root = new _leafBuilder;  _rootPlant::selectedPart = root; _rootPlant::selectedMaterial = nullptr; }


            ImGui::NewLine();
            style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
            if (ImGui::TreeNodeEx("feedback", flags))
            {
                renderGui_perf(_gui);
                ImGui::TreePop();
            }


            if (root)
            {
                ImGui::NewLine();
                FONT_TEXT("header1", root->name.c_str());

                root->treeView();
            }


            ImGui::NewLine();
            style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
            if (ImGui::TreeNodeEx("lodding", flags))
            {
                if (selectedPart)
                {
                    ImGui::Text("size (%2.2f, %2.2f)m", extents.x, extents.y);
                    ImGui::SameLine(columnWidth / 2, 0);
                    if (ImGui::Button("Build all lods", ImVec2(columnWidth / 2 - 10, 0)))
                    {
                        buildAllLods();
                    }
                    ImGui::Text("v - %d, b - %d, u - %d, %d rejected", totalBlocksToRender * VEG_BLOCK_SIZE, totalBlocksToRender, unusedVerts, ribbonVertex::totalRejectedVerts);
                    {
                        if (ImGui::Button("   -   ")) { selectedPart->decrementLods(); }
                        ImGui::SameLine(0, 50);
                        if (ImGui::Button("   +   ")) { selectedPart->incrementLods(); }
                    }

                    for (uint lod = 0; lod < 100; lod++)
                    {
                        levelOfDetail* lodInfo = selectedPart->getLodInfo(lod);
                        if (lodInfo)
                        {
                            if (currentLOD == lod)
                            {
                                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16f, 0.05f, 0.0f, 0.9f));
                            }
                            else
                            {
                                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16f, 0.05f, 0.0f, 0.03f));
                            }

                            ImGui::BeginChildFrame(5678 + lod, ImVec2(columnWidth, 110));
                            {
                                ImGui::Text("%d", lod);
                                ImGui::SameLine(0, 30);
                                if (ImGui::Button("build"))
                                {
                                    //if (lod > 2) build(false);
                                    displayModeSinglePlant = true;
                                    anyChange = true;
                                    lodInfo->pixelSize = extents.y / lodInfo->numPixels;
                                    settings.pixelSize = lodInfo->pixelSize;
                                    currentLOD = lod;
                                }
                                ImGui::SameLine(0, 10);
                                ImGui::SetNextItemWidth(60);
                                if (ImGui::DragInt("##numPix", &(lodInfo->numPixels), 0.1f, 8, 2000))
                                {
                                    //if (lod > 2) build(true);   // to generate new extents THIS IS BAD
                                    //displayModeSinglePlant = true;
                                    anyChange = true;
                                    lodInfo->pixelSize = extents.y / lodInfo->numPixels;
                                    settings.pixelSize = lodInfo->pixelSize;
                                    selectedPart->changed = true;
                                }

                                ImGui::SameLine(0, 10);
                                ImGui::Text("%5.1fmm", lodInfo->pixelSize * 1000.f);


                                // middle line
                                ImGui::NewLine();
                                ImGui::SameLine(0, 10);
                                ImGui::SetNextItemWidth(60);
                                if (ImGui::Checkbox("geo", &lodInfo->useGeometry)) { selectedPart->changed = true; }

                                ImGui::SameLine(0, 10);
                                ImGui::SetNextItemWidth(60);
                                if (ImGui::Combo("type", &lodInfo->bakeType, "none\0diamond\0'4'\0N\0")) { selectedPart->changed = true; }

                                ImGui::SameLine(0, 10);
                                ImGui::SetNextItemWidth(60);
                                if (ImGui::DragInt("##bakeIdx", &(lodInfo->bakeIndex), 0.1f, 0, 2)) { selectedPart->changed = true; }

                                ImGui::Text("%d: verts", lodInfo->numVerts);
                                ImGui::SameLine(0, 30);
                                ImGui::SetNextItemWidth(60);
                                if (ImGui::DragFloat("scale", &lodInfo->geometryPixelScale, 0.01f, 0.1f, 10)) { selectedPart->changed = true; }

                            }
                            ImGui::EndChildFrame();
                            ImGui::PopStyleColor();
                        }
                    }

                    if (ImGui::Button("Build full resolution", ImVec2(columnWidth / 2 - 10, 0)))
                    {
                        settings.pixelSize = 0.00005f;   // half a mm
                        build(true);   // to generate new extents
                        displayModeSinglePlant = true;
                        anyChange = true;
                        currentLOD = -1;

                        for (uint lod = 0; lod < 100; lod++)
                        {
                            levelOfDetail* lodInfo = selectedPart->getLodInfo(lod);
                            if (lodInfo)    lodInfo->pixelSize = extents.y / lodInfo->numPixels;
                        }
                    }

                    if (selectedPart == root)
                    {
                        ImGui::SameLine(0, 0);
                        if (ImGui::Button("calculate extents", ImVec2(columnWidth / 2 - 10, 0)))
                        {
                            settings.pixelSize = 0.00005f;   // half a mm
                            settings.forcePhototropy = true;
                            build(true);   // to generate new extents
                            settings.forcePhototropy = false;
                            displayModeSinglePlant = true;
                            anyChange = true;
                            selectedPart->changed = true;
                            selectedPart->calculate_extents(settings);
                        }
                    }
                }
                ImGui::TreePop();
            }





            ImGui::NewLine();

            if (selectedPart)
            {
                if (ImGui::Button(" Bake materials "))
                {

                    std::filesystem::path PT = selectedPart->path;
                    std::string resource = terrafectorEditorMaterial::rootFolder;

                    std::string newDir = resource + PT.parent_path().string() + "\\bake_" + PT.stem().string();
                    replaceAllVEG(newDir, "/", "\\");
                    newDir = "rmdir /S /Q " + newDir;
                    system(newDir.c_str());

                    newDir = "mkdir " + resource + PT.parent_path().string() + "\\bake_" + PT.stem().string();
                    replaceAllVEG(newDir, "/", "\\");
                    system(newDir.c_str());

                    for (int seed = 1000; seed < 1001; seed++)
                    {
                        settings.seed = seed;
                        settings.pixelSize = 0.0001f;
                        settings.forcePhototropy = true;
                        ribbonVertex::packed.clear();
                        build(true);
                        settings.forcePhototropy = false;
                        selectedPart->calculate_extents(settings);

                        for (int i = 0; i < 10; i++)
                        {
                            if (selectedPart->getBakeInfo(i) && (selectedPart->getBakeInfo(i)->pixHeight > 0))
                            {
                                bake(selectedPart->path, std::to_string(settings.seed), selectedPart->getBakeInfo(i));
                                selectedPart->getBakeInfo(i)->material.reload();
                                selectedPart->changed = true;
                            }
                        }
                    }
                    //selectedPart->save();
                }

                for (int i = 0; i < 10; i++)
                {
                    lodBake* bake = selectedPart->getBakeInfo(i);
                    if (bake)
                    {
                        float columnWidth = ImGui::GetWindowWidth() / 2 - 5;
                        style.Colors[ImGuiCol_Header] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
                        std::string bakeStr = "bake-" + std::to_string(i);
                        if (ImGui::TreeNodeEx(bakeStr.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_OpenOnArrow))
                        {
                            bool changed = false;


                            R_INT("height", bake->pixHeight, 0, 512, "");
                            R_FLOAT("width", bake->bakeWidth, 0.01f, 0.1f, 10.f, "");
                            R_CURVE("start / stop", bake->bake_V, "", "");
                            R_FLOAT("translucency", bake->translucency, 0.01f, 0.1f, 10.f, "");
                            R_FLOAT("alpha", bake->alphaPow, 0.01f, 0.1f, 10.f, "");
                            CHECKBOX("bake AO", &bake->bakeAOToAlbedo, "if used as a billboard, bake the ao to the texture itself for added depth\nif used as a code with vertex ao, leave out");
                            CHECKBOX("forceDiamond", &bake->forceDiamond, "");
                            CHECKBOX("faceCamera", &bake->faceCamera, "");

                            if (changed) selectedPart->changed = true;
                            //anyChange |= changed;

                            ImGui::TreePop();
                        }
                    }
                }
            }








            ImGui::NewLine();

            if (ImGui::DragFloat3("wind dir", &windDir.x, 0.01f, -1.f, 1.f))
            {
                windDir.y = 0;
                windDir = glm::normalize(windDir);
            }
            ImGui::DragFloat("strength", &windStrength, 0.1f, 0.f, 100.f, "%3.2fm/s");
            ImGui::SameLine(0, 20);
            ImGui::Text("%3.2f km/h", windStrength * 3.6f);




            ImGui::NewLine();
            static float numPix = 100;
            numPix = vertex_pack_Settings.objectSize / settings.pixelSize;
            if (ImGui::DragFloat("size", &vertex_pack_Settings.objectSize, 0.01f, 1.f, 64.f, "%3.2fm")) anyChange = true;
            if (ImGui::DragFloat("radius", &vertex_pack_Settings.radiusScale, 0.001f, 0.1f, 8.f, "%3.2fm")) anyChange = true;
            if (ImGui::DragFloat("num pix", &numPix, 1.f, 1.f, 1000.f, "%3.4f")) { settings.pixelSize = vertex_pack_Settings.objectSize / numPix; anyChange = true; }
            ImGui::Text("pixel is %2.1f mm", settings.pixelSize * 1000.f);
            if (ImGui::DragFloat("pix SZ", &settings.pixelSize, 0.001f, 0.001f, 1.f, "%3.2fm")) {
                numPix = vertex_pack_Settings.objectSize / settings.pixelSize; anyChange = true;
            }

            if (ImGui::DragInt("seed", &settings.seed, 1, 0, 1000)) { anyChange = true; }



            ImGui::NewLine();
            style.FrameBorderSize = 1;
            ImGui::Checkbox("cropLines", &cropLines);
            style.FrameBorderSize = 0;

            
            if (ImGui::DragFloat("rnd area", &instanceArea[0], 0.1f, 5.f, 100.f, "%3.2fm"))
            {
                builInstanceBuffer();
            }
            if (ImGui::DragFloat("rnd area1", &instanceArea[1], 0.1f, 5.f, 100.f, "%3.2fm"))
            {
                builInstanceBuffer();
            }
            if (ImGui::DragFloat("rnd area2", &instanceArea[2], 0.1f, 5.f, 100.f, "%3.2fm"))
            {
                builInstanceBuffer();
            }
            if (ImGui::DragFloat("rnd area3", &instanceArea[3], 0.1f, 5.f, 100.f, "%3.2fm"))
            {
                builInstanceBuffer();
            }







            if (root && anyChange)
            {
                ImGui::Text("re building...");
                if (displayModeSinglePlant)
                {
                    ribbonVertex::packed.clear();
                    build(false);

                    levelOfDetail* lod = root->getLodInfo(currentLOD);
                    if (lod)
                    {
                        lod->numVerts = (int)ribbonVertex::ribbons.size();
                        lod->numBlocks = ribbonVertex::packed.size() / VEG_BLOCK_SIZE;
                        lod->unused = ribbonVertex::packed.size() - ribbonVertex::ribbons.size();
                    }
                }
                else
                {
                    buildAllLods();
                }
                anyChange = false;
            }






            ImGui::NextColumn();
            if (selectedPart)
            {
                float columnWidth = ImGui::GetWindowWidth() / 2 - 10;
                float columnHeight = ImGui::GetWindowHeight() - 25;

                //ImVec2 CP = ImGui::GetCursorPos();
                if (selectedPart->changedForSave)
                {
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16f, 0.05f, 0.0f, 0.9f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16f, 0.05f, 0.0f, 0.0f));
                }
                ImGui::BeginChildFrame(123450, ImVec2(columnWidth, columnHeight));
                {
                    selectedPart->renderGui();  // now render the selected item.
                }
                ImGui::EndChildFrame();
                ImGui::PopStyleColor();
                //ImGui::SetCursorPos(CP);

            }
        }
        ImGui::PopFont();
    }
    builderPanel.release();
}


void _rootPlant::buildAllLods()
{
    displayModeSinglePlant = false;
    tempUpdateRender = true;

    uint start = 0;
    settings.pixelSize = 0.001f;    // 1mm just tiny
    build(true); // this is just to generate extents    .. Should generate all of teh pivot data as well
    float Y = extents.y;

    ribbonVertex::packed.clear();

    uint startBlock[16][16];
    uint numBlocks[16][16];
    for (int pIndex = 0; pIndex < 4; pIndex++)
    {
        plantBuf[pIndex].radiusScale = ribbonVertex::radiusScale;
        plantBuf[pIndex].scale = ribbonVertex::objectScale;
        plantBuf[pIndex].offset = ribbonVertex::objectOffset;
        plantBuf[pIndex].Ao_depthScale = 0.3f;  // FIXME unused
        plantBuf[pIndex].bDepth = 1;
        plantBuf[pIndex].bScale = 1;
        plantBuf[pIndex].sunTilt = -0.2f;
        plantBuf[pIndex].size = extents;
        plantBuf[pIndex].shadowUVScale = root->shadowUVScale;
        plantBuf[pIndex].shadowSoftness = root->shadowSoftness;

        plantBuf[pIndex].rootPivot.root = { 0, 0, 0 };
        plantBuf[pIndex].rootPivot.extent = { 0, 1.f / extents.y, 0 };
        plantBuf[pIndex].rootPivot.frequency = root->rootFrequency() * sqrt(extents.y);
        plantBuf[pIndex].rootPivot.stiffness = root->ossilation_stiffness;
        plantBuf[pIndex].rootPivot.shift = root->ossilation_power;

        lodBake* lodZero = root->getBakeInfo(0);
        if (lodZero)
        {
            plantBuf[pIndex].billboardMaterialIndex = lodZero->material.index;
        }

        plantBuf[pIndex].numLods = 0;
        for (uint lod = 100; lod >= 1; lod--)   // backwards since it fixes abug in build()
        {
            levelOfDetail* lodInfo = root->getLodInfo(lod);
            if (lodInfo)
            {
                

                lodInfo->pixelSize = Y / lodInfo->numPixels;
                settings.pixelSize = lodInfo->pixelSize;
                settings.seed = 1000 + pIndex;
                build(false, pIndex * 256 * sizeof(_plant_anim_pivot));
                lodInfo->numVerts = (int)ribbonVertex::ribbons.size();
                lodInfo->numBlocks = ribbonVertex::packed.size() / VEG_BLOCK_SIZE - start;
                lodInfo->unused = ribbonVertex::packed.size() - ribbonVertex::ribbons.size();
                lodInfo->startBlock = start;

                startBlock[pIndex][lod] = start;
                numBlocks[pIndex][lod] = lodInfo->numBlocks;


                plantBuf[pIndex].numLods = __max(plantBuf[pIndex].numLods, lod - 1);    // omdat ons nou agteruit gaan
                plantBuf[pIndex].lods[lod - 1].pixSize = (float)lodInfo->numPixels;
                plantBuf[pIndex].lods[lod - 1].numBlocks = lodInfo->numBlocks;
                plantBuf[pIndex].lods[lod - 1].startVertex = start * VEG_BLOCK_SIZE;



                start += lodInfo->numBlocks;

                fprintf(terrafectorSystem::_logfile, "plant lod : %d, %fm, %d\n", lod, (float)lodInfo->numPixels, lodInfo->numBlocks);
            }
        }
        // need to make a copy of the pivot data here after most detailed build I guess
        //plantpivotData->setBlob(ribbonVertex::pivotPoints.data(), , ribbonVertex::pivotPoints.size() * sizeof(_plant_anim_pivot));
    }

    plantData->setBlob(plantBuf.data(), 0, 4 * sizeof(plant));

    int numV = __min(65536 * 8, ribbonVertex::packed.size());
    vertexData->setBlob(ribbonVertex::packed.data(), 0, numV * sizeof(ribbonVertex8));                // FIXME uploads should be smaller

    settings.seed = 1000;

    binaryPlantOnDisk OnDisk;
    /*
    OnDisk.plantData.resize(4); // FIXME is we do otherthen 4 variations
    for (int i = 0; i < 4; i++)
    {
        OnDisk.plantData[i] = plantBuf[i];
    }
    OnDisk.vertexData.resize(numV); // FIXME is we do otherthen 4 variations
    for (int i = 0; i < numV; i++)
    {
        OnDisk.vertexData[i] = ribbonVertex::packed[i];
    }
    */
    OnDisk.numP = 4;
    OnDisk.numV = numV;
    //OnDisk.getMaterials();
    for (int i = 0; i < numV; i++)
    {
        int idx = (ribbonVertex::packed[i].b >> 8) & 0x3ff;
        _vegMaterial M;
        M.path = _plantMaterial::static_materials_veg.materialVector[idx].relativePath;
        M.name = _plantMaterial::static_materials_veg.materialVector[idx].displayName;
        M.index = idx;
        OnDisk.materials[idx] = M;
    }

    std::string resource = terrafectorEditorMaterial::rootFolder;
    std::ofstream os(resource + root->path + ".binary");
    cereal::JSONOutputArchive archive(os);
    archive(OnDisk);

    std::ofstream osData(resource + root->path + ".binaryData", std::ios::binary);
    osData.write((const char*)plantBuf.data(), 4 * sizeof(plant));
    osData.write((const char*)ribbonVertex::packed.data(), numV * sizeof(ribbonVertex8));
    osData.write((const char*)ribbonVertex::pivotPoints.data(), 4 * 256 * sizeof(_plant_anim_pivot));
}


void binaryPlantOnDisk::getMaterials()
{
    /*
    for (auto& V : vertexData)
    {
        int idx = (V.b >> 8) & 0x3ff;
        materials[idx] = _plantMaterial::static_materials_veg.materialVector[idx];
    }*/
}

void binaryPlantOnDisk::onLoad(std::string path, uint vOffset)
{
    plantData.resize(numP);
    vertexData.resize(numV);
    pivotData.resize(numP * 256);

    std::ifstream osData(path + "Data", std::ios::binary);
    osData.read((char*)plantData.data(), 4 * sizeof(plant));
    osData.read((char*)vertexData.data(), numV * sizeof(ribbonVertex8));
    osData.read((char*)pivotData.data(), numP * 256 * sizeof(_plant_anim_pivot));

    // load materials, and build remapper
    std::string resource = terrafectorEditorMaterial::rootFolder;
    int indexLookup[4096];// just big, bad code
    for (auto& M : materials)
    {
        indexLookup[M.first] = _plantMaterial::static_materials_veg.find_insert_material(std::filesystem::path(resource + M.second.path), false); //terrafectorEditorMaterial::rootFolder + 
    }

    
        for (auto& V : vertexData)
        {
            int idx = (V.b >> 8 ) & 0x3ff;
            V.b ^= (idx << 8);  // xor clears
            V.b += (indexLookup[idx] << 8);
        }

        for (auto& P : plantData)
        {
            for (int i = 0; i <= P.numLods; i++)
            {
                P.lods[i].startVertex += vOffset;
            }
        }


        
}

void _rootPlant::importBinary()
{
    FileDialogFilterVec filters = { {"binary"} };
    std::filesystem::path filepath;
    if (openFileDialog(filters, filepath))
    {
        binaryPlantOnDisk OnDisk;
        std::ifstream os(filepath);
        cereal::JSONInputArchive archive(os);
        archive(OnDisk);

        OnDisk.onLoad(filepath.string(), binVertexOffset / sizeof(ribbonVertex8));

        plantData->setBlob(OnDisk.plantData.data(), binPlantOffset, OnDisk.numP * sizeof(plant));

        int numV = __min(65536 * 8, OnDisk.numV);
        vertexData->setBlob(OnDisk.vertexData.data(), binVertexOffset, numV * sizeof(ribbonVertex8));                // FIXME uploads should be smaller

        plantpivotData->setBlob(OnDisk.pivotData.data(), binPivotOffset, OnDisk.numP * 256 * sizeof(_plant_anim_pivot));

        binVertexOffset += numV * sizeof(ribbonVertex8);
        binPlantOffset += OnDisk.numP * sizeof(plant);
        binPivotOffset += OnDisk.numP * 256 * sizeof(_plant_anim_pivot);
        numBinaryPlants++;

        displayModeSinglePlant = false;
        ribbonVertex::packed.resize(OnDisk.numV);
        tempUpdateRender = true;
        _plantMaterial::static_materials_veg.rebuildStructuredBuffer();
    }
}

void _rootPlant::build(bool _updateExtents, uint pivotOffset)
{
    if (root)
    {
        ribbonVertex::objectScale = vertex_pack_Settings.getScale();
        ribbonVertex::radiusScale = vertex_pack_Settings.radiusScale;
        ribbonVertex::objectOffset = vertex_pack_Settings.getOffset();

        settings.parentStemDir = { 0, 1, 0 };
        settings.root = glm::mat4(1.0);
        settings.node_age = -1;
        settings.normalized_age = 1;

        ribbonVertex::ribbons.clear();
        ribbonVertex::pivotPoints.clear();
        ribbonVertex::clearStats(9999);     // just very large for now
        settings.pivotIndex[0] = 255;
        settings.pivotIndex[1] = 255;
        settings.pivotIndex[2] = 255;
        settings.pivotIndex[3] = 255;
        settings.pivotDepth = 0;


        generator.seed(settings.seed);
        root->clear_build_info();
        root->build(settings, true);

        // Now light the plant

        // now pack it
        if (_updateExtents)
        {
            extents = float2(0, 0);
            for (auto& R : ribbonVertex::ribbons)
            {
                // can we do this earlier during set - to a global extents?
                float2 XZ = R.position.xz;
                float r = glm::length(XZ);
                extents.x = __max(extents.x, r + R.radius);
                extents.y = __max(extents.y, abs(R.position.y) + R.radius);
            }
        }

        int verts = ribbonVertex::ribbons.size();
        int last = verts % VEG_BLOCK_SIZE;
        unusedVerts = 0;
        if (last > 0) unusedVerts = VEG_BLOCK_SIZE - last;

        for (auto& R : ribbonVertex::ribbons)
        {
            R.lightBasic(extents, root->shadowDepth, root->shadowPenetationHeight);
            ribbonVertex8 P = R.pack();
            ribbonVertex::packed.push_back(P);
        }

        // fill up the rest with the first vertex
        for (int i = 0; i < unusedVerts; i++)
        {
            ribbonVertex::packed.push_back(ribbonVertex::packed.front());
        }

        anyChange = false;


        if (ribbonVertex::packed.size() > 0)
        {
            int numV = __min(65536 * 8, ribbonVertex::packed.size());
            vertexData->setBlob(ribbonVertex::packed.data(), 0, numV * sizeof(ribbonVertex8));

            uint numB = ribbonVertex::packed.size() / VEG_BLOCK_SIZE;
            for (int j = 0; j < numB; j++)
            {
                blockBuf[j].vertex_offset = VEG_BLOCK_SIZE * j;
                blockBuf[j].instance_idx = 0;
                blockBuf[j].section_idx = 0;
                blockBuf[j].plant_idx = 0;
            }
            totalBlocksToRender = __min(16384 * 32, numB);
            blockData->setBlob(blockBuf.data(), 0, totalBlocksToRender * sizeof(block_data));

            plantBuf[0].radiusScale = ribbonVertex::radiusScale;
            plantBuf[0].scale = ribbonVertex::objectScale;
            plantBuf[0].offset = ribbonVertex::objectOffset;
            plantBuf[0].Ao_depthScale = 0.3f;
            plantBuf[0].bDepth = 1;
            plantBuf[0].bScale = 1;
            plantBuf[0].sunTilt = -0.2f;
            plantBuf[0].shadowUVScale = root->shadowUVScale;
            plantBuf[0].shadowSoftness = root->shadowSoftness;

            plantBuf[0].rootPivot.root = { 0, 0, 0 };
            plantBuf[0].rootPivot.extent = { 0, 1.f / extents.y, 0 };
            plantBuf[0].rootPivot.frequency = root->rootFrequency() * sqrt(extents.y);
            plantBuf[0].rootPivot.stiffness = root->ossilation_stiffness;
            plantBuf[0].rootPivot.shift = root->ossilation_power;

            // but pack other pivots as we step through the plant

            plantData->setBlob(plantBuf.data(), 0, 1 * sizeof(plant));
            plantpivotData->setBlob(ribbonVertex::pivotPoints.data(), pivotOffset, ribbonVertex::pivotPoints.size() * sizeof(_plant_anim_pivot));
        }
        tempUpdateRender = true;
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
    std::string resource = terrafectorEditorMaterial::rootFolder;
    std::string newRelative = PT.parent_path().string() + "/bake_" + PT.stem().string() + "/";
    std::string newDir = resource + newRelative;


    int superSample = 4;

    float W = _info->extents.x * _info->bakeWidth;      // this is half width
    float H = _info->extents.y;
    float H0 = _info->extents.y * _info->bake_V.x;
    float H1 = _info->extents.y * _info->bake_V.y;
    float delH = H1 - H0;

    int iW = 4 * (int)(_info->pixHeight * W * 2.f / delH / 4) * superSample;      // *4  /4 is to keep blocks of 4
    iW = __max(4, iW);
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
    VP = glm::orthoLH(-W, W, H0, H1, -1000.0f, 1000.0f) * V;
    rmcv::mat4 viewproj;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            viewproj[j][i] = VP[j][i];
        }
    }

    bakeShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
    bakeShader.Vars()["gConstantBuffer"]["eyePos"] = float3(0, 0, 100000);  // just very far sort of parallel
    bakeShader.Vars()["gConstantBuffer"]["bake_radius_alpha"] = W;  // just very far sort of parallel
    bakeShader.Vars()["gConstantBuffer"]["bake_height_alpha"] = H1;  // just very far sort of parallel
    bakeShader.Vars()["gConstantBuffer"]["bake_AoToAlbedo"] = _info->bakeAOToAlbedo;



    _plantMaterial::static_materials_veg.setTextures(varBakeTextures);
    _plantMaterial::static_materials_veg.rebuildStructuredBuffer();

    bakeShader.drawInstanced(renderContext, 32, totalBlocksToRender);
    renderContext->flush(true);


    compute_bakeFloodfill.Vars()->setTexture("gAlbedo", fbo->getColorTexture(0));
    compute_bakeFloodfill.Vars()->setTexture("gNormal", fbo->getColorTexture(2));
    compute_bakeFloodfill.Vars()->setTexture("gTranslucency", fbo->getColorTexture(4));
    compute_bakeFloodfill.Vars()->setTexture("gpbr", fbo->getColorTexture(3));

    int numBuffer = 128; // this only gives 16 pixels due to 8x supersampling, consider more
    for (int i = 0; i < numBuffer; i++)
    {
        compute_bakeFloodfill.dispatch(renderContext, iW / 4, iH / 4);
    }

    renderContext->flush(true);

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

        fbo->getColorTexture(0).get()->captureToFile(2, 0, newDir + "_albedo.png", Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::ExportAlpha);
        fbo->getColorTexture(2).get()->captureToFile(2, 0, newDir + "_normal.png", Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);
        fbo->getColorTexture(4).get()->captureToFile(2, 0, newDir + "_translucency.png", Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);

        //fbo->getColorTexture(0).get()->captureToFile(2, 0, resource + Mat.albedoPath, Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::ExportAlpha);
        //fbo->getColorTexture(2).get()->captureToFile(2, 0, resource + Mat.normalPath, Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);
        //fbo->getColorTexture(4).get()->captureToFile(2, 0, resource + Mat.translucencyPath, Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);

        // FIXME total num MIps to build

        int maxHmip = (int)log2(iH / 4);
        int maxWmip = (int)log2(iW / 4);
        int totalMIP = __min(maxHmip, maxWmip) - 2; // -2 for the supersampling
        std::string mipNumber = std::to_string(totalMIP);
        {
            std::string png = newDir + "_albedo.png";
            std::string cmdExp = resource + "Compressonator\\CompressonatorCLI -miplevels " + mipNumber + " \"" + png + "\" " + resource + "Compressonator\\temp_mip.dds";
            replaceAllVEG(cmdExp, "/", "\\");
            system(cmdExp.c_str());
            std::string cmdExp2 = resource + "Compressonator\\CompressonatorCLI -fd BC7 " + resource + "Compressonator\\temp_mip.dds \"" + resource + Mat.albedoPath + "\"";
            replaceAllVEG(cmdExp2, "/", "\\");
            system(cmdExp2.c_str());
            //??? Shall I not rather use my compute compressor here?
        }

        {
            std::string png = newDir + "_normal.png";
            std::string cmdExp = resource + "Compressonator\\CompressonatorCLI -miplevels " + mipNumber + " \"" + png + "\" " + resource + "Compressonator\\temp_mip.dds";
            replaceAllVEG(cmdExp, "/", "\\");
            system(cmdExp.c_str());
            std::string cmdExp2 = resource + "Compressonator\\CompressonatorCLI -fd BC6H " + resource + "Compressonator\\temp_mip.dds \"" + resource + Mat.normalPath + "\"";
            replaceAllVEG(cmdExp2, "/", "\\");
            system(cmdExp2.c_str());
            //??? Shall I not rather use my compute compressor here?
        }

        {
            std::string png = newDir + "_translucency.png";
            std::string cmdExp = resource + "Compressonator\\CompressonatorCLI -miplevels " + mipNumber + " \"" + png + "\" " + resource + "Compressonator\\temp_mip.dds";
            replaceAllVEG(cmdExp, "/", "\\");
            system(cmdExp.c_str());
            std::string cmdExp2 = resource + "Compressonator\\CompressonatorCLI -fd BC7 " + resource + "Compressonator\\temp_mip.dds \"" + resource + Mat.translucencyPath + "\"";
            replaceAllVEG(cmdExp2, "/", "\\");
            system(cmdExp2.c_str());
            //??? Shall I not rather use my compute compressor here?
        }





        Mat._constData.translucency = _info->translucency;
        Mat._constData.alphaPow = _info->alphaPow;
        Mat._constData.roughness[0] = 0.6f;
        Mat._constData.roughness[1] = 0.6f;


        std::ofstream os(resource + _info->material.path);
        cereal::JSONOutputArchive archive(os);
        archive(Mat);
        //Mat.serialize(archive, _PLANTMATERIALVERSION);

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
        _plantMaterial::static_materials_veg.setTextures(varTextures_Gauraud);
        _plantMaterial::static_materials_veg.setTextures(varTextures_Debug_Pivots);
        _plantMaterial::static_materials_veg.setTextures(varTextures_Debug_Pixels);
        _plantMaterial::static_materials_veg.rebuildStructuredBuffer();
        /*
        vegetationShader.State()->setRasterizerState(rasterstate);  //??? move up
        vegetationShader.State()->setBlendState(blendstate);

        vegetationShader_GOURAUD.State()->setRasterizerState(rasterstate);
        vegetationShader_GOURAUD.State()->setBlendState(blendstate);

        vegetationShader_DEBUG_PIVOTS.State()->setRasterizerState(rasterstate);
        vegetationShader_DEBUG_PIVOTS.State()->setBlendState(blendstate);

        vegetationShader_DEBUG_PIXELS.State()->setRasterizerState(rasterstate);
        vegetationShader_DEBUG_PIXELS.State()->setBlendState(blendstate);
        */
        billboardShader.State()->setRasterizerState(rasterstate);
        billboardShader.State()->setBlendState(blendstate);
        _plantMaterial::static_materials_veg.setTextures(varBBTextures);
    }

    if (ribbonVertex::packed.size() > 1 && !displayModeSinglePlant)
    {
        FALCOR_PROFILE("compute_veg_lods");
        compute_clearBuffers.dispatch(_renderContext, 1, 1);

        compute_calulate_lod.Vars()["gConstantBuffer"]["view"] = _view;
        compute_calulate_lod.Vars()["gConstantBuffer"]["frustum"] = _clipFrustum;
        compute_calulate_lod.Vars()["gConstantBuffer"]["lodBias"] = loddingBias;
        compute_calulate_lod.Vars()["gConstantBuffer"]["firstPlant"] = firstPlant;
        compute_calulate_lod.Vars()["gConstantBuffer"]["lastPlant"] = lastPlant;
        compute_calulate_lod.Vars()["gConstantBuffer"]["firstLod"] = firstLod;
        compute_calulate_lod.Vars()["gConstantBuffer"]["lastLod"] = lastLod;
        compute_calulate_lod.dispatch(_renderContext, 16384 / 256, 1);

        _renderContext->copyResource(buffer_feedback_read.get(), buffer_feedback.get());

        const uint8_t* pData = (uint8_t*)buffer_feedback_read->map(Buffer::MapType::Read);
        std::memcpy(&feedback, pData, sizeof(vegetation_feedback));
        buffer_feedback_read->unmap();
    }


    if (ribbonVertex::packed.size() > 1 && !displayModeSinglePlant)
    {
        FALCOR_PROFILE("billboards");

        billboardShader.State()->setFbo(_fbo);
        billboardShader.State()->setViewport(0, _viewport, true);
        billboardShader.State()->setRasterizerState(rasterstate);
        billboardShader.State()->setBlendState(blendstate);

        billboardShader.Vars()["gConstantBuffer"]["viewproj"] = _viewproj;
        billboardShader.Vars()["gConstantBuffer"]["eyePos"] = camPos;

        billboardShader.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);   // it chences every time we loose focus


        billboardShader.renderIndirect(_renderContext, drawArgs_billboards);

        auto& profiler = Profiler::instance();
        auto eventBB = profiler.getEvent("/onFrameRender/vegetation/billboards");
        gputimeBB = eventBB->getGpuTimeAverage();
    }

    if (ribbonVertex::packed.size() > 1)
    {
        FALCOR_PROFILE("ribbonShader");

        static float time = 0.0f;
        time += gpFramework->getFrameRate().getAverageFrameTime() * 0.001f;

        {
            vegetationShader.State()->setFbo(_fbo);
            vegetationShader.State()->setViewport(0, _viewport, true);
            vegetationShader.State()->setRasterizerState(rasterstate);
            vegetationShader.State()->setBlendState(blendstate);
            vegetationShader.Vars()["gConstantBuffer"]["viewproj"] = _viewproj;
            vegetationShader.Vars()["gConstantBuffer"]["eyePos"] = camPos;
            vegetationShader.Vars()["gConstantBuffer"]["time"] = time;
            vegetationShader.Vars()["gConstantBuffer"]["windDir"] = windDir;
            vegetationShader.Vars()["gConstantBuffer"]["windStrength"] = windStrength;
            vegetationShader.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);   // it changes every time we loose focus
        }

        {
            vegetationShader_GOURAUD.State()->setFbo(_fbo);
            vegetationShader_GOURAUD.State()->setViewport(0, _viewport, true);
            vegetationShader_GOURAUD.State()->setRasterizerState(rasterstate);
            vegetationShader_GOURAUD.State()->setBlendState(blendstate);
            vegetationShader_GOURAUD.Vars()["gConstantBuffer"]["viewproj"] = _viewproj;
            vegetationShader_GOURAUD.Vars()["gConstantBuffer"]["eyePos"] = camPos;
            vegetationShader_GOURAUD.Vars()["gConstantBuffer"]["time"] = time;
            vegetationShader_GOURAUD.Vars()["gConstantBuffer"]["windDir"] = windDir;
            vegetationShader_GOURAUD.Vars()["gConstantBuffer"]["windStrength"] = windStrength;
            vegetationShader_GOURAUD.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);   // it changes every time we loose focus
        }

        {
            vegetationShader_DEBUG_PIVOTS.State()->setFbo(_fbo);
            vegetationShader_DEBUG_PIVOTS.State()->setViewport(0, _viewport, true);
            vegetationShader_DEBUG_PIVOTS.State()->setRasterizerState(rasterstate);
            vegetationShader_DEBUG_PIVOTS.State()->setBlendState(blendstate);
            vegetationShader_DEBUG_PIVOTS.Vars()["gConstantBuffer"]["viewproj"] = _viewproj;
            vegetationShader_DEBUG_PIVOTS.Vars()["gConstantBuffer"]["eyePos"] = camPos;
            vegetationShader_DEBUG_PIVOTS.Vars()["gConstantBuffer"]["time"] = time;
            vegetationShader_DEBUG_PIVOTS.Vars()["gConstantBuffer"]["windDir"] = windDir;
            vegetationShader_DEBUG_PIVOTS.Vars()["gConstantBuffer"]["windStrength"] = windStrength;
            vegetationShader_DEBUG_PIVOTS.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);   // it changes every time we loose focus
        }

        {
            vegetationShader_DEBUG_PIXELS.State()->setFbo(_fbo);
            vegetationShader_DEBUG_PIXELS.State()->setViewport(0, _viewport, true);
            vegetationShader_DEBUG_PIXELS.State()->setRasterizerState(rasterstate);
            vegetationShader_DEBUG_PIXELS.State()->setBlendState(blendstate);
            vegetationShader_DEBUG_PIXELS.Vars()["gConstantBuffer"]["viewproj"] = _viewproj;
            vegetationShader_DEBUG_PIXELS.Vars()["gConstantBuffer"]["eyePos"] = camPos;
            vegetationShader_DEBUG_PIXELS.Vars()["gConstantBuffer"]["time"] = time;
            vegetationShader_DEBUG_PIXELS.Vars()["gConstantBuffer"]["windDir"] = windDir;
            vegetationShader_DEBUG_PIXELS.Vars()["gConstantBuffer"]["windStrength"] = windStrength;
            vegetationShader_DEBUG_PIXELS.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);   // it changes every time we loose focus
        }

        //vegetationShader.drawInstanced(_renderContext, ribbonVertex::packed.size(), 1);
        if (displayModeSinglePlant)
        {
            // single plant
            if (showDebugInShader)
                vegetationShader_DEBUG_PIXELS.drawInstanced(_renderContext, 32, totalBlocksToRender);
            else if (showNumPivots)
                vegetationShader_DEBUG_PIVOTS.drawInstanced(_renderContext, 32, totalBlocksToRender);
            else
                vegetationShader.drawInstanced(_renderContext, 32, totalBlocksToRender);
        }
        else
        {
            if (showDebugInShader)
                vegetationShader_DEBUG_PIXELS.renderIndirect(_renderContext, drawArgs_vegetation);
            else if (showNumPivots)
                vegetationShader_DEBUG_PIVOTS.renderIndirect(_renderContext, drawArgs_vegetation);
            else
                vegetationShader.renderIndirect(_renderContext, drawArgs_vegetation);
        }

        auto& profiler = Profiler::instance();
        auto event = profiler.getEvent("/onFrameRender/vegetation/ribbonShader");
        gputime = event->getGpuTimeAverage();
    }
}


void _rootPlant::builInstanceBuffer()
{
    std::uniform_real_distribution<> RND(-1.f, 1.f);

    //const siv::PerlinNoise::seed_type seed = distPerlin(_rootPlant::generator);
    const siv::PerlinNoise perlin{ 100 };

    totalInstances = __min(16384, totalInstances);  // becuase thats the size fo my buffer

    if (cropLines)
    {
        for (int j = 0; j < 64; j++)
        {
            for (int i = 0; i < 256; i++)
            {
                int index = j * 256 + i;

                instanceBuf[index].plant_idx = index % 4;
                //instanceBuf[index].position = { (float)(j - 32) + RND(generator) * 0.01f, 1000.f, (float)(i - 128) * 0.25f + RND(generator) * 0.01f };
                instanceBuf[index].position = { (float)(j - 32) * 1.4 + RND(generator) * 0.1f, 1000.f, (float)(i - 128) * 0.35f + RND(generator) * 0.1f };
                instanceBuf[index].scale = 1.f + RND(generator) * 0.15f;
                instanceBuf[index].rotation = RND(generator) * 3.14f;
                instanceBuf[index].time_offset = RND(generator) * 100;
            }
        }
    }
    else if (numBinaryPlants == 0)
    {
        static float sum = 0;
        float3 pos;
        for (int i = 0; i < 16384; i++)
        {
            while (sum < 1.f)
            {
                pos = { RND(generator) * instanceArea[0], 1000.f, RND(generator) * instanceArea[0] };
                float noise = perlin.octave2D_01(pos.x / 2.f, pos.z / 2.f, 3);
                sum += pow(noise, 3);
            }
            sum -= 1.f;

            instanceBuf[i].plant_idx = i % 4;
            instanceBuf[i].position = pos;
            instanceBuf[i].scale = 1.f + RND(generator) * 0.2f;
            instanceBuf[i].rotation = RND(generator) * 3.14f;
            instanceBuf[i].time_offset = RND(generator) * 100;
        }
    }
    else
    {
        static float sum = 0;
        float3 pos;
        for (int i = 0; i < 16384; i++)
        {
            int type = i % (numBinaryPlants);
            while (sum < 1)
            {
                pos = { RND(generator) * instanceArea[type], 1000.f, RND(generator) * instanceArea[type] };
                float noise = perlin.octave2D_01(pos.x / 2.f + type, pos.z / 2.f, 3);
                sum += pow(noise, 5);
            }
            sum -= 1;
            
            instanceBuf[i].plant_idx = type * 4;
            instanceBuf[i].position = pos;
            instanceBuf[i].scale = 1.f + RND(generator) * 0.2f;
            instanceBuf[i].rotation = RND(generator) * 3.14f;
            instanceBuf[i].time_offset = RND(generator) * 100;
        }
    }
    instanceBuf[0].position = { 0, 1000, 0 };
    instanceBuf[0].scale = 1.f;
    instanceBuf[0].rotation = 0;

    instanceData->setBlob(instanceBuf.data(), 0, 16384 * sizeof(plant_instance));
}
