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

#pragma optimize("", off)

FILE* logFile;

#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}





void Earthworks_4::onGuiMenubar(Gui* _gui)
{
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::Text("Earthworks 4");

        terrain.onGuiMenubar(_gui);

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

        ImGui::SetCursorPos(ImVec2(screenSize.x - 100, 0));
        ImGui::Text("%3.1f fps", 1000.0 / gpFramework->getFrameRate().getAverageFrameTime());

        ImGui::SetCursorPos(ImVec2(screenSize.x - 15, 0));
        if (ImGui::Selectable("X")) { gpFramework->getWindow()->shutdown(); }
    }
    ImGui::EndMainMenuBar();
}



void Earthworks_4::onGuiRender(Gui* _gui)
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
        terrain.onGuiRender(_gui);

        if (aboutTex && showAbout) {
            Gui::Window a(_gui, "About", { aboutTex->getWidth(), aboutTex->getHeight() }, { 0, 100 });
            a.image("#about", aboutTex, float2(aboutTex->getWidth(), aboutTex->getHeight()));
        }
    }
    ImGui::PopFont();
}



void Earthworks_4::onLoad(RenderContext* _renderContext)
{
    graphicsState = GraphicsState::create();

    BlendState::Desc bsDesc;
    bsDesc.setRtBlend(0, true).setRtParams(0, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::One, BlendState::BlendFunc::Zero);
    graphicsState->setBlendState(BlendState::create(bsDesc));

    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthEnabled(true);
    dsDesc.setDepthWriteMask(true);
    graphicsState->setDepthStencilState(DepthStencilState::create(dsDesc));

    RasterizerState::Desc rsDesc;
    rsDesc.setCullMode(RasterizerState::CullMode::None);
    graphicsState->setRasterizerState(RasterizerState::create(rsDesc));
    //graphicsState->setViewport(0, GraphicsState::Viewport(0, 0, 1000, 1000, 0.f, 1.f), true);

    camera = Camera::create();
    camera->setDepthRange(0.1f, 40000.0f);
    camera->setAspectRatio(1920.0f / 1080.0f);
    camera->setFocalLength(15.0f);
    camera->setPosition(float3(0, 900, 0));
    camera->setTarget(float3(0, 900, 100));

    terrain.onLoad(_renderContext, logFile);
    atmosphere.onLoad(_renderContext, logFile);
    terrain.terrainShader.Vars()->setTexture("gAtmosphereInscatter", atmosphere.getFar().inscatter);
    terrain.terrainShader.Vars()->setTexture("gAtmosphereOutscatter", atmosphere.getFar().outscatter);
    terrain.terrainShader.Vars()->setTexture("SunInAtmosphere", atmosphere.sunlightTexture);
    //terrain.terrainShader.Vars()->setTexture("gSmokeAndDustInscatter", compressed_Albedo_Array);
    //terrain.terrainShader.Vars()->setTexture("gSmokeAndDustOutscatter", compressed_Albedo_Array);

    FILE* file = fopen("camera.bin", "rb");
    if (file) {
        CameraData data;
        fread(&data, sizeof(CameraData), 1, file);
        camera->getData() = data;
        fclose(file);
    }

    std::ifstream is("earthworks4_presets.xml");
    if (is.good()) {
        cereal::XMLInputArchive archive(is);
        serialize(archive, 100);
    }

    aboutTex = Texture::createFromFile("earthworks_4/about.dds", false, true);

    tonemapper.load("Samples/Earthworks_4/hlsl/compute_tonemapper.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);

}



void Earthworks_4::onFrameUpdate(RenderContext* _renderContext)
{
    FALCOR_PROFILE("onFrameUpdate");
    atmosphere.setSunDirection(global_sun_direction);
    atmosphere.getFar().setCamera(camera);
    atmosphere.computeSunInAtmosphere(_renderContext);
    atmosphere.computeVolumetric(_renderContext);

    terrain.terrainShader.Vars()["LightsCB"]["sunDirection"] = global_sun_direction; // should come from somewehere else common
    //terrain.terrainShader.Vars()["LightsCB"]["sunColour"] = float3(10, 10, 10);

    terrain.terrainShader.Vars()["PerFrameCB"]["screenSize"] = screenSize;
    terrain.terrainShader.Vars()["PerFrameCB"]["fog_far_Start"] = atmosphere.getFar().m_params._near;
    terrain.terrainShader.Vars()["PerFrameCB"]["fog_far_log_F"] = atmosphere.getFar().m_logEnd;
    terrain.terrainShader.Vars()["PerFrameCB"]["fog_far_one_over_k"] = atmosphere.getFar().m_oneOverK;
    //terrain.terrainShader.Vars()["PerFrameCB"]["fog_near_Start"] = gis_overlay.strenght;
    //terrain.terrainShader.Vars()["PerFrameCB"]["fog_near_log_F"] = gis_overlay.strenght;
    //terrain.terrainShader.Vars()["PerFrameCB"]["fog_near_one_over_k"] = gis_overlay.strenght;

    terrain.setCamera(CameraType_Main_Center, toGLM(camera->getViewMatrix()), toGLM(camera->getProjMatrix()), camera->getPosition(), true, 1920);
    terrain.update(_renderContext);
}



void Earthworks_4::onFrameRender(RenderContext* _renderContext, const Fbo::SharedPtr& pTargetFbo)
{
    onFrameUpdate(_renderContext);

    // clear
    graphicsState->setFbo(pTargetFbo);
    const float4 clearColor(0.0f, 0.f, 0.f, 1);
    _renderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    _renderContext->clearFbo(hdrFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);

    {
        // blit the sky
        glm::vec4 srcRect = glm::vec4(0, 0, 256, 128);
        glm::vec4 dstRect = glm::vec4(0, 0, screenSize.x, screenSize.y);
        _renderContext->blit(atmosphere.getFar().inscatter_sky->getSRV(0, 1, 0, 1), hdrFbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Linear);
    }

    terrain.onFrameRender(_renderContext, hdrFbo, camera, graphicsState, viewport3d, hdrHalfCopy);

    tonemapper.Vars()->setTexture("hdr", hdrFbo->getColorTexture(0));
    tonemapper.State()->setFbo(pTargetFbo);
    tonemapper.State()->setRasterizerState(graphicsState->getRasterizerState());
    tonemapper.drawInstanced(_renderContext, 3, 1);

    onRenderOverlay(_renderContext, pTargetFbo);

    _renderContext->blit(hdrFbo->getColorTexture(0)->getSRV(0, 1, 0, 1), hdrHalfCopy->getRTV());
    /*
    if (changed) slowTimer = 10;
    slowTimer--;
    if (refresh.minimal && (slowTimer < 0))
    {
        Sleep(30);      // aim for 15fps in this mode
    }*/
}



void Earthworks_4::onRenderOverlay(RenderContext* _renderContext, const Fbo::SharedPtr& pTargetFbo)
{
    Texture::SharedPtr tex = terrafectorEditorMaterial::static_materials.getDisplayTexture();
    if (!tex) tex = _plantMaterial::static_materials_veg.getDisplayTexture();
    if (!tex) {
        tex = roadNetwork::displayThumbnail;
    }
    if (tex)
    {
        uint w = tex->getWidth();
        uint h = tex->getHeight();
        uint scale = __max(1, __max(w / 1024, h / 1024));
        glm::vec4 srcRect = glm::vec4(0, 0, w, h);
        glm::vec4 dstRect = glm::vec4(1000, 30, 1000, 30) + glm::vec4(0, 0, w / scale, h / scale);
        _renderContext->blit(tex->getSRV(0, 1, 0, 1), pTargetFbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Point);
        terrafectorEditorMaterial::static_materials.dispTexIndex = -1;
        _plantMaterial::static_materials_veg.dispTexIndex = -1;
    }
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
    std::ofstream os("earthworks4_presets.xml");
    if (os.good()) {
        cereal::XMLOutputArchive archive(os);
        serialize(archive, 100);
    }
}



bool Earthworks_4::onKeyEvent(const KeyboardEvent& _keyEvent)
{
    slowTimer = 10;
    terrain.onKeyEvent(_keyEvent);
    return false;
}



bool Earthworks_4::onMouseEvent(const MouseEvent& _mouseEvent)
{
    slowTimer = 10;
    terrain.onMouseEvent(_mouseEvent, screenSize, screenMouseScale, screenMouseOffset, camera);
    return false;
}



void Earthworks_4::onResizeSwapChain(uint32_t _width, uint32_t _height)
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
    desc.setColorTarget(0u, ResourceFormat::R11G11B10Float);        // add , true if we want to write to it UAV
    hdrFbo = Fbo::create2D(_width, _height, desc);

    hdrHalfCopy = Texture::create2D(_width / 2, _height / 2, ResourceFormat::R11G11B10Float, 1, 7, nullptr, Falcor::Resource::BindFlags::AllColorViews);
}



void Earthworks_4::guiStyle()
{
#define HI(v)   ImVec4(0.502f, 0.075f, 0.256f, v)
#define MED(v)  ImVec4(0.455f, 0.198f, 0.301f, v)
#define LOW(v)  ImVec4(0.232f, 0.201f, 0.271f, v)
#define BG(v)   ImVec4(0.200f, 0.220f, 0.270f, v)
#define TEXTG(v) ImVec4(0.860f, 0.930f, 0.890f, v)
#define LIME(v) ImVec4(0.38f, 0.52f, 0.10f, v);
#define DARKLIME(v) ImVec4(0.06f, 0.1f, 0.03f, v);

    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.02f, 0.01f, 0.80f);
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

}



int main(int argc, char** argv)
{
    bool allScreens = false;
    for (int i = 0; i < argc; i++)
    {
        if (std::string(argv[i]).find("-allscreens") != std::string::npos) allScreens = true;
    }

    Earthworks_4::UniquePtr pRenderer = std::make_unique<Earthworks_4>();

    SampleConfig config;
    config.windowDesc.title = "Earthworks 4";
    config.windowDesc.resizableWindow = false;
    config.windowDesc.mode = Window::WindowMode::Fullscreen;
    if (allScreens) {
        //config.windowDesc.mode = Window::WindowMode::AllScreens;
    }
    config.windowDesc.width = 2560;
    config.windowDesc.height = 1140;
    config.windowDesc.monitor = 0;

    logFile = fopen("log.txt", "w");

    Sample::run(config, pRenderer);
    return 0;
}
