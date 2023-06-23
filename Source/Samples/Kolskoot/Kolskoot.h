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
#include "PointGrey_Camera.h"

#include "cereal/archives/binary.hpp"
#include "cereal/archives/xml.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/cereal.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/list.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"
#include <fstream>
#define archive_float2(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y));}
#define archive_float3(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z));}
#define archive_float4(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z)); archive(CEREAL_NVP(v.w));}

using namespace Falcor;


struct _block
{
    std::array<glm::vec3, 4>  screenPos;
    std::array<glm::vec3, 4>  videoPos;
    std::array<glm::vec3, 4>  videoEdge;


    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive_float3(screenPos[0]);
        archive_float3(screenPos[1]);
        archive_float3(screenPos[2]);
        archive_float3(screenPos[3]);

        archive_float3(videoPos[0]);
        archive_float3(videoPos[1]);
        archive_float3(videoPos[2]);
        archive_float3(videoPos[3]);

        archive_float3(videoEdge[0]);
        archive_float3(videoEdge[1]);
        archive_float3(videoEdge[2]);
        archive_float3(videoEdge[3]);

        //_archive(CEREAL_NVP(screenPos[0]));
        //_archive(CEREAL_NVP(videoPos));
        //_archive(CEREAL_NVP(videoEdge));
    }
};
CEREAL_CLASS_VERSION(_block, 100);


class videoToScreen
{
public:
    glm::vec3 toScreen(glm::vec3 dot);
    void build(std::array<glm::vec4, 45>  dots, float2 screenSize);


    

    _block grid[4][8];

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(grid));
    }
};
CEREAL_CLASS_VERSION(videoToScreen, 100);



class Kolskoot : public IRenderer
{
public:
    void onLoad(RenderContext* pRenderContext) override;
    void onShutdown();
    void onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo) override;
    void onResizeSwapChain(uint32_t _width, uint32_t _height) override;
    bool onKeyEvent(const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(const MouseEvent& mouseEvent) override;
    void onGuiRender(Gui* pGui) override;
    void onGuiMenubar(Gui* _gui);
    void guiStyle();

private:
    bool mUseTriLinearFiltering = true;
    Sampler::SharedPtr mpPointSampler = nullptr;
    Sampler::SharedPtr mpLinearSampler = nullptr;

    GraphicsProgram::SharedPtr mpProgram = nullptr;
    GraphicsVars::SharedPtr mpProgramVars = nullptr;
    GraphicsState::SharedPtr mpGraphicsState = nullptr;

    bool mDrawWireframe = false;
    bool mUseOriginalTangents = false;
    bool mDontMergeMaterials = false;
    Scene::CameraControllerType mCameraType = Scene::CameraControllerType::FirstPerson;

    Scene::SharedPtr mpScene;

    RasterizerState::SharedPtr mpWireframeRS = nullptr;
    RasterizerState::CullMode mCullMode = RasterizerState::CullMode::Back;

    DepthStencilState::SharedPtr mpNoDepthDS = nullptr;
    DepthStencilState::SharedPtr mpDepthTestDS = nullptr;

    std::string mModelString;

    //PointGrey
    PointGrey_Camera*   pointGreyCamera;
    int calibrationCounter;
    float pgGain = 0;
    float pgGamma = 0;
    Texture::SharedPtr	        pointGreyBuffer = nullptr;
    Texture::SharedPtr	        pointGreyDiffBuffer = nullptr;
    int dot_min = 5;
    int dot_max = 40; 
    int threshold = 20;
    int m_PG_dot_position = 1;
    std::array<glm::vec4, 45> calibrationDots;

    GraphicsState::Viewport     viewport3d;
    float2                      screenSize;
    float2                      screenMouseScale;
    float2                      screenMouseOffset;
    Fbo::SharedPtr		        hdrFbo;
    Texture::SharedPtr	        hdrHalfCopy;
    GraphicsState::SharedPtr    graphicsState;
    Falcor::Camera::SharedPtr	camera;

    videoToScreen   screenMap;

    bool    showPointGrey = false;
    bool    showCalibration = false;

    int modeCalibrate = 0;
};
