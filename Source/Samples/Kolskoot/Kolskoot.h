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
#include "SerialComms.h"

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


#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}




enum _pose {pose_stand, pose_kneel, pose_sit, pose_prone};
enum _action {action_static, action_popup, action_move, action_adjust};    // net staic vir eers


class _setup
{
public:
    float screenWidth = 3.0f;
    float screen_pixelsX = 1000;
    float pixelsPerMeter = 800.f;
    float shootingDistance = 10.f;  // fixme move to somethign that loads or saves
    float eyeHeights[4] = { 500, 800, 900, 1200 };
    int numLanes = 5;
    std::string dataFolder;

    bool requestClose = false;
    void renderGui(Gui* _gui, float _screenX);
    void load();
    void save();

    int zigbeePacketVersion = 1;	// new type packets
    int zigbeeCOM = 0;

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        
        _archive(CEREAL_NVP(screenWidth));
        _archive(CEREAL_NVP(screen_pixelsX));
        _archive(CEREAL_NVP(pixelsPerMeter));
        _archive(CEREAL_NVP(shootingDistance));
        _archive(CEREAL_NVP(eyeHeights));
        _archive(CEREAL_NVP(numLanes));
        _archive(CEREAL_NVP(dataFolder));

        if (_version > 100)
        {
            _archive(CEREAL_NVP(zigbeePacketVersion));
            _archive(CEREAL_NVP(zigbeeCOM));
        }
        
    }
};
CEREAL_CLASS_VERSION(_setup, 101);


enum _ammunition { ammo_9mm, ammo_556, ammo_762 };
class ballisticsSetup
{
public:
    void load();
    void save();
    float bulletDrop(float distance, float apex, float apexHeight, float drop);
    float bulletDrop(float distance);        // just call once at start of exercise and apply consistent, dont simulate

    float2 offset(int _lane);
    float2 adjustOffset(int _lane, float2 error);
    void clearOffsets(int _lane);
    void renderGuiAmmo(Gui* _gui);

    _ammunition currentAmmo;

private:
    
    std::array<std::array<float2, 15>, 3> screen_offsets;       // 15 lanes shpuld be enough for the future, question is ammo, and how this breaks serialize


    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive(CEREAL_NVP((int)currentAmmo));
        //_archive(CEREAL_NVP(screen_offsets));
        //archive_float3(videoEdge[0]);
        for (int a = 0; a < 3; a++)
        {
            for (int l = 0; l < 15; l++)
            {
                archive_float2(screen_offsets[a][l]);
            }
        }
    }
};
CEREAL_CLASS_VERSION(ballisticsSetup, 100);


class target
{
public:
    void renderGui(Gui* _gui, Gui::Window& _window);
    void loadimage();
    void loadimageDialog();
    void loadscoreimage();
    void loadscoreimageDialog();
    void load(std::filesystem::path _path);
    void save(std::filesystem::path _path);
    std::string title = "";
    std::string description;

    int2 size_mm = int2(500, 500);
    Texture::SharedPtr	        image = nullptr;
    Texture::SharedPtr	        score = nullptr;
    std::string texturePath;
    std::string scorePath;
    // scoring image, maybe just raw
    int maxScore;       // to auto calcl exercise score from rounds
    int scoreWidth = 0;
    int scoreHeight = 0;
    char *scoreData;
    

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(title));
        _archive(CEREAL_NVP(description));
        _archive(CEREAL_NVP(texturePath));
        _archive(CEREAL_NVP(scorePath));
        _archive(CEREAL_NVP(size_mm.x));
        _archive(CEREAL_NVP(size_mm.y));

        _archive(CEREAL_NVP(maxScore));
        _archive(CEREAL_NVP(scoreWidth));
        _archive(CEREAL_NVP(scoreHeight));

        loadimage();
        loadscoreimage();
    }
};
CEREAL_CLASS_VERSION(target, 100);


class targetAction
{
public:
    void renderGui(Gui* _gui);

    _action     action;
    bool        dropWhenHit = false;
    float       startTime = 5.f;
    float       upTime = 5.f;
    float       downTime = 2.f;
    int         repeats = 1;
    float       speed = 1.0f;       // move speed

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(action));
        _archive(CEREAL_NVP(dropWhenHit));
        _archive(CEREAL_NVP(startTime));
        _archive(CEREAL_NVP(upTime));
        _archive(CEREAL_NVP(downTime));
        _archive(CEREAL_NVP(repeats));
        _archive(CEREAL_NVP(speed));
    }
};
CEREAL_CLASS_VERSION(targetAction, 100);






class exercise
{
public:
    void renderGui(Gui* _gui, Gui::Window& _window);
    bool renderTargetPopup(Gui* _gui);
    

    std::string title;
    std::string description;
    int maxScore;
    bool isScoring = true;
    float targetDistance = 10.f;

    // target
    // target action
    // timing
    
    int numRounds = 2;
    _pose           pose;
    target          target;     // hy lyk gelukkig hiermaa, selfde naam, ek is nie 100% seker nie
    targetAction    action;


    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(title));
        _archive(CEREAL_NVP(description));
        _archive(CEREAL_NVP(maxScore));
        _archive(CEREAL_NVP(numRounds));
        _archive(CEREAL_NVP(pose));
        _archive(CEREAL_NVP(isScoring));
        _archive(CEREAL_NVP(targetDistance));
        _archive(CEREAL_NVP(target.title));

        for (auto& T : Kolskoot::targetList)
        {
            if (T.title == target.title)
            {
                target = T;
            }
        }
    }
};
CEREAL_CLASS_VERSION(exercise, 100);


class _shots
{
public:
    float2  position;
    float2  position_relative;
    int     score;
};


class _scoringExercise
{
public:
    target              target;
    std::vector<_shots> shots;
    int                 score = 0;
};


class _scoring
{
public:
    void create(size_t numLlanes, size_t numEx);

    std::vector<std::vector<_scoringExercise>> lane_exercise;
};

enum _liveStage { live_intro, live_live, live_scores };
class quickRange        // rename
{
public:
    void renderGui(Gui* _gui, float2 _screenSize, Gui::Window& _window);
    void renderLive(Gui* _gui, float2 _screenSize, Gui::Window& _window, _setup setup, Texture::SharedPtr _bulletHole);
    void renderLiveMenubar(Gui* _gui);
    void load();
    void save();
    void mouseShot(float x, float y, _setup setup);
    bool liveNext();

    std::string title = "please rename";
    std::string description;
    std::vector<exercise> exercises;
    int maxScore;

    // playback data
    int currentExercise;
    _liveStage currentStage = live_intro;

    _scoring score;
    float bulletDrop = 0;
    

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(title));
        _archive(CEREAL_NVP(description));
        _archive(CEREAL_NVP(maxScore));
        _archive(CEREAL_NVP(exercises));        
    }
};
CEREAL_CLASS_VERSION(quickRange, 100);









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


enum _guimode { gui_menu, gui_camera, gui_screen, gui_targets, gui_exercises, gui_live };

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

    _guimode guiMode = gui_menu;
    int modeCalibrate = 0;
    
    // quick range
    quickRange  QR;
    target      targetBuilder;

    Texture::SharedPtr	        bulletHole = nullptr;

    CCommunication ZIGBEE;		// vir AIR beheer
    unsigned char zigbeeID_h;
    unsigned char zigbeeID_l;
    
    void  zigbeePaperMove();
    void  zigbeePaperStop();
    void  zigbeeRounds(unsigned int lane, int R, bool bStop = false);
    void  zigbeeFire(unsigned int lane);

public:
    static std::vector<target> targetList;
    static _setup setupInfo;
    static ballisticsSetup ballistics;
};
