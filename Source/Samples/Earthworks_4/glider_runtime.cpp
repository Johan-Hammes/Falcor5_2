#include "glider.h"
#include "imgui.h"
#include <imgui_internal.h>

//#pragma optimize("", off)

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

    _aoa = glm::clamp((_aoa * 57.f) + 6, 0.f, 34.9f);
    int aoa_idx = (int)_aoa;
    float a = _aoa - aoa_idx;

    float cpA = glm::lerp(cp[b_idx][aoa_idx][_chord], cp[b_idx][aoa_idx + 1][_chord], a);
    float cpB = glm::lerp(cp[b_idx+1][aoa_idx][_chord], cp[b_idx+1][aoa_idx + 1][_chord], a);
    float cp = glm::lerp(cpA, cpB, b);

    return cp;
}





void _gliderRuntime::renderGui(Gui* mpGui)
{
    ImGui::Columns(2);

    uint triangle = chordSize * spanSize;

    if (ImGui::SliderFloat("weight", &weightShift, 0, 1))
    {
        constraints[1].l = weightShift * weightLength;
        constraints[2].l = (1.0f - weightShift) * weightLength;
    }
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

    ImGui::Checkbox("step", &singlestep);
    ImGui::SameLine(0, 50);
    ImGui::Text("%d", frameCnt);

    ImGui::NewLine();

    ImGui::Text("pilot / lines/ wing %2.1fkg, %2.1f g, %2.1fkg", weightPilot, weightLines * 1000.f, weightWing);
    ImGui::Text("altitude %2.1fm", x[triangle].y);

    float3 velHor = v[triangle];
    velHor.y = 0;
    float Vh = glm::length(velHor) * 3.6f;
    ImGui::Text("speed %2.1f km/h", Vh);
    ImGui::Text("vertical %2.1f m/s", v[triangle].y);

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
        ImGui::Text("%2d : %4.1f, %4.1f, %4d%%, %4.1f, %2.1f %d ", (int)(cells[i].AoA * 57), cells[i].cellVelocity, cells[i].brakeSetting, (int)(cells[i].pressure * 99.f), cells[i].ragdoll, cells[i].AoA * 57, (int)cells[i].reynolds);
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

    cross = _cross;

    v.resize(x.size());
    memset(&v[0], 0, sizeof(float3) * v.size());
    n.resize(_span * _chord);
    t.resize(_span * _chord);

    x_old.resize(x.size());


    constraints = _constraints;

    spanSize = _span;
    chordSize = _chord;

    cells.resize(_span);



    //ROOT = float3(5500, 1900, 11500);   // walensee
    ROOT = float3(-7500, 1200, -10400);// hulftegg

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

    pBrakeLeft = linesLeft.getLine("FF1");
    pBrakeRight = linesRight.getLine("FF1");
    pEarsLeft = linesLeft.getLine("ears");
    pEarsRight = linesRight.getLine("ears");
    pSLeft = linesLeft.getLine("S1");
    pSRight = linesRight.getLine("S1");

    static bool first = true;
    if (first && pBrakeLeft)
    {
        first = false;
        if (pBrakeLeft)      maxBrake = pBrakeLeft->length;
        if (pEarsLeft)  maxEars = pEarsLeft->length;
        if (pSLeft)  maxS = pSLeft->length;
    }

    std::string folder = xfoilDir + "/" + wingName + "/cp.bin";
    Pressure.load(folder);

}

//#pragma optimize("", on)

void _gliderRuntime::solve(float _dT)
{
    if (runcount == 0)
    {
        return; // convenient breakpoint
    }
    runcount--;
    frameCnt++;

    _dT = 0.002f;

    // fixme later per cell
    float3 wind = float3(0, 0.f, 0.01f);

    sumFwing = float3(0, 0, 0);
    sumDragWing = float3(0, 0, 0);
    AllSummedForce = float3(0, 0, 0);
    AllSummedLinedrag = float3(0, 0, 0);

    float airDensity = 1.11225f;

    {
        FALCOR_PROFILE("aero");


        for (int i = 0; i < spanSize; i++)                  // cell pressure
        {
            cells[i].old_pressure = cells[i].pressure;      // move to pressure calc after
        }


        for (int i = 0; i < spanSize; i++)                  // velocity and dynamic pressure
        {
            cells[i].v = float3(0, 0, 0);
            for (int j = 0; j < chordSize; j++)
            {
                cells[i].v += v[index(i, j)];
            }
            cells[i].v /= chordSize;

            cells[i].rel_wind = wind - cells[i].v;
            cells[i].cellVelocity = glm::length(cells[i].rel_wind);
            cells[i].rho = 0.5f * airDensity * pow(cells[i].cellVelocity, 2);       // dynamic pressure
            cells[i].rho = __min(cells[i].rho, 1000.f);//clamp

            cells[i].reynolds = (1.125f * cells[i].cellVelocity * cells[i].chordLength) / (0.0000186);
            cells[i].cD = 0.027f / pow(cells[i].reynolds, 1.f / 7.f);
        }


        for (int i = 0; i < spanSize; i++)                  // aoa and F
        {
            int a = __max(0, i - 1);
            int b = __min(spanSize-1, i + 1);
            cells[i].front = glm::normalize(x[index(i, 12)] - x[index(i, 3)]);          // From C forward
            cells[i].right = glm::normalize(x[index(b, 9)] - x[index(a, 9)]);           // at A line
            cells[i].up = glm::normalize(glm::cross(cells[i].front, cells[i].right));

            float3 frontBrake = glm::normalize(x[index(i, 3)] - x[index(i, 0)]);
            float3 upBrake = glm::normalize(glm::cross(frontBrake, cells[i].right));

            float sintheta = -glm::dot(upBrake, cells[i].front);
            cells[i].AoBrake =  glm::lerp(cells[i].AoBrake, sintheta * 1.5f, 0.03f);    // quite smoothed
            cells[i].AoBrake = glm::clamp(cells[i].AoBrake, 0.f, 1.f);

            float3 windDir = glm::normalize(cells[i].rel_wind);
            float aoa = asin(glm::dot(cells[i].up, windDir));
            if (glm::dot(windDir, cells[i].front) > 0) {
                aoa = 3.14159f - aoa;
            }
            cells[i].AoA = glm::lerp(cells[i].AoA, aoa, 0.5f);  // very slightly smoothed
        }



        for (int i = 0; i < spanSize; i++)
        {

            // cell pressure
            float AoAlookup = ((cells[i].AoA * 57.f) + 6.f) / 3.f;
            int idx = __max(0, __min(9, (int)floor(AoAlookup)));
            float dL = __min(1, AoAlookup - idx);
            if (idx == 0) dL = __max(0, dL);
            float pressure10 = __max(0, glm::lerp(CPbrakes[0][idx][10], CPbrakes[0][idx + 1][10], dL));
            float pressure11 = __max(0, glm::lerp(CPbrakes[0][idx][11], CPbrakes[0][idx + 1][11], dL));
            float P =  __max(pressure10, pressure11);
            float windPscale = __min(1.f, cells[i].cellVelocity / 4.f);
            P *= pow(windPscale, 4);

            cells[i].old_pressure = glm::lerp(cells[i].old_pressure, P, 0.35f);
        }


        // the pressure distribution
        for (int i = 0; i < spanSize; i++)
        {
            cells[i].pressure = cells[i].old_pressure;

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
                float MAX = __max(__max(cells[i].old_pressure, cells[i - 1].old_pressure * 0.70f), cells[i + 1].old_pressure * 0.70f);
                cells[i].pressure = lerp(cells[i].old_pressure, MAX, 1.0f);
            }
        }

        // face normals
        for (int i = 0; i < spanSize * chordSize; i++)
        {
            int chord = i % chordSize;
            int span = i / chordSize;

            n[i] = 0.125f * glm::cross((x[cross[i].z] - x[cross[i].w]), (x[cross[i].x] - x[cross[i].y]));
            float3 r = glm::normalize(glm::cross(n[i], -v[i]));
            t[i] = glm::cross(r, n[i]);


            float cp = Pressure.get(cells[span].AoBrake, cells[span].AoA, chord);

            // rag doll pressures
            cells[span].ragdoll = glm::clamp(cells[span].pressure * 3.f, 0.f, 1.f); // bottom 33% of pressure
            float cpRagdoll = -glm::dot(glm::normalize(n[i]), glm::normalize(cells[span].rel_wind));
            cpRagdoll = glm::clamp(cpRagdoll, 0.f, 1.f);
            cp = glm::lerp(cpRagdoll, cp, cells[span].ragdoll);


            n[i] = n[i] * (cells[span].pressure - cp) * cells[span].rho;
            sumFwing += n[i];

            n[i] = 0.125f * glm::cross((x[cross[i].z] - x[cross[i].w]), (x[cross[i].x] - x[cross[i].y]));
            n[i] = n[i] * (cells[span].pressure - cp) * cells[span].rho;

            float dragIncrease = pow(2.f - cells[span].pressure, 5.f);
            float drag = 0.0039 * cells[span].rho * 1.5f * dragIncrease;    // double it for rib turbulece
            t[i] *= drag;
            sumDragWing += t[i];
        }
        bool bCm = true;
    }


    uint start = chordSize * spanSize;

    weightLines = 0;
    weightPilot = 0;
    weightWing = 0;
    pilotDrag = float3(0, 0, 0);
    lineDrag = float3(0, 0, 0);

    {
        FALCOR_PROFILE("pre");
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

            x[i] += (_dT * v[i]) + (a * _dT * _dT);
        }
    }

    linesLeft.windAndTension(x, w, n, v, wind, _dT * _dT);
    linesRight.windAndTension(x, w, n, v, wind, _dT * _dT);
    tensionCarabinerLeft = linesLeft.tencileVector * 0.94f;
    tensionCarabinerRight = linesRight.tencileVector * 0.94f;

    // Solve the body and carabiners
    x[start + 1] += w[start + 1] * tensionCarabinerLeft * _dT * _dT;
    x[start + 2] += w[start + 2] * tensionCarabinerRight * _dT * _dT;
    // drag on the body
    vBody = glm::length(v[start] - wind);
    float rho = 0.5 * airDensity * vBody * vBody;
    if (vBody > 1) {
        pilotDrag = -glm::normalize(v[start] - wind) * rho * 0.7f * 1.3f;
        x[start] += _dT * w[start] * pilotDrag * _dT;
    }


    // Now solve the first 3 constraints multiple times
    for (int k = 0; k < 10; k++)
    {
        for (int i = 0; i < 3; i++)
        {
            uint& idx0 = constraints[i].idx_0;
            uint& idx1 = constraints[i].idx_1;
            float Wsum = w[idx0] + w[idx1];

            if (Wsum > 0)
            {
                float3 S = x[idx1] - x[idx0];
                float L = glm::length(S);
                float3 Snorm = glm::normalize(S);
                float C = L - (constraints[i].l);

                float s = C / Wsum;
                x[idx0] += Snorm * s * w[idx0];
                x[idx1] -= Snorm * s * w[idx1];
            }
        }
    }


    {
        FALCOR_PROFILE("wing constraints");
        // solve
        float dS = 0.8f;
        for (int k = 0; k < 3; k++)
        {
            for (int l = 0; l < 6; l++)
            {
                bool last = false;
                if (l == 5) last = true;
                linesLeft.solveUp(x, last);
                linesRight.solveUp(x, last);
            }

            for (int i = 3; i < constraints.size(); i++)
            {
                solveConstraint(i, dS);
            }
        }
    }



    v_oldRoot = v[spanSize * chordSize];
    {
        FALCOR_PROFILE("post");
        for (int i = 0; i < x.size(); i++)
        {
            v[i] = (x[i] - x_old[i]) / _dT;
            if (std::isnan(x[i].x))
            {
                bool bcm = true;
            }
            if (glm::length(v[i]) > 100)
            {
                bool bcm = true;
            }

            if ((x[i].y + ROOT.y) < 0)
            {
                x[i].y = -ROOT.y;
                v[i] *= 0.3f;
            }
        }

        a_root = (v[spanSize * chordSize] - v_oldRoot) / _dT;

        // NOW reset all x to keep human at 0
        //float3 offset = (x[spanSize / 4 * chordSize] + x[spanSize * chordSize]) * 0.5f;
        //float3 offset = (x[spanSize / 2 * chordSize] );
        float3 offset = (x[spanSize * chordSize]) + float3(0, 0.5, 0);
        //int3 offsetI = offset;
        //offset = offsetI;
        ROOT += offset;
        for (int i = 0; i < x.size(); i++)
        {
            x[i] -= offset;
        }

        if (glm::length(pilotDrag) > 1.f) pilotYaw = atan2(pilotDrag.x, pilotDrag.z);
    }

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

    x[idx0] += S * C / Wsum * w[idx0];
    x[idx1] -= S * C / Wsum * w[idx1];
}


void _gliderRuntime::pack_canopy()
{
    ribbonCount = 0;

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

    // triangle
    uint triangle = spanSize * chordSize;
    setupVert(&ribbon[ribbonCount], 0, x[triangle], 0.01f, 7);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 1], 0.01f, 7);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle], 0.01f, 7);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 2], 0.01f, 7);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle + 1], 0.01f, 7);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 2], 0.01f, 7);     ribbonCount++;

    // constraints
    for (int i = 0; i < constraints.size(); i++)
    {
        //setupVert(&ribbon[ribbonCount], 0, x[constraints[i].idx_0], 0.004f, 7);     ribbonCount++;
        //setupVert(&ribbon[ribbonCount], 1, x[constraints[i].idx_1], 0.004f, 7);     ribbonCount++;
    }
}


void _gliderRuntime::pack_lines()
{
    linesLeft.addRibbon(ribbon, x, ribbonCount);
    linesRight.addRibbon(ribbon, x, ribbonCount);
}

void _gliderRuntime::pack_feedback()
{
    uint triangle = spanSize * chordSize;
    setupVert(&ribbon[ribbonCount], 0, x[triangle], 0.001f, 1);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle] + pilotDrag * 0.05f, 0.1f, 1);     ribbonCount++;

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

                //setupVert(&ribbon[count], 0, x[index(s, c)], 0.001f, color);     count++;
                //setupVert(&ribbon[count], 1, x[index(s, c)] + n[index(s, c)] * 0.3f, 0.004f, color);     count++;
                //setupVert(&ribbon[count], 1, x[index(s, c)] + t[index(s, c)] * 20.f, 0.01f, 4);     count++;
            }
        }
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


