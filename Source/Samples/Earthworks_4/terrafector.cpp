#include "terrafector.h"

#include "imgui.h"
//#include "imgui_internal.h"

#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/archives/xml.hpp"
#include <sstream>
#include <fstream>
#include <iostream>
#include <filesystem>
#include "roads_materials.h"

//lodTriangleMesh     terrafectorSystem::lod_4_mesh;


#pragma optimize( "", off )


bool forceAllTerrfectorRebuild = false;
bool terrafectorSystem::needsRefresh = false;
lodTriangleMesh_LoadCombiner terrafectorSystem::loadCombine_LOD2;
lodTriangleMesh_LoadCombiner terrafectorSystem::loadCombine_LOD4;
lodTriangleMesh_LoadCombiner terrafectorSystem::loadCombine_LOD6;
lodTriangleMesh_LoadCombiner terrafectorSystem::loadCombine_LOD4_bakeLow;
lodTriangleMesh_LoadCombiner terrafectorSystem::loadCombine_LOD4_bakeHigh;
lodTriangleMesh_LoadCombiner terrafectorSystem::loadCombine_LOD4_overlay;

ecotopeSystem *terrafectorSystem::pEcotopes = nullptr;
//GraphicsProgram::SharedPtr		terrafectorSystem::topdownProgramForBlends = nullptr;
FILE* terrafectorSystem::_logfile;
uint logTab;


materialCache terrafectorEditorMaterial::static_materials;


void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}


//"##1" is BAD
#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {auto& style = ImGui::GetStyle(); \
                                style.Colors[ImGuiCol_Text] = ImVec4(0.50f, 0.5, 0.5, 1.f); \
                                style.Colors[ImGuiCol_PopupBg] = ImVec4(0.00f, 0.f, 0.f, 0.9f);  \
                                ImGui::PushFont(_gui->getFont("roboto_26")); \
                                ImGui::SetTooltip(x); \
                                ImGui::PopFont();}

#define TEST(x)		if( x ) {changed = true;}
#define TEXTURE(idx, tooltip)	ImGui::PushID(9999 + idx); \
								ImGui::PushItemWidth(columnWidth - 30); \
								{ \
									style.Colors[ImGuiCol_Button] = ImVec4(0.01f, 0.05f, 0.08f, 0.9f); \
									if (ImGui::Button((textureIndex[idx]>=0) ? textureNames[idx].c_str() : tfElement[idx].c_str(), ImVec2(columnWidth - 30, 0))) { loadTexture(idx); changed = true;} \
									TOOLTIP( texturePaths[idx].c_str() ); \
									if (ImGui::IsItemHovered()) { terrafectorEditorMaterial::static_materials.dispTexIndex = textureIndex[idx]; } \
									ImGui::SameLine(); \
									if (ImGui::Button("X")) { \
										textureIndex[idx] = -1; \
										texturePaths[idx] = ""; \
										textureNames[idx] = ""; \
									} \
									style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f); \
								} \
								ImGui::PopItemWidth(); \
								ImGui::PopID();


#define SUBMATERIAL(idx, tooltip)	ImGui::PushID(59999 + idx); \
								ImGui::PushItemWidth(columnWidth - 30); \
								{ \
									style.Colors[ImGuiCol_Button] = ImVec4(0.01f, 0.09f, 0.04f, 0.9f); \
									if (ImGui::Button(submaterialPaths[idx].c_str(), ImVec2(columnWidth - 30, 0))) { loadSubMaterial(idx); changed = true;} \
									TOOLTIP( submaterialPaths[idx].c_str() ); \
									ImGui::SameLine(); \
									if (ImGui::Button("X")) { \
										clearSubMaterial(idx); \
									} \
									style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f); \
								} \
								ImGui::PopItemWidth(); \
								ImGui::PopID();



#define HEADER(txt)		 ImGui::PushFont(_gui->getFont("roboto_26")); \
						ImGui::Text(txt); \
						ImGui::PopFont();

#define FLOAT_BOOL(name, val)	bool boolean = val > 0; \
								if (ImGui::Checkbox(name, &boolean)) { val = boolean; changed = true;}

#define HEADER_BOOL(txt, name, toggle)	ImGui::PushFont(_gui->getFont("roboto_26")); \
										ImGui::Text(txt); \
										ImGui::PopFont(); \
										ImGui::SameLine();\
										FLOAT_BOOL(name, toggle)




void tileTriangleBlock::clear()
{
}

void tileTriangleBlock::clearRemapping(uint _size)
{
    remapping.resize(_size);
    for (auto& remap : remapping) {
        remap = -1;
    }
    vertexReuse = 0;
}



void tileTriangleBlock::remapMaterials(uint* _map)
{
    for (auto& V : verts) {
        V.material = _map[V.material];
    }
}



void tileTriangleBlock::insertTriangle(const uint _material, const uint _F[3], const aiMesh* _mesh)
{
    for (int i = 0; i < 3; i++)
    {
        if (remapping[_F[i]] < 0) {
            remapping[_F[i]] = verts.size();
            triVertex V;
            V.pos.x = _mesh->mVertices[_F[i]].x; // SWAP zy - NOT GOOD
            V.pos.y = _mesh->mVertices[_F[i]].z;
            V.pos.z = _mesh->mVertices[_F[i]].y * (-1.0f);

            if (_mesh->HasTextureCoords(0))
            {
                V.uv.x = _mesh->mTextureCoords[0][_F[i]].x;
                V.uv.y = _mesh->mTextureCoords[0][_F[i]].y;
            }

            V.material = _material;


            V.alpha = 1;
            if (_mesh->HasVertexColors(0)) {
                V.alpha = _mesh->mColors[0][_F[i]].r;
            }

            verts.push_back(V);
        }
        else {
            vertexReuse++;
        }

        tempIndexBuffer.push_back(remapping[_F[i]]);
    }
}


void lodTriangleMesh::create(uint _lod)
{
    lod = _lod;
    grid = (uint)pow(2, _lod);
    tileSize = 40000.0f / grid; // FIxeme no boundary and hardcoded for 40k
    bufferSize = tileSize / 248.0f * 4.0f;
    tiles.resize(grid * grid);
    materialNames.clear();
}



void lodTriangleMesh::remapMaterials(uint* _map)
{
    for (auto& tile : tiles) {
        tile.remapMaterials(_map);
    }
}



void lodTriangleMesh::prepForMesh(aiAABB _aabb, uint _size, std::string _name)
{
    for (auto& tile : tiles) {
        tile.clearRemapping(_size);
    }

    materialNames.push_back(_name);

    xMin = (uint)__max(0, floor((_aabb.mMin.x + 20000) / tileSize) - 1);
    xMax = (uint)__min(grid, floor((_aabb.mMax.x + 20000) / tileSize) + 2);
    yMin = (uint)__max(0, floor((-_aabb.mMax.y + 20000) / tileSize) - 1);
    yMax = (uint)__min(grid, floor((-_aabb.mMin.y + 20000) / tileSize) + 2);

    // test for possible YZ flip issues
    float xRange = _aabb.mMax.x - _aabb.mMin.x;
    float yRange = _aabb.mMax.y - _aabb.mMin.y;
    float zRange = _aabb.mMax.z - _aabb.mMin.z;
    if ((_aabb.mMin.z < 0) || (_aabb.mMax.z < 0))
    {
        fprintf(terrafectorSystem::_logfile, "          YZ-flip error likely: z values are negative\n");
    }
    if ((_aabb.mMin.z > 2000) || (_aabb.mMax.z > 2000))
    {
        fprintf(terrafectorSystem::_logfile, "          YZ-flip error likely: z values larger than 2000m\n");
    }
    if (yRange < zRange)
    {
        fprintf(terrafectorSystem::_logfile, "          YZ-flip error likely: yRange < zRange\n");
    }
}



int lodTriangleMesh::insertTriangle(const uint _material, const uint _F[3], const aiMesh* _mesh)
{
    float left, right, top, bottom;
    int count = 0;

    for (int y = yMin; y < yMax; y++)
    {
        bottom = -20000 + (y * tileSize);
        top = bottom + tileSize;
        bottom -= bufferSize;
        top += bufferSize;

        for (int x = xMin; x < xMax; x++)
        {
            left = -20000 + (x * tileSize);
            right = left + tileSize;

            left -= bufferSize;
            right += bufferSize;
            bool flags[4] = { true, true, true, true };

            for (int j = 0; j < 3; j++)     // vertex
            {
                flags[0] &= (_mesh->mVertices[_F[j]].x < left);
                flags[1] &= (_mesh->mVertices[_F[j]].x > right);
                flags[2] &= (-_mesh->mVertices[_F[j]].y < bottom);
                flags[3] &= (-_mesh->mVertices[_F[j]].y > top);
            }

            if (!(flags[0] || flags[1] || flags[2] || flags[3]))
            {
                tiles[y * grid + x].insertTriangle(_material, _F, _mesh);
                count++;
            }
        }
    }
    return count;
}


void lodTriangleMesh::logStats()
{
    int i = 0;
    for (auto& tile : tiles) {
        if (tile.verts.size() > 0) {
            fprintf(terrafectorSystem::_logfile, "\n		(%d, %d) - %d)\n", i >> 4, i & 0xf, (int)tile.verts.size());
            fflush(terrafectorSystem::_logfile);
        }
        i++;
    }
}



void lodTriangleMesh::save(const std::string _path)
{
    std::ofstream os(_path, std::ios::binary);
    cereal::BinaryOutputArchive archive(os);
    serialize(archive, 100);
}



bool lodTriangleMesh::load(const std::string _path)
{
    std::ifstream is(_path, std::ios::binary);
    if (!is.fail())
    {
        cereal::BinaryInputArchive archive(is);
        serialize(archive, 100);
        return true;
    }
    return false;
}



void lodTriangleMesh_LoadCombiner::addMesh(std::string _path, lodTriangleMesh& _mesh)
{
    // load materials and generate mapping
    uint materialRemap[4096];
    uint i = 0;
    for (auto& material : _mesh.materialNames) {
        materialRemap[i] = terrafectorEditorMaterial::static_materials.find_insert_material(_path, material);
        i++;
    }

    // Fix material in place
    _mesh.remapMaterials(materialRemap);

    float3 V[3];
    // Append
    for (uint i = 0; i < tiles.size(); i++)
    {
        uint startIndex = tiles[i].verts.size();
        uint startBuffer = tiles[i].tempIndexBuffer.size();

        tiles[i].verts.insert(tiles[i].verts.end(), _mesh.tiles[i].verts.begin(), _mesh.tiles[i].verts.end());
        tiles[i].tempIndexBuffer.insert(tiles[i].tempIndexBuffer.end(), _mesh.tiles[i].tempIndexBuffer.begin(), _mesh.tiles[i].tempIndexBuffer.end());

        for (uint j = startBuffer; j < tiles[i].tempIndexBuffer.size(); j++)
        {
            tiles[i].tempIndexBuffer[j] += startIndex;
            /*
            uint index = j % 3;
            uint IDX = tiles[i].tempIndexBuffer.back();
            V[j] = tiles[i].verts[IDX].pos;
            if (index == 2)
            {
                float circumferance = glm::length(V[1] - V[0]) + glm::length(V[2] - V[1]) + glm::length(V[0] - V[2]);
                bool bCM = true;
            }
            */
        }

    }
}



void lodTriangleMesh_LoadCombiner::create(uint _lod)
{
    tiles.clear();
    gpuTiles.clear();

    lod = _lod;
    grid = (uint)pow(2, _lod);
    tiles.resize(grid * grid);
    //void loadToGPU();
}



void lodTriangleMesh_LoadCombiner::loadToGPU(std::string _path)
{


    

    gpuTiles.clear();
    gpuTileTerrafector tfTile;
    int mostTri = 0;
    int vertexData = 0;
    int indexData = 0;

    for (auto& tile : tiles)
    {
        tfTile.numVerts = tile.verts.size();
        tfTile.numTriangles = tile.tempIndexBuffer.size();          // this is actualy indicis
        tfTile.numBlocks = (uint)ceil((float)tfTile.numTriangles / (128.0f * 3.0f));
        mostTri = __max(mostTri, tfTile.numTriangles);



        // buffer indicis
        tile.tempIndexBuffer.resize(tfTile.numBlocks * 128 * 3);
        for (uint i = tfTile.numTriangles; i < tfTile.numBlocks * 128 * 3; i++)
        {
            tile.tempIndexBuffer[i] = 0;
        }

        if (tfTile.numBlocks > 0)
        {
            tfTile.vertex = Buffer::createStructured(
                sizeof(triVertex),
                tfTile.numVerts,
                Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess,
                Buffer::CpuAccess::None,
                tile.verts.data());

            tfTile.index = Buffer::createStructured(
                sizeof(unsigned int),
                tfTile.numBlocks * 128 * 3,
                Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess,
                Buffer::CpuAccess::None,
                tile.tempIndexBuffer.data());

            vertexData += sizeof(triVertex) * tfTile.numVerts;
            indexData += sizeof(unsigned int) * tfTile.numBlocks * 128 * 3;
        }



        gpuTiles.push_back(tfTile);
    }



    if (_path.size() > 0)
    {
        FILE* file = fopen(_path.c_str(), "wb");
        if (file)
        {
            fwrite(&lod, sizeof(uint), 1, file);
            for (uint i=0; i<tiles.size(); i++)
            {
                fwrite(&gpuTiles[i].numVerts, sizeof(uint), 1, file);
                fwrite(&gpuTiles[i].numTriangles, sizeof(uint), 1, file);
                fwrite(&gpuTiles[i].numBlocks, sizeof(uint), 1, file);
                if (gpuTiles[i].numBlocks > 0)
                {
                    fwrite(tiles[i].verts.data(), sizeof(triVertex), gpuTiles[i].numVerts, file);
                    fwrite(tiles[i].tempIndexBuffer.data(), sizeof(uint), gpuTiles[i].numBlocks * 128 * 3, file);
                }
                
                if (i%(int)(pow(2, lod)) == 0)fprintf(terrafectorSystem::_logfile, "\n");
                fprintf(terrafectorSystem::_logfile, "%7.d ", gpuTiles[i].numVerts);
            }
            fclose(file);
        }
    }


    // now release all CPU memory
    tiles.clear();

    fprintf(terrafectorSystem::_logfile, "\n  lod %d  %dx%d\n", lod, grid, grid);
    fprintf(terrafectorSystem::_logfile, "  block with most triangles has %d\n", mostTri / 3);
    fprintf(terrafectorSystem::_logfile, "  %d Mb VB   %d Mb IB\n\n", vertexData / 1024 / 1024, indexData / 1024 / 1024);



}



materialCache::materialCache()
{
    //   sb_Terrafector_Materials = Buffer::createStructured(sizeof(TF_material), 1024);
}



uint materialCache::find_insert_material(const std::string _path, const std::string _name)
{
    logTab++;
    std::filesystem::path fullPath = _path + _name + ".terrafectorMaterial";
    if (std::filesystem::exists(fullPath))
    {
        logTab--;
        return find_insert_material(fullPath);
    }
    else
    {
        // Now we have to seacrh, but use the first one we find
        std::filesystem::path rootpath = fullPath = terrafectorEditorMaterial::rootFolder + "/terrafectorMaterials";
        for (const auto& entry : std::filesystem::recursive_directory_iterator(fullPath))
        {
            std::string newPath = entry.path().string();
            replaceAll(newPath, "\\", "/");
            if (newPath.find(_name) != std::string::npos)
            {
                logTab--;
                return find_insert_material(newPath);
            }
        }
    }

    // we got here, not good
    for (uint i = 0; i < logTab; i++)   fprintf(terrafectorSystem::_logfile, "  ");
    fprintf(terrafectorSystem::_logfile, "error : material - %s does not exist\n", _name.c_str());
    fflush(terrafectorSystem::_logfile);

    logTab--;
    return 0;
}



uint materialCache::find_insert_material(const std::filesystem::path _path)
{
    logTab++;
    for (uint i = 0; i < materialVector.size(); i++)
    {
        if (materialVector[i].fullPath.compare(_path) == 0)
        {
            //for (uint i = 0; i < logTab; i++)   fprintf(terrafectorSystem::_logfile, "  ");
            //fprintf(terrafectorSystem::_logfile, "materialCache - found (%s)\n", _path.filename().string().c_str());
            logTab--;

            if (!materialVector[i].thumbnail) {
                materialVector[i].thumbnail = Texture::createFromFile(_path.string() + ".jpg", false, true);
            }

            return i;
        }
    }

    // else add new


    uint materialIndex = (uint)materialVector.size();
    materialVector.emplace_back();
    terrafectorEditorMaterial& material = materialVector.back();
    material.import(_path);
    material.thumbnail = Texture::createFromFile(_path.string() + ".jpg", false, true);

    for (uint i = 0; i < logTab; i++)   fprintf(terrafectorSystem::_logfile, "  ");
    fprintf(terrafectorSystem::_logfile, "add Material[%d] - %s\n", materialIndex, _path.filename().string().c_str());
    fflush(terrafectorSystem::_logfile);

    logTab--;
    return materialIndex;
}


int materialCache::find_insert_texture(const std::filesystem::path _path, bool isSRGB)
{
    logTab++;
    for (uint i = 0; i < textureVector.size(); i++)
    {
        if (textureVector[i]->getSourcePath().compare(_path) == 0)
        {
            logTab--;
            return i;
        }
    }

    /*
    * std::string cmdExp = "explorer " + terrafectorEditorMaterial::rootFolder + material.relativePath;
                cmdExp = cmdExp.substr(0, cmdExp.find_last_of("\\/") + 1);
                replaceAllrm(cmdExp, "/", "\\");
                system(cmdExp.c_str());
    */
    std::string ddsFilename = _path.string() + ".earthworks.dds";
    if (!std::filesystem::exists(ddsFilename))
    {
        std::string pathOnly = ddsFilename.substr(0, ddsFilename.find_last_of("\\/") + 1);
        std::string cmdExp = "X:\\resources\\Compressonator\\CompressonatorCLI -miplevels 6 \"" + _path.string() + "\" " + "X:\\resources\\Compressonator\\temp_mip.dds";

        fprintf(terrafectorSystem::_logfile, "%s\n", cmdExp.c_str());
        system(cmdExp.c_str());
        if (isSRGB)
        {
            std::string cmdExp2 = "X:\\resources\\Compressonator\\CompressonatorCLI -fd BC6H  X:\\resources\\Compressonator\\temp_mip.dds \"" + ddsFilename + "\"";
            system(cmdExp2.c_str());
        }
        else
        {
            std::string cmdExp2 = "X:\\resources\\Compressonator\\CompressonatorCLI -fd BC7 -Quality 0.01 X:\\resources\\Compressonator\\temp_mip.dds " + ddsFilename + "\"";
            system(cmdExp2.c_str());
        }
    }
    Texture::SharedPtr tex = Texture::createFromFile(ddsFilename, true, isSRGB);
    if (tex)
    {
        tex->setSourcePath(_path);
        tex->setName(_path.filename().string());
        textureVector.emplace_back(tex);

        float compression = 4.0f;
        if (isSRGB) compression = 4.0f;

        texMb += (float)(tex->getWidth() * tex->getHeight() * 4.0f * 1.333f) / 1024.0f / 1024.0f / compression;	// for 4:1 compression + MIPS

        for (uint i = 0; i < logTab; i++)   fprintf(terrafectorSystem::_logfile, "  ");
        fprintf(terrafectorSystem::_logfile, "%s\n", tex->getName().c_str());
        fflush(terrafectorSystem::_logfile);

        logTab--;
        return (uint)(textureVector.size() - 1);
    }
    else
    {
        for (uint i = 0; i < logTab; i++)   fprintf(terrafectorSystem::_logfile, "  ");
        fprintf(terrafectorSystem::_logfile, "failed %s \n", _path.string().c_str());
        fflush(terrafectorSystem::_logfile);

        logTab--;
        return -1;
    }
}


Texture::SharedPtr materialCache::getDisplayTexture()
{
    if (dispTexIndex >= 0) {
        return textureVector.at(dispTexIndex);
    }
    else {
        return nullptr;
    }
}



void materialCache::setTextures(ShaderVar& _var)
{
    for (size_t i = 0; i < textureVector.size(); i++)
    {
        _var[i] = textureVector[i];
    }
}


// FIXME could also just do the individual one
void materialCache::rebuildStructuredBuffer()
{
    size_t offset = 0;
    for (auto& mat : materialVector)
    {
        sb_Terrafector_Materials->setBlob(&mat._constData, offset, sizeof(TF_material));
        offset += sizeof(TF_material);
    }
}


void materialCache::rebuildAll()
{
    for (auto& mat : materialVector)
    {
        if (mat._constData.materialType == 1)
        {
            for (uint idx = 0; idx < 8; idx++)
            {
                mat._constData.subMaterials[idx] &= 0x00ff0000;

                if (mat.submaterialPaths[idx].size())
                {
                    uint matIdx = terrafectorEditorMaterial::static_materials.find_insert_material(terrafectorEditorMaterial::rootFolder + mat.submaterialPaths[idx]);
                    mat._constData.subMaterials[idx] &= 0xffff0000;
                    mat._constData.subMaterials[idx] += matIdx;
                }
            }
        }

        mat.rebuildConstantbufferData();
    }

    rebuildStructuredBuffer();
}



void materialCache::reFindMaterial(std::filesystem::path currentPath)
{
    /*
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"terrafectorMaterial"} };
    if (saveFileDialog(filters, path))
    {
        for (auto& material : materialVector)
        {
            if (material.fullPath.compare(currentPath) == 0)
            {
                material.fullPath = currentPath;
            }
        }

        roadMaterialCache::getInstance()
    }
    */
}


void materialCache::renameMoveMaterial(terrafectorEditorMaterial& _material)
{
    std::string windowspath = _material.fullPath.string();     // to open in that directory
    replaceAll(windowspath, "//", "/");
    replaceAll(windowspath, "/", "\\");
    std::filesystem::path path = windowspath;   // feels unnesesary


    FileDialogFilterVec filters = { {"terrafectorMaterial"} };
    if (saveFileDialog(filters, path))
    {
        std::string filepath = path.string();
        replaceAll(filepath, "\\", "/");

        if (filepath.find(terrafectorEditorMaterial::rootFolder) == 0)
        {


            std::string relativeOldDoubleSlash = _material.fullPath.string().substr(terrafectorEditorMaterial::rootFolder.length());
            replaceAll(relativeOldDoubleSlash, "/", "//");
            std::string relativePath = filepath.substr(terrafectorEditorMaterial::rootFolder.length());

            // replace if used by a multi material
            for (auto& material : materialVector)
            {
                if (material._constData.materialType == 1)
                {
                    bool changed = false;
                    for (uint i = 0; i < 8; i++)
                    {
                        if (material.submaterialPaths[i].find(relativeOldDoubleSlash) == 0)
                        {
                            material.submaterialPaths[i] = relativePath;
                            changed = true;
                        }
                    }

                    if (changed)
                    {
                        material.save();
                    }
                }
            }


            std::string relativeOld = _material.fullPath.string().substr(terrafectorEditorMaterial::rootFolder.length());
            //replaceAll(relativeOld, "/", "//");
            for (auto& roadMat : roadMaterialCache::getInstance().materialVector)
            {
                bool changed = false;
                for (auto& layer : roadMat.layers)
                {
                    if (layer.material.find(relativeOld) == 0)
                    {
                        changed = true;
                        layer.material = relativePath;
                    }
                }

                if (changed)
                {
                    roadMat.save();
                }
            }


            std::filesystem::rename(_material.fullPath.string(), filepath);
            std::filesystem::rename(_material.fullPath.string() + ".jpg", filepath + ".jpg");
            _material.fullPath = filepath;
        }

        // now seach and replace everything

        // now search and replace this name in everyhtign
        //_material.serialize()
        /*
        if (filepath.find(terrafectorEditorMaterial::rootFolder) == 0)
        {
            std::filesystem::rename(terrafectorEditorMaterial::rootFolder + _material.relativePath, filepath);
            std::filesystem::rename(terrafectorEditorMaterial::rootFolder + _material.relativePath + ".jpg", filepath + ".jpg");

            _material.relativePath = filepath.substr(terrafectorEditorMaterial::rootFolder.length());
            _material.save();
        }*/
    }
}


void materialCache::renderGui(Gui* mpGui, Gui::Window& _window)
{
    auto& style = ImGui::GetStyle();
    ImGuiStyle oldStyle = style;
    float width = ImGui::GetWindowWidth();
    int numColumns = __max(2, (int)floor(width / 140));

    ImGui::PushItemWidth(128);

    if (ImGui::Button("rebuild")) {
        rebuildAll();
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
        S.name = material.fullPath.string().substr(terrafectorEditorMaterial::rootFolder.length());
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
            if (displaySortMap[cnt].name.find("terrafectorMaterials") != std::string::npos)
            {
                thisPath = displaySortMap[cnt].name.substr(21, displaySortMap[cnt].name.find_last_of("\\/") - 21);
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
            terrafectorEditorMaterial& material = materialVector[displaySortMap[cnt].index];

            ImGui::PushID(777 + cnt);
            {
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
                    if (ImGui::Selectable("rename / move")) { renameMoveMaterial(material); }

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


            }
            ImGui::PopID();
            subCount++;
        }
    }




    style = oldStyle;
}



void materialCache::doEvoTextureImportTest()
{
    for (uint i = 0; i < textureVector.size(); i++)
    {

    }
}



void materialCache::renderGuiTextures(Gui* mpGui, Gui::Window& _window)
{
    auto& style = ImGui::GetStyle();
    ImGuiStyle oldStyle = style;
    float width = ImGui::GetWindowWidth();
    int numColumns = __max(2, (int)floor(width / 140));


    char text[1024];
    ImGui::PushFont(mpGui->getFont("roboto_26"));
    ImGui::Text("Tex [%d]   %3.1fMb", (int)textureVector.size(), texMb);
    if (ImGui::Button("Text EVO status")) {
        doEvoTextureImportTest();
    }
    ImGui::PopFont();


    ImGui::PushFont(mpGui->getFont("roboto_20"));
    {
        for (uint i = 0; i < textureVector.size(); i++)
        {
            ImGui::NewLine();
            ImGui::SameLine(40);

            Texture* pT = textureVector[i].get();
            sprintf(text, "%s##%d", pT->getName().c_str(), i);

            if (ImGui::Selectable(text))           {         }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%d  :  %s\n%d x %d", i, pT->getSourcePath().c_str(), pT->getWidth(), pT->getHeight());
                terrafectorEditorMaterial::static_materials.dispTexIndex = i;

                //if (ImGui::IsMouseClicked(1)) {
                //	ImGui::OpenPopupEx(text, false);
                //}
            }

            //if (ImGui::BeginPopup(text)) {
            if (ImGui::BeginPopupContextItem())
            {
                std::string cmdExp = "explorer " + pT->getSourcePath().string();
                std::string cmdPs = "\"C:\\Program Files\\Adobe\\Adobe Photoshop 2022\\Photoshop.exe\" " + pT->getSourcePath().string();
                if (ImGui::Selectable("Explorer")) { system(cmdExp.c_str()); }
                if (ImGui::Selectable("Photoshop")) { system(cmdPs.c_str()); }
                ImGui::Separator();

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

                ImGui::EndPopup();
            }

        }
    }
    ImGui::PopFont();

    style = oldStyle;
}



bool materialCache::renderGuiSelect(Gui* mpGui)
{
    if (selectedMaterial >= 0 && selectedMaterial < materialVector.size())
    {
        return materialVector[selectedMaterial].renderGUI(mpGui);
    }

    return false;
}



bool terrafectorElement::isMeshCached(const std::string _path)
{
    if (!std::filesystem::exists(_path + ".lod4Cache"))
    {
        return false;
    }
    auto timeFile = std::filesystem::last_write_time(_path);
    auto timeCache = std::filesystem::last_write_time(_path + ".lod4Cache");
    if (timeFile < timeCache);
    {
        return true;
    }
    return false;
}



void terrafectorElement::splitAndCacheMesh(const std::string _path)
{
    Assimp::Importer importer;
    lodTriangleMesh lodder_2;
    lodTriangleMesh lodder_4;
    lodTriangleMesh lodder_6;
    lodder_2.create(2);
    lodder_4.create(4);
    lodder_6.create(6);

    bool useLOD2 = _path.find("LOD2") != std::string::npos;
    bool bakeOnlyOverlay = (_path.find("BAKE_ONLY") != std::string::npos) || (_path.find("OVERLAY") != std::string::npos);
    int num2 = 0;
    int num4 = 0;
    int num6 = 0;

    unsigned int flags =
        aiProcess_FlipUVs |
        aiProcess_Triangulate |
        aiProcess_PreTransformVertices |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenBoundingBoxes;

    const aiScene* scene = nullptr;
    scene = importer.ReadFile(_path.c_str(), flags);

    if (scene)
    {
        glm::int4 index;
        triVertex V[3];
        for (uint i = 0; i < scene->mNumMeshes; i++)
        {
            aiMesh* M = scene->mMeshes[i];

            lodder_2.prepForMesh(M->mAABB, M->mNumVertices, scene->mMaterials[M->mMaterialIndex]->GetName().C_Str());
            lodder_4.prepForMesh(M->mAABB, M->mNumVertices, scene->mMaterials[M->mMaterialIndex]->GetName().C_Str());
            lodder_6.prepForMesh(M->mAABB, M->mNumVertices, scene->mMaterials[M->mMaterialIndex]->GetName().C_Str());
            for (uint j = 0; j < M->mNumFaces; j++)
            {
                if (!bakeOnlyOverlay && useLOD2)    num2 += lodder_2.insertTriangle(i, M->mFaces[j].mIndices, M);
                num4 += lodder_4.insertTriangle(i, M->mFaces[j].mIndices, M);
                if (!bakeOnlyOverlay) num6 += lodder_6.insertTriangle(i, M->mFaces[j].mIndices, M);
            }
        }


        if (num2 > 0) lodder_2.save(_path + ".lod2Cache");
        if (num4 > 0) lodder_4.save(_path + ".lod4Cache");
        if (num6 > 0) lodder_6.save(_path + ".lod6Cache");
        std::filesystem::path P = _path;
        if (num2 > 0) terrafectorSystem::loadCombine_LOD2.addMesh(P.parent_path().string() + "/", lodder_2);
        if (num6 > 0) terrafectorSystem::loadCombine_LOD6.addMesh(P.parent_path().string() + "/", lodder_6);

        if (num4 > 0)
        {
            if (_path.find("BAKE_ONLY_BOTTOM") != std::string::npos)
            {
                terrafectorSystem::loadCombine_LOD4_bakeLow.addMesh(P.parent_path().string() + "/", lodder_4);
            }
            else if (_path.find("BAKE_ONLY_TOP") != std::string::npos)
            {
                terrafectorSystem::loadCombine_LOD4_bakeHigh.addMesh(P.parent_path().string() + "/", lodder_4);
            }
            else if (_path.find("OVERLAY") != std::string::npos)
            {
                terrafectorSystem::loadCombine_LOD4_overlay.addMesh(P.parent_path().string() + "/", lodder_4);
            }
            else
            {
                terrafectorSystem::loadCombine_LOD4.addMesh(P.parent_path().string() + "/", lodder_4);
            }
        }

        for (auto& matName : lodder_4.materialNames)
        {
            subMesh S;
            S.materialName = matName;
            S.index = terrafectorEditorMaterial::static_materials.find_insert_material(P.parent_path().string() + "/", matName);
            materials.push_back(S);
        }

    }
}



terrafectorElement& terrafectorElement::find_insert(const std::string _name, tfTypes _type, const std::string _path)
{
    logTab++;
    for (int i = 0; i < children.size(); i++)
    {
        if (children[i].name.compare(_name) == 0)
        {
            return children.at(i);
        }
    }

    children.emplace_back(_type, _name);
    if (_name.find("BAKE_ONLY") != std::string::npos) {
        children.back().bakeOnly = true;
    }

    if (_type == tf_heading)
    {
        for (uint i = 0; i < logTab; i++)   fprintf(terrafectorSystem::_logfile, "  ");
        fprintf(terrafectorSystem::_logfile, "%s\n", _name.c_str());
        fflush(terrafectorSystem::_logfile);
    }

    if (_type == tf_fbx)
    {
        terrafectorElement& me = children.back();
        me.path = _path;
        me.name = _path.substr(_path.find_last_of("\\/") + 1, _path.size());
        //std::string fullName = terrafectorEditorMaterial::rootFolder + _path;
        std::string fullName = _path;
        std::string fullPath = fullName.substr(0, fullName.find_last_of("\\/") + 1);

        for (uint i = 0; i < logTab; i++)   fprintf(terrafectorSystem::_logfile, "  ");
        fprintf(terrafectorSystem::_logfile, "add mesh - %s\n", fullName.c_str());
        fflush(terrafectorSystem::_logfile);


        if (forceAllTerrfectorRebuild || !isMeshCached(_path)) {
            splitAndCacheMesh(_path);
        }
        else
        {
            lodTriangleMesh lodder2;
            lodTriangleMesh lodder4;
            lodTriangleMesh lodder6;
            bool use2 = lodder2.load(_path + ".lod2Cache");
            bool use4 = lodder4.load(_path + ".lod4Cache");
            bool use6 = lodder6.load(_path + ".lod6Cache");

            std::filesystem::path P = _path;
            if (use2)   terrafectorSystem::loadCombine_LOD2.addMesh(P.parent_path().string() + "/", lodder2);
            if (use6)   terrafectorSystem::loadCombine_LOD6.addMesh(P.parent_path().string() + "/", lodder6);
            if (use4)
            {
                if (_path.find("BAKE_ONLY_BOTTOM") != std::string::npos)
                {
                    terrafectorSystem::loadCombine_LOD4_bakeLow.addMesh(P.parent_path().string() + "/", lodder4);
                }
                else if (_path.find("BAKE_ONLY_TOP") != std::string::npos)
                {
                    terrafectorSystem::loadCombine_LOD4_bakeHigh.addMesh(P.parent_path().string() + "/", lodder4);
                }
                else if (_path.find("OVERLAY") != std::string::npos)
                {
                    terrafectorSystem::loadCombine_LOD4_overlay.addMesh(P.parent_path().string() + "/", lodder4);
                }
                else
                {
                    terrafectorSystem::loadCombine_LOD4.addMesh(P.parent_path().string() + "/", lodder4);
                }
            }

            for (auto& matName : lodder4.materialNames)
            {
                subMesh S;
                S.materialName = matName;
                S.index = terrafectorEditorMaterial::static_materials.find_insert_material(P.parent_path().string() + "/", matName);
                me.materials.push_back(S);
            }
        }

    }

    logTab--;
    return children.back();
}


void terrafectorElement::renderGui(Gui* _pGui, float tab)
{
    auto& style = ImGui::GetStyle();
    float y = ImGui::GetCursorPosY();

    if (bExpanded) {
        for (int i = (int)children.size() - 1; i >= 0; i--)
        {
            children.at(i).renderGui(_pGui, tab + 30.0f);
        }
        /*
        if (children.size() > 1) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            ImVec2 A = window->Rect().GetTL();
            A.x += tab + 25;
            A.y += y - 10;
            float delY = ImGui::GetCursorPosY() - y;
            ImVec2 B = A;
            B.y += delY - 10;
            window->DrawList->AddLine(A, B, ImColor(100, 100, 100, 255), 1.0f);
        }
        */
    }

    ImGui::SameLine(tab);
    switch (type) {
    case tf_heading:
        style.Colors[ImGuiCol_Text] = ImVec4(1.f, 1.f, 1.f, 1.f);
        ImGui::PushFont(_pGui->getFont("roboto_20"));
        // fime unique name add counter
        // if not expanded show number fo children
        if (ImGui::Selectable(name.c_str())) {
            bExpanded = !bExpanded;
        }
        ImGui::PopFont();
        break;
    case tf_fbx:
        style.Colors[ImGuiCol_Text] = ImVec4(1.f, 0.5f, 0.2f, 0.7f);
        if (ImGui::Selectable(name.c_str())) {
            bExpanded = !bExpanded;
        }
        if (bExpanded)
        {
            style.Colors[ImGuiCol_Text] = ImVec4(0.1f, 0.7f, 0.4f, 0.7f);
            //static uint CNT = 0;
            //ImGui::PushID(CNT);
            for (uint i = 0; i < materials.size(); i++)
                //for (uint i = (int)materials.size()-1; i >= 0 ; i--)
            {
                ImGui::NewLine();
                ImGui::SameLine(tab + 40);
                if (ImGui::Selectable((materials[i].materialName + "##" + name).c_str()))
                {
                    terrafectorEditorMaterial::static_materials.selectedMaterial = materials[i].index;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%d  :  %s\nMAT %d   :   %s\n", materials[i].sortIndex, materials[i].materialName.c_str(), materials[i].index, terrafectorEditorMaterial::static_materials.materialVector.at(materials[i].index).displayName.c_str());
                }
            }
            //ImGui::PopID();
            //CNT++;
            //if (CNT > 100000) CNT = 0;
        }
        break;
    }

    ImGui::NewLine();
}



void terrafectorElement::loadPath(std::string _path)
{
    for (const auto& entry : std::filesystem::directory_iterator(_path))
    {
        std::string newPath = entry.path().string();
        //replaceAll(newPath, "\\", "/");

        if (entry.is_directory())
        {
            terrafectorElement& e = find_insert(entry.path().filename().string());
            e.loadPath(newPath);
        }
        else
        {
            std::string ext = entry.path().extension().string();
            if (ext.find("fbx") != std::string::npos ||
                ext.find("obj") != std::string::npos) {
                terrafectorElement& e = find_insert(entry.path().filename().string(), tf_fbx, newPath);
            }
        }

    }
}


// class terrafectorMaterial ######################################################################################################################################################

std::string terrafectorEditorMaterial::rootFolder;

terrafectorEditorMaterial::terrafectorEditorMaterial() {
    for (int i = 0; i < 15; i++)
        for (int j = 0; j < 4; j++)
            _constData.ecotopeMasks[i][j] = 0;
}

terrafectorEditorMaterial::~terrafectorEditorMaterial() {
}

// unique hash of blendstates 
uint32_t terrafectorEditorMaterial::blendHash() {
    uint32_t hash = 0;
    return hash;
}

void terrafectorEditorMaterial::import(std::filesystem::path  _path, bool _replacePath) {

    //_path += "_xml";
    std::ifstream is(_path);
    if (is.fail()) {
        displayName = "failed to load";
        fullPath = _path;
    }
    else
    {
        cereal::JSONInputArchive archive(is);
        serialize(archive);

        if (_replacePath) fullPath = _path;
        reloadTextures();
        isModified = false;
    }
}

void terrafectorEditorMaterial::import(bool _replacePath) {
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"terrafectorMaterial"} };
    if (openFileDialog(filters, path))
    {
        import(path, _replacePath);
    }
}



void terrafectorEditorMaterial::save()
{
    std::ofstream os(fullPath);
    cereal::JSONOutputArchive archive(os);
    serialize(archive);
}


// FIXME has to take root path into account
void terrafectorEditorMaterial::reloadTextures()
{
    for (int idx = 0; idx < tfTextureSize; idx++) {
        if (texturePaths[idx].size()) {
            char txt[1000];
            sprintf(txt, "%s", (rootFolder + texturePaths[idx]).c_str());

            textureIndex[idx] = terrafectorEditorMaterial::static_materials.find_insert_texture(rootFolder + texturePaths[idx], tfSRGB[idx]);
            textureNames[idx] = texturePaths[idx].substr(texturePaths[idx].find_last_of("\\/") + 1);
        }
    }
}


void terrafectorEditorMaterial::eXport(std::filesystem::path _path) {
    {
        std::ofstream os(_path);
        cereal::JSONOutputArchive archive(os);
        serialize(archive);
        isModified = false;
    }
}

void terrafectorEditorMaterial::eXport() {
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"terrafectorMaterial"} };
    if (saveFileDialog(filters, path))
    {
        eXport(path);
    }
}


void terrafectorEditorMaterial::loadTexture(int idx)
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"png"}, {"jpg"}, {"tga"}, {"exr"}, {"dds"} };
    if (openFileDialog(filters, path))
    {
        std::string P = path.string();
        replaceAll(P, "\\", "/");
        if (P.find(rootFolder) == 0) {
            std::string relative = P.substr(rootFolder.length());
            textureIndex[idx] = terrafectorEditorMaterial::static_materials.find_insert_texture(P, tfSRGB[idx]);
            textureNames[idx] = path.filename().string();
            texturePaths[idx] = relative;
        }
    }
}



void terrafectorEditorMaterial::loadSubMaterial(int idx)
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"terrafectorMaterial"} };
    if (openFileDialog(filters, path))
    {
        std::string P = path.string();
        replaceAll(P, "\\", "/");
        if (P.find(rootFolder) == 0) {
            std::string relative = P.substr(rootFolder.length());
            submaterialPaths[idx] = relative;

            uint mat = terrafectorEditorMaterial::static_materials.find_insert_material(rootFolder + relative);
            _constData.subMaterials[idx] = 0x00ff0000 + mat;
            rebuildConstantbuffer();
        }
    }
}


void terrafectorEditorMaterial::clearSubMaterial(int idx)
{
    submaterialPaths[idx] = "";
    _constData.subMaterials[idx] = 0;
    rebuildConstantbuffer();
}



bool terrafectorEditorMaterial::renderGUI(Gui* _gui)
{
    bool changed = false;

    float columnWidth = ImGui::GetWindowWidth() / 3 - 20;


#define HI(v)   ImVec4(0.502f, 0.075f, 0.256f, v)
#define MED(v)  ImVec4(0.455f, 0.198f, 0.301f, v)
#define LOW(v)  ImVec4(0.232f, 0.201f, 0.271f, v)
#define BG(v)   ImVec4(0.200f, 0.220f, 0.270f, v)
#define TEXTG(v) ImVec4(0.860f, 0.930f, 0.890f, v)

    auto& style = ImGui::GetStyle();
    ImGuiStyle oldStyle = style;
    //style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.02f, 0.01f, 0.80f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.17f, 0.70f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.14f, 0.17f, 0.90f);

    style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.5f);
    style.Colors[ImGuiCol_ButtonHovered] = MED(0.86f);

    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

    style.FrameRounding = 4.0f;



    ImGui::PushFont(_gui->getFont("roboto_32"));
    {
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        char txt[256];
        sprintf(txt, "%s", displayName.c_str());
        if (ImGui::InputText("##name", txt, 256))
        {
            displayName = txt;
        }
        TOOLTIP(fullPath.string().c_str());
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
    }
    ImGui::PopFont();
    ImGui::PushFont(_gui->getFont("roboto_20"));
    {
        ImGui::SameLine();
        if (isModified)
        {
            style.Colors[ImGuiCol_Button] = ImVec4(0.99f, 0.17f, 0.13f, 0.9f);
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);
            if (ImGui::Button("Save", ImVec2(80, 0))) { eXport(fullPath); }
            TOOLTIP(fullPath.string().c_str());
            style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.5f);
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 100);
        if (ImGui::Button("Load", ImVec2(80, 0))) { import(); }
        TOOLTIP("Load a new file - replacing the current\n You will edit somethign else");


        ImGui::PushItemWidth(columnWidth);
        {
            if (ImGui::Combo("##mattype", &_constData.materialType, "material\0multi - material\0rubber\0puddle\0")) { ; }
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);
        if (ImGui::Button("Save copy", ImVec2(80, 0))) { eXport(); }
        TOOLTIP("Save to a diffirent file");

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 100);
        if (ImGui::Button("Import", ImVec2(80, 0))) { import(false); changed = true; }
        TOOLTIP("Load another files contents into this file");
    }
    ImGui::PopFont();





    ImGui::NewLine();

    ImGui::Columns(3);
    {
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.1f, 0.1f, 0.5f);
        ImGui::BeginChildFrame(123450, ImVec2(columnWidth, 260));
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
        {
            HEADER("uv");
            {
                changed |= ImGui::DragFloat2("scale", &_constData.uvScale.x, 0.1f, -10.0f, 10.0f);
                TOOLTIP("base uv scale");

                float R = _constData.uvRotation * 57.2958f;

                if (ImGui::DragFloat("rotation", &R, 1.f, 0.0f, 360.f, "%3.1f"))
                {
                    changed |= true;
                    _constData.uvRotation = R / 57.2958f;
                }
                TOOLTIP("radians : 1.57079632679489 = 90 degrees");

                ImGui::NewLine();

                changed |= ImGui::DragFloat("world size", &_constData.worldSize, 0.1f, 1.0f, 32.0f, "%2.2f m");
                TOOLTIP("detail texture world size");

                R = _constData.uvWorldRotation * 57.2958f;
                if (ImGui::DragFloat("world rotation", &R, 1.f, 0.0f, 360.f, "%3.1f"))
                {
                    changed |= true;
                    _constData.uvWorldRotation = R / 57.2958f;
                }
                TOOLTIP("radians : 1.57079632679489 = 90 degrees");

                ImGui::NewLine();

                FLOAT_BOOL("bezier - soft edges", _constData.baseAlphaClampU);
                TOOLTIP("clamp u for edge masks");
                if (_constData.baseAlphaClampU) {
                    changed |= ImGui::DragFloat2("uv_clamped", &_constData.uvScaleClampAlpha.x, 0.01f, 0.0f, 1.0f);
                    TOOLTIP("alpha clamp uv scale\nThis is ONLY applied to the base alpha texture when it is also clamped in U");
                }
            }
        }
        ImGui::EndChildFrame();



        ImGui::NewLine();
        style.Colors[ImGuiCol_Border] = ImVec4(0.5f, 0.5f, 0.5f, 0.7f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.01f, 0.01f, 0.01f, 0.7f);
        ImGui::BeginChildFrame(123456, ImVec2(columnWidth, 390));
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
        {
            HEADER_BOOL("alpha", "##useAlpha", _constData.useAlpha);
            if (_constData.useAlpha)
            {
                ImGui::SameLine(columnWidth / 2);
                FLOAT_BOOL("debug", _constData.debugAlpha);

                ImGui::NewLine();
                ImGui::Text("vertex colour (red)");
                changed |= ImGui::SliderFloat("##vertexalpha", &_constData.vertexAlphaScale, 0, 1, "%.2f");
                TOOLTIP("vertex alpha influence - packed in red");

                ImGui::NewLine();
                float Y0 = ImGui::GetCursorPosY();
                ImGui::Text("base texture");
                changed |= ImGui::SliderFloat("##texturealpha", &_constData.baseAlphaScale, 0, 1, "%.2f");
                TOOLTIP("texture alpha influence");
                if (_constData.baseAlphaScale > 0)
                {
                    TEXTURE(baseAlpha, "");

                    ImGui::PushItemWidth(columnWidth / 2 - 10);
                    {
                        changed |= ImGui::DragFloat("##brightnessAlpha", &_constData.baseAlphaBrightness, 0.01f, -1, 1);
                        TOOLTIP("brightness \n\nclamp(alpha  * contrast + brighness)");
                        ImGui::SameLine();
                        changed |= ImGui::DragFloat("##constrastAlpha", &_constData.baseAlphaContrast, 0.1f, 0.1f, 5);
                        TOOLTIP("contrast \n\nclamp(alpha  * contrast + brighness)");


                    }
                    ImGui::PopItemWidth();
                }


                ImGui::SetCursorPosY(Y0 + 130);
                ImGui::Text("detail texture");
                changed |= ImGui::SliderFloat("##detailalpha", &_constData.detailAlphaScale, 0, 1, "%.2f");
                TOOLTIP("detail texture influence");
                if (_constData.detailAlphaScale > 0)
                {
                    TEXTURE(detailAlpha, "");

                    ImGui::PushItemWidth(columnWidth / 2 - 10);
                    {
                        changed |= ImGui::DragFloat("##brightnessAlphaDetail", &_constData.detailAlphaBrightness, 0.05f, -1, 1);
                        TOOLTIP("brightness \n\nclamp(alpha  * contrast + brighness)");
                        ImGui::SameLine();
                        changed |= ImGui::DragFloat("##constrastAlphaDetail", &_constData.detailAlphaContrast, 0.1f, 0.1f, 5);
                        TOOLTIP("contrast \n\nclamp(alpha  * contrast + brighness)");
                    }
                    ImGui::PopItemWidth();

                }
            }
        }
        ImGui::EndChildFrame();

        //if (_constData.materialType == 0)
        {
            ImGui::NewLine();
            style.Colors[ImGuiCol_FrameBg] = ImVec4(0.01f, 0.02f, 0.07f, 0.7f);
            ImGui::BeginChildFrame(1234561, ImVec2(columnWidth, 220));
            style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
            {
                HEADER("physics");
                ImGui::Text("dry grip value");
                ImGui::Text("wet grip value");
                ImGui::Text("what else");
            }
            ImGui::EndChildFrame();
        }

    }


    ImGui::NextColumn();
    {
        if (_constData.materialType == 1)
        {
            char name[256];
            HEADER("Sub materials");
            for (uint i = 0; i < 8; i++)
            {

                SUBMATERIAL(i, "");

                sprintf(name, "alpha##%d", i);
                int alpha = ((_constData.subMaterials[i] >> 16) & 0xff);
                if (ImGui::SliderInt(name, &alpha, 0, 255))
                {
                    _constData.subMaterials[i] &= 0xff00ffff;
                    _constData.subMaterials[i] += alpha << 16;
                    changed |= true;
                    rebuildConstantbuffer();
                }

                sprintf(name, "control##%d", i);
                int ctrl = ((_constData.subMaterials[i] >> 24) & 0xff);
                if (ImGui::SliderInt(name, &ctrl, 0, 5))
                {
                    _constData.subMaterials[i] &= 0x00ffffff;
                    _constData.subMaterials[i] += ctrl << 24;
                    changed |= true;
                    rebuildConstantbuffer();
                }
                ImGui::NewLine();
            }

        }


        if (_constData.materialType == 0)
        {
            style.Colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.01f, 0.01f, 0.7f);
            ImGui::BeginChildFrame(123451, ImVec2(columnWidth, 360));
            style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
            {

                HEADER_BOOL("elevation", "##useElevation", _constData.useElevation);
                if (_constData.useElevation)
                {
                    ImGui::NewLine();
                    changed |= ImGui::Checkbox("absolute", &useAbsoluteElevation);
                    _constData.useAbsoluteElevation = useAbsoluteElevation;
                    TOOLTIP("is true, use absolute height and blend with terrain\notherwise offset is relative i.e. raise or lower the existing ground");
                    ImGui::SameLine(columnWidth / 2);

                    FLOAT_BOOL("vertex.y", _constData.useVertexY);
                    TOOLTIP("add the vertex heights in the mesh");

                    //ImGui::NewLine();

                    changed |= ImGui::DragFloat("y offset", &_constData.YOffset, 0.1f, -10, 10, "%.3f m");
                    TOOLTIP("offset to shift the mesh up or down");

                    ImGui::NewLine();

                    TEXTURE(baseElevation, "");
                    changed |= ImGui::DragFloat("base scale ##elevation", &_constData.baseElevationScale, 0.001f, 0, 1, "%.3f m");
                    TOOLTIP("scale of the displacement texture");
                    changed |= ImGui::DragFloat("base offset ##elevation", &_constData.baseElevationOffset, 0.1f, 0, 1, "%.2f");
                    TOOLTIP("texture value that represents zero");

                    ImGui::NewLine();

                    TEXTURE(detailElevation, "");
                    changed |= ImGui::DragFloat("detail scale ##elevation", &_constData.detailElevationScale, 0.001f, 0, 1, "%.3f m");
                    TOOLTIP("offset to shift the mesh up or down");
                    changed |= ImGui::DragFloat("detail offset ##elevation", &_constData.detailElevationOffset, 0.1f, 0, 1, "%.2f");
                    TOOLTIP("texture value that represents zero");

                    if (textureIndex[baseElevation] == -1) _constData.baseElevationScale = 0;
                    if (textureIndex[detailElevation] == -1) _constData.detailElevationScale = 0;

                }
            }
            ImGui::EndChildFrame();
        }

        if (_constData.materialType == 0)
        {
            ImGui::NewLine();
            style.Colors[ImGuiCol_FrameBg] = ImVec4(0.06f, 0.1f, 0.01f, 0.7f);
            ImGui::BeginChildFrame(123453, ImVec2(columnWidth, 580));
            style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
            {
                HEADER_BOOL("albedo & pbr", "####useAlbedo", _constData.useColour);
                if (_constData.useColour)
                {
                    ImGui::NewLine();
                    changed |= ImGui::Combo("##material index", &_constData.standardMaterialType, "tarmac\0soil\0grass\0");
                    TOOLTIP("material index\n It sets reflectance, microFiber, microShadow and lightWrap to fixed values");
                    ImGui::NewLine();

                    TEXTURE(baseAlbedo, "");

                    ImGui::Text("        ");
                    ImGui::SameLine();
                    changed |= ImGui::SliderFloat("##albedoScale", &_constData.albedoBlend, -1, 1, "base     :    detail");
                    TOOLTIP("blend between main and detail texture \n\n-1 : all main texture \n 0 : 50/50 \n 1 : all detail \n\nA = lerp(albedo, 0.5, saturate(blend)); \nB = lerp(detail, 0.5, saturate(-blend)); \nOut.Albedo.rgb = clamp(0.04, 0.90, A * B * scaleAlbedo * 2); // 0.04, 0.90 charcoal to fresh snow\n");
                    TEXTURE(detailAlbedo, "");

                    changed |= ImGui::ColorEdit3("albedo scale", &_constData.albedoScale.x);
                    TOOLTIP("fixed colour scale \n128 128 128 means no change\nIf the texture is averaged around 0.5 (WARNING ABOUT SRGB)\nthis can be absolute albedo\nThis value is in linear colour - you CANNOT copy values from photoshop in SRGB");

                    if (textureIndex[baseAlbedo] == -1) _constData.albedoBlend = 1;
                    if (textureIndex[detailAlbedo] == -1) _constData.albedoBlend = -1;



                    ImGui::NewLine();
                    HEADER("roughness");

                    TEXTURE(baseRoughness, "");

                    ImGui::Text("        ");
                    ImGui::SameLine();
                    changed |= ImGui::SliderFloat("##roughnessScale", &_constData.roughnessBlend, -1, 1, "base     :    detail");
                    TOOLTIP("blend between main and detail texture \n\n-1 : all main texture \n 0 : 50/50 \n 1 : all detail \n\nA = lerp(albedo, 0.5, saturate(blend)); \nB = lerp(detail, 0.5, saturate(-blend)); \nOut.Albedo.rgb = clamp(0.04, 0.90, A * B * scaleAlbedo * 2); // 0.04, 0.90 charcoal to fresh snow\n");
                    TEXTURE(detailRoughness, "");

                    changed |= ImGui::SliderFloat("rough scale", &_constData.roughnessScale, 0.1f, 10);

                    if (textureIndex[baseRoughness] == -1) _constData.roughnessBlend = 1;
                    if (textureIndex[detailRoughness] == -1) _constData.roughnessBlend = -1;


                    ImGui::NewLine();
                    HEADER("porosity");
                    changed |= ImGui::SliderFloat("porosity", &_constData.porosity, 0, 1);

                    ImGui::NewLine();
                    ImGui::Text("not doing ao at the moment, will try real time");
                }
            }
            ImGui::EndChildFrame();


        }
    }


    ImGui::NextColumn();
    {
        if (_constData.materialType == 0)
        {
            style.Colors[ImGuiCol_FrameBg] = ImVec4(0.03f, 0.03f, 0.03f, 0.7f);
            ImGui::BeginChildFrame(123454, ImVec2(columnWidth, 850));
            style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
            {
                HEADER_BOOL("ecotopes", "##useEcotopes", _constData.useEcotopes);

                changed |= ImGui::SliderFloat("elevation", &_constData.permanenceElevation, 0, 1, "ecotope   :   terrafector");
                TOOLTIP("what controls the final elevation");
                changed |= ImGui::SliderFloat("colour", &_constData.permanenceColour, 0, 1, "ecotope   :   terrafector");
                TOOLTIP("what controls the final colour");
                changed |= ImGui::SliderFloat("ecotopes", &_constData.permanenceEcotopes, 0, 1, "ecotope   :   terrafector");
                TOOLTIP("what controls the final ecotopes");
                //ImGui::NewLine();


                changed |= ImGui::SliderFloat3("cull", &_constData.cullA, 0, 1);
                //changed |= ImGui::SliderFloat("cull bushes", &_constData.cullB, 0, 1);
                //changed |= ImGui::SliderFloat("cull trees", &_constData.cullC, 0, 1);
                //ImGui::NewLine();
                ImGui::NewLine();

                if (_constData.useEcotopes)
                {


                    TEXTURE(ecotope, "");

                    ImGui::NewLine();


                    for (uint i = 0; i < 15; i++)
                    {
                        if (i < terrafectorSystem::pEcotopes->ecotopes.size()) {
                             changed |= ImGui::ColorEdit4(terrafectorSystem::pEcotopes->ecotopes[i].name.c_str(), &_constData.ecotopeMasks[i][0], true);
                        }
                        else
                        {
                            char ect_name[256];
                            sprintf(ect_name, "ecotope %d", i);
                            changed |= ImGui::ColorEdit4(ect_name, &_constData.ecotopeMasks[i][0], true);
                        }
                        TOOLTIP("remember to set alpha");
                        if (i % 3 == 2) ImGui::NewLine();

                    }

                }
            }
            ImGui::EndChildFrame();



        }
    }


    ImGui::Columns(1);

    style = oldStyle;

    if (changed) {
        rebuildConstantbuffer();
        isModified = true;
    }
    return changed;
}



void terrafectorEditorMaterial::rebuildConstantbufferData()
{
    // rework the indicis
    _constData.baseAlphaTexture = textureIndex[baseAlpha];
    _constData.detailAlphaTexture = textureIndex[detailAlpha];
    _constData.baseElevationTexture = textureIndex[baseElevation];
    _constData.detailElevationTexture = textureIndex[detailElevation];
    _constData.baseAlbedoTexture = textureIndex[baseAlbedo];
    _constData.detailAlbedoTexture = textureIndex[detailAlbedo];
    _constData.baseRoughnessTexture = textureIndex[baseRoughness];
    _constData.detailRoughnessTexture = textureIndex[detailRoughness];
    _constData.ecotopeTexture = textureIndex[ecotope];
}

void terrafectorEditorMaterial::rebuildConstantbuffer()
{
    rebuildConstantbufferData();

    //constantBuffer->setBlob(&_constData, 0, sizeof(_constData));
    //constantBuffer->uploadToGPU();
    terrafectorSystem::needsRefresh = true;

    // also update structures buffer for the cvache
    static_materials.rebuildStructuredBuffer();
}








void terrafectorSystem::loadFile()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"terrafectorSystem"} };
    if (openFileDialog(filters, path))
    {
        std::ifstream is(path);
        cereal::BinaryInputArchive archive(is);
        serialize(archive);
    }
}


void terrafectorSystem::saveFile()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"terrafectorSystem"} };
    if (saveFileDialog(filters, path))
    {
        std::ofstream os(path);
        cereal::BinaryOutputArchive archive(os);
        serialize(archive);
    }
}


void terrafectorSystem::renderGui(Gui* mpGui, Gui::Window& _window)
{
    int id = 1000;
    float W = ImGui::GetWindowWidth();
    auto& style = ImGui::GetStyle();
    style.ButtonTextAlign = ImVec2(0, 0.5);
    //style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    //style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.7f, 0.9f, 0.7f);

    ImVec4 oldText = style.Colors[ImGuiCol_Text];
    root.renderGui(mpGui);
    style.Colors[ImGuiCol_Text] = oldText;

    // Materials
    //ImGui::NewLine();
    //terrafectorEditorMaterial::static_materials.renderGui(mpGui, _window);

}


void terrafectorSystem::loadPath(std::string _path, std::string _exportPath, bool _rebuild)
{
    forceAllTerrfectorRebuild = _rebuild;

    fprintf(terrafectorSystem::_logfile, "\n\nterrafectorSystem::loadPath    %s\n", _path.c_str());
    logTab = 0;

    terrafectorSystem::loadCombine_LOD2.create(2);
    terrafectorSystem::loadCombine_LOD4.create(4);
    terrafectorSystem::loadCombine_LOD6.create(6);
    terrafectorSystem::loadCombine_LOD4_bakeLow.create(4);
    terrafectorSystem::loadCombine_LOD4_bakeHigh.create(4);
    terrafectorSystem::loadCombine_LOD4_overlay.create(4);

    root.loadPath(_path);
    fprintf(terrafectorSystem::_logfile, "\n\n");


    fprintf(terrafectorSystem::_logfile, "loadCombine_LOD2.loadToGPU()\n");
    terrafectorSystem::loadCombine_LOD2.loadToGPU(_exportPath + "/terrafector_lod2.gpu");   // this also releases CPU memory
    fprintf(terrafectorSystem::_logfile, "loadCombine_LOD4.loadToGPU()\n");
    terrafectorSystem::loadCombine_LOD4.loadToGPU(_exportPath + "/terrafector_lod4.gpu");   // this also releases CPU memory
    fprintf(terrafectorSystem::_logfile, "loadCombine_LOD6.loadToGPU()\n");
    terrafectorSystem::loadCombine_LOD6.loadToGPU(_exportPath + "/terrafector_lod6.gpu");   // this also releases CPU memory

    fprintf(terrafectorSystem::_logfile, "loadCombine_LOD4_bakeLow.loadToGPU()\n");
    terrafectorSystem::loadCombine_LOD4_bakeLow.loadToGPU("");   // this also releases CPU memory
    fprintf(terrafectorSystem::_logfile, "loadCombine_LOD4_bakeHigh.loadToGPU()\n");
    terrafectorSystem::loadCombine_LOD4_bakeHigh.loadToGPU("");   // this also releases CPU memory
    fprintf(terrafectorSystem::_logfile, "loadCombine_LOD4_overlay.loadToGPU()\n");
    terrafectorSystem::loadCombine_LOD4_overlay.loadToGPU("");   // this also releases CPU memory

    terrafectorEditorMaterial::static_materials.rebuildAll();

    /*
    fprintf(terrafectorSystem::_logfile, "\n\nmeshLoadCombiner\n");
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            fprintf(terrafectorSystem::_logfile, "%6d", terrafectorSystem::loadCombine_LOD4_bakeLow.getTile(y * 16 + x)->numVerts);
        }
        fprintf(terrafectorSystem::_logfile, "\n");
    }
    */
}



void terrafectorSystem::exportMaterialBinary(std::string _path, std::string _evoRoot)
{
    uint textureSize = (uint)terrafectorEditorMaterial::static_materials.textureVector.size();
    uint matSize = (uint)terrafectorEditorMaterial::static_materials.materialVector.size();

    std::string textureName = _path + "/TextureList.gpu";
    FILE* file = fopen(textureName.c_str(), "wb");
    if (file)
    {
        fwrite(&textureSize, 1, sizeof(uint), file);
        fwrite(&matSize, 1, sizeof(uint), file);

        char name[512];
        for (uint i = 0; i < textureSize; i++) {
            std::string path = terrafectorEditorMaterial::static_materials.textureVector[i]->getSourcePath().string();
            path = path.substr(13, path.size());
            path = path.substr(0, path.rfind("."));
            path += ".texture";
            memset(name, 0, 512);
            sprintf(name, "%s", path.c_str());
            fwrite(name, 1, 512, file);

            // Now test if it exists in EVO
            //C:/Kunos/acevo_content/content/
            if (!std::filesystem::exists(_evoRoot + path))
            {
                fprintf(terrafectorSystem::_logfile, "EVO _ DOES NOT EXISTS %s\n", (_evoRoot + path).c_str());
            }
        }


        fclose(file);
    }



    textureName = _path + "/Materials.gpu";
    file = fopen(textureName.c_str(), "wb");
    if (file)
    {
        for (uint i = 0; i < matSize; i++)
        {
            fwrite(&terrafectorEditorMaterial::static_materials.materialVector[i]._constData, 1, sizeof(TF_material), file);
        }
        fclose(file);
    }
}
