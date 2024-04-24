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


extern unsigned int phaseX;
extern unsigned int phaseY;
extern float phaseData[];

void FogVolume::onLoad(FogVolumeParameters params)
{
    m_params = params;
    inscatter = Texture::create3D(params.m_x, params.m_y, params.m_z, params.format, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess | Falcor::Resource::BindFlags::ShaderResource);
    inscatter_cloudbase = Texture::create2D(params.m_x, params.m_y, Falcor::ResourceFormat::RGBA16Float, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess | Falcor::Resource::BindFlags::ShaderResource);
    inscatter_sky = Texture::create2D(params.m_x, params.m_y, Falcor::ResourceFormat::RGBA16Float, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess | Falcor::Resource::BindFlags::ShaderResource);

    outscatter = Texture::create3D(params.m_x, params.m_y, params.m_z, params.format, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess | Falcor::Resource::BindFlags::ShaderResource);
    outscatter_cloudbase = Texture::create2D(params.m_x, params.m_y, Falcor::ResourceFormat::RGBA16Float, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess | Falcor::Resource::BindFlags::ShaderResource);
    outscatter_sky = Texture::create2D(params.m_x, params.m_y, Falcor::ResourceFormat::RGBA16Float, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess | Falcor::Resource::BindFlags::ShaderResource);
}

void FogVolume::update()
{
    m_logEnd = ((float)m_params.m_z - 1.0f) / ((float)m_params.m_z) / (log(m_params._far / m_params._near));
    m_oneOverK = 1.0f / (float)m_params.m_z;

    float base = m_params._far / m_params._near;
    m_SliceStep = pow(base, 1.0f / (m_params.m_z - 1.0f));
    m_SliceZero = m_params._near / m_SliceStep;
}

void FogVolume::setCamera(Camera::SharedPtr _camera)
{
    glm::mat4 W = toGLM(_camera->getViewMatrix().getTranspose());
    
    float3 dir = W[2] * -1.f;
    float3 up = W[1];
    float3 right = W[0];

    //note these functions are overridden in vr and triple... (see CameraStereo and CameraTriple)
    float fovY = 2.0f * std::atan(0.5f * _camera->getFrameHeight() / _camera->getFocalLength());
    float fovX = fovY * _camera->getAspectRatio();
    float tan_fovH = tan(fovX * 0.5f);
    float tan_fovV = tan(fovY * 0.5f);
    float3 dX = right / (m_params.m_x / 2.0f) * tan_fovH;
    float3 dY = up / (m_params.m_y / 2.0f) * tan_fovV * (-1.0f); // inverted due to texture coordinates

    compute_Params.dx = dX;
    compute_Params.dy = dY;
    compute_Params.eye_direction =
        dir - dX * (m_params.m_x / 2.0f - 0.5f) - dY * (m_params.m_y / 2.0f - 0.5f); // move to the center of the top leftmost pixel  ??? should I use edges instead of centers
    compute_Params.eye_position = _camera->getPosition();
    compute_Params.numSlices = m_params.m_z;
    compute_Params.sliceZero = m_SliceZero;
    compute_Params.sliceMultiplier = m_SliceStep;
    compute_Params.sliceStep = m_SliceStep - 1.0f;

    
    
}





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

    {
        atmosphere.sunlightTexture = Texture::create2D(512, 256, Falcor::ResourceFormat::RGBA32Float, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess | Falcor::Resource::BindFlags::ShaderResource);

        atmosphere.compute_sunSlice.load("Samples/Earthworks_4/hlsl/compute_sunlightInAtmosphere.hlsl");
        atmosphere.compute_sunSlice.Vars()->setTexture("gResult", atmosphere.sunlightTexture);

        atmosphere.mainFar.onLoad(FogVolumeParameters{ 256, 128, 256, Falcor::ResourceFormat::R11G11B10Float, true, false, 50.f, 20000.f });
        atmosphere.mainNear.onLoad(FogVolumeParameters{ 256, 128, 256, Falcor::ResourceFormat::RGBA16Float, false, false, 1.f, 100.f });
        atmosphere.parabolicFar.onLoad(FogVolumeParameters{ 128, 128, 32, Falcor::ResourceFormat::R11G11B10Float, true, true, 100.f, 20000.f });

        atmosphere.phaseFunction = Texture::create2D(phaseX, phaseY, Falcor::ResourceFormat::R32Float, 1, 1, phaseData, Falcor::Resource::BindFlags::UnorderedAccess | Falcor::Resource::BindFlags::ShaderResource);

        atmosphere.compute_Atmosphere.load("Samples/Earthworks_4/hlsl/compute_volumeFogAtmosphericScatter.hlsl");

        atmosphere.compute_Atmosphere.Vars()->setTexture("SunInAtmosphere", atmosphere.sunlightTexture);
        atmosphere.compute_Atmosphere.Vars()->setTexture("hazePhaseFunction", atmosphere.phaseFunction);
        

        atmosphere.compute_Atmosphere.Vars()->setTexture("gInscatter", atmosphere.mainFar.inscatter);
        atmosphere.compute_Atmosphere.Vars()->setTexture("gInscatter_cloudBase", atmosphere.mainFar.inscatter_cloudbase);
        atmosphere.compute_Atmosphere.Vars()->setTexture("gInscatter_sky", atmosphere.mainFar.inscatter_sky);
        atmosphere.compute_Atmosphere.Vars()->setTexture("gOutscatter", atmosphere.mainFar.outscatter);
        atmosphere.compute_Atmosphere.Vars()->setTexture("gOutscatter_cloudBase", atmosphere.mainFar.outscatter_cloudbase);
        atmosphere.compute_Atmosphere.Vars()->setTexture("gOutscatter_sky", atmosphere.mainFar.outscatter_sky);

        terrain.terrainShader.Vars()->setTexture("gAtmosphereInscatter", atmosphere.mainFar.inscatter);
        terrain.terrainShader.Vars()->setTexture("gAtmosphereOutscatter", atmosphere.mainFar.outscatter);
        //terrain.terrainShader.Vars()->setTexture("gSmokeAndDustInscatter", compressed_Albedo_Array);
        //terrain.terrainShader.Vars()->setTexture("gSmokeAndDustOutscatter", compressed_Albedo_Array);
    }

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



void Earthworks_4::onFrameRender(RenderContext* _renderContext, const Fbo::SharedPtr& pTargetFbo)
{
    gpDevice->toggleVSync(refresh.vsync);

    {
        FALCOR_PROFILE("sunlight");
        atmosphere.compute_sunSlice.Vars()["FogCloudCommonParams"]["sun_direction"] = atmosphere.common.sun_direction;
        atmosphere.compute_sunSlice.Vars()["FogCloudCommonParams"]["cloudBase"] = atmosphere.common.cloudBase;
        atmosphere.compute_sunSlice.Vars()["FogCloudCommonParams"]["cloudThickness"] = atmosphere.common.cloudThickness;

        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["eye_direction"] = atmosphere.params.eye_direction;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["uv_offset"] = atmosphere.params.uv_offset;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["eye_position"] = atmosphere.params.eye_position;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["uv_scale"] = atmosphere.params.uv_scale;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["dx"] = atmosphere.params.dx;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["uv_pixSize"] = atmosphere.params.uv_pixSize;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["dy"] = atmosphere.params.dy;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["numSlices"] = atmosphere.params.numSlices;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["sliceZero"] = atmosphere.params.sliceZero;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["sliceMultiplier"] = atmosphere.params.sliceMultiplier;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["sliceStep"] = atmosphere.params.sliceStep;

        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["timer"] = atmosphere.params.timer;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["numFogLights"] = atmosphere.params.numFogLights;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["paddTime"] = atmosphere.params.paddTime;

        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["fogAltitude"] = atmosphere.params.fogAltitude;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["fogDensity"] = atmosphere.params.fogDensity;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["fogUVoffset"] = atmosphere.params.fogUVoffset;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["fogMip0Size"] = atmosphere.params.fogMip0Size;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["numFogVolumes"] = atmosphere.params.numFogVolumes;

        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["sunColourBeforeOzone"] = atmosphere.params.sunColourBeforeOzone;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["haze_Turbidity"] = atmosphere.params.haze_Turbidity;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["haze_Colour"] = atmosphere.params.haze_Colour;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["haze_AltitudeKm"] = atmosphere.params.haze_AltitudeKm;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["fog_Colour"] = atmosphere.params.fog_Colour;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["haze_BaseAltitudeKm"] = atmosphere.params.haze_BaseAltitudeKm;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["fog_AltitudeKm"] = atmosphere.params.fog_AltitudeKm;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["fog_BaseAltitudeKm"] = atmosphere.params.fog_BaseAltitudeKm;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["fog_Turbidity"] = atmosphere.params.fog_Turbidity;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["globalExposure"] = atmosphere.params.globalExposure;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["rain_Colour"] = atmosphere.params.rain_Colour;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["rainFade"] = atmosphere.params.rainFade;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["ozone_Density"] = atmosphere.params.ozone_Density;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["ozone_Colour"] = atmosphere.params.ozone_Colour;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["refraction"] = atmosphere.params.refraction;

        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["parabolicProjection"] = atmosphere.params.parabolicProjection;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["parabolicMapHalfSize"] = atmosphere.params.parabolicMapHalfSize;
        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["parabolicUpDown"] = atmosphere.params.parabolicUpDown;

        atmosphere.compute_sunSlice.Vars()["FogAtmosphericParams"]["tmp_IBL_scale"] = atmosphere.params.tmp_IBL_scale;

        atmosphere.compute_sunSlice.dispatch(_renderContext, 512 / 32, 256 / 32);
    }


    {
        FALCOR_PROFILE("volumeFog");
        atmosphere.mainFar.setCamera(camera);
        atmosphere.mainFar.update();

        atmosphere.compute_Atmosphere.Vars()["FogCloudCommonParams"]["sun_direction"] = atmosphere.common.sun_direction;
        atmosphere.compute_Atmosphere.Vars()["FogCloudCommonParams"]["cloudBase"] = atmosphere.common.cloudBase;
        atmosphere.compute_Atmosphere.Vars()["FogCloudCommonParams"]["cloudThickness"] = atmosphere.common.cloudThickness;

        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["eye_direction"] = atmosphere.mainFar.compute_Params.eye_direction;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["uv_offset"] = atmosphere.mainFar.compute_Params.uv_offset;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["eye_position"] = atmosphere.mainFar.compute_Params.eye_position;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["uv_scale"] = atmosphere.mainFar.compute_Params.uv_scale;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["dx"] = atmosphere.mainFar.compute_Params.dx;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["uv_pixSize"] = atmosphere.mainFar.compute_Params.uv_pixSize;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["dy"] = atmosphere.mainFar.compute_Params.dy;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["numSlices"] = atmosphere.mainFar.compute_Params.numSlices;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["sliceZero"] = atmosphere.mainFar.compute_Params.sliceZero;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["sliceMultiplier"] = atmosphere.mainFar.compute_Params.sliceMultiplier;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["sliceStep"] = atmosphere.mainFar.compute_Params.sliceStep;

        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["timer"] = atmosphere.mainFar.compute_Params.timer;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["numFogLights"] = atmosphere.mainFar.compute_Params.numFogLights;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["paddTime"] = atmosphere.mainFar.compute_Params.paddTime;

        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["fogAltitude"] = atmosphere.mainFar.compute_Params.fogAltitude;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["fogDensity"] = atmosphere.mainFar.compute_Params.fogDensity;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["fogUVoffset"] = atmosphere.mainFar.compute_Params.fogUVoffset;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["fogMip0Size"] = atmosphere.mainFar.compute_Params.fogMip0Size;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["numFogVolumes"] = atmosphere.mainFar.compute_Params.numFogVolumes;

        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["sunColourBeforeOzone"] = atmosphere.mainFar.compute_Params.sunColourBeforeOzone;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["haze_Turbidity"] = atmosphere.mainFar.compute_Params.haze_Turbidity;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["haze_Colour"] = atmosphere.mainFar.compute_Params.haze_Colour;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["haze_AltitudeKm"] = atmosphere.mainFar.compute_Params.haze_AltitudeKm;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["fog_Colour"] = atmosphere.mainFar.compute_Params.fog_Colour;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["haze_BaseAltitudeKm"] = atmosphere.mainFar.compute_Params.haze_BaseAltitudeKm;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["fog_AltitudeKm"] = atmosphere.mainFar.compute_Params.fog_AltitudeKm;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["fog_BaseAltitudeKm"] = atmosphere.mainFar.compute_Params.fog_BaseAltitudeKm;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["fog_Turbidity"] = atmosphere.mainFar.compute_Params.fog_Turbidity;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["globalExposure"] = atmosphere.mainFar.compute_Params.globalExposure;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["rain_Colour"] = atmosphere.mainFar.compute_Params.rain_Colour;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["rainFade"] = atmosphere.mainFar.compute_Params.rainFade;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["ozone_Density"] = atmosphere.mainFar.compute_Params.ozone_Density;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["ozone_Colour"] = atmosphere.mainFar.compute_Params.ozone_Colour;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["refraction"] = atmosphere.mainFar.compute_Params.refraction;

        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["parabolicProjection"] = atmosphere.mainFar.compute_Params.parabolicProjection;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["parabolicMapHalfSize"] = atmosphere.mainFar.compute_Params.parabolicMapHalfSize;
        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["parabolicUpDown"] = atmosphere.mainFar.compute_Params.parabolicUpDown;

        atmosphere.compute_Atmosphere.Vars()["FogAtmosphericParams"]["tmp_IBL_scale"] = atmosphere.mainFar.compute_Params.tmp_IBL_scale;

        atmosphere.compute_Atmosphere.Vars()->setSampler("linearSampler", terrain.sampler_Trilinear);
        atmosphere.compute_Atmosphere.Vars()->setSampler("clampSampler", terrain.sampler_Clamp);

        atmosphere.compute_Atmosphere.dispatch(_renderContext, atmosphere.mainFar.m_params.m_x / 8, atmosphere.mainFar.m_params.m_y / 8, 1);


        terrain.terrainShader.Vars()["LightsCB"]["sunDirection"] = atmosphere.common.sun_direction;
        terrain.terrainShader.Vars()["LightsCB"]["sunColour"] = float3(10, 10, 10);
        

        terrain.terrainShader.Vars()["PerFrameCB"]["screenSize"] = screenSize;
        terrain.terrainShader.Vars()["PerFrameCB"]["fog_far_Start"] = atmosphere.mainFar.m_params._near;
        terrain.terrainShader.Vars()["PerFrameCB"]["fog_far_log_F"] = atmosphere.mainFar.m_logEnd;
        terrain.terrainShader.Vars()["PerFrameCB"]["fog_far_one_over_k"] = atmosphere.mainFar.m_oneOverK;
        //terrain.terrainShader.Vars()["PerFrameCB"]["fog_near_Start"] = gis_overlay.strenght;
        //terrain.terrainShader.Vars()["PerFrameCB"]["fog_near_log_F"] = gis_overlay.strenght;
        //terrain.terrainShader.Vars()["PerFrameCB"]["fog_near_one_over_k"] = gis_overlay.strenght;
    }

    terrain.setCamera(CameraType_Main_Center, toGLM(camera->getViewMatrix()), toGLM(camera->getProjMatrix()), camera->getPosition(), true, 1920);
    bool changed = terrain.update(_renderContext);

    //const float4 clearColor(0.38f, 0.52f, 0.10f, 1);
    //const float4 clearColor(0.02f, 0.02f, 0.02f, 1);
    const float4 clearColor(0.0f, 0.f, 0.f, 1);
    _renderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    _renderContext->clearFbo(hdrFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);

    {
        glm::vec4 srcRect = glm::vec4(0, 0, 256, 128);
        glm::vec4 dstRect = glm::vec4(0, 0, screenSize.x, screenSize.y);
        _renderContext->blit(atmosphere.mainFar.inscatter_sky->getSRV(0, 1, 0, 1), hdrFbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Linear);
    }

    graphicsState->setFbo(pTargetFbo);

    terrain.onFrameRender(_renderContext, hdrFbo, camera, graphicsState, viewport3d, hdrHalfCopy);

    {
        FALCOR_PROFILE("tonemapper");
        tonemapper.Vars()->setTexture("hdr", hdrFbo->getColorTexture(0));
        tonemapper.State()->setFbo(pTargetFbo);
        tonemapper.State()->setRasterizerState(graphicsState->getRasterizerState());
        tonemapper.drawInstanced(_renderContext, 3, 1);
    }

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

    {
        glm::vec4 srcRect = glm::vec4(0, 0, 256, 128);
        glm::vec4 dstRect = glm::vec4(100, 100, 100+512, 100+256);
       // _renderContext->blit(atmosphere.sunlightTexture->getSRV(0, 1, 0, 1), pTargetFbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Point);
        static int slice = 0;
       //_renderContext->blit(atmosphere.mainFar.inscatter->getSRV(0, 1, 0, 1), pTargetFbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Point);
       slice++;
       if (slice >= 128) slice = 0;
    }


    

    {
        FALCOR_PROFILE("HDR half copy");
        _renderContext->blit( hdrFbo->getColorTexture(0)->getSRV(0, 1, 0, 1), hdrHalfCopy->getRTV() );
    }
    

    if (changed) slowTimer = 10;
    slowTimer--;
    if (refresh.minimal && (slowTimer < 0))
    {
        Sleep(30);      // aim for 15fps in this mode
    }
    else
    {
        //Sleep(5);
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
    graphicsState->setFbo(hdrFbo);

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
