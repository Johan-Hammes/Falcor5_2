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
#include "Utils/UI/TextRenderer.h"

#pragma optimize("", off)

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

    parent = _parent;
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



void terrainManager::onLoad(RenderContext* pRenderContext, FILE* _logfile)
{
    terrafectors._logfile = _logfile;
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

                unsigned int packValue = ((mX + 1) << 9) + ((mY + 1) << 1);
                vertexData[index] = packValue;
            }
        }
        split.vertex_preload = Texture::create2D(tile_numPixels / 2, tile_numPixels / 2, Falcor::ResourceFormat::R16Uint, 1, 1, vertexData.data(), Resource::BindFlags::ShaderResource);
    }


    {
        split.buffer_tileCenters = Buffer::createStructured(sizeof(float4), numTiles);
        //split.buffer_tileCenter_readback = Buffer::create(sizeof(float4) * numTiles, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::Read);
        split.buffer_tileCenter_readback = Buffer::create(sizeof(float4) * numTiles, Resource::BindFlags::None, Buffer::CpuAccess::Read);
        //split.tileCenters.resize(numTiles);
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

        compressed_Normals_Array = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::R11G11B10Float, numTiles, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);	  // Now an array	  at 1024 tiles its 256 Mb , Fair bit but do-ablwe
        compressed_Albedo_Array = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::BC6HU16, numTiles, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
        compressed_PBR_Array = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::BC6HU16, numTiles, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
        height_Array = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::R32Float, numTiles, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);	  // Now an array	  1024 tiles is 64 MB - really nice and small

        split.drawArgs_quads = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
        split.drawArgs_plants = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
        split.drawArgs_tiles = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);

        split.buffer_feedback = Buffer::createStructured(sizeof(GC_feedback), 1);
        split.buffer_feedback_read = Buffer::createStructured(sizeof(GC_feedback), 1, Resource::BindFlags::None, Buffer::CpuAccess::Read);

        split.buffer_tiles = Buffer::createStructured(sizeof(gpuTile), numTiles);
        split.buffer_instance_quads = Buffer::createStructured(sizeof(instance_PLANT), numTiles * numQuadsPerTile);
        split.buffer_instance_plants = Buffer::createStructured(sizeof(instance_PLANT), numTiles * numPlantsPerTile);

        split.buffer_lookup_terrain = Buffer::createStructured(sizeof(instance_PLANT), 524288);
        split.buffer_lookup_quads = Buffer::createStructured(sizeof(instance_PLANT), 131072);
        split.buffer_lookup_plants = Buffer::createStructured(sizeof(instance_PLANT), 131072);

        split.buffer_terrain = Buffer::createStructured(sizeof(Terrain_vertex), numVertPerTile * numTiles);

        terrainShader.load("Samples/Earthworks_4/hlsl/render_Tiles.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
        terrainShader.Vars()->setBuffer("tiles", split.buffer_tiles);
        terrainShader.Vars()->setBuffer("tileLookup", split.buffer_lookup_terrain);

        compute_TerrainUnderMouse.load("Samples/Earthworks_4/hlsl/compute_terrain_under_mouse.hlsl");
        compute_TerrainUnderMouse.Vars()->setSampler("gSampler", sampler_Clamp);
        compute_TerrainUnderMouse.Vars()->setTexture("gHeight", height_Array);
        compute_TerrainUnderMouse.Vars()->setBuffer("tiles", split.buffer_tiles);
        compute_TerrainUnderMouse.Vars()->setBuffer("groundcover_feedback", split.buffer_feedback);

        // clear
        split.compute_tileClear.load("Samples/Earthworks_4/hlsl/compute_tileClear.hlsl");
        split.compute_tileClear.Vars()->setBuffer("feedback", split.buffer_feedback);

        // split merge
        split.compute_tileSplitMerge.load("Samples/Earthworks_4/hlsl/compute_tileSplitMerge.hlsl");
        split.compute_tileSplitMerge.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileSplitMerge.Vars()->setBuffer("feedback", split.buffer_feedback);

        // generate
        split.compute_tileGenerate.load("Samples/Earthworks_4/hlsl/compute_tileGenerate.hlsl");
        split.compute_tileGenerate.Vars()->setBuffer("quad_instance", split.buffer_instance_quads);
        split.compute_tileGenerate.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileGenerate.Vars()->setTexture("gNoise", split.noise_u16);
        split.compute_tileGenerate.Vars()->setTexture("gHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileGenerate.Vars()->setTexture("gEct1", split.tileFbo->getColorTexture(4));
        split.compute_tileGenerate.Vars()->setTexture("gEct2", split.tileFbo->getColorTexture(5));
        split.compute_tileGenerate.Vars()->setTexture("gEct3", split.tileFbo->getColorTexture(6));
        split.compute_tileGenerate.Vars()->setTexture("gEct4", split.tileFbo->getColorTexture(7));

        // passthrough
        split.compute_tilePassthrough.load("Samples/Earthworks_4/hlsl/compute_tilePassthrough.hlsl");
        split.compute_tilePassthrough.Vars()->setBuffer("quad_instance", split.buffer_instance_quads);
        split.compute_tilePassthrough.Vars()->setBuffer("plant_instance", split.buffer_instance_plants);
        split.compute_tilePassthrough.Vars()->setBuffer("feedback", split.buffer_feedback);
        split.compute_tilePassthrough.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tilePassthrough.Vars()->setTexture("gHgt", split.tileFbo->getColorTexture(0));
        split.compute_tilePassthrough.Vars()->setTexture("gNoise", split.noise_u16);

        // build lookup
        split.compute_tileBuildLookup.load("Samples/Earthworks_4/hlsl/compute_tileBuildLookup.hlsl");
        split.compute_tileBuildLookup.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileBuildLookup.Vars()->setBuffer("tileLookup", split.buffer_lookup_quads);
        split.compute_tileBuildLookup.Vars()->setBuffer("plantLookup", split.buffer_lookup_plants);
        split.compute_tileBuildLookup.Vars()->setBuffer("terrainLookup", split.buffer_lookup_terrain);
        split.compute_tileBuildLookup.Vars()->setBuffer("DrawArgs_Quads", split.drawArgs_quads);
        split.compute_tileBuildLookup.Vars()->setBuffer("DrawArgs_Terrain", split.drawArgs_tiles);
        split.compute_tileBuildLookup.Vars()->setBuffer("feedback", split.buffer_feedback);

        // bicubic
        split.compute_tileBicubic.load("Samples/Earthworks_4/hlsl/compute_tileBicubic.hlsl");
        split.compute_tileBicubic.Vars()->setSampler("linearSampler", sampler_Clamp);
        split.compute_tileBicubic.Vars()->setTexture("gOutput", split.bicubic_upsample_texture);
        split.compute_tileBicubic.Vars()->setTexture("gDebug", split.debug_texture);
        split.compute_tileBicubic.Vars()->setTexture("gOuthgt_TEMPTILLTR", split.tileFbo->getColorTexture(0));

        // ecotopes
        split.compute_tileEcotopes.load("Samples/Earthworks_4/hlsl/compute_tileEcotopes.hlsl");

        // normals
        split.compute_tileNormals.load("Samples/Earthworks_4/hlsl/compute_tileNormals.hlsl");
        split.compute_tileNormals.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileNormals.Vars()->setTexture("gOutNormals", split.normals_texture);
        split.compute_tileNormals.Vars()->setTexture("gOutput", split.debug_texture);
        split.compute_tileNormals.Vars()->setBuffer("tiles", split.buffer_tiles);

        // vertices
        split.compute_tileVerticis.load("Samples/Earthworks_4/hlsl/compute_tileVertices.hlsl");
        split.compute_tileVerticis.Vars()->setSampler("linearSampler", sampler_Clamp);
        split.compute_tileVerticis.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileVerticis.Vars()->setTexture("gOutVerts", split.vertex_A_texture);
        split.compute_tileVerticis.Vars()->setTexture("gDebug", split.debug_texture);
        split.compute_tileVerticis.Vars()->setBuffer("tileCenters", split.buffer_tileCenters);
        split.compute_tileVerticis.Vars()->setBuffer("tiles", split.buffer_tiles);

        // jumpflood
        // It may even be faster to set this up twice and hop between the two
        split.compute_tileJumpFlood.load("Samples/Earthworks_4/hlsl/compute_tileJumpFlood.hlsl");
        split.compute_tileJumpFlood.Vars()->setTexture("gDebug", split.debug_texture);

        // delaunay
        split.compute_tileDelaunay.load("Samples/Earthworks_4/hlsl/compute_tileDelaunay.hlsl");
        split.compute_tileDelaunay.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileDelaunay.Vars()->setTexture("gInVerts", split.vertex_A_texture);
        split.compute_tileDelaunay.Vars()->setBuffer("VB", split.buffer_terrain);
        split.compute_tileDelaunay.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileDelaunay.Vars()->setBuffer("tileDrawargsArray", split.drawArgs_tiles);

        // elevation mipmap
        split.compute_tileElevationMipmap.load("Samples/Earthworks_4/hlsl/compute_tileElevationMipmap.hlsl");
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
        split.compute_bc6h.load("Samples/Earthworks_4/hlsl/compute_bc6h.hlsl");
        split.compute_bc6h.Vars()->setTexture("gOutput", split.bc6h_texture);
    }

    allocateTiles(1024);

    elevationCache.resize(45);
    loadElevationHash();

    init_TopdownRender();

    terrafectorEditorMaterial::rootFolder = settings.dirResource + "/";
    terrafectors.loadPath(settings.dirRoot + "/terrafectors/");
}


void terrainManager::init_TopdownRender()
{
    split.tileCamera = Camera::create();

    terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials = Buffer::createStructured(sizeof(TF_material), 1024); // FIXME hardcoded
    split.shader_spline3D.load("Samples/Earthworks_4/hlsl/render_spline.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
    split.shader_spline3D.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);
    //split.shader_spline3D.Program()->getReflector()
    //split.shader_splineTerrafector.load("Samples/Earthworks_4/hlsl/render_splineTerrafector.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
    //split.shader_splineTerrafector.State()->setFbo(split.tileFbo);
    //split.shader_splineTerrafector.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);


    // mesh terrafector shader
 //   split.shader_meshTerrafector.load("Samples/Earthworks_4/hlsl/render_meshTerrafector.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
  //  split.shader_meshTerrafector.Vars()->setSampler("SMP", sampler_Trilinear);
  //  split.shader_meshTerrafector.Vars()["PerFrameCB"]["gConstColor"] = false;
  //  split.shader_meshTerrafector.State()->setFbo(split.tileFbo);
    //mpSplineShaderTerrafector.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);

    DepthStencilState::Desc depthDesc;
    depthDesc.setDepthEnabled(true);
    depthDesc.setDepthWriteMask(false);
    depthDesc.setStencilEnabled(false);

    depthDesc.setDepthFunc(DepthStencilState::Func::Greater);
    split.depthstateFuther = DepthStencilState::create(depthDesc);

    depthDesc.setDepthFunc(DepthStencilState::Func::LessEqual);
    split.depthstateCloser = DepthStencilState::create(depthDesc);

    depthDesc.setDepthFunc(DepthStencilState::Func::Always);
    split.depthstateAll = DepthStencilState::create(depthDesc);

    RasterizerState::Desc rsDesc;
    rsDesc.setFillMode(RasterizerState::FillMode::Solid).setCullMode(RasterizerState::CullMode::None);
    split.rasterstateSplines = RasterizerState::create(rsDesc);

    BlendState::Desc blendDesc;
    blendDesc.setRtBlend(0, true);
    blendDesc.setRtParams(0, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::Zero, BlendState::BlendFunc::Zero);
    split.blendstateSplines = BlendState::create(blendDesc);

    blendDesc.setIndependentBlend(true);
    for (int i = 0; i < 8; i++)
    {
        // clear all
        blendDesc.setRenderTargetWriteMask(i, true, true, true, true);
        blendDesc.setRtBlend(i, true);
        blendDesc.setRtParams(i, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha);
    }

    blendDesc.setRtParams(0, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::One, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::One, BlendState::BlendFunc::OneMinusSrcAlpha);
    split.blendstateRoadsCombined = BlendState::create(blendDesc);

    splines.bezierData = Buffer::createStructured(sizeof(cubicDouble), splines.maxBezier);
    splines.indexData = Buffer::createStructured(sizeof(bezierLayer), splines.maxIndex);

    splines.dynamic_bezierData = Buffer::createStructured(sizeof(cubicDouble), splines.maxDynamicBezier);
    splines.dynamic_indexData = Buffer::createStructured(sizeof(bezierLayer), splines.maxDynamicIndex);
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

    if (requestPopupDebug) {
        ImGui::OpenPopup("debug");
        requestPopupDebug = false;
    }
    if (ImGui::BeginPopup("debug"))      // modal
    {
        ImGui::Text("%d", m_used.size());
        ImGui::EndPopup();
    }

    Gui::Window rightPanel(_gui, "##rightPanel", { 200, 200 }, { 100, 100 });
    {
        ImGui::PushFont(_gui->getFont("roboto_20"));

        ImGui::PushItemWidth(ImGui::GetWindowWidth() - 20);
        if (ImGui::Combo("###modeSelector", &terrainMode, "ortho photo\0Ecotope\0Terrafector\0Bezier road network\0")) { ; }
        ImGui::PopItemWidth();
        ImGui::NewLine();

        switch (terrainMode)
        {
        case 0: break;
        case 1: break;
        case 2: break;
        case 3:
            mRoadNetwork.renderGUI(_gui);

            //if (ImGui::Button("bake - EVO", ImVec2(W, 0))) { bake(false); }
            //if (ImGui::Button("bake - MAX", ImVec2(W, 0))) { bake(true); }

            /*if (ImGui::Checkbox("show baked", &bSplineAsTerrafector)) { reset(true); }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("'b'");
            ImGui::Checkbox("show road icons", &showRoadOverlay);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("'n'");
            ImGui::Checkbox("show road splines", &showRoadSpline);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("'m'");*/

            ImGui::NewLine();

            auto& style = ImGui::GetStyle();
            style.ButtonTextAlign = ImVec2(0.0, 0.5);
            ImGui::DragFloat("overlay", &gis_overlay.strenght, 0.01f, 0, 1, "%1.2f strength");

            ImGui::DragFloat("redStrength", &gis_overlay.redStrength, 0.01f, 0, 1);
            ImGui::DragFloat("redScale", &gis_overlay.redScale, 0.01f, 0, 100);
            ImGui::DragFloat("redOffset", &gis_overlay.redOffset, 0.01f, 0, 1);

            ImGui::Separator();
            ImGui::DragFloat("jp2 quality", &bake.quality, 0.0001f, 0.0001f, 0.01f, "%3.5f");
            break;

        }
        ImGui::PopFont();
    }
    rightPanel.release();

    if (!ImGui::IsAnyWindowHovered())
    {
        char TTTEXT[1024];
        sprintf(TTTEXT, "%d (%3.1f, %3.1f, %3.1f)\n", split.feedback.tum_idx, split.feedback.tum_Position.x, split.feedback.tum_Position.y, split.feedback.tum_Position.z);
        sprintf(TTTEXT, "splinetest %d (%d, %d) \n", splineTest.index, splineTest.bVertex, splineTest.bSegment);
        ImGui::SetTooltip(TTTEXT);
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
        if (ImGui::MenuItem("debug"))
        {
            requestPopupDebug = true;
        }
        ImGui::EndMenu();
    }

    float x = ImGui::GetCursorPosX();
    ImGui::SetCursorPos(ImVec2(screenSize.x - 600, 0));
    ImGui::Text(lastfile.terrain.c_str());
    ImGui::SetCursorPos(ImVec2(x, 0));
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


void terrainManager::testForSurfaceMain()
{
    bool bCM = true;
    for (auto& tile : m_used)
    {
        bool surface = tile->parent && tile->parent->main_ShouldSplit && tile->child[0] == nullptr;
        bool bCM2 = true;
        if (surface) {
            frustumFlags[tile->index] |= 1 << 20;
        }
    }
}

void terrainManager::testForSurfaceEnv()
{
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



    return false;
}

bool terrainManager::testFrustum(quadtree_tile* _tile)
{
    return true;
}

void terrainManager::markChildrenForRemove(quadtree_tile* _tile)
{
    for (uint i = 0; i < 4; i++) {
        if (_tile->child[i]) {
            markChildrenForRemove(_tile->child[i]);
            _tile->child[i]->forRemove = true;
            _tile->child[i] = nullptr;
        }
    }
}


uint terrainManager::gisHash(glm::vec3 _position)
{
    uint z = (uint)floor((_position.z + (settings.size / 2.0f)) * 16.0f / settings.size);
    uint x = (uint)floor((_position.x + (settings.size / 2.0f)) * 16.0f / settings.size);

    return (z << 16) + x;
}

void terrainManager::gisReload(glm::vec3 _position)
{
    uint hash = gisHash(_position);
    if (hash != gis_overlay.hash)
    {
        gis_overlay.hash = hash;
        uint x = hash & 0xffff;
        uint z = hash >> 16;

        std::filesystem::path path = settings.dirRoot + "/overlay/" + char(65 + hash & 0xff) + "_" + std::to_string(z) + "_image.dds";
        gis_overlay.texture = Texture::createFromFile(path, true, true, Resource::BindFlags::ShaderResource);

        gis_overlay.box = glm::vec4(-350 - (settings.size / 2.0f) + x * 2500, -350 - (settings.size / 2.0f) + z * 2500, 3200, 3200);    // fixme this sort of asumes 40 x 40 km
    }
}


void terrainManager::clearCameras()
{
    for (uint i = 0; i < CameraType_MAX; i++) {
        cameraViews[i].bUse = false;
    }

}

void terrainManager::setCamera(unsigned int _index, glm::mat4 viewMatrix, glm::mat4 projMatrix, float3 position, bool b_use, float _resolution)
{
    if (_index < CameraType_MAX)
    {
        cameraOrigin = position;	// SURE so last set camera does this, but they should just about all be the same right?

        cameraViews[_index].bUse = b_use;
        cameraViews[_index].resolution = _resolution;
        cameraViews[_index].view = viewMatrix;
        cameraViews[_index].proj = projMatrix;
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


bool terrainManager::update(RenderContext* _renderContext)
{
    bool dirty = false;

    updateDynamicRoad(false);
    mRoadNetwork.testHit(split.feedback.tum_Position);
    if (mRoadNetwork.isDirty)
    {
        splines.numStaticSplines = __min((int)roadNetwork::staticBezierData.size(), splines.maxBezier);
        splines.numStaticSplinesIndex = __min((int)roadNetwork::staticIndexData.size(), splines.maxIndex);
        splines.bezierData->setBlob(roadNetwork::staticBezierData.data(), 0, splines.numStaticSplines * sizeof(cubicDouble));
        splines.indexData->setBlob(roadNetwork::staticIndexData.data(), 0, splines.numStaticSplinesIndex * sizeof(bezierLayer));
    }

    split.compute_tileClear.dispatch(_renderContext, 1, 1);
    memset(frustumFlags, 0, sizeof(unsigned int) * 2048);		// clear all

    for (auto& tile : m_used)
    {
        tile->reset();
    }

    for (auto& tile : m_used)
    {
        dirty |= testForSplit(tile);

        if (!tile->main_ShouldSplit && tile->child[0]) {
            markChildrenForRemove(tile);
        }
    }

    for (auto itt = m_used.begin(); itt != m_used.end();)               // do al merges
    {
        if ((*itt)->forRemove) {
            (*itt)->forRemove = false;
            m_free.push_back(*itt);
            itt = m_used.erase(itt);
        }
        else {
            ++itt;
        }
    }

    splitOne(_renderContext);

    {
        testForSurfaceMain();

        for (auto& tile : m_used)
        {
            //bool surface = tile->parent && tile->parent->main_ShouldSplit && tile->child[0] == nullptr;
            bool surface = tile->parent && tile->child[0] == nullptr;
            bool bCM2 = true;
            if (surface) {
                frustumFlags[tile->index] |= 1 << 20;
            }
        }

        auto pCB = split.compute_tileBuildLookup.Vars()->getParameterBlock("gConstants");
        pCB->setBlob(frustumFlags, 0, 2048 * sizeof(unsigned int));	// FIXME number of tiles
        uint cnt = (numTiles + 31) >> 5;
        split.compute_tileBuildLookup.dispatch(_renderContext, cnt, 1);
    }

    // if (dirty)
    {
        // readback of tile centers
        _renderContext->copyResource(split.buffer_tileCenter_readback.get(), split.buffer_tileCenters.get());
        const float4* pData = (float4*)split.buffer_tileCenter_readback.get()->map(Buffer::MapType::Read);
        std::memcpy(&split.tileCenters, pData, sizeof(float4) * numTiles);
        split.buffer_tileCenter_readback.get()->unmap();
        for (int i = 0; i < m_tiles.size(); i++)
        {
            m_tiles[i].origin.y = split.tileCenters[i].x;
            m_tiles[i].boundingSphere.y = split.tileCenters[i].x;
        }
    }

    if (hasChanged) {
        hasChanged = false;
        return true;
    }
    return false;
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
    /* Bloody hell, pick the best one to do*/
    if (m_free.size() < 4) return;


    // FIXME PICK A BETTER ONE HERE
    for (auto& tile : m_used)
    {
        if (tile->forSplit)
        {
            hasChanged = true;
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


    {
        splitRenderTopdown(_tile, _renderContext);
        _renderContext->copySubresource(height_Array.get(), _tile->index, split.tileFbo->getColorTexture(0).get(), 0);  // for picking only
    }

    {
        FALCOR_PROFILE("compute");

        {
            // Do this early to avoid stalls
            FALCOR_PROFILE("copy_VERT");
            _renderContext->copyResource(split.vertex_B_texture.get(), split.vertex_clear.get());			// not 100% sure this clear is needed
            _renderContext->copyResource(split.vertex_A_texture.get(), split.vertex_preload.get());
        }

        {
            // compress and copy colour data
            FALCOR_PROFILE("compress_copy_Albedo");
            split.compute_bc6h.Vars()->setTexture("gSource", split.tileFbo->getColorTexture(1));
            split.compute_bc6h.dispatch(_renderContext, cs_w / 4, cs_w / 4);
            _renderContext->copySubresource(compressed_Albedo_Array.get(), _tile->index, split.bc6h_texture.get(), 0);
        }

        {
            FALCOR_PROFILE("normals");
            //split.compute_tileNormals.Vars()["gConstants"]["pixSize"] = pixelSize;
            split.compute_tileNormals.dispatch(_renderContext, cs_w, cs_w);
        }

        {
            // for verts. Do this early to avoid stalls
            FALCOR_PROFILE("MIP_heights");
            split.tileFbo->getColorTexture(0)->generateMips(_renderContext);
            //cs_tile_ElevationMipmap.dispatch(pRenderContext, w / 4, h / 4); 			// Ek aanvaar die werk en is vinniger 0.057 -> 0.027   maar syncronization issues hier wat ek in A gaan oplos
        }

        {
            FALCOR_PROFILE("copy_PBR");
            split.compute_bc6h.Vars()->setTexture("gSource", split.tileFbo->getColorTexture(2));
            split.compute_bc6h.dispatch(_renderContext, cs_w / 4, cs_w / 4);
            _renderContext->copySubresource(compressed_PBR_Array.get(), _tile->index, split.bc6h_texture.get(), 0);
        }

        {
            FALCOR_PROFILE("verticies");
            split.compute_tileVerticis.Vars()->setSampler("linearSampler", sampler_Clamp);
            split.compute_tileVerticis.Vars()["gConstants"]["constants"] = float4(pixelSize, 1, 2, _tile->index);
            split.compute_tileVerticis.dispatch(_renderContext, cs_w / 2, cs_w / 2);
        }

        {
            FALCOR_PROFILE("copy normals");
            _renderContext->copySubresource(compressed_Normals_Array.get(), _tile->index, split.normals_texture.get(), 0);
            /*
            PROFILE(normalsBC6H);
            csBC6H_compressor.getVars()->setTexture("gSource", mpNormals);
            csBC6H_compressor.dispatch(pRenderContext, w / 4, h / 4);
            pRenderContext->copySubresource(mpCompressed_Normals_Array.get(), pTile->m_idx, mpCompressed_TMP.get(), 0);
            */
        }

        /*
        // both of these need to chnage to work on child not parent
        {
          FALCOR_PROFILE("cs_tile_Generate");
          uint32_t w_gen = tile_numPixels / tile_cs_ThreadSize_Generate - 1;
          uint32_t h_gen = tile_numPixels / tile_cs_ThreadSize_Generate - 1;
          cs_tile_Generate.getCB()->setVariable<uint>(0, pTile->m_idx);
          cs_tile_Generate.dispatch(pRenderContext, w_gen, h_gen);
        }

        {
          FALCOR_PROFILE("cs_tile_Passthrough");
          uint cnt = (numQuadsPerTile) >> 8;	  // FIXME - hiesdie oordoen is es dan stadig - dit behoort Compute indoirect te wees  en die regte getal te gebruik
          cs_tile_Passthrough.getCB()->setVariable<uint>(0, pTile->m_idx);
          cs_tile_Passthrough.getCB()->setVariable<uint>(sizeof(int), pTile->m_X & 0x1);
          cs_tile_Passthrough.getCB()->setVariable<uint>(sizeof(int) * 2, pTile->m_Y & 0x1);
          cs_tile_Passthrough.dispatch(pRenderContext, cnt, 1);
        }
        */

        // jumpflood algorithm (1+JFA+1) tp build voroinoi diagram ------------------------------------------------------------------------
        // ek weet 32 en 6 loops is goed
        {
            FALCOR_PROFILE("jumpflood");

            uint step = 2;
            for (int j = 0; j < 4; j++) {
                split.compute_tileJumpFlood.Vars()["gConstants"]["step"] = step;
                if (j & 0x1) {
                    split.compute_tileJumpFlood.Vars()->setTexture("gInVerts", split.vertex_B_texture);
                    split.compute_tileJumpFlood.Vars()->setTexture("gOutVerts", split.vertex_A_texture);
                }
                else {
                    split.compute_tileJumpFlood.Vars()->setTexture("gInVerts", split.vertex_A_texture);
                    split.compute_tileJumpFlood.Vars()->setTexture("gOutVerts", split.vertex_B_texture);

                }

                split.compute_tileJumpFlood.dispatch(_renderContext, cs_w / 2, cs_w / 2);
                step /= 2;
                if (step < 1) step = 1;
            }

            split.compute_tileJumpFlood.Vars()->setTexture("gInVerts", NULL);
            split.compute_tileJumpFlood.Vars()->setTexture("gOutVerts", NULL);
        }

        {
            FALCOR_PROFILE("delaunay");
            uint32_t firstVertex = _tile->index * numVertPerTile;
            uint32_t zero = 0;
            split.buffer_terrain->getUAVCounter()->setBlob(&zero, 0, sizeof(uint32_t));	// damn, I misuse increment, count in 1's but write in 3's
            split.compute_tileDelaunay.Vars()["gConstants"]["tile_Index"] = _tile->index;
            split.compute_tileDelaunay.dispatch(_renderContext, cs_w / 2, cs_w / 2);
            //mpRenderContext->copyResource(mpDefaultFBO->getColorTexture(0).get(), mpVertsMap.get());
            //mpVertsMap->captureToFile(0, 0, "e:\\voroinoi_verts.png");
        }
    }
}



void terrainManager::splitRenderTopdown(quadtree_tile* _pTile, RenderContext* _renderContext)
{
    FALCOR_PROFILE("renderTopdown");

    // set up the camera -----------------------
    float s = _pTile->size / 2.0f;
    float x = _pTile->origin.x + s;
    float z = _pTile->origin.z + s;
    rmcv::mat4 view, proj;
    view[0] = glm::vec4(1, 0, 0, 0);
    view[1] = glm::vec4(0, 0, 1, 0);
    view[2] = glm::vec4(0, -1, 0, 0);
    view[3] = glm::vec4(-x, z, 0, 1);

    s *= 256.0f / 248.0f;

    if (_pTile->lod == 6 && _pTile->y == 41 && _pTile->x == 33)
    {
        bool bCM = true;
    }
    split.tileCamera->setViewMatrix(view);

    proj = rmcv::toRMCV(glm::orthoLH(-s, s, -s, s, -10000.0f, 10000.0f));
    split.tileCamera->setProjectionMatrix(proj);
    split.tileCamera->getData();

    //terrafectors.render(_renderContext, split.tileCamera, mpTileGraphicsState, mpTileTopdownVars);

    //??? should probably be in the roadnetwork code, but look at the optimize step first
    if (splineAsTerrafector)           // Now render the roadNetwork
    {
        //auto& block = split.shader_splineTerrafector.Vars()->getParameterBlock("gmyTextures");
        //ShaderVar& var = block->findMember("T");
        //terrafectorEditorMaterial::static_materials.setTextures(var);


        split.shader_splineTerrafector.State()->setRasterizerState(split.rasterstateSplines);

        split.shader_splineTerrafector.Vars()["PerFrameCB"]["gConstColor"] = false;

        split.shader_splineTerrafector.Vars()["gConstantBuffer"]["view"] = view;
        split.shader_splineTerrafector.Vars()["gConstantBuffer"]["proj"] = proj;
        split.shader_splineTerrafector.Vars()["gConstantBuffer"]["viewproj"] = proj * view;
        split.shader_splineTerrafector.Vars()["gConstantBuffer"]["numSubdiv"] = split.numSubdiv;
        //split.shader_splineTerrafector.Vars()["gConstantBuffer"]["bHidden"] = 0;
        //split.shader_splineTerrafector.Vars()["gConstantBuffer"]["dLeft"] = 0.0f;
        //split.shader_splineTerrafector.Vars()["gConstantBuffer"]["dRight"] = 0.0f;
        //split.shader_splineTerrafector.Vars()["gConstantBuffer"]["colour"] = float4(0.01, 0.01, 0.01, 0.5);

        // Visible ********************************************************************************
        split.shader_splineTerrafector.State()->setDepthStencilState(split.depthstateAll);

        split.shader_splineTerrafector.State()->setBlendState(split.blendstateRoadsCombined);
        split.shader_splineTerrafector.Vars()->setBuffer("indexData", splines.indexData);
        split.shader_splineTerrafector.Vars()->setBuffer("splineData", splines.bezierData);     // not created yet
        split.shader_splineTerrafector.drawIndexedInstanced(_renderContext, split.numSubdiv * 6, splines.numIndex);
    }
}



void terrainManager::onFrameRender(RenderContext* _renderContext, const Fbo::SharedPtr& _fbo, const Camera::SharedPtr _camera, GraphicsState::SharedPtr _graphicsState)
{
    gisReload(_camera->getPosition());

    glm::mat4 V = toGLM(_camera->getViewMatrix());
    glm::mat4 P = toGLM(_camera->getProjMatrix());
    glm::mat4 VP = toGLM(_camera->getViewProjMatrix());
    rmcv::mat4 view, proj, viewproj;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            view[j][i] = V[j][i];
            proj[j][i] = P[j][i];
            viewproj[j][i] = VP[j][i];
        }
    }


    {
        FALCOR_PROFILE("terrainManager");

        _graphicsState->setFbo(_fbo);

        terrainShader.State() = _graphicsState;
        terrainShader.State()->setFbo(_fbo);

        terrainShader.Vars()->setTexture("gAlbedoArray", compressed_Albedo_Array);
        terrainShader.Vars()->setTexture("gPBRArray", compressed_PBR_Array);
        terrainShader.Vars()->setTexture("gNormArray", compressed_Normals_Array);
        terrainShader.Vars()->setTexture("gGISAlbedo", gis_overlay.texture);

        terrainShader.Vars()->setSampler("gSmpAniso", sampler_ClampAnisotropic);

        terrainShader.Vars()->setBuffer("VB", split.buffer_terrain);

        terrainShader.Vars()["gConstantBuffer"]["view"] = view;
        terrainShader.Vars()["gConstantBuffer"]["proj"] = proj;
        terrainShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        terrainShader.Vars()["gConstantBuffer"]["eye"] = _camera->getPosition();


        terrainShader.Vars()["PerFrameCB"]["gisOverlayStrength"] = gis_overlay.strenght;
        terrainShader.Vars()["PerFrameCB"]["showGIS"] = (int)gis_overlay.show;
        terrainShader.Vars()["PerFrameCB"]["gisBox"] = gis_overlay.box;

        terrainShader.Vars()["PerFrameCB"]["redStrength"] = gis_overlay.redStrength;
        terrainShader.Vars()["PerFrameCB"]["redScale"] = gis_overlay.redScale;
        terrainShader.Vars()["PerFrameCB"]["redOffset"] = gis_overlay.redOffset;

        /*
        terrainShader.Vars()["PerFrameCB"]["screenSize"] = screenSize;
        */
        /*
        terrainShader.Vars()->setTexture("gSmokeAndDustInscatter", nearInscatter);
        terrainShader.Vars()->setTexture("gSmokeAndDustOutscatter", nearOutscatter);
        terrainShader.Vars()->setTexture("gAtmosphereInscatter", farInscatter);
        terrainShader.Vars()->setTexture("gAtmosphereOutscatter", farOutscatter);
        */

        terrainShader.renderIndirect(_renderContext, split.drawArgs_tiles);
    }

    if ((splines.numStaticSplines || splines.numDynamicSplines) && showRoadSpline && !bSplineAsTerrafector)
    {
        FALCOR_PROFILE("splines");

        split.shader_spline3D.State()->setFbo(_fbo);
        split.shader_spline3D.State()->setRasterizerState(split.rasterstateSplines);
        split.shader_spline3D.State()->setBlendState(split.blendstateSplines);
        split.shader_spline3D.State()->setDepthStencilState(split.depthstateAll);

        split.shader_spline3D.Vars()["gConstantBuffer"]["view"] = view;
        split.shader_spline3D.Vars()["gConstantBuffer"]["proj"] = proj;
        split.shader_spline3D.Vars()["gConstantBuffer"]["viewproj"] = viewproj;

        split.shader_spline3D.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);

        auto& block = split.shader_spline3D.Vars()->getParameterBlock("gmyTextures");
        ShaderVar& var = block->findMember("T");        // FIXME pre get
        terrafectorEditorMaterial::static_materials.setTextures(var);

        if (splines.numDynamicSplines > 0)
        {
            split.shader_spline3D.Vars()->setBuffer("splineData", splines.dynamic_bezierData);
            split.shader_spline3D.Vars()->setBuffer("indexData", splines.dynamic_indexData);
            split.shader_spline3D.drawIndexedInstanced(_renderContext, 64 * 6, splines.numDynamicSplines);
        }
        else
        {
            split.shader_spline3D.Vars()->setBuffer("splineData", splines.bezierData);
            split.shader_spline3D.Vars()->setBuffer("indexData", splines.indexData);
            split.shader_spline3D.drawIndexedInstanced(_renderContext, 64 * 6, splines.numStaticSplines);
        }
    }

    {
        FALCOR_PROFILE("terrain_under_mouse");
        compute_TerrainUnderMouse.Vars()["gConstants"]["mousePos"] = mousePosition;
        compute_TerrainUnderMouse.Vars()["gConstants"]["mouseDir"] = mouseDirection;
        compute_TerrainUnderMouse.Vars()["gConstants"]["mouseCoords"] = mouseCoord;
        compute_TerrainUnderMouse.Vars()->setTexture("gHDRBackbuffer", _fbo->getColorTexture(0));

        compute_TerrainUnderMouse.dispatch(_renderContext, 32, 1);

        _renderContext->copyResource(split.buffer_feedback_read.get(), split.buffer_feedback.get());

        const uint8_t* pData = (uint8_t*)split.buffer_feedback_read->map(Buffer::MapType::Read);
        std::memcpy(&split.feedback, pData, sizeof(GC_feedback));
        split.buffer_feedback_read->unmap();

        mouse.hit = false;
        if (split.feedback.tum_idx > 0)
        {
            mouse.hit = true;
            mouse.terrain = split.feedback.tum_Position;
            mouse.cameraHeight = split.feedback.heightUnderCamera;
            mouse.toGround = mouse.terrain - _camera->getPosition();
            mouse.mouseToHeightRatio = glm::length(mouse.toGround) / (_camera->getPosition().y - mouse.cameraHeight);
            if (!ImGui::IsMouseDown(1)) {
                mouse.pan = mouse.terrain;
                mouse.panDistance = glm::length(mouse.toGround);
            }
            if (!ImGui::IsMouseDown(2)) mouse.orbit = mouse.terrain;

        }
    }

    if (debug)
    {
        glm::vec4 srcRect = glm::vec4(0, 0, tile_numPixels, tile_numPixels);
        glm::vec4 dstRect;

        for (int i = 0; i < 8; i++)
        {
            dstRect = glm::vec4(250 + i * 150, 60, 250 + i * 150 + 128, 60 + 128);
            _renderContext->blit(split.tileFbo->getColorTexture(i)->getSRV(0, 1, 0, 1), _fbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Linear);
        }

        dstRect = glm::vec4(250 + 8 * 150, 60, 250 + 8 * 150 + tile_numPixels, 60 + tile_numPixels);
        _renderContext->blit(split.debug_texture->getSRV(0, 1, 0, 1), _fbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Point);


        //dstRect = vec4(512, 612, 1024, 1124);
        //pRenderContext->blit(mpCompressed_Normals->getSRV(0, 1, 0, 1), _fbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Linear);
        char debugTxt[1024];
        TextRenderer::setColor(float3(0.1, 0.1, 0.1));
        sprintf(debugTxt, "%d of %d tiles used", (int)m_used.size(), (int)m_tiles.size());
        TextRenderer::render(_renderContext, debugTxt, _fbo, { 100, 300 });

        sprintf(debugTxt, "%d of %d tiles / verts", split.feedback.numTerrainTiles, split.feedback.numTerrainVerts);
        TextRenderer::render(_renderContext, debugTxt, _fbo, { 100, 320 });
    }
}



void terrainManager::updateDynamicRoad(bool _bezierChanged) {
    // active road ----------------------------------------------------------------------------------------------------------------
    splineTest.bSegment = false;
    splineTest.bVertex = false;
    splineTest.testDistance = 1000;
    splineTest.bCenter = false;
    splineTest.cornerNum = -1;
    splineTest.pos = split.feedback.tum_Position;
    splineTest.bStreetview = false;

    //mRoadNetwork.testHit(feedback.tum_Position);

    static bool bRefresh;
    if (_bezierChanged) { bRefresh = true; }
    if (bRefresh || mRoadNetwork.isDirty)
    {
        splines.numDynamicSplines = 0;
        splines.numDynamicSplinesIndex = 0;
        mRoadNetwork.updateDynamicRoad();
        splines.numDynamicSplines = __min(splines.maxDynamicBezier, (int)roadNetwork::staticBezierData.size());
        splines.numDynamicSplinesIndex = __min(splines.maxDynamicBezier * 10, (int)roadNetwork::staticIndexData.size());
        splines.dynamic_bezierData->setBlob(roadNetwork::staticBezierData.data(), 0, splines.numDynamicSplines * sizeof(cubicDouble));
        splines.dynamic_indexData->setBlob(roadNetwork::staticIndexData.data(), 0, splines.numDynamicSplinesIndex * sizeof(bezierLayer));

        mRoadNetwork.isDirty = false;
    }
    if (!_bezierChanged) { bRefresh = false; }

    mRoadNetwork.intersectionSelectedRoad = nullptr;


    if (mRoadNetwork.currentRoad && mRoadNetwork.currentRoad->points.size() >= 3)
    {
        //mSpriteRenderer.clearDynamic();
        mRoadNetwork.currentRoad->testAgainstPoint(&splineTest);

        // mouse to spline markers ---------------------------------------------------------------------------------------
        /*
        if (splineTest.bVertex) {
            mSpriteRenderer.pushMarker(splineTest.returnPos, 4, 3.0f);
        }
        if (splineTest.bSegment) {
            mSpriteRenderer.pushMarker(splineTest.returnPos, 1);
        }

        // show selection
        for (auto& point : mRoadNetwork.currentRoad->points)
        {
            mSpriteRenderer.pushMarker(point.anchor, 2 - point.constraint, 0.5f);

        }

        if (mRoadNetwork.AI_path_mode)
        {

            int ssize = (int)mRoadNetwork.pathBezier.segments.size();

            for (int i = 0; i < ssize - 1; i++) {
                float3 O1 = mRoadNetwork.pathBezier.segments[i].optimalPos;
                float3 O2 = mRoadNetwork.pathBezier.segments[i + 1].optimalPos;
                mSpriteRenderer.pushLine(O1, O2, 1, 0.5f);

                O1 = mRoadNetwork.pathBezier.segments[i].A;
                O2 = mRoadNetwork.pathBezier.segments[i].B;
                mSpriteRenderer.pushLine(O1, O2, 1, 0.3f);
            }
        }

        mSpriteRenderer.loadDynamic();
        */
    }




    /*

    if (mRoadNetwork.currentIntersection)
    {
        mRoadNetwork.updateDynamicRoad();

        mSpriteRenderer.clearDynamic();
        intersection* I = mRoadNetwork.currentIntersection;

        //mSpriteRenderer.pushMarker(I->anchor, 3 + I->buildQuality, 4.0);			// anchor
        float distAnchor = glm::length(I->anchor - splineTest.pos);
        if (distAnchor < splineTest.testDistance && distAnchor < 8.0f) {
            splineTest.testDistance = distAnchor;
            splineTest.bCenter = true;
            splineTest.bStreetview = false;
        }


        uint RLsize = (uint)I->roadLinks.size();
        for (uint i = 0; i < RLsize; i++) {
            intersectionRoadLink* R = &I->roadLinks[i];
            intersectionRoadLink* RB = &I->roadLinks[(i + RLsize - 1) % RLsize];


            // First test against all the roads attached, BUT EXCLUDE the first vertex
            if (R->roadPtr->testAgainstPoint(&splineTest, false)) {
                mRoadNetwork.intersectionSelectedRoad = R->roadPtr;
                splineTest.bCenter = false;
            }

            float3 Z = R->corner_A - splineTest.pos;
            Z.y = 0;
            float distCorner = glm::length(Z);
            if ((distCorner < 3.0) && (distCorner < splineTest.testDistance)) {
                splineTest.testDistance = distCorner;
                splineTest.bCenter = false;
                splineTest.bSegment = false;
                splineTest.bVertex = false;
                splineTest.cornerNum = i;
                splineTest.cornerFlag = 0;
                mRoadNetwork.intersectionSelectedRoad = nullptr;

            }


            float distA;
            if (R->cornerType == typeOfCorner::artistic) {
                Z = (R->corner_A + R->cornerTangent_A) - splineTest.pos;
                Z.y = 0;
                distA = glm::length(Z);
                if (distA < 3.0 && distA < splineTest.testDistance) {
                    splineTest.testDistance = distA;
                    splineTest.bCenter = false;
                    splineTest.bSegment = false;
                    splineTest.bVertex = false;
                    splineTest.cornerNum = i;
                    splineTest.cornerFlag = -1;
                    mRoadNetwork.intersectionSelectedRoad = nullptr;

                }
            }


            if (RB->cornerType == typeOfCorner::artistic) {
                Z = (R->corner_B + R->cornerTangent_B) - splineTest.pos;
                Z.y = 0;
                distA = glm::length(Z);
                if (distA < 5.0 && distA < splineTest.testDistance) {
                    splineTest.testDistance = distA;
                    splineTest.bCenter = false;
                    splineTest.bSegment = false;
                    splineTest.bVertex = false;
                    splineTest.cornerNum = i;
                    splineTest.cornerFlag = 1;
                    mRoadNetwork.intersectionSelectedRoad = nullptr;
                }
            }


            Z = (I->anchor + R->tangentVector) - splineTest.pos;
            Z.y = 0;
            distA = glm::length(Z);
            if (distA < 5.0 && distA < splineTest.testDistance) {
                splineTest.testDistance = distA;
                splineTest.bCenter = false;
                splineTest.cornerNum = i;
                splineTest.cornerFlag = 2;
                mRoadNetwork.intersectionSelectedRoad = nullptr;
            }




            mSpriteRenderer.pushMarker(R->corner_A, 2 - R->cornerType, 0.5f);						// corner anchor
            //if (R->cornerType == typeOfCorner::artistic)
            {
                mSpriteRenderer.pushLine(R->corner_A, R->corner_A + R->cornerTangent_A, R->cornerType, 0.2f);
                mSpriteRenderer.pushLine(R->corner_B, R->corner_B + R->cornerTangent_B, RB->cornerType, 0.2f);
                mSpriteRenderer.pushMarker(R->corner_A + R->cornerTangent_A, 2 - R->cornerType, 0.5f);			// anchor
                mSpriteRenderer.pushMarker(R->corner_B + R->cornerTangent_B, 2 - RB->cornerType, 0.5f);			// anchor
            }

            mSpriteRenderer.pushLine(I->anchor, I->anchor + R->tangentVector, 0, 0.3f);
            mSpriteRenderer.pushMarker(I->anchor + R->tangentVector, 0, 0.5);			// center tangent
        }






        // now show what we selected ----------------------------------------------------------------------------------
        if (mRoadNetwork.intersectionSelectedRoad) {
            mSpriteRenderer.pushMarker(splineTest.returnPos, 4, 1.0f);
        }
        else {
            if (splineTest.bCenter) {
                mSpriteRenderer.pushMarker(I->anchor, 4, 1.0);
            }
            else if (splineTest.cornerNum >= 0) {
                intersectionRoadLink* R = &I->roadLinks[splineTest.cornerNum];
                switch (splineTest.cornerFlag) {
                case -1:
                    mSpriteRenderer.pushMarker(R->corner_A + R->cornerTangent_A, 4, 1.0);
                    break;
                case 0:
                    mSpriteRenderer.pushMarker(R->corner_A, 4, 1.0);
                    break;
                case 1:
                    mSpriteRenderer.pushMarker(R->corner_B + R->cornerTangent_B, 4, 1.0);
                    break;
                case 2:
                    mSpriteRenderer.pushMarker(I->anchor + R->tangentVector, 4, 1.0);
                    break;
                }
            }
        }

        mSpriteRenderer.loadDynamic();
    }
    */
}



bool terrainManager::onKeyEvent(const KeyboardEvent& keyEvent)
{
    bool keyPressed = (keyEvent.type == KeyboardEvent::Type::KeyPressed);

    splineTest.bCtrl = keyEvent.hasModifier(Input::Modifier::Ctrl);
    splineTest.bShift = keyEvent.hasModifier(Input::Modifier::Shift);
    splineTest.bAlt = keyEvent.hasModifier(Input::Modifier::Alt);

    if (keyEvent.type == KeyboardEvent::Type::KeyPressed)
    {
        if (keyEvent.hasModifier(Input::Modifier::Ctrl))
        {
            if (keyEvent.key == Input::Key::D)          // debug
            {
                debug = !debug;
            }
        }

        switch (terrainMode)
        {
        case 3:
            

            // selection 
            if (mRoadNetwork.currentRoad && splineTest.bCtrl)
            {
                switch (keyEvent.key)
                {
                case Input::Key::A:	// select all
                    mRoadNetwork.currentRoad->selectAll();
                    mRoadNetwork.selectionType = 2;
                    return true;
                    break;
                case Input::Key::D:	// select all
                    mRoadNetwork.currentRoad->clearSelection();
                    mRoadNetwork.selectionType = 0;
                    return true;
                    break;
                }
            }

            if (keyPressed)
            {
                if (keyEvent.hasModifier(Input::Modifier::Ctrl))		// CTRL + key
                {
                    switch (keyEvent.key)
                    {
                    case Input::Key::C:		// copy
                        if (splineTest.bVertex) {
                            mRoadNetwork.copyVertex(splineTest.index);
                        }
                        break;
                    case Input::Key::V:		// paste
                        if (keyEvent.hasModifier(Input::Modifier::Shift))		// CTRL + SHIFT  + key
                        {
                            if (splineTest.bVertex) {
                                mRoadNetwork.pasteVertexMaterial(splineTest.index);
                            }
                        }
                        else {
                            if (splineTest.bVertex) {
                                mRoadNetwork.pasteVertexGeometry(splineTest.index);
                            }
                        }
                        break;
                    }
                }
                else
                {
                    switch (keyEvent.key)
                    {
                    case Input::Key::Del:
                        if (mRoadNetwork.currentIntersection) mRoadNetwork.deleteCurrentIntersection();
                        if (mRoadNetwork.currentRoad) mRoadNetwork.deleteCurrentRoad();
                        break;
                    }
                }

                mRoadNetwork.updateAllRoads();
                updateDynamicRoad(true);
            }

            switch (keyEvent.key)
            {
            case Input::Key::Escape:	// deselect all
                if (keyPressed) {
                    {
                        if (mRoadNetwork.popupVisible) {
                            ImGui::CloseCurrentPopup();
                            // stuff that, dpoesnt work no key or mouse events, cunsumed by popup
                        }
                        else {
                            mRoadNetwork.currentRoad = nullptr;
                            mRoadNetwork.currentIntersection = nullptr;
                            mRoadNetwork.intersectionSelectedRoad = nullptr;
                            //updateStaticRoad();
                            updateDynamicRoad(true);
                        }
                    }
                }
                return true;
                break;
            case Input::Key::S:
                if (keyPressed && keyEvent.hasModifier(Input::Modifier::Ctrl)) {
                    mRoadNetwork.quickSave();
                }
                break;
            case Input::Key::X:
                // road new spline
                if (keyPressed && keyEvent.hasModifier(Input::Modifier::Ctrl)) {
                    if (mRoadNetwork.currentIntersection) {
                        mRoadNetwork.currentRoad = nullptr;
                        mRoadNetwork.currentIntersection_findRoads();
                        //updateStaticRoad();
                        updateDynamicRoad(true);
                    }
                }
                return true;
                break;
            case Input::Key::G:
                // road new spline
                if (keyPressed) {
                    mRoadNetwork.newRoadSpline();
                    //updateStaticRoad();
                }
                return true;
                break;


            case Input::Key::F:
                // new node
                if (keyPressed) {
                    mRoadNetwork.newIntersection();
                    mRoadNetwork.currentIntersection->updatePosition(split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    mRoadNetwork.currentIntersection_findRoads();
                    updateDynamicRoad(true);
                }
                return true;
                break;
            case Input::Key::H:
                if (keyPressed && splineTest.bVertex) {
                    mRoadNetwork.breakCurrentRoad(splineTest.index);
                    //updateStaticRoad();
                    updateDynamicRoad(true);
                }
                return true;
                break;
            case Input::Key::J:
                if (keyPressed) {
                    mRoadNetwork.deleteCurrentRoad();
                    //updateStaticRoad();
                    updateDynamicRoad(true);
                }
                return true;
                break;
            case Input::Key::K:
                if (keyPressed) {
                    mRoadNetwork.deleteCurrentIntersection();
                    //updateStaticRoad();
                    updateDynamicRoad(true);
                }
                return true;
                break;
            case Input::Key::B:
                if (keyPressed) {
                    if (splineTest.bVertex && keyEvent.hasModifier(Input::Modifier::Ctrl)) {
                        mRoadNetwork.currentRoad->points[splineTest.index].isBridge = !mRoadNetwork.currentRoad->points[splineTest.index].isBridge;
                    }
                    else
                    {
                        bSplineAsTerrafector = !bSplineAsTerrafector;
                        reset(true);
                    }
                }
                return true;
                break;
            case Input::Key::N:
                if (keyPressed) {
                    showRoadOverlay = !showRoadOverlay;
                }
                return true;
                break;
            case Input::Key::M:
                if (keyPressed) {
                    showRoadSpline = !showRoadSpline;
                }
                return true;
                break;
            case Input::Key::Up:
                if (splineTest.bVertex) {
                    if (keyEvent.hasModifier(Input::Modifier::Ctrl)) {
                        mRoadNetwork.currentRoad->points[splineTest.index].addTangent(1.0f);
                        mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    }
                    else {
                        mRoadNetwork.currentRoad->points[splineTest.index].addHeight(0.1f);
                        mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    }
                }
                break;

            case Input::Key::Down:
                if (splineTest.bVertex) {
                    if (keyEvent.hasModifier(Input::Modifier::Ctrl)) {
                        mRoadNetwork.currentRoad->points[splineTest.index].addTangent(-1.0f);
                        mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    }
                    else {
                        mRoadNetwork.currentRoad->points[splineTest.index].addHeight(-0.1f);
                        mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    }
                }
                break;

            case Input::Key::Left:
                if (splineTest.bVertex) {
                    mRoadNetwork.currentRoad->points[splineTest.index].camber += 0.01f;
                    mRoadNetwork.currentRoad->points[splineTest.index].constraint = e_constraint::camber;
                    mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                }
                break;
            case Input::Key::Right:
                if (splineTest.bVertex) {
                    mRoadNetwork.currentRoad->points[splineTest.index].camber -= 0.01f;
                    mRoadNetwork.currentRoad->points[splineTest.index].constraint = e_constraint::camber;
                    mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                }
                break;
            }

            break;
        default:
            break;
        }
    }
    return false;
}


bool terrainManager::onMouseEvent(const MouseEvent& mouseEvent, glm::vec2 _screenSize, Camera::SharedPtr _camera)
{
    glm::vec2 pos = mouseEvent.pos;
    pos.y = 1.0 - pos.y;
    glm::vec3 N = glm::unProject(glm::vec3(pos * _screenSize, 0.0f), toGLM(_camera->getViewMatrix()), toGLM(_camera->getProjMatrix()), glm::vec4(0, 0, _screenSize));
    glm::vec3 F = glm::unProject(glm::vec3(pos * _screenSize, 1.0f), toGLM(_camera->getViewMatrix()), toGLM(_camera->getProjMatrix()), glm::vec4(0, 0, _screenSize));
    mouseDirection = glm::normalize(F - N);
    screenSize = _screenSize;
    mousePosition = _camera->getPosition();
    mouseCoord = mouseEvent.pos * _screenSize;


    switch (mouseEvent.type)
    {
    case MouseEvent::Type::Move:
    {
        //if (bRightButton)  // PAN
        if (ImGui::IsMouseDown(1))
        {
            glm::vec3 newPos = mouse.pan - mouseDirection * (_camera->getPosition().y - mouse.pan.y) / fabs(mouseDirection.y);
            glm::vec3 deltaPos = newPos - _camera->getPosition();

            glm::vec3 newTarget = _camera->getTarget() + deltaPos;
            _camera->setPosition(newPos);
            _camera->setTarget(newTarget);
            hasChanged = true;
        }

        // orbit
        if (ImGui::IsMouseDown(2))
        {
            glm::vec3 D = _camera->getTarget() - _camera->getPosition();
            glm::vec3 U = glm::vec3(0, 1, 0);
            glm::vec3 R = glm::normalize(glm::cross(U, D));
            glm::vec2 diff = pos - mousePositionOld;
            glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), diff.x * 10.0f, glm::vec3(0, 1, 0));

            glm::vec3 Dnorm = glm::normalize(D);
            if ((Dnorm.y < -0.99f) && (diff.y < 0)) diff.y = 0;
            if ((Dnorm.y > 0.0f) && (diff.y > 0)) diff.y = 0;

            glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), diff.y * 10.0f, R);
            mouse.toGround = glm::vec4(mouseDirection, 0) * mouse.orbitRadius * yaw * pitch;
            glm::vec4 newDir = glm::vec4(D, 0) * yaw * pitch;

            _camera->setPosition(mouse.orbit - mouse.toGround);
            _camera->setTarget(mouse.orbit - mouse.toGround + glm::vec3(newDir));
            hasChanged = true;
        }
        mousePositionOld = pos;
    }
    break;
    case MouseEvent::Type::Wheel:
    {
        if (mouse.hit)
        {
            float scale = 1.0 - mouseEvent.wheelDelta.y / 6.0f;
            mouse.toGround *= scale;
            glm::vec3 newPos = mouse.terrain - mouse.toGround;
            glm::vec3 deltaPos = newPos - _camera->getPosition();
            glm::vec3 newTarget = _camera->getTarget() + deltaPos;

            _camera->setPosition(newPos);
            _camera->setTarget(newTarget);
            hasChanged = true;
        }
    }
    break;
    case MouseEvent::Type::ButtonDown:
    {
        if (mouseEvent.button == Input::MouseButton::Middle)
        {
            mouse.orbitRadius = glm::length(mouse.toGround);
        }
    }
    break;
    case MouseEvent::Type::ButtonUp:
    {
    }
    break;
    }


    // rebuild from new camera
    {
        glm::vec3 N = glm::unProject(glm::vec3(pos * _screenSize, 0.0f), toGLM(_camera->getViewMatrix()), toGLM(_camera->getProjMatrix()), glm::vec4(0, 0, _screenSize));
        glm::vec3 F = glm::unProject(glm::vec3(pos * _screenSize, 1.0f), toGLM(_camera->getViewMatrix()), toGLM(_camera->getProjMatrix()), glm::vec4(0, 0, _screenSize));
        mouseDirection = glm::normalize(F - N);
        screenSize = _screenSize;
        mousePosition = _camera->getPosition();
        mouseCoord = mouseEvent.pos * _screenSize;
    }



    // Now for a complete new block for mouse road interactions
    // ******************************************************************************************
    static bool bDragEvent;
    static glm::vec2 prevPos;
    glm::vec2 diff = mouseEvent.pos - prevPos;
    prevPos = mouseEvent.pos;

    switch (mouseEvent.type)
    {
    case MouseEvent::Type::Move:
    {
        mRoadNetwork.testHit(split.feedback.tum_Position);

        bDragEvent = true;
        if (bLeftButton)
        {
            if (splineTest.bVertex) {
                if (mRoadNetwork.currentRoad) {
                    if (splineTest.bCtrl)
                    {
                        mRoadNetwork.currentRoad->points[splineTest.index].B += diff.x * 10.0f;
                        mRoadNetwork.currentRoad->points[splineTest.index].B = __min(1, __max(0, mRoadNetwork.currentRoad->points[splineTest.index].B));
                    }
                    else if (splineTest.bShift)
                    {
                        mRoadNetwork.currentRoad->points[splineTest.index].tangent_Offset += diff.x * 100.0f;
                        mRoadNetwork.currentRoad->points[splineTest.index].solveMiddlePos();
                    }
                    else if (splineTest.bAlt)
                    {
                    }
                    else
                    {
                        mRoadNetwork.currentRoad->points[splineTest.index].setAnchor(split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    }

                    mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                }
                else if (mRoadNetwork.intersectionSelectedRoad) {
                    mRoadNetwork.intersectionSelectedRoad->points[splineTest.index].setAnchor(split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    mRoadNetwork.intersectionSelectedRoad->solveRoad(splineTest.index);
                    mRoadNetwork.solveIntersection();
                }
            }

            if (mRoadNetwork.currentIntersection) {
                if (splineTest.bCenter) {
                    mRoadNetwork.currentIntersection->updatePosition(split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    {
                        FALCOR_PROFILE("solveIntersection");
                        mRoadNetwork.solveIntersection();
                    }
                }

                if (splineTest.cornerNum >= 0) {
                    mRoadNetwork.currentIntersection->updateCorner(split.feedback.tum_Position, split.feedback.tum_Normal, splineTest.cornerNum, splineTest.cornerFlag);
                    mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerType = typeOfCorner::artistic;
                }
            }
            updateDynamicRoad(true);
        }
    }
    break;
    case MouseEvent::Type::Wheel:

        if (splineTest.bShift) {
            if (splineTest.bVertex && splineTest.bCtrl) {
                if (mRoadNetwork.currentRoad) {
                    mRoadNetwork.incrementLane(-1, mouseEvent.wheelDelta.y / 10.0f, mRoadNetwork.currentRoad, splineTest.bAlt);
                    mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    updateDynamicRoad(true);
                }
                else if (mRoadNetwork.intersectionSelectedRoad) {
                    mRoadNetwork.incrementLane(-1, mouseEvent.wheelDelta.y / 10.0f, mRoadNetwork.intersectionSelectedRoad, splineTest.bAlt);
                    mRoadNetwork.intersectionSelectedRoad->solveRoad(splineTest.index);
                    mRoadNetwork.solveIntersection();
                    updateDynamicRoad(true);
                }
                return true;
            }
        }
        else {
            if (splineTest.bVertex && splineTest.bCtrl) {
                if (mRoadNetwork.currentRoad) {
                    mRoadNetwork.incrementLane(splineTest.index, mouseEvent.wheelDelta.y / 10.0f, mRoadNetwork.currentRoad, splineTest.bAlt);
                    mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    updateDynamicRoad(true);
                }
                else if (mRoadNetwork.intersectionSelectedRoad) {
                    mRoadNetwork.incrementLane(splineTest.index, mouseEvent.wheelDelta.y / 10.0f, mRoadNetwork.intersectionSelectedRoad, splineTest.bAlt);
                    mRoadNetwork.intersectionSelectedRoad->solveRoad(splineTest.index);
                    mRoadNetwork.solveIntersection();
                    updateDynamicRoad(true);
                }
                return true;
            }


            if (splineTest.bCtrl && mRoadNetwork.currentIntersection) {
                if (mRoadNetwork.currentIntersection && splineTest.cornerNum >= 0) {
                    if (splineTest.bAlt) {
                        mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerRadius += mouseEvent.wheelDelta.y / 20.0f;
                        mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerType = typeOfCorner::radius;
                    }
                    else {
                        mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerRadius += mouseEvent.wheelDelta.y / 5.0f;
                        mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerType = typeOfCorner::radius;
                    }
                    if (mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerRadius < 0.2f) mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerRadius = 0.2f;
                    mRoadNetwork.solveIntersection();
                    updateDynamicRoad(true);
                    return true;
                }
            }
        }
        break;

    case MouseEvent::Type::ButtonDown:
    {
        if (mouseEvent.button == Input::MouseButton::Left)
        {
            bLeftButton = true;

            // SUB selection
            if (splineTest.bCtrl) {
                if (splineTest.bVertex) {
                    mRoadNetwork.selectionType = 1;
                    int mid = (mRoadNetwork.selectFrom + mRoadNetwork.selectTo) / 2;
                    if (splineTest.index < mRoadNetwork.selectFrom) {
                        mRoadNetwork.selectFrom = splineTest.index;
                    }
                    else if (splineTest.index > mRoadNetwork.selectTo) {
                        mRoadNetwork.selectTo = splineTest.index;
                    }
                    else {
                        if (splineTest.index < mid) {
                            mRoadNetwork.selectFrom = splineTest.index;
                        }
                        else {
                            mRoadNetwork.selectTo = splineTest.index;
                        }
                    }

                }
            }


            // Selection but now add all the possible subselections
            if (splineTest.bCtrl) {								// selection


                if (mRoadNetwork.AI_path_mode) {
                    mRoadNetwork.addRoad();
                }
                else
                {
                    mRoadNetwork.doSelect(split.feedback.tum_Position);

                    if (mRoadNetwork.bHIT) {
                        mRoadNetwork.setEditRight(mRoadNetwork.hitRoadRight);
                        mRoadNetwork.setEditLane(mRoadNetwork.hitRoadLane);
                        //updateStaticRoad();

                    }
                    updateDynamicRoad(true);

                }
            }
            else if (mRoadNetwork.currentRoad) {
                if (!splineTest.bVertex && !splineTest.bSegment) {
                    mRoadNetwork.currentRoad->pushPoint(split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    updateDynamicRoad(true);
                }

                if (splineTest.bSegment) {
                    mRoadNetwork.currentRoad->insertPoint(splineTest.index, split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    updateDynamicRoad(true);
                }
            }

        }
        if (mouseEvent.button == Input::MouseButton::Middle)
        {
            bMiddelButton = true;
        }
        if (mouseEvent.button == Input::MouseButton::Right)
        {
            bRightButton = true;

            if (splineTest.bCtrl)
            {
                if (mRoadNetwork.currentRoad && splineTest.bVertex && mRoadNetwork.currentRoad->points.size() > splineTest.index) {
                    mRoadNetwork.currentRoad->deletePoint(splineTest.index);
                    updateDynamicRoad(true);
                }

                if (mRoadNetwork.currentIntersection) {
                    if (splineTest.cornerNum >= 0) {
                        mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerType = typeOfCorner::automatic;
                        mRoadNetwork.solveIntersection();
                        updateDynamicRoad(true);
                    }
                    if (splineTest.bCenter) {
                        mRoadNetwork.currentIntersection->customNormal = false;
                    }
                }
            }
        }
    }
    break;


    case MouseEvent::Type::ButtonUp:
    {
        if (mouseEvent.button == Input::MouseButton::Left)
        {
            bLeftButton = false;
            bDragEvent = false;
        }
        if (mouseEvent.button == Input::MouseButton::Middle)
        {
            bMiddelButton = false;
            bDragEvent = false;
        }
        if (mouseEvent.button == Input::MouseButton::Right)
        {
            //bShowRoadPopup = true;
            bDragEvent = false;
            //popupPos = mouseEvent.pos * m_ScreenSize;
            bRightButton = false;
        }
    }
    break;
    }
    return false;
}


void terrainManager::onHotReload(HotReloadFlags reloaded)
{}


