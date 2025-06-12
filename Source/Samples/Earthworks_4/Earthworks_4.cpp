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
#include <filesystem>
#include <iostream>

 //#pragma optimize("", off)

FILE* logFile;

#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}





void Earthworks_4::onGuiMenubar(Gui* _gui)
{
    auto& style = ImGui::GetStyle();
    switch (terrain.terrainMode)
    {
    case _terrainMode::vegetation:      style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.02f, 0.04f, 0.01f, 1.0f);   break;
    case _terrainMode::ecotope:         style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.01f, 0.06f, 0.06f, 1.0f);   break;
    case _terrainMode::terrafector:     style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.24f, 0.14f, 0.0f, 1.0f);   break;
    case _terrainMode::roads:           style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.005f, 0.005f, 0.005f, 1.0f);   break;
    case _terrainMode::glider:          style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);   break;
    }

    if (ImGui::BeginMainMenuBar())
    {
        ImGui::PushFont(_gui->getFont("header2"));
        {
            ImGui::SetCursorPos(ImVec2(10, -3));
            ImGui::Text("Earthworks 4");

            ImGui::SetCursorPos(ImVec2(screenSize.x - 120, -3));
            ImGui::Text("%3.1f fps", 1000.0 / gpFramework->getFrameRate().getAverageFrameTime());

            ImGui::SetCursorPos(ImVec2(screenSize.x - 15, -3));
            if (ImGui::Selectable("X")) { gpFramework->getWindow()->shutdown(); }
        }
        ImGui::PopFont();

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

    

        ImGui::EndMainMenuBar();
    }
}



void Earthworks_4::onGuiRender(Gui* _gui)
{
    static bool first = true;
    if (first)
    {
        std::cout << "onGuiRender() - first\n";
        fprintf(logFile, "add fonts Sienthas\n");
        fflush(logFile);
        _gui->addFont("H1", "Framework/Fonts/Sienthas.otf", screenSize.y / 14);
        _gui->addFont("H2", "Framework/Fonts/Sienthas.otf", screenSize.y / 17);

        float scale = 1.f;
        if (screenSize.y > 1600.f) scale = 0.8f;
        _gui->addFont("header1", "Framework/Fonts/Nunito-Regular.ttf", scale * screenSize.y / 40);
        _gui->addFont("header2", "Framework/Fonts/Nunito-Regular.ttf", scale * screenSize.y / 55);
        _gui->addFont("default", "Framework/Fonts/Nunito-Regular.ttf", scale * screenSize.y / 65);
        _gui->addFont("bold", "Framework/Fonts/Nunito-Bold.ttf", scale * screenSize.y / 65);
        _gui->addFont("italic", "Framework/Fonts/Nunito-Italic.ttf", scale * screenSize.y / 65);

        _gui->addFont("small", "Framework/Fonts/Nunito-Bold.ttf", scale * screenSize.y / 100);
        guiStyle();
        first = false;
    }


    if (showEditGui)
    {
        if (!hideGui)
        {
            ImGui::PushFont(_gui->getFont("default"));
            {
                onGuiMenubar(_gui);

                terrain.onGuiRender(_gui, &atmosphere.params);

                if (aboutTex && showAbout) {
                    Gui::Window a(_gui, "About", { aboutTex->getWidth(), aboutTex->getHeight() }, { 0, 100 });
                    a.image("#about", aboutTex, float2(aboutTex->getWidth(), aboutTex->getHeight()));
                }
            }
            ImGui::PopFont();
        }
    }
    //else if (!terrain.useFreeCamWhileGliding)
    else
    {
        ImGuiIO& io = ImGui::GetIO();
        io.WantCaptureMouse = true;
        if (terrain.useFreeCamWhileGliding || terrain.GliderDebugVisuals) {
            io.WantCaptureMouse = false;
        }
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
        Gui::Window all(_gui, "##fullscreen", { screenSize.x, screenSize.y }, { 0, 0 }, Gui::WindowFlags::Empty | Gui::WindowFlags::NoResize);
        {
            //all.graph()
            all.windowPos(0, 0);
            all.windowSize((int)screenSize.x, (int)screenSize.y);
            terrain.onGuiRenderParaglider(all, _gui, screenSize, &atmosphere.params);

            /*
            ImGui::PushFont(_gui->getFont("H1"));
            {
                //ImGui::SetCursorPos(ImVec2(screenSize.x - 100, 0));
                ImGui::SetCursorPos(ImVec2(100, 100));
                ImGui::Text("%3.1f fps", 1000.0 / gpFramework->getFrameRate().getAverageFrameTime());
            }*/
            ImGui::PopFont();
        }
        all.release();
        ImGui::PopStyleColor();

    }
}



void Earthworks_4::onLoad(RenderContext* _renderContext)
{
    std::cout << "onLoad()\n";




    /*
    float a = -10;
    float b = 0;
    float c = 10;

    for (int i = 0; i < 20; i++)
    {
        float d1 = b - a;
        float dd1 = (d1 - 1.f) / 2.f;
        a += dd1;
        b -= dd1;

        float d2 = c - b;
        float dd2 = (d2 - 1.f) / 2.f;
        b += dd2;
        c -= dd2;

        float avs = (a + b + c) / 3;

        fprintf(logFile, "%1.15f, %1.15f, %1.15f    avs   %1.15f\n", a, b, c, avs);
        fflush(logFile);
    }

    {
        // wing, line body test
        double p[5] = { 0, 2.5, 5, 7.5, 10};
        double pCLIP[5] = { 0, 2.5, 5, 7.5, 10 };
        double p0[5] = { 0, 2.5, 5, 7.5, 10 };
        double m[5] = { 100, 0.1, 0.1, 0.1, 5};
        double w[5] = { 0.01, 10, 10, 10, 0.2 };
        double f = 1000; //Newton
        double t = 0.00555555555555;
        double t2 = t * t;
        double dMin = 0.0000012;

        double CoM = (p[0] * m[0] + p[1] * m[1] + p[2] * m[2] + p[3] * m[3] + p[4] * m[4]) / (p[0] + p[1] + p[2] + p[3] + p[4] );

        fprintf(logFile, "\n\n");
        fprintf(logFile, "%1.15f, (%1.15f), (%1.15f), (%1.15f), (%1.15f), %1.15f     , %1.15f\n", p[0], p[1] - p[0], p[2] - p[1], p[3] - p[2], p[4] - p[3], p[4], CoM);
        //p[0] -= (f / m[0]) * t2;
        //p[4] += (f / m[4]) * t2;

        CoM = (p[0] * m[0] + p[1] * m[1] + p[2] * m[2] + p[3] * m[3] + p[4] * m[4]) / (p[0] + p[1] + p[2] + p[3] + p[4]);
        double dL;

        fprintf(logFile, "%1.15f, (%1.15f), (%1.15f), (%1.15f), (%1.15f), %1.15f     , %1.15f\n", p[0], p[1] - p[0], p[2] - p[1], p[3] - p[2], p[4] - p[3], p[4], CoM);
        fprintf(logFile, "\n\n");
        fprintf(logFile, "%1.15f mm, %1.15f mm           , %1.15f\n", (p[0] - p0[0]) * 1000, (p[4] - p0[4]) * 1000, CoM);

        for (int k = 0; k < 100; k++)
        {
            p[0] -= (f / m[0]) * t2;
            p[4] += (f / m[4]) * t2;

            for (int i = 0; i < 10; i++)
            {
                for (int c = 0; c < 4; c++)
                {
                    double wSum = w[c] + w[c + 1];
                    double l = (p[c + 1] - p[c]) - 2.5;
                    l = (double)((int)(l / dMin)) * dMin;
                    p[c] += w[c] / wSum * l;
                    p[c + 1] -= w[c + 1] / wSum * l;
                }
                p[0] = (double)((int)(p[0] / dMin)) * dMin;
                p[1] = (double)((int)(p[1] / dMin)) * dMin;
                p[2] = (double)((int)(p[2] / dMin)) * dMin;
                p[3] = (double)((int)(p[3] / dMin)) * dMin;
                p[4] = (double)((int)(p[4] / dMin)) * dMin;


                CoM = (p[0] * m[0] + p[1] * m[1] + p[2] * m[2] + p[3] * m[3] + p[4] * m[4]) / (p[0] + p[1] + p[2] + p[3] + p[4]);
                dL = ((p[4] - p[0]) - 10) * 1000;
                //fprintf(logFile, "%1.15f, (%1.10f), (%1.10f), (%1.10f), (%1.10f), %1.15f\n", p[0], p[1] - p[0] - 2.5, p[2] - p[1] - 2.5, p[3] - p[2] - 2.5, p[4] - p[3] - 2.5, p[4]);
                //fprintf(logFile, "%1.15f mm, %1.15f mm      dL %1.15f mm     , %1.15f\n", (p[0] - p0[0]) * 1000, (p[4] - p0[4]) * 1000, dL, CoM);
            }
            fprintf(logFile, "%1.15f mm, %1.15f mm      dL %1.15f mm     , %1.15f\n", (p[0] - p0[0]) * 1000, (p[4] - p0[4]) * 1000, dL, CoM);
        }
    }
    */

    /*
    float2 A = float2(-20906.8073893663f, 21854.7731031066f);
    float2 B = float2(-21739.4126790621f, -18140.4947773098f);
    float2 C = float2(19087.8133413223f, 21022.9225606067f);
    float2 D = float2(18256.7017062721f, -18972.3242194264f);

    float2 H1 = C - A;
    float2 H2 = D - B;

    float2 V1 = B - A;
    float2 V2 = D - C;

    float lh1 = glm::length(H1);
    float lh2 = glm::length(H2);
    float lv1 = glm::length(V1);
    float lv2 = glm::length(V2);

    double th1 = atan2(H1.y, H1.x);
    double th2 = atan2(H2.y, H2.x);
    double tv1 = atan2(V1.y, V1.x);
    double tv2 = atan2(V2.y, V2.x);

    float left = 2702000;
    float right = 2742000;
    float top = 1218000;
    float bottom = 1218000;
    */

    fprintf(logFile, "Earthworks_4::onLoad()\n");
    fflush(logFile);

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


    std::cout << "  terrain\n";
    terrain.onLoad(_renderContext, logFile);

    std::cout << "  atmosphere\n";
    fprintf(logFile, "atmosphere.onLoad()\n");
    fflush(logFile);
    atmosphere.onLoad(_renderContext, logFile);
    terrain.terrainShader.Vars()->setTexture("gAtmosphereInscatter", atmosphere.getFar().inscatter);
    terrain.terrainShader.Vars()->setTexture("gAtmosphereOutscatter", atmosphere.getFar().outscatter);
    terrain.terrainShader.Vars()->setTexture("SunInAtmosphere", atmosphere.sunlightTexture);


    terrain.terrainSpiteShader.Vars()->setTexture("gAtmosphereInscatter", atmosphere.getFar().inscatter);
    terrain.terrainSpiteShader.Vars()->setTexture("gAtmosphereOutscatter", atmosphere.getFar().outscatter);
    terrain.terrainSpiteShader.Vars()->setTexture("SunInAtmosphere", atmosphere.sunlightTexture);


    terrain.rappersvilleShader.Vars()->setTexture("gAtmosphereInscatter", atmosphere.getFar().inscatter);
    terrain.rappersvilleShader.Vars()->setTexture("gAtmosphereOutscatter", atmosphere.getFar().outscatter);
    terrain.rappersvilleShader.Vars()->setTexture("SunInAtmosphere", atmosphere.sunlightTexture);

    terrain.gliderwingShader.Vars()->setTexture("gAtmosphereInscatter", atmosphere.getFar().inscatter);
    terrain.gliderwingShader.Vars()->setTexture("gAtmosphereOutscatter", atmosphere.getFar().outscatter);
    terrain.gliderwingShader.Vars()->setTexture("SunInAtmosphere", atmosphere.sunlightTexture);

    //terrain.triangleShader.Vars()->setTexture("gAtmosphereInscatter", atmosphere.getFar().inscatter);
    //terrain.triangleShader.Vars()->setTexture("gAtmosphereOutscatter", atmosphere.getFar().outscatter);
    terrain.triangleShader.Vars()->setTexture("gAtmosphereInscatter_Sky", atmosphere.getFar().inscatter_sky);

    terrain.vegetation.ROOT.vegetationShader.Vars()->setTexture("gAtmosphereInscatter", atmosphere.getFar().inscatter);
    terrain.vegetation.ROOT.vegetationShader.Vars()->setTexture("gAtmosphereOutscatter", atmosphere.getFar().outscatter);
    terrain.vegetation.ROOT.vegetationShader.Vars()->setTexture("SunInAtmosphere", atmosphere.sunlightTexture);

    terrain.vegetation.ROOT.billboardShader.Vars()->setTexture("gAtmosphereInscatter", atmosphere.getFar().inscatter);
    terrain.vegetation.ROOT.billboardShader.Vars()->setTexture("gAtmosphereOutscatter", atmosphere.getFar().outscatter);
    terrain.vegetation.ROOT.billboardShader.Vars()->setTexture("SunInAtmosphere", atmosphere.sunlightTexture);



    std::cout << "  cfd\n";
    fprintf(logFile, "terrain.cfdStart()\n");
    fflush(logFile);
    terrain.cfdStart();
    std::thread thread_obj_cfd(&terrainManager::cfdThread, &terrain);
    thread_obj_cfd.detach();


    /*
    std::thread thread_obj_paraglider(&terrainManager::paragliderThread, &terrain, std::ref(glider_barrier));
    thread_obj_paraglider.detach();
    std::thread thread_obj_paraglider_B(&terrainManager::paragliderThread_B, &terrain, std::ref(glider_barrier));
    thread_obj_paraglider_B.detach();
    */

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
    //loadColorCube("F:/terrains/colorcubes/K_TONE Vintage_KODACHROME.cube");
    loadColorCube(terrain.settings.dirResource + "/colorcubes/ColdChrome.cube");

}


void Earthworks_4::loadColorCube(std::string name)
{
    std::string line;
    float3 color;
    std::ifstream cube(name);
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
    else
    {
        fprintf(logFile, "ERROR - loadColorCube() - failed\n");
        fflush(logFile);
    }
}






void Earthworks_4::onFrameUpdate(RenderContext* _renderContext)
{
    //global_sun_direction = glm::normalize(float3(1, -0.544f, -0.72));
    //global_sun_direction = glm::normalize(float3(1, -0.144f, 0));

    static bool first = true;
    if (first)
    {
        fprintf(logFile, "if (first)\n");
        fflush(logFile);
        first = false;
        terrain.shadowEdges.load(terrain.settings.dirRoot + "/gis/_export/root4096.bil", -global_sun_direction.y);
        terrain.shadowEdges.sunAngle = 0.95605f;
        terrain.shadowEdges.dAngle = 0.0001f;
        terrain.shadowEdges.requestNewShadow = true;

        terrain.terrainShadowTexture = Texture::create2D(4096, 4096, Falcor::ResourceFormat::RG32Float, 1, 1, terrain.shadowEdges.shadowH, Falcor::Resource::BindFlags::UnorderedAccess | Falcor::Resource::BindFlags::ShaderResource);

        std::thread thread_shadows(&_shadowEdges::solveThread, &terrain.shadowEdges);
        thread_shadows.detach();

        terrain.terrainShader.Vars()->setTexture("terrainShadow", terrain.terrainShadowTexture);
        terrain.rappersvilleShader.Vars()->setTexture("terrainShadow", terrain.terrainShadowTexture);
        terrain.gliderwingShader.Vars()->setTexture("terrainShadow", terrain.terrainShadowTexture);

        //terrain.terrainSpiteShader.Vars()->setTexture("terrainShadow", terrain.terrainShadowTexture);
        atmosphere.setTerrainShadow(terrain.terrainShadowTexture);

    }

    {
        static int cnt = 0;
        if (terrain.shadowEdges.shadowReady)
        {
            fprintf(logFile, "terrain.shadowEdges.shadowReady\n");
            fflush(logFile);
            FALCOR_PROFILE("shadow update");
            _renderContext->updateTextureData(terrain.terrainShadowTexture.get(), terrain.shadowEdges.shadowH);
            global_sun_direction = terrain.shadowEdges.sunAng;
            terrain.shadowEdges.shadowReady = false;
            cnt++;
        }

    }

    {
        FALCOR_PROFILE("onFrameUpdate");


        {
            float3 sunUp = {0, 1, 0};
            float3 sunRight = glm::normalize(glm::cross(sunUp, global_sun_direction));
            sunUp = glm::normalize(glm::cross(global_sun_direction, sunRight));
            terrain.vegetation.ROOT.vegetationShader.Vars()["LightsCB"]["sunDirection"] = global_sun_direction; // should come from somewehere else common
            terrain.vegetation.ROOT.vegetationShader.Vars()["LightsCB"]["sunRightVector"] = sunRight; // should come from somewehere else common
            terrain.vegetation.ROOT.vegetationShader.Vars()["LightsCB"]["sunUpVector"] = sunUp; // should come from somewehere else common
            terrain.vegetation.ROOT.vegetationShader.Vars()["PerFrameCB"]["screenSize"] = screenSize;
            terrain.vegetation.ROOT.vegetationShader.Vars()["PerFrameCB"]["fog_far_Start"] = atmosphere.getFar().m_params._near;
            terrain.vegetation.ROOT.vegetationShader.Vars()["PerFrameCB"]["fog_far_log_F"] = atmosphere.getFar().m_logEnd;
            terrain.vegetation.ROOT.vegetationShader.Vars()["PerFrameCB"]["fog_far_one_over_k"] = atmosphere.getFar().m_oneOverK;

            terrain.vegetation.ROOT.billboardShader.Vars()["LightsCB"]["sunDirection"] = global_sun_direction; // should come from somewehere else common
            terrain.vegetation.ROOT.billboardShader.Vars()["LightsCB"]["sunRightVector"] = sunRight; // should come from somewehere else common
            terrain.vegetation.ROOT.billboardShader.Vars()["LightsCB"]["sunUpVector"] = sunUp; // should come from somewehere else common
            terrain.vegetation.ROOT.billboardShader.Vars()["PerFrameCB"]["screenSize"] = screenSize;
            terrain.vegetation.ROOT.billboardShader.Vars()["PerFrameCB"]["fog_far_Start"] = atmosphere.getFar().m_params._near;
            terrain.vegetation.ROOT.billboardShader.Vars()["PerFrameCB"]["fog_far_log_F"] = atmosphere.getFar().m_logEnd;
            terrain.vegetation.ROOT.billboardShader.Vars()["PerFrameCB"]["fog_far_one_over_k"] = atmosphere.getFar().m_oneOverK;

            terrain.terrainShader.Vars()["LightsCB"]["sunDirection"] = global_sun_direction; // should come from somewehere else common
            terrain.rappersvilleShader.Vars()["LightsCB"]["sunDirection"] = global_sun_direction; // should come from somewehere else common
            terrain.gliderwingShader.Vars()["LightsCB"]["sunDirection"] = global_sun_direction; // should come from somewehere else common
            //terrain.terrainShader.Vars()["LightsCB"]["sunColour"] = float3(10, 10, 10);

            terrain.terrainShader.Vars()["PerFrameCB"]["screenSize"] = screenSize;
            terrain.terrainShader.Vars()["PerFrameCB"]["fog_far_Start"] = atmosphere.getFar().m_params._near;
            terrain.terrainShader.Vars()["PerFrameCB"]["fog_far_log_F"] = atmosphere.getFar().m_logEnd;
            terrain.terrainShader.Vars()["PerFrameCB"]["fog_far_one_over_k"] = atmosphere.getFar().m_oneOverK;
            //terrain.terrainShader.Vars()["PerFrameCB"]["fog_near_Start"] = gis_overlay.strenght;
            //terrain.terrainShader.Vars()["PerFrameCB"]["fog_near_log_F"] = gis_overlay.strenght;
            //terrain.terrainShader.Vars()["PerFrameCB"]["fog_near_one_over_k"] = gis_overlay.strenght;

            terrain.terrainSpiteShader.Vars()["gConstantBuffer"]["screenSize"] = screenSize;
            terrain.terrainSpiteShader.Vars()["gConstantBuffer"]["fog_far_Start"] = atmosphere.getFar().m_params._near;
            terrain.terrainSpiteShader.Vars()["gConstantBuffer"]["fog_far_log_F"] = atmosphere.getFar().m_logEnd;
            terrain.terrainSpiteShader.Vars()["gConstantBuffer"]["fog_far_one_over_k"] = atmosphere.getFar().m_oneOverK;

            terrain.rappersvilleShader.Vars()["PerFrameCB"]["screenSize"] = screenSize;
            terrain.rappersvilleShader.Vars()["PerFrameCB"]["fog_far_Start"] = atmosphere.getFar().m_params._near;
            terrain.rappersvilleShader.Vars()["PerFrameCB"]["fog_far_log_F"] = atmosphere.getFar().m_logEnd;
            terrain.rappersvilleShader.Vars()["PerFrameCB"]["fog_far_one_over_k"] = atmosphere.getFar().m_oneOverK;

            terrain.gliderwingShader.Vars()["PerFrameCB"]["screenSize"] = screenSize;
            terrain.gliderwingShader.Vars()["PerFrameCB"]["fog_far_Start"] = atmosphere.getFar().m_params._near;
            terrain.gliderwingShader.Vars()["PerFrameCB"]["fog_far_log_F"] = atmosphere.getFar().m_logEnd;
            terrain.gliderwingShader.Vars()["PerFrameCB"]["fog_far_one_over_k"] = atmosphere.getFar().m_oneOverK;

            terrain.setCamera(CameraType_Main_Center, toGLM(camera->getViewMatrix()), toGLM(camera->getProjMatrix()), camera->getPosition(), true, 1920);
        }


        //if (terrain.terrainMode != _terrainMode::vegetation)
        {
            atmosphere.setSmokeTime(terrain.cfd.clipmap.lodOffsets, terrain.cfd.clipmap.lodScales);
            atmosphere.setSMOKE(terrain.cfd.sliceVolumeTexture);

            atmosphere.setSunDirection(global_sun_direction);
            atmosphere.getFar().setCamera(camera);
            atmosphere.computeSunInAtmosphere(_renderContext);
            atmosphere.computeVolumetric(_renderContext);
        }

        terrain.update(_renderContext);
    }

}



void Earthworks_4::onFrameRender(RenderContext* _renderContext, const Fbo::SharedPtr& pTargetFbo)
{
    gpDevice->toggleVSync(refresh.vsync);
    onFrameUpdate(_renderContext);


    if (!terrain.fullResetDoNotRender)
    {
        FALCOR_PROFILE("onFrameRender");

        // clear
        {
            graphicsState->setFbo(pTargetFbo);
            const float4 clearColor(0.0f, 0.f, 0.f, 1);
            _renderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
            _renderContext->clearFbo(hdrFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
        }

        {
            // blit the sky
            //glm::vec4 srcRect = glm::vec4(0, 0, atmosphere.getFar().m_params.m_x, atmosphere.getFar().m_params.m_y);
            //glm::vec4 dstRect = glm::vec4(0, 0, screenSize.x, screenSize.y);
            //_renderContext->blit(atmosphere.getFar().inscatter_sky->getSRV(0, 1, 0, 1), hdrFbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Linear);
        }


        terrain.onFrameRender(_renderContext, hdrFbo, camera, graphicsState, viewport3d, hdrHalfCopy);

        {
            FALCOR_PROFILE("tonemapper");
            tonemapper.Vars()->setTexture("hdr", hdrFbo->getColorTexture(0));
            tonemapper.Vars()->setTexture("cube", colorCube);
            tonemapper.Vars()->setSampler("linearSampler", terrain.sampler_Clamp);
            tonemapper.State()->setFbo(pTargetFbo);
            tonemapper.State()->setRasterizerState(graphicsState->getRasterizerState());
            tonemapper.drawInstanced(_renderContext, 3, 1);
        }

        onRenderOverlay(_renderContext, pTargetFbo);

        _renderContext->blit(hdrFbo->getColorTexture(0)->getSRV(0, 1, 0, 1), hdrHalfCopy->getRTV());

    }
    /*
    if (changed) slowTimer = 10;
    slowTimer--;
    if (refresh.minimal && (slowTimer < 0))
    {
        Sleep(30);      // aim for 15fps in this mode
    }*/
    static uint cnt = 0;

    /*
    if (terrain.cfd.recordingCFD)
    {
        pTargetFbo->getColorTexture(0)->captureToFile(0, 0, "E:/record/frame__" + std::to_string(cnt) + ".png");
        cnt++;
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
        uint scale = __min(8, __max(1, 1024 / h));
        glm::vec4 srcRect = glm::vec4(0, 0, w, h);
        glm::vec4 dstRect = glm::vec4(1000, 30, 1000, 30) + glm::vec4(0, 0, w * scale, h * scale);
        _renderContext->blit(tex->getSRV(0, 1, 0, 1), pTargetFbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Point);
        terrafectorEditorMaterial::static_materials.dispTexIndex = -1;
        _plantMaterial::static_materials_veg.dispTexIndex = -1;
    }
}


void Earthworks_4::onShutdown()
{
    fprintf(logFile, "Earthworks_4::onShutdown()\n");
    fflush(logFile);

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

    if (_keyEvent.type == KeyboardEvent::Type::KeyPressed)
    {
        /*
        if (_keyEvent.key == Input::Key::G)
        {
            terrain.cfd.recordingCFD = true;
            terrain.cfd.cfd_play_k = 0;
        }*/
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



bool Earthworks_4::onMouseEvent(const MouseEvent& _mouseEvent)
{
    slowTimer = 10;
    terrain.onMouseEvent(_mouseEvent, screenSize, screenMouseScale, screenMouseOffset, camera);
    return false;
}



void Earthworks_4::onResizeSwapChain(uint32_t _width, uint32_t _height)
{
    auto monitorDescs = MonitorInfo::getMonitorDescs();
    //screenSize = monitorDescs[0].resolution;   // THIS is used o my dual scren setup - BUT flipping my screns at some stage by umpluggin one broke this, so it has to be fixd first
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
    //clearTexture

    fprintf(logFile, "Earthworks_4::onResizeSwapChain()  %d, %d\n", _width, _height);
    fflush(logFile);
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
//int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    logFile = fopen("log.txt", "w");
    fprintf(logFile, "main()\n");
    fflush(logFile);


    bool allScreens = false;

    for (int i = 0; i < argc; i++)
    {
        if (std::string(argv[i]).find("-allscreens") != std::string::npos) allScreens = true;
    }


    fprintf(logFile, "SampleConfig config;\n");
    fflush(logFile);



    SampleConfig config;
    config.windowDesc.title = "Earthworks 4";
    config.windowDesc.resizableWindow = false;
    config.windowDesc.mode = Window::WindowMode::Fullscreen;
    if (allScreens) {
        //config.windowDesc.mode = Window::WindowMode::AllScreens;
    }
    config.windowDesc.width = 2560;
    config.windowDesc.height = 1140;
    config.windowDesc.monitor = 1;

    // HDR
    // config.windowDesc.monitor = 1;
    //config.deviceDesc.colorFormat = ResourceFormat::RGB10A2Unorm;

    fprintf(logFile, "std::make_unique\n");
    fflush(logFile);

    Earthworks_4::UniquePtr pRenderer = std::make_unique<Earthworks_4>();

    fprintf(logFile, "Sample::run(config, pRenderer);\n");
    fflush(logFile);

    Sample::run(config, pRenderer);

    fclose(logFile);
    return 0;
}
