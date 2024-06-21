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
#include <windows.h>
#include <mmsystem.h>

Texture::SharedPtr	        backdrop;

#pragma optimize("", off)

uint menuHeight = 64;
Kolskoot::UniquePtr pKolskoot;

std::vector<_target> Kolskoot::targetList;
_setup Kolskoot::setup;
ballisticsSetup Kolskoot::ballistics;
laneAirEnable Kolskoot::airToggle;
CCommunication Kolskoot::ZIGBEE;		// vir AIR beheer
quickRange  Kolskoot::QR;
float2 mouseScreen;
static bool targetPopupOpen = false;
bool requestLive = false;

Texture::SharedPtr	        Kolskoot::bulletHole = nullptr;
Texture::SharedPtr	        Kolskoot::ammoType[3] = { nullptr, nullptr, nullptr };



#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}








int pdf_Line(HPDF_Page page, float x, float y, float x2, float y2)
{
    HPDF_Page_SetRGBStroke(page, 0.0, 0.0, 0.0);
    HPDF_Page_SetLineWidth(page, 0.3f);
    HPDF_Page_MoveTo(page, x, HPDF_Page_GetHeight(page) - y);
    HPDF_Page_LineTo(page, x2, HPDF_Page_GetHeight(page) - y2);
    HPDF_Page_Stroke(page);
    return 0;
}


enum { align_LEFT, align_MIDDLE, align_RIGHT };
float pdf_Text(HPDF_Page page, HPDF_Font font, float x, float y, std::string txt, float size, int alignment)
{
    HPDF_Page_SetFontAndSize(page, font, size);
    HPDF_Page_SetWordSpace(page, 2);
    float w = HPDF_Page_TextWidth(page, txt.c_str());
    switch (alignment)
    {
    case align_LEFT:
        break;
    case align_MIDDLE:
        x = x - (w / 2);
        break;
    case align_RIGHT:
        x = x - w;
        break;
    }

    HPDF_Page_BeginText(page);
    HPDF_Page_MoveTextPos(page, x, HPDF_Page_GetHeight(page) - y);
    HPDF_Page_ShowText(page, txt.c_str());
    HPDF_Page_EndText(page);

    return 	w;	// return width
}
/*
void pdf_PNG(HPDF_Doc  pdf, HPDF_Page page, std::string name, float x, float y, float w, float h)
{
    HPDF_Image image = HPDF_LoadPngImageFromFile(pdf, name.c_str());
    if (image)
    {
        HPDF_Page_DrawImage(page, image, x, HPDF_Page_GetHeight(page) - y, w, h);
    }
}
*/
void pdf_JPG(HPDF_Doc  pdf, HPDF_Page page, std::string name, float x, float y, float w, float h)
{
    HPDF_Image image = HPDF_LoadJpegImageFromFile(pdf, name.c_str());
    if (image)
    {
        HPDF_Page_DrawImage(page, image, x, HPDF_Page_GetHeight(page) - y, w, h);
    }
}

int pdf_Circle(HPDF_Page page, float x, float y, float r)
{
    HPDF_Page_SetRGBStroke(page, 0.0, 0.0, 0.0);
    HPDF_Page_SetLineWidth(page, 1.0);
    HPDF_Page_Circle(page, x, HPDF_Page_GetHeight(page) - y, r);
    HPDF_Page_Stroke(page);
    return 0;
}

int pdf_CircleWhite(HPDF_Page page, float x, float y, float r)
{
    HPDF_Page_SetRGBStroke(page, 1.0, 1.0, 1.0);
    HPDF_Page_SetLineWidth(page, 2.0);
    HPDF_Page_Circle(page, x, HPDF_Page_GetHeight(page) - y, r);
    HPDF_Page_Stroke(page);
    return 0;
}

#ifdef HPDF_DLL
void  __stdcall
#else
void
#endif
error_handler(HPDF_STATUS   error_no,
    HPDF_STATUS   detail_no,
    void* user_data)
{
    // do something
}









void _setup::renderGui(Gui* _gui, float _screenX, bool _intructor)
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
        ImVec2 cursor = ImGui::GetCursorPos();
        screen_pixelsX = _screenX;

        ImGui::SetCursorPosX(screen_pixelsX - 600);
        if (ImGui::Button("...")) {
            std::filesystem::path _path;
            if (chooseFolderDialog(_path))
            {
                dataFolder = _path.string();
            }
        }

        ImGui::SameLine(0, 10);

        ImGui::SetNextItemWidth(500);
        char T[256];
        sprintf(T, "%s", dataFolder.c_str());
        if (ImGui::InputText("root folder", T, 256)) {
            dataFolder = T;
        }

        //ImGui::SameLine();
        ImGui::SetCursorPosX(screen_pixelsX - 200);
        ImGui::SetNextItemWidth(200);
        if (ImGui::Selectable("save & close")) { save(); requestClose = true; }

        



        ImGui::SetCursorPos(cursor);

        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("screen size", &screenWidth, 0.01f, 1.0f, 10.0f, "%3.2f m");
        TOOLTIP("horizontal width of the screen in meters\nIf 2 screens, this is the width of both");
        pixelsPerMeter = screen_pixelsX / screenWidth;
        //ImGui::PushFont(_gui->getFont("roboto_20"));
        //ImGui::Text("     %3.2f screen of %d pixels = %3.2f pixels per meter", screenWidth, (int)screen_pixelsX, pixelsPerMeter);
        //ImGui::PopFont();

        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("distance", &shootingDistance, 0.1f, 1, 100, "%3.2f m");
        TOOLTIP("Measure the distance from the screen to the shooting line");

        ImGui::SetNextItemWidth(200);
        ImGui::DragInt("lanes", &numLanes, 0.04f, 1, 15);

        ImGui::SetNextItemWidth(200);
        ImGui::InputInt("zigbeeCom", &zigbeeCOM, 1);




        ImGui::SetCursorPos(ImVec2(600, cursor.y));
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("stand", &eyeHeights[0], 1, 100, 2000);

        ImGui::SetCursorPosX(600);
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("kneel", &eyeHeights[1], 1, 100, 2000);

        ImGui::SetCursorPosX(600);
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("sit", &eyeHeights[2], 1, 100, 2000);

        ImGui::SetCursorPosX(600);
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("prone##1", &eyeHeights[3], 1, 100, 2000);







        if (!_intructor)
        {
            ImGui::NewLine();
            //ImGui::SetCursorPos(ImVec2(0, 350));
            ImGui::Columns(numLanes);
            for (int i = 0; i < numLanes; i++)
            {
                ImGui::Text("lane %d", i + 1);
                ImGui::NextColumn();
            }
        }


        ImGui::SetCursorPosX(0);
        ImGui::SetCursorPosY(eyeHeights[0]);
        ImGui::SetNextItemWidth(200);
        ImGui::Selectable("  standing");
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
        {
            eyeHeights[0] = ImGui::GetMousePos().y - 15 - menuHeight;
        }
        ImGui::Separator();

        ImGui::SetCursorPosY(eyeHeights[1]);
        ImGui::Selectable("  kneeling");
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
        {
            eyeHeights[1] = ImGui::GetMousePos().y - 15 - menuHeight;
        }
        ImGui::Separator();

        ImGui::SetCursorPosY(eyeHeights[2]);
        ImGui::Selectable("  sitting");
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
        {
            eyeHeights[2] = ImGui::GetMousePos().y - 15 - menuHeight;
        }
        ImGui::Separator();

        ImGui::SetCursorPosY(eyeHeights[3]);
        ImGui::Selectable("  prone");
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
        {
            eyeHeights[3] = ImGui::GetMousePos().y - 15 - menuHeight;
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
        serialize(archive, 101);
    }
}
void _setup::save()
{
    std::ofstream os("screenInfo.json");
    cereal::JSONOutputArchive archive(os);
    serialize(archive, 101);
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
    hasChanged = false;
}


float2 ballisticsSetup::offset(int _lane)
{
    return screen_offsets[currentAmmo][_lane];
}


float2 ballisticsSetup::adjustOffset(int _lane, float2 error)
{
    hasChanged = true;
    screen_offsets[currentAmmo][_lane] += error;
    return screen_offsets[currentAmmo][_lane];
}

void ballisticsSetup::clearOffsets(int _lane)
{
    hasChanged = true;
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
    ImGui::Combo("###ammoSelector", (int*)&currentAmmo, " 9 mm\0 5.56 mm\0 7.62 mm\0");
}






void laneAirEnable::load()
{
    if (std::filesystem::exists("air_toggle.json"))
    {
        std::ifstream is("air_toggle.json");
        cereal::JSONInputArchive archive(is);
        serialize(archive, 100);
    }
}

void laneAirEnable::save()
{
    std::ofstream os("air_toggle.json");
    cereal::JSONOutputArchive archive(os);
    serialize(archive, 100);
}






void _target::loadimage(std::string _root)
{
    image = Texture::createFromFile(_root + texturePath, true, true);
}

void _target::loadimageDialog()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"png"}, {"jpg"}, {"dds"}, {"bmp"} };
    if (openFileDialog(filters, path))
    {
        texturePath = path.filename().string();
        loadimage(path.parent_path().string() + "/targets/");
    }
}


void _target::loadscoreimage(std::string _root)
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

void _target::loadscoreimageDialog()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"png"}, {"jpg"}, {"dds"}, {"bmp"} };
    if (openFileDialog(filters, path))
    {
        scorePath = path.filename().string();
        loadscoreimage(path.parent_path().string() + "/");
    }
}

void _target::load(std::filesystem::path _path)
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

void _target::save(std::filesystem::path _path)
{
    std::ofstream os(_path);
    if (os.good()) {
        cereal::XMLOutputArchive archive(os);
        serialize(archive, 101);
    }
}


void _target::renderGui(Gui* _gui, Gui::Window& _window)
{
    //bool open = true;

    //if (showGui)
    {
        ImGui::PushFont(_gui->getFont("roboto_48_bold"));
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

            ImGui::Checkbox("drop when hit", &dropWhenHit);

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


void targetAction::renderIntroGui(Gui* _gui, int rounds)
{
    switch (action)
    {
    case 0: ImGui::Text("Static"); break;
    case 1: ImGui::Text("Popup"); break;
    case 2: ImGui::Text("Move"); break;
    case 3: ImGui::Text("Sighting"); break;
    }

    ImGui::NewLine();

    ImGui::Text("%d  X", repeats);


    ImGui::NewLine();
    ImGui::SameLine(0, 50);
    ImGui::Text("%d  rounds", rounds);

    if (action == 1 || action == 2)
    {
        ImGui::NewLine();
        ImGui::SameLine(0, 50);
        ImGui::Text("%d  seconds up", (int)upTime);

        ImGui::NewLine();
        ImGui::SameLine(0, 50);
        ImGui::Text("%d  seconds down", (int)downTime);

        if (playWhistle) {
            ImGui::NewLine();
            ImGui::SameLine(0, 50);
            ImGui::Text("whistle");
        }
    }

}


void targetAction::renderGui(Gui* _gui)
{
    ImGui::SetNextItemWidth(390);
    ImGui::Combo("###actionSelector", (int*)&action, "static\0popup\0move\0sight");

    ImGui::Checkbox("drop", &dropWhenHit);
    TOOLTIP("Does the target drop down when hit");

    ImGui::Checkbox("whistle", &playWhistle);
    TOOLTIP("Play a whistle sound at the start of each round");

    ImGui::NewLine();
    ImGui::SetNextItemWidth(110);
    ImGui::DragInt("repeats", &repeats, 1, 1, 15);
    //if (repeats > 1)
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









void _targetList::load()
{
    std::ifstream is(filename);
    if (is.good()) {
        cereal::XMLInputArchive archive(is);
        serialize(archive, 100);
    }
}

void _targetList::save(char* filename)
{
    std::ofstream os(filename);
    if (os.good()) {
        cereal::XMLOutputArchive archive(os);
        serialize(archive, 100);
    }
}






bool exercise::renderTargetPopup(Gui* _gui, Gui::Window& _window)
{
    int cnt = 0;
    float line = 0;
    for (auto& T : Kolskoot::targetList)
    {
        int x = cnt * 200;

        ImGui::SetCursorPos(ImVec2((float)x, line * 280));
        ImGui::Text(T.title.c_str());
        ImGui::SetCursorPos(ImVec2((float)x, line * 280 + 20));
        if(_window.imageButton(T.title.c_str(), T.image, float2(150, 100)))
        //if (ImGui::Button(T.title.c_str(), ImVec2(200, 0)))     // Just width acurate and large heigth t
        {
            target = T;
            targetPopupOpen = false;
            return true;
        }

        cnt++;
        cnt = cnt % 4;
        if (cnt == 0)
        {
            line++;
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
    //if (targets.targets.size() > 0 && targets.targets[0].t.image)
    if (target.image)
    {
        if (_window.imageButton("scoreImage", target.image, float2(150, 100)))
        //if (_window.imageButton("scoreImage", targets.targets[0].t.image, float2(150, 100)))     // Just width acurate and large heigth t
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
        ImGui::PushFont(_gui->getFont("roboto_20"));
        {
            if (renderTargetPopup(_gui, _window))
            {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::PopFont();
        
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



void exercise::renderIntroGui(Gui* _gui, Gui::Window& _window, ImVec2 _offset, ImVec2 _size)
{
    float space = _size.x * 0.02f;
    float w = (_size.x - (4 * space)) / 3.f;
    float h = _size.y * 0.7f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 1, 0, 255));
    {
        ImGui::SetCursorPos(ImVec2(space, _size.y * 0.15f));
        ImGui::BeginChildFrame(100, ImVec2(w, h));
        {
            float ratio = target.size.x / target.size.y;
            float targetHeight = h * 0.6f;
            float targetWidth = w * 0.8f;
            float hw = targetHeight * ratio;
            targetWidth = __min(targetWidth, hw);
            targetHeight = targetWidth / ratio;
            float left = (w - targetWidth) * 0.5f;
            {
                ImGui::SetCursorPos(ImVec2(left, 50.f));
                _window.imageButton("scoreImage", target.image, float2(targetWidth, targetHeight));
            }

            ImGui::NewLine();
            ImGui::NewLine();
            ImGui::SameLine(left, 0);
            switch (pose)
            {
            case 0: ImGui::Text("Standing");  break;
            case 1: ImGui::Text("Kneeling");  break;
            case 2: ImGui::Text("Sitting");  break;
            case 3: ImGui::Text("Prone");  break;
            }

            ImGui::NewLine();
            ImGui::SameLine(left, 0);
            ImGui::Text("%d m", (int)targetDistance);
        }
        ImGui::EndChildFrame();




        ImGui::SetCursorPos(ImVec2(w + space * 2, _size.y * 0.15f));
        ImGui::BeginChildFrame(101, ImVec2(w, h));
        {
            ImGui::SetCursorPos(ImVec2(w * 0.1f, 50.f));
            action.renderIntroGui(_gui, numRounds);
        }
        ImGui::EndChildFrame();




        ImGui::SetCursorPos(ImVec2(w * 2 + space * 3, _size.y * 0.15f));
        ImGui::BeginChildFrame(102, ImVec2(w, h));
        {
            ImGui::SetCursorPos(ImVec2(w * 0.1f, 50.f));
            _window.image("ammo", Kolskoot::ammoType[Kolskoot::ballistics.currentAmmo], float2(w * 0.8f, h * 0.8f), false);
        }
        ImGui::EndChildFrame();
    }
    ImGui::PopStyleColor();
}





void quickRange::loadPath(std::filesystem::path _root)
{
    std::ifstream is(_root);
    if (is.good()) {
        cereal::XMLInputArchive archive(is);
        serialize(archive, 100);
        title = _root.filename().string().substr(0, _root.filename().string().length() - 15);
    }
}



void quickRange::load(std::filesystem::path _root)
{
    std::filesystem::path path = _root;
    FileDialogFilterVec filters = { {"exercises.json"} };
    if (openFileDialog(filters, path))
    {
        std::ifstream is(path);
        if (is.good()) {
            cereal::XMLInputArchive archive(is);
            serialize(archive, 100);
            title = path.filename().string().substr(0, path.filename().string().length() - 15);
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

void quickRange::play()
{
    requestLive = true;
    currentExercise = 0;
    currentStage = live_intro;
    score.create(Kolskoot::setup.numLanes, exercises.size());      // FIXME needs numlanes
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
                ImGui::Text(title.c_str());

                ImGui::SameLine(900, 0);
                if (ImGui::Button("load")) {
                    load(Kolskoot::setup.dataFolder + "/exercises/");
                }


                ImGui::SameLine(0, 40);
                if (ImGui::Button("save")) {
                    save();
                }

                ImGui::SameLine(0, 100);
                ImGui::SetNextItemWidth(100);
                int numX = exercises.size();
                int numCurrent = numX;
                if (ImGui::DragInt("# exercises", &numX, 0.04f, 0, 10))
                {
                    exercises.resize(numX);

                    for (int n = numCurrent; n < numX; n++)
                    {
                        exercises[n].target = Kolskoot::targetList.front();
                    }
                    
                    /*for (auto& E : exercises)
                    {
                        if (E.target.maxScore == 0)
                        {
                            E.target = Kolskoot::targetList[0];
                        }
                    }*/
                }

                if (exercises.size() > 0)
                {
                    ImGui::SameLine(0, 200);
                    if (ImGui::Button("Test")) {
                        play();
                    }
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
    if (currentExercise >= exercises.size())  return;

    ImGui::SameLine(Kolskoot::setup.screen_pixelsX * 0.25, 0);
    ImGui::Text("%s", title.c_str());

    ImGui::SameLine(Kolskoot::setup.screen_pixelsX * 0.5, 0);
    ImGui::PushFont(_gui->getFont("roboto_20"));
    ImGui::SetCursorPosY(10);
    ImGui::Text("Exercise", currentExercise + 1, exercises.size());
    ImGui::PopFont();
    ImGui::SetCursorPosY(0);
    ImGui::Text("%d / %d", currentExercise + 1, exercises.size());

    ImGui::SameLine(0, 100);
    ImGui::Text("%s", exercises[currentExercise].title.c_str());
}



void quickRange::renderTarget(Gui* _gui, Gui::Window& _window, Texture::SharedPtr _bulletHole, int _lane)
{
    auto& Ex = exercises[currentExercise];
    auto& target = Ex.target;

    static float hscale[15] = { 0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f };
    if (actionPhase == 0)
    {
        hscale[_lane] += 0.05f;
        hscale[_lane] = __min(1, hscale[_lane]);
    }
    else
    {
        hscale[_lane] -= 0.05f;
        hscale[_lane] = __max(0.02f, hscale[_lane]);
    }

    // drop when hit
    if (target.dropWhenHit && score.lane_exercise.at(_lane).at(currentExercise).score > 0)
    {
        hscale[_lane] = 0.02f;
    }

    if (Ex.action.dropWhenHit && score.lane_exercise.at(_lane).at(currentExercise).score > 0)
    {
        hscale[_lane] = 0.02f;
    }

    // drop when shots done
    if (Ex.action.dropWhenHit && getRoundsLeft(_lane) == 0)
    {
        // FIXEM can do with a delay and animation
        // OK this does not work because we multiply out rounds
        //hscale[_lane] = 0.02f;
    }

    float2 pixSize = target.size * Kolskoot::setup.meterToPixels(Ex.targetDistance);
    
    float2 topleft = Kolskoot::setup.laneCenter(_lane, Ex.pose) - (pixSize * 0.5f);
    topleft.y += (1.f - hscale[_lane]) * pixSize.y;
    pixSize.y *= hscale[_lane];

    ImGui::SetCursorPos(ImVec2(topleft.x, topleft.y));
    _window.image("testImage", target.image, pixSize, false);

    for (int s = 0; s < score.lane_exercise.at(_lane).at(currentExercise).shots.size(); s++)
    {
        float2 pos = score.lane_exercise.at(_lane).at(currentExercise).shots.at(s).position_relative;
        float2 screenPos = (pos - 0.5f) * pixSize + Kolskoot::setup.laneCenter(_lane, Ex.pose);
        float bulletSize = Kolskoot::setup.meterToPixels(Ex.targetDistance) * 0.03f;  // exagerated 3cm but that makes teh hole about 1cm
        bulletSize = __max(bulletSize, 15);

        ImGui::SetCursorPos(ImVec2(screenPos.x - (bulletSize / 2), screenPos.y - (bulletSize / 2)));
        _window.image("Shot", _bulletHole, float2(bulletSize, bulletSize * hscale[_lane]), false);
    }

}



float quickRange::renderScope(Gui* _gui, Gui::Window& _window, Texture::SharedPtr _bulletHole, int _lane, float _size, float _y, bool _inst)
{
    auto& Ex = exercises[currentExercise];
    auto& target = Ex.target;

    // fixed height of 200 pixels
    float2 pixSize = target.size * (_size / target.size.y) * 0.95f;
    float2 topleft = float2(10, 10);// Kolskoot::setupInfo.laneCenter(_lane, Ex.pose);
    if (_inst)
    {
        pixSize = target.size * (_size / target.size.x) * 0.95f;
        topleft = float2(10, 10);
    }
    //topleft.y = _y;
    //topleft -= (pixSize * 0.5f);
    float2 center = topleft + (pixSize * 0.5f);

    ImGui::SetCursorPos(ImVec2(topleft.x, topleft.y));
    _window.image("testImage", target.image, pixSize, false);

    for (int s = 0; s < score.lane_exercise.at(_lane).at(currentExercise).shots.size(); s++)
    {
        float2 pos = score.lane_exercise.at(_lane).at(currentExercise).shots.at(s).position_relative;
        float2 screenPos = center;
        screenPos += (pos - 0.5f) * pixSize;
        float bulletSize = __max(20, _size / 20.f);

        ImGui::SetCursorPos(ImVec2(screenPos.x - (bulletSize / 2), screenPos.y - (bulletSize / 2)));
        _window.image("Shot", _bulletHole, float2(bulletSize, bulletSize), false);
    }

    return pixSize.x * 0.5f;
}


void quickRange::renderLiveInstructor(Gui* _gui, Gui::Window& _window, Texture::SharedPtr _bulletHole)
{
    if (currentExercise >= exercises.size())  return;

    auto& Ex = exercises[currentExercise];
    auto& target = Ex.target;

    ImGui::PushFont(_gui->getFont("roboto_48_bold"));
    {
        switch (currentStage)
        {
        case live_intro:
            for (int lane = 0; lane < Kolskoot::setup.numLanes; lane++)
            {
                float laneWidth = (float)Kolskoot::setup.instructorSize.x / Kolskoot::setup.numLanes;
                float laneLeft = laneWidth * lane;
                float laneRight = laneLeft + laneWidth;

                char AirName[256];
                sprintf(AirName, "##air_%d", lane);
                ImGui::SetCursorPos(ImVec2(laneLeft + 5, 5));
                ImGui::Checkbox(AirName, &Kolskoot::airToggle.air[Kolskoot::ballistics.currentAmmo][lane]);

                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(200, 200, 0, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
                {
                    ImGui::SetCursorPos(ImVec2(laneLeft + laneWidth * 0.5 - 40, 5));
                    ImGui::BeginChildFrame(33 + lane, ImVec2(80.f, 60.f));
                    {
                        ImGui::SetCursorPos(ImVec2(30, 3));
                        if (lane >= 9) ImGui::SetCursorPos(ImVec2(15, 3));
                        ImGui::Text("%d", lane + 1);
                    }
                    ImGui::EndChildFrame();
                }
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();
            }
            Ex.renderIntroGui(_gui, _window, ImVec2((float)Kolskoot::setup.instructorOffet.x, (float)Kolskoot::setup.instructorOffet.y), ImVec2((float)Kolskoot::setup.instructorSize.x, (float)Kolskoot::setup.instructorSize.y));
            break;

        case live_live:
        case live_scores:
            ImGui::BeginColumns("lanes", Kolskoot::setup.numLanes);
            for (int lane = 0; lane < Kolskoot::setup.numLanes; lane++)
            {
                float laneWidth = (float)Kolskoot::setup.instructorSize.x / Kolskoot::setup.numLanes;
                float laneLeft = laneWidth * lane;
                float laneRight = laneLeft + laneWidth;

                char AirName[256];
                sprintf(AirName, "##air_%d", lane);
                ImGui::SetCursorPos(ImVec2(laneLeft + 5, 5));
                ImGui::Checkbox(AirName, &Kolskoot::airToggle.air[Kolskoot::ballistics.currentAmmo][lane]);

                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(200, 200, 0, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
                {
                    ImGui::SetCursorPos(ImVec2(laneLeft + laneWidth * 0.5 - 40, 5));
                    ImGui::BeginChildFrame(33 + lane, ImVec2(80.f, 60.f));
                    {
                        ImGui::SetCursorPos(ImVec2(30, 3));
                        if (lane >= 9) ImGui::SetCursorPos(ImVec2(15, 3));
                        ImGui::Text("%d", lane + 1);
                    }
                    ImGui::EndChildFrame();
                }
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();


                if (Kolskoot::airToggle.air[Kolskoot::ballistics.currentAmmo][lane])
                {
                    float scopeWidth = laneWidth;// *0.95;
                    float scopeHeight = scopeWidth * 2.0;
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(20, 20, 20, 30));
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 100, 100, 255));
                    {
                        ImGui::SetCursorPos(ImVec2(laneLeft + laneWidth * 0.5 - scopeWidth * 0.5, 200));
                        ImGui::BeginChildFrame(1933 + lane, ImVec2(scopeWidth, scopeHeight));
                        {
                            float w = renderScope(_gui, _window, _bulletHole, lane, scopeWidth, 10, true);

                            //ImGui::SetCursorPos(ImVec2(scopeWidth - 150, 54));
                            ImGui::SetCursorPos(ImVec2(10, scopeHeight - 80));
                            ImGui::Text("%d/%d", score.lane_exercise.at(lane).at(currentExercise).score, target.maxScore * Ex.numRounds);
                        }
                        ImGui::EndChildFrame();
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopStyleColor();
                }

                ImGui::SetCursorPos(ImVec2(laneLeft, Kolskoot::setup.screen_pixelsY - menuHeight - 80));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
                ImGui::Text(".");   // just somethign to force teh down lines
                ImGui::PopStyleColor();

                ImGui::NextColumn();
            }
            ImGui::EndColumns();
            break;

        }
    }
    ImGui::PopFont();

}



void quickRange::renderLive(Gui* _gui, Gui::Window& _window, Texture::SharedPtr _bulletHole)
{
    if (currentExercise >= exercises.size())  return;

    auto& Ex = exercises[currentExercise];
    auto& target = Ex.target;



    ImGui::PushFont(_gui->getFont("roboto_48_bold"));
    {
        switch (currentStage)
        {
        case live_intro:
        {
            float x0 = (float)Kolskoot::setup.screen_pixelsX / Kolskoot::setup.num3DScreens;
            float start = 0;
            for (int i = 0; i < Kolskoot::setup.num3DScreens; i++)
            {
                ImGui::SetCursorPos(ImVec2(i * x0, 0));
                ImGui::BeginChildFrame(123 + i, ImVec2(x0, Kolskoot::setup.screen_pixelsY));
                {
                    Ex.renderIntroGui(_gui, _window, ImVec2((float)Kolskoot::setup.XOffset3D + i * x0, 0), ImVec2(x0, (float)Kolskoot::setup.screen_pixelsY));
                }
                ImGui::EndChildFrame();
            }
        }
        break;

        case live_live:
        {

            float screenWidth = Kolskoot::setup.screen_pixelsX / Kolskoot::setup.num3DScreens;
            ImGui::SetCursorPos(ImVec2(0, Kolskoot::setup.eyeHeights[Ex.pose] - (screenWidth / 2)));
            _window.image("logo", backdrop, float2(screenWidth, screenWidth), false);

            if (Kolskoot::setup.num3DScreens > 1)
            {
                ImGui::SetCursorPos(ImVec2(2560, Kolskoot::setup.eyeHeights[Ex.pose] - 1280));
                _window.image("logo", backdrop, float2(screenWidth, screenWidth), false);
            }


            ImGui::BeginColumns("lanes", Kolskoot::setup.numLanes);
            for (int lane = 0; lane < Kolskoot::setup.numLanes; lane++)
            {
                float laneWidth = Kolskoot::setup.screen_pixelsX / Kolskoot::setup.numLanes;
                float laneLeft = laneWidth * lane;
                float laneRight = laneLeft + laneWidth;

                char AirName[256];
                sprintf(AirName, "##air_%d", lane);
                ImGui::Checkbox(AirName, &Kolskoot::airToggle.air[Kolskoot::ballistics.currentAmmo][lane]);


                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(200, 200, 0, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
                {
                    ImGui::SetCursorPos(ImVec2(laneLeft + laneWidth * 0.5 - 40, 5));
                    ImGui::BeginChildFrame(33 + lane, ImVec2(80.f, 60.f));
                    {
                        if (lane < 9) ImGui::SetCursorPos(ImVec2(30, 3));
                        else ImGui::SetCursorPos(ImVec2(15, 3));
                        ImGui::Text("%d", lane + 1);
                    }
                    ImGui::EndChildFrame();
                }
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();

                ImGui::SameLine();



                if (Kolskoot::airToggle.air[Kolskoot::ballistics.currentAmmo][lane])
                {

                    if (Ex.action.action == action_adjust)
                    {
                        ImGui::PushFont(_gui->getFont("roboto_48"));
                        ImGui::Text("offset : %d, %d px", (int)(Kolskoot::ballistics.offset(lane).x), (int)(Kolskoot::ballistics.offset(lane).y));
                        ImGui::PopFont();
                    }

                    renderTarget(_gui, _window, _bulletHole, lane);         // Draw the target


                    // scope #######################################################################################
                    float scopeWidth = __min(400, laneWidth - 20);
                    float scopeHeight = scopeWidth * 0.6;
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(20, 20, 20, 255));
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 100, 100, 255));
                    {
                        if (Kolskoot::setup.eyeHeights[Ex.pose] < Kolskoot::setup.screen_pixelsY * 0.5f)
                        {
                            ImGui::SetCursorPos(ImVec2(laneLeft + laneWidth * 0.5 - scopeWidth * 0.5, Kolskoot::setup.screen_pixelsY - 100 - scopeHeight));
                            ImGui::BeginChildFrame(933 + lane, ImVec2(scopeWidth, scopeHeight));
                        }
                        else
                        {
                            ImGui::SetCursorPos(ImVec2(laneLeft + laneWidth * 0.5 - scopeWidth * 0.5, 100));
                            ImGui::BeginChildFrame(933 + lane, ImVec2(scopeWidth, scopeHeight));
                        }


                        {
                            float w = renderScope(_gui, _window, _bulletHole, lane, scopeHeight - 20, 10);



                            ImGui::PushFont(_gui->getFont("roboto_32"));
                            {
                                ImGui::SetCursorPos(ImVec2(scopeWidth - 170, 54));
                                ImGui::Text("shots %d / %d", score.lane_exercise.at(lane).at(currentExercise).shots.size(), Ex.numRounds);

                            }
                            ImGui::PopFont();

                            ImGui::SetCursorPos(ImVec2(scopeWidth - 170, 154));
                            ImGui::Text("%d/%d", score.lane_exercise.at(lane).at(currentExercise).score, target.maxScore * Ex.numRounds);


                        }
                        ImGui::EndChildFrame();
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopStyleColor();



                    adjustBoresight(lane);                                  // test if we are adjusting boresigth and do it
                }

                ImGui::SetCursorPos(ImVec2(laneLeft, Kolskoot::setup.screen_pixelsY - menuHeight - 80));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
                ImGui::Text(".");   // just somethign to force teh down lines
                ImGui::PopStyleColor();

                ImGui::NextColumn();
            }
            ImGui::EndColumns();
        }
        break;

        case live_scores:
            ImGui::BeginColumns("lanes", Kolskoot::setup.numLanes);
            for (int lane = 0; lane < Kolskoot::setup.numLanes; lane++)
            {
                float laneWidth = Kolskoot::setup.screen_pixelsX / Kolskoot::setup.numLanes;
                float laneLeft = laneWidth * lane;
                float laneRight = laneLeft + laneWidth;


                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(200, 200, 0, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
                {
                    ImGui::SetCursorPos(ImVec2(laneLeft + laneWidth * 0.5 - 30, 5));
                    ImGui::BeginChildFrame(33 + lane, ImVec2(60.f, 60.f));
                    {
                        ImGui::SetCursorPos(ImVec2(15, -3));
                        ImGui::Text("%d", lane + 1);
                    }
                    ImGui::EndChildFrame();
                }
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();


                if (Kolskoot::airToggle.air[Kolskoot::ballistics.currentAmmo][lane])
                {
                    float scopeWidth = laneWidth - 10;// *0.6;
                    float scopeHeight = scopeWidth * 2.0;
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(20, 20, 20, 30));
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 100, 100, 255));
                    {
                        ImGui::SetCursorPos(ImVec2(laneLeft + laneWidth * 0.5 - scopeWidth * 0.5, 200));
                        ImGui::BeginChildFrame(1933 + lane, ImVec2(scopeWidth, scopeHeight));
                        {
                            float w = renderScope(_gui, _window, _bulletHole, lane, scopeWidth, 10, true);

                            ImGui::SetCursorPos(ImVec2(10, scopeHeight-80));
                            ImGui::Text("%d/%d", score.lane_exercise.at(lane).at(currentExercise).score, target.maxScore * Ex.numRounds);
                        }
                        ImGui::EndChildFrame();
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopStyleColor();
                }

                ImGui::SetCursorPos(ImVec2(laneLeft, Kolskoot::setup.screen_pixelsY - menuHeight - 80));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
                ImGui::Text(".");   // just somethign to force teh down lines
                ImGui::PopStyleColor();

                ImGui::NextColumn();
            }
            ImGui::EndColumns();
            break;

        }
    }
    ImGui::PopFont();
}



void quickRange::adjustBoresight(int _lane)
{
    auto& Ex = exercises[currentExercise];
    auto& shots = score.lane_exercise.at(_lane).at(currentExercise).shots;

    if (Ex.action.action == action_adjust && (shots.size() >= Ex.numRounds))
    {
        float2 avs = float2(0, 0);
        for (auto& s : shots)
        {
            avs += s.position;
        }
        avs /= shots.size();

        Kolskoot::ballistics.adjustOffset(_lane, avs);
        shots.clear();
    }
}



void quickRange::mouseShot(float x, float y, _setup setup)
{
    auto& Ex = exercises[currentExercise];
    auto& target = Ex.target;
    int lane = (int)floor(x * setup.numLanes / setup.screen_pixelsX);

    x -= Kolskoot::ballistics.offset(lane).x;
    y -= Kolskoot::ballistics.offset(lane).y;

    // move all of this to target
    float tw = 0.001f * target.size_mm.x / setup.screenWidth * setup.screen_pixelsX * setup.shootingDistance / Ex.targetDistance;
    float th = tw * target.size_mm.y / target.size_mm.x;
    float px = (setup.screen_pixelsX / setup.numLanes / 2) - (tw / 2) + (setup.screen_pixelsX / setup.numLanes * lane);
    float py = (setup.eyeHeights[Ex.pose]) - (th * 0.5f);

    float shotX = (x - px) / tw;    // 0-1 texture space
    float shotY = (y - py) / th;

    float absX = (shotX - 0.5f) * tw;    // meters relativeto center
    float absY = (shotY - 0.5f) * th;

    // translate to scoring space
    int scoreX = (int)(shotX * target.scoreWidth);
    int scoreY = (int)(shotY * target.scoreHeight);
    int shotScore = 0;
    if (scoreX >= 0 && scoreX < target.scoreWidth &&
        scoreY >= 0 && scoreY < target.scoreHeight)
    {
        int scoreIndex = (scoreY * target.scoreWidth) + scoreX;
        shotScore = target.scoreData[scoreIndex];
    }

    auto& scoreTarget = score.lane_exercise.at(lane).at(currentExercise);
    auto& shot = scoreTarget.shots;
    scoreTarget.score += shotScore;
    scoreTarget.target = target;
    shot.emplace_back();
    shot.back().score = shotScore;
    shot.back().position = float2(absX, absY);
    shot.back().position_relative = float2(shotX, shotY);
}



bool quickRange::liveNext()
{
    auto& Ex = exercises[currentExercise];

    switch (currentStage) {
    case live_intro:
        currentStage = live_live;
        bulletDrop = Kolskoot::ballistics.bulletDrop(Ex.targetDistance);
        actionRepeats = -1;
        actionCounter = Ex.action.startTime;
        actionPhase = 1;
        break;

    case live_live:
        currentStage = live_scores;
        bulletDrop = Kolskoot::ballistics.bulletDrop(Ex.targetDistance);    // just incase first one misses
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



void quickRange::updateLive(float _dT)
{
    actionCounter -= _dT;

    if (actionCounter < 0)
    {
        actionPhase = (actionPhase + 1) % 2;

        switch (actionPhase)
        {
        case 0:
        {
            actionRepeats++;
            if (actionRepeats < exercises[currentExercise].action.repeats)
            {
                actionCounter = exercises[currentExercise].action.upTime;
                if (exercises[currentExercise].action.action == action_static || exercises[currentExercise].action.action == action_adjust)
                {
                    actionCounter = 100000; /// just REALLy big
                }
                // show target

                if (exercises[currentExercise].action.playWhistle)
                {
                    std::string filename = Kolskoot::setup.dataFolder + "/sounds/wistle.wav";
                    PlaySoundA((LPCSTR)(filename.c_str()), NULL, SND_FILENAME | SND_ASYNC);
                }
            }
            else
            {
                // we are done
                liveNext();
            }
        }
        break;

        case 1:
        {
            actionCounter = exercises[currentExercise].action.downTime;
            // hide target
        }
        break;
        }
    }
}




void quickRange::print()
{
    HPDF_Doc  pdf;
    HPDF_Page page;
    HPDF_Font pdf_font;
    float pageWidth, pageHeight;
    float x, y;

    pdf = HPDF_New(error_handler, NULL);
    pdf_font = HPDF_GetFont(pdf, "Candara", NULL);
    page = HPDF_AddPage(pdf);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    pageHeight = HPDF_Page_GetHeight(page);
    pageWidth = HPDF_Page_GetWidth(page);

    x = 20;
    y = 60;

    pdf_Line(page, pageWidth / 2, 90, pageWidth / 2, pageHeight - 70);
    pdf_Text(page, pdf_font, x, y, "test", 10, align_LEFT);

    HPDF_SaveToFile(pdf, (Kolskoot::setup.dataFolder + "/printing/test.pdf").c_str());
    //system("lpr filename");
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






std::string menu::clean(const std::string _s)
{
    std::string str = _s;
    size_t start_pos = 0;
    while ((start_pos = str.find("\\", start_pos)) != std::string::npos) {
        str.replace(start_pos, 1, "/");
        start_pos += 1;
    }
    return str;
}

void menu::load()
{
    items.clear();

    std::filesystem::path fullPath = Kolskoot::setup.dataFolder + "/exercises/";
    int  baseLength = fullPath.string().length();

    for (const auto& entry : std::filesystem::recursive_directory_iterator(fullPath))
    {
        if (!entry.is_directory())
        {
            items.emplace_back();
            items.back().fullPath = clean(entry.path().string());

            items.back().path = items.back().fullPath.substr(0, items.back().fullPath.find_last_of("\\/")); // remove filename
            if (items.back().path.length() > baseLength)
            {
                items.back().path = items.back().path.substr(baseLength);   // remove frnt
            }
            else
            {
                items.back().path = "";
            }

            items.back().name = items.back().fullPath.substr(items.back().fullPath.find_last_of("\\/") + 1);
            items.back().name = items.back().name.substr(0, items.back().name.length() - 15);
        }
    }
}


void menu::renderGui(Gui* _gui, Gui::Window& _window)
{
    std::string oldPath = "";

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(80, 80, 80, 255));
    {
        ImGui::PushFont(_gui->getFont("roboto_48"));
        {
            ImGui::BeginColumns("test", 5);

            for (auto& I : items)
            {
                if (I.path != oldPath)
                {
                    oldPath = I.path;
                    ImGui::NewLine();
                    ImGui::Text(I.path.c_str());
                }

                ImGui::NewLine();
                ImGui::SameLine(0, 50);
                ImVec2 size = ImVec2(300, 0);
                if (ImGui::Selectable(I.name.c_str()))
                {
                    Kolskoot::QR.loadPath(I.fullPath);
                    Kolskoot::QR.play();
                }

                if (ImGui::GetCursorPosY() > Kolskoot::setup.screen_pixelsY - 80)
                {
                    ImGui::NextColumn();
                }
            }
        }
        ImGui::PopFont();
    }
    ImGui::PopStyleColor();
}










bool sort_x(glm::vec4& a, glm::vec4& b) {
    return (a.x < b.x);
}
bool sort_y(glm::vec4& a, glm::vec4& b) {
    return (a.y < b.y);
}

void videoToScreen::build(std::array<glm::vec4, 45>  dots, float2 screenSize, float offsetX)
{
    std::sort(dots.begin(), dots.end(), sort_y);

    std::sort(dots.begin() + 0, dots.begin() + 9, sort_x);
    std::sort(dots.begin() + 9, dots.begin() + 18, sort_x);
    std::sort(dots.begin() + 18, dots.begin() + 27, sort_x);
    std::sort(dots.begin() + 27, dots.begin() + 36, sort_x);
    std::sort(dots.begin() + 36, dots.begin() + 45, sort_x);

    /*
    * float x0 = setup.screen_pixelsX / setup.num3DScreens;
        ImGui::SetWindowPos(ImVec2(x0 * _screen + setup.XOffset3D, 0), 0);
        ImGui::SetWindowSize(ImVec2(x0, screenSize.y), 0);

        ImGui::PushFont(_gui->getFont("roboto_32"));
        {
            if (modeCalibrate == 1) {

                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                static ImVec4 col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                const ImU32 col32 = ImColor(col);

                float dY = (screenSize.y - 40) / 4;
                float dX = (x0 - 40) / 8;
                float left = x0 * _screen + setup.XOffset3D;
    */
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

            grid[y][x].screenPos[0] = glm::vec3(offsetX + 20 + x * dX, 20 + y * dY, 0);
            grid[y][x].screenPos[1] = glm::vec3(offsetX + 20 + (x + 1) * dX, 20 + y * dY, 0);
            grid[y][x].screenPos[2] = glm::vec3(offsetX + 20 + (x + 1) * dX, 20 + (y + 1) * dY, 0);
            grid[y][x].screenPos[3] = glm::vec3(offsetX + 20 + x * dX, 20 + (y + 1) * dY, 0);
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
    FALCOR_PROFILE("onGuiMenubar");

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 16));
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::SetCursorPosX((float)setup.instructorOffet.x);

        {
            if (ImGui::Button("Menu", ImVec2(100, 0)))
            {
                guiMode = gui_menu;
                zigbeeRounds(0, 0, true);		// turn all air off
            }
            TOOLTIP("'esc'");
        }


        if (guiMode == gui_live)
        {
            //ImGui::SetCursorPos(ImVec2(screenSize.x - 250, screenSize.y - 120));
            ImGui::SameLine(0, 50);
            if (ImGui::Button("Next >", ImVec2(150, 0)))
            {
                if (QR.liveNext())
                {
                    guiMode = gui_menu;
                    zigbeeRounds(0, 0, true);		// turn all air off
                }
            }
            TOOLTIP("'space-bar'");

            QR.renderLiveMenubar(_gui);

            ImGui::SameLine(800, 0);
        }
        else
        {
            bool selected = false;
            //ImGui::SameLine(0, 20);
            //ImGui::Text("Kolskoot");

            {
                ImGui::SameLine(0, 50);
                ImGui::SetNextItemWidth(250);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
                ballistics.renderGuiAmmo(_gui);
                ImGui::PopStyleColor();
            }

            ImGui::SameLine(0, 200);
            ImGui::SetNextItemWidth(150);

            ImGui::SetNextItemWidth(150);
            if (ImGui::BeginMenu("Settings"))
            {
                if (ImGui::MenuItem("Camera"))
                {
                    guiMode = gui_camera;
                    modeCalibrate = 0;
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

            ImGui::SameLine(0, 80);
            ImGui::Selectable("Exercise builder", &selected, 0, ImVec2(200, 0));
            if (selected) {
                guiMode = gui_exercises;
                selected = false;
                QR.clear();
                QR.exercises.resize(1);
                QR.exercises[0].target = Kolskoot::targetList.front();
            }


            


            if (ballistics.hasChanged)
            {
                ImGui::SameLine(0, 20);
                if (ImGui::Button("Save modified boresight"))
                {
                    ballistics.save();
                }
            }

        }

        ImGui::PushFont(_gui->getFont("roboto_20"));
        {
            ImGui::SetCursorPos(ImVec2((float)(setup.instructorSize.x - 120), 25));
            ImGui::Text("%d fps", (int)(1000.0 / gpFramework->getFrameRate().getAverageFrameTime()));
        }
        ImGui::PopFont();

        ImGui::SetCursorPos(ImVec2((float)(setup.instructorSize.x - 60), 0.f));
        if (ImGui::Button("X", ImVec2(60, 0))) { gpFramework->getWindow()->shutdown(); }

    }
    ImGui::EndMainMenuBar();
    ImGui::PopStyleVar();
}



void Kolskoot::renderBranding(Gui* _gui, Gui::Window& _window, uint2 _size)
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(5, 5, 10, 255));
    float x0 = setup.screen_pixelsX / setup.num3DScreens;
    for (int i = 0; i < setup.num3DScreens; i++)
    {
        ImGui::SetCursorPos(ImVec2(i * x0, 0));
        ImGui::BeginChildFrame(1234 + i, ImVec2(x0, setup.screen_pixelsY));
        {
            ImGui::SetCursorPos(ImVec2((x0 - 1063) * 0.5f, (_size.y - 355) * 0.5f - 100));
            _window.image("logo", kolskootLogo, float2(1063, 355));

            ImGui::SetCursorPos(ImVec2((x0 - 400) * 0.5f, (_size.y - 400.f)));
            _window.image("ammo", Kolskoot::ammoType[Kolskoot::ballistics.currentAmmo], float2(400, 200), false);
        }
        ImGui::EndChildFrame();
    }
    ImGui::PopStyleColor();

}

void Kolskoot::renderTargets(Gui* _gui, Gui::Window& _window, uint2 _size)
{
    float x0 = (float)(_size.x / 2);

    ImGui::SetCursorPosX(0);
    ImGui::BeginChildFrame(3237, ImVec2(x0, (float)_size.y));
    {
        ImGui::PushFont(_gui->getFont("roboto_32"));
        int W = (int)floor(x0 / 240);
        for (int i = 0; i < targetList.size(); i++)
        {
            float x = (float)(i % W);
            float y = (float)(i / W);
            ImGui::SetCursorPos(ImVec2(20 + x * 240, 40 + y * 400));
            ImGui::Text(targetList[i].title.c_str());

            ImGui::SetCursorPos(ImVec2(20 + x * 240, 40 + 32 + y * 400));
            if (_window.imageButton(targetList[i].title.c_str(), targetList[i].image, float2(200, 150)))
            {
                targetBuilder.load(targetList[i].fullPath);
            }
        }
        ImGui::PopFont();
    }
    ImGui::EndChildFrame();

    ImGui::SetCursorPos(ImVec2(x0, 0));
    ImGui::BeginChildFrame(3238, ImVec2(x0, (float)_size.y));
    {
        targetBuilder.renderGui(_gui, _window);
    }
    ImGui::EndChildFrame();

    //menuButton();

}

void Kolskoot::renderExerciseBuilder(Gui* _gui, Gui::Window& _window, uint2 _size)
{
    QR.renderGui(_gui, screenSize, _window);

    if (requestLive)
    {
        requestLive = false;
        Kolskoot::guiMode = gui_live;
    }
    //menuButton();
}


void Kolskoot::renderLiveInstructor(Gui* _gui, Gui::Window& _window, uint2 _size)
{
    QR.renderLiveInstructor(_gui, _window, bulletHole);
    
    ImGui::PushFont(_gui->getFont("roboto_32"));
    {
        //nextButton();
        //menuButton();
    }
    ImGui::PopFont();
    
}


void Kolskoot::renderLive(Gui* _gui, Gui::Window& _window, uint2 _size)
{
    QR.renderLive(_gui, _window, bulletHole);
    /*
    float x0 = setup.screen_pixelsX / setup.num3DScreens;
    for (int i = 0; i < setup.num3DScreens; i++)
    {
        ImGui::SetCursorPos(ImVec2(i * x0, 0));
        ImGui::BeginChildFrame(123 + i, ImVec2(x0, setup.screen_pixelsY));
        {
            QR.renderLive(_gui, _window, bulletHole);
        }
        ImGui::EndChildFrame();
    }
    */
}

void Kolskoot::renderCamera(Gui* _gui, Gui::Window& _window, uint2 _size)
{
    float x0 = setup.screen_pixelsX / setup.num3DScreens;
    for (int i = 0; i < setup.num3DScreens; i++)
    {
        ImGui::SetCursorPos(ImVec2(i * x0, 0));
        ImGui::BeginChildFrame(123 + i, ImVec2(x0, setup.screen_pixelsY - menuHeight));
        {
            if (pointGreyCamera)
            {
                if ((modeCalibrate == 0) || (cameraToCalibratate != i))
                {
                    renderCamera_main(_gui, _window, _size, i);
                }
                else
                {
                    renderCamera_calibrate(_gui, _size, i);
                }
            }
        }
        ImGui::EndChildFrame();
    }
}

void Kolskoot::renderCamera_main(Gui* _gui, Gui::Window& _window, uint2 _size, uint _screen)
{
    ImGui::PushFont(_gui->getFont("roboto_32"));
    {
        if (!pointGreyCamera->isConnected(_screen))
        {
            ImGui::Text("Camera not connected");
        }
        else
        {
            // FIZXME GETSErial is VERY slow save ot somewhere
            ImGui::Text("serial  %d", pointGreyCamera->getSerialNumber(_screen));
            ImGui::Text("%d x %d", (int)pointGreyCamera->bufferSize.x, (int)pointGreyCamera->bufferSize.y);
            
            ImGui::NewLine();

            ImGui::SetNextItemWidth(400);
            if (ImGui::SliderFloat("Gain", &setup.pgGain, 0, 12)) {
                pointGreyCamera->setGain(setup.pgGain);
            }
            ImGui::SetNextItemWidth(400);
            if (ImGui::SliderFloat("Gamma", &setup.pgGamma, 0, 1)) {
                pointGreyCamera->setGamma(setup.pgGamma);
            }

            ImGui::SetNextItemWidth(400);
            ImGui::DragInt("dot min", &setup.dot_min, 1, 1, 200);
            ImGui::SetNextItemWidth(400);
            ImGui::DragInt("dot max", &setup.dot_max, 1, 1, 300);
            ImGui::SetNextItemWidth(400);
            ImGui::DragInt("threshold", &setup.threshold, 1, 1, 100);
            ImGui::SetNextItemWidth(400);
            ImGui::DragInt("m_PG_dot_position", &setup.m_PG_dot_position, 1, 0, 2);
            pointGreyCamera->setDots(setup.dot_min, setup.dot_max, setup.threshold, setup.m_PG_dot_position);

            ImGui::NewLine();
            ImGui::SameLine(350, 0);
            if (ImGui::Button("SAVE")) {
                setup.save();
            }

            ImGui::NewLine();
            
            if (ImGui::Button("Take Reference Image")) {
                pointGreyCamera->setReferenceFrame(true, 5);
            }

            ImGui::SameLine(400, 0);
            if (ImGui::Button("Swap")) {
                uint tmp = setup.cameraSerial[0];
                setup.cameraSerial[0] = setup.cameraSerial[1];
                setup.cameraSerial[1] = tmp;
                pointGreyCamera->setFirstCamera(setup.cameraSerial[0]);
            }

            if (ImGui::Button("Calibrate")) {
                modeCalibrate = 1;
                //guiMode = gui_menu;
                cameraToCalibratate = _screen;
            }

            ImGui::SameLine(400, 0);
            if (ImGui::Button("Test")) {
                modeCalibrate = 3;
                //guiMode = gui_menu;
                cameraToCalibratate = _screen;
            }

            if (pointGreyCamera->m_bSwapCameras) {
                ImGui::SameLine(0, 50);
                ImGui::Text("SWAPPED");
            }


            ImVec2 C = ImGui::GetCursorPos();
            if (pointGreyBuffer[_screen])
            {
                if (_window.imageButton("testImage", pointGreyBuffer[_screen], pointGreyCamera->bufferSize))
                {
                }
            }
            else
            {
                pointGreyBuffer[_screen] = Texture::create2D((int)pointGreyCamera->bufferSize.x, (int)pointGreyCamera->bufferSize.y, ResourceFormat::R8Unorm, 1, 1, pointGreyCamera->bufferData[_screen]);
            }

            C.x += (int)pointGreyCamera->bufferSize.x + 50;
            ImGui::SetCursorPos(C);
            if (pointGreyDiffBuffer[_screen])
            {
                _window.imageButton("testImage", pointGreyDiffBuffer[_screen], pointGreyCamera->bufferSize);
            }
            else
            {
                pointGreyDiffBuffer[_screen] = Texture::create2D((int)pointGreyCamera->bufferSize.x, (int)pointGreyCamera->bufferSize.y, ResourceFormat::R8Unorm, 1, 1, pointGreyCamera->bufferData[_screen]);
            }
        }
    }
    ImGui::PopFont();
}


void Kolskoot::renderCamera_calibrate(Gui* _gui, uint2 _size, uint _screen)
{
    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.8f, 0.8f, 1.f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 1.f);

   
    Gui::Window calibrationPanel(_gui, "##Calibrate", { screenSize.x, screenSize.y }, { 0, 0 }, Gui::WindowFlags::NoResize | Gui::WindowFlags::SetFocus);
    {

        float x0 = setup.screen_pixelsX / setup.num3DScreens;
        ImGui::SetWindowPos(ImVec2(x0 * _screen + setup.XOffset3D, 0), 0);
        ImGui::SetWindowSize(ImVec2(x0, screenSize.y), 0);

        ImGui::PushFont(_gui->getFont("roboto_32"));
        {
            if (modeCalibrate == 1) {

                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                static ImVec4 col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                const ImU32 col32 = ImColor(col);

                float dY = (screenSize.y - 40) / 4;
                float dX = (x0 - 40) / 8;
                float left = x0 * _screen + setup.XOffset3D;
                for (int y = 0; y < 5; y++)
                {
                    for (int x = 0; x < 9; x++)
                    {
                        draw_list->AddCircle(ImVec2(left + 20 + (x * dX), 20 + (y * dY)), 3, col32, 20, 6);
                    }
                }

                static bool Diff = false;
                ImGui::SetCursorPos(ImVec2(100, 70));
                ImGui::Checkbox("show diff", &Diff);
                ImGui::SameLine(0, 50);
                

                ImGui::SetCursorPos(ImVec2(100, 100));
                if (pointGreyBuffer[_screen])
                {
                    if (Diff)
                    {
                        calibrationPanel.imageButton("testImage", pointGreyDiffBuffer[_screen], float2(x0 - 200, screenSize.y - 200), false);
                    }
                    else
                    {
                        calibrationPanel.imageButton("testImage", pointGreyBuffer[_screen], float2(x0 - 200, screenSize.y - 200), false);
                    }
                
                    
                    
                }


                style.Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.8f, 0.8f, 1.f);
                /*
                ImGui::SetCursorPos(ImVec2(50, 50));
                ImGui::Text("Top left");
                ImGui::SetCursorPos(ImVec2(x0 - 200, 50));
                ImGui::Text("Top right");
                ImGui::SetCursorPos(ImVec2(50, screenSize.y - 50));
                ImGui::Text("Bottom left");
                ImGui::SetCursorPos(ImVec2(x0 - 200, screenSize.y - 50));
                ImGui::Text("Bottom right");
                
                ImGui::SetCursorPos(ImVec2(x0 / 2 + 150, screenSize.y / 2 - 60));
                ImGui::Text("remove the filter");
                ImGui::SetCursorPos(ImVec2(x0 / 2 + 150, screenSize.y / 2 - 30));
                ImGui::Text("zoom and focus the camera");
                ImGui::SetCursorPos(ImVec2(x0 / 2 + 150, screenSize.y / 2 - 0));
                ImGui::Text("set the brightness");
                ImGui::SetCursorPos(ImVec2(x0 / 2 + 150, screenSize.y / 2 + 30));
                ImGui::Text("press 'space' when done");
                */
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
                    style.Colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.2f, 1.f);

                    ImGui::PushFont(_gui->getFont("roboto_48_bold"));
                    {
                        ImGui::SetCursorPos(ImVec2(x0 / 2 - 150, screenSize.y / 2 - 200));
                        ImGui::Text("adjust the light untill all 45 dots are found");
                        ImGui::SetCursorPos(ImVec2(x0 / 2 - 150, screenSize.y / 2 - 160));
                        ImGui::Text("hide cursor by dragging it to the bottom of the screen");
                        uint ndots = pointGreyCamera->numDots[_screen];

                        ImGui::SetCursorPos(ImVec2(x0 / 2 - 150, screenSize.y / 2 - 120));
                        ImGui::Text("also log dot count here  %d / 45", ndots);

                        if (ndots == 45) {
                            ImGui::SetCursorPos(ImVec2(x0 / 2 - 150, screenSize.y / 2 - 60));
                            ImGui::Text("all dots picked up, press space to save");
                            for (int j = 0; j < 45; j++) {
                                calibrationDots[j] = pointGreyCamera->dotArray[_screen][j];
                            }
                        }
                    }


                    style.Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.8f, 0.8f, 1.f);

                    float dY = (screenSize.y - 40) / 4;
                    float dX = (x0 - 40) / 8;


                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    static ImVec4 col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                    const ImU32 col32 = ImColor(col);

                    float left = x0 * _screen + setup.XOffset3D;
                    for (int y = 0; y < 5; y++)
                    {
                        for (int x = 0; x < 9; x++)
                        {
                            //ImGui::SetCursorPos(ImVec2(10 + x * dX, 10 + y * dY));
                            //ImGui::Text("o");
                            draw_list->AddCircle(ImVec2(left + 20 + (x * dX), 20 + (y * dY)), 3, col32, 20, 6);
                        }
                    }
                }
            }

            if (modeCalibrate == 3)
            {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                static ImVec4 col = ImVec4(1.0f, 0.0f, 0.0f, 0.3f);
                const ImU32 col32 = ImColor(col);

                /*
                for (int i = 0; i < setup.numLanes; i++)
                {
                    zigbeeRounds(i, 100);
                } */   // only in s apecila test setup for both cameras

                for (int j = 0; j < pointGreyCamera->numDots[_screen]; j++) {
                    glm::vec3 screen = screenMap[cameraToCalibratate].toScreen(pointGreyCamera->dotArray[_screen][j]);
                    draw_list->AddCircle(ImVec2(screen.x, screen.y), 18, col32, 30, 6);
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

        uint2 InstSize = uint2(setup.instructorSize.x, setup.instructorSize.y - menuHeight);
        uint2 LiveSize = uint2((int)screenSize.x, (int)screenSize.y - menuHeight);

        if (setup.hasInstructorScreen)
        {
            Gui::Window Inst(_gui, "instructor", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            Inst.windowSize(InstSize.x, InstSize.y);
            Inst.windowPos(setup.instructorOffet.x, menuHeight);
            {
                switch (guiMode)
                {
                case gui_menu:
                    guiMenu.renderGui(_gui, Inst);
                    break;
                case gui_camera:
                    break;
                case gui_screen:
                    setup.renderGui(_gui, (float)setup.instructorSize.x, true);
                    break;
                case gui_targets:
                    renderTargets(_gui, Inst, InstSize);
                    break;
                case gui_exercises:
                    renderExerciseBuilder(_gui, Inst, InstSize);
                    break;
                case gui_live:
                    renderLiveInstructor(_gui, Inst, LiveSize);
                    break;
                }
            }
            Inst.release();

            Gui::Window Live(_gui, "live", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            Live.windowSize(LiveSize.x, LiveSize.y);
            Live.windowPos(setup.XOffset3D, menuHeight);
            {
                switch (guiMode)
                {
                case gui_menu:
                    renderBranding(_gui, Live, LiveSize);
                    break;
                case gui_camera:
                    renderCamera(_gui, Live, InstSize);
                    break;
                case gui_screen:
                    setup.renderGui(_gui, screenSize.x);
                    break;
                case gui_targets:
                    renderBranding(_gui, Live, LiveSize);
                    break;
                case gui_exercises:
                    renderBranding(_gui, Live, LiveSize);
                    break;
                case gui_live:
                    renderLive(_gui, Live, LiveSize);
                    break;
                }
            }
            Live.release();
        }
        else
        {
            Gui::Window Live(_gui, "live", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            Live.windowSize(LiveSize.x, LiveSize.y);
            Live.windowPos(0, menuHeight);
            {
                switch (guiMode)
                {
                case gui_menu:
                    guiMenu.renderGui(_gui, Live);
                    break;
                case gui_camera:
                    renderCamera(_gui, Live, InstSize);
                    break;
                case gui_screen:
                    setup.renderGui(_gui, screenSize.x);
                    break;
                case gui_targets:
                    renderTargets(_gui, Live, LiveSize);
                    break;
                case gui_exercises:
                    renderExerciseBuilder(_gui, Live, LiveSize);
                    break;
                case gui_live:
                    QR.renderLive(_gui, Live, bulletHole);
                    ImGui::PushFont(_gui->getFont("roboto_32"));
                    {
                        //nextButton();
                        //menuButton();
                    }
                    ImGui::PopFont();
                    break;
                }
            }
            Live.release();
        }



        if (setup.requestClose)
        {
            setup.requestClose = false;
            guiMode = gui_menu;
        }
        if (requestLive)
        {
            requestLive = false;
            guiMode = gui_live;
        }




        return;
        switch (guiMode)
        {
        case gui_menu:
        {
            Gui::Window menuPanel(_gui, "Menu", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            {
                if (setup.hasInstructorScreen) {
                    menuPanel.windowSize(setup.instructorSize.x, setup.instructorSize.y - menuHeight);
                    menuPanel.windowPos(setup.instructorOffet.x, menuHeight);
                }
                else
                {
                    menuPanel.windowSize((int)screenSize.x, (int)screenSize.y - menuHeight);
                    menuPanel.windowPos(0, menuHeight);
                }
                guiMenu.renderGui(_gui, menuPanel);
            }
            menuPanel.release();

            if (requestLive)
            {
                requestLive = false;
                guiMode = gui_live;
            }
        }
        break;

        case gui_camera:
            //menuButton();
            break;

        case gui_screen:
        {
            if (setup.hasInstructorScreen) {
                Gui::Window screenInst(_gui, "Screen", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
                {
                    screenInst.windowSize((int)screenSize.x, (int)screenSize.y - menuHeight);
                    screenInst.windowPos(setup.XOffset3D, menuHeight);
                    setup.renderGui(_gui, screenSize.x);
                    if (setup.requestClose)
                    {
                        setup.requestClose = false;
                        guiMode = gui_menu;
                    }
                }
                screenInst.release();
            }

            Gui::Window screenPanel(_gui, "Screen", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            {
                screenPanel.windowSize((int)screenSize.x, (int)screenSize.y - menuHeight);
                screenPanel.windowPos(setup.XOffset3D, menuHeight);
                setup.renderGui(_gui, screenSize.x);
                if (setup.requestClose)
                {
                    setup.requestClose = false;
                    guiMode = gui_menu;
                }
            }
            screenPanel.release();

            //menuButton();
        }
        break;

        case gui_targets:
        {
            Gui::Window targetPicker(_gui, "Target picker", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            targetPicker.windowSize((int)screenSize.x / 2, (int)screenSize.y - menuHeight);
            targetPicker.windowPos(0, menuHeight);
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
            targetPanel.windowSize((int)screenSize.x / 2, (int)screenSize.y - menuHeight);
            targetPanel.windowPos((int)screenSize.x / 2, menuHeight);
            targetBuilder.renderGui(_gui, targetPanel);
            targetPanel.release();
            //menuButton();
        }
        break;

        case gui_exercises:
        {
            Gui::Window exercisePanel(_gui, "Excercises", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            exercisePanel.windowSize((int)screenSize.x, (int)screenSize.y - menuHeight);
            exercisePanel.windowPos(setup.XOffset3D, menuHeight);
            QR.renderGui(_gui, screenSize, exercisePanel);
            exercisePanel.release();

            if (requestLive)
            {
                requestLive = false;
                Kolskoot::guiMode = gui_live;
            }
            //menuButton();
        }
        break;

        case gui_live:
        {
            FALCOR_PROFILE("gui_live");
            Gui::Window livePanel(_gui, "Live", { 100, 100 }, { 0, 0 }, Falcor::Gui::WindowFlags::NoResize);
            livePanel.windowSize((int)screenSize.x, (int)screenSize.y - menuHeight);
            livePanel.windowPos(setup.XOffset3D, menuHeight);
            QR.renderLive(_gui, livePanel, bulletHole);
            ImGui::PushFont(_gui->getFont("roboto_32"));
            {
                //nextButton();
                //menuButton();
            }
            ImGui::PopFont();
            livePanel.release();

        }
        break;
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

    setup.load();
    pointGreyCamera->setFirstCamera(setup.cameraSerial[0]);

    ballistics.load();
    airToggle.load();
    bulletHole = Texture::createFromFile(setup.dataFolder + "/targets/bullet.dds", true, true);

    ammoType[0] = Texture::createFromFile(setup.dataFolder + "/9mm.dds", true, true);
    ammoType[1] = Texture::createFromFile(setup.dataFolder + "/556.dds", true, true);
    ammoType[2] = Texture::createFromFile(setup.dataFolder + "/762.dds", true, true);

    kolskootLogo = Texture::createFromFile(setup.dataFolder + "/icons/kolskootLogo.png", true, true);

    backdrop = Texture::createFromFile(setup.dataFolder + "/icons/range_outdoor.png", true, true);

    if (std::filesystem::exists("kolskootCamera_0.xml"))
    {
        std::ifstream is("kolskootCamera_0.xml");
        if (is.good()) {
            cereal::XMLInputArchive archive(is);
            screenMap[0].serialize(archive, 100);
        }
    }

    if (std::filesystem::exists("kolskootCamera_1.xml"))
    {
        std::ifstream is("kolskootCamera_1.xml");
        if (is.good()) {
            cereal::XMLInputArchive archive(is);
            screenMap[1].serialize(archive, 100);
        }
    }


    // targets
    for (const auto& entry : std::filesystem::directory_iterator(setup.dataFolder + "/targets"))
    {
        if (!entry.is_directory())
        {
            //std::string ext = entry.path().extension().string();
            if (entry.path().string().find("target.json") != std::string::npos) {
                targetList.emplace_back();
                targetList.back().load(entry.path());
            }
        }
    }

    int ret = ZIGBEE.OpenPort((eComPort)setup.zigbeeCOM, br_38400, 8, ptNONE, sbONE);
    if (ret != 0)
    {
        // log zigbee failed
    }

    graphicsState = GraphicsState::create();

    QR.print();
    // menu
    guiMenu.load(); // remember to reload after we save stuff

    guiStyle();
}



void Kolskoot::onShutdown()
{
    airToggle.save();

    PointGrey_Camera::FreeSingleton();

    if (ZIGBEE.IsOpen())
    {
        zigbeeRounds(0, 0, true);
        Sleep(100);
        ZIGBEE.ClosePort();
    }
}



void Kolskoot::mouseShot()
{
    if (ImGui::IsMouseClicked(0))       // insert a mouse shot
    {
        ImVec2 mouse = ImGui::GetMousePos();
        if ((mouse.y > 150) && mouse.y < (screenSize.y - 100))         // ignore the bottom of teh screen so we can click next etc
        {
            float offsetX = mouse.x - setup.XOffset3D;
            int lane = (int)floor(offsetX / (screenSize.x / setup.numLanes));
            if ((lane >= 0) && (lane < setup.numLanes))
            {
                if (QR.getRoundsLeft(lane) > 0)
                {
                    QR.mouseShot(offsetX, mouse.y, setup);
                    zigbeeFire(lane);                   // R4 / AK

                    PlaySoundA((LPCSTR)(setup.dataFolder + "/sounds/Beretta_shot.wav").c_str(), NULL, SND_FILENAME | SND_ASYNC);// - the correct code
                }
            }
        }
    }
}



void Kolskoot::pointGreyShot()
{
    static int pistolLaneDelay[20];

    for (uint i = 0; i < setup.num3DScreens; i++)
    {
        while (pointGreyCamera->dotQueue[i].size()) {
            glm::vec3 screen = screenMap[i].toScreen(pointGreyCamera->dotQueue[i].front());

            if (!(screen.x == 0 && screen.y == 0))
            {
                float offsetX = screen.x - setup.XOffset3D;
                int lane = (int)floor(offsetX / (screenSize.x / setup.numLanes));
                if ((lane >= 0) && (lane < setup.numLanes))
                {
                    bool pistolDelay = true;
                    if (Kolskoot::ballistics.currentAmmo == ammo_9mm) {
                        if (pistolLaneDelay[lane] < 2)
                        {
                            pistolDelay = false;    // dont shoot 
                        }
                        else
                        {
                            pistolDelay = true;
                            pistolLaneDelay[lane] = 0;
                        }
                    }

                    if (pistolDelay && QR.getRoundsLeft(lane) > 0)
                    {
                        QR.mouseShot(offsetX, screen.y, setup);
                        zigbeeFire(lane);                   // R4 / AK
                        PlaySoundA((LPCSTR)(setup.dataFolder + "/sounds/Beretta_shot.wav").c_str(), NULL, SND_FILENAME | SND_ASYNC);// - the correct code
                    }
                }
            }
            pointGreyCamera->dotQueue[i].pop();
        }
    }

    for (int i = 0; i < 20; i++)
    {
        pistolLaneDelay[i] ++;
    }
}



// _Play through this exercise
void Kolskoot::updateLive()
{
    QR.updateLive(gpFramework->getFrameRate().getAverageFrameTime() * 0.001f);
    airOffAfterLive = true;
    mouseShot();
    pointGreyShot();

    unsigned char channels = 0;
    for (int i = 0; i < setup.numLanes; i++)                        // turn air on for pistols if there are rounds left
    {
        if (airToggle.inUse(ballistics.currentAmmo, i))
        {
            //zigbeeRounds(i, QR.getRoundsLeft(i));
            int R = QR.getRoundsLeft(i);
            if (R > 0)
            {
                channels |= 1 << i;
            }
        }
        else
        {
            //zigbeeRounds(i, 0);
        }
    }
    zigbeeRoundsCombined(channels);
}



void Kolskoot::onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
    {
        FALCOR_PROFILE("pointGreyBuffer");
        if (guiMode == gui_camera || guiMode == gui_menu)
        {
            pointGreyCamera->SetCalibrateMode(true);
            for (int i = 0; i < setup.num3DScreens; i++)
            {
                if (pointGreyBuffer[i])
                {
                    pRenderContext->updateTextureData(pointGreyBuffer[i].get(), pointGreyCamera->bufferReference[i]);
                }
                if (pointGreyDiffBuffer[i])
                {
                    pRenderContext->updateTextureData(pointGreyDiffBuffer[i].get(), pointGreyCamera->bufferThreshold[i]);
                }
            }
        }
        else
        {
            pointGreyCamera->SetCalibrateMode(false);       // queue mode for fireing
        }
    }

    {
        // preiodically clear teh zigbee buffer
        static int zcnt = 0;
        zcnt = (zcnt++) % 16;
        if (zcnt == 0)     zigbeeClear();   // clear every couple of frames
    }


    // live fire shooting
    if (guiMode == gui_live && QR.currentStage == live_live)
    {
        updateLive();
    }
    else if (airOffAfterLive && guiMode == gui_live)
    {
        zigbeeRounds(0, 0, true);       // turn air off, calling with true set all channels
        airOffAfterLive = false;
    }



    {
        // at the moment all renderign is doen with imGui so just clear
        FALCOR_PROFILE("clear");
        const float4 clearColor(0.2f, 0.02f, 0.2f, 1);
        pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
        mpGraphicsState->setFbo(pTargetFbo);
    }



    //Sleep(2); // weird, dit is genoeg om frame sync aan te sit
}



bool Kolskoot::onKeyEvent(const KeyboardEvent& keyEvent)
{
    if (mpScene && mpScene->onKeyEvent(keyEvent)) return true;

    if (keyEvent.type == KeyboardEvent::Type::KeyPressed)
    {
        if (keyEvent.key == Input::Key::Escape)
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

        if ((modeCalibrate > 0) && (keyEvent.key == Input::Key::Space))
        {
            modeCalibrate++;

            if (modeCalibrate == 2) {
                calibrationCounter = 120;
            }
            else if (modeCalibrate == 3)
            {
                float x0 = setup.screen_pixelsX / setup.num3DScreens;
                screenMap[cameraToCalibratate].build(calibrationDots, float2(x0, screenSize.y), setup.XOffset3D + (x0 * cameraToCalibratate));
                char name[256];
                sprintf(name, "kolskootCamera_%d.xml", cameraToCalibratate);
                std::ofstream os(name);
                if (os.good()) {
                    cereal::XMLOutputArchive archive(os);
                    screenMap[cameraToCalibratate].serialize(archive, 100);
                }
            }
        }

        if (guiMode == gui_live)
        {
            if (keyEvent.key == Input::Key::Space)
            {
                if (QR.liveNext())
                {
                    guiMode = gui_menu;
                    zigbeeRounds(0, 0, true);		// turn all air off
                }
            }
            if (keyEvent.key == Input::Key::Escape)
            {
                guiMode = gui_menu; // FIME maybe know if it awas a test run and retirn to exercise builder instead
                zigbeeRounds(0, 0, true);		// turn all air off
            }
        }
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
            setup.eyeHeights[0] = mouseEvent.pos.y * screenSize.y;
        }
    }
    /*
    if ((guiMode == gui_live) && (mouseEvent.type == MouseEvent::Type::ButtonDown))                         // insert a mouse shot
    {
        QR.mouseShot((mouseEvent.pos.x * screenSize.y), (mouseEvent.pos.y * screenSize.y), setup);
    }
    */
    return false;
}



void Kolskoot::onResizeSwapChain(uint32_t _width, uint32_t _height)
{
    auto monitorDescs = MonitorInfo::getMonitorDescs();
    uint numMonitor = monitorDescs.size();
    uint Screens = __min(setup.num3DScreens, numMonitor);

    screenSize = monitorDescs[0].resolution;
    for (int i = 1; i < Screens; i++) {
        screenSize.x += monitorDescs[i].resolution.x;
    }

    viewport3d.originX = 0;
    viewport3d.originY = 0;
    viewport3d.minDepth = 0;
    viewport3d.maxDepth = 1;
    viewport3d.width = screenSize.x;
    viewport3d.height = screenSize.y;

    setup.screen_pixelsX = screenSize.x;
    setup.screen_pixelsY = screenSize.y;

    camera->setAspectRatio(screenSize.x / screenSize.y);

    screenMouseScale.x = _width / screenSize.x;
    screenMouseScale.y = _height / screenSize.y;
    screenMouseOffset.x = 0;
    screenMouseOffset.y = 0;

    setup.instructorSize.x = (int)screenSize.x;  // so weven if no screen use this value for menu
    if (setup.hasInstructorScreen && setup.instructorScreenIdx <= numMonitor)
    {
        setup.instructorOffet = uint2(screenSize.x, 0);
        setup.instructorSize = monitorDescs[setup.instructorScreenIdx].resolution;
    }

    setup.XOffset3D = 0;
    if (setup.instructorScreenIdx == 0)
    {
        setup.instructorOffet = uint2(0, 0);
        setup.XOffset3D = setup.instructorSize.x;
    }

    if (!setup.hasInstructorScreen)
    {
        setup.XOffset3D = 0;
    }

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
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.01f, 0.01f, 0.01f, 1.f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.17f, 0.70f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.14f, 0.17f, 0.90f);

    style.Colors[ImGuiCol_Button] = ImVec4(0.03f, 0.03f, 0.3f, 0.5f);
    style.Colors[ImGuiCol_ButtonHovered] = DARKLIME(1.f);
    style.Colors[ImGuiCol_HeaderHovered] = DARKLIME(1.f);

    style.Colors[ImGuiCol_FrameBg] = ImVec4(0, 0.03f, 0.03f, 0.5);

    style.Colors[ImGuiCol_Tab] = ImVec4(0.03f, 0.03f, 0.03f, 1.f);
    style.Colors[ImGuiCol_TabHovered] = DARKLIME(1.f);
    style.Colors[ImGuiCol_TabActive] = DARKLIME(1.f);

    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.03f, 0.03f, 0.03f, 1.0f);

    style.Colors[ImGuiCol_ModalWindowDimBg] = DARKLIME(0.3f);

    // FIXME only for live
    style.Colors[ImGuiCol_Button] = ImVec4(0.03f, 0.03f, 0.03f, 0.8f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
    //style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.0f, 0.0f, 0.f);

    style.FrameRounding = 0.0f;

    style.ScrollbarSize = 20;
    style.FrameBorderSize = 0;
    style.FramePadding = ImVec2(0, 0);

    style.WindowPadding = ImVec2(0, 0);;              // Padding within a window.
    style.WindowRounding = 0;             // Radius of window corners rounding. Set to 0.0f to have rectangular windows.
    style.WindowBorderSize = 0;

}



void   Kolskoot::zigbeePaperMove()
{
    if (!ZIGBEE.IsOpen()) return;
    unsigned char channels = 1 << 4;		// set this to true

    unsigned char STR[13];
    unsigned long bytes = ZIGBEE.Read(STR, 13);	// read   ??? Hoekom moet on sbuffer skoonmaak  FIXME
    Sleep(30);

    unsigned char ANS[13];
    switch (setup.zigbeePacketVersion)
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
    switch (setup.zigbeePacketVersion)
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


void  Kolskoot::zigbeeRoundsCombined(unsigned char channels)
{
    if (!ZIGBEE.IsOpen()) return;
    if (Kolskoot::ballistics.currentAmmo != ammo_9mm) return;
    if (channels == zigbeeChannels)  return;

    zigbeeChannels = channels;

    unsigned char ANS[13];

    switch (setup.zigbeePacketVersion)
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
        /*
        unsigned char STR[13];			// veelvoude van 10
        unsigned long bytes = ZIGBEE.Read(STR, 13);	// read   ??? Hoekom moet on sbuffer skoonmaak  FIXME
        Sleep(30);      // FIXME kan ons hierdie baie kleiner kry? 10ms dalk
        */
        unsigned char ANS[13];

        switch (setup.zigbeePacketVersion)
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
    }
}



void  Kolskoot::zigbeeFire(unsigned int lane)       //R4 and AK
{
    if (!ZIGBEE.IsOpen()) return;
    if (Kolskoot::ballistics.currentAmmo == ammo_9mm) return;


    // test R for change and return - write to zigbee
    /*
    unsigned char STR[50];			            // veelvoude van 10
    memset(STR, 0, 50);			                // clear
    unsigned long bytes = ZIGBEE.Read(STR, 13);	// read   ??? Hoekom moet ons buffer skoonmaak?
    */

    unsigned char ANS[13];

    switch (setup.zigbeePacketVersion)
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
        ANS[2] = 0x0b;	// lengte van pakkie
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


void  Kolskoot::zigbeeClear()       //just read teh buffer to clear it
{
    if (!ZIGBEE.IsOpen()) return;
    unsigned char STR[50];			            // veelvoude van 10
    memset(STR, 0, 50);			                // clear
    unsigned long bytes = ZIGBEE.Read(STR, 13);	// read   ??? Hoekom moet ons buffer skoonmaak?
}


int main(int argc, char** argv)
{
    bool allScreens = false;
    for (int i = 0; i < argc; i++)
    {
        if (std::string(argv[i]).find("-allscreens") != std::string::npos) allScreens = true;
    }

    // TEMP
    allScreens = true;

    pKolskoot = std::make_unique<Kolskoot>();

    SampleConfig config;
    config.windowDesc.title = "Kolskoot";
    config.windowDesc.resizableWindow = true;
    config.windowDesc.mode = Window::WindowMode::Fullscreen;
    if (allScreens) {
        config.windowDesc.mode = Window::WindowMode::AllScreens;
    }
    config.windowDesc.width = 1260;
    config.windowDesc.height = 1140;
    config.windowDesc.monitor = 0;

    Sample::run(config, pKolskoot);
    return 0;
}
