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

#include"harupdf/include/hpdf.h"

#define archive_float2(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y));}
#define archive_float3(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z));}
#define archive_float4(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z)); archive(CEREAL_NVP(v.w));}

using namespace Falcor;


#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}




enum _pose { pose_stand, pose_kneel, pose_sit, pose_prone };
enum _action { action_static, action_popup, action_move, action_adjust };    // net staic vir eers


class _screenLayout
{
    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(screenWidth));
    }
};
CEREAL_CLASS_VERSION(_screenLayout, 100);


class _setup
{
public:
    float2 laneCenter(int _lane, int _pose)
    {
        float2 ret;
        ret.x = (screen_pixelsX / numLanes / 2) + (screen_pixelsX / numLanes * _lane);
        ret.y = eyeHeights[_pose] - 40;
        return ret;
    }
    float meterToPixels(float _dst) {
        return screen_pixelsX / screenWidth * shootingDistance / _dst;
    }
    float screenWidth = 3.0f;
    float screen_pixelsX = 1000;
    float screen_pixelsY = 1000;
    float pixelsPerMeter = 800.f;
    float shootingDistance = 10.f;  // fixme move to somethign that loads or saves
    float eyeHeights[4] = { 500, 800, 900, 1200 };
    int numLanes = 5;
    std::string dataFolder;

    int num3DScreens = 2;
    uint XOffset3D;
    uint cameraSerial[2] = {13050978, 16294735};

    bool hasInstructorScreen = true;
    uint instructorScreenIdx = 0;
    uint2 instructorOffet = uint2(0, 0);
    uint2 instructorSize = uint2(0, 0);

    bool requestClose = false;
    void renderGui(Gui* _gui, float _screenX, bool _intructor = false);
    void load();
    void save();

    int zigbeePacketVersion = 1;	// new type packets
    int zigbeeCOM = 0;


    float pgGain = 0;
    float pgGamma = 0;
    int dot_min = 5;
    int dot_max = 200;
    int threshold = 20;
    int m_PG_dot_position = 1;

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

        _archive(CEREAL_NVP(zigbeePacketVersion));
        _archive(CEREAL_NVP(zigbeeCOM));

        if (_version > 100)
        {
            _archive(CEREAL_NVP(num3DScreens));
            _archive(CEREAL_NVP(cameraSerial[0]));
            _archive(CEREAL_NVP(cameraSerial[1]));

            _archive(CEREAL_NVP(hasInstructorScreen));
            _archive(CEREAL_NVP(instructorScreenIdx));
            _archive(CEREAL_NVP(instructorOffet.x));
            _archive(CEREAL_NVP(instructorOffet.y));
            _archive(CEREAL_NVP(instructorSize.x));
            _archive(CEREAL_NVP(instructorSize.y));

            _archive(CEREAL_NVP(pgGain));
            _archive(CEREAL_NVP(pgGamma));
            _archive(CEREAL_NVP(dot_min));
            _archive(CEREAL_NVP(dot_max));
            _archive(CEREAL_NVP(threshold));
            _archive(CEREAL_NVP(m_PG_dot_position));
            
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

    bool hasChanged = false;

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


class laneAirEnable
{
public:
    void load();
    void save();
    void toggle(_ammunition _ammo, int _lane)
    {
        air[_ammo][_lane] = !air[_ammo][_lane];
    };
    bool inUse(_ammunition _ammo, int _lane)
    {
        return air[_ammo][_lane];
    };

    std::array<std::array<bool, 15>, 3> air;       // 15 lanes shpuld be enough for the future, question is ammo, and how this breaks serialize

private:


    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(air);
    }
};
CEREAL_CLASS_VERSION(laneAirEnable, 100);


class _target
{
public:
    void renderGui(Gui* _gui, Gui::Window& _window);
    void loadimage(std::string _root);
    void loadimageDialog();
    void loadscoreimage(std::string _root);
    void loadscoreimageDialog();
    void load(std::filesystem::path _path);
    void save(std::filesystem::path _path);
    std::string title = "";
    std::string description;

    int2 size_mm = int2(500, 500);
    float2 size;
    Texture::SharedPtr	        image = nullptr;
    Texture::SharedPtr	        score = nullptr;
    std::string texturePath;
    std::string scorePath;
    // scoring image, maybe just raw
    int maxScore;       // to auto calcl exercise score from rounds
    int scoreWidth = 0;
    int scoreHeight = 0;
    char* scoreData;
    bool dropWhenHit = false;

    std::string fullPath;


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

        size.x = (float)size_mm.x * 0.001f;
        size.y = (float)size_mm.y * 0.001f;
        loadimage(Kolskoot::setup.dataFolder + "/targets/");
        loadscoreimage(Kolskoot::setup.dataFolder + "/targets/");

        if (_version > 100)
        {
            _archive(CEREAL_NVP(dropWhenHit));
            
        }
    }
};
CEREAL_CLASS_VERSION(_target, 101);


class targetAction
{
public:
    void renderGui(Gui* _gui);
    void renderIntroGui(Gui* _gui, int rounds);

    _action     action;
    bool        dropWhenHit = false;
    float       startTime = 5.f;
    float       upTime = 5.f;
    float       downTime = 2.f;
    int         repeats = 1;
    float       speed = 1.0f;       // move speed
    bool        playWhistle = false;

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
        if (_version > 100)
        {
            _archive(CEREAL_NVP(playWhistle));
        }
    }
};
CEREAL_CLASS_VERSION(targetAction, 101);


/*  This adds offset information to space them around
* 
*/
class _displayTarget  
{
    public:
    float3 offset = float3(0, 0, 0);        // offset in meters
    _target t;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive(CEREAL_NVP(t.title));
        archive_float3(offset);

        for (auto& T : Kolskoot::targetList)
        {
            if (T.title == t.title)
            {
                t = T;
            }
        }
    }
};
CEREAL_CLASS_VERSION(_displayTarget, 100);


class _targetList
{
public:
    void load();
    void save(char * filename);
    std::string filename;
    std::vector<_displayTarget> targets;

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(filename));
        _archive(CEREAL_NVP(targets));
    }
};
CEREAL_CLASS_VERSION(_targetList, 100);


class exercise
{
public:
    void renderGui(Gui* _gui, Gui::Window& _window);
    void renderIntroGui(Gui* _gui, Gui::Window& _window, ImVec2 _offset, ImVec2 _size);
    bool renderTargetPopup(Gui* _gui);

    std::string title;
    std::string description;
    int maxScore;
    bool isScoring = true;
    float targetDistance = 10.f;

    int numRounds = 2;
    _pose           pose;
    _target          target;     // hy lyk gelukkig hiermaa, selfde naam, ek is nie 100% seker nie
    targetAction    action;

    _targetList     targets;


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

        if (_version > 100)
        {
            _archive(CEREAL_NVP(action));
        }

        if (_version > 101)
        {
            _archive(CEREAL_NVP(targets.filename));
            targets.load();
        }

        for (auto& T : Kolskoot::targetList)
        {
            if (T.title == target.title)
            {
                target = T;

                // temporary fill into target List see if we can build them all
                targets.filename = T.title;
                targets.targets.emplace_back();
                targets.targets.back().offset = float3(0, 0, 0);
                targets.targets.back().t = target;
                targets.save("F:/test.targetlist.xml");
            }
        }
    }
};
CEREAL_CLASS_VERSION(exercise, 102);


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
    _target              target;
    std::vector<_shots> shots;
    int                 score = 0;
};


class _scoring
{
public:
    void create(size_t numLlanes, size_t numEx);

    std::vector<std::vector<_scoringExercise>> lane_exercise;
};


struct menuItem
{
    std::string     fullPath;
    std::string     path;
    std::string     name;
};

class menu
{
    std::vector<menuItem> items;
    std::string clean(const std::string _s);

public:
    void load();
    void renderGui(Gui* _gui, Gui::Window& _window);

};

enum _liveStage { live_intro, live_live, live_scores };
class quickRange        // rename
{
public:
    void renderGui(Gui* _gui, float2 _screenSize, Gui::Window& _window);
    void renderTarget(Gui* _gui, Gui::Window& _window, Texture::SharedPtr _bulletHole, int _lane);
    float renderScope(Gui* _gui, Gui::Window& _window, Texture::SharedPtr _bulletHole, int _lane, float _size, float _y, bool _inst = false);
    void renderLive(Gui* _gui, Gui::Window& _window, Texture::SharedPtr _bulletHole);
    void renderLiveInstructor(Gui* _gui, Gui::Window& _window, Texture::SharedPtr _bulletHole);
    void adjustBoresight(int _lane);
    void renderLiveMenubar(Gui* _gui);
    void load(std::filesystem::path _root);
    void loadPath(std::filesystem::path _root);
    void save();
    void play();
    void mouseShot(float x, float y, _setup setup);
    bool liveNext();
    int getRoundsLeft(int _lane);
    void updateLive(float _dT);
    void print();

    void clear() {
        exercises.clear();
        description.clear();
        title.clear();
        maxScore = 0;
        currentExercise = 0;
    }

    std::string title = "please rename";        // DO NOT EDIT from filename
    std::string description;
    std::vector<exercise> exercises;
    int maxScore;

    // playback data
    int currentExercise;
    _liveStage currentStage = live_intro;

    _scoring score;
    float bulletDrop = 0;

    float       actionCounter = 0.f;
    int         actionRepeats = 0;
    int         actionPhase = 0;


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
    void build(std::array<glm::vec4, 45>  dots, float2 screenSize, float offsetX);




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
    void renderBranding(Gui* _gui, Gui::Window& _window, uint2 _size);
    void renderTargets(Gui* _gui, Gui::Window& _window, uint2 _size);
    void renderExerciseBuilder(Gui* _gui, Gui::Window& _window, uint2 _size);
    void renderCamera(Gui* _gui, Gui::Window& _window, uint2 _size);
    void renderCamera_main(Gui* _gui, Gui::Window& _window, uint2 _size, uint _screen);
    void renderCamera_calibrate(Gui* _gui, uint2 _size, uint _screen);
    void renderLive(Gui* _gui, Gui::Window& _window, uint2 _size);
    void renderLiveInstructor(Gui* _gui, Gui::Window& _window, uint2 _size);
    void onGuiMenubar(Gui* _gui);
    //void menuButton();
    //void nextButton();
    void guiStyle();
    void playQR(std::string file);

    

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

    //std::string mModelString;

    //PointGrey
    PointGrey_Camera* pointGreyCamera;
    videoToScreen   screenMap[2];          // MULTIPEL for 10 lane

    int calibrationCounter;
    
    Texture::SharedPtr	        pointGreyBuffer[2] = { nullptr, nullptr };
    Texture::SharedPtr	        pointGreyDiffBuffer[2] = { nullptr, nullptr };
    /*float pgGain = 0;
    float pgGamma = 0;
    int dot_min = 5;
    int dot_max = 40;
    int threshold = 20;
    int m_PG_dot_position = 1; */
    std::array<glm::vec4, 45> calibrationDots;

    GraphicsState::Viewport     viewport3d;
    float2                      screenSize;
    float2                      screenMouseScale;
    float2                      screenMouseOffset;
    Fbo::SharedPtr		        hdrFbo;
    Texture::SharedPtr	        hdrHalfCopy;
    GraphicsState::SharedPtr    graphicsState;
    Falcor::Camera::SharedPtr	camera;


    Texture::SharedPtr	        kolskootLogo;


    menu        guiMenu;

    _guimode guiMode = gui_menu;
    int modeCalibrate = 0;
    int cameraToCalibratate = 0;

    // quick range
    
    _target      targetBuilder;

    


    unsigned char zigbeeID_h;
    unsigned char zigbeeID_l;

    void  zigbeePaperMove();
    void  zigbeePaperStop();
    void  zigbeeRounds(unsigned int lane, int R, bool bStop = false);
    void  zigbeeFire(unsigned int lane);
    void  zigbeeClear();

    // live playback
    void updateLive();
    void mouseShot();
    void pointGreyShot();
    bool airOffAfterLive = true;

    HPDF_Doc pdf;
public:
    static std::vector<_target> targetList;
    static _setup setup;
    static ballisticsSetup ballistics;
    static laneAirEnable airToggle;
    static CCommunication ZIGBEE;		// vir AIR beheer
    static quickRange  QR;

    static Texture::SharedPtr	        bulletHole;
    static Texture::SharedPtr	        ammoType[3];

    // should maybe reqwrite as singleton class
};
