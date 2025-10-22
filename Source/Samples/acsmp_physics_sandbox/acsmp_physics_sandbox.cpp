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
#include "acsmp_physics_sandbox.h"
#include "Utils/UI/TextRenderer.h"
#include "imgui.h"
#include "Core/Platform/MonitorInfo.h"
#include <filesystem>
#include <fstream>
#include <iostream>

//#pragma optimize("", off)


#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}



void acsmp_physics_sandbox::onGuiMenubar(Gui* _gui)
{
    auto& style = ImGui::GetStyle();

    if (ImGui::BeginMainMenuBar())
    {
        ImGui::PushFont(_gui->getFont("header2"));
        {
            ImGui::SetCursorPos(ImVec2(10, -3));
            ImGui::Text("acsmp_physics_sandbox");

            ImGui::SetCursorPos(ImVec2(screenSize.x - 150, -3));
            ImGui::Text("%3.1f fps", 1000.0 / gpFramework->getFrameRate().getAverageFrameTime());

            ImGui::SetCursorPos(ImVec2(screenSize.x - 15, -3));
            if (ImGui::Selectable("X")) { gpFramework->getWindow()->shutdown(); }
        }
        ImGui::PopFont();

        // call sub menus here

        if (ImGui::BeginMenu("settings"))
        {
            ImGui::Text("camera");
            float focal = camera->getFocalLength();
            if (ImGui::SliderFloat("focal length", &focal, 5, 100, "%.1fmm")) { camera->setFocalLength(focal); }
            if (ImGui::MenuItem("reset")) {
                camera->setDepthRange(0.3f, 20000.0f);
                camera->setAspectRatio(1920.0f / 1080.0f);
                camera->setFocalLength(15.0f);
                camera->setPosition(float3(0, 500, 0));
            }

            ImGui::Separator();
            ImGui::Checkbox("vsync", &refresh.vsync);
            ImGui::Checkbox("slow refresh", &refresh.minimal);
            TOOLTIP("GPU saver - sleep some of the time");
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}



void acsmp_physics_sandbox::onGuiRender(Gui* _gui)
{
    static bool first = true;
    if (first)
    {
        _gui->addFont("H1", "Framework/Fonts/Sienthas.otf", screenSize.y / 14);
        _gui->addFont("H2", "Framework/Fonts/Sienthas.otf", screenSize.y / 17);

        float scale = 1.f;
        if (screenSize.y > 1600.f) scale = 0.9f;
        _gui->addFont("header0", "Framework/Fonts/Nunito-Regular.ttf", scale * screenSize.y / 20);
        _gui->addFont("header1", "Framework/Fonts/Nunito-Regular.ttf", scale * screenSize.y / 40);
        _gui->addFont("header2", "Framework/Fonts/Nunito-Bold.ttf", scale * screenSize.y / 55);
        _gui->addFont("default", "Framework/Fonts/Nunito-Regular.ttf", scale * screenSize.y / 65);
        _gui->addFont("bold", "Framework/Fonts/Nunito-Bold.ttf", scale * screenSize.y / 65);
        _gui->addFont("italic", "Framework/Fonts/Nunito-Italic.ttf", scale * screenSize.y / 65);

        _gui->addFont("small", "Framework/Fonts/Nunito-Bold.ttf", scale * screenSize.y / 100);
        guiStyle();
        first = false;
    }

    ImGui::GetIO().FontDefault = _gui->getFont("default");
    
    if (showEditGui)
    {
        if (!hideGui)
        {
            {
                onGuiMenubar(_gui);

                // call other gui's here
            }
        }
    }
    else
    {
        // this is the fullscreen HUD mode
        ImGuiIO& io = ImGui::GetIO();
        io.WantCaptureMouse = true; // need to look when this has to be false

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
        {
            Gui::Window fullscreen(_gui, "##fullscreen", { screenSize.x, screenSize.y }, { 0, 0 }, Gui::WindowFlags::Empty | Gui::WindowFlags::NoResize);
            {
                fullscreen.windowPos(0, 0);
                fullscreen.windowSize((int)screenSize.x, (int)screenSize.y);
            }
            fullscreen.release();
        }
        ImGui::PopStyleColor();
    }
}



void acsmp_physics_sandbox::onLoad(RenderContext* _renderContext)
{
    graphicsState = GraphicsState::create();
    graphicsState->setBlendState(BlendState::create(BlendState::Desc().setRtBlend(0, true).setRtParams(0, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::One, BlendState::BlendFunc::Zero)));
    graphicsState->setDepthStencilState(DepthStencilState::create(DepthStencilState::Desc().setDepthEnabled(true).setDepthWriteMask(true)));
    graphicsState->setRasterizerState(RasterizerState::create(RasterizerState::Desc().setCullMode(RasterizerState::CullMode::None)));

    Sampler::Desc samplerDesc;
    samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp).setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setMaxAnisotropy(8);
    sampler_Clamp = Sampler::create(samplerDesc);

    camera = Camera::create();
    camera->setDepthRange(0.1f, 40000.0f);
    camera->setAspectRatio(1920.0f / 1080.0f);
    camera->setFocalLength(15.0f);
    camera->setPosition(float3(0, 900, 0));
    camera->setTarget(float3(0, 900, 100));

    // load the last camera position
    FILE* file = fopen("camera.bin", "rb");
    if (file) {
        CameraData data;
        fread(&data, sizeof(CameraData), 1, file);
        camera->getData() = data;
        fclose(file);
    }

    std::ifstream is("presets.json");
    if (is.good()) {
        cereal::JSONInputArchive archive(is);
        serialize(archive, 100);
    }


    tonemapper.load("Samples/Earthworks_4/hlsl/compute_tonemapper.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
    //loadColorCube("F:/terrains/colorcubes/K_TONE Vintage_KODACHROME.cube");
    // FIXME Ineed to solve resource dir better than in terrain, or colour cubes needs to live somewhere else
    //loadColorCube(terrain.settings.dirResource + "/colorcubes/ColdChrome.cube");
}



void acsmp_physics_sandbox::loadColorCube(std::string _name)
{
    std::string line;
    float3 color;
    std::ifstream cube(_name);
    if (cube.good())
    {
        std::getline(cube, line);
        std::getline(cube, line);
        std::getline(cube, line);
        std::getline(cube, line);
        std::getline(cube, line);

        float3 colorcube[33][33][33];
        for (int i = 0; i < 33; i++)
        {
            for (int j = 0; j < 33; j++)
            {
                for (int k = 0; k < 33; k++)
                {
                    cube >> colorcube[i][j][k].x >> colorcube[i][j][k].y >> colorcube[i][j][k].z;
                }
            }
        }
        colorCube = Texture::create3D(33, 33, 33, Falcor::ResourceFormat::RGB32Float, 1, colorcube, Falcor::Resource::BindFlags::ShaderResource);
    }
}



void acsmp_physics_sandbox::onFrameUpdate(RenderContext* _renderContext)
{
    FALCOR_PROFILE("onFrameUpdate");
}



void acsmp_physics_sandbox::onFrameRender(RenderContext* _renderContext, const Fbo::SharedPtr& pTargetFbo)
{
    gpDevice->toggleVSync(refresh.vsync);

    // ??? Cant we just loop this until fullResetDoNotRender maybe even inside
    onFrameUpdate(_renderContext);

    // render {} contains FALCOR_PROFILE
    {
        FALCOR_PROFILE("onFrameRender");

        // clear
        {
            graphicsState->setFbo(pTargetFbo);
            const float4 clearColor(0.0f, 0.f, 0.f, 1);
            _renderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
            _renderContext->clearFbo(hdrFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
        }

        // render here

        {
            FALCOR_PROFILE("tonemapper");
            tonemapper.Vars()->setTexture("hdr", hdrFbo->getColorTexture(0));
            tonemapper.Vars()->setTexture("cube", colorCube);
            tonemapper.Vars()->setSampler("linearSampler", sampler_Clamp);
            tonemapper.State()->setFbo(pTargetFbo);
            tonemapper.State()->setRasterizerState(graphicsState->getRasterizerState());
            tonemapper.drawInstanced(_renderContext, 3, 1);
        }

        // copy frame buffer to the half buffer for previous frame
        _renderContext->blit(hdrFbo->getColorTexture(0)->getSRV(0, 1, 0, 1), hdrHalfCopy->getRTV());
    }

    if (refresh.minimal)
    {
        // This mode saves your GPU during normal work while allowing to quickly test full performance
        // aim for 15fps in this mode
        Sleep(20);
    }
}





void acsmp_physics_sandbox::onShutdown()
{
    FILE* file = fopen("camera.bin", "wb");
    if (file) {
        CameraData& data = camera->getData();
        fwrite(&data, sizeof(CameraData), 1, file);
        fclose(file);
    }
    std::ofstream os("presets.json");
    if (os.good()) {
        cereal::JSONOutputArchive archive(os);
        serialize(archive, 100);
    }
}



bool acsmp_physics_sandbox::onKeyEvent(const KeyboardEvent& _keyEvent)
{
    if (_keyEvent.type == KeyboardEvent::Type::KeyPressed)
    {
        if (_keyEvent.key == Input::Key::D)
        {
            showEditGui = !showEditGui;
        }
        if (_keyEvent.key == Input::Key::Q)
        {
            hideGui = !hideGui;
        }
    }
    return false;
}



bool acsmp_physics_sandbox::onMouseEvent(const MouseEvent& _mouseEvent)
{
    return false;
}



void acsmp_physics_sandbox::onResizeSwapChain(uint32_t _width, uint32_t _height)
{
    auto monitorDescs = MonitorInfo::getMonitorDescs();
    screenSize = float2(_width, _height);
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
    desc.setColorTarget(0u, ResourceFormat::R11G11B10Float);        // add , true if we want to write to it UAV
    hdrFbo = Fbo::create2D(_width, _height, desc);

    hdrHalfCopy = Texture::create2D(_width / 2, _height / 2, ResourceFormat::R11G11B10Float, 1, 1, nullptr, Falcor::Resource::BindFlags::AllColorViews);
}



void acsmp_physics_sandbox::guiStyle()
{
#define HI(v)   ImVec4(0.502f, 0.075f, 0.256f, v)
#define MED(v)  ImVec4(0.455f, 0.198f, 0.301f, v)
#define LOW(v)  ImVec4(0.232f, 0.201f, 0.271f, v)
#define BG(v)   ImVec4(0.200f, 0.220f, 0.270f, v)
#define TEXTG(v) ImVec4(0.860f, 0.930f, 0.890f, v)
#define LIME(v) ImVec4(0.38f, 0.52f, 0.10f, v);
#define DARKLIME(v) ImVec4(0.06f, 0.1f, 0.03f, v);
#define GREY(i,v) ImVec4(i, i, i, v);

    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.01f, 0.01f, 0.01f, 0.90f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.17f, 0.70f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.14f, 0.17f, 0.90f);

    style.Colors[ImGuiCol_Button] = GREY(0.01f, 1.f);
    style.Colors[ImGuiCol_ButtonHovered] = GREY(0.03f, 1.f);
    style.Colors[ImGuiCol_HeaderHovered] = DARKLIME(1.f);

    style.Colors[ImGuiCol_Tab] = ImVec4(0.03f, 0.03f, 0.03f, 1.f);
    style.Colors[ImGuiCol_TabHovered] = DARKLIME(1.f);
    style.Colors[ImGuiCol_TabActive] = DARKLIME(1.f);

    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.03f, 0.03f, 0.03f, 1.0f);

    style.Colors[ImGuiCol_ModalWindowDimBg] = DARKLIME(0.3f);

    style.Colors[ImGuiCol_FrameBg] = GREY(0.f, 0.93f);
    style.Colors[ImGuiCol_FrameBgHovered] = GREY(0.03f, 0.93f);
    style.Colors[ImGuiCol_FrameBgActive] = GREY(0.02f, 0.93f);

    style.FrameRounding = 0.f;
    style.FramePadding = ImVec2(0, 0);

    style.ScrollbarSize = 25;

    style.WindowPadding = ImVec2(0, 0);              // Padding within a window.
    style.WindowRounding = 0.f;                     // Radius of window corners rounding. Set to 0.0f to have rectangular windows.
    style.WindowBorderSize = 0.f;
}



int main(int argc, char** argv)
{
    bool allScreens = false;
    for (int i = 0; i < argc; i++)
    {
        if (std::string(argv[i]).find("-allscreens") != std::string::npos) allScreens = true;
    }

    SampleConfig config;
    config.windowDesc.title = "acsmp_physics_sandbox";
    config.windowDesc.resizableWindow = true;
    config.windowDesc.mode = Window::WindowMode::Normal;
    if (allScreens) {
        //config.windowDesc.mode = Window::WindowMode::AllScreens;
    }
    //config.windowDesc.width = 800;
    //config.windowDesc.height = 600;
    //config.windowDesc.monitor = 0;

    // HDR
    // this code is needed to run correctly on an HDR monitor, no idea how to automatically detect this
    // config.windowDesc.monitor = 1;
    //config.deviceDesc.colorFormat = ResourceFormat::RGB10A2Unorm;

    acsmp_physics_sandbox::UniquePtr pRenderer = std::make_unique<acsmp_physics_sandbox>();
    Sample::run(config, pRenderer);

    return 0;
}
