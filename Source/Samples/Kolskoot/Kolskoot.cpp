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
#include <FreeImage.h>



#pragma optimize("", off)


std::vector<target> Kolskoot::targetList;
_setup Kolskoot::setupInfo;
ballisticsSetup Kolskoot::ballistics;
CCommunication Kolskoot::ZIGBEE;		// vir AIR beheer
float2 mouseScreen;
static bool targetPopupOpen = false;
bool requestLive = false;

#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}


void _setup::renderGui(Gui* _gui, float _screenX)
{

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiStyle styleOld = style;
    /*
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 0, 0, 1));

    //ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 0, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 3);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0.03f, 0.03f, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3);
    */
    ImGui::PushFont(_gui->getFont("roboto_32"));
    {

        if (ImGui::Button("...")) {
            std::filesystem::path _path;
            if (chooseFolderDialog(_path))
            {
                dataFolder = _path.string();
            }
        }

        ImGui::SameLine(0, 10);

        ImGui::SetNextItemWidth(600);
        char T[256];
        sprintf(T, "%s", dataFolder.c_str());
        if (ImGui::InputText("root folder", T, 256)) {
            dataFolder = T;
        }




        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("pixels per meter", &screenWidth, 0.01f, 1.0f, 10.0f, "%3.2f m");
        TOOLTIP("horizontal width of the screen in meters");
        screen_pixelsX = _screenX;
        pixelsPerMeter = screen_pixelsX / screenWidth;
        ImGui::PushFont(_gui->getFont("roboto_20"));
        ImGui::Text("     %3.2f screen of %d pixels = %3.2f pixels per meter", screenWidth, (int)screen_pixelsX, pixelsPerMeter);
        ImGui::PopFont();

        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("distance", &shootingDistance, 0.1f, 1, 100, "%3.2f m");
        TOOLTIP("Measure the distance from the screen to the shooting line");

        ImGui::SetNextItemWidth(200);
        ImGui::DragInt("lanes", &numLanes, 1, 1, 15);

        ImGui::SetNextItemWidth(200);
        ImGui::InputInt("zigbeeCom", &zigbeeCOM, 1);
        
        
        

        ImGui::SetCursorPos(ImVec2(800, 50));
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("stand", &eyeHeights[0], 1, 100, 2000);

        ImGui::SetCursorPos(ImVec2(800, 100));
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("kneel", &eyeHeights[1], 1, 100, 2000);

        ImGui::SetCursorPos(ImVec2(800, 150));
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("sit", &eyeHeights[2], 1, 100, 2000);

        ImGui::SetCursorPos(ImVec2(800, 200));
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("prone##1", &eyeHeights[3], 1, 100, 2000);




        ImGui::SetCursorPos(ImVec2(screen_pixelsX - 200, 200));
        ImGui::SetNextItemWidth(200);
        if (ImGui::Selectable("save & close")) { save(); requestClose = true; }



        //ImGui::NewLine();
        ImGui::SetCursorPos(ImVec2(0, 350));
        ImGui::Columns(numLanes);
        for (int i = 0; i < numLanes; i++)
        {
            ImGui::Text("lane %d", i + 1);
            ImGui::NextColumn();
        }


        ImGui::SetCursorPosX(0);
        ImGui::SetCursorPosY(eyeHeights[0]);
        ImGui::SetNextItemWidth(200);
        ImGui::Selectable("  standing");
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
        {
            eyeHeights[0] = ImGui::GetMousePos().y - 15 - 40;
        }
        ImGui::Separator();

        ImGui::SetCursorPosY(eyeHeights[1]);
        ImGui::Selectable("  kneeling");
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
        {
            eyeHeights[1] = ImGui::GetMousePos().y - 15 - 40;
        }
        ImGui::Separator();

        ImGui::SetCursorPosY(eyeHeights[2]);
        ImGui::Selectable("  sitting");
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
        {
            eyeHeights[2] = ImGui::GetMousePos().y - 15 - 40;
        }
        ImGui::Separator();

        ImGui::SetCursorPosY(eyeHeights[3]);
        ImGui::Selectable("  prone");
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
        {
            eyeHeights[3] = ImGui::GetMousePos().y - 15 - 40;
        }
        ImGui::Separator();

        for (int i = 1; i < 4; i++)
        {
            if (eyeHeights[i] < eyeHeights[i - 1] + 30)
            {
                eyeHeights[i] = eyeHeights[i - 1] + 30;
            }
        }

        for (int i = 0; i < 3; i++)
        {
            if (eyeHeights[i] > eyeHeights[i + 1] - 30)
            {
                eyeHeights[i] = eyeHeights[i + 1] - 30;
            }
        }



    }
    ImGui::PopFont();
}
void _setup::load()
{
    if (std::filesystem::exists("screenInfo.json"))
    {
        std::ifstream is("screenInfo.json");
        cereal::JSONInputArchive archive(is);
        serialize(archive, 100);
    }
}
void _setup::save()
{
    std::ofstream os("screenInfo.json");
    cereal::JSONOutputArchive archive(os);
    serialize(archive, 100);
}








void ballisticsSetup::load()
{
    if (std::filesystem::exists("ballistics_offsets.json"))
    {
        std::ifstream is("ballistics_offsets.json");
        cereal::JSONInputArchive archive(is);
        serialize(archive, 100);
    }
}
void ballisticsSetup::save()
{
    std::ofstream os("ballistics_offsets.json");
    cereal::JSONOutputArchive archive(os);
    serialize(archive, 100);
}


float2 ballisticsSetup::offset(int _lane)
{
    return screen_offsets[currentAmmo][_lane];
}


float2 ballisticsSetup::adjustOffset(int _lane, float2 error)
{
    screen_offsets[currentAmmo][_lane] += error;
    return screen_offsets[currentAmmo][_lane];
}

void ballisticsSetup::clearOffsets(int _lane)
{
    screen_offsets[currentAmmo][_lane] += float2(0, 0);
}

//https://ballisticscalculator.winchester.com/#!/result
// for now just pow(x, 2) but do betetr to accoutn for slowing down
float ballisticsSetup::bulletDrop(float distance, float apex, float apexHeight, float drop100)
{
    float dd = abs(apex - distance);
    float P = pow(dd / 100.f, 2);
    return apexHeight - (P * drop100);
}
float ballisticsSetup::bulletDrop(float distance)
{
    switch (currentAmmo)
    {
    case ammo_9mm:
        return bulletDrop(distance, 7.0f, 0.00018f, 0.292f);
        break;
    case ammo_556:
        return bulletDrop(distance, 78.0f, 0.00508f, 0.084f);   // breek effe na 200m
        break;
    case ammo_762:
        return bulletDrop(distance, 78.0f, 0.00508f, 0.076f);   // breek effe na 200m
        break;
    }
    return 0;
}

void ballisticsSetup::renderGuiAmmo(Gui* _gui)
{
    ImGui::Combo("###ammoSelector", (int*)&currentAmmo, " 9mm\0 5.56\0 7.62\0");
}







void target::loadimage(std::string _root)
{
    image = Texture::createFromFile(_root + texturePath, true, true);
}

void target::loadimageDialog()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"png"}, {"jpg"}, {"dds"}, {"bmp"} };
    if (openFileDialog(filters, path))
    {
        texturePath = path.filename().string();
        loadimage(path.parent_path().string() + "/targets/");
    }
}


void target::loadscoreimage(std::string _root)
{
    score = Texture::createFromFile(_root + scorePath, true, true);

    FREE_IMAGE_FORMAT fifFormat = FIF_UNKNOWN;
    fifFormat = FreeImage_GetFileType((_root + scorePath).c_str(), 0);
    if (fifFormat == FIF_UNKNOWN)  // Can't get the format from the file. Use file extension
    {
        fifFormat = FreeImage_GetFIFFromFilename((_root + scorePath).c_str());
    }

    // Check the library supports loading this image type
    if (FreeImage_FIFSupportsReading(fifFormat))
    {
        // Read the DIB
        FIBITMAP* pDib = FreeImage_Load(fifFormat, (_root + scorePath).c_str());
        if (pDib)
        {
            scoreWidth = FreeImage_GetWidth(pDib);
            scoreHeight = FreeImage_GetHeight(pDib);
            int pitch = FreeImage_GetPitch(pDib);
            FREE_IMAGE_COLOR_TYPE colorType = FreeImage_GetColorType(pDib);
            BYTE* data = FreeImage_GetBits(pDib);
            maxScore = 0;

            scoreData = new char[scoreWidth * scoreHeight];
            if (colorType == FIC_PALETTE)
            {
                for (int y = 0; y < scoreHeight; y++) {
                    for (int x = 0; x < scoreWidth; x++) {
                        int index = (y * pitch) + x;
                        if (data[index] > maxScore) maxScore = data[index];
                        int indexFlip = ((scoreHeight - y - 1) * pitch) + x;
                        scoreData[index] = data[indexFlip];
                    }
                }
            }
            FreeImage_Unload(pDib);
        }
    }
}

void target::loadscoreimageDialog()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"png"}, {"jpg"}, {"dds"}, {"bmp"} };
    if (openFileDialog(filters, path))
    {
        scorePath = path.filename().string();
        loadscoreimage(path.parent_path().string() + "/");
    }
}

void target::load(std::filesystem::path _path)
{
    
    std::ifstream is(_path);
    if (is.good()) {
        cereal::XMLInputArchive archive(is);
        serialize(archive, 100);
        loadimage(_path.parent_path().string() + "/");
        loadscoreimage(_path.parent_path().string() + "/");
        fullPath = _path.string();
    }
}

void target::save(std::filesystem::path _path)
{
    std::ofstream os(_path);
    if (os.good()) {
        cereal::XMLOutputArchive archive(os);
        serialize(archive, 100);
    }
}


void target::renderGui(Gui* _gui, Gui::Window& _window)
{
    //bool open = true;

    //if (showGui)
    {
        ImGui::PushFont(_gui->getFont("roboto_64"));
        //ImGui::SameLine(100);
        char T[256];
        sprintf(T, "%s", title.c_str());
        if (ImGui::InputText("name", T, 256)) {
            title = T;
        }
        //ImGui::Text("%s", title.c_str());
        ImGui::PopFont();
        ImGui::NewLine();



        ImGui::PushFont(_gui->getFont("roboto_48"));
        {
            sprintf(T, "%s", description.c_str());
            if (ImGui::InputText("description", T, 256)) {
                description = T;
            }

            if (ImGui::Button("load")) {
                std::filesystem::path path;
                FileDialogFilterVec filters = { {"target.json"} };
                if (openFileDialog(filters, path))
                {
                    load(path);
                }
            }


            ImGui::SameLine(0, 100);
            if (ImGui::Button("save")) {
                std::filesystem::path path;
                FileDialogFilterVec filters = { {"target.json"} };
                if (saveFileDialog(filters, path))
                {
                    save(path);
                }
            }


            ImGui::NewLine();

            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                float W = 400;
                float H = W * scoreHeight / scoreWidth;
                ImGui::BeginChildFrame(1000, ImVec2(W, H));
                {
                    ImVec2 w = ImGui::GetWindowPos();
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    if (image)
                    {
                        ImVec2 q = ImGui::GetCursorPos();

                        if (_window.imageButton("testImage", image, float2(W, H), false))
                        {
                            loadimageDialog();
                        }
                        if (ImGui::IsItemHovered())
                        {
                            ImGuiIO& io = ImGui::GetIO();
                            int mouseX = (int)(io.MousePos.x - w.x);
                            int mouseY = (int)(io.MousePos.y - w.y);
                            int scoreX = (int)(mouseX * scoreWidth / W);
                            int scoreY = (int)(mouseY * scoreHeight / H);
                            int score = scoreData[(scoreY * scoreWidth) + scoreX];
                            ImGui::SetTooltip("%d", score);
                        }
                    }
                    else {
                        if (ImGui::Button("Load image", ImVec2(W, H)))
                        {
                            loadimageDialog();
                        }
                    }
                }
                ImGui::EndChildFrame();


                ImGui::SameLine(0, 200);

                ImGui::BeginChildFrame(1001, ImVec2(W, H));
                {
                    if (score)
                    {
                        if (_window.imageButton("scoreImage", score, float2(W, H), false))
                        {
                            loadscoreimageDialog();
                        }
                    }
                    else {
                        if (ImGui::Button("Load score", ImVec2(W, H)))
                        {
                            loadscoreimageDialog();
                        }
                    }
                }
                ImGui::EndChildFrame();

                ImGui::PopStyleVar();

                ImGui::NewLine();
                ImGui::NewLine();
                ImGui::SameLine(610);
                ImGui::Text("Maximum score - %d", maxScore);

                ImGui::SameLine(10);

                ImGui::SetNextItemWidth(300);
                ImGui::DragInt("width", &size_mm.x, 1, 0, 5000, "%d mm");
                TOOLTIP("Width of the target in mm.\nThis is the width of the image that was loaded, please calculate if you image contains a border.")
                    ImGui::SetNextItemWidth(300);
                ImGui::DragInt("height", &size_mm.y, 1, 0, 5000, "%d mm");
                TOOLTIP("Height of the target in mm.");

                //ImGui::Text("hist  (%d, %d, %d, %d, %d, %d, %d)", hist[0], hist[1], hist[2], hist[3], hist[4], hist[5], hist[6]);



            }
        }
        ImGui::PopFont();
    }

}





void targetAction::renderGui(Gui* _gui)
{
    ImGui::SetNextItemWidth(390);
    ImGui::Combo("###actionSelector", (int*)&action, "static\0popup\0move\0sight");

    ImGui::Checkbox("drop", &dropWhenHit);
    TOOLTIP("Does the target drop down when hit");

    ImGui::NewLine();
    ImGui::SetNextItemWidth(110);
    ImGui::DragInt("repeats", &repeats, 1, 1, 15);
    if (repeats > 1)
    {
        ImGui::SetNextItemWidth(110);
        ImGui::DragFloat("start", &startTime, 0.1f, 0, 60, "%3.1f s");

        ImGui::SetNextItemWidth(110);
        ImGui::DragFloat("up", &upTime, 0.1f, 0, 60, "%3.1f s");

        ImGui::SetNextItemWidth(110);
        ImGui::DragFloat("down", &downTime, 0.1f, 0, 60, "%3.1f s");
    }
    ImGui::NewLine();
    if (action == action_move)
    {
        ImGui::SetNextItemWidth(110);
        ImGui::DragFloat("speed", &speed, 0.1f, 0.1f, 20.f, "%3.1f m/s");
    }
}












bool exercise::renderTargetPopup(Gui* _gui)
{
    for (auto& T : Kolskoot::targetList)
    {
        if (ImGui::Button(T.title.c_str(), ImVec2(200, 0)))     // Just width acurate and large heigth t
        {
            target = T;
            targetPopupOpen = false;
            return true;
        }
    }


    return false;
}


void exercise::renderGui(Gui* _gui, Gui::Window& _window)
{
    ImGui::SameLine(0, 0);
    char T[256];
    sprintf(T, "%s", title.c_str());
    if (ImGui::InputText("", T, 256)) {
        title = T;
    }


    ImGui::PushFont(_gui->getFont("roboto_32"));
    {
        ImGui::NewLine();
        ImGui::Checkbox("scoring", &isScoring);
        TOOLTIP("Does this exercise count towards the total score");
    }
    ImGui::PopFont();

    ImGui::PushFont(_gui->getFont("roboto_20"));
    {
        ImGui::NewLine();
        ImGui::Text("Info like total score");
    }
    ImGui::PopFont();


    ImGui::NewLine();
    ImGui::SameLine(20);
    if (target.image)
    {
        if (_window.imageButton("scoreImage", target.image, float2(150, 100)))     // Just width acurate and large heigth t
        {
            ImGui::OpenPopup("Targets");
        }
    }
    else
    {
        if (ImGui::Button("target", ImVec2(150, 100)))
        {
            ImGui::OpenPopup("Targets");
        }
    }

    if (ImGui::BeginPopup("Targets")) {
        // Draw popup contents.

        if (renderTargetPopup(_gui))
        {
            ImGui::CloseCurrentPopup();
        }
        /*if (ImGui::Button("close")) {
            ImGui::CloseCurrentPopup();
        }*/
        ImGui::EndPopup();
    }

    /*
    if (targetPopupOpen)
    {
        Gui::Window targetPanel(_window, "TargetsK", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize | Falcor::Gui::WindowFlags::SetFocus);
        {
            ImVec2 P = ImGui::GetCursorScreenPos();
            targetPanel.windowSize((int)800, 800);
            //targetPanel.windowPos((int)P.x, (int)P.y);
            //renderTargetPopup(_gui, targetPanel);
        }
        targetPanel.release();
    } */

    /*
    if (ImGui::BeginCombo("##custom combo", current_item, ImGuiComboFlags_NoArrowButton))
    {
        for (int n = 0; n < IM_ARRAYSIZE(items); n++)
        {
            bool is_selected = (current_item == items[n]);
            if (ImGui::Selectable(items[n], is_selected))
                current_item = items[n];
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    */
    //ImGui::SetNextItemWidth(110);
    ImGui::DragFloat("##distance", &targetDistance, 1.0f, 3, 300, "%3.1f m");
    TOOLTIP("distance to the target");

    ImGui::SetNextItemWidth(390);
    ImGui::Combo("###modeSelector", (int*)&pose, "standing\0kneeling\0sitting\0prone\0");

    ImGui::NewLine();
    ImGui::Separator();
    ImGui::SetNextItemWidth(110);
    ImGui::DragInt("rounds", &numRounds, 1, 1, 15);
    TOOLTIP("number of rounds per repeat\ni.e. 2 rounds with 5 popup repeats will result in a total of 10 rounds");


    action.renderGui(_gui);

}






void quickRange::load()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"exercises.json"} };
    if (openFileDialog(filters, path))
    {
        std::ifstream is(path);
        if (is.good()) {
            cereal::XMLInputArchive archive(is);
            serialize(archive, 100);
        }
    }
}


void quickRange::save()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"exercises.json"} };
    if (saveFileDialog(filters, path))
    {
        std::ofstream os(path);
        if (os.good()) {
            cereal::XMLOutputArchive archive(os);
            serialize(archive, 100);
        }
    }
}


void quickRange::renderGui(Gui* _gui, float2 _screenSize, Gui::Window& _window)
{
    {
        ImGui::PushFont(_gui->getFont("roboto_48"));
        {
            {
                ImGui::SameLine(10);
                ImGui::SetNextItemWidth(700);
                char T[256];
                sprintf(T, "%s", title.c_str());
                if (ImGui::InputText("", T, 256)) {
                    title = T;
                }

                ImGui::SameLine(0, 100);
                if (ImGui::Button("load")) {
                    load();
                }


                ImGui::SameLine(0, 100);
                if (ImGui::Button("save")) {
                    save();
                }

                ImGui::SameLine(0, 100);
                ImGui::SetNextItemWidth(100);
                int numX = exercises.size();
                if (ImGui::DragInt("# exercises", &numX, 1, 0, 20))
                {
                    exercises.resize(numX);
                }


                ImGui::SameLine(_screenSize.x - 200, 0);
                if (ImGui::Button("Start")) {
                    requestLive = true;
                    currentExercise = 0;
                    currentStage = live_intro;
                    score.create(5, exercises.size());      // FIXME needs numlanes
                }
            }

            ImGuiStyle& style = ImGui::GetStyle();
            ImGuiStyle styleOld = style;

            ImGui::NewLine();
            float startY = ImGui::GetCursorPosY();
            int x = 0;
            int y = 0;

            for (auto it = exercises.begin(); it != exercises.end(); ++it)
            {
                ImGui::SetCursorPos(ImVec2(x * 400.f, startY));
                ImGui::BeginChildFrame(1376 + y * 300 + x, ImVec2(390, 1100));
                {
                    ImGui::Text("%d :  ", x + 1);
                    it->renderGui(_gui, _window);
                }
                ImGui::EndChildFrame();

                x++;
            }


            style = styleOld;
        }
        ImGui::PopFont();
    }
}



void quickRange::renderLiveMenubar(Gui* _gui)
{
    ImGui::Text("%s       :       Exercise %d       :       %s", title.c_str(), currentExercise + 1, exercises[currentExercise].title.c_str());
}

void quickRange::renderLive(Gui* _gui, float2 _screenSize, Gui::Window& _window, _setup setup, Texture::SharedPtr _bulletHole)
{
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiStyle styleOld = style;
        ImGui::PushFont(_gui->getFont("roboto_48"));
        {
            if (currentExercise < exercises.size())
            {
                switch (currentStage)
                {
                case live_intro:
                    exercises[currentExercise].renderGui(_gui, _window);
                    break;
                case live_live:
                    ImGui::BeginColumns("lanes", setup.numLanes);
                    for (int lane = 0; lane < setup.numLanes; lane++)
                    {
                        float centerX = (setup.screen_pixelsX / setup.numLanes / 2) + (setup.screen_pixelsX / 5 * lane);
                        float centerY = setup.eyeHeights[exercises[currentExercise].pose] - 40;
                        float mToPix = setup.screen_pixelsX / setup.screenWidth * setup.shootingDistance / exercises[currentExercise].targetDistance;


                        float tw = mToPix * 0.001f * exercises[currentExercise].target.size_mm.x;
                        float th = mToPix * 0.001f * exercises[currentExercise].target.size_mm.y;
                        float px = centerX - (tw / 2);
                        float py = centerY - (th * 0.5f);

                        ImGui::SetCursorPos(ImVec2((setup.screen_pixelsX / setup.numLanes / 2) - 10 + (setup.screen_pixelsX / 5 * lane), 20));
                        ImGui::Text("%d", lane + 1);
                        ImGui::Text("%d / %d", score.lane_exercise.at(lane).at(currentExercise).shots.size(), exercises[currentExercise].numRounds);
                        ImGui::Text("%d", score.lane_exercise.at(lane).at(currentExercise).score);
                        ImGui::Text("drop : %d mm", (int)(bulletDrop * 1000));
                        ImGui::Text("offset : %d, %d px", (int)(Kolskoot::ballistics.offset(lane).x), (int)(Kolskoot::ballistics.offset(lane).y));
                        //




                        ImGui::SetCursorPos(ImVec2(px, py));
                        _window.image("testImage", exercises[currentExercise].target.image, float2(tw, th), false);

                        for (int s = 0; s < score.lane_exercise.at(lane).at(currentExercise).shots.size(); s++)
                        {
                            float2 pos = score.lane_exercise.at(lane).at(currentExercise).shots.at(s).position_relative;
                            float px = centerX + (tw * (pos.x - 0.5f));
                            float py = centerY + (th * (pos.y - 0.5f));

                            float bulletSize = mToPix * 0.03f;  // exagerated 3cm but that makes teh hole about 1cm

                            ImGui::SetCursorPos(ImVec2(px - (bulletSize / 2), py - (bulletSize / 2)));
                            _window.image("Shot", _bulletHole, float2(bulletSize, bulletSize), false);
                            //_bulletHole
                        }


                        if (exercises[currentExercise].action.action == action_adjust && (score.lane_exercise.at(lane).at(currentExercise).shots.size() >= exercises[currentExercise].numRounds))
                        {
                            // adjusst the shots
                            // DMAN REALLY SAVE position in meters on the target as well - MUCH easierto work with
                            float2 avs = float2(0, 0);
                            for (int s = 0; s < score.lane_exercise.at(lane).at(currentExercise).shots.size(); s++)
                            {
                                avs += score.lane_exercise.at(lane).at(currentExercise).shots[s].position;
                            }
                            avs /= score.lane_exercise.at(lane).at(currentExercise).shots.size();
                            //avs.y += bulletDrop;
                            //avs *= mToPix;

                            Kolskoot::ballistics.adjustOffset(lane, avs);
                            score.lane_exercise.at(lane).at(currentExercise).shots.clear();
                        }

                        ImGui::NextColumn();
                    }
                    ImGui::EndColumns();
                    break;
                case live_scores:   // not sure about this
                    ImGui::Text("scores page");
                    break;
                }
            }
        }
        ImGui::PopFont();
        style = styleOld;
    }
}


void quickRange::mouseShot(float x, float y, _setup setup)
{
    int lane = (int)floor(x * setup.numLanes / setup.screen_pixelsX);

    x -= Kolskoot::ballistics.offset(lane).x;
    y -= Kolskoot::ballistics.offset(lane).y;

    // move all of this to target
    float tw = 0.001f * exercises[currentExercise].target.size_mm.x / setup.screenWidth * setup.screen_pixelsX * setup.shootingDistance / exercises[currentExercise].targetDistance;
    float th = tw * exercises[currentExercise].target.size_mm.y / exercises[currentExercise].target.size_mm.x;
    float px = (setup.screen_pixelsX / setup.numLanes / 2) - (tw / 2) + (setup.screen_pixelsX / 5 * lane);
    float py = (setup.eyeHeights[exercises[currentExercise].pose]) - (th * 0.5f);

    float shotX = (x - px) / tw;    // 0-1 texture space
    float shotY = (y - py) / th;

    float absX = (shotX - 0.5f) * tw;    // meters relativeto center
    float absY = (shotY - 0.5f) * th;

    // translate to scoring space
    int scoreX = (int)(shotX * exercises[currentExercise].target.scoreWidth);
    int scoreY = (int)(shotY * exercises[currentExercise].target.scoreHeight);
    int shotScore = 0;
    if (scoreX >= 0 && scoreX < exercises[currentExercise].target.scoreWidth &&
        scoreY >= 0 && scoreY < exercises[currentExercise].target.scoreHeight)
    {
        int scoreIndex = (scoreY * exercises[currentExercise].target.scoreWidth) + scoreX;
        shotScore = exercises[currentExercise].target.scoreData[scoreIndex];
    }

    score.lane_exercise.at(lane).at(currentExercise).score += shotScore;
    score.lane_exercise.at(lane).at(currentExercise).shots.emplace_back();
    score.lane_exercise.at(lane).at(currentExercise).shots.back().score = shotScore;
    score.lane_exercise.at(lane).at(currentExercise).shots.back().position = float2(absX, absY);
    score.lane_exercise.at(lane).at(currentExercise).shots.back().position_relative = float2(shotX, shotY);
    score.lane_exercise.at(lane).at(currentExercise).target = exercises[currentExercise].target;
}


bool quickRange::liveNext()
{
    switch (currentStage) {
    case live_intro:
        currentStage = live_live;
        bulletDrop = Kolskoot::ballistics.bulletDrop(exercises[currentExercise].targetDistance);
        break;
    case live_live:
        currentStage = live_scores;
        bulletDrop = Kolskoot::ballistics.bulletDrop(exercises[currentExercise].targetDistance);    // just incase first one misses
        break;
    case live_scores:
        currentStage = live_intro;
        currentExercise++;
        if (currentExercise >= exercises.size())  // we are done, return to quick range view
        {
            return true;
        }
        break;
    }



    return false;
}


int quickRange::getRoundsLeft(int _lane)
{
    int total = exercises[currentExercise].numRounds * exercises[currentExercise].action.repeats;
    int left = total - score.lane_exercise.at(_lane).at(currentExercise).shots.size();
    return  __max(0, left);
}



void _scoring::create(size_t _numlanes, size_t _numEx)
{
    lane_exercise.clear();
    lane_exercise.resize(_numlanes);
    for (auto& lane : lane_exercise)
    {
        lane.resize(_numEx);
    }
}
















bool sort_x(glm::vec4& a, glm::vec4& b) {
    return (a.x < b.x);
}
bool sort_y(glm::vec4& a, glm::vec4& b) {
    return (a.y < b.y);
}

void videoToScreen::build(std::array<glm::vec4, 45>  dots, float2 screenSize)
{
    std::sort(dots.begin(), dots.end(), sort_y);

    std::sort(dots.begin() + 0, dots.begin() + 9, sort_x);
    std::sort(dots.begin() + 9, dots.begin() + 18, sort_x);
    std::sort(dots.begin() + 18, dots.begin() + 27, sort_x);
    std::sort(dots.begin() + 27, dots.begin() + 36, sort_x);
    std::sort(dots.begin() + 36, dots.begin() + 45, sort_x);

    float dY = (screenSize.y - 40) / 4;
    float dX = (screenSize.x - 40) / 8;

    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            uint index = y * 9 + x;
            //grid[y][x].screenpos[0]
            grid[y][x].videoPos[0] = dots[index + 0];
            grid[y][x].videoPos[1] = dots[index + 1];
            grid[y][x].videoPos[2] = dots[index + 10];
            grid[y][x].videoPos[3] = dots[index + 9];
            grid[y][x].videoPos[0].z = 0;
            grid[y][x].videoPos[1].z = 0;
            grid[y][x].videoPos[2].z = 0;
            grid[y][x].videoPos[3].z = 0;

            grid[y][x].videoEdge[0] = glm::normalize(grid[y][x].videoPos[1] - grid[y][x].videoPos[0]);
            grid[y][x].videoEdge[1] = glm::normalize(grid[y][x].videoPos[2] - grid[y][x].videoPos[1]);
            grid[y][x].videoEdge[2] = glm::normalize(grid[y][x].videoPos[3] - grid[y][x].videoPos[2]);
            grid[y][x].videoEdge[3] = glm::normalize(grid[y][x].videoPos[0] - grid[y][x].videoPos[3]);

            grid[y][x].screenPos[0] = glm::vec3(20 + x * dX, 20 + y * dY, 0);
            grid[y][x].screenPos[1] = glm::vec3(20 + (x + 1) * dX, 20 + y * dY, 0);
            grid[y][x].screenPos[2] = glm::vec3(20 + (x + 1) * dX, 20 + (y + 1) * dY, 0);
            grid[y][x].screenPos[3] = glm::vec3(20 + x * dX, 20 + (y + 1) * dY, 0);
        }
    }
}

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

            float wA = glm::cross(-vA, grid[y][x].videoEdge[0]).z;
            float wB = glm::cross(-vB, grid[y][x].videoEdge[1]).z;
            float wC = glm::cross(-vC, grid[y][x].videoEdge[2]).z;
            float wD = glm::cross(-vD, grid[y][x].videoEdge[3]).z;

            if (wA >= 0 && wB >= 0 && wC >= 0 && wD >= 0)		// ons het die regte blok
            {
                float wX = wA / (wA + wC);
                float wY = wD / (wB + wD);
                screen = grid[y][x].screenPos[0] * (1.0f - wX) * (1.0f - wY) +
                    grid[y][x].screenPos[1] * (1.0f - wX) * (wY)+
                    grid[y][x].screenPos[2] * (wX) * (wY)+
                    grid[y][x].screenPos[3] * (wX) * (1.0f - wY);
            }
        }
    }
    return screen;
}









void Kolskoot::onGuiMenubar(Gui* _gui)
{
    if (ImGui::BeginMainMenuBar())
    {
        {
            bool selected = false;
            ImGui::Text("Kolskoot");

            ImGui::SameLine(0, 200);
            ImGui::SetNextItemWidth(150);


            if (guiMode == gui_live)
            {
                QR.renderLiveMenubar(_gui);
            }
            else
            {
                ImGui::SetNextItemWidth(150);
                if(ImGui::BeginMenu("Settings"))
                {
                    if (ImGui::MenuItem("Camera"))
                    {
                        guiMode = gui_camera;
                    }
                    if (ImGui::MenuItem("Screen"))
                    {
                        guiMode = gui_screen;
                    }
                    if (ImGui::MenuItem("Targets"))
                    {
                        guiMode = gui_targets;
                    }
                    ImGui::EndMenu();
                }
                

                /*
                ImGui::Selectable("Camera", &selected, 0, ImVec2(150, 0));
                if (selected) {
                    guiMode = gui_camera;
                    selected = false;
                }

                ImGui::SameLine(0, 20);
                ImGui::SetNextItemWidth(150);
                ImGui::Selectable("Screen", &selected, 0, ImVec2(150, 0));
                if (selected) {
                    guiMode = gui_screen;
                    selected = false;
                }

                ImGui::SameLine(0, 20);
                ImGui::SetNextItemWidth(150);
                ImGui::Selectable("Targets", &selected, 0, ImVec2(150, 0));
                if (selected) {
                    guiMode = gui_targets;
                    selected = false;
                }
                */

                ImGui::SameLine(0, 20);
                ImGui::SetNextItemWidth(150);
                ImGui::Selectable("Exercises", &selected, 0, ImVec2(150, 0));
                if (selected) {
                    guiMode = gui_exercises;
                    selected = false;
                }

                ImGui::SameLine(0, 20);
                ImGui::SetNextItemWidth(150);
                ballistics.renderGuiAmmo(_gui);
            }


            ImGui::SetCursorPos(ImVec2(screenSize.x - 30, 0));
            if (ImGui::Selectable("X")) { gpFramework->getWindow()->shutdown(); }
        }
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

    ImGui::PushFont(_gui->getFont("roboto_32"));
    {
        onGuiMenubar(_gui);

        switch (guiMode)
        {
        case gui_menu:
            break;

        case gui_camera:
            break;

        case gui_screen:
        {
            Gui::Window screenPanel(_gui, "Screen", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            {
                screenPanel.windowSize((int)screenSize.x, (int)screenSize.y - 40);
                screenPanel.windowPos(0, 40);
                setupInfo.renderGui(_gui, screenSize.x);
                if (setupInfo.requestClose)
                {
                    setupInfo.requestClose = false;
                    guiMode = gui_menu;
                }
            }
            screenPanel.release();
        }
        break;

        case gui_targets:
        {
            Gui::Window targetPicker(_gui, "Target picker", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            targetPicker.windowSize((int)screenSize.x / 2, (int)screenSize.y - 40);
            targetPicker.windowPos(0, 40);
            {
                ImGui::PushFont(_gui->getFont("roboto_32"));
                int W = (int)floor(screenSize.x / 2 / 240);
                for (int i = 0; i < targetList.size(); i++)
                {
                    float x = (float)(i % W);
                    float y = (float)(i / W);
                    ImGui::SetCursorPos(ImVec2(20 + x * 240, 40 + y * 400));
                    ImGui::Text(targetList[i].title.c_str());

                    ImGui::SetCursorPos(ImVec2(20 + x * 240, 40 + 32 + y * 400));
                    if (targetPicker.imageButton(targetList[i].title.c_str(), targetList[i].image, float2(200, 150)))
                    {
                        targetBuilder.load(targetList[i].fullPath);
                    }
                }
                ImGui::PopFont();
            }
            targetPicker.release();

            Gui::Window targetPanel(_gui, "Target", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            targetPanel.windowSize((int)screenSize.x / 2, (int)screenSize.y - 40);
            targetPanel.windowPos((int)screenSize.x / 2, 40);
            targetBuilder.renderGui(_gui, targetPanel);
            targetPanel.release();
        }
        break;

        case gui_exercises:
        {
            Gui::Window exercisePanel(_gui, "Excercises", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            exercisePanel.windowSize((int)screenSize.x, (int)screenSize.y - 40);
            exercisePanel.windowPos(0, 40);
            QR.renderGui(_gui, screenSize, exercisePanel);
            exercisePanel.release();

            if (requestLive)
            {
                requestLive = false;
                Kolskoot::guiMode = gui_live;
            }

        }
        break;

        case gui_live:
        {
            Gui::Window livePanel(_gui, "Live", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            livePanel.windowSize((int)screenSize.x, (int)screenSize.y - 40);
            livePanel.windowPos(0, 40);
            QR.renderLive(_gui, screenSize, livePanel, setupInfo, bulletHole);
            livePanel.release();

        }
        break;
        }






        if (guiMode == gui_camera)
        {
            Gui::Window pointgreyPanel(_gui, "PointGrey", { 900, 900 }, { 100, 100 }, Falcor::Gui::WindowFlags::NoResize);
            pointgreyPanel.windowSize((int)screenSize.x, (int)screenSize.y - 40);
            pointgreyPanel.windowPos(0, 40);
            {
                ImGui::PushFont(_gui->getFont("roboto_32"));
                {
                    if (!pointGreyCamera->isConnected())
                    {
                        ImGui::Text("Camera not connected");
                    }
                    else
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
                            guiMode = gui_menu;
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

                        ImGui::SetCursorPos(ImVec2(100, 100));
                        if (pointGreyBuffer)
                        {
                            calibrationPanel.imageButton("testImage", pointGreyBuffer, float2(screenSize.x - 200, screenSize.y - 200), false);
                        }
                        ImGui::Text("cnt %d", pointGreyCamera->getFrameCount(0));

                        style.Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.8f, 0.8f, 1.f);
                        ImGui::SetCursorPos(ImVec2(50, 50));
                        ImGui::Text("Top left");
                        ImGui::SetCursorPos(ImVec2(screenSize.x - 200, 50));
                        ImGui::Text("Top right");
                        ImGui::SetCursorPos(ImVec2(50, screenSize.y - 50));
                        ImGui::Text("Bottom left");
                        ImGui::SetCursorPos(ImVec2(screenSize.x - 200, screenSize.y - 50));
                        ImGui::Text("Bottom right");

                        ImGui::SetCursorPos(ImVec2(screenSize.x / 2 + 150, screenSize.y / 2 - 60));
                        ImGui::Text("remove the filter");
                        ImGui::SetCursorPos(ImVec2(screenSize.x / 2 + 150, screenSize.y / 2 - 30));
                        ImGui::Text("zoom and focus the camera");
                        ImGui::SetCursorPos(ImVec2(screenSize.x / 2 + 150, screenSize.y / 2 - 0));
                        ImGui::Text("set the brightness");
                        ImGui::SetCursorPos(ImVec2(screenSize.x / 2 + 150, screenSize.y / 2 + 30));
                        ImGui::Text("press 'space' when done");
                        //pointGreyCamera->setReferenceFrame(true, 1);

                    }

                    if (modeCalibrate == 2) {
                        if (calibrationCounter > 0) {
                            if (calibrationCounter == 100) {
                                pointGreyCamera->setReferenceFrame(true, 5);
                            }
                            calibrationCounter--;
                        }
                        else
                        {
                            style.Colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.1f, 1.f);


                            ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 - 200));
                            ImGui::Text("adjust the light untill all 45 dots are found");
                            ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 - 160));
                            ImGui::Text("hide cursor by dragging it to the bottom of the screen");
                            uint ndots = pointGreyCamera->dotQueue.size();

                            ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 - 120));
                            ImGui::Text("also log dot count here  %d / 45", ndots);

                            if (ndots == 45) {
                                ImGui::SetCursorPos(ImVec2(screenSize.x / 2 - 150, screenSize.y / 2 - 60));
                                ImGui::Text("all dots picked up, press space to save");
                                for (int j = 0; j < 45; j++) {
                                    calibrationDots[j] = pointGreyCamera->dotQueue.front();
                                    pointGreyCamera->dotQueue.pop();
                                }
                            }

                            while (pointGreyCamera->dotQueue.size()) {
                                pointGreyCamera->dotQueue.pop();
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

                    if (modeCalibrate == 3)
                    {
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        static ImVec4 col = ImVec4(1.0f, 0.0f, 0.0f, 0.3f);
                        const ImU32 col32 = ImColor(col);


                        while (pointGreyCamera->dotQueue.size()) {
                            glm::vec3 screen = screenMap.toScreen(pointGreyCamera->dotQueue.front());
                            draw_list->AddCircle(ImVec2(screen.x, screen.y), 18, col32, 30, 6);

                            for (int i = 0; i < setupInfo.numLanes; i++)
                            {
                                zigbeeRounds(i, 100);
                            }

                            pointGreyCamera->dotQueue.pop();
                        }

                    }
                }
                ImGui::PopFont();
            }
            calibrationPanel.release();


            if (modeCalibrate == 4) {
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

    setupInfo.load();
    ballistics.load();
    bulletHole = Texture::createFromFile(setupInfo.dataFolder + "/targets/bullet.dds", true, true);

    // targets

    for (const auto& entry : std::filesystem::directory_iterator(setupInfo.dataFolder + "/targets"))
    {
        if (!entry.is_directory())
        {
            std::string ext = entry.path().extension().string();
            if (ext.find("json") != std::string::npos) {
                targetList.emplace_back();
                targetList.back().load(entry.path());
            }
        }
    }

    int ret = ZIGBEE.OpenPort((eComPort)setupInfo.zigbeeCOM, br_38400, 8, ptNONE, sbONE);
    if (ret != 0)
    {
        // log zigbee failed
    }

    graphicsState = GraphicsState::create();

    guiStyle();
}



void Kolskoot::onShutdown()
{
    PointGrey_Camera::FreeSingleton();

    if (ZIGBEE.IsOpen())
    {
        zigbeeRounds(0, 0, true);
        Sleep(100);
        ZIGBEE.ClosePort();
    }
}

// FIXME make an update function even if i call it from eher or onUpate

void Kolskoot::onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
    gpDevice->toggleVSync(true);

    if (guiMode == gui_camera)
    {
        if (pointGreyBuffer)
        {
            pRenderContext->updateTextureData(pointGreyBuffer.get(), pointGreyCamera->bufferReference);
        }
        if (pointGreyDiffBuffer)
        {
            pRenderContext->updateTextureData(pointGreyDiffBuffer.get(), pointGreyCamera->bufferThreshold);
        }
    }

    const float4 clearColor(0.02f, 0.02f, 0.02f, 1);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    mpGraphicsState->setFbo(pTargetFbo);



    if ((guiMode == gui_live) && (ImGui::IsMouseClicked(0)))
    {
        ImVec2 mouse = ImGui::GetMousePos();
        // insert a mouse shot
        QR.mouseShot(mouse.x, mouse.y, setupInfo);
    }


    // live fire shooting
    if (guiMode == gui_live && QR.currentStage == live_live)
    {
        while (pointGreyCamera->dotQueue.size()) {
            glm::vec3 screen = screenMap.toScreen(pointGreyCamera->dotQueue.front());

            if (!(screen.x == 0 && screen.y == 0))
            {
                int lane = (int)floor(screen.x / (screenSize.x / setupInfo.numLanes));
                QR.mouseShot(screen.x, screen.y, setupInfo);
                zigbeeFire(lane);                   // R4 / AK
            }
            pointGreyCamera->dotQueue.pop();
        }

        // turn air on for pistols if there are rounds left
        for (int i = 0; i < setupInfo.numLanes; i++)
        {
            zigbeeRounds(i, QR.getRoundsLeft(i));
            //zigbeeRounds(i, 5);
        }
    }
    else if (guiMode == gui_live)
    {
        for (int i = 0; i < setupInfo.numLanes; i++)
        {
            zigbeeRounds(i, 0, true);       // turn air off
        }
    }


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

    Sleep(10);
}



bool Kolskoot::onKeyEvent(const KeyboardEvent& keyEvent)
{
    if (mpScene && mpScene->onKeyEvent(keyEvent)) return true;

    if ((keyEvent.type == KeyboardEvent::Type::KeyPressed) && (keyEvent.key == Input::Key::R))
    {
        return true;
    }

    if ((keyEvent.type == KeyboardEvent::Type::KeyPressed) && (keyEvent.key == Input::Key::Escape))
    {
        if (targetPopupOpen) {
            targetPopupOpen = false;
        }
        else
        {
            guiMode = gui_menu;
            zigbeeRounds(0, 0, true);		// turn all air off
        }
        return true;
    }

    if ((modeCalibrate > 0) && (keyEvent.type == KeyboardEvent::Type::KeyPressed) && (keyEvent.key == Input::Key::Space))
    {
        modeCalibrate++;

        if (modeCalibrate == 2) {
            calibrationCounter = 120;
        }
        else if (modeCalibrate == 3) {
            screenMap.build(calibrationDots, screenSize);

            std::ofstream os("kolskootCamera.xml");
            if (os.good()) {
                cereal::XMLOutputArchive archive(os);
                screenMap.serialize(archive, 100);
            }
        }
    }

    if ((guiMode == gui_live) && (keyEvent.type == KeyboardEvent::Type::KeyPressed) && (keyEvent.key == Input::Key::Space))
    {
        if (QR.liveNext())
        {
            guiMode = gui_exercises;
            zigbeeRounds(0, 0, true);		// turn all air off
        }
    }
    if ((guiMode == gui_live) && (keyEvent.type == KeyboardEvent::Type::KeyPressed) && (keyEvent.key == Input::Key::Escape))
    {
        guiMode = gui_exercises;
        zigbeeRounds(0, 0, true);		// turn all air off
    }

    return false;
}



bool Kolskoot::onMouseEvent(const MouseEvent& mouseEvent)
{
    if (mouseEvent.type == MouseEvent::Type::Move)
    {
        mouseScreen = mouseEvent.screenPos;
        if (ImGui::IsMouseDown(0))
        {
            //screenInfo.eyeHeights[screenInfo.dragSelect] = mouseEvent.pos.y;
            setupInfo.eyeHeights[0] = mouseEvent.pos.y * screenSize.y;
        }
    }

    if ((guiMode == gui_live) && (mouseEvent.type == MouseEvent::Type::ButtonDown))
    {
        // insert a mouse shot
        QR.mouseShot((mouseEvent.pos.x * screenSize.y), (mouseEvent.pos.y * screenSize.y), setupInfo);
    }

    return false;
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

    style.Colors[ImGuiCol_FrameBg] = ImVec4(0, 0.03f, 0.03f, 1);

    style.Colors[ImGuiCol_Tab] = ImVec4(0.03f, 0.03f, 0.03f, 1.f);
    style.Colors[ImGuiCol_TabHovered] = DARKLIME(1.f);
    style.Colors[ImGuiCol_TabActive] = DARKLIME(1.f);

    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.03f, 0.03f, 0.03f, 1.0f);

    style.Colors[ImGuiCol_ModalWindowDimBg] = DARKLIME(0.3f);

    // FIXME only for live
    style.Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.0f, 0.0f, 0.f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.0f, 0.0f, 0.f);
    //style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.0f, 0.0f, 0.f);

    style.FrameRounding = 0.0f;

    style.ScrollbarSize = 10;
    style.FrameBorderSize = 0;
    //style.FramePadding = ImVec2(0, 0);

}




void   Kolskoot::zigbeePaperMove()
{
    if (!ZIGBEE.IsOpen()) return;
    unsigned char channels = 1 << 4;		// set this to true

    unsigned char STR[13];			// veelvoude van 10
    unsigned long bytes = ZIGBEE.Read(STR, 13);	// read   ??? Hoekom moet on sbuffer skoonmaak  FIXME
    Sleep(30);

    unsigned char ANS[13];
    switch (setupInfo.zigbeePacketVersion)
    {
    case 0:
        ANS[0] = 0xff;
        ANS[1] = 0xff;
        ANS[2] = 0x00;	// device ID
        ANS[3] = 0x00;
        ANS[4] = 0x00;	//own id
        ANS[5] = 0x01;
        ANS[6] = 0x0d;	//lenght - all bytes
        ANS[7] = 0x16;	//command
        ANS[8] = 0x00;	// body
        ANS[9] = 0x00;
        ANS[10] = channels;
        ANS[11] = 0x00; // status
        ANS[12] = 0xDE - channels;
        ZIGBEE.Write(ANS, 13);
        break;
    case 1:
        ANS[0] = 0xff;
        ANS[1] = 0xff;
        ANS[2] = 0x0b;	// device ID
        ANS[3] = 0x00;
        ANS[4] = zigbeeID_h;	//own id
        ANS[5] = zigbeeID_l;
        ANS[6] = 0x16;	//lenght - all bytes
        ANS[7] = 0x00;	//command
        ANS[8] = channels;
        ANS[9] = 0x00; // status
        int checksum = 0x100;
        for (int i = 0; i < 10; i++)  checksum -= ANS[i];
        ANS[10] = checksum & 0xff;  //0xB6 - (1 << lane);
        ZIGBEE.Write(ANS, 11);
        break;
    }
}

void   Kolskoot::zigbeePaperStop()
{
    if (!ZIGBEE.IsOpen()) return;
    unsigned char channels = 0;

    unsigned char STR[13];			// veelvoude van 10
    unsigned long bytes = ZIGBEE.Read(STR, 13);	// read   ??? Hoekom moet on sbuffer skoonmaak  FIXME
    Sleep(30);

    unsigned char ANS[13];
    switch (setupInfo.zigbeePacketVersion)
    {
    case 0:
        ANS[0] = 0xff;
        ANS[1] = 0xff;
        ANS[2] = 0x00;	// device ID
        ANS[3] = 0x00;
        ANS[4] = 0x00;	//own id
        ANS[5] = 0x01;
        ANS[6] = 0x0d;	//lenght - all bytes
        ANS[7] = 0x16;	//command
        ANS[8] = 0x00;	// body
        ANS[9] = 0x00;
        ANS[10] = channels;
        ANS[11] = 0x00; // status
        ANS[12] = 0xDE - channels;
        ZIGBEE.Write(ANS, 13);
        break;
    case 1:
        ANS[0] = 0xff;
        ANS[1] = 0xff;
        ANS[2] = 0x0b;	// device ID
        ANS[3] = 0x00;
        ANS[4] = zigbeeID_h;	//own id
        ANS[5] = zigbeeID_l;
        ANS[6] = 0x16;	//lenght - all bytes
        ANS[7] = 0x00;	//command
        ANS[8] = channels;
        ANS[9] = 0x00; // status
        int checksum = 0x100;
        for (int i = 0; i < 10; i++)  checksum -= ANS[i];
        ANS[10] = checksum & 0xff;  //0xB6 - (1 << lane);
        ZIGBEE.Write(ANS, 11);
        break;
    }
}


void  Kolskoot::zigbeeRounds(unsigned int lane, int R, bool bStop)
{
    if (!ZIGBEE.IsOpen()) return;


    // test ammo for air cycled and return
    if ((!bStop) && (Kolskoot::ballistics.currentAmmo != ammo_9mm)) return;

    static unsigned char channels = 0;
    unsigned char newchannels = channels;
    if (R > 0)	newchannels |= 1 << lane;		// set this to true
    else		newchannels &= ~(1 << lane);	// clear this bit

    if (bStop) newchannels = 0;		// on stop command clear it all

    if (bStop || (channels != newchannels))
    {
        channels = newchannels;

        unsigned char STR[13];			// veelvoude van 10
        unsigned long bytes = ZIGBEE.Read(STR, 13);	// read   ??? Hoekom moet on sbuffer skoonmaak  FIXME
        Sleep(30);


        unsigned char ANS[13];

        switch (setupInfo.zigbeePacketVersion)
        {
        case 0:
            ANS[0] = 0xff;
            ANS[1] = 0xff;
            ANS[2] = 0x00;	// device ID
            ANS[3] = 0x00;
            ANS[4] = 0x00;	//own id
            ANS[5] = 0x01;
            ANS[6] = 0x0d;	//lenght - all bytes
            ANS[7] = 0x16;	//command
            ANS[8] = 0x00;	// body
            ANS[9] = 0x00;
            ANS[10] = channels;
            ANS[11] = 0x00; // status
            ANS[12] = 0xDE - channels;

            ZIGBEE.Write(ANS, 13);
            break;

        case 1:
            ANS[0] = 0xff;
            ANS[1] = 0xff;
            ANS[2] = 0x0b;	// device ID
            ANS[3] = 0x00;
            ANS[4] = zigbeeID_h;	//own id
            ANS[5] = zigbeeID_l;
            ANS[6] = 0x16;	//lenght - all bytes
            ANS[7] = 0x00;	//command
            ANS[8] = channels;
            ANS[9] = 0x00; // status

            int checksum = 0x100;
            for (int i = 0; i < 10; i++)  checksum -= ANS[i];

            ANS[10] = checksum & 0xff;  //0xB6 - (1 << lane);
            ZIGBEE.Write(ANS, 11);

            //ANS[10] = 0xE3 - channels;

            ZIGBEE.Write(ANS, 11);
            break;
        }


        //m_pLogger->Log( LOG_INFO, L"zigbee (%d, %d, %d) - %.2X%.2X %.2X%.2X %.2X%.2X %.2X%.2X %.2X%.2X %.2X%.2X %.2X", lane, R, channels, ANS[0], ANS[1], ANS[2], ANS[3], ANS[4], ANS[5], ANS[6], ANS[7], ANS[8], ANS[9], ANS[10], ANS[11], ANS[12] );
    }
}

void  Kolskoot::zigbeeFire(unsigned int lane)       //R4 and AK
{
    if (!ZIGBEE.IsOpen()) return;
    if (Kolskoot::ballistics.currentAmmo == ammo_9mm) return;


    // test R for change and return - write to zigbee

    unsigned char STR[50];			// veelvoude van 10
    memset(STR, 0, 50);			// clear
    unsigned long bytes = ZIGBEE.Read(STR, 13);	// read   ??? Hoekom moet on sbuffer skoonmaak  FIXME

    unsigned char ANS[13];

    switch (setupInfo.zigbeePacketVersion)
    {
    case 0:
        ANS[0] = 0xff;
        ANS[1] = 0xff;
        ANS[2] = 0x00;	// device ID
        ANS[3] = 0x00;
        ANS[4] = 0x00;	//own id
        ANS[5] = 0x01;
        ANS[6] = 0x0d;	//lenght - all bytes
        ANS[7] = 0x18;	//command
        ANS[8] = 0x00;	// body
        ANS[9] = 0x00;
        ANS[10] = 1 << lane;
        ANS[11] = 0x00; // status
        ANS[12] = 0xDC - (1 << lane);
        ZIGBEE.Write(ANS, 13);
        break;

    case 1:
        ANS[0] = 0xff;
        ANS[1] = 0xff;
        ANS[2] = 0x0b;	// lengte van oakkie
        ANS[3] = 0x00;
        ANS[4] = zigbeeID_h;//0x69;	//ID from config file  FIXME
        ANS[5] = zigbeeID_l;//0xc0;
        ANS[6] = 0x18;	//cammand 
        ANS[7] = 0x00;	//command
        ANS[8] = 1 << lane;
        ANS[9] = 0x00; // status

        int checksum = 0x100;
        for (int i = 0; i < 10; i++)  checksum -= ANS[i];

        ANS[10] = checksum & 0xff;  //0xB6 - (1 << lane);
        ZIGBEE.Write(ANS, 11);
        break;
    }
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
