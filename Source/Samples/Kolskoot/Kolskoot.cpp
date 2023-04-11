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




glm::vec3 videoToScreen::toScreen(glm::vec3 dot)
{
    glm::vec3 screen = glm::vec3(0, 0, 0);

    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            glm::vec3 vA = dot - grid[y][x].videoPos[0];
            glm::vec3 vB = dot - grid[y][x].videoPos[1];
            glm::vec3 vC = dot - grid[y][x].videoPos[2];
            glm::vec3 vD = dot - grid[y][x].videoPos[3];

            float wA = glm::cross(vA, grid[y][x].videoEdge[0]).z;
            float wB = glm::cross(vB, grid[y][x].videoEdge[1]).z;
            float wC = glm::cross(vC, grid[y][x].videoEdge[2]).z;
            float wD = glm::cross(vD, grid[y][x].videoEdge[3]).z;

            if (wA >= 0 && wB >= 0 && wC >= 0 && wD >= 0)		// ons het die regte blok
            {
                float wX = wA / (wA + wC);
                float wY = wD / (wB + wD);
                screen = grid[y][x].screenPos[0] * (1.0f - wX) * (1.0f - wY) +
                    grid[y][x].screenPos[1] * (1.0f - wX) * (wY)+
                    grid[y][x].screenPos[2] * (wX) * (wY)+
                    grid[y][x].screenPos[3] * (wX) * (1.0f - wY);
                // do with lerp

            }
        }
    }
    return screen;        
}









void Kolskoot::onGuiMenubar(Gui* _gui)
{
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::Text("Kolskoot          ");


        if (ImGui::BeginMenu("Settings"))
        {
            ImGui::Checkbox("camera", &showPointGrey);


            ImGui::EndMenu();
        }


        ImGui::SetCursorPos(ImVec2(screenSize.x - 15, 0));
        if (ImGui::Selectable("X")) { gpFramework->getWindow()->shutdown(); }
    }
    ImGui::EndMainMenuBar();
}



void Kolskoot::onGuiRender(Gui* _gui)
{
    static bool init = true;
    if (init)
    {
        guiStyle();
        init = false;
    }

    ImGui::PushFont(_gui->getFont("roboto_20"));
    {
        onGuiMenubar(_gui);

        if (showPointGrey)
        {
            Gui::Window pointgreyPanel(_gui, "PointGrey", { 900, 900 }, { 100, 100 });
            {
                ImGui::PushFont(_gui->getFont("roboto_32"));
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

                        ImGui::DragInt("dot min", &dot_min, 1, 1, 200);
                        ImGui::DragInt("dot max", &dot_max, 1, 1, 200);
                        ImGui::DragInt("threshold", &threshold, 1, 1, 100);
                        ImGui::DragInt("m_PG_dot_position", &m_PG_dot_position, 1, 0, 2);
                        pointGreyCamera->setDots(dot_min, dot_max, threshold, m_PG_dot_position);

                        if (ImGui::Button("Take Reference Image")) {
                            pointGreyCamera->setReferenceFrame(true, 5);
                        }

                        if (ImGui::Button("Calibrate")) {
                            modeCalibrate = 1;
                            showPointGrey = false;
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

                        if (pointGreyDiffBuffer)
                        {
                            pointgreyPanel.imageButton("testImage", pointGreyDiffBuffer, pointGreyCamera->bufferSize);
                        }
                        else
                        {
                            pointGreyDiffBuffer = Texture::create2D((int)pointGreyCamera->bufferSize.x, (int)pointGreyCamera->bufferSize.y, ResourceFormat::R8Unorm, 1, 1, pointGreyCamera->bufferData);
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
                ImGui::PopFont();

            }
            pointgreyPanel.release();
        }


        if (modeCalibrate > 0)
        {
            
            auto& style = ImGui::GetStyle();
            style.Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.8f, 0.8f, 1.f);
            style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.f);
            style.Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 1.f);

            Gui::Window calibrationPanel(_gui, "##Calibrate", { screenSize.x, screenSize.y }, { 0, 0 }, Gui::WindowFlags::NoResize);
            {
                ImGui::SetWindowPos(ImVec2(0, 0), 0);
                ImGui::SetWindowSize(ImVec2(screenSize.x, screenSize.y), 0);
                //ImGui::BringWindowToDisplayFront(calibrationPanel);
                ImGui::PushFont(_gui->getFont("roboto_32"));
                {

                    if (modeCalibrate == 1) {
                        style.Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.8f, 0.8f, 1.f);
                        ImGui::SetCursorPos(ImVec2(50, 50));
                        ImGui::Text("Top left");
                        ImGui::SetCursorPos(ImVec2(screenSize.x - 200, 50));
                        ImGui::Text("Top right");
                        ImGui::SetCursorPos(ImVec2(50, screenSize.y - 50));
                        ImGui::Text("Bottom left");
                        ImGui::SetCursorPos(ImVec2(screenSize.x - 200, screenSize.y - 50));
                        ImGui::Text("Bottom right");

                        ImGui::SetCursorPos(ImVec2(screenSize.x/2 - 150, screenSize.y/2 - 60));
                        ImGui::Text("remove the filter");
                        ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 - 30));
                        ImGui::Text("zoom and focus the camera");
                        ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 - 0));
                        ImGui::Text("set the brightness");
                        ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 + 30));
                        ImGui::Text("press 'space' when done");
                        pointGreyCamera->setReferenceFrame(true, 1);

                        style.Colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.3f, 1.f);
                        ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 + 100));
                        ImGui::Text("set the ");
                    }

                    if (modeCalibrate == 2) {
                        style.Colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.3f, 1.f);


                        ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 - 200));
                        ImGui::Text("adjust the light untill all 45 dots are found");
                        ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 - 160));
                        ImGui::Text("hide cursor by dragging it to the bottom of the screen");
                        uint ndots = pointGreyCamera->dotQueue.size();
                        while (pointGreyCamera->dotQueue.size()) {
                            pointGreyCamera->dotQueue.pop();
                        }
                        ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 - 120));
                        ImGui::Text("also log dot count here  %d / 45", ndots);

                        if (ndots == 45) {
                            ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 - 60));
                            ImGui::Text("all dots picked up, press space to save");
                        }

                        style.Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.8f, 0.8f, 1.f);

                        float dY = (screenSize.y - 40) / 4;
                        float dX = (screenSize.x - 40) / 8;


                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        static ImVec4 col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                        const ImU32 col32 = ImColor(col);

                        for (int y = 0; y < 5; y++)
                        {
                            for (int x = 0; x < 9; x++)
                            {
                                //ImGui::SetCursorPos(ImVec2(10 + x * dX, 10 + y * dY));
                                //ImGui::Text("o");
                                draw_list->AddCircle(ImVec2(20 + (x * dX), 20 + (y * dY)), 4, col32, 20, 6);
                            }
                        }
                    }
                }
                ImGui::PopFont();
            }
            calibrationPanel.release();
            

            if (modeCalibrate == 3) {
                modeCalibrate = 0;
            }
        }

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

    guiStyle();
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
    if (pointGreyDiffBuffer)
    {
        pRenderContext->updateTextureData(pointGreyDiffBuffer.get(), pointGreyCamera->bufferThreshold);
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

    if ((modeCalibrate > 0) &&(keyEvent.type == KeyboardEvent::Type::KeyPressed) && (keyEvent.key == Input::Key::Space))
    {
        modeCalibrate++;
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



void Kolskoot::guiStyle()
{
#define HI(v)   ImVec4(0.502f, 0.075f, 0.256f, v)
#define MED(v)  ImVec4(0.455f, 0.198f, 0.301f, v)
#define LOW(v)  ImVec4(0.232f, 0.201f, 0.271f, v)
#define BG(v)   ImVec4(0.200f, 0.220f, 0.270f, v)
#define TEXTG(v) ImVec4(0.860f, 0.930f, 0.890f, v)
#define LIME(v) ImVec4(0.38f, 0.52f, 0.10f, v);
#define DARKLIME(v) ImVec4(0.06f, 0.1f, 0.03f, v);

    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.0f, 0.0f, 1.f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.17f, 0.70f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.14f, 0.17f, 0.90f);

    style.Colors[ImGuiCol_Button] = ImVec4(0.03f, 0.03f, 0.3f, 0.5f);
    style.Colors[ImGuiCol_ButtonHovered] = DARKLIME(1.f);
    style.Colors[ImGuiCol_HeaderHovered] = DARKLIME(1.f);

    style.Colors[ImGuiCol_Tab] = ImVec4(0.03f, 0.03f, 0.03f, 1.f);
    style.Colors[ImGuiCol_TabHovered] = DARKLIME(1.f);
    style.Colors[ImGuiCol_TabActive] = DARKLIME(1.f);

    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.03f, 0.03f, 0.03f, 1.0f);

    style.Colors[ImGuiCol_ModalWindowDimBg] = DARKLIME(0.3f);

    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.f, 0.f, 0.f, 0.8f);

    style.FrameRounding = 4.0f;

    style.ScrollbarSize = 10;
    style.FrameBorderSize = 0;

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
