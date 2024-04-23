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
#pragma once
#include "Falcor.h"
#include "terrain.h"
#include "RenderGraph/RenderGraph.h"

using namespace Falcor;


struct  fogCloudCommonParams {

    float3 sun_direction = float3(-0.96592582628906f, -0.2588190451f, 0.f);
    float pad_fog1;

    float cloudBase = 5000.f;
    float cloudThickness = 100.f; 
    float2 paddcloudB;
};

struct   fogAtmosphericParams {
    float3 eye_direction;
    float uv_offset;

    float3 eye_position;
    float uv_scale;

    float3 dx;
    float uv_pixSize;

    float3 dy;
    uint numSlices;

    float sliceZero;
    float sliceMultiplier; // distance to the next slice (multiply with current slice)
    float sliceStep;	   // size of this slice (multiply with current slice)
    float slicePADD2;

    float timer;
    int numFogLights;
    float2 paddTime;

    // possible temporary near density
    float2 fogAltitude; //(base, noise)	??? Not sure if base will be sued in the end, get from Fog texture
    float2 fogDensity;	//(base, noise)
    float2 fogUVoffset; // animate to simulate wind
    float fogMip0Size;	// Size of a single pixel on MIP 0, since compute shaders have to explicitely compute mip level
    float numFogVolumes;

    // Block for the new atmosphere
    // ############################################################################################################################
    float3 sunColourBeforeOzone{ 1.0f, 1.0f, 1.0f };
    float haze_Turbidity = 1.5f;

    float3 haze_Colour{ 0.95, 0.95, 0.95 };
    float haze_AltitudeKm = 1.2f;

    float3 fog_Colour{ 0.95, 0.95, 0.95 };
    float haze_BaseAltitudeKm = 0;

    float fog_AltitudeKm = 0.02f;
    float fog_BaseAltitudeKm = 0.85f;
    float fog_Turbidity = 8.0f;
    float globalExposure = 1.0f / 20000.0f;

    float3 rain_Colour{ 0.1, 0.1, 0.1 };
    float rainFade = 1.0f;

    float ozone_Density = 0.7f;
    float3 ozone_Colour{ 0.65, 1.6, 0.085 };

    float4 refraction{ 0.022, 0.027, 0.03, 0 };
    // ############################################################################################################################

    // info for parabolic projection
    float parabolicProjection;
    float parabolicMapHalfSize;
    float parabolicUpDown;
    float parabolicPADD;

    // Temporary block - debug sliders for atmosphere
    // ############################################################################################################################
    float tmp_IBL_scale;
    float tmp_B;
    float tmp_C;
    float tmp_D;
    // ############################################################################################################################
};


struct FogVolumeParameters {
    int m_x;
    int m_y;
    int m_z;
    Falcor::ResourceFormat format;
    bool m_bRGBOut;
    bool bParabolic;
    float _near = 50.f;
    float _far = 20000.f;
};


class FogVolume {

public:
    void onLoad(FogVolumeParameters params);
    void update();
    void setCamera(Camera::SharedPtr _camera);
    //void setLights(const std::vector<LightRenderData>& L);

public:
    FogVolumeParameters m_params;
    float m_logEnd;	  // (k-1 / k) / log(far) - for exponential slices
    float m_oneOverK; // 1.5 / k    - the 0.5 makes up for half pixel offsets

    float m_SliceStep; // Distance between two slices - multiplier
    float m_SliceZero; // m_start / m_StepMultiplier   - the first slice is closer than the near plane

    Texture::SharedPtr inscatter;
    Texture::SharedPtr inscatter_cloudbase;
    Texture::SharedPtr inscatter_sky;

    Texture::SharedPtr outscatter;
    Texture::SharedPtr outscatter_cloudbase;
    Texture::SharedPtr outscatter_sky;

    // parameters for compute shader to generate fog
    fogAtmosphericParams compute_Params;
    //VolumeFogLight compute_Lights[RENDERER_MAX_FOG_LIGHTS];
};

class Earthworks_4 : public IRenderer
{
public:
    void onLoad(RenderContext* _renderContext) override;
    void onFrameRender(RenderContext* _renderContext, const Fbo::SharedPtr& pTargetFbo) override;
    void onShutdown() override;
    void onResizeSwapChain(uint32_t _width, uint32_t _height) override;
    bool onKeyEvent(const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(const MouseEvent& mouseEvent) override;
    void onGuiRender(Gui* _gui) override;
    void onGuiMenubar(Gui* _gui);

private:
    void guiStyle();

    GraphicsState::Viewport     viewport3d;
    float2                      screenSize;
    float2                      screenMouseScale;
    float2                      screenMouseOffset;
    Fbo::SharedPtr		        hdrFbo;
    Fbo::SharedPtr		        tonemappedFbo;
    Texture::SharedPtr	        hdrHalfCopy;

    GraphicsState::SharedPtr    graphicsState;
    Camera::SharedPtr	        camera;
    pixelShader		            tonemapper;
    terrainManager              terrain;
    bool showAbout = false;
    Texture::SharedPtr aboutTex;
    int slowTimer = 0;      // this is used to alow donw framerate
    

    struct {
        computeShader		compute_sunSlice;
        Texture::SharedPtr  sunlightTexture = nullptr;
        Texture::SharedPtr  phaseFunction = nullptr;
        fogCloudCommonParams common;
        fogAtmosphericParams params;

        FogVolume mainNear;
        FogVolume mainFar;
        FogVolume parabolicFar;
        computeShader		compute_Atmosphere;

    } atmosphere;

    struct {
        bool    vsync = true;
        bool    minimal = true;
    } refresh;


    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(refresh.vsync));
        _archive(CEREAL_NVP(refresh.minimal));
    }
};
CEREAL_CLASS_VERSION(Earthworks_4, 100);

