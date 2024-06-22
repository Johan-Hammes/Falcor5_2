#include "glider.h"
#include "imgui.h"
#include <imgui_internal.h>

using namespace std::chrono;
//#pragma optimize("", off)

#define GLM_FORCE_SSE2
#define GLM_FORCE_ALIGNED

extern void setupVert(rvB_Glider* r, int start, float3 pos, float radius, int _mat = 0);
float3 AllSummedForce;
float3 AllSummedLinedrag;





void _cp::load(std::string _name)
{
    std::ifstream CPbin(_name, std::ios::binary);
    if (CPbin.good())
    {
        CPbin.read(reinterpret_cast<char*>(&cp), sizeof(float) * 6 * 36 * 25);
        CPbin.close();
    }
}


float _cp::get(float _brake, float _aoa, uint _chord)
{
    _brake = glm::clamp(_brake * 5.f, 0.f, 4.99f);
    int b_idx = (int)_brake;
    float b = _brake - b_idx;

    _aoa = glm::clamp((_aoa * 57.f) + 6, 0.f, 25.9f);
    int aoa_idx = (int)_aoa;
    float a = _aoa - aoa_idx;

    float cpA = glm::lerp(cp[b_idx][aoa_idx][_chord], cp[b_idx][aoa_idx + 1][_chord], a);
    float cpB = glm::lerp(cp[b_idx + 1][aoa_idx][_chord], cp[b_idx + 1][aoa_idx + 1][_chord], a);
    float cp = glm::lerp(cpA, cpB, b);

    return cp;
}




void _gliderRuntime::renderHUD(Gui* pGui, float2 _screen)
{
    uint triangle = chordSize * spanSize;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.2f, 0.2f, 0.2f, 0.1f));

    ImGui::PushFont(pGui->getFont("H1"));
    {
        // variometer
        float dLine = 100.f / 1440.f * _screen.y;

        ImGui::SetCursorPos(ImVec2(_screen.x - 300, _screen.y / 2));
        ImGui::Text("%2.1f", v[triangle].y);
        ImGui::SetCursorPos(ImVec2(_screen.x - 150, _screen.y / 2));
        ImGui::Text("m/s");

        ImGui::SetCursorPos(ImVec2(_screen.x - 320, _screen.y / 2 + dLine));
        ImGui::Text("%d", (int)(x[triangle].y + ROOT.y));
        ImGui::SetCursorPos(ImVec2(_screen.x - 150, _screen.y / 2 + dLine));
        ImGui::Text("m");

        float3 vel = v[triangle];
        vel.y = 0;
        float Vgrnd = glm::length(vel) * 3.6f;
        float Vair = glm::length(vel - pilotWind) * 3.6f;

        ImGui::SetCursorPos(ImVec2(_screen.x - 500, _screen.y / 2 + 2* dLine));
        ImGui::Text("ground %d", (int)Vgrnd);
        ImGui::SetCursorPos(ImVec2(_screen.x - 170, _screen.y / 2 + 2* dLine));
        ImGui::Text("km/h");

        ImGui::SetCursorPos(ImVec2(_screen.x - 400, _screen.y / 2 + 3* dLine));
        ImGui::Text("air %d", (int)Vair);
        ImGui::SetCursorPos(ImVec2(_screen.x - 170, _screen.y / 2 + 3* dLine));
        ImGui::Text("km/h");

        ImGui::SetCursorPos(ImVec2(_screen.x - 500, _screen.y / 2 + 4* dLine));
        ImGui::Text("glide ratio  1 : %d", (int)fabs(glideRatio));

        float gs = glm::length(a_root - float3(0, 9.8f, 0)) / 9.8f;
        ImGui::SetCursorPos(ImVec2(_screen.x - 300, _screen.y / 2 + 5* dLine));
        ImGui::Text("%1.1f g", gs);

    }
    ImGui::PopFont();



    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::PushFont(pGui->getFont("roboto_32"));
    {
        // weight shift
        ImGui::SetCursorPos(ImVec2(_screen.x / 2 - 150, _screen.y - 150));
        ImGui::SetNextItemWidth(300);
        if (ImGui::SliderFloat("weight shift", &weightShift, 0, 1, "%.2f"));

        // bake forcves
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.f));
        {
            float leftFPix = pBrakeLeft->tencileForce * 10;
            ImGui::SetCursorPos(ImVec2(_screen.x / 2 - 400, _screen.y - 100 - leftFPix));
            ImGui::BeginChildFrame(3331, ImVec2(30, leftFPix));
            ImGui::EndChildFrame();

            float rightFPix = pBrakeRight->tencileForce * 10;
            ImGui::SetCursorPos(ImVec2(_screen.x / 2 + 370, _screen.y - 100 - rightFPix));
            ImGui::BeginChildFrame(3332, ImVec2(30, rightFPix));
            ImGui::EndChildFrame();
        }
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.02f, 0.02f, 0.02f, 1.f));
        {
            float leftFPix = (maxBrake - pBrakeLeft->length) * 300;
            ImGui::SetCursorPos(ImVec2(_screen.x / 2 - 410, _screen.y - 400 + leftFPix));
            ImGui::BeginChildFrame(3301, ImVec2(60, 10 + rumbleLeft * 50));
            ImGui::EndChildFrame();

            float rightFPix = (maxBrake - pBrakeRight->length) * 300;
            ImGui::SetCursorPos(ImVec2(_screen.x / 2 + 360, _screen.y - 400 + rightFPix));
            ImGui::BeginChildFrame(3302, ImVec2(60, 10 + rumbleRight * 50));
            ImGui::EndChildFrame();
        }
        ImGui::PopStyleColor();



        // line lengths
        /*
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 1.0f));
        ImGui::SetCursorPos(ImVec2(_screen.x - 300, 100));
        ImGui::Text(" F %4d cm  %4d cm", (int)((maxBrake - pBrakeLeft->length) * 100.f), (int)((maxBrake - pBrakeRight->length) * 100.f));
        ImGui::SetCursorPos(ImVec2(_screen.x - 300, 140));
        ImGui::Text("A3 %4d cm  %4d cm", (int)((maxEars - pEarsLeft->length) * 100.f), (int)((maxEars - pEarsRight->length) * 100.f));
        ImGui::PopStyleColor();
        */

        // warnings
        if (CPU_usage < 0.9f)   ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
        else if (CPU_usage < 1.0f)   ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.1f, 1.0f));
        else    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.1f, 0.1f, 1.0f));
        ImGui::SetCursorPos(ImVec2(_screen.x - 200, 60));
        ImGui::Text("CPU  %d %%", (int)(CPU_usage * 100.f));
        ImGui::PopStyleColor();

        //ImGui::SetCursorPos(ImVec2(_screen.x - 300, 300));
        //ImGui::Text("L  %d N R %d N", (int)(pBrakeLeft->tencileForce), (int)(pBrakeRight->tencileForce));


    }
    ImGui::PopFont();
    ImGui::PopStyleColor();


    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
}



void _gliderRuntime::renderGui(Gui* mpGui)
{
    ImGui::Columns(2);

    uint triangle = chordSize * spanSize;

    ImGui::Text("ribbonCount %d", ribbonCount);



    if (ImGui::SliderFloat("weight", &weightShift, 0, 1))
    {
        constraints[1].l = weightShift * weightLength;
        constraints[2].l = (1.0f - weightShift) * weightLength;
    }
    /*
    ImGui::Checkbox("symmetry", &symmetry);

    if (pBrakeLeft) ImGui::SliderFloat("F left", &pBrakeLeft->length, 0.1f, maxBrake);
    if (pBrakeRight) ImGui::SliderFloat("F right", &pBrakeRight->length, 0.1f, maxBrake);

    if (pEarsLeft) ImGui::SliderFloat("ears left", &pEarsLeft->length, 0.1f, maxEars);
    if (pEarsRight) ImGui::SliderFloat("ears right", &pEarsRight->length, 0.1f, maxEars);

    if (pSLeft) ImGui::SliderFloat("S left", &pSLeft->length, 0.1f, maxS);
    if (pSRight) ImGui::SliderFloat("S right", &pSRight->length, 0.1f, maxS);

    if (symmetry)
    {
        if (pBrakeLeft) pBrakeRight->length = pBrakeLeft->length;
        if (pEarsLeft) pEarsRight->length = pEarsLeft->length;
        if (pSLeft) pSRight->length = pSLeft->length;
    }
    */
    ImGui::Checkbox("step", &singlestep);
    ImGui::SameLine(0, 50);
    ImGui::Text("%d", frameCnt);

    ImGui::NewLine();
    ImGui::PushFont(mpGui->getFont("roboto_20"));
    {
        ImGui::Text("Energy %2.1f,    %2.1f", energy, d_energy);
        ImGui::Text("Energy %2.1f,    1::%2.1f", relAlt, glideRatio);

    }
    ImGui::PopFont();


    ImGui::NewLine();

    ImGui::Text("pilot / lines/ wing %2.1fkg, %2.1f g, %2.1fkg", weightPilot, weightLines * 1000.f, weightWing);
    ImGui::Text("altitude %2.1fm", x[triangle].y);

    float3 velHor = v[triangle];
    velHor.y = 0;
    float Vh = glm::length(velHor) * 3.6f;
    ImGui::Text("speed %2.1f km/h", Vh);
    ImGui::Text("vertical %2.1f m/s", v[triangle].y);
    ImGui::Text("ROOT    (%2.1f, %2.1f, %2.1f)", ROOT.x, ROOT.y, ROOT.z);
    ImGui::Text("TRIGGER   %d  %d", leftTrigger, rightTrigger);
    float3 body = ROOT + x[triangle];
    float3 W = windTerrain.W(body);
    float H = windTerrain.H(body);

    ImGui::Text("Height  %d Wind %2.1f  (%2.1f, %2.1f, %2.1f)", (int)(body.y - H), glm::length(W), W.x, W.y, W.z);


    ImGui::NewLine();
    ImGui::Text("wing surface drag %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(sumDragWing), sumDragWing.x, sumDragWing.y, sumDragWing.z);
    ImGui::Text("line drag %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(AllSummedLinedrag), AllSummedLinedrag.x, AllSummedLinedrag.y, AllSummedLinedrag.z);
    ImGui::Text("pilot drag %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(pilotDrag), pilotDrag.x, pilotDrag.y, pilotDrag.z);

    ImGui::NewLine();
    ImGui::Text("wing force %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(sumFwing), sumFwing.x, sumFwing.y, sumFwing.z);
    ImGui::Text("ALLSUMMED %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(AllSummedForce), AllSummedForce.x, AllSummedForce.y, AllSummedForce.z);
    //ImGui::Text("carabiner Left %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(tensionCarabinerLeft), tensionCarabinerLeft.x, tensionCarabinerLeft.y, tensionCarabinerLeft.z);
    //ImGui::Text("carabinerRight %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(tensionCarabinerRight), tensionCarabinerRight.x, tensionCarabinerRight.y, tensionCarabinerRight.z);
    float3 sum = tensionCarabinerLeft + tensionCarabinerRight;
    ImGui::Text("BODY %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(sum), sum.x, sum.y, sum.z);


    ImGui::NewLine();
    ImGui::Text("AoA, cellV, brake, pressure, ragdoll");
    for (int i = 0; i < spanSize; i += 3)
    {
        ImGui::Text("%2d : %4.1f, %4.1f, %4d%%, %4.1f, %2.1f %d ", (int)(cells[i].AoA * 57), cells[i].cellVelocity, cells[i].AoBrake, (int)(cells[i].pressure * 99.f), cells[i].ragdoll, cells[i].AoA * 57, (int)cells[i].reynolds);
    }

    ImGui::NextColumn();

    static char SEARCH[256];
    ImGui::InputText("search", SEARCH, 256);
    linesLeft.renderGui(SEARCH);
    linesRight.renderGui(SEARCH);

}


// it gets parabuilder data
// this should load from disk
void _gliderRuntime::setup(std::vector<float3>& _x, std::vector<float>& _w, std::vector<uint4>& _cross, uint _span, uint _chord, std::vector<_constraint>& _constraints)
{
    x = _x;
    w = _w;

    exportGliderShape();

    cross = _cross;

    v.resize(x.size());
    memset(&v[0], 0, sizeof(float3) * v.size());
    n.resize(_span * _chord);
    t.resize(_span * _chord);

    soundLowPass.resize(x.size());

    cp_feedback.resize(_span * _chord);

    x_old.resize(x.size());


    constraints = _constraints;

    spanSize = _span;
    chordSize = _chord;

    cells.resize(_span);



    //ROOT = float3(5500, 1900, 11500);   // walensee
    ROOT = float3(-7500, 1300, -10400);// hulftegg
    //ROOT = float3(20000, 1000, 20000);// hulftegg

    ROOT = float3(9000, 2100, 8000);// middlke
    // 
    //ROOT = float3(15500, 1300, 14400);// windy south
    //ROOT = float3(-500, 500, 15400);// windy south landings

    for (int i = 0; i < spanSize; i++)
    {
        cells[i].v = float3(0, -1.5, -7);
        cells[i].pressure = 1;
        cells[i].chordLength = glm::length(x[index(i, 12)] - x[index(i, 0)]); // fixme
    }

    for (int i = 0; i < x.size(); i++)
    {
        x[i] += float3(0, 0, 0);
        v[i] = float3(0, -1.5, -7);
    }

    for (int i = 0; i < n.size(); i++)
    {
        n[i] = 0.125f * glm::cross((x[cross[i].z] - x[cross[i].w]), (x[cross[i].x] - x[cross[i].y]));
        // this should be on disk
    }

    frameCnt = 0;
    vBody = 0;


    relAlt = 0;
    relDist = 0;


    int startVert = spanSize * chordSize;
    weightLength = glm::length(x[startVert + 1] - x[startVert]) * 2.f;




    std::string folder = xfoilDir + "/" + wingName + "/cp.bin";
    Pressure.load(folder);

}


void _gliderRuntime::setupLines()
{
    static bool first = true;
    if (first)
    {
        pBrakeLeft = linesLeft.getLine("FF1");
        pBrakeRight = linesRight.getLine("FF1");
        pEarsLeft = linesLeft.getLine("ears");
        pEarsRight = linesRight.getLine("ears");
        pSLeft = linesLeft.getLine("S1");
        pSRight = linesRight.getLine("S1");

        if (pBrakeLeft)
        {
            first = false;
            if (pBrakeLeft)      maxBrake = pBrakeLeft->length;
            if (pEarsLeft)  maxEars = pEarsLeft->length;
            if (pSLeft)  maxS = pSLeft->length;
        }
    }
}


void _gliderRuntime::solve_air(float _dT, bool _left)
{
    float airDensity = 1.11225f;    // Fixme form CFD later

    uint from = 0;
    uint to = spanSize >> 1;
    if (_left) {
        from = spanSize >> 1;
        to = spanSize;
    }

    //  cell velocity and dynamic pressure
    float oneOverChord = 1.f / chordSize;
    float3 cellWind = { 0, 0, 0 };
    for (int i = from; i < to; i++)
    {
        cells[i].v = { 0, 0, 0 };
        for (int j = 0; j < chordSize; j++)
        {
            cells[i].v += v[index(i, j)] * oneOverChord;    // average cell velocity
        }

        if (i < spanSize / 2) {
            cellWind = glm::lerp(wingWind[0], wingWind[1], (float)i / (float)(spanSize / 2));       // with left right, this can simplify
        }
        else {
            cellWind = glm::lerp(wingWind[2], wingWind[1], (float)(spanSize - 1 - i) / (float)(spanSize / 2));
        }
        cells[i].flow_direction = cellWind - cells[i].v;
        cells[i].cellVelocity = glm::length(cells[i].flow_direction);
        cells[i].flow_direction /= cells[i].cellVelocity;
        cells[i].rho = __min(300.f, 0.5f * airDensity * (cells[i].cellVelocity * cells[i].cellVelocity));       // dynamic pressure , nut sure why its clamped
        cells[i].reynolds = (1.125f * cells[i].cellVelocity * cells[i].chordLength) / (0.0000186);
        cells[i].cD = 0.027f / pow(cells[i].reynolds, 1.f / 7.f);
    }
}

void _gliderRuntime::solve_aoa(float _dT, bool _left)
{
    uint from = 0;
    uint to = spanSize >> 1;
    if (_left) {
        from = spanSize >> 1;
        to = spanSize;
    }

    for (int i = from; i < to; i++)                  // aoa and brake
    {
        cells[i].AoA = 0;
        cells[i].AoBrake = 0;

        if (cells[i].cellVelocity > 0.01f)
        {
            float3 ra = x[index(i, 3)] - x[index(i, 15)];
            float3 rb = x[index(i, 9)] - x[index(i, 21)];
            cells[i].front = glm::normalize(x[index(i, 10)] - x[index(i, 3)]);              // from C forward
            cells[i].right = glm::normalize(glm::cross(ra, rb));
            cells[i].up = glm::normalize(glm::cross(cells[i].front, -cells[i].right));      // -right because of right handed system 

            float3 frontBrake = glm::normalize(x[index(i, 3)] - x[index(i, 0)]);
            float3 upBrake = glm::normalize(glm::cross(frontBrake, -cells[i].right));
            float sintheta = glm::clamp(-1.5f * glm::dot(upBrake, cells[i].front), 0.f, 1.f);
            cells[i].AoBrake = glm::lerp(cells[i].AoBrake, sintheta, 0.01f);    // quite smoothed AoBrake

            float aoa = asin(glm::dot(cells[i].up, cells[i].flow_direction));
            if (glm::dot(cells[i].flow_direction, cells[i].front) > 0) {
                aoa = 3.14159f - aoa;
            }
            cells[i].AoA = glm::lerp(cells[i].AoA, aoa + 0.21f, 0.5f);                          // very slightly smoothed AoA
            // +0.21f compensates for line lengths
        }
    }
}

void _gliderRuntime::solve_pressure(float _dT, bool _left)
{
    uint from = 0;
    uint to = spanSize >> 1;
    if (_left) {
        from = spanSize >> 1;
        to = spanSize;
    }

    for (int i = from; i < to; i++)  // cell pressure
    {
        float pressure10 = glm::clamp(Pressure.get(cells[i].AoBrake, cells[i].AoA, 10), 0.f, 1.f);
        float pressure11 = glm::clamp(Pressure.get(cells[i].AoBrake, cells[i].AoA, 11), 0.f, 1.f);
        float P = __max(pressure10, pressure11);
        float windPscale = __min(1.f, cells[i].cellVelocity * 0.25f);
        P *= pow(windPscale, 4);
        cells[i].old_pressure = glm::lerp(cells[i].pressure, P, 0.08f);
        cells[i].pressure = cells[i].old_pressure;
    }


    for (int i = from; i < to; i++)                  // the pressure distribution
    {
        if (i < 5)
        {
            cells[i].pressure = cells[5].old_pressure;
        }
        else if (i >= spanSize - 5)
        {
            cells[i].pressure = cells[spanSize - 6].old_pressure;
        }
        else
        {
            float MAX = __max(__max(cells[i].old_pressure, cells[i - 1].old_pressure * 0.95f), cells[i + 1].old_pressure * 0.95f);
            cells[i].pressure = lerp(cells[i].old_pressure, MAX, 1.0f);
        }
    }
}

void _gliderRuntime::solve_surface(float _dT, bool _left)
{
    uint from = 0;
    uint to = (spanSize * chordSize) >> 1;
    if (_left) {
        from = (spanSize * chordSize) >> 1;
        to = spanSize * chordSize;
    }

    for (int i = from; i < to; i++)                      // face normals, area and flow tangent
    {
        int chord = i % chordSize;
        int span = i / chordSize;

        n[i] = 0.125f * glm::cross((x[cross[i].z] - x[cross[i].w]), (x[cross[i].x] - x[cross[i].y]));
        float Area = glm::length(n[i]);     //??? preclaxc
        float3 N = n[i] / Area;
        t[i] = Area * glm::normalize(cells[span].flow_direction - N * glm::dot(N, cells[span].flow_direction));

        float cp = Pressure.get(cells[span].AoBrake, cells[span].AoA, chord);

        // rag doll pressures
        float rag_aoaMin = glm::clamp((cells[span].AoA * 57.f + 15.f) * 0.2f, 0.f, 1.f);
        float rag_aoaMax = 1.f - glm::clamp(((cells[span].AoA * 57.f - 50.f) * 0.02f), 0.f, 1.f);
        float rag_Pressure = glm::clamp(cells[span].pressure * 3.f, 0.f, 1.f); // bottom 33% of pressure
        cells[span].ragdoll = __min(rag_Pressure, __min(rag_aoaMin, rag_aoaMax));
        float cpRagdoll = -glm::dot(N, cells[span].flow_direction);
        cpRagdoll = glm::clamp(cpRagdoll, 0.f, 1.f);
        cp = glm::lerp(cpRagdoll, cp, cells[span].ragdoll);

        //cp_feedback[i] = n[i] * (cells[span].pressure - cp) * cells[span].rho;
        //sumFwing += cp_feedback[i];
        n[i] = n[i] * (-cp) * cells[span].rho * 0.6f;
        sumFwing += n[i];

        float dragIncrease = pow(2.f - cells[span].pressure, 2.f);
        //dragIncrease = glm::lerp(dragIncrease, dragIncrease * 2, 1 - cells[span].ragdoll);
        float drag = 0.0039 * cells[span].rho * 1.0f * dragIncrease;    // double it for rib turbulece
        t[i] *= drag;
        sumDragWing += t[i];
    }
}



void _gliderRuntime::solve_PRE(float _dT)
{
    // Pre solve
    memcpy(&x_old[0], &x[0], sizeof(float3) * x.size());
    for (int i = 0; i < x.size(); i++)
    {
        float3 a = float3(0, -9.8, 0);      // gravity
        if (i < spanSize * chordSize) {
            a += w[i] * n[i];
            a += w[i] * t[i];
            weightWing += 1.f / w[i];
        }
        else if (i < spanSize * chordSize + 3)
        {
            weightPilot += 1.f / w[i];
        }
        else if (w[i] > 0)
        {
            weightLines += 1.f / w[i];
        }

        x[i] += (_dT * v[i]) + (a * _dT * _dT);     // 0.06ms
    }
}

void _gliderRuntime::solve_linesTensions(float _dT)
{
    linesLeft.windAndTension(x, w, n, v, pilotWind, _dT * _dT);     // 0.04ms
    tensionCarabinerLeft = linesLeft.tencileVector;

    linesRight.windAndTension(x, w, n, v, pilotWind, _dT * _dT);
    tensionCarabinerRight = linesRight.tencileVector;
}

void _gliderRuntime::solve_triangle(float _dT)
{
    uint start = chordSize * spanSize;
    float airDensity = 1.11225f;    // Fixme form CFD later

    // Solve the body and carabiners
    x[start + 1] += w[start + 1] * tensionCarabinerLeft * _dT * _dT;
    x[start + 2] += w[start + 2] * tensionCarabinerRight * _dT * _dT;
    // drag on the body
    vBody = glm::length(v[start] - pilotWind);
    float rho = 0.5 * airDensity * vBody * vBody;
    if (vBody > 1) {
        pilotDrag = -glm::normalize(v[start] - pilotWind) * rho * 0.6f * 0.9f;
        x[start] += _dT * w[start] * pilotDrag * _dT;
    }

    for (int k = 0; k < 10; k++)            // Now solve the first 3 constraints multiple times for weight shift and pilot gravity
    {
        for (int i = 0; i < 3; i++)
        {
            solveConstraint(i, 0.5f);
        }
    }
}


void _gliderRuntime::solve_wing(float _dT, bool _left)
{
    // solve
    float dS = 0.7f;    // also pass in
    uint numS = (constraints.size() - 3) / 2;

    if (_left)
    {
        /*
        for (int l = 0; l < 10; l++)
        {
            bool last = false;
            if (l == 9) last = true;
            linesLeft.solveUp(x, last);
            //linesRight.solveUp(x, last);
        }
        */
        for (int i = 3; i < numS + 3; i++)
        {
            solveConstraint(i, dS);
        }
    }
    else
    {
        /*
        for (int l = 0; l < 10; l++)
        {
            bool last = false;
            if (l == 9) last = true;
            //linesLeft.solveUp(x, last);
            linesRight.solveUp(x, last);
        }
        */
        for (int i = numS + 3; i < constraints.size(); i++)
        {
            solveConstraint(i, dS);
        }
    }
}


void _gliderRuntime::solve_POST(float _dT)
{
    uint start = chordSize * spanSize;
    v_oldRoot = v[spanSize * chordSize];

    float Hpilot = windTerrain.H(ROOT + x[start]);
    bool groundProximity = (ROOT + x[start]).y < (Hpilot + 5.f);

    //FALCOR_PROFILE("post");
    for (int i = 0; i < x.size(); i++)
    {
        v[i] = (x[i] - x_old[i]) / _dT;         // 0.03ms

        if (groundProximity)
        {

            float3 p = ROOT + x[i];
            float H = windTerrain.H(p) + 0.3f;             // 0.16ms   ONLY do close to ground for landings, fly as wella s possible
            if (p.y < H)
            {
                x[i].y = H - ROOT.y;
                v[i] *= 0.01f;
            }
        }
    }

    a_root = (v[spanSize * chordSize] - v_oldRoot) / _dT;


    // Energy balance
    float h = (ROOT + x[spanSize * chordSize]).y;
    energyOld = energy;
    energy = (100 * 9.8f * h) + (0.5 * 100 * vBody * vBody);
    d_energy = (energy - energyOld) / _dT;

    float3 del = v[spanSize * chordSize] * _dT;
    relAlt += del.y;
    del.y = 0;
    relDist += glm::length(del);
    glideRatio = relDist / relAlt;
}


void _gliderRuntime::movingROOT()
{
    // moving ROOT
    float3 offset = glm::lerp(x[spanSize / 2 * chordSize + 5], x[spanSize * chordSize], 0.4f);   // between pilot and wing but closer to wing
    int3 offsetI = offset;
    offset = offsetI;

    if (offset.x != 0 || offset.y != 0 || offset.z != 0)
    {
        ROOT += offset;
        for (int i = 0; i < x.size(); i++)
        {
            x[i] -= offset;
        }
    }
}


void _gliderRuntime::eye_andCamera()
{
    uint start = chordSize * spanSize;
    uint midWing = chordSize * spanSize / 2 + 12;
    // NOW reset all x to keep human at 0
    float3 pilotRight = glm::normalize(x[start + 1] - x[start + 2]);
    float3 pilotback = glm::cross(x[start + 1] - x[start], x[start + 2] - x[start]);
    float3 pilotUp = glm::normalize(glm::cross(pilotback, pilotRight));
    pilotUp = glm::lerp(pilotUp, float3(0, 1, 0), 0.1f);
    pilotback = glm::normalize(pilotback);





    static float3 smoothUp = { 0, 1, 0 };
    static float3 smoothBack = { 0, 0, -1 };
    static float3 dragShute = ROOT;
    static float3 dragShuteBack = { 0, 0, -1 };;

    switch (cameraType)
    {
    case 0:
        smoothUp = glm::lerp(smoothUp, pilotUp, 0.3f);
        smoothBack = glm::lerp(smoothBack, pilotback, 0.3f);
        EyeLocal = x[start] - smoothBack * 0.3f + smoothUp * 0.4f;

        // now add rotations
    // yaw
        camDir = smoothBack;// pilotback;// glm::normalize(smoothCamDir* cos(cameraYaw) + smoothCamRight * sin(cameraYaw));
        camUp = smoothUp;// pilotUp;
        camRight = glm::cross(camUp, camDir);
        camDir = glm::normalize(camDir * cos(cameraYaw) + camRight * sin(cameraYaw));
        camRight = glm::cross(camUp, camDir);

        // pitch
        camDir = glm::normalize(camDir * cos(cameraPitch) + camUp * sin(cameraPitch));
        camUp = glm::normalize(glm::cross(camDir, camRight));
        camRight = glm::cross(camUp, camDir);

        EyeLocal -= camDir * cameraDistance;
        break;



    case 1:
        pilotRight = glm::lerp(pilotRight, glm::cross(float3(0, 1, 0), dragShuteBack), 0.5f);
        smoothUp = glm::normalize(glm::cross(dragShuteBack, pilotRight));
        // // rotate up

        //smoothUp = glm::lerp(smoothUp, pilotUp, 0.95f);
        pilotback.y *= 0.5f;
        pilotback = glm::normalize(pilotback);
        smoothBack = glm::lerp(smoothBack, pilotback, 0.05f);

        //dragShute = glm::lerp(dragShute, x[start] + ROOT - glm::normalize(v[start]) * (5.f + 2.f * cameraDistance), 0.04f);
        dragShute = glm::lerp(dragShute, x[start] + ROOT - smoothBack * (5.f + 2.f * cameraDistance), 0.18f);
        EyeLocal = dragShute - ROOT;

        dragShuteBack = glm::lerp(dragShuteBack, smoothBack, 0.05f);

        // now add rotations

    // yaw
        camDir = dragShuteBack;// pilotback;// glm::normalize(smoothCamDir* cos(cameraYaw) + smoothCamRight * sin(cameraYaw));
        camUp = smoothUp;// pilotUp;
        camRight = glm::cross(camUp, camDir);
        camDir = glm::normalize(camDir * cos(cameraYaw) + camRight * sin(cameraYaw));
        camRight = glm::cross(camUp, camDir);

        // pitch
        camDir = glm::normalize(camDir * cos(cameraPitch) + camUp * sin(cameraPitch));
        camUp = glm::normalize(glm::cross(camDir, camRight));
        camRight = glm::cross(camUp, camDir);


        break;
    case 2:
        pilotRight = glm::lerp(pilotRight, glm::cross(float3(0, 1, 0), dragShuteBack), 0.5f);
        smoothUp = float3(0, 1, 0);// glm::normalize(glm::cross(dragShuteBack, pilotRight));
        EyeLocal = glm::lerp(x[start], x[midWing], 0.7f);
        
        dragShuteBack = glm::lerp(dragShuteBack, smoothBack, 0.05f);
        // now add rotations
    // yaw
        camDir = smoothBack;// pilotback;// glm::normalize(smoothCamDir* cos(cameraYaw) + smoothCamRight * sin(cameraYaw));
        camUp = smoothUp;// pilotUp;
        camRight = glm::cross(camUp, camDir);
        camDir = glm::normalize(camDir * cos(cameraYaw) + camRight * sin(cameraYaw));
        camRight = glm::cross(camUp, camDir);

        // pitch
        camDir = glm::normalize(camDir * cos(cameraPitch) + camUp * sin(cameraPitch));
        camUp = glm::normalize(glm::cross(camDir, camRight));
        camRight = glm::cross(camUp, camDir);

        EyeLocal -= camDir * cameraDistance * 20.f;
        break;
    }




    camPos = ROOT + EyeLocal;
}


void _gliderRuntime::solve(float _dT)
{
    setJoystick();

    //if (runcount == 0)       return; // convenient breakpoint
    //runcount--;
    frameCnt++;

    sumFwing = float3(0, 0, 0);
    sumDragWing = float3(0, 0, 0);
    AllSummedForce = float3(0, 0, 0);
    AllSummedLinedrag = float3(0, 0, 0);

    weightLines = 0;
    weightPilot = 0;
    weightWing = 0;
    pilotDrag = float3(0, 0, 0);
    lineDrag = float3(0, 0, 0);

    /*

    auto a = high_resolution_clock::now();

    solve_air(_dT);
    solve_aoa(_dT);
    solve_pressure(_dT);
    solve_surface(_dT);

    auto b = high_resolution_clock::now();

    solve_PRE(_dT);
    solve_linesTensions(_dT);
    solve_triangle(_dT);

    auto c = high_resolution_clock::now();

    solve_wing(_dT);

    auto d = high_resolution_clock::now();

    solve_POST(_dT);

    auto e = high_resolution_clock::now();
    time_ms_Aero = (float)duration_cast<microseconds>(b - a).count() / 1000.;
    time_ms_Pre = (float)duration_cast<microseconds>(c - b).count() / 1000.;
    time_ms_Wing = (float)duration_cast<microseconds>(d - c).count() / 1000.;
    time_ms_Post = (float)duration_cast<microseconds>(e - d).count() / 1000.;

    movingROOT();

    */


    /*
    // sounds
    static bool first = true;
    static float maxVal = 0;
    float soundVal = 0;
    for (int i = 0; i < spanSize * chordSize; i++)
    {
        //float thisVal = glm::length(x[i] - EyeLocal);
        float thisVal = glm::length(v[i] - v[spanSize * chordSize]);
        soundVal += thisVal - soundLowPass[i];
        soundLowPass[i] = glm::lerp(soundLowPass[i], thisVal, 0.2f);
    }
    soundVal /= (float)(spanSize * chordSize);

    sound_500Hz[soundPtr] = soundVal;
    maxVal = __max(maxVal, abs(soundVal));

    // prime my highpass
    if (first&& soundPtr == 500)
    {
        first = false;
        soundPtr = 0;
    }

    soundPtr++;
    if (soundPtr == 10000)
    {
        maxVal *= 1.5f; // to avoid clipping
        for (int i = 0; i < spanSize * chordSize; i++)
        {
            sound_500Hz[i] /= maxVal;
        }

        // save to disk
        std::ofstream ofs;
        ofs.open("E:/wing_sound.raw", std::ios::binary);
        if (ofs)
        {
            ofs.write((char*)sound_500Hz.data(), 10000 * sizeof(float));
            ofs.close();
        }
        soundPtr = 0;
    }
    */
}


void _gliderRuntime::solveConstraint(uint _i, float _step)
{
#define Cst constraints[_i]
#define idx0 Cst.idx_0
#define idx1 Cst.idx_1

    float Wsum = w[idx0] + w[idx1];
    float3 S = x[idx1] - x[idx0];
    float L = glm::length(S);
    float C = (L - Cst.l) / L * _step;
    if (C > 0) {
        C *= Cst.tensileStiff;
    }
    else {
        C *= __max(Cst.compressStiff, Cst.pressureStiff * cells[Cst.cell].pressure * cells[Cst.cell].pressure);
    }

    //   if (std::isnan(C))
    //   {
    //       bool bcm = true;
    //   }

    x[idx0] += S * C / Wsum * w[idx0];
    x[idx1] -= S * C / Wsum * w[idx1];
}


void _gliderRuntime::constraintThread_a()
{
    float dS = 0.7f;
    uint num = (constraints.size() - 3) / 2;
    for (int i = 3; i < 3 + num; i++)
    {
        solveConstraint(i, dS);
    }
}

void _gliderRuntime::constraintThread_b()
{
    float dS = 0.7f;
    uint num = (constraints.size() - 3) / 2;
    for (int i = 3 + num; i < constraints.size(); i++)
    {
        solveConstraint(i, dS);
    }
}

void _gliderRuntime::lineThread_left()
{
    for (int l = 0; l < 10; l++)
    {
        bool last = false;
        if (l == 9) last = true;
        linesLeft.solveUp(x, last);
        //linesRight.solveUp(x, last);
    }
}

void _gliderRuntime::lineThread_right()
{
    for (int l = 0; l < 10; l++)
    {
        bool last = false;
        if (l == 9) last = true;
        //linesLeft.solveUp(x, last);
        linesRight.solveUp(x, last);
    }

}



void _gliderRuntime::packWing(float3 pos, float3 norm, float3 uv)
{
    packedWing[wingVertexCount].pos = pos;
    packedWing[wingVertexCount].normal = norm;
    packedWing[wingVertexCount].uv = uv;
    wingVertexCount++;
}

void _gliderRuntime::pack_canopy()
{


    // into the new solid wing
    for (int i = 0; i < spanSize * chordSize; i++)
    {
        int chord = i % chordSize;
        int span = i / chordSize;

        n[i] = glm::normalize(glm::cross((x[cross[i].z] - x[cross[i].w]), (x[cross[i].x] - x[cross[i].y])));
    }

    wingVertexCount = 0;
    float3 P, P2, N, N2, UV, UV2;
    int c;
    float dC;   // expand ratio
    float dU = 1.f / (float)(spanSize);
    float dV = 1.f / (float)(chordSize);
    for (int s = 0; s < spanSize - 1; s++)
    {
        for (int expand = 0; expand < 4; expand++)
        {
            c = 0;

            float dP = 0.f;
            float dP2 = 0.7f;
            float dS = 0;
            float dS2 = 0.2f;
            float dR = 0;
            float dR2 = -1;
            if (expand == 1) { dS = 0.2f; dS2 = 0.5f; dP = 0.7f; dP2 = 1.f;  dR = -1; dR2 = 0; }
            if (expand == 2) { dS = 0.5f; dS2 = 0.8f; dP = 1.f; dP2 = 0.7f;  dR2 = 1; }
            if (expand == 3) { dS = 0.8f; dS2 = 1.0f; dP = 0.7f; dP2 = 0.f;  dR = 1; dR2 = 0; } // bit arb

            P = glm::lerp(x[index(s, c)], x[index(s + 1, c)], dS);
            N = glm::lerp(n[index(s, c)], n[index(s + 1, c)], dS);
            UV = float3((s + dS) * dU, c * dV, 0);
            UV2 = float3((s + dS + 0.25) * dU, c * dV, 0);

            packWing(ROOT + P, N, UV);

            for (int c = 0; c < chordSize; c++)
            {
                float PRES = pow(cells[s].pressure, 2);
                UV.y = c * dV;
                UV2.y = c * dV;
                dC = 0.04f;
                if (c <= 6) dC *= (float)c / 6.f;
                if (c >= chordSize - 7) dC *= (float)(chordSize - 1 - c) / 6.f;

                P = glm::lerp(x[index(s, c)], x[index(s + 1, c)], dS);
                N = glm::lerp(n[index(s, c)], n[index(s + 1, c)], dS);
                N = glm::normalize(N + cells[s].right * dR * PRES * 0.5f);
                P += N * dC * dP * PRES;

                packWing(ROOT + P, N, UV);

                P2 = glm::lerp(x[index(s, c)], x[index(s + 1, c)], dS2);
                N2 = glm::lerp(n[index(s, c)], n[index(s + 1, c)], dS2);
                N2 = glm::normalize(N2 + cells[s].right * dR2 * PRES * 0.5f);
                P2 += N2 * dC * dP2 * PRES;

                packWing(ROOT + P2, N2, UV2);

            }

            c = chordSize - 1;
            P2 = glm::lerp(x[index(s, c)], x[index(s + 1, c)], dS2);
            N2 = glm::lerp(n[index(s, c)], n[index(s + 1, c)], dS2);
            P2 += N2 * dC;
            packWing(ROOT + P2, N2, UV2);
        }
    }

    /*
    // chord
    for (int s = 0; s < spanSize; s++)
    {
        for (int c = 0; c < chordSize; c++)
        {
            setupVert(&ribbon[ribbonCount], c == 0 ? 0 : 1, x[index(s, c)], 0.005f);     ribbonCount++;
        }
    }

    // span
    for (int c = 0; c < chordSize; c++)
    {
        for (int s = 0; s < spanSize; s++)
        {
            setupVert(&ribbon[ribbonCount], s == 0 ? 0 : 1, x[index(s, c)], 0.005f);     ribbonCount++;
        }
    }
    */

    // triangle


    /*
    // constraints
    for (int i = 0; i < constraints.size(); i++)
    {
        //setupVert(&ribbon[ribbonCount], 0, x[constraints[i].idx_0], 0.004f, 7);     ribbonCount++;
        //setupVert(&ribbon[ribbonCount], 1, x[constraints[i].idx_1], 0.004f, 7);     ribbonCount++;
    } */
}


void _gliderRuntime::pack_lines()
{
    ribbonCount = 0;
    linesLeft.addRibbon(ribbon, x, ribbonCount);
    linesRight.addRibbon(ribbon, x, ribbonCount);
}

void _gliderRuntime::pack_feedback()
{
    uint triangle = spanSize * chordSize;
    //setupVert(&ribbon[ribbonCount], 0, x[triangle], 0.001f, 1);     ribbonCount++;
    //setupVert(&ribbon[ribbonCount], 1, x[triangle] + pilotDrag * 0.05f, 0.1f, 1);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle], 0.01f, 7);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 1], 0.01f, 7);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle], 0.01f, 7);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 2], 0.01f, 7);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle + 1], 0.01f, 7);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 2], 0.01f, 7);     ribbonCount++;

    // normals and tangetns
    int ribcount = 0;
    for (int s = 0; s < spanSize; s++)
    {
        for (int c = 0; c < chordSize; c++)
        {
            ribcount++;
            ribcount = ribcount % 5;
            //if (ribcount == 0)
            {
                uint color = 1;
                if (c < 12) color = 2;

                //setupVert(&ribbon[ribbonCount], 0, x[index(s, c)], 0.001f, color);     ribbonCount++;
                //setupVert(&ribbon[ribbonCount], 1, x[index(s, c)] + cp_feedback[index(s, c)] * 0.4f, 0.019f, color);     ribbonCount++;
                // 
                //setupVert(&ribbon[ribbonCount], 1, x[index(s, c)] + t[index(s, c)] * 20.f, 0.01f, 4);     ribbonCount++;
            }
        }
    }

    for (int s = 0; s < spanSize; s++)
    {
        uint color = 1;
        //setupVert(&ribbon[ribbonCount], 0, x[index(s, 0)], 0.001f, color);     ribbonCount++;
        //setupVert(&ribbon[ribbonCount], 1, x[index(s, 0)] + cells[s].right * 0.4f, 0.019f, color);     ribbonCount++;
    }


    /*
    for (int s = 0; s < spanSize; s += 1)
    {
        setupVert(&ribbon[ribbonCount], 0, x[index(s, 0)], 0.005f, 2);     ribbonCount++;
        setupVert(&ribbon[ribbonCount], 1, x[index(s, 0)] + cells[s].rel_wind * 0.2f, 0.016f, 2);     ribbonCount++;

        setupVert(&ribbon[ribbonCount], 0, x[index(s, 0)], 0.005f, 2);     ribbonCount++;
        setupVert(&ribbon[ribbonCount], 1, x[index(s, 0)] - cells[s].rel_wind * 0.3f, 0.016f, 2);     ribbonCount++;

        setupVert(&ribbon[ribbonCount], 0, x[index(s, 0)], 0.001f, 1);     ribbonCount++;
        setupVert(&ribbon[ribbonCount], 1, x[index(s, 0)] + pilotDrag * 0.05f, 0.016f, 1);     ribbonCount++;

        setupVert(&ribbon[ribbonCount], 0, x[index(s, 0)], 0.005f, 7);     ribbonCount++;
        setupVert(&ribbon[ribbonCount], 1, x[index(s, 0)] + cells[s].front * 3.3f, 0.016f, 7);     ribbonCount++;

        setupVert(&ribbon[ribbonCount], 0, x[index(s, 0)], 0.005f, 7);     ribbonCount++;
        setupVert(&ribbon[ribbonCount], 1, x[index(s, 0)] + cells[s].up * 3.3f, 0.016f, 7);     ribbonCount++;

        //        setupVert(&ribbon[ribbonCount], 0, x[index(s, 12)], 0.005f, 1);     ribbonCount++;
        //        setupVert(&ribbon[ribbonCount], 1, x[index(s, 12)] + cells[s].v * 0.2f, 0.02f, 1);     ribbonCount++;
    } */


    // WIND
    {
        uint color = 3;
        float3 W = windTerrain.W(ROOT + x[triangle]);
        setupVert(&ribbon[ribbonCount], 0, x[triangle], 0.001f, color);     ribbonCount++;
        setupVert(&ribbon[ribbonCount], 1, x[triangle] + W * 1.f, 0.029f, color);     ribbonCount++;


        /*
        color = 1;
        setupVert(&ribbon[ribbonCount], 0, x[triangle], 0.01f, color);     ribbonCount++;
        setupVert(&ribbon[ribbonCount], 1, x[triangle] + pilotWind * 5.f, 0.1f, color);     ribbonCount++;

        setupVert(&ribbon[ribbonCount], 0, x[0 * chordSize + 12], 0.01f, color);     ribbonCount++;
        setupVert(&ribbon[ribbonCount], 1, x[0 * chordSize + 12] + wingWind[0] * 5.f, 0.1f, color);     ribbonCount++;

        setupVert(&ribbon[ribbonCount], 0, x[25 * chordSize + 12], 0.01f, color);     ribbonCount++;
        setupVert(&ribbon[ribbonCount], 1, x[25 * chordSize + 12] + wingWind[1] * 5.f, 0.1f, color);     ribbonCount++;

        setupVert(&ribbon[ribbonCount], 0, x[49 * chordSize + 12], 0.01f, color);     ribbonCount++;
        setupVert(&ribbon[ribbonCount], 1, x[49 * chordSize + 12] + wingWind[2] * 5.f, 0.1f, color);     ribbonCount++;
        */
    }
    /*
    for (int y = -10; y < 10; y++)
    {
        for (int x = -10; x < 10; x++)
        {
            float3 P = ROOT + float3(x * 10, 0, y * 10);
            P.y = windTerrain.H(P) + 10.f;  // 10 mete above terrain
            float3 W = windTerrain.W(P);
        }
    }
    */
}

void _gliderRuntime::pack()
{
    for (int i = 0; i < ribbonCount; i++)
    {
        packedRibbons[i] = ribbon[i].pack();
    }
    changed = true;
}

void _gliderRuntime::load()
{
}

void _gliderRuntime::save()
{
}






void _gliderRuntime::exportGliderShape()
{
    aiScene* scene = new aiScene;
    scene->mRootNode = new aiNode();

    scene->mNumMaterials = 1;
    scene->mMaterials = new aiMaterial * [scene->mNumMaterials];
    for (int i = 0; i < scene->mNumMaterials; i++)
        scene->mMaterials[i] = new aiMaterial();

    scene->mNumMeshes = 1;
    scene->mMeshes = new aiMesh * [scene->mNumMeshes];
    scene->mRootNode->mNumMeshes = scene->mNumMeshes;   // insert all into root
    scene->mRootNode->mMeshes = new unsigned int[scene->mRootNode->mNumMeshes];
    for (int i = 0; i < scene->mNumMeshes; i++) {
        scene->mMeshes[i] = new aiMesh();
        scene->mMeshes[i]->mMaterialIndex = i;
        scene->mRootNode->mMeshes[i] = i;
    }


    for (int i = 0; i < scene->mNumMeshes; i++)
    {
        auto pMesh = scene->mMeshes[i];
        pMesh->mVertices = new aiVector3D[spanSize * chordSize];       // for now just insert all verts
        memcpy(pMesh->mVertices, x.data(), sizeof(float3) * spanSize * chordSize);
        pMesh->mNumVertices = spanSize * chordSize;

        pMesh->mNumFaces = (spanSize - 1) * (chordSize - 1);
        pMesh->mFaces = new aiFace[pMesh->mNumFaces];
        pMesh->mPrimitiveTypes = aiPrimitiveType_POLYGON;

        //for (uint i = 0; i < pMesh->mNumFaces; i++)
        for (uint s = 0; s < spanSize - 1; s++)
            for (uint c = 0; c < chordSize - 1; c++)
            {
                {
                    uint idx = s * (chordSize - 1) + c;
                    aiFace& face = pMesh->mFaces[idx];

                    face.mIndices = new unsigned int[4];
                    face.mNumIndices = 4;

                    face.mIndices[0] = s * chordSize + c;
                    face.mIndices[1] = s * chordSize + c + 1;
                    face.mIndices[2] = (s + 1) * chordSize + c + 1;
                    face.mIndices[3] = (s + 1) * chordSize + c;

                    // close the back
                    if (c == chordSize - 2)
                    {
                        face.mIndices[1] = s * chordSize + 0;
                        face.mIndices[2] = (s + 1) * chordSize + 0;
                    }
                }
            }
    }

    Assimp::Exporter exp;
    exp.Export(scene, "obj", "E:/glider.obj");
}


void _gliderRuntime::setJoystick()
{

    static bool gamePadLeft;

    static int controllerId = -1;
    if (controllerId == -1)
    {

        for (DWORD i = 0; i < XUSER_MAX_COUNT && controllerId == -1; i++)
        {
            XINPUT_STATE state;
            ZeroMemory(&state, sizeof(XINPUT_STATE));

            if (XInputGetState(i, &state) == ERROR_SUCCESS)
                controllerId = i;
        }
    }
    else
    {
        XINPUT_STATE state;
        if (XInputGetState(controllerId, &state) == ERROR_SUCCESS)
        {
            if ((!gamePadLeft && state.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0)
            {
                //exportGliderShape();
                cameraType = (cameraType + 1) % 3;
            }
            gamePadLeft = state.Gamepad.wButtons & XINPUT_GAMEPAD_X;

            

            static SHORT normXZero = 0;
            static SHORT normYZero = 0;
            if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0)
            {
                normXZero = state.Gamepad.sThumbRX;
                normYZero = state.Gamepad.sThumbRY;
            }




            float normLX = fmaxf(-1, (float)(state.Gamepad.sThumbRX - normXZero) / 32767);
            float normLY = fmaxf(-1, (float)(state.Gamepad.sThumbRY - normYZero) / 32767);
            //cameraDistance -= ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0) * 0.2001f;
            //cameraDistance += ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0) * 0.2001f;
            if (abs(normLX) < 0.05f)normLX = 0;
            if (abs(normLY) < 0.05f)normLY = 0;

            if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) cameraDistance -= 0.01f;
            if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) cameraDistance += 0.01f;
            cameraDistance = glm::clamp(cameraDistance, 0.01f, 10.f);

            cameraYaw -= normLX * 0.003f;
            cameraPitch -= normLY * 0.0015f;
            cameraPitch = glm::clamp(cameraPitch, -1.3f, 1.3f);

            //if (runcount == 0)  return;


            float normRX = fmaxf(-1, (float)state.Gamepad.sThumbLX / 32767);
            weightShift = glm::lerp(weightShift, normRX * 0.2f + 0.5f, 0.01f);
            constraints[1].l = weightShift * weightLength;
            constraints[2].l = (1.0f - weightShift) * weightLength;

            if (pBrakeLeft)
            {

                leftTrigger = state.Gamepad.bLeftTrigger;
                rightTrigger = state.Gamepad.bRightTrigger;
                float leftTrigger = pow((float)state.Gamepad.bLeftTrigger / 255, 1.0f);
                float rightTrigger = pow((float)state.Gamepad.bRightTrigger / 255, 1.0f);

                if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0)
                {
                    pEarsLeft->length = maxEars - leftTrigger * 1.2f;
                    pEarsRight->length = maxEars - rightTrigger * 1.2f;
                }
                else if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0)
                {
                    pSLeft->length = maxS - leftTrigger;
                    pSRight->length = maxS - rightTrigger;
                }
                else
                {
                    pBrakeLeft->length = glm::lerp(pBrakeLeft->length, maxBrake - leftTrigger * 0.7f, 0.1f);
                    pBrakeRight->length = glm::lerp(pBrakeRight->length, maxBrake - rightTrigger * 0.7f, 0.1f);
                }


                if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_A) == 0)
                {
                    pEarsLeft->length = glm::lerp(pEarsLeft->length, maxEars, 0.01f);
                    pEarsRight->length = glm::lerp(pEarsRight->length, maxEars, 0.01f);
                }

                if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_B) == 0)
                {
                    pSLeft->length = glm::lerp(pSLeft->length, maxS, 0.01f);
                    pSRight->length = glm::lerp(pSRight->length, maxS, 0.01f);
                }



                rumbleLeft = pBrakeLeft->tencileForce;
                rumbleLeftAVS = glm::lerp(rumbleLeftAVS, rumbleLeft, 0.8f);
                //rumbleLeft = (pEarsLeft->tencileForce / 20.f) + (rumbleLeft - rumbleLeftAVS) / rumbleLeftAVS;
                rumbleLeft = abs((rumbleLeft - rumbleLeftAVS) * 0.71f);
                rumbleLeft = pow(rumbleLeft, 0.3f);
                rumbleLeft = __max(0, rumbleLeft);
                rumbleLeft = __min(1, rumbleLeft);
                //rumbleLeft = 0;// leftTrigger;


                rumbleRight = 0;// rightTrigger;
                rumbleRight = pBrakeRight->tencileForce;
                rumbleRightAVS = glm::lerp(rumbleRightAVS, rumbleRight, 0.8f);
                //rumbleLeft = (pEarsLeft->tencileForce / 20.f) + (rumbleLeft - rumbleLeftAVS) / rumbleLeftAVS;
                rumbleRight = abs((rumbleRight - rumbleRightAVS) * 0.71f);
                rumbleRight = pow(rumbleRight, 0.3f);
                rumbleRight = __max(0, rumbleRight);
                rumbleRight = __min(1, rumbleRight);

                XINPUT_VIBRATION vib;
                rumbleLeft = 0;
                rumbleRight = 0;
                vib.wLeftMotorSpeed = (WORD)(rumbleLeft * 65535);
                vib.wRightMotorSpeed = (WORD)(rumbleRight * 65535);
                XInputSetState(controllerId, &vib);

                //XINPUT_VIBRATION_EX v2;
                //XInputSetStateEx(controllerId, &v2);
            }
            else
            {
                setupLines();
            }
        }
    }


}



#pragma optimize("", off)



aiVector3D _swissBuildings::reproject(aiVector3D _v)
{
    float2 rp_A = float2(-20906.8073893663f, 21854.7731031066f);
    float2 rp_B = float2(-21739.4126790621f, -18140.4947773098f);
    float2 rp_C = float2(19087.8133413223f, 21022.9225606067f);
    float2 rp_D = float2(18256.7017062721f, -18972.3242194264f);
    float rp_left = 2702000;
    float rp_right = 2742000;
    float rp_dW = rp_right - rp_left;
    float rp_top = 1258000;
    float rp_bottom = 1218000;
    float rp_dH = rp_top - rp_bottom;

    _v.z *= -1;
    float dx = (_v.x - rp_left) / rp_dW;
    float dz = (_v.z - rp_bottom) / rp_dH;
    float2 A1 = glm::lerp(rp_B, rp_D, dx);    // bottom
    float2 A2 = glm::lerp(rp_A, rp_C, dx);    // tpp
    float2 P = glm::lerp(A1, A2, dz);

    aiVector3D vout = _v;
    vout.x = P.x;
    vout.z = -P.y;
    return vout;
}


glm::dvec3 _swissBuildings::reproject(double x, double y, double z)
{
    glm::dvec2 rp_A = float2(-20906.8073893663, 21854.7731031066);
    glm::dvec2 rp_B = float2(-21739.4126790621, -18140.4947773098);
    glm::dvec2 rp_C = float2(19087.8133413223, 21022.9225606067);
    glm::dvec2 rp_D = float2(18256.7017062721, -18972.3242194264);
    double rp_left = 2702000;
    double rp_right = 2742000;
    double rp_dW = rp_right - rp_left;
    double rp_top = 1258000;
    double rp_bottom = 1218000;
    double rp_dH = rp_top - rp_bottom;

    //_v.z *= -1;
    double dx = (x - rp_left) / rp_dW;
    double dz = (z - rp_bottom) / rp_dH;
    glm::dvec2 A1 = glm::lerp(rp_B, rp_D, dx);    // bottom
    glm::dvec2 A2 = glm::lerp(rp_A, rp_C, dx);    // tpp
    glm::dvec2 P = glm::lerp(A1, A2, dz);

    glm::dvec3 vout;
    vout.x = P.x;
    vout.y = y;
    vout.z = -P.y;
    return vout;
}

glm::dvec3 _swissBuildings::reprojectLL(double latt, double lon, double alt)
{
    glm::dvec2 rp_A = float2(-12832.706486136117746, 14467.072170339033619);
    glm::dvec2 rp_B = float2(17361.895270298864489, 14478.709804310250547);
    glm::dvec2 rp_C = float2(17489.612316045942862, -28879.451053363558458);
    glm::dvec2 rp_D = float2(-12927.105836690829165, -28891.100647661860421);
    double rp_left = 8.9;
    double rp_right = 9.3;
    double rp_dW = rp_right - rp_left;
    double rp_top = 47.4;
    double rp_bottom = 47.01;
    double rp_dH = rp_top - rp_bottom;


    double dx = (lon - rp_left) / rp_dW;
    double dz = (latt - rp_bottom) / rp_dH;
    glm::dvec2 A1 = glm::lerp(rp_D, rp_C, dx);    // bottom
    glm::dvec2 A2 = glm::lerp(rp_A, rp_B, dx);    // tpp
    glm::dvec2 P = glm::lerp(A1, A2, dz);

    glm::dvec3 vout;
    vout.x = P.x;
    vout.y = alt;
    vout.z = -P.y;
    return vout;
}




//#include "json.hpp
#include "json.hpp"
using json = nlohmann::json;

//#include <rapidjson/document.h>
//#include "rapidjson/writer.h"
//#include "rapidjson/stringbuffer.h"
//using namespace rapidjson;

inline bool compare(aiVector3D a, float3 b)
{
    float3 diff = glm::abs(float3(a.x, a.y, a.z) - b);
    float manhattan = diff.x + diff.y + diff.z;
    if (manhattan < 0.01f) return true;
    return false;
}


void _swissBuildings::processGeoJSON(std::string _path, std::string _name)
{


    std::ifstream fJson(_path + _name + ".geojson");
    std::stringstream buffer;
    buffer << fJson.rdbuf();

    auto json = json::parse(buffer.str());


    float3 averagePos;
    all_verts.clear();
    all_faces.clear();

    for (auto features : json["features"])
    {
        auto properties = features["properties"];
        auto G = features["geometry"];
        auto uuid = properties["UUID"];

        auto type = G["type"];
        auto coordinates = G["coordinates"];


        auto A1 = coordinates.array();
        int numFaces = coordinates.size();
        averagePos = float3(0, 0, 0);

        for (int a = 0; a < numFaces; a++)
        {
            auto X = coordinates[a];
            auto Y = X[0];
            int numVerts = Y.size();
            if (numVerts != 4)
            {
                bool bcm = true;        // seems fine BUT WHY
            }
            faces.push_back(vertsDouble.size() + 3);
            faces.push_back(vertsDouble.size() + 2);
            faces.push_back(vertsDouble.size() + 1);
            faces.push_back(vertsDouble.size() + 0);    // flip faces
            for (int c = 0; c < numVerts; c++)
            {
                auto Z = Y[c];
                vertsDouble.push_back(reproject(Z[0], Z[2], Z[1]));
            }

            faceInfo.emplace_back();
            _faceInfo& F = faceInfo.back();
            F.vertIndex[0] = vertsDouble.size() - 1;
            F.vertIndex[1] = vertsDouble.size() - 2;
            F.vertIndex[2] = vertsDouble.size() - 3;
            F.vertIndex[3] = vertsDouble.size() - 4; // also flipped
            float3 Va = vertsDouble[F.vertIndex[0]] - vertsDouble[F.vertIndex[2]];
            float3 Vb = vertsDouble[F.vertIndex[1]] - vertsDouble[F.vertIndex[3]];
            F.normal = glm::normalize(glm::cross(Va, Vb));
            if (F.normal.y < -0.5f)         F.type = 0;     //floor
            else if (F.normal.y > 0.05f)    F.type = 2;     //roof
            else                            F.type = 1;     //walls
        }

        //bottom center
        glm::dvec3 center = glm::dvec3(0, 0, 0);
        for (int i = 0; i < vertsDouble.size(); i++)
        {
            center += vertsDouble[i];
        }
        center /= (double)vertsDouble.size();
        for (int i = 0; i < vertsDouble.size(); i++)
        {
            center.y = __min(center.y, vertsDouble[i].y);
        }
        for (int i = 0; i < vertsDouble.size(); i++)
        {
            verts.push_back(float3(vertsDouble[i] - center));
        }

        //findWalls();
        //exportBuilding(_path + _name + "_" + std::to_string(numBuildings), verts, faces);

        // now updfate the hole
        uint baseVertex = all_verts.size();
        //all_verts.insert(all_verts.end(), verts.begin(), verts.end());
        float3 centerF = center;
        for (auto& vd : vertsDouble) {
            all_verts.push_back(vd);
        }
        for (auto& ff : faces) {
            all_faces.push_back(ff + baseVertex);
        }

        numBuildings++;
        verts.clear();
        vertsDouble.clear();
        faces.clear();
        faceInfo.clear();
        wallInfo.clear();
    }

    exportBuilding(_path + _name, all_verts, all_faces);
}


void _swissBuildings::findWalls()
{
    for (auto& F : faceInfo)
    {
        F.planeW = glm::dot(verts[F.vertIndex[0]], F.normal);
    }

    // lets find flat walls and roofs
    wallInfo.clear();
    float3 lastNormal = float3(0, 0, 0);
    float lastW = 0;
    uint facecount = 0;
    for (auto& F : faceInfo)
    {
        if (F.type == 1)
        {
            float tanTheta = glm::dot(F.normal, lastNormal);
            float inPlane = glm::dot(verts[F.vertIndex[0]], lastNormal) - lastW;
            if (tanTheta < 0.95f || inPlane > 0.5f)
            {
                wallInfo.emplace_back();
                wallInfo.back().normal = F.normal;
                wallInfo.back().W = F.planeW;
                wallInfo.back().faces.push_back(facecount);
                lastNormal = F.normal;
                lastW = F.planeW;
            }
            else
            {
                wallInfo.back().faces.push_back(facecount);
            }
        }
        facecount++;
    }

    float3 up = float3(0, 1, 0);
    for (auto& W : wallInfo)
    {
        float3 min = float3(100000, 100000, 100000);
        float3 max = -1.f * min;
        float3 right = glm::normalize(glm::cross(up, W.normal));

        for (auto& F : W.faces)
        {
            for (int vrt = 0; vrt < 4; vrt++)
            {
                float u = glm::dot(verts[faceInfo[F].vertIndex[vrt]], right);
                float v = glm::dot(verts[faceInfo[F].vertIndex[vrt]], up);
                min.x = __min(min.x, u);
                min.y = __min(min.y, v);
                max.x = __max(max.x, u);
                max.y = __max(max.y, v);
            }
        }
        W.length = max.x - min.x;
        W.height = max.y - min.y;
        W.stories = (int)(W.height / 3.f);

        findWallEdges(W);
    }

    /*
    for (auto& W : wallInfo)
    {
        for (auto& F : W.faces)
        {
            for (int vrt = 0; vrt < 4; vrt++)
            {
                verts[faceInfo[F].vertIndex[vrt]] += faceInfo[F].normal;
            }
        }
    }*/
}


void _swissBuildings::findWallEdges(_wallinfo& _wall)
{
    // also combine walls, I am not sure our method finds them all, just test in plane
    for (auto& F : _wall.faces)
    {
        auto& face = faceInfo[F];
        for (int vrt = 0; vrt < 4; vrt++)
        {
            auto& edge = face.edges[vrt];
            edge.normal = verts[face.vertIndex[(vrt + 1) % 4]] - verts[face.vertIndex[vrt]];
            edge.length = glm::length(edge.normal);
            edge.normal = glm::normalize(edge.normal);

            if (edge.normal.y > 0.9f) edge.type = leading;
            if (edge.normal.y < -0.9f) edge.type = trailing;
            if (glm::dot(edge.normal, _wall.right) > 0.9f)  edge.type = roof;
            if (glm::dot(edge.normal, _wall.right) < -0.9f)  edge.type = edgefloor;
        }
    }

    // now test for shared edges
}


void _swissBuildings::exportBuilding(std::string _path, std::vector<float3> _v, std::vector<unsigned int>_f)
{
    aiScene* scene = new aiScene;
    scene->mRootNode = new aiNode();


    scene->mNumMaterials = 3;
    scene->mMaterials = new aiMaterial * [scene->mNumMaterials];
    for (int i = 0; i < scene->mNumMaterials; i++)
        scene->mMaterials[i] = new aiMaterial();

    scene->mNumMeshes = 1;
    scene->mMeshes = new aiMesh * [scene->mNumMeshes];
    scene->mRootNode->mNumMeshes = scene->mNumMeshes;   // insert all into root
    scene->mRootNode->mMeshes = new unsigned int[scene->mRootNode->mNumMeshes];
    for (int i = 0; i < scene->mNumMeshes; i++) {
        scene->mMeshes[i] = new aiMesh();
        scene->mMeshes[i]->mMaterialIndex = i;
        scene->mRootNode->mMeshes[i] = i;
    }


    for (int i = 0; i < scene->mNumMeshes; i++)
    {
        auto pMesh = scene->mMeshes[i];
        pMesh->mVertices = new aiVector3D[_v.size()];       // for now just insert all verts
        memcpy(pMesh->mVertices, _v.data(), sizeof(float3) * _v.size());
        pMesh->mNumVertices = _v.size();

#ifdef WALLSPLIT
        uint numType = 0;
        for (auto& F : faceInfo)
        {
            if (F.type == i) numType++;
        }
        pMesh->mNumFaces = numType;// _f.size() / 4;
        pMesh->mFaces = new aiFace[pMesh->mNumFaces];
        pMesh->mPrimitiveTypes = aiPrimitiveType_POLYGON;

        uint faceNum = 0;
        for (auto& F : faceInfo)
        {
            if (F.type == i)
            {
                aiFace& face = pMesh->mFaces[faceNum];

                face.mIndices = new unsigned int[4];
                face.mNumIndices = 4;

                face.mIndices[0] = F.vertIndex[0];
                face.mIndices[1] = F.vertIndex[1];
                face.mIndices[2] = F.vertIndex[2];
                face.mIndices[3] = F.vertIndex[3];

                faceNum++;
    }
}
#else
        pMesh->mNumFaces = _f.size() / 4;
        pMesh->mFaces = new aiFace[pMesh->mNumFaces];
        pMesh->mPrimitiveTypes = aiPrimitiveType_POLYGON;

        for (uint i = 0; i < pMesh->mNumFaces; i++)
        {
            aiFace& face = pMesh->mFaces[i];

            face.mIndices = new unsigned int[4];
            face.mNumIndices = 4;

            face.mIndices[0] = _f[i * 4 + 0];
            face.mIndices[1] = _f[i * 4 + 1];
            face.mIndices[2] = _f[i * 4 + 2];
            face.mIndices[3] = _f[i * 4 + 3];
        }
#endif
}

    Assimp::Exporter exp;
    exp.Export(scene, "obj", _path + ".obj");
}


void _swissBuildings::processWallRoof(std::string _path)
{
    // misuse for thermal process
    int cnt, alt1, alt2, alt3, year, month, day, hours, minutes, flightseconds, id;
    double latt, lon, lattBottom, lonBottom, lattTop, lonTop;
    std::string date, time;

    uint inCount = 0;
    uint dCount = 0;
    float4 data[16384][3];

    std::ofstream ofs, ofsBin;
    ofs.open("E:/thermal_Summer_West.csv");
    ofsBin.open("E:/thermal_waalenstadt_July_Aug_Afternoons_WindEast.raw", std::ios::binary);
    std::ifstream ifs;
    //ifs.open("E:/thermal_waalenstadt.csv");
    ifs.open("E:/thermal_Summer.csv");

    if (ifs)
    {
        while (!ifs.eof())
        {
            ifs >> cnt;
            ifs >> latt >> lon >> alt1;
            ifs >> lattBottom >> lonBottom >> alt2;
            ifs >> lattTop >> lonTop >> alt3;
            //ifs >> date >> time;
            ifs >> year >> month >> day >> hours >> minutes;
            ifs >> flightseconds >> id;

            //if (lon > 9.25f && lon < 9.333 && latt > 47.1 && latt < 47.2 && month >=7 && month <=8 && hours >= 12 &&
            //    lonTop > lonBottom)

            float3 ground = reprojectLL(latt, lon, alt1);
            float3 bottom = reprojectLL(lattBottom, lonBottom, alt2);
            float3 top = reprojectLL(lattTop, lonTop, alt3);
            float3 dir = glm::normalize(top - bottom);
            float3 selectDir = normalize(float3(1, 5, -0.5));
            float DOT = dot(dir, selectDir);

            inCount++;

            //if (lon > 9.00f && lon < 9.333 && latt > 47.1 && latt < 47.4 &&
            //    month >= 7 && month <= 8 && hours >= 12) // && dir.y > 0.9f)
            if (lon > 9.00f && lon < 9.333 && latt > 47.1 && latt < 47.3 &&
                month >= 6 && month <= 9 && DOT > 0.95f)
            {
                ofs << cnt << " ";
                ofs << latt << " " << lon << " " << alt1 << " ";
                ofs << lattBottom << " " << lonBottom << " " << alt2 << " ";
                ofs << lattTop << " " << lonTop << " " << alt3 << " ";
                //ofs << date << " " << time << " ";
                ofs << year << " " << month << " " << day << " " << hours << " " << minutes << "  ";
                ofs << flightseconds << " " << id << "\n";

                /*
                float3 ground = reprojectLL(latt, lon, alt1);
                float3 bottom = reprojectLL(lattBottom, lonBottom, alt2);
                float3 top = reprojectLL(lattTop, lonTop, alt3);
                float speed = (float)(alt3 - alt2) / (float)flightseconds;
                //ofsBin << ground.x << ground.y << ground.z << speed;
                //ofsBin << bottom.x << bottom.y << bottom.z << speed;
                //ofsBin << top.x << top.y << top.z << speed;

                */
                float speed = (float)(alt3 - alt2) / (float)flightseconds;
                data[dCount][0] = float4(ground.x, ground.y, ground.z, speed);
                data[dCount][1] = float4(bottom.x, bottom.y, bottom.z, speed);
                data[dCount][2] = float4(top.x, top.y, top.z, speed);
                dCount++;

            }
        }

        ofsBin.write((char*)data, sizeof(float4) * 3 * dCount);

        ifs.close();
        ofs.close();
        ofsBin.close();
    }
    return;




    Assimp::Importer importer;
    unsigned int flags = aiProcess_Triangulate | aiProcess_PreTransformVertices;
    const aiScene* sceneWall = importer.ReadFile((_path + "optimized.dae").c_str(), flags);
    //const aiScene* sceneRoof = importer.ReadFile((_path + "roofs_opt.dae").c_str(), flags);
    uint numfaces = 0;
    std::vector<_buildingVertex> VERTS;

    //if (sceneWall && sceneRoof)
    if (sceneWall)
    {
        for (int m = 0; m < sceneWall->mNumMeshes; m++)
        {
            numfaces += sceneWall->mMeshes[m]->mNumFaces;
        }
        /*for (int m = 0; m < sceneRoof->mNumMeshes; m++)
        {
            numfaces += sceneRoof->mMeshes[m]->mNumFaces;
        }*/

        VERTS.resize(numfaces * 3);

        uint vidx = 0;
        for (int m = 0; m < sceneWall->mNumMeshes; m++)
        {
            aiMesh* M = sceneWall->mMeshes[m];

            for (uint j = 0; j < M->mNumFaces; j++)
            {
                aiFace face = M->mFaces[j];
                for (int idx = 0; idx < face.mNumIndices; idx++)
                {
                    uint i = face.mIndices[idx];
                    VERTS[vidx].material = 0;
                    VERTS[vidx].pos = *(float3*)&M->mVertices[i];
                    VERTS[vidx].normal = *(float3*)&M->mNormals[i];
                    //VERTS[vidx].uv = *(float3*)&M->mTextureCoords[0][i];
                    vidx++;
                }
            }
        }
        /*for (int m = 0; m < sceneRoof->mNumMeshes; m++)
        {
            aiMesh* M = sceneRoof->mMeshes[m];

            for (uint j = 0; j < M->mNumFaces; j++)
            {
                aiFace face = M->mFaces[j];
                for (int idx = 0; idx < face.mNumIndices; idx++)
                {
                    uint i = face.mIndices[idx];
                    VERTS[vidx].material = 0;
                    VERTS[vidx].pos = *(float3*)&M->mVertices[i];
                    VERTS[vidx].normal = *(float3*)&M->mNormals[i];
                    VERTS[vidx].uv = *(float3*)&M->mTextureCoords[0][i];
                    vidx++;
                }
            }
        }*/


        std::ofstream ofs;
        ofs.open(_path + "rappersville.raw", std::ios::binary);
        if (ofs)
        {
            ofs.write((char*)VERTS.data(), VERTS.size() * sizeof(_buildingVertex));
            ofs.close();
        }
        ofs.open(_path + "rappersville.info.txt");
        if (ofs)
        {
            ofs << VERTS.size() << "\n";
            ofs << "1\n";
            ofs.close();
        }
    }

}



void _swissBuildings::process(std::string _path)
{
    Assimp::Importer importer;
    unsigned int flags =
        aiProcess_FlipUVs |
        aiProcess_Triangulate |
        aiProcess_PreTransformVertices |
        aiProcess_GenNormals;
    //  but remove normals from file in assimp

    const aiScene* scene = nullptr;
    scene = importer.ReadFile(_path.c_str(), flags);

    if (scene)
    {
        for (int m = 0; m < scene->mNumMeshes; m++)
        {
            aiMesh* M = scene->mMeshes[m];
            //M->mMaterialIndex SINGLE material per mesh so we aresplit already here
            // this is part of a house, later read original and do splits here, I am sure we can figure out when we leave a house

            // reproject
            for (int v = 0; v < M->mNumVertices; v++)
            {
                M->mVertices[v] = reproject(M->mVertices[v]);
            }
        }


        Assimp::Exporter exporter;
        exporter.Export(scene, "fbx", _path + ".reproj.fbx");
    }

    // lest see if we can identify individual buildings
    if (scene)
    {
        float3 V[1000];
        uint faces[1000][3];
        uint numV = 0;
        uint numF = 0;
        uint numBuildings = 0;
        for (int m = 0; m < scene->mNumMeshes; m++)
        {
            aiMesh* M = scene->mMeshes[m];

            for (uint j = 0; j < M->mNumFaces; j++)
            {
                aiFace& face = M->mFaces[j];
                bool found = false;
                for (int k = 0; k < numV; k++)
                {
                    if (compare(M->mVertices[face.mIndices[0]], V[k]) ||
                        compare(M->mVertices[face.mIndices[1]], V[k]) ||
                        compare(M->mVertices[face.mIndices[2]], V[k]))
                    {
                        V[numV + 0] = *((float3*)&M->mVertices[face.mIndices[0]]);
                        V[numV + 1] = *((float3*)&M->mVertices[face.mIndices[0]]);
                        V[numV + 2] = *((float3*)&M->mVertices[face.mIndices[0]]);
                        numV += 3;
                        numF++;
                    }
                }
            }
        }
    }

    if (scene)
    {
        uint numfaces = 0;
        for (int m = 0; m < scene->mNumMeshes; m++)
        {
            numfaces += scene->mMeshes[m]->mNumFaces;
        }

        std::vector<_buildingVertex> VERTS;
        VERTS.resize(numfaces * 3);

        uint vidx = 0;
        for (int m = 0; m < scene->mNumMeshes; m++)
        {
            aiMesh* M = scene->mMeshes[m];

            for (uint j = 0; j < M->mNumFaces; j++)
            {
                aiFace face = M->mFaces[j];
                for (int idx = 0; idx < face.mNumIndices; idx++)
                {
                    aiVector3D V = M->mVertices[face.mIndices[idx]];
                    aiVector3D N = M->mNormals[face.mIndices[idx]];
                    aiVector3D U = M->mTextureCoords[0][face.mIndices[idx]];
                    VERTS[vidx].pos = float3(V.x, V.y, V.z);
                    VERTS[vidx].material = M->mMaterialIndex;
                    VERTS[vidx].normal = float3(N.x, N.y, N.z);
                    VERTS[vidx].uv = float3(U.x, U.y, U.z);
                    vidx++;
                }
            }
        }

        std::ofstream ofs;
        ofs.open(_path + ".raw", std::ios::binary);
        if (ofs)
        {
            ofs.write((char*)VERTS.data(), VERTS.size() * sizeof(_buildingVertex));
            ofs.close();
        }
        ofs.open(_path + ".info.txt");
        if (ofs)
        {
            ofs << VERTS.size() << "\n";
            ofs << scene->mNumMeshes / 2 << "\n";
            ofs.close();
        }
    }

}





//#pragma optimize("", off)

#include <iostream>
#include <fstream>
void _windTerrain::load(std::string filename, float3 _wind)
{
    int edgeCnt = 0;
    int shadowEdge = 0;

    std::ifstream ifs;
    ifs.open(filename, std::ios::binary);

    if (ifs)
    {
        ifs.read((char*)height, 4096 * 4096 * 4);
        ifs.close();

        for (int y = 1; y < 4095; y++)
        {
            for (int x = 1; x < 4095; x++)
            {
                float3 A = float3(19.53125, height[y + 1][x + 1], 19.53125) - float3(0, height[y - 1][x - 1], 0);
                float3 B = float3(19.53125, height[y - 1][x + 1], 0) - float3(0, height[y + 1][x - 1], 19.53125);
                norm[y][x] = glm::normalize(glm::cross(A, B));
                //norm[y][x].z *= -1;

                Nc[y][x][0] = (int)((norm[y][x].x * 0.5f + 0.5f) * 255.f);
                Nc[y][x][1] = (int)((norm[y][x].z * 0.5f + 0.5f) * 255.f);
                Nc[y][x][2] = (int)((norm[y][x].y * 0.5f + 0.5f) * 255.f);

                image[y][x] = 0;
            }
        }

        //edges
        for (int i = 0; i < 4096; i++)
        {
            norm[i][0] = norm[i][1];
            norm[i][4095] = norm[i][4094];
        }
        for (int i = 0; i < 4096; i++)
        {
            norm[0][i] = norm[1][i];
            norm[4095][i] = norm[4094][i];
        }


        for (int y = 0; y < 4096; y++)
        {
            for (int x = 0; x < 4096; x++)
            {
                float3 sum = float3(0, 0, 0);
                for (int d = -5; d < 5; d++)  // 400 m
                {
                    int X = __min(4095, __max(0, x + d));
                    sum += norm[y][X];
                }
                windTemp[y][x] = sum / 11.f;
            }
        }

        for (int x = 0; x < 4096; x++)
        {
            for (int y = 0; y < 4096; y++)
            {
                float3 sum = float3(0, 0, 0);
                for (int d = -5; d < 5; d++)  // 400 m
                {
                    int Y = __min(4095, __max(0, y + d));
                    sum += windTemp[Y][x];
                }
                norm[y][x] = sum / 11.f;
            }
        }

        setWind(_wind);


        solveWind();
        saveRaw();
    }
}


void _windTerrain::setWind(float3 _wind)
{
    for (int y = 0; y < 4096; y++)              // initialize wind, use 1 and scale later
    {
        for (int x = 0; x < 4096; x++)
        {
            wind[y][x] = _wind;
        }
    }

    normalizeWindLinear();
}

float3 _windTerrain::normProj(float3 w, float3 half)
{
    w -= 2 * glm::dot(w, half) * half;
    return w;
}

void _windTerrain::normalizeWind()
{
    for (int y = 1; y < 4095; y++)
    {
        for (int x = 1; x < 4095; x++)
        {
            float3 N = norm[y][x];
            //N.z *= -1;
            float W = glm::length(wind[y][x]);
            wind[y][x] -= N * glm::dot(wind[y][x], N);
            if (glm::length(wind[y][x]) > 0.01f)
            {
                wind[y][x] = W * glm::normalize(wind[y][x]);
            }
        }
    }
}

void _windTerrain::normalizeWindLinear()
{
    for (int y = 5; y < 4090; y++)
    {
        for (int x = 5; x < 4090; x++)
        {
            float3 W = wind[y][x];
            float2 dir = float2(W.x, W.z);
            dir = glm::normalize(dir);
            float hf = Hrel(float2(x, y) + dir * 2.f);
            float hb = Hrel(float2(x, y) - dir * 2.f);
            float slope = (hf - hb) / (4.f * 9.765625);

            W.y = 0;
            float Y = slope * glm::length(W);
            W.y = Y;
            wind[y][x] = W;
        }
    }
}

float3 _windTerrain::N(float2 _uv)
{
    int2 idx = glm::floor(_uv);
    float2 d1 = glm::fract(_uv);
    float2 d0 = 1.f - d1;
    return  (norm[idx.y][idx.x] * d0.x * d0.y) +
        (norm[idx.y][idx.x + 1] * d1.x * d0.y) +
        (norm[idx.y + 1][idx.x] * d0.x * d1.y) +
        (norm[idx.y + 1][idx.x + 1] * d1.x * d1.y);
}


//#pragma optimize("", off)
void _windTerrain::solveWind()
{

    float3 windTotal = float3(0, 0, 0);
    for (int y = 0; y < 4096; y++)              // initialize wind, use 1 and scale later
    {
        for (int x = 0; x < 4096; x++)
        {
            windTotal += wind[y][x];
        }
    }
    windTotal /= (4096 * 4096);
    float windAvs = glm::length(windTotal);
    bool cm = true;


    for (int i = 0; i < 20; i++)
    {
        OutputDebugString(L".");
        memset(windTemp, 0, sizeof(float3) * 4096 * 4096);

        for (int y = 0; y < 4096; y++)              // initialize wind, use 1 and scale later
        {
            for (int x = 0; x < 4096; x++)
            {
                float2 newXY = float2(x, y) + glm::normalize(float2(wind[y][x].x, wind[y][x].z)) * 3.f;    // half pixel base for now
                float2 d = glm::fract(newXY);
                float2 d0 = 1.f - d;
                int2 R = glm::floor(newXY);
                if (R.x >= 0 && R.x < 4095 && R.y >= 0 && R.y < 4095)
                {
                    float3 newNorm = N(newXY);
                    float3 half = glm::normalize(norm[y][x] + newNorm);
                    if (glm::dot(norm[y][x], N(newXY)) < 0.9f)
                    {
                        bool cm = true;
                    }
                    float3 windShift = normProj(wind[y][x], half);

                    if (glm::length(wind[y][x]) / glm::length(windShift) > 1.01f)
                    {
                        //image[y][x] = 255;
                        bool cm = true;
                    }
                    windTemp[R.y][R.x] += d0.x * d0.y * windShift;
                    windTemp[R.y][R.x + 1] += d.x * d0.y * windShift;
                    windTemp[R.y + 1][R.x] += d0.x * d.y * windShift;
                    windTemp[R.y + 1][R.x + 1] += d.x * d.y * windShift;
                    if (isnan(windTemp[R.y][R.x].x))
                    {
                        bool cm = true;
                    }
                }
            }
        }


        //memcpy(wind, windTemp, sizeof(float3) * 4096 * 4096);

        // diffuse
        for (int y = 2; y < 4094; y++)              // initialize wind, use 1 and scale later
        {
            for (int x = 2; x < 4094; x++)
            {
                float3 sum = float3(0, 0, 0);
                for (int i = -4; i < 5; i++)              // initialize wind, use 1 and scale later
                {
                    for (int j = -4; j < 5; j++)
                    {
                        sum += windTemp[y + i][x + j];
                    }
                }
                //sum += windTemp[y][x] * 6.f;
                wind[y][x] = sum / 81.f;
            }
        }

        static int CNT = 0;
        CNT++;
        CNT = CNT % 3;
        //if (CNT == 0)
        normalizeWind();
    }

    normalizeWindLinear();

    // high wind
    for (int y = 0; y < 4096; y++)
    {
        for (int x = 0; x < 4096; x++)
        {
            float3 sum = float3(0, 0, 0);
            for (int d = -30; d < 30; d++)  // 400 m
            {
                int X = __min(4095, __max(0, x + d));
                sum += wind[y][X];
            }
            windTemp[y][x] = sum / 61.f;
        }
    }

    for (int x = 0; x < 4096; x++)
    {
        for (int y = 0; y < 4096; y++)
        {
            float3 sum = float3(0, 0, 0);
            for (int d = -30; d < 30; d++)  // 400 m
            {
                int Y = __min(4095, __max(0, y + d));
                sum += windTemp[Y][x];
            }
            windTop[y][x] = sum / 61.f;
        }
    }

}




void _windTerrain::saveRaw()
{
    memset(image, 0, sizeof(unsigned char) * 4096 * 4096);

    for (int y = 0; y < 4096; y += 1)
    {
        for (int x = 0; x < 4096; x += 1)
        {
            //int up = 155;
            //if (wind[y][x].y > 0.05f) up = 100 + wind[y][x].y
            //image[y][x] = (int)((wind[y][x].y + 1.f) * 40.f) % 256;
            //if (fabs(wind[y][x].y) < 0.05f) image[y][x] = 255;

            Nc[y][x][0] = 0;// (int)(-wind[y][x].x * 50.f) % 256;
            Nc[y][x][1] = 0;// (int)(wind[y][x].z * 50.f) % 256;
            Nc[y][x][2] = 0;// (int)((wind[y][x].y + 1.f) * 50.f) % 256;
        }
    }

    for (int y = 0; y < 4096; y += 25)
    {
        for (int x = 0; x < 4096; x += 25)
        {
            //image[y][x] = (int)(wind[y][x].y * 50.f) % 256;

            //image[y][x] = (int)(glm::length(wind[y][x]) * 50.f) % 256;

            float2 XY = float2(x, y);
            float2 w = float2(wind[y][x].x, wind[y][x].z) * 0.6f;
            for (int k = 0; k < 200; k++)    // so wind of 1 is 16 pixels
            {
                if (XY.x >= 0 && XY.x < 4095 && XY.y >= 0 && XY.y < 4095)
                {
                    if (wind[(int)XY.y][(int)XY.x].y > 0)
                    {
                        image[(int)XY.y][(int)XY.x] = 50 + k;
                    }
                    else
                    {
                        image[(int)XY.y][(int)XY.x] = 50 + 50 * k % 2;
                    }
                    //Nc[(int)XY.y][(int)XY.x][0] = 50 + k;
                    float3 W = wind[(int)XY.y][(int)XY.x];
                    Nc[(int)XY.y][(int)XY.x][0] = (int)(glm::length(W) * 150.f) % 256;
                    Nc[(int)XY.y][(int)XY.x][1] = 0;// (int)(wind[y][x].z * 50.f) % 256;
                    Nc[(int)XY.y][(int)XY.x][2] = (int)((W.y) * 150.f) % 256;


                    XY += w;
                    w = float2(W.x, W.z) * 0.6f;
                }
            }

        }
    }


    std::ofstream ofs;

    ofs.open("E:/norm.raw", std::ios::binary);
    if (ofs)
    {
        ofs.write((char*)Nc, 4096 * 4096 * 3);
        ofs.close();
    }


    ofs.open("E:/wind.raw", std::ios::binary);
    if (ofs)
    {
        ofs.write((char*)image, 4096 * 4096);
        ofs.close();
    }

    ofs.open("E:/wind.bin", std::ios::binary);
    if (ofs)
    {
        ofs.write((char*)wind, 4096 * 4096 * sizeof(float3));
        ofs.write((char*)windTop, 4096 * 4096 * sizeof(float3));
        ofs.close();
    }

    ofs.open("E:/height.bin", std::ios::binary);
    if (ofs)
    {
        ofs.write((char*)height, 4096 * 4096 * sizeof(float));
        ofs.close();
    }
}



void _windTerrain::loadRaw()
{
    std::ifstream ifs;

    ifs.open("E:/wind.bin", std::ios::binary);
    if (ifs)
    {
        ifs.read((char*)wind, 4096 * 4096 * sizeof(float3));
        ifs.read((char*)windTop, 4096 * 4096 * sizeof(float3));
        ifs.close();
    }

    ifs.open("E:/height.bin", std::ios::binary);
    if (ifs)
    {
        ifs.read((char*)height, 4096 * 4096 * sizeof(float));
        ifs.close();
    }
}

float _windTerrain::H(float3 _pos)
{
    float scale = 4096.f / 40000.f; // for half pixel
    float2 uv = float2(_pos.x + 20000, _pos.z + 20000) * scale - 0.5f;
    int2 idx = uv;
    float2 d1 = glm::fract(uv);
    float2 d0 = 1.f - d1;

    return  (height[idx.y][idx.x] * d0.x * d0.y) +
        (height[idx.y][idx.x + 1] * d1.x * d0.y) +
        (height[idx.y + 1][idx.x] * d0.x * d1.y) +
        (height[idx.y + 1][idx.x + 1] * d1.x * d1.y);
}

float _windTerrain::Hrel(float2 _pos)
{
    float2 uv = _pos;
    int2 idx = uv;
    float2 d1 = glm::fract(uv);
    float2 d0 = 1.f - d1;

    return  (height[idx.y][idx.x] * d0.x * d0.y) +
        (height[idx.y][idx.x + 1] * d1.x * d0.y) +
        (height[idx.y + 1][idx.x] * d0.x * d1.y) +
        (height[idx.y + 1][idx.x + 1] * d1.x * d1.y);
}

float3 _windTerrain::W(float3 _pos)
{
    float scale = 4096.f / 40000.f; // for half pixel
    float2 uv = float2(_pos.x + 20000, _pos.z + 20000) * scale - 0.5f;
    int2 idx = uv;
    float2 d1 = glm::fract(uv);
    float2 d0 = 1.f - d1;

    float3 Wbottom = (wind[idx.y][idx.x] * d0.x * d0.y) +
        (wind[idx.y][idx.x + 1] * d1.x * d0.y) +
        (wind[idx.y + 1][idx.x] * d0.x * d1.y) +
        (wind[idx.y + 1][idx.x + 1] * d1.x * d1.y);

    float3 Wtop = (windTop[idx.y][idx.x] * d0.x * d0.y) +
        (windTop[idx.y][idx.x + 1] * d1.x * d0.y) +
        (windTop[idx.y + 1][idx.x] * d0.x * d1.y) +
        (windTop[idx.y + 1][idx.x + 1] * d1.x * d1.y);

    // do levels and wind return
    float h = __max(0.f, _pos.y - H(_pos));
    float L = __min(1.f, h / 100.f);
    float3 W = glm::lerp(Wbottom, Wtop * 2.f, L);

    float L0 = pow(__min(1.f, h / 5.f), 2.f);
    W = glm::lerp(float3(0, 0, 0), W, L0) * 2.7f;
    return W;
}
