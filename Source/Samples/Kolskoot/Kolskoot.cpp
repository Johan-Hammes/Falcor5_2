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
#include "Kolskoot.h"
#include "Utils/UI/TextRenderer.h"
#include "imgui.h"
#include <imgui_internal.h>
#include "Core/Platform/MonitorInfo.h"



void Kolskoot::onGuiMenubar(Gui* _gui)
{
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::Text("Kolskoot          ");


        if (ImGui::BeginMenu("Settings"))
        {
            ImGui::Text("camera");

            ImGui::EndMenu();
        }


        ImGui::SetCursorPos(ImVec2(screenSize.x - 15, 0));
        if (ImGui::Selectable("X")) { gpFramework->getWindow()->shutdown(); }
    }
    ImGui::EndMainMenuBar();
}



void Kolskoot::onGuiRender(Gui* _gui)
{
    ImGui::PushFont(_gui->getFont("roboto_20"));
    {
        onGuiMenubar(_gui);


        Gui::Window pointgreyPanel(_gui, "PointGrey", {900, 900}, {100, 100});
        {
            if (pointGreyCamera->isConnected())
            {
                ImGui::Text("serial  %d", pointGreyCamera->getSerialNumber());
                ImGui::Text("%d x %d", (int)pointGreyCamera->bufferSize.x, (int)pointGreyCamera->bufferSize.y);

                ImGui::NewLine();
                
                if (ImGui::SliderFloat("Gain", &pgGain, 0, 12)) {
                    pointGreyCamera->setGain(pgGain);
                }
                if (ImGui::SliderFloat("Gamma", &pgGamma, 0, 1)) {
                    pointGreyCamera->setGamma(pgGamma);
                }

                ImVec2 C = ImGui::GetCursorPos();
                if (pointGreyBuffer)
                {
                    if (pointgreyPanel.imageButton("testImage", pointGreyBuffer, pointGreyCamera->bufferSize))
                    {

                    }
                }
                else
                {
                    pointGreyBuffer = Texture::create2D((int)pointGreyCamera->bufferSize.x, (int)pointGreyCamera->bufferSize.y, ResourceFormat::R8Unorm, 1, 1, pointGreyCamera->bufferData);
                }


                uint ndots = pointGreyCamera->dotQueue.size();
                ImGui::Text("dots  %d", ndots);
                for (uint i = 0; i < ndots; i++)
                {
                    glm::vec4 dot = pointGreyCamera->dotQueue.front();
                    pointGreyCamera->dotQueue.pop();
                    ImGui::SetCursorPos(ImVec2(C.x + dot.x, C.y + dot.y));
                    ImGui::Text("x");
                }

            }

        }
        pointgreyPanel.release();

    }
    ImGui::PopFont();
}




void Kolskoot::onLoad(RenderContext* pRenderContext)
{
    mpGraphicsState = GraphicsState::create();

    // Create rasterizer state
    RasterizerState::Desc wireframeDesc;
    wireframeDesc.setFillMode(RasterizerState::FillMode::Wireframe);
    wireframeDesc.setCullMode(RasterizerState::CullMode::None);
    mpWireframeRS = RasterizerState::create(wireframeDesc);

    // Depth test
    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthEnabled(false);
    mpNoDepthDS = DepthStencilState::create(dsDesc);
    dsDesc.setDepthFunc(ComparisonFunc::Less).setDepthEnabled(true);
    mpDepthTestDS = DepthStencilState::create(dsDesc);

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPointSampler = Sampler::create(samplerDesc);
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    mpLinearSampler = Sampler::create(samplerDesc);

    pointGreyCamera = PointGrey_Camera::GetSingleton();
    gpDevice->toggleVSync(true);


    camera = Falcor::Camera::create();
    camera->setDepthRange(0.5f, 40000.0f);
    camera->setAspectRatio(1920.0f / 1080.0f);
    camera->setFocalLength(15.0f);
    camera->setPosition(float3(0, 900, 0));
    camera->setTarget(float3(0, 900, 100));

    graphicsState = GraphicsState::create();
}



void Kolskoot::onShutdown()
{
    PointGrey_Camera::FreeSingleton();
}



void Kolskoot::onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{

    if (pointGreyBuffer)
    {
        pRenderContext->updateTextureData(pointGreyBuffer.get(), pointGreyCamera->bufferData);
    }

    const float4 clearColor(0.02f, 0.02f, 0.02f, 1);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    mpGraphicsState->setFbo(pTargetFbo);

    if (mpScene)
    {
        mpScene->update(pRenderContext, gpFramework->getGlobalClock().getTime());

        // Set render state
        if (mDrawWireframe)
        {
            mpGraphicsState->setDepthStencilState(mpNoDepthDS);
            mpProgramVars["PerFrameCB"]["gConstColor"] = true;

            mpScene->rasterize(pRenderContext, mpGraphicsState.get(), mpProgramVars.get(), mpWireframeRS, mpWireframeRS);
        }
        else
        {
            mpGraphicsState->setDepthStencilState(mpDepthTestDS);
            mpProgramVars["PerFrameCB"]["gConstColor"] = false;

            mpScene->rasterize(pRenderContext, mpGraphicsState.get(), mpProgramVars.get(), mCullMode);
        }
    }

    TextRenderer::render(pRenderContext, mModelString, pTargetFbo, float2(10, 30));
}



bool Kolskoot::onKeyEvent(const KeyboardEvent& keyEvent)
{
    if (mpScene && mpScene->onKeyEvent(keyEvent)) return true;

    if ((keyEvent.type == KeyboardEvent::Type::KeyPressed) && (keyEvent.key == Input::Key::R))
    {
        return true;
    }
    return false;
}



bool Kolskoot::onMouseEvent(const MouseEvent& mouseEvent)
{
    return mpScene ? mpScene->onMouseEvent(mouseEvent) : false;
}



void Kolskoot::onResizeSwapChain(uint32_t _width, uint32_t _height)
{
    auto monitorDescs = MonitorInfo::getMonitorDescs();
    screenSize = monitorDescs[0].resolution;
    viewport3d.originX = 0;
    viewport3d.originY = 0;
    viewport3d.minDepth = 0;
    viewport3d.maxDepth = 1;
    viewport3d.width = screenSize.x;
    viewport3d.height = screenSize.y;

    camera->setAspectRatio(screenSize.x / screenSize.y);

    screenMouseScale.x = _width / screenSize.x;
    screenMouseScale.y = _height / screenSize.y;
    screenMouseOffset.x = 0;
    screenMouseOffset.y = 0;

    Fbo::Desc desc;
    desc.setDepthStencilTarget(ResourceFormat::D24UnormS8);
    desc.setColorTarget(0u, ResourceFormat::R11G11B10Float);
    hdrFbo = Fbo::create2D(_width, _height, desc);
    graphicsState->setFbo(hdrFbo);

    hdrHalfCopy = Texture::create2D(_width / 2, _height / 2, ResourceFormat::R11G11B10Float, 1, 7, nullptr, Falcor::Resource::BindFlags::RenderTarget);
}



int main(int argc, char** argv)
{
    Kolskoot::UniquePtr pRenderer = std::make_unique<Kolskoot>();

    SampleConfig config;
    config.windowDesc.title = "Kolskoot";
    config.windowDesc.resizableWindow = true;
    config.windowDesc.mode = Window::WindowMode::Fullscreen;
    config.windowDesc.width = 1260;
    config.windowDesc.height = 1140;
    config.windowDesc.monitor = 0;

    Sample::run(config, pRenderer);
    return 0;
}
