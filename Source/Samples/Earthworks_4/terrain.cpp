/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "terrain.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <random>



void _terrainSettings::renderGui(Gui* _gui)
{
    ImGui::PushFont(_gui->getFont("roboto_32"));
    {
        static const int maxSize = 2048;
        char buf[maxSize];
        sprintf_s(buf, maxSize, name.c_str());
        ImGui::SetNextItemWidth(300);
        if (ImGui::InputText("##name", buf, maxSize))
        {
            name = buf;
        }

        ImGui::NewLine();
        ImGui::PushFont(_gui->getFont("roboto_20"));
        {
            ImGui::Text("projection");
            sprintf_s(buf, maxSize, projection.c_str());
            ImGui::SetNextItemWidth(1000);
            if (ImGui::InputText("##projection", buf, maxSize))
            {
                projection = buf;
            }
        }
        ImGui::Text("size");
        ImGui::PopFont();

        ImGui::SetNextItemWidth(150);
        ImGui::DragFloat("##size", &size, 1, 1000, 200000, "%5.0f m ");

        ImGui::NewLine();
        ImGui::Button("Save");
        ImGui::SameLine();
        ImGui::Button("Save as");
    }
    ImGui::PopFont();
}


void quadtree_tile::init(uint _index)
{
    index = _index;
    parent = nullptr;
    child[0] = nullptr;
    child[1] = nullptr;
    child[2] = nullptr;
    child[3] = nullptr;
}

void quadtree_tile::set(uint _lod, uint _x, uint _y, float _size, float4 _origin, quadtree_tile* _parent)
{
    lod = _lod;
    x = _x;
    y = _y;
    size = _size;
    origin = _origin;

    boundingSphere = origin + float4(size / 2, 0, size / 2, 0);
    boundingSphere.w = 1.0f;

    parent = parent;
    child[0] = nullptr;
    child[1] = nullptr;
    child[2] = nullptr;
    child[3] = nullptr;

    if (parent)
    {
        origin.y = parent->boundingSphere.y - size * 2;
        boundingSphere.y = parent->boundingSphere.y;
    }
    else
    {
        origin.y = size * 2;
        boundingSphere.y = 0;
    }

    numQuads = 0;
    numPlants = 0;

    forSplit = false;
    forRemove = false;

    elevationHash = 0;
}




terrainManager::terrainManager()
{
    std::ifstream is("lastFile.xml");
    if (is.good()) {
        cereal::XMLInputArchive archive(is);
        archive(CEREAL_NVP(lastfile));
    }
}



terrainManager::~terrainManager()
{
    std::ofstream os("lastFile.xml");
    if (os.good()) {
        cereal::XMLOutputArchive archive(os);
        archive(CEREAL_NVP(lastfile));
    }
}



void terrainManager::onLoad(RenderContext* pRenderContext)
{
    {
        Sampler::Desc samplerDesc;
        samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp).setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setMaxAnisotropy(8);
        sampler_Clamp = Sampler::create(samplerDesc);

        samplerDesc.setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap).setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setMaxAnisotropy(8);
        sampler_Trilinear = Sampler::create(samplerDesc);

        samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp).setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setMaxAnisotropy(4);
        sampler_ClampAnisotropic = Sampler::create(samplerDesc);
    }

    {
        split.debug_texture = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::RGBA8Unorm, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess);
        split.bicubic_upsample_texture = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::R32Float, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess);
        split.normals_texture = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::R11G11B10Float, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess);
        split.vertex_A_texture = Texture::create2D(tile_numPixels / 2, tile_numPixels / 2, Falcor::ResourceFormat::R16Uint, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Falcor::Resource::BindFlags::UnorderedAccess);
        split.vertex_B_texture = Texture::create2D(tile_numPixels / 2, tile_numPixels / 2, Falcor::ResourceFormat::R16Uint, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Falcor::Resource::BindFlags::UnorderedAccess);
    }

    {
        std::vector<glm::uint16> vertexData(tile_numPixels / 2 * tile_numPixels / 2);
        memset(vertexData.data(), 0, tile_numPixels / 2 * tile_numPixels / 2 * sizeof(glm::uint16));	  // set to zero's
        split.vertex_clear = Texture::create2D(tile_numPixels / 2, tile_numPixels / 2, Falcor::ResourceFormat::R16Uint, 1, 1, vertexData.data(), Resource::BindFlags::ShaderResource);

        // This is the preload for the square pattern
        std::array<glm::uint16, 17> pattern = { 0, 7, 15, 23, 31, 39, 47, 55, 63, 71, 79, 87, 95, 103, 111, 119, 126 };
        for (int y = 0; y <= 16; y++)
        {
            int mY = pattern[y];
            for (int x = 0; x <= 16; x++)
            {
                int mX = pattern[x];
                glm::uint index = (mY + 4) * 128 + mX + 4;
                if (x == 16) index -= 4;
                if (y == 16) index -= 4 * 128;
            }
        }
        split.vertex_preload = Texture::create2D(tile_numPixels / 2, tile_numPixels / 2, Falcor::ResourceFormat::R16Uint, 1, 1, vertexData.data(), Resource::BindFlags::ShaderResource);
    }


    {
        split.buffer_tileCenters = Buffer::createStructured(sizeof(float4), numTiles);
        split.buffer_tileCenter_readback = Buffer::create(sizeof(float4) * numTiles, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::Read);
        split.tileCenters.resize(numTiles);
    }


    {
        // u16 noise texture
        std::mt19937 generator(2);
        std::uniform_int_distribution<> distribution(0, 65535);
        std::vector<unsigned short> random(256 * 256);
        for (int i = 0; i < 256 * 256; i++)
        {
            random[i] = distribution(generator);
        }
        split.noise_u16 = Texture::create2D(256, 256, ResourceFormat::R16Uint, 1, 1, random.data());

        // frame buffer
        Fbo::Desc desc;
        desc.setDepthStencilTarget(ResourceFormat::D24UnormS8);			// keep for now, not sure why, but maybe usefult for cuts
        desc.setColorTarget(0u, ResourceFormat::R32Float, true);		// elevation
        desc.setColorTarget(1u, ResourceFormat::R11G11B10Float, true);	// albedo
        desc.setColorTarget(2u, ResourceFormat::R11G11B10Float, true);	// pbr
        desc.setColorTarget(3u, ResourceFormat::R11G11B10Float, true);	// alpha
        desc.setColorTarget(4u, ResourceFormat::RGBA8Unorm, true);		// ecotopes  ? R11G11B10Float 
        desc.setColorTarget(5u, ResourceFormat::RGBA8Unorm, true);		// ecotopes
        desc.setColorTarget(6u, ResourceFormat::RGBA8Unorm, true);		// ecotopes
        desc.setColorTarget(7u, ResourceFormat::RGBA8Unorm, true);		// ecotopes
        split.tileFbo = Fbo::create2D(tile_numPixels, tile_numPixels, desc, 1, 8);
        split.bakeFbo = Fbo::create2D(split.bakeSize, split.bakeSize, desc, 1, 8);


        split.drawArgs_quads = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
        split.drawArgs_plants = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
        split.drawArgs_tiles = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);

        split.buffer_feedback = Buffer::createStructured(sizeof(GC_feedback), 1);

        split.buffer_tiles = Buffer::createStructured(sizeof(gpuTile), numTiles);
        split.buffer_instance_quads = Buffer::createStructured(sizeof(instance_PLANT), numTiles * numQuadsPerTile);
        split.buffer_instance_plants = Buffer::createStructured(sizeof(instance_PLANT), numTiles * numPlantsPerTile);

        split.buffer_lookup_terrain = Buffer::createStructured(sizeof(instance_PLANT), 524288);
        split.buffer_lookup_quads = Buffer::createStructured(sizeof(instance_PLANT), 131072);
        split.buffer_lookup_plants = Buffer::createStructured(sizeof(instance_PLANT), 131072);

        split.buffer_terrain = Buffer::createStructured(sizeof(Terrain_vertex), numVertPerTile * numTiles);


        // clear
        split.compute_tileClear.load("Samples/Earthworks_4/compute_tileClear.hlsl");
        split.compute_tileClear.Vars()->setBuffer("feedback", split.buffer_feedback);

        // split merge
        split.compute_tileSplitMerge.load("Samples/Earthworks_4/compute_tileSplitMerge.hlsl");
        split.compute_tileSplitMerge.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileSplitMerge.Vars()->setBuffer("feedback", split.buffer_feedback);

        // generate
        split.compute_tileGenerate.load("Samples/Earthworks_4/compute_tileGenerate.hlsl");
        split.compute_tileGenerate.Vars()->setBuffer("quad_instance", split.buffer_instance_quads);
        split.compute_tileGenerate.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileGenerate.Vars()->setTexture("gNoise", split.noise_u16);
        split.compute_tileGenerate.Vars()->setTexture("gHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileGenerate.Vars()->setTexture("gEct1", split.tileFbo->getColorTexture(4));
        split.compute_tileGenerate.Vars()->setTexture("gEct2", split.tileFbo->getColorTexture(5));
        split.compute_tileGenerate.Vars()->setTexture("gEct3", split.tileFbo->getColorTexture(6));
        split.compute_tileGenerate.Vars()->setTexture("gEct4", split.tileFbo->getColorTexture(7));

        // passthrough
        split.compute_tilePassthrough.load("Samples/Earthworks_4/compute_tilePassthrough.hlsl");
        split.compute_tilePassthrough.Vars()->setBuffer("quad_instance", split.buffer_instance_quads);
        split.compute_tilePassthrough.Vars()->setBuffer("plant_instance", split.buffer_instance_plants);
        split.compute_tilePassthrough.Vars()->setBuffer("feedback", split.buffer_feedback);
        split.compute_tilePassthrough.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tilePassthrough.Vars()->setTexture("gHgt", split.tileFbo->getColorTexture(0));
        split.compute_tilePassthrough.Vars()->setTexture("gNoise", split.noise_u16);

        // build lookup
        split.compute_tileBuildLookup.load("Samples/Earthworks_4/compute_tileBuildLookup.hlsl");
        split.compute_tileBuildLookup.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileBuildLookup.Vars()->setBuffer("tileLookup", split.buffer_lookup_quads);
        split.compute_tileBuildLookup.Vars()->setBuffer("plantLookup", split.buffer_lookup_plants);
        split.compute_tileBuildLookup.Vars()->setBuffer("terrainLookup", split.buffer_lookup_terrain);
        split.compute_tileBuildLookup.Vars()->setBuffer("DrawArgs_Quads", split.drawArgs_quads);
        split.compute_tileBuildLookup.Vars()->setBuffer("DrawArgs_Terrain", split.drawArgs_tiles);
        split.compute_tileBuildLookup.Vars()->setBuffer("feedback", split.buffer_feedback);

        // bicubic
        split.compute_tileBicubic.load("Samples/Earthworks_4/compute_tileBicubic.hlsl");
        split.compute_tileBicubic.Vars()->setSampler("linearSampler", sampler_Clamp);
        split.compute_tileBicubic.Vars()->setTexture("gOutput", split.bicubic_upsample_texture);
        split.compute_tileBicubic.Vars()->setTexture("gDebug", split.debug_texture);
        split.compute_tileBicubic.Vars()->setTexture("gOuthgt_TEMPTILLTR", split.tileFbo->getColorTexture(0));

        // ecotopes
        split.compute_tileEcotopes.load("Samples/Earthworks_4/compute_tileEcotopes.hlsl");

        // normals
        split.compute_tileNormals.load("Samples/Earthworks_4/compute_tileNormals.hlsl");
        split.compute_tileNormals.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileNormals.Vars()->setTexture("gOutNormals", split.normals_texture);
        split.compute_tileNormals.Vars()->setTexture("gOutput", split.debug_texture);
        split.compute_tileNormals.Vars()->setBuffer("tiles", split.buffer_tiles);

        // vertices
        split.compute_tileVerticis.load("Samples/Earthworks_4/compute_tileVertices.hlsl");
        split.compute_tileVerticis.Vars()->setSampler("linearSampler", sampler_Clamp);
        split.compute_tileVerticis.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileVerticis.Vars()->setTexture("gOutVerts", split.vertex_A_texture);
        split.compute_tileVerticis.Vars()->setTexture("gDebug", split.debug_texture);
        split.compute_tileVerticis.Vars()->setBuffer("tileCenters", split.buffer_tileCenters);
        split.compute_tileVerticis.Vars()->setBuffer("tiles", split.buffer_tiles);

        // jumpflood
        // It may even be faster to set this up twice and hop between the two
        split.compute_tileJumpFlood.load("Samples/Earthworks_4/compute_tileJumpFlood.hlsl");
        split.compute_tileJumpFlood.Vars()->setTexture("gDebug", split.debug_texture);

        // delaunay
        split.compute_tileDelaunay.load("Samples/Earthworks_4/compute_tileDelaunay.hlsl");
        split.compute_tileDelaunay.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileDelaunay.Vars()->setTexture("gInVerts", split.vertex_A_texture);
        split.compute_tileDelaunay.Vars()->setBuffer("VB", split.buffer_terrain);
        split.compute_tileDelaunay.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileDelaunay.Vars()->setBuffer("tileDrawargsArray", split.drawArgs_tiles);

        // elevation mipmap
        split.compute_tileElevationMipmap.load("Samples/Earthworks_4/compute_tileElevationMipmap.hlsl");
        split.compute_tileElevationMipmap.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileElevationMipmap.Vars()->setTexture("gDebug", split.debug_texture);
        const auto& mipmapReflector = split.compute_tileElevationMipmap.Reflector()->getDefaultParameterBlock();
        ParameterBlock::BindLocation bind_mip1 = mipmapReflector->getResourceBinding("gMip1");
        ParameterBlock::BindLocation bind_mip2 = mipmapReflector->getResourceBinding("gMip2");
        ParameterBlock::BindLocation bind_mip3 = mipmapReflector->getResourceBinding("gMip3");
        split.compute_tileElevationMipmap.Vars()->setUav(bind_mip1, split.tileFbo->getColorTexture(0)->getUAV(1));
        split.compute_tileElevationMipmap.Vars()->setUav(bind_mip2, split.tileFbo->getColorTexture(0)->getUAV(2));
        split.compute_tileElevationMipmap.Vars()->setUav(bind_mip3, split.tileFbo->getColorTexture(0)->getUAV(3));

        // BC6H compressor
        split.bc6h_texture = Texture::create2D(tile_numPixels / 4, tile_numPixels / 4, Falcor::ResourceFormat::RGBA32Uint, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess);
        split.compute_bc6h.load("Samples/Earthworks_4/compute_bc6h.hlsl");
        split.compute_bc6h.Vars()->setTexture("gOutput", split.bc6h_texture);
    }

    allocateTiles(1024);

    elevationCache.resize(45);
    loadElevationHash();
}

void terrainManager::allocateTiles(uint numTiles)
{
    quadtree_tile tile;

    m_tiles.clear();
    m_tiles.reserve(numTiles);
    split.cpuTiles.clear();
    split.cpuTiles.resize(numTiles);


    for (uint i = 0; i < numTiles; i++)
    {
        tile.init(i);
        m_tiles.push_back(tile);
    }

    reset();
}

void terrainManager::reset(bool _fullReset)
{
    fullResetDoNotRender = _fullReset;

    m_free.clear();
    m_used.clear();

    for (uint i = 0; i < m_tiles.size(); i++)
    {
        m_free.push_back(&m_tiles[i]);
    }

    quadtree_tile* root = m_free.front();
    m_free.pop_front();
    root->set(0, 0, 0, 40000, float4(-20000.0f, 0, -20000.0f, 0.0f), nullptr);

    m_used.push_back(root);

    // FIXME this could be done faster by changing the cache compute shader to alwas clear if its Tile zero thats caching
    // or add a special shader for that
    if (split.buffer_tiles) {
        for (uint i = 0; i < m_tiles.size(); i++)
        {
            split.cpuTiles[i].lod = 0;
            split.cpuTiles[i].Y = 0;
            split.cpuTiles[i].X = 0;
            split.cpuTiles[i].flags = 0;

            split.cpuTiles[i].origin = float3(0, 0, 0);
            split.cpuTiles[i].scale_1024 = 0;

            split.cpuTiles[i].numQuads = 0;
            split.cpuTiles[i].numPlants = 0;
            split.cpuTiles[i].numTriangles = 0;
            split.cpuTiles[i].numLights = 0;
        }
        split.buffer_tiles->setBlob(&split.cpuTiles, 0, m_tiles.size() * sizeof(gpuTile));
        // lets hope for now the upload is now automatic
        //split.buffer_tiles->>uploadToGPU();
    }
}

uint32_t getHashFromTileCoords(unsigned int lod, unsigned int y, unsigned int x) {
    return (lod << 28) + (y << 14) + (x);
}

void terrainManager::loadElevationHash()
{
    std::string fullpath = settings.dirRoot + "/elevations.txt";
    elevationTileHashmap.clear();
    reset();


    FILE* pFileHgt = fopen(fullpath.c_str(), "r");
    if (pFileHgt)
    {
        heightMap map;
        int texSize;
        char filename[256];
        int items = 1;
        do {
            items = fscanf(pFileHgt, "%d %d %d %d %f %f %f %f %f %s\n", &map.lod, &map.y, &map.x, &texSize, &map.origin.x, &map.origin.y, &map.size, &map.hgt_offset, &map.hgt_scale, filename);
            if (items > 0)
            {
                uint32_t hash = getHashFromTileCoords(map.lod, map.y, map.x);

                fullpath = settings.dirRoot + filename;
                if (map.lod == 0)
                {
                    std::vector<unsigned short> data;
                    data.resize(texSize * texSize);

                    FILE* pData = fopen(fullpath.c_str(), "r");
                    if (pData)
                    {
                        fread(data.data(), sizeof(unsigned short), texSize * texSize, pData);
                        fclose(pData);
                    }

                    split.rootElevation = Texture::create2D(texSize, texSize, Falcor::ResourceFormat::R16Unorm, 1, 1, data.data(), Resource::BindFlags::ShaderResource);
                    elevationTileHashmap[hash] = map;
                }
                else
                {
                    map.filename = fullpath;
                    char txt[1024];
                    sprintf(txt, "%s", fullpath.c_str());
                    elevationTileHashmap[hash] = map;
                }
            }
        } while (items > 0);

        fclose(pFileHgt);
    }

}

void terrainManager::onShutdown()
{
}


void terrainManager::onGuiRender(Gui* _gui)
{
    if (requestPopupSettings) {
        ImGui::OpenPopup("settings");
        requestPopupSettings = false;
    }
    if (ImGui::BeginPopup("settings"))      // modal
    {
        settings.renderGui(_gui);
        ImGui::EndPopup();
    }
}


void terrainManager::onGuiMenubar(Gui* pGui)
{
    bool b = false;

    ImGui::SetCursorPos(ImVec2(150, 0));
    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Text] = ImVec4(0.38f, 0.52f, 0.10f, 1);
    //ImGui::Text(presets.name.c_str());
    style.Colors[ImGuiCol_Text] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

    ImGui::SetCursorPos(ImVec2(200, 0));
    if (ImGui::BeginMenu("terrain"))
    {
        if (ImGui::MenuItem("settings"))
        {
            requestPopupSettings = true;

        }
        ImGui::EndMenu();
    }
}

//mimic hlsl all()
bool all(float4 in)
{
    if (in.x == 0) return false;
    if (in.y == 0) return false;
    if (in.z == 0) return false;
    if (in.w == 0) return false;
    return true;
}

#define mainMaxLod 15
bool terrainManager::testForSplit(quadtree_tile* _tile)
{
    _tile->main_ShouldSplit = false;
    _tile->env_ShouldSplit = false;

    if (_tile->lod > mainMaxLod)
        return false;

    float scale = _tile->size / glm::length(float3(_tile->boundingSphere) - cameraOrigin);
    float boundingSphereSize = _tile->size * 0.8f;

    for (int i = 0; i < CameraType_MAX; i++) {
        if (cameraViews[i].bUse) {

            float4 viewBS = cameraViews[i].view * _tile->boundingSphere;
            float4 test = saturate(viewBS * cameraViews[i].frustumMatrix + float4(boundingSphereSize, boundingSphereSize, boundingSphereSize, boundingSphereSize));
            bool inFrust = all(test);

            viewBS.w = 0;
            float distance = length(viewBS) + 0.01f;
            float fovscale = glm::length(cameraViews[i].proj[0]);
            float m_halfAngle_to_Pixels = cameraViews[i].resolution * fovscale / 4.0f;
            float lod_Pix = _tile->size / distance * m_halfAngle_to_Pixels;



            if (lod_Pix > 150 && inFrust)
            {
                _tile->main_ShouldSplit = true;
            }
            else if (lod_Pix > 300)
            {
                _tile->main_ShouldSplit = true;
            }
        }
    }

    if (_tile->main_ShouldSplit && !_tile->child[0]) {
        _tile->forSplit = true;
        return true;
    }

    if (!_tile->main_ShouldSplit) {
        markForRemove(_tile);
        return true;
    }

    return false;
}

bool terrainManager::testFrustum(quadtree_tile* _tile)
{
    return true;
}

void terrainManager::markForRemove(quadtree_tile* _tile)
{}


void terrainManager::clearCameras()
{
    for (uint i = 0; i < CameraType_MAX; i++) {
        cameraViews[i].bUse = false;
    }

}

void terrainManager::setCamera(unsigned int _index, glm::mat4 *viewMatrix, glm::mat4 *projMatrix, float3 position, bool b_use, float _resolution)
{
    if (_index < CameraType_MAX)
    {
        cameraOrigin = position;	// SURE so last set camera does this, but they should just about all be the same right?

        cameraViews[_index].bUse = b_use;
        cameraViews[_index].resolution = _resolution;
        cameraViews[_index].view = *viewMatrix;
        cameraViews[_index].proj = *projMatrix;
        cameraViews[_index].viewProj = cameraViews[_index].view * cameraViews[_index].proj;
        cameraViews[_index].position = position;

        // remeber that these are transposed as well here
        cameraViews[_index].frustumPlane[0].x = cameraViews[_index].proj[0][3] + cameraViews[_index].proj[0][0];
        cameraViews[_index].frustumPlane[0].y = cameraViews[_index].proj[1][3] + cameraViews[_index].proj[1][0];
        cameraViews[_index].frustumPlane[0].z = cameraViews[_index].proj[2][3] + cameraViews[_index].proj[2][0];
        cameraViews[_index].frustumPlane[0].w = cameraViews[_index].proj[3][3] + cameraViews[_index].proj[3][0];

        cameraViews[_index].frustumPlane[1].x = cameraViews[_index].proj[0][3] - cameraViews[_index].proj[0][0];
        cameraViews[_index].frustumPlane[1].y = cameraViews[_index].proj[1][3] - cameraViews[_index].proj[1][0];
        cameraViews[_index].frustumPlane[1].z = cameraViews[_index].proj[2][3] - cameraViews[_index].proj[2][0];
        cameraViews[_index].frustumPlane[1].w = cameraViews[_index].proj[3][3] - cameraViews[_index].proj[3][0];

        cameraViews[_index].frustumPlane[2].x = cameraViews[_index].proj[0][3] + cameraViews[_index].proj[0][1];
        cameraViews[_index].frustumPlane[2].y = cameraViews[_index].proj[1][3] + cameraViews[_index].proj[1][1];
        cameraViews[_index].frustumPlane[2].z = cameraViews[_index].proj[2][3] + cameraViews[_index].proj[2][1];
        cameraViews[_index].frustumPlane[2].w = cameraViews[_index].proj[3][3] + cameraViews[_index].proj[3][1];

        cameraViews[_index].frustumPlane[3].x = cameraViews[_index].proj[0][3] - cameraViews[_index].proj[0][1];
        cameraViews[_index].frustumPlane[3].y = cameraViews[_index].proj[1][3] - cameraViews[_index].proj[1][1];
        cameraViews[_index].frustumPlane[3].z = cameraViews[_index].proj[2][3] - cameraViews[_index].proj[2][1];
        cameraViews[_index].frustumPlane[3].w = cameraViews[_index].proj[3][3] - cameraViews[_index].proj[3][1];

        cameraViews[_index].frustumMatrix = glm::mat4x4(
            cameraViews[_index].frustumPlane[0].x, cameraViews[_index].frustumPlane[0].y, cameraViews[_index].frustumPlane[0].z, cameraViews[_index].frustumPlane[0].w,
            cameraViews[_index].frustumPlane[1].x, cameraViews[_index].frustumPlane[1].y, cameraViews[_index].frustumPlane[1].z, cameraViews[_index].frustumPlane[1].w,
            cameraViews[_index].frustumPlane[2].x, cameraViews[_index].frustumPlane[2].y, cameraViews[_index].frustumPlane[2].z, cameraViews[_index].frustumPlane[2].w,
            cameraViews[_index].frustumPlane[3].x, cameraViews[_index].frustumPlane[3].y, cameraViews[_index].frustumPlane[3].z, cameraViews[_index].frustumPlane[3].w);
    }
}


void terrainManager::update(RenderContext* _renderContext)
{
    bool dirty = false;
    
    split.compute_tileClear.dispatch(_renderContext, 1, 1);
    
    for (auto& tile : m_used)
    {
        tile->reset();
        dirty |= testForSplit(tile);
    }

    for (auto itt = m_used.begin(); itt != m_used.end();)               // do al merges
    {
        if ((*itt)->forRemove) {
            m_free.push_back(*itt);
            itt = m_used.erase(itt);
        }
        else {
            ++itt;
        }
    }
    
    splitOne(_renderContext);

    {
        uint cnt = (numTiles + 31) >> 5;
        //split.compute_tileBuildLookup.dispatch(_renderContext, cnt, 1);
    }

    if (dirty)
    {
        /*
        // readback of tile centers
        _renderContext->copyResource(split.buffer_tileCenter_readback.get(), split.buffer_tileCenters.get());
        const float4* pData = (float4*)split.buffer_tileCenter_readback.get()->map(Buffer::MapType::Read);
        std::memcpy(&split.tileCenters, pData, sizeof(float4) * 2048);
        split.buffer_tileCenter_readback.get()->unmap();
        for (int i = 0; i < m_tiles.size(); i++)
        {
            m_tiles[i].origin.y = split.tileCenters[i].x;
            m_tiles[i].boundingSphere.y = split.tileCenters[i].x;
        }
        */
    }
}






void terrainManager::hashAndCache(quadtree_tile* pTile)
{
    
    uint32_t hash = getHashFromTileCoords(pTile->lod, pTile->y, pTile->x);
    std::map<uint32_t, heightMap>::iterator it = elevationTileHashmap.find(hash);
    if (it != elevationTileHashmap.end()) {
        pTile->elevationHash = hash;
    }

    if (pTile->elevationHash > 0)
    {
        textureCacheElement map;

        if (!elevationCache.get(pTile->elevationHash, map))
        {
            std::array<unsigned short, 1048576> data;

            ojph::codestream codestream;
            ojph::j2c_infile j2c_file;
            j2c_file.open(elevationTileHashmap[pTile->elevationHash].filename.c_str());
            codestream.enable_resilience();
            codestream.set_planar(false);
            codestream.read_headers(&j2c_file);
            codestream.create();
            int next_comp;

            for (int i = 0; i < 1024; ++i)
            {
                ojph::line_buf* line = codestream.pull(next_comp);
                int32_t* dp = line->i32;
                for (int j = 0; j < 1024; j++) {
                    int16_t val = (int16_t)*dp++;
                    data[i * 1024 + j] = val;
                }
            }
            codestream.close();

            map.texture = Texture::create2D(1024, 1024, Falcor::ResourceFormat::R16Unorm, 1, 1, data.data(), Resource::BindFlags::ShaderResource);
            elevationCache.set(pTile->elevationHash, map);
        }

        split.compute_tileBicubic.Vars()->setTexture("gInput", map.texture);
    }
    
}


void terrainManager::setChild(quadtree_tile* pTile, int y, int x)
{
    int childIdx = (y << 1) + x;
    float origX = pTile->size / 2.0f * x;
    float origY = pTile->size / 2.0f * y;

    pTile->child[childIdx] = m_free.front();
    m_free.pop_front();
    pTile->child[childIdx]->set(pTile->lod + 1, pTile->x * 2 + x, pTile->y * 2 + y, pTile->size / 2.0f, pTile->origin + float4(origX, 0, origY, 0), pTile);
    m_used.push_back(pTile->child[childIdx]);
    pTile->child[childIdx]->elevationHash = pTile->elevationHash;
}


void terrainManager::splitOne(RenderContext* _renderContext)
{
    if (m_free.size() < 4) return;

    for (auto const& tile : m_used)
    {
        if (tile->forSplit)
        {
            hashAndCache(tile);

            if (true)	// antani, check for hashAndCache() return value is it ready
            {

                FALCOR_PROFILE("split");

                setChild(tile, 0, 0);
                setChild(tile, 0, 1);
                setChild(tile, 1, 0);
                setChild(tile, 1, 1);

                {
                    FALCOR_PROFILE("cs_tile_SplitMerge");
                    tileForSplit children[4];
                    for (int i = 0; i < 4; i++) {
                        children[i].index = tile->child[i]->index;
                        children[i].lod = tile->child[i]->lod;
                        children[i].y = tile->child[i]->y;
                        children[i].x = tile->child[i]->x;

                        children[i].origin = tile->child[i]->origin;
                        children[i].scale = tile->child[i]->size;
                    }

                    /*const auto& pReflector = split.compute_tileSplitMerge.Vars()->getReflection();
                    const auto& pDefaultBlock = pReflector->getDefaultParameterBlock();
                    const auto& mPerLightCbLoc = pDefaultBlock->getResourceBinding("gConstants");
                    auto pCB = split.compute_tileSplitMerge.Vars()->getParameterBlock(mPerLightCbLoc); */
                    auto pCB = split.compute_tileSplitMerge.Vars()->getParameterBlock("gConstants");
                    pCB->setBlob(children, 0, 4 * sizeof(tileForSplit));
                    split.compute_tileSplitMerge.dispatch(_renderContext, 1, 1);
                }


                {
                    FALCOR_PROFILE("extract_all");
                    for (int i = 0; i < 4; i++) {
                        splitChild(tile->child[i], _renderContext);
                        testForSplit(tile->child[i]);		// so its frustum flags are set
                    }
                }
                return;
            }
        }
    }
}

void terrainManager::splitChild(quadtree_tile* _tile, RenderContext* _renderContext)
{
    const uint32_t cs_w = tile_numPixels / tile_cs_ThreadSize;
    const float2 origin = float2(_tile->origin.x, _tile->origin.z);
    const float outerSize = _tile->size * tile_numPixels / tile_InnerPixels;
    const float pixelSize = outerSize / tile_numPixels;


    {
        // why is it nessesary to clear before bicubic
        _renderContext->clearFbo(split.tileFbo.get(), glm::vec4(0.2f, 0.9f, 0.2f, 1.0f), 1.0f, 0, FboAttachmentType::All);
    }


    {
        FALCOR_PROFILE("bicubic");
        heightMap& elevationMap = elevationTileHashmap[_tile->elevationHash];

        float2 bicubicOffset = (origin - elevationMap.origin) / elevationMap.size;
        float S = pixelSize / elevationMap.size;
        float2 bicubicSize = float2(S, S);

        if (_tile->elevationHash == 0)
        {
            split.compute_tileBicubic.Vars()->setTexture("gInput", split.rootElevation);
        }
        split.compute_tileBicubic.Vars()["gConstants"]["offset"] = bicubicOffset;
        split.compute_tileBicubic.Vars()["gConstants"]["size"] = bicubicSize;
        split.compute_tileBicubic.Vars()["gConstants"]["hgt_offset"] = elevationMap.hgt_offset;
        split.compute_tileBicubic.Vars()["gConstants"]["hgt_scale"] = elevationMap.hgt_scale;
        split.compute_tileBicubic.dispatch(_renderContext, cs_w, cs_w);
    }
}

void terrainManager::splitRenderTopdown(quadtree_tile* _pTile, RenderContext* _renderContext)
{

}



void terrainManager::onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& _fbo)
{

    if (debug)
    {
        glm::vec4 srcRect = glm::vec4(0, 0, tile_numPixels, tile_numPixels);
        glm::vec4 dstRect;

        for (int i = 0; i < 8; i++)
        {
            dstRect = glm::vec4(250 + i * 150, 60, 250 + i * 150 + 128, 60 + 128);
            pRenderContext->blit(split.tileFbo->getColorTexture(i)->getSRV(0, 1, 0, 1), _fbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Linear);
        }

        dstRect = glm::vec4(250 + 8 * 150, 60, 250 + 8 * 150 + tile_numPixels * 2, 60 + tile_numPixels * 2);
        pRenderContext->blit(split.debug_texture->getSRV(0, 1, 0, 1), _fbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Point);

        //dstRect = vec4(512, 612, 1024, 1124);
        //pRenderContext->blit(mpCompressed_Normals->getSRV(0, 1, 0, 1), _fbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Linear);
    }
}


bool terrainManager::onKeyEvent(const KeyboardEvent& keyEvent)
{
    if (keyEvent.type == KeyboardEvent::Type::KeyPressed)
    {
        if (keyEvent.mods == Input::ModifierFlags::Ctrl)
        {
            if (keyEvent.key == Input::Key::D)          // debug
            {
                debug = !debug;
            }
        }
    }
    return false;
}


bool terrainManager::onMouseEvent(const MouseEvent& mouseEvent)
{
    return false;
}


void terrainManager::onHotReload(HotReloadFlags reloaded)
{}


