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
#include "Utils/UI/TextRenderer.h"
#include "imgui.h"
#include "Core/Platform/MonitorInfo.h"

#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}

void Earthworks_4::onGuiMenubar(Gui* pGui)
{
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::Text("Earthworks 4");

        terrain.onGuiMenubar(pGui);

        if (ImGui::BeginMenu("settings"))
        {
            ImGui::Text("camera");
            float focal = camera->getFocalLength();
            if( ImGui::SliderFloat("focal length", &focal, 5, 100, "%.1fmm")) { camera->setFocalLength(focal); }
            if (ImGui::MenuItem("reset")) {
                camera->setDepthRange(0.5f, 40000.0f);
                camera->setAspectRatio(1920.0f / 1080.0f);
                camera->setFocalLength(15.0f);
                camera->setPosition(float3(0, 500, 0));
            }
            //camera->renderUI()

            ImGui::Separator();
            ImGui::Checkbox("vsync", &refresh.vsync);
            ImGui::Checkbox("only on change", &refresh.minimal);
            TOOLTIP("ultimate low power - only refresh on changes");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("help"))
        {
            ImGui::Checkbox("about Earthworks 4", &showAbout);
            ImGui::EndMenu();
        }

        ImGui::SetCursorPos(ImVec2(screenSize.x - 15, 0));
        if (ImGui::Selectable("X")) { gpFramework->getWindow()->shutdown(); }
    }
    ImGui::EndMainMenuBar();
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

    ImGui::PushFont(pGui->getFont("roboto_20"));
    {
        onGuiMenubar(pGui);
        terrain.onGuiRender(pGui);
        Gui::Window w(pGui, "Test", { 400, 300 }, { 0, 100 });

        if (aboutTex && showAbout) {
            //ImGui::OpenPopup("1234");
            Gui::Window a(pGui, "About", { aboutTex->getWidth(), aboutTex->getHeight() }, { 0, 100 });
            a.image("#about", aboutTex, float2(aboutTex->getWidth(), aboutTex->getHeight()));
        }
    }
    ImGui::PopFont();
}



void Earthworks_4::onLoad(RenderContext* pRenderContext)
{
    graphicsState = GraphicsState::create();

    camera = Camera::create();
    camera->setDepthRange(0.5f, 40000.0f);
    camera->setAspectRatio(1920.0f / 1080.0f);
    camera->setFocalLength(15.0f);
    camera->setPosition(float3(0, 500, 0));

    terrain.onLoad(pRenderContext);

    FILE* file = fopen("camera.bin", "rb");
    if (file) {
        CameraData data;
        fread(&data, sizeof(CameraData), 1, file);
        camera->getData() = data;
        fclose(file);
    }
    std::ifstream is("earthworks4_presets.json");
    if (is.good()) {
        cereal::JSONInputArchive archive(is);
        serialize(archive, 100);
    }

    aboutTex = Texture::createFromFile("earthworks_4/about.dds", false, true);
}



void Earthworks_4::onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
    //const float4 clearColor(0.38f, 0.52f, 0.10f, 1);
    const float4 clearColor(0.01f, 0.01f, 0.01f, 1);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(hdrFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);

    terrain.onFrameRender(pRenderContext, pTargetFbo);
}



void Earthworks_4::onShutdown()
{
    terrain.onShutdown();

    FILE* file = fopen("camera.bin", "wb");
    if (file) {
        CameraData& data = camera->getData();
        fwrite(&data, sizeof(CameraData), 1, file);
        fclose(file);
    }
    std::ofstream os("earthworks4_presets.json");
    if (os.good()) {
        cereal::JSONOutputArchive archive(os);
        serialize(archive, 100);
    }
}



bool Earthworks_4::onKeyEvent(const KeyboardEvent& _keyEvent)
{
    terrain.onKeyEvent(_keyEvent);
    return false;
}



bool Earthworks_4::onMouseEvent(const MouseEvent& _mouseEvent)
{
    terrain.onMouseEvent(_mouseEvent);
    return false;
}



void Earthworks_4::onResizeSwapChain(uint32_t _width, uint32_t _height)
{
    auto monitorDescs = MonitorInfo::getMonitorDescs();
    screenSize = monitorDescs[0].resolution;
    viewport3d.width = screenSize.x / (float)_width;
    viewport3d.height = screenSize.y / (float)_height;

    camera->setAspectRatio(screenSize.x / screenSize.y);

    Fbo::Desc desc;
    desc.setDepthStencilTarget(ResourceFormat::D24UnormS8);
    desc.setColorTarget(0u, ResourceFormat::R11G11B10Float);
    hdrFbo = Fbo::create2D(_width, _height, desc);
    graphicsState->setFbo(hdrFbo);

    hdrHalfCopy = Texture::create2D(_width / 2, _height / 2, ResourceFormat::R11G11B10Float, 1, 7, nullptr, Falcor::Resource::BindFlags::RenderTarget);
}



void Earthworks_4::initGui(Gui* pGui)
{
    //pGui->addFont("roboto_14", "Framework/Fonts/Roboto-Regular.ttf");
    //pGui->addFont("roboto_14_bold", "Framework/Fonts/Roboto-Bold.ttf");
    //pGui->addFont("roboto_14_italic", "Framework/Fonts/Roboto-Italic.ttf");

    //pGui->addFont("roboto_20", "Framework/Fonts/Roboto-Regular.ttf", 20);
    //pGui->addFont("roboto_20_bold", "Framework/Fonts/Roboto-Bold.ttf", 20);

    //pGui->addFont("roboto_26", "Framework/Fonts/Roboto-Regular.ttf", 26);
    //pGui->addFont("roboto_26_bold", "Framework/Fonts/Roboto-Bold.ttf", 26);
}



void Earthworks_4::guiStyle(Gui* pGui)
{
#define HI(v)   ImVec4(0.502f, 0.075f, 0.256f, v)
#define MED(v)  ImVec4(0.455f, 0.198f, 0.301f, v)
#define LOW(v)  ImVec4(0.232f, 0.201f, 0.271f, v)
#define BG(v)   ImVec4(0.200f, 0.220f, 0.270f, v)
#define TEXTG(v) ImVec4(0.860f, 0.930f, 0.890f, v)
#define LIME(v) ImVec4(0.38f, 0.52f, 0.10f, v);
#define DARKLIME(v) ImVec4(0.12f, 0.15f, 0.03f, v);

    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.17f, 0.70f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.14f, 0.17f, 0.90f);

    style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.5f);
    style.Colors[ImGuiCol_ButtonHovered] = DARKLIME(1.f);
    style.Colors[ImGuiCol_HeaderHovered] = DARKLIME(1.f);

    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.03f, 0.03f, 0.03f, 1.0f);

    style.FrameRounding = 4.0f;
}



int main(int argc, char** argv)
{
    Earthworks_4::UniquePtr pRenderer = std::make_unique<Earthworks_4>();

    SampleConfig config;
    config.windowDesc.title = "Earthworks 4";
    config.windowDesc.resizableWindow = true;
    config.windowDesc.mode = Window::WindowMode::Fullscreen;
    config.windowDesc.width = 2560;
    config.windowDesc.height = 1440;
    config.windowDesc.monitor = 0;

    Sample::run(config, pRenderer);
    return 0;
}
