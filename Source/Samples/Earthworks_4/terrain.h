/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
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
#pragma once
#include "Falcor.h"
#include "computeShader.h"
#include "pixelShader.h"
#include"groundcover_defines.hlsli"
#include"terrainDefines.hlsli"
#include"gpuLights_defines.hlsli"
#include"terrafector.h"

#include "../../external/cereal-master/include/cereal/cereal.hpp"
#include "../../external/cereal-master/include/cereal/archives/binary.hpp"
#include "../../external/cereal-master/include/cereal/archives/json.hpp"
#include "../../external/cereal-master/include/cereal/archives/xml.hpp"
#include <fstream>
#define archive_float2(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y));}
#define archive_float3(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z));}
#define archive_float4(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z)); archive(CEREAL_NVP(v.w));}


using namespace Falcor;


struct _lastFile
{
    // these are for quick load
    std::string terrain = "X:/resources/terrains/eifel/eifel.terrain";
    std::string road = "X:/resources/terrains/eifel/roads/day6.roadnetwork";
    std::string roadMaterial = "X:/resources/terrafectors_and_road_materials/roads/sidewalk_Asphalt.roadMaterial";
    std::string terrafectorMaterial = "";
    std::string texture = "";
    std::string fbx = "";


    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(terrain));
        _archive(CEREAL_NVP(road));
        _archive(CEREAL_NVP(roadMaterial));
        _archive(CEREAL_NVP(terrafectorMaterial));
        _archive(CEREAL_NVP(texture));
        _archive(CEREAL_NVP(fbx));
    }
};
CEREAL_CLASS_VERSION(_lastFile, 100);


struct _terrainSettings
{
    // these are for quick load
    std::string name = "eifel";
    std::string projection = "\" + proj = tmerc + lat_0 = 50.39 + lon_0 = 6.91 + k_0 = 1 + x_0 = 0 + y_0 = 0 + ellps = GRS80 + units = m\"";
    float size = 40000.f;


    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(name));
        _archive(CEREAL_NVP(projection));
        _archive(CEREAL_NVP(size));
    }

    void renderGui(Gui* _gui);
};
CEREAL_CLASS_VERSION(_terrainSettings, 100);



class quadtree_tile
{
public:
    void init(uint _index);
    void set(uint _lod, uint _x, uint _y, float _size, float4 _origin, quadtree_tile* _parent);

private:
public:
    uint index;
    quadtree_tile* parent;
    quadtree_tile* child[4];

    float4 boundingSphere;		// position has to be power of two to allow us to store large world offsets using float rather than double
    float4 origin;
    float size;
    uint lod;
    uint y;
    uint x;

    bool	forSplit;
    bool	forRemove;          // the children
    bool	main_ShouldSplit;
    bool	env_ShouldSplit;

    uint numQuads;
    uint numPlants;
    uint elevationHash;
};



class terrainManager
{
public:
    terrainManager();
    virtual ~terrainManager();

    void onLoad(RenderContext* _renderContext);
    void onShutdown();
    void onGuiRender(Gui* pGui);
    void onGuiMenubar(Gui* pGui);
    void onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo);
    bool onKeyEvent(const KeyboardEvent& keyEvent);
    bool onMouseEvent(const MouseEvent& mouseEvent);
    void onHotReload(HotReloadFlags reloaded);

    void update(RenderContext* pRenderContext);

private:
    uint                        numTiles = 1024;
    std::vector<quadtree_tile>	m_tiles;
    std::list<quadtree_tile*>	m_free;
    std::list<quadtree_tile*>	m_used;

    _lastFile lastfile;
    _terrainSettings settings;

    bool requestPopupSettings = false;

    computeShader compute_MouseUnderTerrain;

    struct {
        uint                bakeSize = 1024;
        Fbo::SharedPtr		tileFbo;
        Fbo::SharedPtr		bakeFbo;
        Texture::SharedPtr	noise_u16;

        computeShader		compute_tileClear;
        computeShader		compute_tileSplitMerge;
        computeShader		compute_tileGenerate;
        computeShader		compute_tilePassthrough;
        computeShader		compute_tileBuildLookup;
        computeShader		compute_tileBicubic;		// upsample heights with bicubic filter
        computeShader		compute_tileEcotopes;
        computeShader		compute_tileNormals;
        computeShader		compute_tileVerticis;
        computeShader		compute_tileJumpFlood;
        computeShader		compute_tileDelaunay;
        computeShader		compute_tileElevationMipmap;

        // BC6H compressor
        computeShader           compute_bc6h;
        Texture::SharedPtr      bc6h_texture;

        Buffer::SharedPtr       drawArgs_quads;
        Buffer::SharedPtr       drawArgs_plants;
        Buffer::SharedPtr       drawArgs_tiles;         // block based
        Buffer::SharedPtr       buffer_feedback;

        std::vector<gpuTile>    cpuTiles;
        Buffer::SharedPtr       buffer_tiles;
        Buffer::SharedPtr       buffer_instance_quads;
        Buffer::SharedPtr       buffer_instance_plants;

        Buffer::SharedPtr       buffer_lookup_terrain;
        Buffer::SharedPtr       buffer_lookup_quads;
        Buffer::SharedPtr       buffer_lookup_plants;

        Buffer::SharedPtr	    buffer_tileCenters;
        Buffer::SharedPtr		buffer_tileCenter_readback;
        std::vector<float4>		tileCenters;

        Buffer::SharedPtr       buffer_terrain;

        Texture::SharedPtr      debug_texture;
        Texture::SharedPtr      bicubic_upsample_texture;
        Texture::SharedPtr      normals_texture;
        Texture::SharedPtr      vertex_A_texture;
        Texture::SharedPtr      vertex_B_texture;
        Texture::SharedPtr      vertex_clear;		        // 0 for fast clear
        Texture::SharedPtr      vertex_preload;	            // a pre allocated 1/8 verts
    } split;

    Sampler::SharedPtr			sampler_Trilinear;
    Sampler::SharedPtr			sampler_Clamp;
    Sampler::SharedPtr			sampler_ClampAnisotropic;

    
};
