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

using namespace Falcor;


class videoToScreen
{
    glm::vec3 toScreen(glm::vec3 dot);

    struct _block
    {
        glm::vec3 screenPos[4];
        glm::vec3 videoPos[4];
        glm::vec3 videoEdge[4];
    };

    _block grid[8][4];

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive_float3(grid);
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
    float pgGain = 0;
    float pgGamma = 0;
    Texture::SharedPtr	        pointGreyBuffer = nullptr;
    Texture::SharedPtr	        pointGreyDiffBuffer = nullptr;
    int dot_min = 5;
    int dot_max = 20; 
    int threshold = 20;
    int m_PG_dot_position = 1;

    GraphicsState::Viewport     viewport3d;
    float2                      screenSize;
    float2                      screenMouseScale;
    float2                      screenMouseOffset;
    Fbo::SharedPtr		        hdrFbo;
    Texture::SharedPtr	        hdrHalfCopy;
    GraphicsState::SharedPtr    graphicsState;
    Falcor::Camera::SharedPtr	camera;

    bool    showPointGrey = false;
    bool    showCalibration = false;

    int modeCalibrate = 0;
};
