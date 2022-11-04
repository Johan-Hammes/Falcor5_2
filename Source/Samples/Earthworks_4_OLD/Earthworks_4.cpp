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
#include "Earthworks_4.h"
#include "dear_imgui/imgui.h"

#if FALCOR_D3D12_AVAILABLE
FALCOR_EXPORT_D3D12_AGILITY_SDK
#endif



void Earthworks_4::onGuiMenubar(Gui* pGui)
{
    ImGui::PushFont( pGui->getFont("roboto_20") );
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::Text("Earthworks 4");

        terrain.onGuiMenubar(pGui);

        ImGui::SetCursorPos(ImVec2(screenSize.x - 15, 0));
        if (ImGui::Selectable("X")) { gpFramework->getWindow()->shutdown(); }
    }
    ImGui::EndMainMenuBar();
    ImGui::PopFont();
}



void Earthworks_4::onGuiRender(Gui* pGui)
{
    static bool init = true;
    if (init)
    {
        guiStyle(pGui);
        initGui(pGui);
        init = false;
    }
    onGuiMenubar(pGui);

    terrain.onGuiRender(pGui);
}



void Earthworks_4::onLoad(RenderContext* pRenderContext)
{
    

    graphicsState = GraphicsState::create();

    camera = Camera::create();
    camera->setDepthRange(0.5f, 40000.0f);
    camera->setAspectRatio(1920.0f / 1080.0f);
    camera->setFocalLength(15.0f);
    camera->setPosition(float3(0, 500, -2000));

    terrain.onLoad(pRenderContext);
}



void Earthworks_4::onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
    const float4 clearColor(0.38f, 0.52f, 0.10f, 1);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(hdrFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);

    terrain.onFrameRender(pRenderContext, pTargetFbo);
}



void Earthworks_4::onShutdown()
{
    terrain.onShutdown();
}



bool Earthworks_4::onKeyEvent(const KeyboardEvent& keyEvent)
{
    terrain.onKeyEvent(keyEvent);
    return false;
}



bool Earthworks_4::onMouseEvent(const MouseEvent& mouseEvent)
{
    terrain.onMouseEvent(mouseEvent);
    return false;
}



void Earthworks_4::onHotReload(HotReloadFlags reloaded)
{
}



void Earthworks_4::onResizeSwapChain(uint32_t width, uint32_t height)
{
    screenSize = float2(width, height);

    camera->setAspectRatio(screenSize.x/ screenSize.y);

    Fbo::Desc desc;
    desc.setDepthStencilTarget(ResourceFormat::D24UnormS8);
    desc.setColorTarget(0u, ResourceFormat::R11G11B10Float);
    hdrFbo = Fbo::create2D(width, height, desc);
    graphicsState->setFbo(hdrFbo);

    hdrHalfCopy = Texture::create2D(width / 2, height / 2, ResourceFormat::R11G11B10Float, 1, 7, nullptr, Falcor::Resource::BindFlags::RenderTarget);
}



void Earthworks_4::initGui(Gui* pGui)
{
    pGui->addFont("roboto_14", "Framework/Fonts/Roboto-Regular.ttf");
    pGui->addFont("roboto_14_bold", "Framework/Fonts/Roboto-Bold.ttf");
    pGui->addFont("roboto_14_italic", "Framework/Fonts/Roboto-Italic.ttf");

    pGui->addFont("roboto_20", "Framework/Fonts/Roboto-Regular.ttf", 20);
    pGui->addFont("roboto_20_bold", "Framework/Fonts/Roboto-Bold.ttf", 20);

    pGui->addFont("roboto_26", "Framework/Fonts/Roboto-Regular.ttf", 26);
    pGui->addFont("roboto_26_bold", "Framework/Fonts/Roboto-Bold.ttf", 26);
}



void Earthworks_4::guiStyle(Gui* pGui)
{
#define HI(v)   ImVec4(0.502f, 0.075f, 0.256f, v)
#define MED(v)  ImVec4(0.455f, 0.198f, 0.301f, v)
#define LOW(v)  ImVec4(0.232f, 0.201f, 0.271f, v)
#define BG(v)   ImVec4(0.200f, 0.220f, 0.270f, v)
#define TEXTG(v) ImVec4(0.860f, 0.930f, 0.890f, v)

    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.02f, 0.01f, 0.80f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.17f, 0.70f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.14f, 0.17f, 0.90f);

    style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.5f);
    style.Colors[ImGuiCol_ButtonHovered] = MED(0.86f);

    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.03f, 0.03f, 0.03f, 1.0f);

    style.FrameRounding = 4.0f;
}



int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    Earthworks_4::UniquePtr pRenderer = std::make_unique<Earthworks_4>();
    SampleConfig config;
    config.windowDesc.title = "Earthworks 4";
    config.windowDesc.resizableWindow = true;
    config.windowDesc.mode = Window::WindowMode::Fullscreen;
    config.windowDesc.width = 2560;
    config.windowDesc.height = 1440;
    //?? 10 bits vs 8bit backbuffer
    // And look at monitor settings from old falcor, needs change in Window.cpp

    Sample::run(config, pRenderer);
    return 0;
}
