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
#include "terrain.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <random>
#include "Utils/UI/TextRenderer.h"
#include "assimp/Exporter.hpp"
using namespace Assimp;

#pragma optimize("", off)

#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}



_lastFile terrainManager::lastfile;
//materialCache_plants    terrainManager::vegetationMaterialCache;
materialCache_plants _plantMaterial::static_materials_veg;


void setupVert(rvB* r, int start, float3 pos, float radius, int _mat = 0)
{
    r->type = false;
    r->startBit = start;
    r->position = pos;
    r->radius = radius;
    r->bitangent = float3(0, 1, 0);
    r->tangent = float3(0, 0, 1);
    r->material = _mat;
    r->albedoScale = 127;
    r->translucencyScale = 127;
    r->uv = float2(1, 4);

    if (start == 1)
    {
        rvB* rprev = r--;
        float3 tangent = glm::normalize(r->position - rprev->position);
        r->bitangent = tangent;
        rprev->bitangent = tangent;
    }
}

// AIR
// ###############################################################################################################################################

void _airSim::setup()
{
    float R = 100.f;    // 100m sphere arounf (0, 0, 0);

    std::mt19937 generator(101);
    std::uniform_real_distribution<> distribution(-R, R);

    float V = 4.18879020479f * R * R * R;
    int numP = 1024;
    float v = V / numP;

    particles.clear();
    particles.emplace_back();
    particles.back().volume = 0.f;
    particles.back().mass = 0.f;
    particles.back().r = 0.f;       // make 0 the null particle

    int cnt = numP;
    while (cnt > 0)
    {
        float3 p = float3(distribution(generator), distribution(generator), distribution(generator));
        float l = glm::length(p);
        if (l < R)
        {
            particles.emplace_back();
            particles.back().pos = p;
            particles.back().volume = v;
            particles.back().r = std::cbrt(v / 4.18879020479f);
            for (int i = 0; i < 16; i++) {
                particles.back().space[i] = 0;  // set to null particle
            }
            cnt--;
        }
    }

    int linkCount = 0;
    for (int j = 1; j <= numP; j++)
    {
        for (int k = 1; k <= numP; k++)
        {
            if (j != k)
            {
                float l = glm::length(particles[j].pos - particles[k].pos);
                if (l < 3 * particles[j].r)
                {
                    for (int i = 0; i < 16; i++) {
                        if (particles[j].space[i] == 0)
                        {
                            particles[j].space[i] = k;
                            linkCount++;
                            break;
                        }
                    }
                }
            }
        }
    }

    bool bCM = true;
}

void _airSim::update(float _dt)
{
    uint hash[100];
    float3 norm[16];
    float dst[16];
    float W[16];
    float wSum;
    float d[16];
    int numP = particles.size() - 1;
    float3 A = float3(0, 0, 0);

    // calculate volumes
    {
        FALCOR_PROFILE("weights");
        for (int i = 1; i <= numP; i++)
        {
            wSum = 0;
            particles[i].firstEmpty = -1;
            A = float3(0, 0, 0);
            float Wmin = 100000;
            float RRi = particles[i].r * particles[i].r;
            particles[i].dVel = float3(0, 0, 0);
            for (int j = 0; j < 16; j++)
            {
                W[j] = 0;
                uint idx = particles[i].space[j];
                float RRj = particles[idx].r * particles[idx].r;
                if (idx > 0)
                {
                    norm[j] = particles[idx].pos - particles[i].pos;
                    dst[j] = glm::dot(norm[j], norm[j]);// glm::length(norm[j]);      // THSI SI TEH BOTTLENEC but fast on GPU
                    W[j] = RRi / dst[j];
                    W[j] *= W[j];
                    wSum += W[j];

                    // Find smallets W
                    if (W[j] < Wmin)
                    {
                        Wmin = W[j];
                        particles[i].firstEmpty = j;
                    }

                    particles[i].dVel += particles[idx].vel * W[j];
                }
            }

            // add in a wall at spehere edge
            //float toEdge = 100.f - glm::length(particles[i].pos);
            //float Wedge = (particles[i].r * particles[i].r * 0.25f) * (toEdge * 0.5f);
            //wSum += Wedge;
            particles[i].dVel *= 1.0f / wSum;
            particles[i].volNew = 1.3333f / wSum * (1.3333f * 3.14f * particles[i].r * particles[i].r * particles[i].r);



            A = float3(0, 0, 0);
            for (int j = 0; j < 16; j++)
            {
                uint idx = particles[i].space[j];
                if (idx > 0)
                {
                    A -= norm[j] * (W[j]) / wSum;
                }
            }
            //A += glm::normalize(particles[i].pos) * (Wedge / wSum);
            particles[i].acell = A * wSum * 50.f;

        }
    }


    {
        FALCOR_PROFILE("advect");
        for (int i = 1; i <= numP; i++)
        {
            particles[i].volRel = particles[i].volNew;
            particles[i].vel *= 0.9f;
            particles[i].vel += particles[i].dVel * 0.05f;
            particles[i].vel += particles[i].acell * _dt;
            particles[i].pos += particles[i].vel * _dt;


            if (glm::length(particles[i].pos) > 100.f)
            {
                particles[i].pos = 100.f * glm::normalize(particles[i].pos);
                particles[i].vel *= 0.9f;
            }
        }
    }



    {
        FALCOR_PROFILE("neighbours");

        // get list of indicis
        static uint hashadd = 0;
        for (int i = 1 + hashadd; i <= numP; i += 100)
        {
            float dR = particles[i].r * particles[i].r * 9.f;
            float minR = 1000000;
            if (particles[i].firstEmpty >= 0)
            {
                memset(hash, 0, sizeof(uint) * 100);
                hash[(i + hashadd) % 100] = 1;
                for (int j = 0; j < 16; j++)
                {
                    int ij = particles[i].space[j];
                    if (ij != 0)
                    {
                        hash[(ij + hashadd) % 100] = 1;
                    }
                }
                // now the hash is complete

                for (int j = 0; j < 16; j++)
                {
                    int ij = particles[i].space[j];
                    if (ij != 0)
                    {
                        for (int k = 0; k < 16; k++)
                        {
                            int ik = particles[ij].space[k];
                            if (ik > 0 && hash[(ik + hashadd) % 100] == 0)
                            {
                                float3 D = particles[i].pos - particles[ik].pos;
                                float f = glm::dot(D, D);
                                if (f < dR && f < minR)
                                {
                                    particles[i].space[particles[i].firstEmpty] = ik;
                                    minR = f;
                                }
                                /*
                                float dst = glm::length(particles[i].pos - particles[ik].pos);
                                if (dst < particles[i].r * 3)
                                {
                                    particles[i].space[particles[i].firstEmpty] = ik;
                                    break;
                                }
                                */
                            }
                        }
                    }
                }
            }
        }
        hashadd++;
        hashadd %= 100;
    }

}

void _airSim::pack()
{
    ribbonCount = 0;

    for (auto& P : particles)
    {
        setupVert(&ribbon[ribbonCount], 0, P.pos + float3(0, -0.5f, 0), 0.5f);     ribbonCount++;
        setupVert(&ribbon[ribbonCount], 1, P.pos + float3(0, 0.5f, 0), 0.5f);     ribbonCount++;
        for (auto& idx : P.space)
        {
            if (idx > 0)
            {
                setupVert(&ribbon[ribbonCount], 0, P.pos, 0.1f);     ribbonCount++;
                setupVert(&ribbon[ribbonCount], 1, particles[idx].pos, 0.1f);     ribbonCount++;
            }
        }
    }


    for (int i = 0; i < ribbonCount; i++)
    {
        packedRibbons[i] = ribbon[i].pack();
    }
    changed = true;
}













// PARAGLIDERS
// ###############################################################################################################################################
extern glm::vec3 cubic_Casteljau(float t, glm::vec3 P0, glm::vec3 P1, glm::vec3 P2, glm::vec3 P3);

void _bezierGlider::solveArcLenght()
{
    arcLength = 0;
    float3 p0 = cubic_Casteljau(0, P0, P1, P2, P3);
    float3 p1;
    for (float t = 0; t < 1; t += 0.001f)
    {
        p1 = cubic_Casteljau(t, P0, P1, P2, P3);
        arcLength += glm::length(p1 - p0);
        p0 = p1;
    }
}


float3 _bezierGlider::pos(float _s)
{
    float Length = 0;
    float3 p0 = cubic_Casteljau(0, P0, P1, P2, P3);
    float3 p1;
    for (float t = 0; t < 1; t += 0.001f)
    {
        p1 = cubic_Casteljau(t, P0, P1, P2, P3);
        Length += glm::length(p1 - p0);
        p0 = p1;
        if ((Length / arcLength) >= _s) return p0;
    }
    return float3(0, 0, 0);
}

void _bezierGlider::renderGui(Gui* mpGui)
{
    ImGui::DragFloat3("p0", &P0.x, 0.1f, -20, 20);
    ImGui::DragFloat3("p1", &P1.x, 0.1f, -20, 20);
    ImGui::DragFloat3("p2", &P2.x, 0.1f, -20, 20);
    ImGui::DragFloat3("p3", &P3.x, 0.1f, -20, 20);
}


void cubic_Casteljau_Para(float t, glm::vec3 P0, glm::vec3 P1, glm::vec3 P2, glm::vec3 P3, glm::vec3& pos, glm::vec3& tangent, glm::vec3& bitangent)
{
    glm::vec3 pA = lerp(P0, P1, t);
    glm::vec3 pB = lerp(P1, P2, t);
    glm::vec3 pC = lerp(P2, P3, t);

    glm::vec3 pD = lerp(pA, pB, t);
    glm::vec3 pE = lerp(pB, pC, t);

    pos = lerp(pD, pE, t);
    tangent = glm::normalize(pE - pD);
    bitangent = glm::normalize(glm::cross(tangent, glm::vec3(0, 0, -1)));
}













uint _lineBuilder::calcVerts(float _spacing)
{
    uint verts = (uint)ceil(length / _spacing);
    for (auto& C : children) {
        verts += C.calcVerts(_spacing);
    }
    return verts;
}


void _lineBuilder::addVerts(std::vector<float3>& _x, std::vector<float>& _w, std::vector<float>& _cd, uint& _idx, uint _parentidx, float _spacing)
{
    isLineEnd = children.size() == 0;
    isCarabiner = name.find("Carabiner") != std::string::npos;

    numSegments = 1;// (uint)ceil((float)length / _spacing);
    parent_Idx = _parentidx;
    start_Idx = _idx;
    end_Idx = start_Idx + numSegments - 1;

    for (int i = 0; i < numSegments; i++)
    {
        float t = (float)(i + 1) / (float)numSegments;
        _x[start_Idx + i] = glm::lerp(_x[parent_Idx], endPos, t);
        _w[start_Idx + i] = 1.f / (_spacing * 1000.f * diameter * diameter);            // EDELRID SPEC
        _cd[start_Idx + i] = 1.f;
    }

    _idx += numSegments;
    for (auto& Ch : children) {
        Ch.addVerts(_x, _w, _cd, _idx, end_Idx, _spacing);
    }
}


void _lineBuilder::set(std::string _n, float _l, float _d, float3 _color, int2 _attach, float3 _p)
{
    name = _n;
    length = _l;
    diameter = _d;
    color = _color;
    attachment = _attach;
    endPos = _p;
}


_lineBuilder& _lineBuilder::pushLine(std::string _n, float _l, float _d, float3 _color, int2 _attach, float3 _dir, int _segments)
{
    // need to fix _P
    if (_segments > 1)
    {
        children.emplace_back();
        children.back().set(_n, _l, _d, _color, uint2(0, 0), float3(0, 0, 0));
        return children.back().pushLine(_n, _l, _d, _color, _attach, _dir, _segments - 1);
    }
    else
    {
        children.emplace_back();
        children.back().set(_n, _l, _d, _color, _attach, _dir);
        return children.back();
    }
}


void _lineBuilder::mirror(int _span)
{
    if (children.size() == 0) {
        attachment.x = (_span - 1) - attachment.x;
        endPos.x *= -1;
    }
    for (auto& C : children) {
        C.mirror(_span);
    }
    name = "L_" + name;
}


float3 _lineBuilder::averageEndpoint(float3 _start)
{
    if (children.size() == 0) {
        return endPos - _start;
    }

    float3 p = float3(0, 0, 0);
    for (auto& C : children) {
        p += C.averageEndpoint(_start);
    }
    return p;
}


void _lineBuilder::solveLayout(float3 _start, uint _chord)
{
    rho_Line = (0.5 * 1.11225f) * 1.2f * (diameter * length);       // 1.2 seems betetr Cd for cylinder
    rho_End = 0.5f * rho_Line;

    m_Line = (length * 1000.f * diameter * diameter);
    m_End = m_Line * 0.5f;

    if (children.size() > 0) {
        float3 end = averageEndpoint(_start);
        float3 p = glm::normalize(end);
        endPos = _start + length * p;
    }
    else {
        wingIdx = attachment.x * _chord + attachment.y;
    }

    for (auto& C : children) {
        C.solveLayout(endPos, _chord);
        rho_End += 0.5f * C.rho_Line;
        m_End += 0.5f * C.m_Line;
    }
}
    


void _lineBuilder::renderGui(int tab)
{
    if ((     (name.find("F") == 0) || (name.find("Carabiner") != std::string::npos) || (name.find("strap") != std::string::npos)    ) && (name.find("L_") == std::string::npos))
    {
        ImGui::NewLine();
        ImGui::SameLine(0, (float)(tab * 10));
        ImGui::Text("%s", name.c_str());
        ImGui::SameLine(150, 0);
        ImGui::Text("%1.5f %2d%%, %2d%%     %3dN    %3dN ", stretchRatio, (int)(t_ratio * 99), (int)(t_combined * 99), (int)maxTencileForce, (int)tencileForce);
        //ImGui::Text("%dmm, %1.2fmm %3.1fg   %3.2fg   %3.2fmm", (int)(length * 1000.f), diameter * 1000.f, 1000.f * m_End, 1000.f * m_Line, f_Line);
        
    }

    for (auto& C : children) {
        C.renderGui(tab + 1);
    }
}


void _lineBuilder::addRibbon(rvB* _r, std::vector<float3>& _x, int& _c)
{
    float3 red = float3(0.8f, 0.01f, 0.01f);
    float3 yellow = float3(0.8f, 0.8f, 0.01f);
    float3 green = float3(0.01f, 0.8f, 0.01f);
    float3 blue = float3(0.01f, 0.01f, 0.7f);
    float3 turqoise = float3(0.01f, 0.7f, 0.7f);
    float3 orange = float3(0.7f, 0.3f, 0.01f);
    float3 black = float3(0.01f, 0.01f, 0.01f);

    uint cIndex = 0;
    if (color == red) cIndex = 1;
    if (color == yellow) cIndex = 2;
    if (color == green) cIndex = 3;
    if (color == blue) cIndex = 4;
    if (color == turqoise) cIndex = 5;
    if (color == orange) cIndex = 6;
    if (color == black) cIndex = 7;


    {
        setupVert(&_r[_c], 0, _x[parent_Idx], diameter + 0.005f, cIndex);     _c++;
        setupVert(&_r[_c], 1, _x[end_Idx], diameter + 0.005f, cIndex);     _c++;
    }

    /*
    //if ((name.find("A") == 0) && (name.find("L_") == std::string::npos))
    {
        setupVert(&_r[_c], 0, _x[parent_Idx], diameter + 0.02f, cIndex);     _c++;
        for (int i = start_Idx; i <= end_Idx; i++)
        {
            setupVert(&_r[_c], 1, _x[i], diameter + 0.02f, cIndex);     _c++;
        }
    }

    
    {
        float tF = sqrt(tencileForce);
        float3 tN = glm::normalize(tencileVector);
                setupVert(&_r[_c], 0, _x[end_Idx], 0.001f, cIndex);     _c++;
                setupVert(&_r[_c], 1, _x[end_Idx] + tF * tN * 0.2f, tF * 0.003f, cIndex);     _c++;
    }

    if (isLineEnd)
    {
        float tF = sqrt(glm::length(wingForce));
        float3 tN = glm::normalize(wingForce);
               setupVert(&_r[_c], 0, _x[end_Idx], 0.001f, 7);     _c++;
               setupVert(&_r[_c], 1, _x[end_Idx] + tF * tN * 0.2f, tF * 0.003f, 7);     _c++;
    }
    */


    for (auto& C : children) {
        C.addRibbon(_r, _x, _c);
    }
}

float3 AllSummedForce;
float3 AllSummedLinedrag;


void _lineBuilder::wingTencile(std::vector<float3>& _x, std::vector<float3>& _n)
{
    
        tencileVector = float3(0, 0, 0);

        for (int s = __max(0, attachment.x - 1); s <= __min(49, attachment.x + 1); s++)
        {
            switch (attachment.y)
            {
            case 9:
                for (int c = 8; c <= 16; c++) {
                    tencileVector += _n[s * 25 + c];
                }
                break;
            case 8: case 7: case 6: case 5: case 4: case 3: case 2: case 1:
                for (int c = attachment.y - 1; c <= attachment.y + 1; c++) {
                    tencileVector += _n[s * 25 + c];
                    tencileVector += _n[s * 25 + 24 - c];
                }
                break;
            case 0:
                for (int c = 0; c <= 1; c++) {
                    tencileVector += _n[s * 25 + c];
                    tencileVector += _n[s * 25 + 24 - c];
                }
                break;
            }
        }
        wingForce = tencileVector;
        AllSummedForce += tencileVector;


        float3 lineNormal = glm::normalize(_x[end_Idx] - _x[parent_Idx]);
        tencileRequest = __max(0.0001f, glm::dot(lineNormal, tencileVector));

        tencileForce = __min(tencileRequest, maxTencileForce);
        tencileVector = tencileForce * lineNormal;

        //wingForce = tencileRequest * lineNormal;
    
}

float _lineBuilder::maxT(float _r, float _dT)
{
    float K = __max(0, (_r - 0.98f) * 50.f);
    return K * 200.f;

    _dT = 1;
    float SLOPE = -100;
    float cutoffX = atan(sqrt(-1.f / SLOPE));
    float cutoffT = _dT / tan(cutoffX);
    float cutoffR = cos(0.57f * cutoffX);

    /*
    float x = acos(stretchRatio) * 1.75f;
    if (stretchRatio > 1.f)
    {
        x = -acos(2 - stretchRatio) * 1.75f;
    }
    */
    if (_r > cutoffR)
    {
        //float X2 = SLOPE * (x - cutoffX);
        float X2 = SLOPE * (_r - cutoffR);
        return cutoffT + 400 * (X2 * X2);
    }
    else
    {
        float x = acos(stretchRatio) * 1.75f;
        return _dT / tan(x);
    }
}

void _lineBuilder::windAndTension_2(std::vector<float3>& _x, std::vector<float>& _w, std::vector<float3>& _n, std::vector<float3>& _v, float3 _wind, float _dtSquare)
{
    float3 V = _v[end_Idx] - _wind;
    float v = glm::length(V);
    float rho = rho_End * v * v;
    float3 LINEn = _x[end_Idx] - _x[parent_Idx];
    stretchRatio = glm::length(LINEn) / length;
    LINEn = glm::normalize(LINEn);
    LINEn.y = 0;
    float gravityScale = 0;// glm::length(LINEn);
    maxTencileForce = maxT(stretchRatio, rho_Line * v * v * 4.f);
    float3 fWind = -glm::normalize(V) * rho;
    _x[end_Idx] += (fWind / m_End + float3(0, -9.8f * gravityScale, 0)) * _dtSquare;
    AllSummedLinedrag += fWind;

    if (std::isnan(_x[end_Idx].x))
    {
        bool bcm = true;
    }

    //f_Line = rho / m_End * _dtSquare * 1000.f;// rho_Line* v* v;
    f_Line = rho_Line * v * v;

    if (isLineEnd)
    {
        wingTencile(_x, _n);
        t_ratio = tencileForce / tencileRequest;

        if (isLineEnd)
        {
            //_x[end_Idx] += wingForce * 0.1f / m_End * _dtSquare;
        }

        
    }
    
    for (auto& C : children) {
        C.windAndTension_2(_x, _w, _n, _v, _wind, _dtSquare);
    }

    // now on the way down
    if (!isLineEnd)
    {
        tencileVector = float3(0, 0, 0);
        for (auto& C : children) {
            tencileVector += C.tencileVector;
        }

        
        if (!isCarabiner)
        {
            float SumRequest = glm::length(tencileVector);
            float3 lineNormal = glm::normalize(_x[end_Idx] - _x[parent_Idx]);
            tencileRequest = __max(0.0001f, glm::dot(tencileVector, lineNormal));
            tencileForce = __min(tencileRequest, maxTencileForce);
            tencileVector = tencileForce * lineNormal;
            t_ratio = tencileForce / tencileRequest;

            if (tencileRequest < SumRequest * 0.9f)
            {
                bool bCM = true;
            }
        }
        else
        {
            tencileForce = glm::length(tencileVector);
            t_ratio = 1;
        }
    }
}


void _lineBuilder::windAndTension(std::vector<float3>& _x, std::vector<float>& _w, std::vector<float3>& _v, float3 _wind, float _dtSquare)
{
    uint a = parent_Idx;
    uint b = start_Idx;
    stretchLength = 0;
    while (b <= end_Idx)
    {
        stretchLength += glm::length(_x[b] - _x[a]);
        a = b;
        b++;
    }
    stretchRatio = __min(1.f, __max(0.f, ((stretchLength / length) - 0.98f) * 50.f));     // 2 % of stretch
    //stretchRatio = pow(stretchRatio, 0.5f);     // or smoothstep, just not linear


    if (numSegments > 1) {
    }


    float straightL = glm::length(_x[parent_Idx] - _x[end_Idx]);
    straightRatio = straightL / length;

    t_ratio = __max(0.f, __min(1.f, (straightRatio - 0.98f) * 50.f));
    float rho0 = (0.5 * 1.11225f) * 0.6f * (diameter * 2 * length / numSegments);
    //float rho0 = (0.5 * 1.11225f) * 0.6f * (diameter * length);
    //float rho0 = (0.5f * 1.11225f) * 0.6f * (0.002f) * length * 4.f;

    
    

    // Move all verts using wind gavity and tensile drag
    float sumDrag = 0.f;
    float3 p0 = _x[parent_Idx];
    for (int j = start_Idx; j < end_Idx; j++)      // wind drag
    {
        float3 t1 = tencileForce * glm::normalize(p0 - _x[j]);
        float3 t2 = tencileForce * glm::normalize(_x[j+1] - _x[j]);
        float3 relWind =  _v[j] - _wind;
        float3 tangent = glm::normalize(_x[j + 1] - p0);
        float tangentWind = glm::dot(tangent, relWind);
        relWind -= tangentWind * tangent;
        float v = __min(20.f, glm::length(relWind));
        float rho = rho0 * v * v * 1.f;
        float3 f_total = - (glm::normalize(relWind) * rho);
        AllSummedLinedrag += f_total;
        //f_total += t1 + t2;
        _x[j] += (f_total * _w[j] + float3(0, -9.8f, 0)) * _dtSquare ;
        sumDrag += rho;

        p0 = _x[j];
    }

    // do the last one
    if (!isLineEnd)
    {
        float3 pEnd = _x[end_Idx];
        //float3 f_total = tencileForce * glm::normalize(_x[end_Idx - 1] - pEnd);
        for (auto& C : children) {
            //f_total += C.tencileForce * glm::normalize(_x[C.start_Idx] - pEnd);
        }

        float3 relWind =  _v[end_Idx] - _wind;
        float v = glm::length(relWind);
        float rho = rho0 * v * v * 1.f;
        float3 f_total = -glm::normalize(relWind) * rho;
        _x[end_Idx] += (f_total * _w[end_Idx] + float3(0, -9.8f, 0)) * _dtSquare;
    }

    {
        float S = __min(0.9999f, straightRatio);
        float l = sqrt(1 - S * S);
        maxTencileForce = sumDrag / l * stretchRatio;
    }

    for (auto& C : children) {
        C.windAndTension(_x, _w, _v, _wind, _dtSquare);
    }
}


float3 _lineBuilder::tensionDown(std::vector<float3>& _x, std::vector<float3>& _n)
{
    

    if (isLineEnd)
    {
        tencileVector = float3(0, 0, 0);

        for (int s = __max(0, attachment.x - 1); s <= __min(49, attachment.x + 1); s++)
        {
            switch (attachment.y)
            {
            case 9:
                for (int c = 8; c <= 16; c++) {
                    tencileVector += _n[s * 25 + c];
                }
                break;
            case 8: case 7: case 6: case 5: case 4: case 3: case 2: case 1:
                for (int c = attachment.y - 1; c <= attachment.y + 1; c++) {
                    tencileVector += _n[s * 25 + c];
                    tencileVector += _n[s * 25 + 24 - c];
                }
                break;
            case 0:
                for (int c = 0; c <= 1; c++) {
                    tencileVector += _n[s * 25 + c];
                    tencileVector += _n[s * 25 + 24 - c];
                }
                break;
            }
        }
        wingForce = tencileVector;
        AllSummedForce += tencileVector;

        
        float3 lineNormal = glm::normalize(_x[end_Idx] - _x[end_Idx-1]);
        float Fn = __max(0.0001f, glm::dot(lineNormal, tencileVector));

        //if (Fn < maxTencileForce) t_ratio = 1;

        //tencileForce = Fn * t_ratio * lineNormal;
        tencileForce = __min(Fn, maxTencileForce);
        tencileVector = tencileForce * lineNormal;
    }
    else
    {
        tencileVector = float3(0, 0, 0);
        for (auto& C : children) {
            tencileVector += C.tensionDown(_x, _n);
        }
        

        if (!isCarabiner)
        {
            float3 lineNormal = glm::normalize(_x[start_Idx] - _x[parent_Idx]);
            float F = __max(0.0001f, glm::dot(tencileVector, lineNormal));
            tencileVector = lineNormal * F;
        }

        tencileForce = glm::length(tencileVector);
    }

    return tencileVector;
    t_ratio = tencileForce / maxTencileForce;
}

void _lineBuilder::tensionPercentageUp()
{
}

// also do tensio here for next round
void _lineBuilder::solveUp(std::vector<float3>& _x, bool _lastRound, float _ratio)
{
    t_combined = __min(t_ratio, _ratio);        // extremely likely wrong

    float w0 = !isCarabiner;
    float w1 = 1;
    if (isLineEnd)
    {
        w1 = 0;
        _x[end_Idx] = _x[wingIdx];
    }
    if (_lastRound)
    {
        w0 = 0.1f;
        w1 = 1;
    }

    float3 L = _x[end_Idx] - _x[parent_Idx];
    float lL = glm::length(L);
    if (lL > length)
    {
        float w = w0 + w1;
        float3 n = glm::normalize(L);
        float dN = (lL - length) * straightRatio;
        if (!_lastRound)
        {
            dN *= 0.6f;
        }

        _x[parent_Idx] += n * w0 / w * dN;
        _x[end_Idx] -= n * w1 / w * dN;
    }

    for (auto& C : children) {
        C.solveUp(_x, _lastRound, t_combined);
    }

    if (_lastRound && isLineEnd)
    {
        float3 Pfinal = glm::lerp(_x[wingIdx], _x[end_Idx], t_combined);
        _x[wingIdx] = Pfinal;
        _x[end_Idx] = Pfinal;
    }

    // now solve teh sub sections, also pull only
    if (_lastRound && (numSegments > 1))
    {
        float segLength = length / numSegments;
        for (int i = 0; i < 1; i++)
        {
            uint idx0 = parent_Idx;
            for (int j = start_Idx; j <= end_Idx; j++)
            {
                L = _x[j] - _x[idx0];
                float lL = glm::length(L);
                if (lL > segLength)        // FIXME save this value
                {
                    w0 = !(idx0 == parent_Idx);
                    w1 = !(j == end_Idx);
                    float w = w0 + w1;
                    float3 n = glm::normalize(L);
                    float dN = (lL - segLength);

                    _x[idx0] += n * w0 / w * dN;
                    _x[j] -= n * w1 / w * dN;
                }

                idx0 = j;
            }
        }
    }
}


// also do tension here for next round
void _lineBuilder::solveUp_2(std::vector<float3>& _x, bool _lastRound, float _ratio, float _w0)
{
    t_combined = __min(t_ratio, _ratio); 

    float w1 = 1.0f / m_End; 
    if (isLineEnd)
    {
        _x[end_Idx] = _x[wingIdx]; // chiken and egg shit
        if (_lastRound) w1 *= 5;
    }

    float3 L = _x[end_Idx] - _x[parent_Idx];
    float Lbefore = glm::length(L);
    float lL = glm::length(L);
    if (lL > length)
    {
        float w = _w0 + w1;
        float dN = (lL - length) / lL * 0.9f;  // never 100%
        _x[parent_Idx] += L * _w0 / w * dN;
        _x[end_Idx] -= L * w1 / w * dN;
    }

    L = _x[end_Idx] - _x[parent_Idx];
    float Lafter = glm::length(L);

    for (auto& C : children) {
        C.solveUp_2(_x, _lastRound, t_combined, w1);
    }

    if (_lastRound && isLineEnd)
    {
        float3 Pfinal = glm::lerp(_x[wingIdx], _x[end_Idx], t_combined);
        _x[wingIdx] = Pfinal;
        _x[end_Idx] = Pfinal;

        float3 L = _x[end_Idx] - _x[parent_Idx];
        float Lend = glm::length(L);
        if (glm::length(L) < length && t_combined > 0.8f)
        {
            bool bCM = true;
        }
    }
}


_lineBuilder* _lineBuilder::getLine(std::string _s)
{
    if (name.find(_s) != std::string::npos)
    {
        return this;
    }

    for (auto& C : children) {
        _lineBuilder* answer = C.getLine(_s);
        if (answer) return answer;
    }

    return nullptr;
}





uint _gliderBuilder::index(uint _s, uint _c)
{
    return (_s * chordSize) + _c;
}

uint4 _gliderBuilder::crossIdx(int _s, int _c)
{
    // span clamps, chord wraps
    uint4 C;
    C.x = index(__max(_s - 1, 0), (_c + chordSize - 1) % chordSize);
    C.y = index(__min(_s + 1, spanSize - 1), (_c + 1) % chordSize);
    C.z = index(__max(_s - 1, 0), (_c + 1) % chordSize);
    C.w = index(__min(_s + 1, spanSize - 1), (_c + chordSize - 1) % chordSize);

    return C;
}

uint4 _gliderBuilder::quadIdx(int _s, int _c)
{
    // no clmaps or wraps because we calle it from spanSize-1 chorSize -1
    uint4 C;
    C.x = index(_s, _c);
    C.y = index(_s, _c + 1);
    C.z = index(_s + 1, _c + 1);
    C.w = index(_s + 1, _c);

    return C;
}


void _gliderBuilder::buildWing()
{
    leadingEdge.set(float3(0, 0, 0), float3(3, 0, 0), float3(8, 0, 0), float3(10, 0, 1.43f));
    trailingEdge.set(float3(0, 0, 0), float3(3, 0, 0), float3(7, 0, 0), float3(10, 0, 0.94f));
    curve.set(float3(0, 7.3, 0), float3(0.9, 7.3, 0), float3(4.2f, 7.3, 0), float3(4.7f, 4.55f, 0));

    float SPAN[25] = { 1, 0.976f, 0.949f, 0.918f, 0.887f, 0.854f, 0.819f, 0.783f, 0.745f, 0.706f, 0.666f, 0.624f, 0.581f, 0.539f, 0.495f, 0.449f, 0.404f, 0.358f, 0.311f, 0.264f, 0.216f, 0.169f, 0.121f, 0.073f, 0.025f };


    uint numV = spanSize * chordSize;
    x.resize(numV);
    w.resize(numV);
    cdA.resize(numV);
    cross.resize(numV);
    area.resize(numV);
    cellWidth.resize(spanSize);
    cellChord.resize(spanSize);
    ribNorm.resize(spanSize);

    std::vector<float3> lead, mid, trail;
    lead.resize(spanSize);
    mid.resize(spanSize);
    trail.resize(spanSize);

    curve.solveArcLenght();
    leadingEdge.solveArcLenght();
    trailingEdge.solveArcLenght();
    flatSpan = curve.arcLength * 2;

    //  build leading and trailing edge
    /// #######################################################################################################
    for (int s = 0; s < spanSize; s++)
    {
        float t;
        if (s < 25)   t = SPAN[s];
        else t = SPAN[49 - s];
        //float t = fabs(1.f - (2.f * (float)s / float(spanSize - 1)));

        glm::vec3 P, T, B, LEAD, TRAIL;
        LEAD = leadingEdge.pos(t);
        TRAIL = trailingEdge.pos(t);
        mid[s] = curve.pos(t);    // FIXME bad - we need measured values
        mid[s].y += 0.05f;

        if (s >= spanSize / 2) {
            mid[s].x *= -1;
        }

        lead[s] = mid[s];
        lead[s].z = LEAD.z - 1.7;                         // also move 1 m forward to help with start solve - above CG
        trail[s] = mid[s];
        trail[s].z += maxChord - TRAIL.z - 1.7;

        cellChord[s] = glm::length(lead[s] - trail[s]);
        cellWidth[s] = 0;
        if (s > 0)
        {
            cellWidth[s - 1] = glm::length(mid[s] - mid[s - 1]);
        }
    }

    // rib normals
    for (int s = 0; s < spanSize; s++)
    {
        uint a = __max(0, s - 1);
        uint b = __min(spanSize - 1, s + 1);
        ribNorm[s] = glm::normalize(glm::cross(mid[b] - mid[a], float3(0, 0, 1)));
    }

    // solve positions
    for (int s = 0; s < spanSize; s++)
    {
        float chordLength = cellChord[s];
        for (int c = 0; c < chordSize; c++)
        {
            float3 P = glm::lerp(lead[s], trail[s], wingshape[c].x);
            x[index(s, c)] = P + ribNorm[s] * (wingshape[c].y * chordLength);     // scaled with chord
            cross[index(s, c)] = crossIdx(s, c);
            cdA[index(s, c)] = 0.f;
        }
    }


    // solve area
    totalArea = 0;
    flatArea = 0;
    for (int s = 0; s < spanSize; s++)
    {
        float chordLength = cellChord[s];
        flatArea += chordLength * cellWidth[s];

        for (int c = 0; c < chordSize; c++)
        {
            uint ix = index(s, c);

            float3 v1 = x[cross[ix].x] - x[cross[ix].y];
            float3 v2 = x[cross[ix].z] - x[cross[ix].w];
            area[ix] = glm::length(glm::cross(v1, v2)) * 0.125f;
            totalArea += area[ix];
        }
    }

    // solve weight
    for (int s = 0; s < spanSize; s++)
    {
        for (int c = 0; c < chordSize; c++)
        {
            float m = (wingWeight * area[index(s, c)] / totalArea);
            w[index(s, c)] = 1.f / m;
        }
    }
}


#define L(a,b) glm::length(x[a]-x[b])
void _gliderBuilder::builWingConstraints()
{
    uint4 idx;

    for (int s = 0; s < spanSize; s++)
    {
        bool odd = s % 2;
        if (s == 25) odd = !odd;

        // span chord and diagonal
        if (s < spanSize - 1)
        {
            for (int c = 0; c < chordSize - 1; c++)
            {
                float chordPush = 0.1f;
                if (c > 12) chordPush = 0.2f;
                if (c==11 || c == 12)chordPush = 0.f;
                idx = quadIdx(s, c);
                constraints.push_back(_constraint(idx.x, idx.y, s, L(idx.x, idx.y), 1.f, 0.1f, chordPush));  // chord
                constraints.push_back(_constraint(idx.x, idx.w, s, L(idx.x, idx.w), 1.f, 0.1f, 0.4f));  // span
                if (c < 10)
                {
                    // Pust only diagonals top to bottom
                    // cross diagonals at the bottom
                    uint4 idxTop = quadIdx(s, 21 - c);
                    constraints.push_back(_constraint(idx.x, idxTop.w, s, L(idx.x, idxTop.w), 0.f, 0.f, 0.7f));  // diagonal and zigzag them
                    constraints.push_back(_constraint(idx.y, idxTop.z, s, L(idx.y, idxTop.z), 0.f, 0.f, 0.7f));  // diagonal and zigzag them

                    constraints.push_back(_constraint(idx.w, idxTop.x, s, L(idx.w, idxTop.x), 0.f, 0.f, 0.7f));  // diagonal and zigzag them
                    constraints.push_back(_constraint(idx.z, idxTop.y, s, L(idx.z, idxTop.y), 0.f, 0.f, 0.7f));  // diagonal and zigzag them
                }

                if (s < 4 || (s >= 46)) {
                    constraints.push_back(_constraint(idx.x, idx.z, s, L(idx.x, idx.z), 1.f, 0.f, 0.4f));  // diagonal and zigzag them
                    constraints.push_back(_constraint(idx.y, idx.w, s, L(idx.y, idx.w), 1.f, 0.f, 0.4f));  // diagonal and zigzag them
                }
                else {
                    if (odd) {
                        constraints.push_back(_constraint(idx.x, idx.z, s, L(idx.x, idx.z), 0.9f, 0.f, 0.4f));  // diagonal and zigzag them
                    }
                    else {
                        constraints.push_back(_constraint(idx.y, idx.w, s, L(idx.y, idx.w), 0.9f, 0.f, 0.4f));  // diagonal and zigzag them
                    }
                }
                odd = !odd;
                if (c == 11) odd = !odd;

            }
            // last span, close it off - is it nesesary, although it does stiffen the trailing edge
            constraints.push_back(_constraint(idx.w, idx.z, s, L(idx.w, idx.z), 1.f, 0.f, 0.8f));  // span
        }

        // LAST CHORD constraint

        for (int c = 0; c < chordSize - 1; c++)
        {
            float chordPush = 0.1f;
            if (c > 12) chordPush = 0.7f;
            idx = quadIdx(spanSize - 1, c);
            constraints.push_back(_constraint(idx.x, idx.y, s, L(idx.x, idx.y), 1.f, 0.f, chordPush));  // chord
        }


        // verticals
        uint a = index(s, 0);
        uint b = index(s, chordSize - 1);
        constraints.push_back(_constraint(a, b, s, L(a, b), 1.f, 0.f, 0.6f));  // trail

        a = index(s, 1);
        b = index(s, chordSize - 2);
        constraints.push_back(_constraint(a, b, s, L(a, b), 1.f, 0.f, 0.6f));  // trail-1

        a = index(s, 11);
        b = index(s, 13);
        constraints.push_back(_constraint(a, b, s, L(a, b), 1.f, 0.f, 0.4f));  // chord-1

        // diagobnal verticals in rib
        for (int c = 1; c < chordSize / 2 - 1; c++)
        {
            a = index(s, c);
            b = index(s, chordSize - 2 - c);
            constraints.push_back(_constraint(a, b, s, L(a, b), 1.f, 0.f, 0.4f));  // vertical

            a = index(s, c + 1);
            b = index(s, chordSize - 1 - c);
            constraints.push_back(_constraint(a, b, s, L(a, b), 1.f, 0.f, 0.4f));  // vertical
        }

        if (s < 4 || s >= 46)
        {
            for (int c = 1; c < chordSize / 2 - 1; c++)
            {
                a = index(s, c);
                b = index(s, chordSize - 1 - c);
                constraints.push_back(_constraint(a, b, s, L(a, b), 1.f, 0.f, 1.f));  // vertical
                /*
                a = index(s, c);
                b = index(s, chordSize - 2 - c);
                constraints.push_back(_constraint(a, b, s, L(a, b), 1.f, 0.f, 1.f));  // vertical

                a = index(s, c + 1);
                b = index(s, chordSize - 1 - c);
                constraints.push_back(_constraint(a, b, s, L(a, b), 1.f, 0.f, 1.f));  // vertical
                */
            }
        }

        // diagonal ribs at lines
        // FIXME specify in file format where the lines attach
        if ((s == 5) || (s == 8) || (s == 11) || (s == 14) || (s == 17) || (s == 20) || (s == 23) ||
            (s == 26) || (s == 29) || (s == 32) || (s == 35) || (s == 38) || (s == 41) || (s == 44))
        {
            for (int c = 0; c < chordSize - 1; c++)
            {
                if ((c == 9) || (c == 6) || (c == 3))
                {
                    uint cTop = chordSize - 1 - c;
                    a = index(s, c);
                    b = index(s, cTop);
                    constraints.push_back(_constraint(a, b, s, L(a, b), 1.f, 0.f, 0.7f));  // striaghht up

                    for (uint dc = cTop - 1; dc <= cTop + 1; dc++)
                    {
                        b = index(s - 1, dc);
                        constraints.push_back(_constraint(a, b, s, L(a, b), 1.f, 0.f, 0.7f));  // diagonal
                        b = index(s + 1, dc);
                        constraints.push_back(_constraint(a, b, s, L(a, b), 1.f, 0.f, 0.7f));  // diagonal
                    }
                }
            }
        }

    }
}



void _gliderBuilder::buildLines()
{
    float3 red = float3(0.8f, 0.01f, 0.01f);
    float3 yellow = float3(0.8f, 0.8f, 0.01f);
    float3 green = float3(0.01f, 0.8f, 0.01f);
    float3 blue = float3(0.01f, 0.01f, 0.7f);
    float3 turqoise = float3(0.01f, 0.7f, 0.7f);
    float3 orange = float3(0.7f, 0.3f, 0.01f);
    float3 black = float3(0.01f, 0.01f, 0.01f);

    // This is for Nova size medium
    auto& lA = linesRight.pushLine("A_strap", 0.4f, 0.005f, black);
    {
        auto& A1 = lA.pushLine("A1", 4.707f / 3.f, 0.0013f, red, uint2(0, 0), float3(0, 0, 0), 3);
        {
            A1.pushLine("AG01", 2.438f / 2.f, 0.0009f, red, uint2(23, 9), x[index(23, 9)], 2);
            A1.pushLine("AG02", 2.405f / 2.f, 0.0009f, red, uint2(20, 9), x[index(20, 9)], 2);
        }
        auto& A2 = lA.pushLine("A2", 4.742f / 3.f, 0.0013f, red, uint2(0, 0), float3(0, 0, 0), 3);
        {
            A2.pushLine("AG03", 2.332f / 2.f, 0.0009f, red, uint2(17, 9), x[index(17, 9)], 2);
            A2.pushLine("AG04", 2.302f / 2.f, 0.0009f, red, uint2(14, 9), x[index(14, 9)], 2);
        }
    }

    auto& lA3 = linesRight.pushLine("A3_strap", 0.4f, 0.005f, black);
    {
        auto& A3 = lA3.pushLine("A_ears", 3.932f / 2.f, 0.0013f, red, uint2(0, 0), float3(0, 0, 0), 2);
        {
            A3.pushLine("AG05", 3.041f / 2.f, 0.0009f, red, uint2(11, 9), x[index(11, 9)], 2);
            A3.pushLine("AG06", 2.927f / 2.f, 0.0009f, red, uint2(8, 9), x[index(8, 9)], 2);
            A3.pushLine("AG07", 2.801f / 2.f, 0.0009f, red, uint2(5, 9), x[index(5, 9)], 2);
        }
    }

    auto& lB = linesRight.pushLine("B_strap", 0.47f, 0.005f, black);
    {
        auto& B1 = lB.pushLine("B1", 4.663f / 2.f, 0.0013f, yellow, uint2(0, 0), float3(0, 0, 0), 2);
        {
            B1.pushLine("BG01", 2.386f / 2.f, 0.0009f, yellow, uint2(23, 6), x[index(23, 6)], 2);
            B1.pushLine("BG02", 2.356f / 2.f, 0.0009f, yellow, uint2(20, 6), x[index(20, 6)], 2);
        }
        auto& B2 = lB.pushLine("B2", 4.71f / 2.f, 0.0013f, yellow, uint2(0, 0), float3(0, 0, 0), 2);
        {
            B2.pushLine("BG03", 2.278f / 2.f, 0.0009f, yellow, uint2(17, 6), x[index(17, 6)], 2);
            B2.pushLine("BG04", 2.256f / 2.f, 0.0009f, yellow, uint2(14, 6), x[index(14, 6)], 2);
        }
        auto& B3 = lB.pushLine("B3", 3.879f / 2.f, 0.0013f, yellow, uint2(0, 0), float3(0, 0, 0), 2);
        {
            B3.pushLine("BG05", 3.026f / 2.f, 0.0009f, yellow, uint2(11, 6), x[index(11, 6)], 2);
            B3.pushLine("BG06", 2.926f / 2.f, 0.0009f, yellow, uint2(8, 6), x[index(8, 6)], 2);
            B3.pushLine("BG07", 2.817f / 2.f, 0.0009f, yellow, uint2(5, 6), x[index(5, 6)], 2);
        }


        auto& S1 = lB.pushLine("S1", 4.914f / 3.f, 0.0013f, green, uint2(0, 0), float3(0, 0, 0), 3);
        {
            S1.pushLine("SAG1", 1.418f, 0.0009f, green, uint2(2, 9), x[index(2, 9)]);
            S1.pushLine("SBG1", 1.437f, 0.0009f, green, uint2(2, 5), x[index(2, 5)]);
            auto& SM1 = S1.pushLine("SM1", 0.699f, 0.0009f, green, uint2(23, 4)); {
                SM1.pushLine("SG01", 0.451f, 0.0009f, green, uint2(0, 9), x[index(0, 9)]);
                SM1.pushLine("SG02", 0.471f, 0.0009f, green, uint2(0, 7), x[index(0, 7)]);
            }
            auto& SM2 = S1.pushLine("SM2", 0.898f, 0.0009f, green, uint2(20, 4)); {
                SM2.pushLine("SG03", 0.346f, 0.0009f, green, uint2(0, 3), x[index(0, 3)]);
                SM2.pushLine("SG04", 0.431f, 0.0009f, green, uint2(0, 1), x[index(0, 1)]);
            }
        }
    }

    auto& lC = linesRight.pushLine("C_strap", 0.54f, 0.005f, black);
    {
        auto& C1 = lC.pushLine("C1", 4.663f / 2.f, 0.0013f, turqoise, uint2(0, 0), float3(0, 0, 0), 2);
        {
            C1.pushLine("CG01", 2.386f / 2.f, 0.0009f, turqoise, uint2(23, 3), x[index(23, 3)], 2);
            C1.pushLine("CG02", 2.356f / 2.f, 0.0009f, turqoise, uint2(20, 3), x[index(20, 3)], 2);
        }
        auto& C2 = lC.pushLine("C2", 4.71f / 2.f, 0.0013f, turqoise, uint2(0, 0), float3(0, 0, 0), 2);
        {
            C2.pushLine("CG03", 2.278f / 2.f, 0.0009f, turqoise, uint2(17, 3), x[index(17, 3)], 2);
            C2.pushLine("CG04", 2.256f / 2.f, 0.0009f, turqoise, uint2(14, 3), x[index(14, 3)], 2);
        }
        auto& C3 = lC.pushLine("C3", 3.879f / 2.f, 0.0013f, turqoise, uint2(0, 0), float3(0, 0, 0), 2);
        {
            C3.pushLine("CG05", 3.026f / 3.f, 0.0009f, turqoise, uint2(11, 3), x[index(11, 3)], 3);
            C3.pushLine("CG06", 2.926f / 3.f, 0.0009f, turqoise, uint2(8, 3), x[index(8, 3)], 3);
            C3.pushLine("CG07", 2.817f / 3.f, 0.0009f, turqoise, uint2(5, 3), x[index(5, 3)], 3);
        }
    }


    auto& strapF = lC.pushLine("F_strap", 0.08f, 0.005f, black);

    auto& FF2 = strapF.pushLine("FF1+2", 1.2f + 0.464f, 0.002f, red, uint2(0, 0), float3(0, 0, 0));
    {
        //auto& FF2 = lFF1.pushLine("FF2", , 0.002f, red);
        {
            auto& F1 = FF2.pushLine("F1", (2.451f) / 2.f, 0.002f, red, uint2(0, 0), float3(0, 0, 0), 2);
            {
                auto& FM1 = F1.pushLine("FM1", 2.468f / 2.f, 0.0012f, red, uint2(0, 0), float3(0, 0, 0), 2);
                {
                    FM1.pushLine("FG01", 1.474f, 0.0009f, red, uint2(22, 0), x[index(22, 0)]);
                    FM1.pushLine("FG02", 1.138f, 0.0009f, red, uint2(20, 0), x[index(20, 0)]);
                }
                auto& FM2 = F1.pushLine("FM2", 2.309f / 2.f, 0.0012f, red, uint2(0, 0), float3(0, 0, 0), 2);
                {
                    FM2.pushLine("FG03", 1.087f, 0.0009f, red, uint2(16, 0), x[index(16, 0)]);
                    FM2.pushLine("FG04", 0.923f, 0.0009f, red, uint2(14, 0), x[index(14, 0)]);
                }
            }
            auto& F2 = FF2.pushLine("F2", 2.984f / 2.f, 0.002f, red, uint2(0, 0), float3(0, 0, 0), 2);
            {
                auto& FM3 = F2.pushLine("FM3", 1.674f, 0.0012f, red, uint2(0, 0), float3(0, 0, 0));
                {
                    FM3.pushLine("FG05", 0.927f, 0.0009f, red, uint2(11, 0), x[index(11, 0)]);
                    FM3.pushLine("FG06", 0.723f, 0.0009f, red, uint2(8, 0), x[index(8, 0)]);
                }
                auto& FM4 = F2.pushLine("FM4", 1.389f, 0.0012f, red, uint2(0, 0), float3(0, 0, 0));
                {
                    FM4.pushLine("FG07", 0.821f, 0.0009f, red, uint2(5, 0), x[index(5, 0)]);
                    FM4.pushLine("FG08", 0.519f, 0.0009f, red, uint2(3, 0), x[index(3, 0)]);
                }
            }
        }
    }


    linesRight.length = 0.08f;          // make it 1 cm, because the layout of the carabiner messes with the maths the curve nad strap widths
    linesRight.name = "right_Carabiner";
    linesLeft.name = "left_Carabiner";

    linesLeft = linesRight;
    linesLeft.mirror(50);

    linesLeft.solveLayout(float3(-0.1f, 0, 0), chordSize);        // this does a basic layout on lenths
    linesRight.solveLayout(float3(0.1f, 0, 0), chordSize);


    //_lineBuilder *pR_FF2 = linesRight.getLine("FF2");pBrakeLeft
    //if (pR_FF2) pR_FF2->length = 0.03f;
}


void _gliderBuilder::generateLines()
{
    float spacing = 0.2f;

    // calc numVerts;
    uint startVert = x.size();
    uint numVerts = 3;   //The two caribiners
    numVerts += linesLeft.calcVerts(spacing) * 2;

    uint totalVerts = startVert + numVerts;
    x.resize(totalVerts);
    w.resize(totalVerts);
    cdA.resize(totalVerts);

    // two caribiners
    // FIXME in future start from CG
    x[startVert + 0] = float3(0.0f, -0.2f, 0);
    w[startVert + 0] = 1.f / 90.f;
    cdA[startVert + 0] = 0.7f * 0.4f;      // Cd * A

    x[startVert + 1] = float3(-0.15f, 0, 0);
    x[startVert + 2] = float3(0.15f, 0, 0);
    w[startVert + 1] = 1.f / 3.f;
    w[startVert + 2] = 1.f / 3.f;
    cdA[startVert + 1] = 0;
    cdA[startVert + 2] = 0;

    constraints.push_back(_constraint(startVert + 1, startVert + 2, 0, glm::length(x[startVert + 1] - x[startVert + 2]), 1.f, 1.f, 0.f));
    constraints.push_back(_constraint(startVert + 0, startVert + 1, 0, glm::length(x[startVert + 1] - x[startVert]), 1.f, 1.f, 0.f));
    constraints.push_back(_constraint(startVert + 0, startVert + 2, 0, glm::length(x[startVert + 2] - x[startVert]), 1.f, 1.f, 0.f));

    uint idx = startVert + 3;
    linesLeft.addVerts(x, w, cdA, idx, startVert + 1, spacing);
    linesRight.addVerts(x, w, cdA, idx, startVert + 2, spacing);
}


void _gliderBuilder::solveLines()
{
}

void _gliderBuilder::renderGui(Gui* mpGui)
{
}


void _gliderBuilder::visualsPack(rvB* ribbon, rvPacked* packed, int& count, bool& changed)
{
    count = 0;

    for (int s = 0; s < spanSize; s++)
    {
        for (int c = 0; c < chordSize; c++)
        {
            setupVert(&ribbon[count], c == 0 ? 0 : 1, x[index(s, c)], 0.005f);     count++;
        }
    }

    // span
    for (int c = 0; c < chordSize; c++)
    {
        if ((c == 0) || (c == 3) || (c == 6) || (c == 9) || (c == 12))
            //if ((c == 0) || (c == 11))
        {
            for (int s = 0; s < spanSize; s++)
            {
                setupVert(&ribbon[count], s == 0 ? 0 : 1, x[index(s, c)], 0.005f);     count++;
            }
        }
    }

    /*
    // constraints
    for (int i = 0; i < constraints.size(); i++)
    {
        setupVert(&ribbon[count], 0, x[constraints[i].idx_0], 0.004f, 0);     count++;
        setupVert(&ribbon[count], 1, x[constraints[i].idx_1], 0.004f, 0);     count++;
    }
    */
    //lines
    linesLeft.addRibbon(ribbon, x, count);
    linesRight.addRibbon(ribbon, x, count);

    for (int i = 0; i < count; i++)
    {
        packed[i] = ribbon[i].pack();
    }
    changed = true;
}





void _gliderBuilder::buildCp()
{
    Cp.resize(11);
    wingshape.resize(25);

    for (int i = 0; i < 11; i++)
    {
        Cp[i].resize(25);
        char name[256];
        sprintf(name, "F:\\xfoil\\naca4415_A_%d.txt", i);
        analizeCp(name, i);
    }

    std::ofstream outfile("F:\\xfoil\\wing_10.txt");    // 9 degrees
    for (int i = 0; i < wingshape.size(); i++)
    {
        float shp = 0.4f;
        float y = wingshape[i].y;
        if (i == 22) y -= 0.03f * shp;
        if (i == 23) y -= 0.09f * shp;
        if (i == 24) y -= 0.15f * shp;

        if (i == 2) y -= 0.03f * shp;
        if (i == 1) y -= 0.09f * shp;
        if (i == 0) y -= 0.15f * shp;
        //char T[256];
        //sprintf(T, "", );
        outfile << wingshape[i].x << " " << y << "\n";
    }
    outfile.close();

    std::ofstream out2file("F:\\xfoil\\wing_30.txt");    // 9 degrees
    for (int i = 0; i < wingshape.size(); i++)
    {
        float shp = 0.3f;
        float y = wingshape[i].y;
        if (i == 22) y -= 0.03f * shp;
        if (i == 23) y -= 0.09f * shp;
        if (i == 24) y -= 0.15f * shp;

        if (i == 2) y -= 0.03f * shp;
        if (i == 1) y -= 0.09f * shp;
        if (i == 0) y -= 0.15f * shp;
        //char T[256];
        //sprintf(T, "", );
        out2file << wingshape[i].x << " " << y << "\n";
    }
    out2file.close();

    //for
    char name[256];
    std::string line;
    float x, y, cp;
    
    std::ofstream outcpfile("F:\\xfoil\\wing_CP.txt");    // 9 degrees
    for (int j = 0; j < 11; j++)
    {
        sprintf(name, "F:\\xfoil\\naca4415_A_%d_FLAP10.txt", j);
        std::ifstream flap10(name);
        std::getline(flap10, line);
        std::getline(flap10, line);
        std::getline(flap10, line);

        sprintf(name, "F:\\xfoil\\naca4415_A_%d_FLAP20.txt", j);
        std::ifstream flap20(name);
        std::getline(flap20, line);
        std::getline(flap20, line);
        std::getline(flap20, line);

        sprintf(name, "F:\\xfoil\\naca4415_A_%d_FLAP30.txt", j);
        std::ifstream flap30(name);
        std::getline(flap30, line);
        std::getline(flap30, line);
        std::getline(flap30, line);

        sprintf(name, "F:\\xfoil\\naca4415_A_%d_FLAP40.txt", j);
        std::ifstream flap40(name);
        std::getline(flap40, line);
        std::getline(flap40, line);
        std::getline(flap40, line);

        sprintf(name, "F:\\xfoil\\naca4415_A_%d_FLAP50.txt", j);
        std::ifstream flap50(name);
        std::getline(flap50, line);
        std::getline(flap50, line);
        std::getline(flap50, line);

        for (int k = 0; k < 25; k++)
        {
            CPbrakes[0][j][k] = Cp[j][k];

            flap10 >> x >> y >> cp;
            CPbrakes[1][j][24 - k] = cp;

            flap20 >> x >> y >> cp;
            CPbrakes[2][j][24 - k] = cp;

            flap30 >> x >> y >> cp;
            CPbrakes[3][j][24 - k] = cp;

            flap40 >> x >> y >> cp;
            CPbrakes[4][j][24 - k] = cp;

            flap50 >> x >> y >> cp;
            CPbrakes[5][j][24 - k] = cp;
        }

        for (int k = 0; k < 25; k++)
        {
            outcpfile << CPbrakes[0][j][k] << "   " << CPbrakes[1][j][k] << "   " << CPbrakes[2][j][k] << "   " << CPbrakes[3][j][k] << "   " << CPbrakes[4][j][k] << "   " << CPbrakes[5][j][k] << "\n";
        }
        outcpfile << "\n";

        flap10.close();
        flap20.close();
        flap30.close();
        flap40.close();
        flap50.close();
    }



    std::ofstream outTfile("F:\\xfoil\\T.txt");    // 9 degrees
    //for (float r = 1.02f; r > 0.8f; r -= 0.001f)

    float Ta = 1.1f;
    float SLOPE = -100;
    float cutoffX = atan(sqrt(-1.f / SLOPE));
    float cutoffT = Ta / tan(cutoffX);

    //outTfile << "SLOPE " << SLOPE << "\n";
    //outTfile << "Ta    " << Ta << "\n";
    //outTfile << "(" << cutoffX << ", " << cutoffT << ")\n\n";

    for (float x = 0.3f; x > -0.3f; x -= 0.01f)
    {
        //float x = acos(r) * 1.75f;
        float r = cos(0.57f * x);
        if (x < 0)
        {
            float r2 = 1.0f - r;
            r = 1.0f + r2;
        }
        
        //float slope = -1 / TAN / TAN;
        float cutoffR = cos(0.57f * cutoffX);

        if (x < cutoffX)
        {
            //float X2 = SLOPE * (x - cutoffX);
            float X2 = SLOPE * (r - cutoffR);
            float Tmax2 = cutoffT + 400 * (X2 * X2);
            outTfile << r << ", " << Tmax2 << ", " << "    ----> LEFT\n";
        }
        else
        {
            float TAN = tan(x);
            float Tmax = Ta / TAN;
            outTfile << r << ", " << Tmax << ", " << "\n";
        }
    }
    outTfile.close();

    bool cm = true;
}








void _gliderBuilder::analizeCp(std::string name, int cp_idx)
{
    std::ifstream infile(name.c_str());    // 9 degrees
    std::string line;
    std::getline(infile, line);
    std::getline(infile, line);
    std::getline(infile, line);

    struct _P {
        float3 p;
        float3 n;
        float3 f;
        float3 sumF;    // when sampling down
        float3 sumN;    // when sampling down
        float cp;
        float area;
        uint idx;
    };
    std::vector<_P> p;
    float3 forceTotal = float3(0, 0, 0);
    float3 forceTotalSmall = float3(0, 0, 0);
    float3 staticTotal = float3(0, 0, 0);

    float x, y, cp;
    while (infile >> x >> y >> cp)
    {
        _P pnew;
        pnew.p = float3(x, y, 0);
        pnew.cp = cp;
        p.push_back(pnew);
    }

    float3 R = float3(0, 0, 1);
    for (int i = 1; i < p.size() - 1; i++)
    {
        p[i].n = glm::normalize(glm::cross(p[i + 1].p - p[i - 1].p, R));
        p[i].area = glm::length(p[i + 1].p - p[i - 1].p) * 0.5f;
    }
    p[0].n = glm::normalize(glm::cross(p[1].p - p[0].p, R));
    p[0].area = glm::length(p[1].p - p[0].p) * 0.5f;
    uint last = p.size() - 1;
    p[last].n = glm::normalize(glm::cross(p[last].p - p[last - 1].p, R));
    p[last].area = glm::length(p[last].p - p[last - 1].p) * 0.5f;

    for (int i = 0; i < p.size(); i++)
    {
        p[i].f = -p[i].n * p[i].cp * p[i].area;
        forceTotal += p[i].f;
        staticTotal += p[i].n * 1.f * p[i].area;
    }




    // NOW reduce
    uint nose = 5;
    uint total = 20 + nose;
    std::vector<_P> tiny;
    tiny.resize(total);
    tiny[0].idx = 0;
    uint tinyIdx = 0;

    float d[8] = { 1.f, 0.73f, 0.37f, 0.09f, 0.09f, 0.37f, 0.73f, 1.f };

    uint ABC[8];
    ABC[0] = 0;
    ABC[7] = last;
    int idx = 1;
    for (int i = 0; i < p.size(); i++)
    {
        if (idx < 4) {
            if (p[i].p.x < d[idx]) {
                ABC[idx] = i;

                uint i0 = ABC[idx - 1];
                tiny[tinyIdx + 0].idx = i0;
                tiny[tinyIdx + 1].idx = i0 + ((i - i0) / 3);
                tiny[tinyIdx + 2].idx = i0 + ((i - i0) * 2 / 3);
                tinyIdx += 3;
                idx++;
            }
        }
        else {
            if (p[i].p.y < 0)
            {
                if (p[i].p.x > d[idx]) {
                    ABC[idx] = i;

                    if (idx == 4) {
                        // nose
                        uint i0 = ABC[idx - 1];
                        tiny[tinyIdx + 0].idx = i0;
                        tiny[tinyIdx + 1].idx = i0 + ((i - i0) / 6);
                        tiny[tinyIdx + 2].idx = i0 + ((i - i0) * 2 / 6);
                        tiny[tinyIdx + 3].idx = i0 + ((i - i0) * 3 / 6);
                        tiny[tinyIdx + 4].idx = i0 + ((i - i0) * 4 / 6);
                        tiny[tinyIdx + 5].idx = i0 + ((i - i0) * 5 / 6);
                        tiny[tinyIdx + 6].idx = i;
                        tinyIdx += 7;
                    }
                    else
                    {
                        uint i0 = ABC[idx - 1];
                        tiny[tinyIdx + 0].idx = i0 + ((i - i0) / 3);
                        tiny[tinyIdx + 1].idx = i0 + ((i - i0) * 2 / 3);
                        tiny[tinyIdx + 2].idx = i;
                        tinyIdx += 3;
                    }
                    idx++;
                }
            }
        }
    }
    uint i0 = ABC[idx - 1];
    tiny[tinyIdx + 0].idx = i0 + ((last - i0) / 3);
    tiny[tinyIdx + 1].idx = i0 + ((last - i0) * 2 / 3);
    tiny[tinyIdx + 2].idx = last;


    tinyIdx = 0;
    uint pIdx = 0;
    while (tinyIdx < total)
    {
        tiny[tinyIdx].cp = 0;
        uint idxEnd = last;
        if (tinyIdx < total - 1) idxEnd = (tiny[tinyIdx].idx + tiny[tinyIdx + 1].idx) >> 1;
        for (int j = pIdx; j < idxEnd; j++)
        {
            tiny[tinyIdx].cp += p[j].cp;
        }
        tiny[tinyIdx].cp /= (float)(idxEnd - pIdx);
        pIdx = idxEnd;
        tinyIdx++;
    }

    for (int j = 0; j < total; j++)
    {
        tiny[j].p = p[tiny[j].idx].p;
    }
    // area and normal
    for (int j = 0; j < total; j++)
    {
        uint start = __max(0, j - 1);
        uint end = __min(total - 1, j + 1);
        tiny[j].area = glm::length(tiny[end].p - tiny[start].p) * 0.5f;
        tiny[j].n = glm::normalize(glm::cross(tiny[end].p - tiny[start].p, R));
        tiny[j].f = -tiny[j].n * tiny[j].cp * tiny[j].area;
        forceTotalSmall += tiny[j].f;
    }

    bool bCm = true;

    // Now save it all
    for (int j = 0; j < total; j++)
    {
        Cp[cp_idx][24 - j] = tiny[j].cp;
        wingshape[24 - j] = float3(tiny[j].p.x, tiny[j].p.y, tiny[j].area);
    }

    
}









void _gliderRuntime::renderGui(Gui* mpGui)
{
    ImGui::Columns(2);
    //ImGui::BeginChildFrame(1000, ImVec2(300, 160));
    //ImGui::Text("leadingEdge");
    //leadingEdge.renderGui(mpGui);
    //ImGui::EndChildFrame();
    float3 velHor = v[chordSize * spanSize];

    if (ImGui::SliderFloat("weight", &weightShift, 0, 1))
    {
        constraints[1].l = weightShift * weightLength;
        constraints[2].l = (1.0f - weightShift) * weightLength;
    }
    ImGui::Checkbox("symmetry", &symmetry);
    
    if(pBrakeLeft) ImGui::SliderFloat("F left", &pBrakeLeft->length, 0.1f, maxBrake);
    if(pBrakeRight) ImGui::SliderFloat("F right", &pBrakeRight->length, 0.1f, maxBrake);

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
    ImGui::Text("%d", frameCnt);

    ImGui::NewLine();
    ImGui::Text("v_root %2.1f, %2.3f, %2.1f", velHor.x, velHor.y, velHor.z);
    ImGui::Text("a_root %2.1f, %2.3f, %2.1f", a_root.x, a_root.y, a_root.z);
    ImGui::Text("vBody %2.3f m/s", vBody);

    ImGui::NewLine();

    ImGui::Text("pos %2.1f, %2.1f, %2.1f", ROOT.x, ROOT.y, ROOT.z);
    ImGui::Text("pilot / lines/ wing %2.1f, %2.1f g, %2.1f", weightPilot, weightLines * 1000.f, weightWing);


    velHor.y = 0;
    float Vh = glm::length(velHor) * 3.6f;
    ImGui::Text("speed %2.1f km/h", Vh);
    ImGui::Text("vertical %2.1f m/s", v[chordSize * spanSize].y);

    ImGui::NewLine();
    ImGui::Text("wing lift %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(sumFwing), sumFwing.x, sumFwing.y, sumFwing.z);
    ImGui::Text("wing F %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(sumFwing_F), sumFwing_F.x, sumFwing_F.y, sumFwing_F.z);
    ImGui::Text("wing C %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(sumFwing_C), sumFwing_C.x, sumFwing_C.y, sumFwing_C.z);
    ImGui::Text("wing B %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(sumFwing_B), sumFwing_B.x, sumFwing_B.y, sumFwing_B.z);
    ImGui::Text("wing A %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(sumFwing_A), sumFwing_A.x, sumFwing_A.y, sumFwing_A.z);
    float3 abc = sumFwing_A + sumFwing_B + sumFwing_C;
    ImGui::Text("ABC %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(abc), abc.x, abc.y, abc.z);

    ImGui::Text("wing drag %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(sumDragWing), sumDragWing.x, sumDragWing.y, sumDragWing.z);
    ImGui::Text("line drag %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(AllSummedLinedrag), AllSummedLinedrag.x, AllSummedLinedrag.y, AllSummedLinedrag.z);
    ImGui::Text("pilot drag %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(pilotDrag), pilotDrag.x, pilotDrag.y, pilotDrag.z);
    
    ImGui::NewLine();
    ImGui::Text("ALLSUMMED %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(AllSummedForce), AllSummedForce.x, AllSummedForce.y, AllSummedForce.z);

    ImGui::Text("tips %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(sumLineEndings), sumLineEndings.x, sumLineEndings.y, sumLineEndings.z);
    ImGui::Text("carabiner Left %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(tensionCarabinerLeft), tensionCarabinerLeft.x, tensionCarabinerLeft.y, tensionCarabinerLeft.z);
    ImGui::Text("carabinerRight %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(tensionCarabinerRight), tensionCarabinerRight.x, tensionCarabinerRight.y, tensionCarabinerRight.z);
    float3 sum = tensionCarabinerLeft + tensionCarabinerRight;
    ImGui::Text("BODY %2.1f N    (%2.1f, %2.1f, %2.1f)", glm::length(sum), sum.x, sum.y, sum.z);





    ImGui::NewLine();
    ImGui::Text("AoA cellV");
    for (int i = 0; i < spanSize; i += 1)
    {
        ImGui::Text(" % 2.1f, %4.1f, %4.1f, %4d%%, %4.1f", (cells[i].AoA * 57), cells[i].cellVelocity, cells[i].brakeSetting, (int)(cells[i].pressure * 99.f), cells[i].ragdoll);
    }


    ImGui::NextColumn();


    ImGui::Text("wing v %2.1f m/s    (%2.1f, %2.1f, %2.1f)", glm::length(wingV), wingV.x, wingV.y, wingV.z);
    ImGui::Text("winf a %2.1f m/s/s    (%2.1f, %2.1f, %2.1f)", glm::length(wingA), wingA.x, wingA.y, wingA.z);
    ImGui::NewLine();
    //23/ 9
    ImGui::Text("AG01 v %2.1f m/s    (%2.1f, %2.1f, %2.1f)", glm::length(v[23 * 25 + 9]), v[23 * 25 + 9].x, v[23 * 25 + 9].y, v[23 * 25 + 9].z);
    ImGui::Text("root -> AG01 %2.6f", root_AG01);

    ImGui::NewLine();
    linesLeft.renderGui();
    linesRight.renderGui();

}



void _gliderRuntime::setup(std::vector<float3>& _x, std::vector<float>& _w, std::vector<float>& _cd, std::vector<uint4>& _cross, uint _span, uint _chord, std::vector<_constraint>& _constraints)
{
    x = _x;
    w = _w;
    cd = _cd;
    cross = _cross;
    l.resize(x.size());
    v.resize(x.size());
    memset(&v[0], 0, sizeof(float3) * x.size());
    n.resize(_span * _chord);
    t.resize(_span * _chord);

    x_old.resize(x.size());
    l_old.resize(x.size());

    constraints = _constraints;

    spanSize = _span;
    chordSize = _chord;

    cells.resize(_span);



    //ROOT = float3(5500, 1900, 11500);   // walensee
    ROOT = float3(-7500, 1000, -10400);// hulfteff

    for (int i = 0; i < spanSize; i++)
    {
        cells[i].v = float3(0, -1.5, -7);
        cells[i].pressure = 1;
    }

    for (int i = 0; i < v.size(); i++)
    {
        v[i] = float3(0, -1.5, -7);
    }

    for (int i = 0; i < x.size(); i++)
    {
        x[i] += float3(0, 0, 0);
    }
    for (int i = 0; i < n.size(); i++)
    {
        n[i] = 0.125f * glm::cross((x[cross[i].z] - x[cross[i].w]), (x[cross[i].x] - x[cross[i].y]));
    }

    frameCnt = 0;
    vBody = 0;

    pBrakeLeft = linesLeft.getLine("FF1");
    pBrakeRight = linesRight.getLine("FF1");

    pEarsLeft = linesLeft.getLine("ears");
    pEarsRight = linesRight.getLine("ears");
    //_lineBuilder *pR_FF2 = linesRight.getLine("FF2");
    //if (pR_FF2) pR_FF2->length = 0.03f;

    pSLeft = linesLeft.getLine("S1");
    pSRight = linesRight.getLine("S1");

    static bool firstStart = true;

    if (firstStart && pBrakeLeft)
    {
        firstStart = false;
        if (pBrakeLeft)      maxBrake = pBrakeLeft->length;
        if (pEarsLeft)  maxEars = pEarsLeft->length;
        if (pSLeft)  maxS = pSLeft->length;
    }
}


void _gliderRuntime::solve(float _dT)
{
    if (runcount == 0)
    {
        return; // convenient breakpoint
    }
    runcount--;
    frameCnt++;

    _dT = 0.003f;
    // fixme later per cell
    float3 wind = float3(0, 1.5f, 9.0f) * 0.007f;

    sumFwing = float3(0, 0, 0);
    sumFwing_F = float3(0, 0, 0);
    sumFwing_C = float3(0, 0, 0);
    sumFwing_B = float3(0, 0, 0);
    sumFwing_A = float3(0, 0, 0);
    sumDragWing = float3(0, 0, 0);
    AllSummedForce = float3(0, 0, 0);
    AllSummedLinedrag = float3(0, 0, 0);

    {
        FALCOR_PROFILE("aero");
        for (int i = 0; i < spanSize; i++)
        {
            cells[i].old_pressure = cells[i].pressure;
        }

        for (int i = 0; i < spanSize; i++)
        {

            // front - ha ha ha - this is harder, unless I specifically orent my wing when I calculate brakes
            cells[i].front = glm::normalize(x[index(i, 12)] - x[index(i, 3)]);  // From C forward

            if (std::isnan(cells[i].front.x))
            {
                bool bcm = true;
            }

            // up vector ####################################################################
            cells[i].up = float3(0, 0, 0);
            for (int j = 0; j < 10; j++)
            {
                cells[i].up -= n[index(i, j)];
                cells[i].up += n[index(i, 24 - j)];
            }
            float3 r = glm::cross(cells[i].up, cells[i].front);
            cells[i].up = glm::cross(cells[i].front, r);
            cells[i].up = glm::normalize(cells[i].up);

            float3 frontBrake = glm::normalize(x[index(i, 3)] - x[index(i, 0)]);  // From C forward
            float3 upBrake = glm::normalize(glm::cross(frontBrake, r));
            cells[i].AoBrake = - glm::dot(upBrake, cells[i].front) * 0.8f;
            cells[i].AoBrake = __min(1.f, cells[i].AoBrake);
            cells[i].AoBrake = __max(0.f, cells[i].AoBrake);
                
            if (std::isnan(cells[i].AoBrake))
            {
                bool bcm = true;
            }
            if (std::isnan(cells[i].v.x))
            {
                bool bcm = true;
            }


            float3 VV = float3(0, 0, 0);
            for (int j = 0; j < chordSize; j++)
            {
                VV += v[index(i, j)];
            }
            VV /= chordSize;
            cells[i].v = lerp(cells[i].v, VV, 1.f);
            cells[i].cellVelocity = glm::length(cells[i].v);

            //if (std::isnan(cells[i].cellVelocity))
            if (cells[i].cellVelocity > 100)
            {
                bool bcm = true;
            }

            if (cells[i].cellVelocity < 0.1f)
            {
                bool bcm = true;
            }

            float3 cellWind = wind - cells[i].v;
            cells[i].rel_wind = cellWind;
            float windSpeed = glm::length(cellWind);
            cells[i].rho = 0.5 * 1.11225f * windSpeed * windSpeed;       // dynamic pressure
            float3 windDir = glm::normalize(cellWind);   // per cell later
            cells[i].AoA = asin(glm::dot(cells[i].up, windDir));
            if (glm::dot(windDir, cells[i].front) > 0) {
                cells[i].AoA = 3.141592653589793238462f - cells[i].AoA;
            }
            if (i < 10)
            {
                cells[i].AoA += (float)(10 - i) * 0.005f;
            }
            if (i > spanSize - 11)
            {
                cells[i].AoA += (float)(i - (spanSize -11)) * 0.005f;
            }
            //cells[i].AoA = __max(0, cells[i].AoA);

            cells[i].chordLength = 3.f; // fixme
            cells[i].reynolds = (1.125f * windSpeed * cells[i].chordLength) / (0.0000186);     // dynamic viscocity 25C air
            cells[i].cD = 0.027f / pow(cells[i].reynolds, 1.f / 7.f);

            // cell pressure
            float AoAlookup = ((cells[i].AoA * 57.f) + 6.f) / 3.f;
            int idx = __max(0, __min(9, (int)floor(AoAlookup)));
            float dL = __min(1, AoAlookup - idx);
            if (idx == 0) dL = __max(0, dL);
            float pressure10 = __max(0, glm::lerp(CPbrakes[0][idx][10], CPbrakes[0][idx + 1][10], dL));
            float pressure11 = __max(0, glm::lerp(CPbrakes[0][idx][11], CPbrakes[0][idx + 1][11], dL));
            float P = __max(pressure10, pressure11);
            float windPscale = __min(1.f, windSpeed / 4.f);
            P *= pow(windPscale, 4);

            cells[i].old_pressure = glm::lerp(cells[i].old_pressure, P, 0.01f);
        }

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
                float MAX = __max(__max(cells[i].old_pressure, cells[i - 1].old_pressure * 0.80f), cells[i + 1].old_pressure * 0.80f);
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

            float AoAlookup = ((cells[span].AoA * 57.f) + 6.f) / 3.f;
            int idx = __max(0, __min(9, (int)floor(AoAlookup)));
            float dL = __min(1, AoAlookup - idx);
            if (idx == 0) dL = __max(0, dL);
            float cp = glm::lerp(CPbrakes[0][idx][chord], CPbrakes[0][idx + 1][chord], dL);

            float brake = __max(0.f, __min(4.9f, cells[span].AoBrake * 6.95f));
            cells[span].brakeSetting = brake;
            int idxBrake = (int)floor(brake);
            float dB = __min(1, brake - idxBrake);
            float CP_B0 = glm::lerp(CPbrakes[idxBrake][idx][chord], CPbrakes[idxBrake+1][idx][chord], dB);
            float CP_B1 = glm::lerp(CPbrakes[idxBrake][idx+1][chord], CPbrakes[idxBrake + 1][idx+1][chord], dB);
            cp = glm::lerp(CP_B0, CP_B1, dL);

            // rag doll pressures
            cells[span].ragdoll = 0;
            if (idx > 8) cells[span].ragdoll = __min(0.6f, __max(0, AoAlookup - 8));
            if (cells[span].AoA <= -0.1) cells[span].ragdoll = __min(1, __max(0, (0.1f - cells[span].AoA) * 40.f));
            float cpRagdoll = -glm::dot(glm::normalize(n[i]), glm::normalize(cells[span].rel_wind));
            cp = glm::lerp(cp, __max(0, cpRagdoll), cells[span].ragdoll);




            n[i] = n[i] * (cells[span].pressure - cp) * cells[span].rho;
            //n[i] = n[i] * (0 - cp) * cells[span].rho;
            sumFwing += n[i];

            if (span > 3 || span < 46) {
                if (chord < 2 || chord > 22) {
                    sumFwing_F += n[i];
                }

                if ((chord >= 2 && chord <= 4) || (chord >= 20 && chord <= 22)) {
                    sumFwing_C += n[i];
                }

                if ((chord >= 5 && chord <= 7) || (chord >= 17 && chord <= 19)) {
                    sumFwing_B += n[i];
                }

                if ((chord >= 8 && chord <= 16)) {
                    sumFwing_A += n[i];
                }
            }

            //float Newton = glm::length(n[i]);

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
    float3 oldB = x[start];
    float3 oldL = x[start + 1];
    float3 oldR = x[start + 2];

    weightLines = 0;
    weightPilot = 0;
    weightWing = 0;

    pilotDrag = float3(0, 0, 0);
    lineDrag = float3(0, 0, 0);

    vBody = glm::lerp(vBody, glm::length(v[start] - wind), 1.f);      // FIXME needs general win
    //float lineRho = (0.5 * 1.11225f * vBody * vBody) * 0.6f * (0.1f / 10000.f);
    //lineRho = __max(0.000001, lineRho);

    {
        FALCOR_PROFILE("pre");
        // Pre solve
        memcpy(&x_old[0], &x[0], sizeof(float3) * x.size());
        for (int i = 0; i < x.size(); i++)
        {
            //x_old[i] = x[i];

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

    float3 g_oldB = x[start];
    float3 g_oldL = x[start + 1];
    float3 g_oldR = x[start + 2];

    //float3 lineF = -glm::normalize(v[start] - wind) * lineRho;


    linesLeft.windAndTension_2(x, w, n, v, wind, _dT* _dT);
    linesRight.windAndTension_2(x, w, n, v, wind, _dT* _dT);
    tensionCarabinerLeft = linesLeft.tencileVector * 0.94f;
    tensionCarabinerRight = linesRight.tencileVector * 0.94f;

    //tensionCarabinerLeft = linesLeft.tensionDown(x, n) * 0.94f;
    //tensionCarabinerRight = linesRight.tensionDown(x, n) * 0.94f;

    //linesLeft.windAndTension(x, w, v, wind, _dT* _dT);
    //linesRight.windAndTension(x, w, v, wind, _dT* _dT);

    // Solve the body and carabiners
    x[start + 1] += w[start + 1] * tensionCarabinerLeft * _dT * _dT;
    x[start + 2] += w[start + 2] * tensionCarabinerRight * _dT * _dT;
    // drag on the body
    vBody = glm::length(v[start] - wind);
    float rho = 0.5 * 1.11225f * vBody * vBody;
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

    float3 solve_oldB = x[start];
    float3 solve_oldL = x[start + 1];
    float3 solve_oldR = x[start + 2];


    {
        FALCOR_PROFILE("wing constraints");
        // solve
        float dS = 0.8f;
        for (int k = 0; k < 3; k++)
        {
            for (int l = 0; l < 3; l++)
            {
                bool last = false;
                if (l == 2) last = true;
                linesLeft.solveUp_2(x, last);
                linesRight.solveUp_2(x, last);
            }


            for (int i = 3; i < constraints.size(); i++)
            {
                solveConstraint(i, dS);
            }
            //dS += 0.05f;

            x[start] = solve_oldB;          // maybe unnesesay I think I have caraqbibner weigths correct
            x[start + 1] = solve_oldL;
            x[start + 2] = solve_oldR;
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

        //OFFSET = (x[spanSize * chordSize]) + float3(0, 0.5, 0);
    }


    wingVold = wingV;
    wingV = float3(0, 0, 0);
    uint cnt = 0;
    for (int j = 10; j < 40; j++)
    {
        for (int i = 0; i < chordSize; i++)
        {
            wingV += v[j * chordSize + i];
            cnt++;
        }
    }
    wingV /= (float)cnt;
    wingA = (wingV - wingVold) / _dT;
    root_AG01 = glm::length(x[start] - x[23 * chordSize + 9]);


    if (glm::length(pilotDrag) > 1.f) pilotYaw = atan2(pilotDrag.x, pilotDrag.z);
}


void _gliderRuntime::solveConstraint(uint _i, float _step)
{
#define Cst constraints[_i]
#define idx0 Cst.idx_0
#define idx1 Cst.idx_1

    float Wsum = w[idx0] + w[idx1];
    float3 S = x[idx1] - x[idx0];
    float L = glm::length(S);
    //float L = (S.x * S.x + S.y * S.y + S.z * S.z) / Cst.l;   //l^2 centeres arounf 1.0f
    //float L = L1 * Cst.l;
    if (L == 0)
    {
        L = 0.001f;
        S.y += 0.001f;
    }
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


void _gliderRuntime::pack(rvB* ribbon, rvPacked* packed, int& count, bool& changed)
{
    count = 0;

    for (int s = 0; s < spanSize; s+=1)
    {
        for (int c = 0; c < chordSize; c++)
        {
                     setupVert(&ribbon[count], c == 0 ? 0 : 1, x[index(s, c)], 0.008f, 0);     count++;
        }
    }

    // span
    for (int c = 0; c < chordSize; c++)
    {
        //if ((c == 0) || (c == 3) || (c == 6) || (c == 9) || (c == 12))
        {
            for (int s = 0; s < spanSize; s++)
            {
                setupVert(&ribbon[count], s == 0 ? 0 : 1, x[index(s, c)], 0.006f);     count++;
            }
        }
        
    }

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
                //                setupVert(&ribbon[count], 0, x[index(s, c)], 0.001f, color);     count++;
                //                setupVert(&ribbon[count], 1, x[index(s, c)] + n[index(s, c)] * 0.3f, 0.004f, color);     count++;
                                //setupVert(&ribbon[count], 1, x[index(s, c)] + t[index(s, c)] * 20.f, 0.01f, 4);     count++;
            }
        }
    }

    // triangle
    uint start = spanSize * chordSize;
    setupVert(&ribbon[count], 0, x[start], 0.01f, 7);     count++;
    setupVert(&ribbon[count], 1, x[start + 1], 0.01f, 7);     count++;

    setupVert(&ribbon[count], 0, x[start], 0.01f, 7);     count++;
    setupVert(&ribbon[count], 1, x[start + 2], 0.01f, 7);     count++;

    setupVert(&ribbon[count], 0, x[start + 1], 0.01f, 7);     count++;
    setupVert(&ribbon[count], 1, x[start + 2], 0.01f, 7);     count++;


    setupVert(&ribbon[count], 0, x[start], 0.001f, 1);     count++;
    setupVert(&ribbon[count], 1, x[start] + pilotDrag * 0.05f, 0.1f, 1);     count++;

    //setupVert(&ribbon[count], 0, x[start] + float3(0, 3, 0), 0.001f, 1);     count++;
    //setupVert(&ribbon[count], 1, x[start] + float3(0, 3, 0) + lineDrag * 0.05f, 0.16f, 1);     count++;





    // constraints
    for (int i = 0; i < constraints.size(); i++)
    {
  //      setupVert(&ribbon[count], 0, x[constraints[i].idx_0], 0.004f, 7);     count++;
    //    setupVert(&ribbon[count], 1, x[constraints[i].idx_1], 0.004f, 7);     count++;
    }


    // Feedbacl
    for (int s = 0; s < spanSize; s += 1)
    {
 /*               setupVert(&ribbon[count], 0, x[index(s, 0)], 0.005f, 2);     count++;
                setupVert(&ribbon[count], 1, x[index(s, 0)] + cells[s].rel_wind * 0.2f, 0.016f, 2);     count++;

                setupVert(&ribbon[count], 0, x[index(s, 0)], 0.005f, 2);     count++;
                setupVert(&ribbon[count], 1, x[index(s, 0)] - cells[s].rel_wind * 0.3f, 0.016f, 2);     count++;

                setupVert(&ribbon[count], 0, x[index(s, 0)], 0.001f, 1);     count++;
                setupVert(&ribbon[count], 1, x[index(s, 0)] + pilotDrag * 0.05f, 0.016f, 1);     count++;

                setupVert(&ribbon[count], 0, x[index(s, 0)], 0.005f, 7);     count++;
                setupVert(&ribbon[count], 1, x[index(s, 0)] + cells[s].front * 3.3f, 0.016f, 7);     count++;

                setupVert(&ribbon[count], 0, x[index(s, 0)], 0.005f, 7);     count++;
                setupVert(&ribbon[count], 1, x[index(s, 0)] + cells[s].up * 3.3f, 0.016f, 7);     count++;
                
                */
        //        setupVert(&ribbon[count], 0, x[index(s, 12)], 0.005f, 1);     count++;
        //        setupVert(&ribbon[count], 1, x[index(s, 12)] + cells[s].v * 0.2f, 0.02f, 1);     count++;
    }


    //lines
    linesLeft.addRibbon(ribbon, x, count);
    linesRight.addRibbon(ribbon, x, count);

    for (int i = 0; i < count; i++)
    {
        packed[i] = ribbon[i].pack();
    }
    changed = true;

}

void _gliderRuntime::load()
{
}

void _gliderRuntime::save()
{
}

uint _gliderRuntime::index(uint _s, uint _c)
{
    return _s * chordSize + _c;
}


















#include <iostream>
#include <fstream>
void _shadowEdges::load()
{
    int edgeCnt = 0;
    int shadowEdge = 0;

    std::ifstream ifs;
    ifs.open("F:\\terrains\\switserland_Steg\\gis\\_export\\root4096.bil", std::ios::binary);

    if (ifs)
    {
        ifs.read((char*)height, 4096 * 4096 * 4);
        ifs.close();

        for (int y = 0; y < 4095; y++)
        {
            for (int x = 0; x < 4095; x++)
            {
                Nx[y][x] = (height[y][x] - height[y][x + 1]) / 9.765625f;    // 10 meter between pixels SHIT NOT TRUE
            }
        }


        float angle = 0.04f;  // about 10 degrees
        float a_min = -angle;
        float a_max = -angle - 0.05f;

        memset(edge, 0, 4096 * 4096);
        for (int y = 1; y < 4094; y++)
        {
            for (int x = 1; x < 4094; x++)
            {
                if (Nx[y][x] > a_min)
                {
                    edge[y][x] = (unsigned char)(128.f * saturate(Nx[y][x] - a_min));  // hill shade
                }


                if ((Nx[y][x] < a_min) && (Nx[y][x + 1] > a_max))
                {
                    edge[y][x] = 255;
                    edgeCnt++;


                    float H = height[y][x];
                    for (int j = x - 1; j > 0; j--)
                    {
                        H -= angle * 9.765625f;
                        if (H > height[y][j])
                        {
                            edge[y][j] = 0;
                            //shadowEdge++;
                            //break;
                        }
                        else break;
                    }
                }



                /*
                float H = height[y][x+1];
                for (int j = x + 2; j < 4095; j++)
                {
                    H += angle * 9.765625f;          // about 10 degrees
                    if (H > 1500) break;
                    if (H < height[y][j])
                    {
                        edge[y][x] /= 4;
                        shadowEdge++;
                        break;
                    }
                }
                */



                /*
                if (Nx[y][x] < -0.03)
                {
                    if ((Nx[y][x + 1] - Nx[y][x]) > 0.05f)
                    {
                        edge[y][x] = 255;

                        // SUNRISE - is this pixel in shadow when it can also cast
                        float H = height[y][x];
                        for (int j = x + 1; j < 4095; j++)
                        {
                            H -= Nx[y][x] * 9.765625f;
                            if (H < height[y][j])
                            {
                                edge[y][x] = 100;
                                break;
                            }
                        }

                        // sunrise how far can it cast a shadow if it all

                    }

                    if (edge[y][x] == 255)
                    {
                        // VERY EARLY MORENIGN SHASDOWN

                        float H = height[y][x];
                        for (int j = x + 1; j < 4095; j++)
                        {
                            //H -= Nx[y][x] * 9.765625f;
                            H += 0.03f * 9.765625f;          // about 10 degrees
                            if (H < height[y][j])
                            {
                                edge[y][x] = 50;
                                break;
                            }
                        }

                    }
                }
                */

            }
        }

        std::ofstream ofs;
        ofs.open("F:\\terrains\\switserland_Steg\\gis\\_export\\edge.raw", std::ios::binary);
        if (ofs)
        {
            ofs.write((char*)edge, 4096 * 4096);
            ofs.close();
        }

    }
}






float objectScale = 0.002f;  //0.002 for trees
float radiusScale = 2.0f;//  so biggest radius now objectScale / 2.0f;
float O = 16384.0f * objectScale * 0.5f;
float3 objectOffset = float3(O, O * 0.5f, O);

float static_Ao_depthScale = 0.3f;
float static_sunTilt = -0.2f;
float static_bDepth = 20.0f;
float static_bScale = 0.5f;





rvPacked rvB::pack()
{
    int x14 = ((int)((position.x + objectOffset.x) / objectScale)) & 0x3fff;
    int y16 = ((int)((position.y + objectOffset.y) / objectScale)) & 0xffff;
    int z14 = ((int)((position.z + objectOffset.z) / objectScale)) & 0x3fff;

    int anim8 = (int)floor(__min(1.f, __max(0.f, anim_blend)) * 255.f);

    int u7 = ((int)(uv.x * 127.f)) & 0x7f;
    int v15 = ((int)(uv.y * 255.f)) & 0x7fff;
    //int radius8 = (__min(255, (int)(radius  / radiusScale))) & 0xff;        // FIXME POW for better distribution
    //floar Rtemp = radius / radiusScale;
    int radius8 = (int)(pow(__min(1.0f, radius / radiusScale), 0.5f) * 255.f) & 0xff;        // square

    float up_yaw = atan2(bitangent.z, bitangent.x) + 3.1415926535897932;
    float up_pitch = atan2(bitangent.y, length(float2(bitangent.x, bitangent.z))) + 1.570796326794;
    int up_yaw9 = ((int)(up_yaw * 81.487)) & 0x1ff;
    int up_pitch8 = ((int)(up_pitch * 81.487)) & 0xff;

    float left_yaw = atan2(tangent.z, tangent.x) + 3.1415926535897932;
    float left_pitch = atan2(tangent.y, length(float2(tangent.x, tangent.z))) + 1.570796326794;
    int left_yaw9 = ((int)(left_yaw * 81.487)) & 0x1ff;
    int left_pitch8 = ((int)(left_pitch * 81.487)) & 0xff;
    {
        float plane, x, y, z;
        z = sin((left_yaw9 - 256) * 0.01227);
        x = cos((left_yaw9 - 256) * 0.01227);
        y = sin((left_pitch8 - 128) * 0.01227);
        plane = cos((left_pitch8 - 128) * 0.01227);
        float3 reconstruct = float3(plane * x, y, plane * z);
        if (dot(reconstruct, tangent) < 0.98)
        {
            bool bCm = true;
        }
    }


    float coneyaw = atan2(lightCone.z, lightCone.x) + 3.1415926535897932;
    float conepitch = atan2(lightCone.y, length(float2(lightCone.x, lightCone.z))) + 1.570796326794;
    int coneYaw9 = ((int)(coneyaw * 81.17)) & 0x1ff;
    int conePitch8 = ((int)(conepitch * 81.17)) & 0xff;
    int cone7 = (int)((lightCone.w + 1.f) * 63.5f);
    int depth8 = (int)(clamp(lightDepth, 0.f, 2.0f) * 127.5f);




    rvPacked p;

    p.a = ((type & 0x1) << 31) + (startBit << 30) + (x14 << 16) + y16;
    p.b = (z14 << 18) + ((material & 0x3ff) << 8) + anim8;
    p.c = (up_yaw9 << 23) + (up_pitch8 << 15) + v15;
    p.d = (left_yaw9 << 23) + (left_pitch8 << 15) + (u7 << 8) + radius8;
    p.e = (coneYaw9 << 23) + (conePitch8 << 15) + (cone7 << 8) + depth8;
    p.f = (ao << 24) + (shadow << 16) + (albedoScale << 8) + translucencyScale;
    return p;
}





std::string materialCache_plants::clean(const std::string _s)
{
    std::string str = _s;
    size_t start_pos = 0;
    while ((start_pos = str.find("\\", start_pos)) != std::string::npos) {
        str.replace(start_pos, 2, "/");
        start_pos += 1;
    }
    return str;
}

int materialCache_plants::find_insert_material(const std::string _path, const std::string _name)
{
    std::filesystem::path fullPath = _path + _name + ".vegetationMaterial";
    if (std::filesystem::exists(fullPath))
    {
        return find_insert_material(fullPath);
    }
    else
    {
        // Now we have to search, but use the first one we find
        std::string fullName = _name + ".vegetationMaterial";
        fullPath = terrafectorEditorMaterial::rootFolder + "/vegetationMaterials";
        for (const auto& entry : std::filesystem::recursive_directory_iterator(fullPath))
        {
            std::string subPath = clean(entry.path().string());
            if (subPath.find(fullName) != std::string::npos)
            {
                return find_insert_material(subPath);
            }
        }
    }

    fprintf(terrafectorSystem::_logfile, "error : vegetation material - %s does not exist\n", _name.c_str());
    return -1;
}


// force a new one? copy last and force a save ?
int materialCache_plants::find_insert_material(const std::filesystem::path _path)
{
    for (uint i = 0; i < materialVector.size(); i++)
    {
        if (materialVector[i].fullPath.compare(_path) == 0)
        {
            return i;
        }
    }

    // else add new
    if (std::filesystem::exists(_path))
    {
        uint materialIndex = (uint)materialVector.size();
        _plantMaterial& material = materialVector.emplace_back();
        material.import(_path);
        fprintf(terrafectorSystem::_logfile, "add vegeatation material[%d] - %s\n", materialIndex, _path.filename().string().c_str());
        return materialIndex;
    }

    return -1;
}



int materialCache_plants::find_insert_texture(const std::filesystem::path _path, bool isSRGB)
{
    modified = true;

    for (uint i = 0; i < textureVector.size(); i++)
    {
        if (textureVector[i]->getSourcePath().compare(_path) == 0)
        {
            return i;
        }
    }

    Texture::SharedPtr tex = Texture::createFromFile(_path.string(), true, isSRGB);
    if (tex)
    {
        tex->setSourcePath(_path);
        tex->setName(_path.filename().string());
        textureVector.emplace_back(tex);

        float compression = 4.0f;
        if (isSRGB) compression = 4.0f;

        texMb += (float)(tex->getWidth() * tex->getHeight() * 4.0f * 1.333f) / 1024.0f / 1024.0f / compression;	// for 4:1 compression + MIPS

        fprintf(terrafectorSystem::_logfile, "%s\n", tex->getName().c_str());

        return (uint)(textureVector.size() - 1);
    }
    else
    {
        fprintf(terrafectorSystem::_logfile, "failed %s \n", _path.string().c_str());
        return -1;
    }


}


Texture::SharedPtr materialCache_plants::getDisplayTexture()
{
    if (dispTexIndex >= 0) {
        return textureVector.at(dispTexIndex);
    }
    return nullptr;
}



void materialCache_plants::setTextures(ShaderVar& _var)
{
    for (size_t i = 0; i < textureVector.size(); i++)
    {
        _var[i] = textureVector[i];
    }

    modified = false;
}


// FIXME could also just do the individual one
//terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials = Buffer::createStructured(sizeof(TF_material), 2048); // FIXME hardcoded
void materialCache_plants::rebuildStructuredBuffer()
{
    size_t offset = 0;
    for (auto& mat : materialVector)
    {
        sb_vegetation_Materials->setBlob(&mat._constData, offset, sizeof(sprite_material));
        offset += sizeof(sprite_material);
    }
}






void materialCache_plants::renderGui(Gui* mpGui, Gui::Window& _window)
{
    auto& style = ImGui::GetStyle();
    ImGuiStyle oldStyle = style;
    float width = ImGui::GetWindowWidth();
    int numColumns = __max(2, (int)floor(width / 140));

    ImGui::PushItemWidth(128);

    if (ImGui::Button("import")) {
        std::filesystem::path path = terrainManager::lastfile.vegMaterial;
        FileDialogFilterVec filters = { {"vegetationMaterial"} };
        if (openFileDialog(filters, path))
        {
            find_insert_material(path);
            terrainManager::lastfile.vegMaterial = path.string();
        }
    }

    if (ImGui::Button("rebuild")) {
        rebuildStructuredBuffer();
    }

    ImVec2 rootPos = ImGui::GetCursorPos();

    struct sortDisplay
    {
        bool operator < (const sortDisplay& str) const
        {
            return (name < str.name);
        }
        std::string name;
        int index;
    };

    std::vector<sortDisplay> displaySortMap;

    sortDisplay S;
    int cnt = 0;
    for (auto& material : materialVector)
    {
        S.name = material.fullPath.string().substr(terrafectorEditorMaterial::rootFolder.length() - 1);
        S.index = cnt;
        displaySortMap.push_back(S);
        cnt++;
    }
    std::sort(displaySortMap.begin(), displaySortMap.end());


    {
        std::string path = "";

        int subCount = 0;
        for (cnt = 0; cnt < materialVector.size(); cnt++)
        {
            std::string  thisPath = "other";
            if (displaySortMap[cnt].name.find("vegetationMaterials") != std::string::npos)
            {
                thisPath = displaySortMap[cnt].name.substr(21, displaySortMap[cnt].name.find_last_of("\\/") - 22);
            }
            if (thisPath != path) {
                ImGui::PushFont(mpGui->getFont("roboto_32"));
                ImGui::NewLine();
                ImGui::Text(thisPath.c_str());
                ImGui::PopFont();
                path = thisPath;
                subCount = 0;
                rootPos = ImGui::GetCursorPos();
            }
            _plantMaterial& material = materialVector[displaySortMap[cnt].index];

            ImGui::PushID(777 + cnt);
            {
                if (ImGui::Button(material.displayName.c_str()))
                {
                    selectedMaterial = displaySortMap[cnt].index;
                }
                /*
                uint x = subCount % numColumns;
                uint y = (int)floor(subCount / numColumns);
                ImGui::SetCursorPos(ImVec2(x * 140 + rootPos.x, y * 160 + rootPos.y));

                int size = material.displayName.size() - 19;
                if (size >= 0)
                {
                    ImGui::Text((material.displayName.substr(0, 19) + "..").c_str());
                }
                else
                {
                    ImGui::Text(material.displayName.c_str());
                }

                ImGui::SetCursorPos(ImVec2(x * 140 + rootPos.x, y * 160 + 20 + rootPos.y));
                if (material.thumbnail) {
                    if (_window.imageButton("testImage", material.thumbnail, float2(128, 128)))
                    {
                        selectedMaterial = displaySortMap[cnt].index;
                    }
                }
                else
                {
                    style.Colors[ImGuiCol_Button] = ImVec4(material._constData.albedoScale.x, material._constData.albedoScale.y, material._constData.albedoScale.z, 1.f);
                    if (ImGui::Button("##test", ImVec2(128, 128)))
                    {
                        selectedMaterial = displaySortMap[cnt].index;
                    }
                }
                style.Colors[ImGuiCol_Button] = ImVec4(0.01f, 0.01f, 0.01f, 0.7f);

                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::Selectable("Explorer")) {
                        std::string cmdExp = "explorer " + material.fullPath.string();  //
                        cmdExp = cmdExp.substr(0, cmdExp.find_last_of("\\/") + 1);
                        replaceAll(cmdExp, "//", "/");
                        replaceAll(cmdExp, "/", "\\");
                        system(cmdExp.c_str());
                    }

                    ImGui::EndPopup();
                }

                if (material._constData.materialType == 1)
                {
                    ImGui::SetCursorPos(ImVec2(x * 140 + rootPos.x, y * 160 + 20 + 10 + rootPos.y));
                    ImGui::Selectable("MULTI");
                    ImGui::LabelText("m1", "MULTI");
                }

                */
            }
            ImGui::PopID();
            subCount++;
        }
    }




    style = oldStyle;
}







void materialCache_plants::renderGuiTextures(Gui* mpGui, Gui::Window& _window)
{
    auto& style = ImGui::GetStyle();
    ImGuiStyle oldStyle = style;
    float width = ImGui::GetWindowWidth();
    int numColumns = __max(2, (int)floor(width / 140));


    char text[1024];
    ImGui::PushFont(mpGui->getFont("roboto_26"));
    ImGui::Text("Tex [%d]   %3.1fMb", (int)textureVector.size(), texMb);
    ImGui::PopFont();


    ImGui::PushFont(mpGui->getFont("roboto_20"));
    {
        for (uint i = 0; i < textureVector.size(); i++)
        {
            ImGui::NewLine();
            ImGui::SameLine(40);

            Texture* pT = textureVector[i].get();
            sprintf(text, "%s##%d", pT->getName().c_str(), i);

            if (ImGui::Selectable(text)) {}

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%d  :  %s\n%d x %d", i, pT->getSourcePath().c_str(), pT->getWidth(), pT->getHeight());
                _plantMaterial::static_materials_veg.dispTexIndex = i;
            }

            //if (ImGui::BeginPopup(text)) {
            if (ImGui::BeginPopupContextItem())
            {
                std::string cmdExp = "explorer " + pT->getSourcePath().string();
                std::string cmdPs = "\"C:\\Program Files\\Adobe\\Adobe Photoshop 2022\\Photoshop.exe\" " + pT->getSourcePath().string();
                if (ImGui::Selectable("Explorer")) { system(cmdExp.c_str()); }
                if (ImGui::Selectable("Photoshop")) { system(cmdPs.c_str()); }
                ImGui::Separator();

                /*
                std::string texPath = pT->getSourcePath().string();
                texPath = texPath.substr(terrafectorEditorMaterial::rootFolder.size());
                int matCnt = 0;
                for (auto material : materialVector)
                {
                    for (int j = 0; j < 9; j++)
                    {
                        if (material.textureNames[j].size() > 0)
                        {
                            if (texPath.find(material.textureNames[j]) != std::string::npos)
                            {
                                if (ImGui::Selectable(material.displayName.c_str())) { selectedMaterial = matCnt; }
                            }
                        }
                    }
                    matCnt++;
                }
                */
                ImGui::EndPopup();
            }

        }
    }
    ImGui::PopFont();

    style = oldStyle;
}



bool materialCache_plants::renderGuiSelect(Gui* mpGui)
{
    if (selectedMaterial >= 0 && selectedMaterial < materialVector.size())
    {
        materialVector[selectedMaterial].renderGui(mpGui);
        return true;
    }
    else
    {
        ImGui::Text("default material");
        static _plantMaterial M;
        M.renderGui(mpGui);
    }

    return false;
}












void _plantMaterial::renderGui(Gui* _gui)
{
    ImGui::PushFont(_gui->getFont("roboto_20"));
    {

        ImGui::Text(displayName.c_str());

        ImGui::SetNextItemWidth(100);
        if (ImGui::Button("load")) { import(); }

        ImGui::SameLine(0, 30);
        ImGui::SetNextItemWidth(100);
        if (ImGui::Button("save")) { eXport(); }

        ImGui::PushID(9990);
        if (ImGui::Selectable(albedoName.c_str())) { loadTexture(0); }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("ALBEDO");
        }
        ImGui::PopID();


        ImGui::PushID(9991);
        if (ImGui::Selectable(alphaName.c_str())) { loadTexture(1); }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("ALPHA");
        }
        ImGui::PopID();


        ImGui::PushID(9992);
        if (ImGui::Selectable(translucencyName.c_str())) { loadTexture(2); }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("TRANSLUCENCY");
        }
        ImGui::PopID();


        ImGui::PushID(9993);
        if (ImGui::Selectable(normalName.c_str())) { loadTexture(3); }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("NORMAL");
        }
        ImGui::PopID();

        ImGui::DragFloat("Translucency", &_constData.translucency, 0.01f, 0, 1);
        ImGui::DragFloat("alphaPow", &_constData.alphaPow, 0.01f, 0.1f, 1);

        ImGui::NewLine();
        ImGui::Text("%d rgb", _constData.albedoTexture);
        ImGui::Text("%d a", _constData.alphaTexture);
        ImGui::Text("%d t", _constData.translucencyTexture);
        ImGui::Text("%d N", _constData.normalTexture);
    }
    ImGui::PopFont();
}


void _plantMaterial::import(std::filesystem::path _path, bool _replacePath)
{
    std::ifstream is(_path);
    if (is.fail()) {
        displayName = "failed to load";
        fullPath = _path;
    }
    else
    {
        cereal::JSONInputArchive archive(is);
        serialize(archive, 100);

        if (_replacePath) fullPath = _path;
        displayName = _path.filename().string();
        reloadTextures();
        isModified = false;
    }
}
void _plantMaterial::import(bool _replacePath)
{
    std::filesystem::path path = terrainManager::lastfile.vegMaterial;
    FileDialogFilterVec filters = { {"vegetationMaterial"} };
    if (openFileDialog(filters, path))
    {
import(path, _replacePath);
        terrainManager::lastfile.vegMaterial = path.string();
    }
}
void _plantMaterial::save()
{
    std::ofstream os(fullPath);
    cereal::JSONOutputArchive archive(os);
    serialize(archive, 100);
}
void _plantMaterial::eXport(std::filesystem::path _path)
{
    std::ofstream os(_path);
    cereal::JSONOutputArchive archive(os);
    serialize(archive, 100);
    isModified = false;
}
void _plantMaterial::eXport()
{
    std::filesystem::path path = terrainManager::lastfile.vegMaterial;
    FileDialogFilterVec filters = { {"vegetationMaterial"} };
    if (saveFileDialog(filters, path))
    {
        eXport(path);
        terrainManager::lastfile.vegMaterial = path.string();
    }
}
void _plantMaterial::reloadTextures()
{
    _constData.albedoTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + albedoPath, true);
    _constData.alphaTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + alphaPath, true);
    _constData.normalTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + normalPath, false);
    _constData.translucencyTexture = _plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + translucencyPath, false);
}



void replaceAllVEG(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

void _plantMaterial::loadTexture(int idx)
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"png"}, {"jpg"}, {"tga"}, {"exr"}, {"dds"}, {"tif"} }; //??? only DDS
    if (openFileDialog(filters, path))
    {
        std::string P = path.string();
        replaceAllVEG(P, "\\", "/");
        if (P.find(terrafectorEditorMaterial::rootFolder) == 0) {
            std::string relative = P.substr(terrafectorEditorMaterial::rootFolder.length());
            switch (idx)
            {
            case 0:
                albedoPath = relative;
                albedoName = path.filename().string();
                _constData.albedoTexture = static_materials_veg.find_insert_texture(P, true);
                break;
            case 1:
                alphaPath = relative;
                alphaName = path.filename().string();
                _constData.alphaTexture = static_materials_veg.find_insert_texture(P, false);
                break;
            case 2:
                translucencyPath = relative;
                translucencyName = path.filename().string();
                _constData.translucencyTexture = static_materials_veg.find_insert_texture(P, false);
                break;
            case 3:
                normalPath = relative;
                normalName = path.filename().string();
                _constData.normalTexture = static_materials_veg.find_insert_texture(P, false);
                break;
            }
        }
    }
}



















void _leaf::load()
{
    std::filesystem::path path = terrainManager::lastfile.leaves;
    FileDialogFilterVec filters = { {"leaf"} };
    if (openFileDialog(filters, path))
    {
        std::ifstream is(path.string());
        cereal::JSONInputArchive archive(is);
        serialize(archive, _LEAF_VERSION);
        terrainManager::lastfile.leaves = path.string();
    }
}

void _leaf::save()
{
    std::filesystem::path path = terrainManager::lastfile.leaves;
    FileDialogFilterVec filters = { {"leaf"} };
    if (saveFileDialog(filters, path))
    {
        std::ofstream os(path.string());
        cereal::JSONOutputArchive archive(os);
        serialize(archive, _LEAF_VERSION);
        terrainManager::lastfile.leaves = path.string();
    }
}

void _leaf::reloadMaterials()
{
    leaf_Material.index = _plantMaterial::static_materials_veg.find_insert_material(leaf_Material.name);
}

void _leaf::loadLeafMaterial()
{
    std::filesystem::path path = terrainManager::lastfile.vegMaterial;
    FileDialogFilterVec filters = { {"vegetationMaterial"} };
    if (openFileDialog(filters, path))
    {
        leaf_Material.index = _plantMaterial::static_materials_veg.find_insert_material(path);
        leaf_Material.name = path.string();       // FIXME dont liek this,. should be relative
        leaf_Material.displayname = path.filename().string().substr(0, path.filename().string().length() - 19);
        terrainManager::lastfile.vegMaterial = path.string();
    }
}


void _leaf::renderGui(Gui* _gui)
{
    ImGui::PushFont(_gui->getFont("roboto_20_bold"));
    {
        ImGui::Text("Leaf : ");
        //ImGui::SameLine(0, 10);
        //ImGui::Text(filename.c_str()); fixme add a filename and load and save
    }
    ImGui::PopFont();
    ImGui::Text("[%d]v", ribbonLength);

    if (ImGui::Button("Load##leaf"))
    {
        load();
        changed = true;
    }

    ImGui::SameLine(0, 20);
    if (ImGui::Button("Save##leaf"))
    {
        save();
    }

    ImGui::Text("lod");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(170);
    if (ImGui::SliderInt("##lod", &lod, 0, 3)) changed = true;


    ImGui::NewLine();

    ImGui::Text("stem");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##stemL", &stem_length.x, 1.f, 0, 100, "%2.fmm")) changed = true;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##stemR", &stem_length.y, 0.1f, 0, 1, "%1.2f rnd")) changed = true;


    if (stem_length.x > 0)
    {
        ImGui::NewLine();
        ImGui::Text("width");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##stemW", &stem_width, 1.f, 0, 100, "%2.fmm")) changed = true;


        ImGui::Text("stem crv");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##stemcrvL", &stem_curve.x, 0.01f, -1.5, 1.5, "%2.2f")) changed = true;
        ImGui::SameLine(0, 10);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##stemcrvR", &stem_curve.y, 0.1f, 0, 1, "%1.2f rnd")) changed = true;

        if (ImGui::Checkbox("intergrate stem", &integrate_stem)) changed = true;
    }


    ImGui::NewLine();
    ImGui::Text("Leaf");
    if (ImGui::Checkbox("face camera", &cameraFacing)) changed = true;

    ImGui::Text("leaf L");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##leafL", &leaf_length.x, 1.f, 0, 1000, "%2.fmm")) changed = true;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##leafLR", &leaf_length.y, 0.1f, 0, 1, "%1.2f rnd")) changed = true;

    ImGui::Text("leaf W");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##leafW", &leaf_width.x, 1.f, 0, 1000, "%2.fmm")) changed = true;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##leafWR", &leaf_width.y, 0.1f, 0, 1, "%1.2f rnd")) changed = true;

    ImGui::Text("offset");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##offsetW", &width_offset.x, 0.01f, 0.3f, 3, "%1.2fmm")) changed = true;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##offsetR", &width_offset.y, 0.1f, 0, 1, "%1.2f rnd")) changed = true;

    ImGui::Text("s2l crv");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##s2lcrvL", &stemtoleaf_curve.x, 0.01f, -1, 1, "%2.2f")) changed = true;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##s2lcrvR", &stemtoleaf_curve.y, 0.1f, 0, 1, "%1.2f rnd")) changed = true;

    ImGui::Text("leaf crv");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##leafcrvL", &leaf_curve.x, 0.01f, -3, 3, "%2.2f")) changed = true;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##leafcrvR", &leaf_curve.y, 0.1f, 0, 1, "%1.2f rnd")) changed = true;

    ImGui::Text("leaf twist");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##leaftwistL", &leaf_twist.x, 0.01f, -1, 1, "%2.2f")) changed = true;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##leaftwistR", &leaf_twist.y, 0.1f, 0, 1, "%1.2f rnd")) changed = true;

    ImGui::Text("verts");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragInt("##minVerts", &minVerts, 1, 2, 6)) changed = true;
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragInt("##maxVerts", &maxVerts, 1, 4, 16)) changed = true;

    ImGui::Text("gravity");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##gravitrophy", &gravitrophy, 0.01f, -3, 3)) changed = true;



    ImGui::NewLine();
    if (ImGui::Button(leaf_Material.displayname.c_str(), ImVec2(200, 0)))
    {
        loadLeafMaterial();
    }
    TOOLTIP(leaf_Material.displayname.c_str());
    if (ImGui::DragFloat("albedo", &albedoScale, 0.01f, 0, 2, "%2.2f")) changed = true;
    if (ImGui::DragFloat("albedo new", &albedoScaleNew, 0.01f, 0, 2, "%2.2f")) changed = true;
    if (ImGui::DragFloat("translucency", &translucencyScale, 0.01f, 0, 2, "%2.2f")) changed = true;
    if (ImGui::DragFloat("translucency new", &translucencyScaleNew, 0.01f, 0, 2, "%2.2f")) changed = true;

    /*
    if (ImGui::Selectable(textureName.c_str()))
    {
        loadTexture();
    }
    ImGui::Text("material index - %d", material);
    */




}

void _leaf::buildLeaf(float _age, float _lodPixelsize, int _seed, glm::mat4 vertex, glm::vec3 _pos, glm::vec3 _twigAxis, int overrideLod)
{
    std::mt19937 generator(_seed);
    std::uniform_real_distribution<> distribution(-1.f, 1.f);

    maxDistanceFromStem = 0;
    float3 stemPos = _pos;


    if (overrideLod >= 0) { lod = overrideLod; }
    _age = pow(_age, 0.5f);
    float minWidth = 0;
    bool rotate = cameraFacing;
    float testAngle = 0.1f;
    int stemCNT = 8;
    int leafCNT = 16;

    switch (lod) {
    case 0:
        minWidth = leaf_width.x * 0.4f;
        rotate = true;
        testAngle = 0.2f;
        stemCNT = 4;
        leafCNT = minVerts;
        break;
    case 1:
        minWidth = leaf_width.x * 0.2f;
        testAngle = 0.1f;
        stemCNT = 4;
        leafCNT = minVerts;
        break;
    case 2:
        minWidth = leaf_width.x * 0.02f;
        testAngle = 0.05f;
        stemCNT = 4;
        leafCNT = (maxVerts + minVerts) >> 1;
        break;
    case 3:
        minWidth = 0;
        testAngle = 0.02f;
        stemCNT = 4;
        leafCNT = maxVerts;
        break;
    }

    // Lets go fixed steps, and then skip some  - just enough

    int stemVisibleCount = 0;
    float stemStep = 1.0f / (float)stemCNT;
    float leafStep = 1.0f / (float)leafCNT;
    float sumAngle = 1000;  // just large
    float sumWidth = 0; //??? about this one

    // stem
    // -----------------------------------------------------------------------------------------------------------
    if (stem_length.x > 0)
    {
        for (int i = 0; i <= stemCNT; i++)
        {
            float stemCurve = (stem_curve.x + (distribution(generator) * stem_curve.y)) * stemStep;
            float s_Length = (stem_length.x * (1.f + distribution(generator) * stem_length.y)) * stemStep * 0.001f * _age;
            float t = (float)i * stemStep;

            ribbon[ribbonLength].type = integrate_stem ? rotate : 0;
            ribbon[ribbonLength].startBit = i > 0;
            ribbon[ribbonLength].position = _pos;
            ribbon[ribbonLength].radius = stem_width * .001f * 0.5f * _age;
            ribbon[ribbonLength].bitangent = vertex[2];
            ribbon[ribbonLength].tangent = vertex[0];
            //ribbon[ribbonLength].material = integrate_stem ? _mat : _matStem;
            ribbon[ribbonLength].material = 0;
            ribbon[ribbonLength].uv = float2(1.f, t);
            if (integrate_stem)
            {
                ribbon[ribbonLength].uv.x = stem_width / leaf_width.x;
                ribbon[ribbonLength].albedoScale = (unsigned char)(glm::lerp(albedoScaleNew, albedoScale, _age) * 127.f);
                ribbon[ribbonLength].translucencyScale = (unsigned char)(glm::lerp(translucencyScaleNew, translucencyScale, _age) * 127.f);
            }

            if ((sumAngle > testAngle) && ((stem_width * .001f) > minWidth))
            {
                ribbonLength++;
                sumAngle = 0;
            }

            if (i < (stemCNT - 1))
            {
                glm::mat4 crv = glm::rotate(glm::mat4(1.0), stemCurve, (glm::vec3)vertex[0]);
                vertex = crv * vertex;
                _pos += (glm::vec3)vertex[2] * s_Length;
                sumAngle += abs(stemCurve);
            }


        }
    }



    // now do the bend between the stalk and the leaf
    // -----------------------------------------------------------------------------------------------------------
    float Curve = stemtoleaf_curve.x * (1.f + distribution(generator) * stemtoleaf_curve.y);
    glm::mat4 crv = glm::rotate(glm::mat4(1.0), Curve, (glm::vec3)vertex[0]);
    vertex = crv * vertex;


    // leaf
    // -----------------------------------------------------------------------------------------------------------
    bool firstleafVertex = true;
    sumAngle = 1000;  // just large
    sumWidth = 0;

    float l_L = (leaf_length.x * (1.f + distribution(generator) * leaf_length.y)) * leafStep * 0.001f * _age;
    float l_W = (leaf_width.x * (1.f + distribution(generator) * leaf_width.y)) * 0.001f * _age;

    //float du[5] = { 0.1f, 0.8f, 1.f, 0.7f, 0.1f };
    float l_curve = leaf_curve.x * leafStep;
    float l_twist = leaf_twist.x * leafStep;
    for (int j = 0; j <= leafCNT; j++)
    {
        float t = (float)j * leafStep;
        float du = sin(pow(t, width_offset.x) * 3.1415f) + width_offset.y;
        if (du > 1) du = 1;
        if (leafCNT < 3) du = 1;

        ribbon[ribbonLength].type = !rotate;
        ribbon[ribbonLength].startBit = !firstleafVertex;
        ribbon[ribbonLength].position = _pos;
        ribbon[ribbonLength].radius = l_W * 0.5f * du;
        if (rotate) ribbon[ribbonLength].radius *= 0.5f;
        ribbon[ribbonLength].bitangent = vertex[2];
        ribbon[ribbonLength].tangent = vertex[0];
        ribbon[ribbonLength].material = leaf_Material.index;
        ribbon[ribbonLength].albedoScale = (unsigned char)(glm::lerp(albedoScaleNew, albedoScale, _age) * 127.f);
        ribbon[ribbonLength].translucencyScale = (unsigned char)(glm::lerp(translucencyScaleNew, translucencyScale, _age) * 127.f);
        ribbon[ribbonLength].uv = float2(du, 1.f + t);

        //if ((sumAngle > testAngle) && ((l_W * du) > minWidth))
        {
            ribbonLength++;
            sumAngle = 0;
            firstleafVertex = false;
        }

        if (integrate_stem && j == 0 && (ribbonLength > 0))
        {
            ribbonLength--; // discard terh first one
        }


        l_curve += ((distribution(generator) * leaf_curve.y)) * leafStep;
        l_twist += ((distribution(generator) * leaf_twist.y)) * leafStep;

        // test for through ground - bend upwards.
        float3 groundTest = _pos + (glm::vec3)vertex[2] * l_L;
        if (groundTest.y < (2 * l_L) && vertex[2][1] < 0)
        {
            l_curve += 2.0f * leafStep * vertex[2][1];
        }
        float down = (l_W / 2 - _pos.y);
        if (down > 0)
        {
            _pos.y += pow(down, 2);
        }

        // Stem intersection test and bend away
        float dotStem = abs(glm::dot((glm::vec3)vertex[2], _twigAxis));
        //if (vertex[2][1] > 0.5)
        {
            //glm::vec Rs = glm::cross((glm::vec3)vertex[2], _twigAxis);
            l_curve += 0.5 * leafStep * pow(dotStem, 15);
        }

        glm::vec3 D = vertex[2];

        // Gravitrophy ---------------------------------------------------------------
        glm::vec3 graviR = glm::normalize(glm::cross(D, glm::vec3(0, 1, 0)));
        glm::mat4 graviM = glm::rotate(glm::mat4(1.0), gravitrophy * leafStep, graviR);
        vertex = graviM * vertex;

        glm::vec3 L = vertex[0];
        glm::mat4 crv = glm::rotate(glm::mat4(1.0), l_curve, L);
        vertex = crv * vertex;

        glm::mat4 twist = glm::rotate(glm::mat4(1.0), l_twist, D);
        vertex = twist * vertex;

        sumAngle += abs(l_curve);

        _pos += D * l_L;

        float3 lateralPos = _pos - (glm::dot(_pos - stemPos, _twigAxis) * _twigAxis);
        float dst = glm::length(lateralPos - stemPos);
        if (dst > maxDistanceFromStem)maxDistanceFromStem = dst;
    }
    //ribbonLength++;
}





void _twigLod::load()
{
    std::filesystem::path path = terrainManager::lastfile.twig;
    FileDialogFilterVec filters = { {"twig_LOD"} };
    if (openFileDialog(filters, path))
    {
        std::ifstream is(path.string());
        cereal::JSONInputArchive archive(is);
        serialize(archive, 100);
    }
}

void _twigLod::save()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"twig_LOD"} };
    if (saveFileDialog(filters, path))
    {
        std::ofstream os(path.string());
        cereal::JSONOutputArchive archive(os);
        serialize(archive, _TWIG_VERSION);
    }
}




void _twig::reloadMaterials()
{
    stem_Material.index = _plantMaterial::static_materials_veg.find_insert_material(stem_Material.name);
    sprite_Material.index = _plantMaterial::static_materials_veg.find_insert_material(sprite_Material.name);
    /*
    for (int m = 0; m < materialList.size(); m++)
    {
        materialList[m].index = _plantMaterial::static_materials_veg.find_insert_material(materialList[m].name);
    }

    for (int m = 0; m < leafmaterialList.size(); m++)
    {
        leafmaterialList[m].index = _plantMaterial::static_materials_veg.find_insert_material(leafmaterialList[m].name);
    }
    */
}


void _twig::load()
{
    std::filesystem::path path = terrainManager::lastfile.twig;
    FileDialogFilterVec filters = { {"twig"} };
    if (openFileDialog(filters, path))
    {
        std::ifstream is(path.string());
        cereal::JSONInputArchive archive(is);
        serialize(archive, _TWIG_VERSION);
        changed = true;
        filename = path.filename().string();
        filepath = path.string();
        terrainManager::lastfile.twig = filepath;
    }
}

void _twig::save()
{
    std::ofstream os(filepath);
    cereal::JSONOutputArchive archive(os);
    serialize(archive, _TWIG_VERSION);
    changedForSaving = false;
}


void _twig::saveas()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"twig"} };
    if (saveFileDialog(filters, path))
    {
        std::ofstream os(path.string());
        cereal::JSONOutputArchive archive(os);
        serialize(archive, _TWIG_VERSION);

        filename = path.filename().string();
        filepath = path.string();
        terrainManager::lastfile.twig = filepath;
    }
}


void _twig::loadStemMaterial()
{
    std::filesystem::path path = terrainManager::lastfile.vegMaterial;
    FileDialogFilterVec filters = { {"vegetationMaterial"} };
    if (openFileDialog(filters, path))
    {
        stem_Material.index = _plantMaterial::static_materials_veg.find_insert_material(path);
        stem_Material.name = path.string();       // FIXME dont liek this,. should be relative
        stem_Material.displayname = path.filename().string().substr(0, path.filename().string().length() - 19);
        terrainManager::lastfile.vegMaterial = path.string();
    }
}





void _twig::renderGui(Gui* _gui)
{



    float W = ImGui::GetWindowWidth() / 3.f;
    ImGui::PushID(7701);
    {
        ImGui::PushFont(_gui->getFont("roboto_20_bold"));
        {
            ImGui::Text("Twig : ");
            ImGui::SameLine(0, 10);
            ImGui::Text(filename.c_str());

        }
        ImGui::PopFont();

        //ImGui::SameLine(0, 30);
        ImGui::Text("(%3.2f, %3.2f)m", extents.x * 2, extents.y);

        ImGui::SameLine(0, 30);
        ImGui::Text("[%d]v", ribbonLength);

        //ImGui::SameLine(0, 50);
        if (ImGui::Button("Load")) load();

        if (changedForSaving) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.0f, 0.5f));
            ImGui::SameLine(0, 20);
            if (ImGui::Button("Save"))  save();
            ImGui::PopStyleColor();
        }


        ImGui::SameLine(0, 20);
        if (ImGui::Button("Save as"))  saveas();

        ImGui::Text("lod");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(170);
        if (ImGui::SliderInt("##twiglod", &lod, 0, 3)) changed = true;



        ImGui::NewLine();


        ImGui::Text("age");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragInt("##start", &startSegment, 1, 0, 15)) changed = true;
        TOOLTIP("first segment with leaves");

        ImGui::SetNextItemWidth(80);
        ImGui::SameLine(0, 10);
        if (ImGui::DragFloat("##age", &age, 0.1f, 1.2f, 20, "%2.1f")) changed = true;
        startSegment = __min(startSegment, (int)floor(age));

        ImGui::Text("length");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##stemwL", &stem_length.x, 1.f, 0, 500, "%2.fmm")) changed = true;
        TOOLTIP("segment length in mm");
        ImGui::SameLine(0, 10);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##stewmR", &stem_length.y, 0.02f, 0, 1, "%1.2f %")) changed = true;
        TOOLTIP("randomness");

        ImGui::Text("width");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##width", &stem_width, 1.f, 0, 100, "%2.fmm")) changed = true;

        ImGui::Text("curve");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##stemCrvL", &stem_curve.x, 0.01f, 0, 2, "%2.2f")) changed = true;
        TOOLTIP("total curvature");
        ImGui::SameLine(0, 10);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##stemCrvR", &stem_curve.y, 0.02f, 0, 1, "%1.2f %")) changed = true;


        ImGui::Text("gravity");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##stemGraviPhoto", &stem_phototropism, 0.01f, -1, 1, "%2.2f")) changed = true;
        TOOLTIP("negative - add gravity\n positive add phototropism");



        if (ImGui::Button(stem_Material.displayname.c_str(), ImVec2(W, 0)))
        {
            loadStemMaterial();
        }
        TOOLTIP(stem_Material.displayname.c_str());

        if (ImGui::Checkbox("show stem", &has_stem)) changed = true;



        ImGui::NewLine();
        ImGui::Text("# leaves");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragInt("##numLeaves", &numLeaves, 1, 1, 15)) changed = true;

        ImGui::Text("stalk angle");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##stem_stalkL", &stem_stalk.x, 0.01f, -2, 2, "%2.2f")) changed = true;
        TOOLTIP("root angle\nradians\0 is in line with the stem, 1.5 is away, larger droops");
        ImGui::SameLine(0, 10);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##stem_stalkR", &stem_stalk.y, 0.01f, -2, 2, "%1.2f rnd")) changed = true;
        TOOLTIP("change with age\n the amount that this angle changes with leaf age");

        ImGui::Text("rotation");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##leafRotation", &leafRotation, 0.01f, 0, 2, "%3.2f")) changed = true;
        TOOLTIP("spin around the axis from one leaf cluster to the next");

        ImGui::Text("age factor");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##leaf_age_power", &leaf_age_power, 0.1f, 0.1f, 10, "%3.2f")) changed = true;
        TOOLTIP("scale tte age along the twig, 0 is newest growth");

        if (ImGui::Checkbox("twistAway", &twistAway)) changed = true;
        TOOLTIP("for single leaves\ndoes the stem twist away from the angle of the leaf");

        ImGui::SameLine(0, 10);
        if (ImGui::Checkbox("tipleaves", &tipleaves));
        TOOLTIP("is there a special cluster of leaves right at the tip");



        /*
        ImGui::NewLine();
        ImGui::Text("leaf materials");
        if (ImGui::Button("add leafmaterial"))
        {
            leafmaterialList.emplace_back();
        }
        for (int m = 0; m < leafmaterialList.size(); m++)
        {
            ImGui::PushID(7800 + m);
            {
                ImGui::Text("%d", leafmaterialList[m].index);
                ImGui::SameLine(0, 10);
                ImGui::SetNextItemWidth(200);
                if (ImGui::Button(leafmaterialList[m].displayname.c_str()))
                {
                    std::filesystem::path path = terrainManager::lastfile.vegMaterial;
                    FileDialogFilterVec filters = { {"vegetationMaterial"} };
                    if (openFileDialog(filters, path))
                    {
                        leafmaterialList[m].index = _plantMaterial::static_materials_veg.find_insert_material(path);
                        leafmaterialList[m].name = path.string();       // FIXME dont liek this,. should be relative
                        leafmaterialList[m].displayname = path.filename().string();
                        terrainManager::lastfile.vegMaterial = path.string();
                    }
                }


            }
            ImGui::PopID();
        }
        */


        ImGui::NewLine();
        ImGui::Text("lodding");

        ImGui::NewLine();
        if (ImGui::Button("bake to material")) { bakeToMaterial(); }
    }
    ImGui::PopID();

    ImGui::NextColumn();// leaves
    ImGui::PushID(7702);
    {
        leaves.renderGui(_gui);
        if (leaves.changed) changed = true;
    }
    ImGui::PopID();

    ImGui::NextColumn();
    ImGui::PushID(7703);
    {

    }
    ImGui::PopID();

    changedForSaving |= changed;
}



void _twig::build(float _age, float _lodPixelsize, glm::mat4 _start, int rndSeed, int overrideLod, float _scale)
{
    std::mt19937 generator(rndSeed);
    std::uniform_real_distribution<> distribution(-1.f, 1.f);

    if (overrideLod >= 0) { lod = overrideLod; }
    float maxRadiusLod0 = 0;
    ribbonLength = 0;
    glm::mat4   stalk = _start;// glm::rotate(glm::mat4(1.0), -1.57079632679f, glm::vec3(1, 0, 0));
    glm::vec3   _pos = _start[3];// glm::vec3(0, 0, 0);

    // roatate the stals upwards
    float uprotTheta = atan2(_start[1][1], _start[0][1]) + distribution(generator) * 0.4f;
    glm::mat4 UPstalk = glm::rotate(glm::mat4(1.0), uprotTheta, (glm::vec3)_start[2]);
    stalk = UPstalk * _start;

    if (_age == 0) _age = this->age;
    int     stemCNT = (int)ceil(_age);
    float   stemStep = 1.0f / ((float)stemCNT - 1.f);
    int     leafOffset = 0;
    if (has_stem) leafOffset = stemCNT;
    float   stemCurve = stem_curve.x * 0.1f;

    float randStartLeafRotation = distribution(generator);

    for (int i = 0; i < stemCNT; i++)
    {
        float t = (float)i * stemStep;
        float s_Length = (stem_length.x * (1.f + distribution(generator) * stem_length.y)) * 0.001f * _scale;
        float leafAge = 1.f - pow(i / _age, leaf_age_power);
        if (tipleaves) leafAge = 1;
        stemCurve += (distribution(generator) * stem_curve.y) * stemStep;

        if (has_stem || (lod == 0))
        {
            ribbon[ribbonLength].type = 0;
            ribbon[ribbonLength].startBit = i > 0;
            ribbon[ribbonLength].position = _pos;
            ribbon[ribbonLength].radius = stem_width * .001f * 0.5f * pow(leafAge, 0.4f);
            ribbon[ribbonLength].bitangent = stalk[2];
            ribbon[ribbonLength].tangent = stalk[0];
            ribbon[ribbonLength].material = stem_Material.index;
            ribbon[ribbonLength].uv = float2(1.f, t);
            ribbonLength++;
        }

        // leaves
        //if (!tipleaves)
        {
            if (i >= startSegment)
            {

                for (int l = 0; l < numLeaves; l++)
                {

                    float rot = l * 6.14f / numLeaves + (i * leafRotation) + distribution(generator) * 0.05f;// +randStartLeafRotation;
                    glm::mat4 R = glm::rotate(glm::mat4(1.0), rot, (glm::vec3)stalk[2]);
                    glm::mat4 leaf = R * stalk;
                    float branch = stem_stalk.x + stem_stalk.y * leafAge + distribution(generator) * 0.4f;
                    glm::mat4 B = glm::rotate(glm::mat4(1.0), branch, (glm::vec3)leaf[0]);
                    leaf = B * leaf;

                    // rpatte these with RND so they roughly face upwards
                    float uprotTheta = atan2(leaf[1][1], leaf[0][1]) + distribution(generator) * 0.4f - 1.57f + 0.5f;
                    glm::mat4 UP = glm::rotate(glm::mat4(1.0), uprotTheta, (glm::vec3)leaf[2]);
                    leaf = UP * leaf;

                    leaves.ribbonLength = 0;
                    int matIndex = 0;// leafmaterialList.size() > 0 ? leafmaterialList[0].index : 0;
                    int leaflod = (overrideLod >= 0) ? overrideLod : lod;
                    if (lod == 0) leaflod = 3;
                    leaves.buildLeaf(leafAge, 0.f, 10 + l + i * 17 + rndSeed, leaf, _pos, (glm::vec3)stalk[2], leaflod);
                    if (lod > 0)
                    {
                        if (!tipleaves || (l == 0 && i == stemCNT - 1))
                        {
                            for (int k = 0; k < leaves.ribbonLength; k++) {
                                ribbon[leafOffset + k] = leaves.ribbon[k];
                            }
                            leafOffset += leaves.ribbonLength;
                        }
                    }
                    else
                    {
                        ribbon[ribbonLength - 1].radius = leaves.maxDistanceFromStem / 2.0f;       // FIXE use this in bake width if I can 
                        maxRadiusLod0 = __max(maxRadiusLod0, leaves.maxDistanceFromStem / 2.0f);
                    }
                }
            }
        }

        if (i < stemCNT)
        {
            glm::mat4 crv = glm::rotate(glm::mat4(1.0), stemCurve, (glm::vec3)stalk[0]);
            stalk = crv * stalk;
            glm::mat4 crv2 = glm::rotate(glm::mat4(1.0), (float)(distribution(generator) * stem_curve.y), (glm::vec3)stalk[0]);
            stalk = crv2 * stalk;

            // Gravitrophy ---------------------------------------------------------------
            glm::vec3 D = stalk[2];
            glm::vec3 graviR = glm::normalize(glm::cross(D, glm::vec3(0, 1, 0)));
            glm::mat4 graviM = glm::rotate(glm::mat4(1.0), stem_phototropism * stemStep, graviR);
            stalk = graviM * stalk;

            _pos += (glm::vec3)stalk[2] * s_Length;
        }
    }


    //if (tipleaves)
    {
        /*
        leaves.buildLeaf(1.0f, 0.f, 100, stalk, _pos, (glm::vec3)stalk[2], lod);
        if (lod > 0)
        {
            for (int k = 0; k < leaves.ribbonLength; k++) {
                ribbon[leafOffset + k] = leaves.ribbon[k];
            }
            leafOffset += leaves.ribbonLength;
        }*/
    }

    if (lod == 0)
    {
        ribbon[0].radius = maxRadiusLod0 * _scale;
        ribbon[0].uv = float2(1, 0);
        ribbon[0].albedoScale = (unsigned char)(leaves.albedoScale * 127.f);
        ribbon[0].translucencyScale = (unsigned char)(leaves.translucencyScale * 127.f);

        ribbon[1] = ribbon[ribbonLength - 1];
        ribbon[1].radius = maxRadiusLod0 * _scale;
        ribbon[1].position += ribbon[1].bitangent * (maxRadiusLod0 * 0.5f);
        ribbon[1].uv = float2(1, 1);
        ribbon[1].albedoScale = (unsigned char)(leaves.albedoScale * 127.f);
        ribbon[1].translucencyScale = (unsigned char)(leaves.translucencyScale * 127.f);

        //ribbon[2] = ribbon[1];
        //ribbon[3] = ribbon[1];

        leafOffset = 2;
    }
    /*
     if (lod == 0)
     {
         // simplify the shape
         int current = 1;
         float step = stemStep * startSegment;
         for (int i = startSegment; i < ribbonLength; i++)
         {
             if (step > ribbon[i].radius * 2 || (i == ribbonLength-1))
             {
                 ribbon[current] = ribbon[i];
                 current++;
                 step = 0;
             }
             step += stemStep;
         }

         // fix up first width since thias l is likely original stem width and will cut things off
         ribbon[0].radius = (ribbon[0].radius + ribbon[1].radius) / 2.0f;
         leafOffset = current;

         // and fix dU
         for (int i = 0; i < ribbonLength; i++)
         {
             ribbon[i].uv.x = ribbon[i].radius / maxRadiusLod0;
         }

         // and move the last one higher to include th top leaves
         ribbon[current - 1].position.y = extents.y;
     }
     */

    ribbonLength = leafOffset;

    if (lod == 3)
    {
        // calc extents
        extents = float3(0, 0, 0);
        for (int i = 0; i < ribbonLength; i++)
        {
            extents.x = __max(extents.x, abs(ribbon[i].position.x) + ribbon[i].radius);
            extents.y = __max(extents.y, abs(ribbon[i].position.y) + ribbon[i].radius);
            extents.z = __max(extents.z, abs(ribbon[i].position.z) + ribbon[i].radius);
        }
    }
}





void _twig::bakeToMaterial()
{
}







void _twigCollection::renderGui(Gui* _gui)
{
    ImGui::Text(twig.filename.c_str());

    ImGui::Text("numTwigs");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragInt("##numTwigs", &numTwigs, 1, 1, 20)) changed = true;

    ImGui::Text("radius");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##radius", &radius, 0.001f, 0.001f, 2.f)) changed = true;

    ImGui::Text("radiusAngle");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##radiusAngle", &radiusAngle, 0.01f, 0.0f, 2.f)) changed = true;

    ImGui::Text("radiusAgeScale");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##radiusAgeScale", &radiusAgeScale, 0.01f, 0.01f, 4.f)) changed = true;

    ImGui::Text("rnd");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##rnd", &rnd, 0.01f, 0.0f, 1.f)) changed = true;
}




float4 _weedLight::lightVertex(float3 _pos, float* depth)
{
    float3 N = glm::normalize(_pos);
    float m = __max(0.1f, N.y);
    float dT = 0;

    for (float t = 0; t < 10; t += 0.001f)
    {
        float x = t * width;
        float y0 = m * t;
        float y1 = baseheight + (height - baseheight) * exp(-(x * x));
        if (y0 > y1)
        {
            *depth = __max(0, sqrt(x * x + y1 * y1) - glm::length(_pos));
            dT = t;
            break;
        }
    }

    return float4(N, lerp(cosTop, cosBottom, saturate(dT)));
}


void _weedLight::renderGui(Gui* _gui)
{
}




void _weed::renderGui(Gui* _gui)
{
    ImGui::PushFont(_gui->getFont("roboto_20_bold"));
    {
        ImGui::Text("Weed : ");
        ImGui::SameLine(0, 10);
        ImGui::Text(filename.c_str());

    }
    ImGui::PopFont();

    ImGui::Text("(%3.2f, %3.2f)m", extents.x * 2, extents.y);

    ImGui::SameLine(0, 30);
    ImGui::Text("[%d]v", ribbonLength);

    if (ImGui::Button("Load")) load();

    if (changedForSaving) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.0f, 0.5f));
        ImGui::SameLine(0, 20);
        if (ImGui::Button("Save"))  save();
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(0, 20);
    if (ImGui::Button("Save as"))  saveas();

    ImGui::Text("lod");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(170);
    if (ImGui::SliderInt("##twiglod", &lod, 0, 3)) changed = true;

    ImGui::NewLine();

    ImGui::Text("age");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##age", &age, 0.1f, 1.2f, 20, "%2.1f")) changed = true;
    TOOLTIP("age");

    if (ImGui::Button("new twig"))
    {
        twigs.emplace_back();
        visibleTwig = twigs.size() - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("delete twig"))
    {
        twigs.erase(twigs.begin() + visibleTwig);
        visibleTwig = twigs.size() - 1;
    }

    if (twigs.size() > 1) {
        ImGui::DragInt("##tw", &visibleTwig, 1, 0, twigs.size() - 1);
    }
    else ImGui::NewLine();

    if (visibleTwig >= 0)
    {
        twigs.at(visibleTwig).renderGui(_gui);
    }

    ImGui::NewLine();
    ImGui::PushFont(_gui->getFont("roboto_20_bold"));
    {
        ImGui::Text("Lighting");
    }
    ImGui::PopFont();

    ImGui::Text("W/H");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##WHod", &L.width, 0.01f, 0.1f, 10.0f)) changed = true;
    ImGui::SameLine(0, 20);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##H", &L.height, 0.01f, 0.1f, 10.0f)) changed = true;

    ImGui::Text("base");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##base", &L.baseheight, 0.01f, 0.1f, 10.0f)) changed = true;

    if (ImGui::Button("from extents")) {
        L.width = extents.x * 0.8f;
        L.height = extents.y * 0.8f;
        L.baseheight = L.height / 2;
        changed = true;
    }

    ImGui::Text("density");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##density", &L.density, 0.01f, 0.1f, 10.0f)) changed = true;

    ImGui::Text("cos(theta)");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##CT1", &L.cosTop, 0.01f, 0.1f, 10.0f)) changed = true;
    ImGui::SameLine(0, 20);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##CT2", &L.cosBottom, 0.01f, 0.1f, 10.0f)) changed = true;


    ImGui::Text("Ao_depthScale");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##Ao_depthScale", &L.Ao_depthScale, 0.01f, 0.001f, 1.0f)) changed = true;

    ImGui::Text("sunTilt");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##sunTilt", &L.sunTilt, 0.01f, -1.0f, 1.0f)) changed = true;

    ImGui::Text("bDepth");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##bDepth", &L.bDepth, 0.1f, 0.1f, 50.0f)) changed = true;

    ImGui::Text("bScale");
    ImGui::SameLine(80, 0);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##bScale", &L.bScale, 0.01f, 0.01f, 1.0f)) changed = true;




    if (visibleTwig >= 0)
    {
        ImGui::NextColumn();
        twigs.at(visibleTwig).twig.renderGui(_gui);
        changed |= twigs.at(visibleTwig).twig.changed;
    }
}




void _weed::build(float _age, float _lodPixelsize)
{
    std::mt19937 generator(100);
    std::uniform_real_distribution<> distribution(-1.f, 1.f);
    ribbonLength = 0;

    for (auto& T : twigs)
    {
        for (int i = 0; i < T.numTwigs; i++)
        {
            glm::mat4 S = glm::rotate(glm::mat4(1.0), -1.57079632679f, glm::vec3(1, 0, 0));
            glm::vec3 startPos = glm::vec3(distribution(generator), 0, distribution(generator));
            while (glm::length(startPos) > 1)
            {
                startPos = glm::vec3(distribution(generator), 0, distribution(generator));
            }

            float outside = glm::length(startPos);
            startPos *= T.radius;



            glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), startPos));

            float rotateOutwards = T.radiusAngle * outside;
            rotateOutwards *= (1.0f + T.rnd * (distribution(generator)));

            glm::mat4 R = glm::rotate(glm::mat4(1.0), rotateOutwards, right);
            S = R * S;

            float ageRange = 0.5f + 0.5f * ((1 - outside) * T.radiusAgeScale);
            float A = age;// *(1.0f + T.rnd * (distribution(generator)))* ageRange;

            S[3] = glm::vec4(startPos, 0);

            T.twig.build(A, 0.f, S, i);

            for (int k = 0; k < T.twig.ribbonLength; k++) {
                ribbon[ribbonLength + k] = T.twig.ribbon[k];
            }
            ribbonLength += T.twig.ribbonLength;
        }
    }


    if (lod == 3)
    {
        // calc extents
        extents = float3(0, 0, 0);
        for (int i = 0; i < ribbonLength; i++)
        {
            extents.x = __max(extents.x, abs(ribbon[i].position.x) + ribbon[i].radius);
            extents.y = __max(extents.y, abs(ribbon[i].position.y) + ribbon[i].radius);
            extents.z = __max(extents.z, abs(ribbon[i].position.z) + ribbon[i].radius);
        }
    }
}




void _weed::load()
{
    std::filesystem::path path = terrainManager::lastfile.weed;
    FileDialogFilterVec filters = { {"weed"} };
    if (openFileDialog(filters, path))
    {
        std::ifstream is(path.string());
        cereal::JSONInputArchive archive(is);
        serialize(archive, _WEED_VERSION);
        changed = true;
        filename = path.filename().string();
        filepath = path.string();
        terrainManager::lastfile.weed = filepath;
        visibleTwig = twigs.size() - 1;
    }
}

void _weed::save()
{
    std::ofstream os(filepath);
    cereal::JSONOutputArchive archive(os);
    serialize(archive, _WEED_VERSION);
    changedForSaving = false;
}


void _weed::saveas()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"weed"} };
    if (saveFileDialog(filters, path))
    {
        std::ofstream os(path.string());
        cereal::JSONOutputArchive archive(os);
        serialize(archive, _WEED_VERSION);

        filename = path.filename().string();
        filepath = path.string();
        terrainManager::lastfile.weed = filepath;
    }
}





void _cubemap::toCube(float3 _v)
{
    float3 vAbs = float3(fabs(_v.x), fabs(_v.y), fabs(_v.z));

    if ((vAbs.x > vAbs.y) && (vAbs.x > vAbs.z))
    {
        face = (_v.x > 0) ? 0 : 1;
        _v /= vAbs.x;
        dx = _v.z * cubeHalfSize + cubeHalfSize + 1.f;
        dy = _v.y * cubeHalfSize + cubeHalfSize + 1.f;
    }
    else if (vAbs.y > vAbs.z)
    {
        face = (_v.y > 0) ? 2 : 3;
        _v /= vAbs.y;
        dx = _v.x * cubeHalfSize + cubeHalfSize + 1.f;
        dy = _v.z * cubeHalfSize + cubeHalfSize + 1.f;
    }
    else
    {
        face = (_v.z > 0) ? 4 : 5;
        _v /= vAbs.z;
        dx = _v.x * cubeHalfSize + cubeHalfSize + 1.f;
        dy = _v.y * cubeHalfSize + cubeHalfSize + 1.f;
    }

    x = (int)floor(dx);
    y = (int)floor(dy);
    dx -= x;
    dy -= y;        // not nie reg nie ek wil middelpunte van pixels he
}


float3 _cubemap::toVec(int face, int y, int x)
{
    float3 V;
    float S = cubeHalfSize + 1.f;
    float SS = (float)cubeHalfSize;
    switch (face)
    {
    case 0:
        V.x = 1;
        V.z = (x - S) / SS;
        V.y = (y - S) / SS;
        break;
    case 1:
        V.x = -1;
        V.z = (x - S) / SS;
        V.y = (y - S) / SS;
        break;
    case 2:
        V.y = 1;
        V.x = (x - S) / SS;
        V.z = (y - S) / SS;
        break;
    case 3:
        V.y = -1;
        V.x = (x - S) / SS;
        V.z = (y - S) / SS;
        break;
    case 4:
        V.z = 1;
        V.x = (x - S) / SS;
        V.y = (y - S) / SS;
        break;
    case 5:
        V.z = -1;
        V.x = (x - S) / SS;
        V.y = (y - S) / SS;
        break;
    }

    return glm::normalize(V);
}

/*
float3 _cubemap::toVec(glm::int3 c)
{
} */


float _cubemap::sampleDistance(float3 _v)
{
    toCube(_v);
    return data[face][y][x].d;
}


void _cubemap::clear()
{
    for (int f = 0; f < 6; f++)
    {
        for (int y = 0; y < cubeHalfSize * 2 + 2; y++)
        {
            for (int x = 0; x < cubeHalfSize * 2 + 2; x++)
            {
                data[f][y][x].d = 0;
                data[f][y][x].cone = 0;
                data[f][y][x].sum = 0;
                data[f][y][x].dir = float3(0, 0, 0);
            }
        }
    }
}

void _cubemap::writeDistance(float3 _v)     // fixme alpha
{
    glm::vec3 V = glm::normalize((_v - center) * scale);

    toCube(V);
    data[face][y][x].d = __max(data[face][y][x].d, glm::length(_v - center));
}


// net later vir smooth lookup
void _cubemap::solveEdges()
{
    float max = 0;
    for (int f = 0; f < 6; f++)
    {
        for (int y = 0; y < cubeHalfSize * 2 + 2; y++)
        {
            for (int x = 0; x < cubeHalfSize * 2 + 2; x++)
            {
                max = __max(max, data[f][y][x].d);
            }
        }
    }

    for (int f = 0; f < 6; f++)
    {
        for (int y = 0; y < cubeHalfSize * 2 + 2; y++)
        {
            for (int x = 0; x < cubeHalfSize * 2 + 2; x++)
            {
                data[f][y][x].d = __max(data[f][y][x].d, max * 0.5f);
            }
        }
    }



    for (int i = 1; i < cubeHalfSize * 2 + 1; i++)
    {
        //data[0][0][i] = data[3][0][i];
    }
}

void _cubemap::solve()
{
    for (int f = 0; f < 6; f++)
    {
        for (int y = 1; y < cubeHalfSize * 2 + 1; y++)
        {
            for (int x = 1; x < cubeHalfSize * 2 + 1; x++)
            {
                if (x == 8 && y == 8)
                {
                    bool bCM = true;
                }
                float3 V = toVec(f, y, x);
                float3 U = float3(0, 1, 0);
                if ((face == 2) || (face == 3)) {
                    U = float3(0, 0, 1);
                }

                float3 R = normalize(cross(U, V));
                U = cross(V, R);

                float scale = 0.4f; // about 30 degrees
                float3 v1 = V - U * scale;
                float3 v2 = V + (U * scale * 0.5f) + (R * scale * 0.8616f);
                float3 v3 = V + (U * scale * 0.5f) - (R * scale * 0.8616f);

                float d1 = sampleDistance(v1);
                float d2 = sampleDistance(v2);
                float d3 = sampleDistance(v3);
                v1 *= d1;
                v2 *= d2;
                v3 *= d3;

                float3 middle = (v1 + v2 + v3) * 0.333333f;
                float CONE = (data[f][y][x].d - length(middle)) / data[f][y][x].d;

                data[f][y][x].dir = normalize(cross(v2 - v1, v3 - v1));
                data[f][y][x].cone = CONE;
            }
        }
    }
}


float4 _cubemap::light(float3 _p, float* _depth)
{
    float3 virtualCenter = center;
    if (_p.y < center.y)
    {
        _p.y = glm::lerp(_p.y, center.y, 0.5f);
    }

    toCube(_p - virtualCenter);
    *_depth = data[face][y][x].d - length(_p - virtualCenter);
    return float4(data[face][y][x].dir, data[face][y][x].cone);
}







void _GroveBranch::reset()
{
    nodes.clear();
}



void _GroveTree::loadStemMaterial()
{
    std::filesystem::path path = terrainManager::lastfile.vegMaterial;
    FileDialogFilterVec filters = { {"vegetationMaterial"} };
    if (openFileDialog(filters, path))
    {
        branch_Material.index = _plantMaterial::static_materials_veg.find_insert_material(path);
        branch_Material.name = path.string();       // FIXME dont liek this,. should be relative
        branch_Material.displayname = path.filename().string().substr(0, path.filename().string().length() - 19);
        terrainManager::lastfile.vegMaterial = path.string();
    }
}

void _GroveTree::reloadMaterials()
{
    branch_Material.index = _plantMaterial::static_materials_veg.find_insert_material(branch_Material.name);
}

void _GroveTree::renderGui(Gui* _gui)
{
    ImGui::PushFont(_gui->getFont("roboto_20"));
    {

        /*
        leafBuilder.renderGui(_gui);
        if (leafBuilder.changed)
        {
            leafBuilder.changed = false;
            leafBuilder.ribbonLength = 0;

            for (int y = 0; y < 10; y++)
            {
                for (int x = 0; x < 10; x++)
                {
                    glm::mat4 vertex = glm::mat4(1.0);

                    vertex = glm::rotate(vertex, 0.628f * (float)x + (y), glm::vec3(0, 1, 0));
                    //vertex = glm::translate(vertex, (glm::vec3)vertex[2] * 0.02f);
                    //vertex = glm::translate(vertex, glm::vec3(0, 0.1 * y, 0));

                    leafBuilder.buildLeaf((10 - y) * 0.1f, 0.001f, y * 102 + x, vertex, glm::vec3(0, 0.1 * y, 0) + (glm::vec3)vertex[2] * 0.02f);
                }

            }

            rebuildRibbons_Leaf();
        }
        */

        Gui::Window weedPanel(_gui, "weed builder##tfPanel", { 900, 900 }, { 100, 100 });
        {

            ImGui::PushFont(_gui->getFont("roboto_18"));
            ImGui::PushItemWidth(170);
            if (ImGui::Combo("###modeSelector", (int*)&rootMode, "Grove tree\0Weed\0Twig\0Leaf\0")) { ; }
            ImGui::PopItemWidth();
            ImGui::NewLine();



            ImGui::Columns(3);

            switch (rootMode)
            {
            case mode_grove:
                ImGui::DragInt("##size", &numFour, 1, 0, 100);
                if (ImGui::Button("load")) {
                    load();
                }
                ImGui::Text("branches - %d", branches.size());
                ImGui::Text("branch leaves - %d", branchLeaves.size());
                ImGui::Text("# ribbons - %d", numBranchRibbons);
                ImGui::Text("# tipleaves - %d", endLeaves.size());
                ImGui::Text("# side B found - %d", numSideBranchesFound);
                ImGui::Text("# dead ends - %d", numDeadEnds);
                ImGui::Text("# bad ends - %d", numBadEnds);


                if (ImGui::Checkbox("branch leaves", &includeBranchLeaves)) {
                    rebuildRibbons();
                }
                if (ImGui::Checkbox("tip leaves", &includeEndLeaves)) {
                    rebuildRibbons();
                }
                if (ImGui::Checkbox("showOnlyUnattached", &showOnlyUnattached)) {
                    rebuildRibbons();
                }


                if (ImGui::DragFloat("start", &startRadius, 0.1f, 0.01f, 10.f)) {
                    rebuildRibbons();
                }
                if (ImGui::DragFloat("end", &endRadius, 0.001f, 0.001f, 0.1f)) {
                    rebuildRibbons();
                }
                if (ImGui::DragFloat("step", &stepFactor, 0.1f, 1.0f, 15.f)) {
                    rebuildRibbons();
                }
                if (ImGui::DragFloat("bend", &bendFactor, 0.01f, 0.6f, 1.f)) {
                    rebuildRibbons();
                }

                if (ImGui::Button(branch_Material.displayname.c_str(), ImVec2(170, 0)))
                {
                    loadStemMaterial();
                    rebuildRibbons();
                }
                TOOLTIP(branch_Material.displayname.c_str());

                ImGui::NewLine();
                ImGui::Text("Twigs");
                if (ImGui::DragFloat("twig scale", &twigScale, 0.01f, 0.001f, 1.f)) {
                    rebuildRibbons();
                }

                if (ImGui::DragFloat("brnachAge", &branchTwigsAge, 0.1f, 0.1f, 10.f)) {
                    rebuildRibbons();
                }



                if (ImGui::DragFloat("tip Age", &tipTwigsAge, 0.1f, 0.1f, 10.f)) {
                    rebuildRibbons();
                }

                if (ImGui::DragFloat("branch Skip", &twigSkip, 0.01f, 0.f, 30.f)) {
                    rebuildRibbons();
                }


                ImGui::Text("density");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##density", &L.density, 0.01f, 0.1f, 10.0f));

                ImGui::Text("cos(theta)");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##CT1", &L.cosTop, 0.01f, 0.1f, 10.0f));
                ImGui::SameLine(0, 20);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##CT2", &L.cosBottom, 0.01f, 0.1f, 10.0f));


                ImGui::Text("Ao_depthScale");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##Ao_depthScale", &static_Ao_depthScale, 0.01f, 0.001f, 1.0f));

                ImGui::Text("sunTilt");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##sunTilt", &static_sunTilt, 0.01f, -1.0f, 1.0f));

                ImGui::Text("bDepth");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##bDepth", &static_bDepth, 0.1f, 0.1f, 50.0f));

                ImGui::Text("bScale");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##bScale", &static_bScale, 0.01f, 0.01f, 1.0f));


                //Visibility rules
                ImGui::NewLine();

                ImGui::Text("Visibility");

                ImGui::Text("toRoot");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::Checkbox("##vis_root", &vis.expandToRoot));

                ImGui::Text("toTip");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::Checkbox("##visTip", &vis.expandToLeaves));

                ImGui::Text("smallerThan");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::Checkbox("##visSamller", &vis.SmallerThan));

                ImGui::Text("radius");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##visRadius", &vis.radiusCutoff, 0.01f, 0.01f, 1.0f));

                ImGui::Text("core");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##visCore", &vis.coreRadius, 0.01f, 0.f, 5.0f));

                ImGui::Text("test");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##visTest", &vis.TestSize, 0.01f, 0.01f, 1.0f));

                if (ImGui::Button("rebuild Vis"))
                {
                    rebuildVisibility();
                }





                ImGui::NextColumn();
                tipTwigs.renderGui(_gui);
                if (tipTwigs.changed) {
                    rebuildRibbons();
                    tipTwigs.changed = false;
                }

                break;
            case mode_weed:
                weedBuilder.renderGui(_gui);

                break;
            case mode_twig:
                twigBuilder.renderGui(_gui);
                break;
            case mode_leaf:
                leafBuilder.renderGui(_gui);
                break;
            }




            ImGui::PopFont();
        }
        weedPanel.release();


        if (twigBuilder.changed)
        {
            twigBuilder.changed = false;
            twigBuilder.ribbonLength = 0;

            glm::mat4 vertex = glm::mat4(1.0);
            //vertex = glm::rotate(vertex, 0.628f * (float)x + (y), glm::vec3(0, 1, 0));

            glm::mat4   S = glm::rotate(glm::mat4(1.0), -1.57079632679f, glm::vec3(1, 0, 0));   // upright ay 0.0.0
            twigBuilder.build(0, 0.f, S);

            rebuildRibbons_Twig();
        }


        if (weedBuilder.changed)
        {
            weedBuilder.changed = false;
            weedBuilder.ribbonLength = 0;

            glm::mat4   S = glm::rotate(glm::mat4(1.0), -1.57079632679f, glm::vec3(1, 0, 0));   // upright ay 0.0.0
            weedBuilder.build(6, 0.f);

            rebuildRibbons_Weed();
        }

    }
    ImGui::PopFont();
}

void _GroveTree::importTwig()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"obj.twig"} };
    if (openFileDialog(filters, path))
    {
        twiglength = 0;
        objfile = fopen(path.string().c_str(), "r");
        if (objfile) {
            int numSegments;
            int ret = 10;
            enfOfFile = false;
            while (ret > 0)
            {
                ret = fscanf(objfile, "%d\n", &numSegments);
                float v = 0.f;
                float dV = 1.0f / (numSegments - 1);
                for (int i = 0; i < numSegments; i++)
                {
                    float3 vin = readVertex();
                    float3 v0 = float3(vin.x, vin.z + vin.x, -vin.y);
                    vin = readVertex();
                    float3 v1 = float3(vin.x, vin.z + vin.x, -vin.y);
                    twig[twiglength].type = 0;
                    twig[twiglength].startBit = i > 0;
                    twig[twiglength].position = (v0 + v1) * 0.5f;
                    twig[twiglength].bitangent = (v1 - v0) * 0.5f;
                    twig[twiglength].uv = float2(1, v);
                    twig[twiglength].material = 1;
                    v += dV;
                    twiglength++;
                }
            }
            fclose(objfile);
        }
    }
}

void _GroveTree::readHeader()
{
    enfOfFile = false;
    char header[256];
    fscanf(objfile, "%39[^\n]\n", header);
    fscanf(objfile, "%39[^\n]\n", header);
    fscanf(objfile, "%39[^\n]\n", header);
    //fscanf(objfile, "%s\n", header);
    //fscanf(objfile, "%39[^,]\n", header);    // just read 3 lines

    //enfOfFile = true;
    //while (enfOfFile) {
    //    readVertex();
    //}

    verts[0] = readVertex();
    numVerts = 1;
    branchMode = true;
}


void _GroveTree::read2()
{
    // completes a triangle and calculate its plane
    verts[1] = readVertex();
    verts[2] = readVertex();
    numVerts = 3;
    nodeDir = glm::normalize(glm::cross(verts[1] - verts[0], verts[2] - verts[0]));
    nodeOffset = glm::dot(nodeDir, verts[0]);
    isPlanar = true;
}

void _GroveTree::readahead1()
{
    verts[numVerts] = readVertex();
    float offset = glm::dot(nodeDir, verts[numVerts]) - nodeOffset;
    if (fabs(offset) > 0.003) {
        isPlanar = false;
    }
    numVerts++;
}

float3 _GroveTree::readVertex()
{
    float3 v;
    char type[256];
    int ret = fscanf(objfile, "%s %f %f %f\n", type, &v.x, &v.y, &v.z);
    if (ret < 4) {
        enfOfFile = true;
    }
    else {
        enfOfFile = false;
    }
    totalVerts++;
    return v;
}

void _GroveTree::testBranchLeaves()
{
    float3 center = (verts[0] + verts[1] + verts[2]) / 3.0f;
    for (int j = 1; j < currentBranch->nodes.size(); j++) {         // start at 1, dont seacr that first node, it breaks branch inetrsections
        float3 nodeCenter = currentBranch->nodes[j].pos;
        float dist = glm::length(nodeCenter - center);
        if (fabs(dist) < 0.004f) {
            //we have a leaf, add it in future
            float l1 = glm::length(verts[1] - verts[0]);
            float l2 = glm::length(verts[2] - verts[1]);
            float l3 = glm::length(verts[0] - verts[2]);
            float radius = 0.19f * (l1 + l2 + l3);

            _leafNode L;
            L.pos = currentBranch->nodes[j].pos;
            L.dir = nodeDir;
            L.branchNode = j;
            L.branchIndex = branches.size() - 1;
            branchLeaves.push_back(L);
            currentBranch->leaves.push_back(L);

            verts[0] = readVertex();
            read2();
            return;
        }
    }

    // we dont, this is the start of the next branch, and we have 3 already
    branchMode = true;
    branches.emplace_back();
    currentBranch = &branches.back();
    currentBranch->nodes.clear();
    oldNumVerts = 1000;
}


void _GroveTree::load()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"obj"} };
    if (openFileDialog(filters, path))
    {
        filename = path.string();
        branches.clear();
        branchLeaves.clear();
        endLeaves.clear();
        totalVerts = 0;
        numDeadEnds = 0;
        numBadEnds = 0;


        objfile = fopen(filename.c_str(), "r");
        if (objfile) {
            readHeader();
            branches.emplace_back();
            currentBranch = &branches.back();
            currentBranch->nodes.clear();
            read2();
            oldNumVerts = 1000; // just bog to alwasy keep first node
            while (!enfOfFile)
            {
                if (branchMode)
                {
                    while (isPlanar) {
                        readahead1();
                    }

                    // solve vert and save in branch
                    float3 center = float3(0, 0, 0);
                    for (int j = 0; j < numVerts - 1; j++) {
                        center += verts[j] * (1.0f / (numVerts - 1));
                    }

                    float l1 = glm::length(verts[1] - verts[0]);
                    float l2 = glm::length(verts[2] - verts[1]);
                    float l3 = glm::length(verts[0] - verts[2]);
                    float radius = 0.19f * (l1 + l2 + l3);

                    if (oldNumVerts < numVerts) {
                        if ((numVerts == 8) && (!currentBranch->isDead)) {
                            // so when the verts increase we have hit the endcap of a branch
                            /*
                            endLeaves.emplace_back();
                            endLeaves.back().pos = center;
                            endLeaves.back().dir = nodeDir;
                            */
                        }
                        else if (numVerts == 5) {
                            numDeadEnds++;
                        }
                        else {
                            numBadEnds++;
                        }
                        branchMode = false;
                    }
                    oldNumVerts = numVerts;

                    //if(branches.size()==1 && currentBranch->nodes)
                    if (currentBranch->nodes.size() == 1)
                    {
                        currentBranch->nodes[0].radius = radius;
                    }
                    _branchnode BN;
                    BN.pos = center;
                    BN.radius = radius;
                    BN.dir = nodeDir;
                    currentBranch->nodes.push_back(BN);

                    verts[0] = verts[numVerts - 1];
                    read2();
                }
                else
                {
                    testBranchLeaves();
                }

            }
            fclose(objfile);



            findSideBranches();
            propagateDead(3);
            calcLight();
            rebuildRibbons();

        }
    }
}

void _GroveTree::rebuildVisibility()
{
    for (auto& branch : branches)
    {
        branch.isVisible = false;

        if (vis.coreRadius > 0)
        {
            bool anyVisible = false;

            for (auto& node : branch.nodes)
            {
                node.isNodeVisible = false;

                if ((abs(node.pos.x) < vis.coreRadius) &&
                    ((node.pos.z > -vis.coreRadius) || (node.pos.y > extents.y - vis.coreRadius)))
                {
                    node.isNodeVisible = true;
                    anyVisible |= node.isNodeVisible;
                }
            }
            branch.isVisible = anyVisible;
        }
        else
        {
            if (vis.SmallerThan)
            {
                bool anyVisible = false;
                for (auto& node : branch.nodes)
                {
                    node.isNodeVisible = node.radius < vis.radiusCutoff;
                    anyVisible |= node.isNodeVisible;
                }
                branch.isVisible = anyVisible;
            }

            /*
            if (!vis.SmallerThan)
            {
                bool anyVisible = false;
                for (auto& node : branch.nodes)
                {
                    node.isVisible = node.radius > vis.radiusCutoff;
                    anyVisible |= node.isVisible;
                }
                branch.isVisible = anyVisible;
            }
            */
        }


        // Front of tree
        {
            bool anyVisible = false;

            for (auto& node : branch.nodes) // look for that crossover 45 degrees 2 meters away
            {
                if ((node.pos.z < -2.0f) && (abs(node.pos.x) / abs(node.pos.z) < 1))
                    //if ((node.pos.z < -3.0f))
                {
                    anyVisible = true;
                }
                node.isNodeVisible = anyVisible;
            }
            branch.isVisible = anyVisible;
        }

    }




    if (vis.expandToRoot)
    {
        for (auto& branch : branches)
        {
            if (branch.isVisible)
            {
                int root = branch.rootBranch;
                while (root >= 0)
                {
                    branches[root].isVisible = true;
                    root = branches[root].rootBranch;
                }
            }
        }
    }


    if (vis.expandToLeaves)
    {
        for (int B = 0; B < branches.size(); B++)
        {
            auto& root = branches[branches[B].rootBranch];

            if (root.isVisible && (root.nodes[branches[B].sideNode].radius < vis.radiusCutoff))
            {
                branches[B].isVisible = true;
            }
        }
    }



    rebuildRibbons();
}


bool pointCylindar(const glm::vec3& point, const glm::vec3& A, const glm::vec3& B, const float radius)
{
    glm::vec3 dir = B - A;
    float L = glm::length(dir);
    dir = glm::normalize(dir);
    glm::vec3 P = point - A;
    float dist = glm::dot(P, dir);
    if ((dist >= -radius) && (dist < (L + radius))) {
        glm::vec3 Pline = dir * dist;
        if (glm::length(P - Pline) < radius) {
            return true;
        }
    }

    return false;
}

void _GroveTree::propagateDead(int _root)
{
}


void _GroveTree::findSideBranches()
{
    numSideBranchesFound = 0;
    for (int B = 0; B < branches.size(); B++)
    {
        glm::vec3 P0 = branches[B].nodes[0].pos;
        float R0 = branches[B].nodes[0].radius;
        for (int C = B - 1; C >= 0; C--)   // try <B again
        {
            if (branches[B].rootBranch >= 0) continue; // have al;ready found it

            if (branches[C].nodes[0].radius > R0)
            {
                for (int n = 0; n < branches[C].nodes.size() - 1; n++)
                {
                    //if (branches[C].nodes[n].radius < (R0 * 0.6)) break;

                    if (pointCylindar(P0, branches[C].nodes[n].pos, branches[C].nodes[n + 1].pos, branches[C].nodes[n].radius * 4))
                    {
                        branches[B].rootBranch = C;
                        branches[B].sideNode = n;
                        if (branches[C].isDead)
                        {
                            branches[B].isDead = true;
                        }
                        branches[C].sideBranches.push_back(B);
                        numSideBranchesFound++;
                        break;
                    }
                }
            }
        }
    }

    for (int B = 0; B < branches.size(); B++)
    {
        if (!branches[B].isDead)
        {
            endLeaves.emplace_back();
            endLeaves.back().pos = branches[B].nodes.back().pos;
            endLeaves.back().dir = branches[B].nodes.back().dir;
            endLeaves.back().branchIndex = B;
        }
    }
}



void _GroveTree::rebuildRibbons()
{
    std::mt19937 generator(100);
    std::uniform_real_distribution<> distribution(-1.f, 1.f);
    std::uniform_real_distribution<> dist_0_1(0.f, 1.f);

    numBranchRibbons = 0;

    // add the branches
    //for (int b = 0; b < branches.size(); b++)
    for (auto& branch : branches)
    {
        if (showOnlyUnattached && branch.rootBranch >= 0) continue;
        //if (showOnlyUnattached && b == 0) continue;

        if (branch.isVisible && branch.nodes.size() > 1)        // FIXME remocve branches with too few nodes at insert time
        {
            float v = 0.0f;
            float3 prevPos = branch.nodes.front().pos;


            bool notFirst = false;
            for (auto& node : branch.nodes)
            {
                v += length(node.pos - prevPos) * 2.5f;

                if (node.isNodeVisible)
                {
                    branchRibbons[numBranchRibbons].type = 0;
                    branchRibbons[numBranchRibbons].startBit = notFirst;
                    branchRibbons[numBranchRibbons].position = node.pos;
                    branchRibbons[numBranchRibbons].bitangent = node.dir;
                    branchRibbons[numBranchRibbons].radius = node.radius;
                    branchRibbons[numBranchRibbons].material = branch_Material.index;
                    branchRibbons[numBranchRibbons].lightCone = light.cubemap.light(branchRibbons[numBranchRibbons].position, &branchRibbons[numBranchRibbons].lightDepth);
                    branchRibbons[numBranchRibbons].uv = float2(1, v);


                    // now see if we are goingt to keep it or overwrite it
                    {
                        if (notFirst)
                        {
                            float stepThis = stepFactor * pow((endRadius / node.radius), 0.3);/// 0.2 IS TE ERG

                            if ((node.radius < startRadius) &&
                                (node.radius > endRadius) &&
                                (glm::length(branchRibbons[numBranchRibbons].position - branchRibbons[numBranchRibbons - 1].position) / node.radius > stepThis))
                            {
                                numBranchRibbons++;
                            }
                        }
                        else if (node.radius > endRadius) {             // test for the start
                            numBranchRibbons++;
                            notFirst = true;
                        }
                    }
                }
                prevPos = node.pos;
            }
        }
    }



    if (includeBranchLeaves) {
        float skip = twigSkip * twigSkip * 2.0f;
        int step = __max((int)skip, 1);
        for (int b = 0; b < branchLeaves.size(); b += step)
        {
            if (branches[branchLeaves[b].branchIndex].nodes[branchLeaves[b].branchNode].isNodeVisible)
            {
                float3 D = normalize(branchLeaves[b].dir);
                float3 U = float3(0, 1, 0);
                float3 R = normalize(cross(U, D));
                U = cross(D, R);

                glm::mat4 S;
                S[0] = float4(R, 0);
                S[1] = float4(U, 0);
                S[2] = float4(D, 0);
                S[3] = float4(branchLeaves[b].pos, 0);// -(S[2] * 0.0f); // 20cm backwards if scaled


                float scale = pow((float)step, 0.5);
                if (step > 1)
                {
                    S[3] += S[2] * (1 - scale) * 0.2f;
                    tipTwigs.build(branchTwigsAge + distribution(generator), 0.f, S, b, 0, scale);
                }
                else
                {
                    if (dist_0_1(generator) < skip)
                    {
                        tipTwigs.build(branchTwigsAge + distribution(generator), 0.f, S, b, 0, 1.0f);
                    }
                    else {
                        tipTwigs.build(branchTwigsAge + distribution(generator), 0.f, S, b, 2, 1.0f);
                    }

                }

                for (int k = 0; k < tipTwigs.ribbonLength; k++) {
                    branchRibbons[numBranchRibbons] = tipTwigs.ribbon[k];
                    branchRibbons[numBranchRibbons].lightCone = light.cubemap.light(branchRibbons[numBranchRibbons].position, &branchRibbons[numBranchRibbons].lightDepth);
                    numBranchRibbons++;
                }
            }
        }
    }

    if (includeEndLeaves) {

        float skip = twigSkip * twigSkip * 2.0f;
        int step = __max((int)skip, 1);
        for (int b = 0; b < endLeaves.size(); b += step)
        {
            auto& branch = branches[endLeaves[b].branchIndex];
            if (branch.isVisible && branch.nodes.back().isNodeVisible)
            {
                float3 P = endLeaves[b].pos;
                float3 D = normalize(endLeaves[b].dir);
                float3 U = float3(0, 1, 0);
                float3 R = normalize(cross(U, D));
                U = cross(D, R);

                glm::mat4 S;
                S[0] = float4(R, 0);
                S[1] = float4(U, 0);
                S[2] = float4(D, 0);
                S[3] = float4(P, 0);// -(S[2] * 0.0f); // 20cm backwards if scaled

                float scale = pow((float)step, 0.5);
                if (step > 1)
                {
                    S[3] += S[2] * (1 - scale) * 0.2f;
                    tipTwigs.build(tipTwigsAge + distribution(generator), 0.f, S, b, 0, scale);
                }
                else
                {
                    if (dist_0_1(generator) < skip)
                    {
                        tipTwigs.build(tipTwigsAge + distribution(generator), 0.f, S, b, 0, 1.0f);
                    }
                    else
                    {
                        tipTwigs.build(tipTwigsAge + distribution(generator), 0.f, S, b, 2, 1.0f);
                    }
                }


                for (int k = 0; k < tipTwigs.ribbonLength; k++) {
                    branchRibbons[numBranchRibbons] = tipTwigs.ribbon[k];
                    branchRibbons[numBranchRibbons].lightCone = light.cubemap.light(branchRibbons[numBranchRibbons].position, &branchRibbons[numBranchRibbons].lightDepth);
                    numBranchRibbons++;
                }
            }
        }
    }


    extents = float3(0, 0, 0);
    for (int i = 0; i < numBranchRibbons; i++)
    {
        extents.x = __max(extents.x, abs(branchRibbons[i].position.x));
        extents.y = __max(extents.y, abs(branchRibbons[i].position.y));
        extents.z = __max(extents.z, abs(branchRibbons[i].position.z));
    }


    for (int i = 0; i < numBranchRibbons; i++)
    {
        packedRibbons[i] = branchRibbons[i].pack();
    }

    bChanged = true;
}

void _GroveTree::rebuildRibbons_Leaf()
{
    for (int i = 0; i < leafBuilder.ribbonLength; i++)
    {
        packedRibbons[i] = leafBuilder.ribbon[i].pack();
    }
    bChanged = true;
    numBranchRibbons = leafBuilder.ribbonLength;
}

void _GroveTree::rebuildRibbons_Twig()
{
    extents = twigBuilder.extents;

    // Doen myy multi plant lighting hier
    for (int i = 0; i < twigBuilder.ribbonLength; i++)
    {
        twigBuilder.ribbon[i].lightDepth = 0.2f;    // DOEN hierdie beter
        float3 Ldir = glm::normalize(twigBuilder.ribbon[i].position - float3(0, 0.1f, 0));
        twigBuilder.ribbon[i].lightCone = float4(Ldir, 0);
        packedRibbons[i] = twigBuilder.ribbon[i].pack();
    }
    bChanged = true;
    numBranchRibbons = twigBuilder.ribbonLength;
}


void _GroveTree::rebuildRibbons_Weed()
{
    extents = weedBuilder.extents;

    // Doen myy multi plant lighting hier
    for (int i = 0; i < weedBuilder.ribbonLength; i++)
    {
        weedBuilder.ribbon[i].lightCone = weedBuilder.L.lightVertex(weedBuilder.ribbon[i].position, &weedBuilder.ribbon[i].lightDepth);
        //float3 Ldir = glm::normalize(weedBuilder.ribbon[i].position - float3(0, 0.1f, 0));
        //weedBuilder.ribbon[i].lightCone = float4(Ldir, 0);
        packedRibbons[i] = weedBuilder.ribbon[i].pack();
    }
    bChanged = true;
    numBranchRibbons = weedBuilder.ribbonLength;

    static_Ao_depthScale = weedBuilder.L.Ao_depthScale;
    static_sunTilt = weedBuilder.L.sunTilt;
    static_bDepth = weedBuilder.L.bDepth;
    static_bScale = weedBuilder.L.bScale;
}


/*
glm::int3 _GroveTree::cubeLookup(glm::vec3 Vin)
{
    glm::vec3 V = glm::normalize((Vin - light.center) * light.scale);
    glm::int3 R;
    if ((fabs(V.x) > fabs(V.y)) && (fabs(V.x) > fabs(V.z)))
    {
        if (V.x > 0) R.x = 0;
        else R.x = 1;
        V /= fabs(V.x);
        R.y = (int)floor(V.y * 8.f) + 8;
        R.z = (int)floor(V.z * 8.f) + 8;
    }
    else if (fabs(V.y) > fabs(V.x))
    {
        if (V.y > 0) R.x = 2;
        else R.x = 3;
        V /= fabs(V.y);
        R.y = (int)floor(V.x * 8.f) + 8;
        R.z = (int)floor(V.z * 8.f) + 8;
    }
    else
    {
        if (V.z > 0) R.x = 4;
        else R.x = 5;
        V /= fabs(V.z);
        R.y = (int)floor(V.x * 8.f) + 8;
        R.z = (int)floor(V.y * 8.f) + 8;
    }


    return R;
}
*/

void _GroveTree::calcLight()
{
    // center
    light.center = float3(0, 0, 0);
    light.Min = float3(10000, 10000, 10000);
    light.Max = float3(-10000, -10000, -10000);
    int cnt = endLeaves.size();
    for (int b = 0; b < cnt; b++)
    {
        light.center += endLeaves[b].pos;
        light.Min = glm::min(light.Min, endLeaves[b].pos);
        light.Max = glm::max(light.Max, endLeaves[b].pos);
    }
    if (cnt > 0)
    {
        light.center /= cnt;
    }
    glm::vec3 extents = light.Max - light.Min;
    light.scale = 2.0f / extents;

    light.cubemap.clear();
    light.cubemap.center = light.center;
    light.cubemap.scale = light.scale;
    light.cubemap.twigOffset = 0.1f;


    for (int b = 0; b < cnt; b++)
    {
        light.cubemap.writeDistance(endLeaves[b].pos + endLeaves[b].dir * 0.5f);
    }

    for (int b = 0; b < branchLeaves.size(); b++)
    {
        light.cubemap.writeDistance(branchLeaves[b].pos + branchLeaves[b].dir * 0.2f);
    }

    light.cubemap.solveEdges();
    light.cubemap.solve();
}










void _terrainSettings::renderGui(Gui* _gui)
{
    ImGui::PushFont(_gui->getFont("roboto_32"));
    {
        static const int maxSize = 2048;
        char buf[maxSize];
        sprintf_s(buf, maxSize, name.c_str());
        ImGui::SetNextItemWidth(300);
        if (ImGui::InputText("##name", buf, maxSize))
        {
            name = buf;
        }

        ImGui::NewLine();
        ImGui::PushFont(_gui->getFont("roboto_20"));
        {
            ImGui::Text("projection");
            sprintf_s(buf, maxSize, projection.c_str());
            ImGui::SetNextItemWidth(1000);
            if (ImGui::InputText("##projection", buf, maxSize))
            {
                projection = buf;
            }
        }
        ImGui::Text("size");
        ImGui::PopFont();

        ImGui::SetNextItemWidth(150);
        ImGui::DragFloat("##size", &size, 1, 1000, 200000, "%5.0f m ");

        ImGui::NewLine();
        sprintf_s(buf, maxSize, dirRoot.c_str());
        ImGui::SetNextItemWidth(600);
        if (ImGui::InputText("root", buf, maxSize))
        {
            dirRoot = buf;
        }

        sprintf_s(buf, maxSize, dirExport.c_str());
        ImGui::SetNextItemWidth(600);
        if (ImGui::InputText("export(EVO)", buf, maxSize))
        {
            dirExport = buf;
        }

        sprintf_s(buf, maxSize, dirGis.c_str());
        ImGui::SetNextItemWidth(600);
        if (ImGui::InputText("gis", buf, maxSize))
        {
            dirGis = buf;
        }

        sprintf_s(buf, maxSize, dirResource.c_str());
        ImGui::SetNextItemWidth(600);
        if (ImGui::InputText("resource", buf, maxSize))
        {
            dirResource = buf;
        }

        ImGui::NewLine();

        ImGui::NewLine();
        if (ImGui::Button("Save"))
        {
            std::filesystem::path path;
            FileDialogFilterVec filters = { {"terrainSettings.json"} };
            if (saveFileDialog(filters, path))
            {
                std::ofstream os(path);
                cereal::JSONOutputArchive archive(os);
                serialize(archive, 100);
            }
        }
    }
    ImGui::PopFont();
}


void quadtree_tile::init(uint _index)
{
    index = _index;
    parent = nullptr;
    child[0] = nullptr;
    child[1] = nullptr;
    child[2] = nullptr;
    child[3] = nullptr;
}

void quadtree_tile::set(uint _lod, uint _x, uint _y, float _size, float4 _origin, quadtree_tile* _parent)
{
    lod = _lod;
    x = _x;
    y = _y;
    size = _size;
    origin = _origin;

    boundingSphere = origin + float4(size / 2, 0, size / 2, 0);
    boundingSphere.w = 1.0f;

    parent = _parent;
    child[0] = nullptr;
    child[1] = nullptr;
    child[2] = nullptr;
    child[3] = nullptr;

    if (parent)
    {
        origin.y = parent->boundingSphere.y - size * 2;
        boundingSphere.y = parent->boundingSphere.y;
    }
    else
    {
        origin.y = size * 2;
        boundingSphere.y = 0;
    }

    numQuads = 0;
    numPlants = 0;

    forSplit = false;
    forRemove = false;

    elevationHash = 0;
}




terrainManager::terrainManager()
{
    std::ifstream is("lastFile.xml");
    if (is.good()) {
        cereal::XMLInputArchive archive(is);
        archive(CEREAL_NVP(lastfile));

        mRoadNetwork.lastUsedFilename = lastfile.road;
    }

    std::ifstream isT(lastfile.terrain);
    if (isT.good()) {
        cereal::JSONInputArchive archive(isT);
        settings.serialize(archive, 100);
    }

    terrafectorSystem::pEcotopes = &mEcosystem;
}



terrainManager::~terrainManager()
{
    std::ofstream os("lastFile.xml");
    if (os.good()) {
        cereal::XMLOutputArchive archive(os);
        lastfile.road = mRoadNetwork.lastUsedFilename.string();
        archive(CEREAL_NVP(lastfile));
    }
}



void terrainManager::onLoad(RenderContext* pRenderContext, FILE* _logfile)
{
    terrafectors._logfile = _logfile;
    {
        Sampler::Desc samplerDesc;
        samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp).setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setMaxAnisotropy(8);
        sampler_Clamp = Sampler::create(samplerDesc);

        samplerDesc.setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap).setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setMaxAnisotropy(8);
        sampler_Trilinear = Sampler::create(samplerDesc);

        samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp).setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setMaxAnisotropy(4);
        sampler_ClampAnisotropic = Sampler::create(samplerDesc);

        samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap).setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setMaxAnisotropy(1);
        sampler_Ribbons = Sampler::create(samplerDesc);
    }

    {
        split.debug_texture = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::RGBA8Unorm, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess);
        //split.bicubic_upsample_texture = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::R32Float, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess);
        split.normals_texture = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::R11G11B10Float, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess);
        split.vertex_A_texture = Texture::create2D(tile_numPixels / 2, tile_numPixels / 2, Falcor::ResourceFormat::R16Uint, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Falcor::Resource::BindFlags::UnorderedAccess);
        split.vertex_B_texture = Texture::create2D(tile_numPixels / 2, tile_numPixels / 2, Falcor::ResourceFormat::R16Uint, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Falcor::Resource::BindFlags::UnorderedAccess);
    }

    {
        std::vector<glm::uint16> vertexData(tile_numPixels / 2 * tile_numPixels / 2);
        memset(vertexData.data(), 0, tile_numPixels / 2 * tile_numPixels / 2 * sizeof(glm::uint16));	  // set to zero's
        split.vertex_clear = Texture::create2D(tile_numPixels / 2, tile_numPixels / 2, Falcor::ResourceFormat::R16Uint, 1, 1, vertexData.data(), Resource::BindFlags::ShaderResource);

        // This is the preload for the square pattern
        /*
        std::array<glm::uint16, 17> pattern = { 0, 7, 15, 23, 31, 39, 47, 55, 63, 71, 79, 87, 95, 103, 111, 119, 126 };
        for (int y = 0; y <= 16; y++)
        {
            int mY = pattern[y];
            for (int x = 0; x <= 16; x++)
            {
                int mX = pattern[x];
                glm::uint index = (mY + 4) * 128 + mX + 4;
                if (x == 16) index -= 4;
                if (y == 16) index -= 4 * 128;

                unsigned int packValue = ((mX + 1) << 9) + ((mY + 1) << 1);
                vertexData[index] = packValue;
            }
        }
        */

        // kante
        for (uint i = 1; i < 128; i += 2)
        {
            vertexData[(1 << 7) + i] = (1 << 7) + i;
            vertexData[(127 << 7) + i] = (127 << 7) + i;
            vertexData[(i << 7) + 1] = (i << 7) + 1;
            vertexData[(i << 7) + 127] = (i << 7) + 127;
        }

        for (uint y = 9; y < 128; y += 8)
        {
            for (uint x = 9; x < 128; x += 8)
            {
                vertexData[(y << 7) + x] = (y << 7) + x;
            }
        }

        for (uint i = 5; i < 128; i += 4)
        {
            vertexData[(5 << 7) + i] = (5 << 7) + i;
            vertexData[(125 << 7) + i] = (125 << 7) + i;
            vertexData[(i << 7) + 5] = (i << 7) + 5;
            vertexData[(i << 7) + 125] = (i << 7) + 125;
        }
        split.vertex_preload = Texture::create2D(tile_numPixels / 2, tile_numPixels / 2, Falcor::ResourceFormat::R16Uint, 1, 1, vertexData.data(), Resource::BindFlags::ShaderResource);
    }


    {
        split.buffer_tileCenters = Buffer::createStructured(sizeof(float4), numTiles);
        //split.buffer_tileCenter_readback = Buffer::create(sizeof(float4) * numTiles, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::Read);
        split.buffer_tileCenter_readback = Buffer::create(sizeof(float4) * numTiles, Resource::BindFlags::None, Buffer::CpuAccess::Read);
        //split.tileCenters.resize(numTiles);
    }


    {
        // u16 noise texture
        std::mt19937 generator(2);
        std::uniform_int_distribution<> distribution(0, 65535);
        std::vector<unsigned short> random(256 * 256);
        for (int i = 0; i < 256 * 256; i++)
        {
            random[i] = distribution(generator);    // FIXME for 12 ecotopes
        }
        split.noise_u16 = Texture::create2D(256, 256, ResourceFormat::R16Uint, 1, 1, random.data());

        // frame buffer
        Fbo::Desc desc;
        desc.setDepthStencilTarget(ResourceFormat::D24UnormS8);			// keep for now, not sure why, but maybe usefult for cuts
        desc.setColorTarget(0u, ResourceFormat::R32Float, true);		// elevation
        desc.setColorTarget(1u, ResourceFormat::R11G11B10Float, true);	// albedo
        desc.setColorTarget(2u, ResourceFormat::R11G11B10Float, true);	// pbr
        desc.setColorTarget(3u, ResourceFormat::R11G11B10Float, true);	// alpha
        desc.setColorTarget(4u, ResourceFormat::RGBA8Unorm, true);		// ecotopes  ? R11G11B10Float 
        desc.setColorTarget(5u, ResourceFormat::RGBA8Unorm, true);		// ecotopes
        desc.setColorTarget(6u, ResourceFormat::RGBA8Unorm, true);		// ecotopes
        desc.setColorTarget(7u, ResourceFormat::RGBA8Unorm, true);		// ecotopes
        split.tileFbo = Fbo::create2D(tile_numPixels, tile_numPixels, desc, 1, 8);
        split.bakeFbo = Fbo::create2D(split.bakeSize, split.bakeSize, desc, 1, 8);
        bake.copy_texture = Texture::create2D(split.bakeSize, split.bakeSize, Falcor::ResourceFormat::R32Float, 1, 1, nullptr, Resource::BindFlags::None);

        Fbo::Desc descVegBake;
        desc.setDepthStencilTarget(ResourceFormat::D24UnormS8);			// keep for now, not sure why, but maybe usefult for cuts
        desc.setColorTarget(0u, ResourceFormat::RGBA8Unorm, true);		// albedo
        desc.setColorTarget(1u, ResourceFormat::RGBA8Unorm, true);	    // normal
        desc.setColorTarget(2u, ResourceFormat::R11G11B10Float, true);	// pbr
        desc.setColorTarget(3u, ResourceFormat::R11G11B10Float, true);	// extra
        bakeFbo_plants = Fbo::create2D(1024, 1024, desc, 1, 1);

        compressed_Normals_Array = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::R11G11B10Float, numTiles, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);	  // Now an array	  at 1024 tiles its 256 Mb , Fair bit but do-ablwe
        //compressed_Albedo_Array = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::BC6HU16, numTiles, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
        compressed_Albedo_Array = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::R11G11B10Float, numTiles, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
        compressed_PBR_Array = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::BC6HU16, numTiles, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
        height_Array = Texture::create2D(tile_numPixels, tile_numPixels, Falcor::ResourceFormat::R32Float, numTiles, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);	  // Now an array	  1024 tiles is 64 MB - really nice and small

        split.drawArgs_quads = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
        split.drawArgs_plants = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
        split.drawArgs_clippedloddedplants = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
        split.drawArgs_tiles = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
        split.dispatchArgs_plants = Buffer::createStructured(sizeof(t_DispatchArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);


        split.buffer_feedback = Buffer::createStructured(sizeof(GC_feedback), 1);
        split.buffer_feedback_read = Buffer::createStructured(sizeof(GC_feedback), 1, Resource::BindFlags::None, Buffer::CpuAccess::Read);

        split.buffer_tiles = Buffer::createStructured(sizeof(gpuTile), numTiles);
        split.buffer_tiles_readback = Buffer::createStructured(sizeof(gpuTile), numTiles, Resource::BindFlags::None, Buffer::CpuAccess::Read);
        split.buffer_instance_quads = Buffer::createStructured(sizeof(instance_PLANT), numTiles * numQuadsPerTile);
        split.buffer_instance_plants = Buffer::createStructured(sizeof(instance_PLANT), numTiles * numPlantsPerTile);
        split.buffer_clippedloddedplants = Buffer::createStructured(sizeof(xformed_PLANT), 1024 * 1024); //32 bytes

        split.buffer_lookup_terrain = Buffer::createStructured(sizeof(tileLookupStruct), 524288);   // enopugh for 32M tr  overkill FIXME - LOG
        split.buffer_lookup_quads = Buffer::createStructured(sizeof(instance_PLANT), 131072);     // enough for 8M far plants
        split.buffer_lookup_plants = Buffer::createStructured(sizeof(instance_PLANT), 131072);

        split.buffer_terrain = Buffer::createStructured(sizeof(Terrain_vertex), numVertPerTile * numTiles);

        terrainShader.load("Samples/Earthworks_4/hlsl/terrain/render_Tiles.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
        terrainShader.Vars()->setBuffer("tiles", split.buffer_tiles);
        terrainShader.Vars()->setBuffer("tileLookup", split.buffer_lookup_terrain);

        terrainShader.Vars()->setTexture("gAlbedoArray", compressed_Albedo_Array);
        terrainShader.Vars()->setTexture("gPBRArray", compressed_PBR_Array);
        terrainShader.Vars()->setTexture("gNormArray", compressed_Normals_Array);
        terrainShader.Vars()->setSampler("gSmpAniso", sampler_ClampAnisotropic);
        terrainShader.Vars()->setBuffer("VB", split.buffer_terrain);

        terrainShader.Vars()["PerFrameCB"]["gisOverlayStrength"] = gis_overlay.strenght;
        terrainShader.Vars()["PerFrameCB"]["redStrength"] = gis_overlay.redStrength;
        terrainShader.Vars()["PerFrameCB"]["redScale"] = gis_overlay.redScale;
        terrainShader.Vars()["PerFrameCB"]["redOffset"] = gis_overlay.redOffset;

        spriteTexture = Texture::createFromFile("F:/terrains/switserland_Steg/ecosystem/sprite_diff.DDS", true, true);
        spriteNormalsTexture = Texture::createFromFile("F:/terrains/switserland_Steg/ecosystem/sprite_norm.DDS", true, false);

        terrainSpiteShader.load("Samples/Earthworks_4/hlsl/terrain/render_tile_sprite.hlsl", "vsMain", "psMain", Vao::Topology::PointList, "gsMain");
        terrainSpiteShader.Vars()->setBuffer("tiles", split.buffer_tiles);
        terrainSpiteShader.Vars()->setBuffer("tileLookup", split.buffer_lookup_quads);
        terrainSpiteShader.Vars()->setBuffer("instanceBuffer", split.buffer_instance_quads);        // WHY BOTH
        terrainSpiteShader.Vars()->setSampler("gSampler", sampler_ClampAnisotropic);
        terrainSpiteShader.Vars()->setSampler("gSmpLinearClamp", sampler_Clamp);
        terrainSpiteShader.Vars()->setTexture("gAlbedo", spriteTexture);
        terrainSpiteShader.Vars()->setTexture("gNorm", spriteNormalsTexture);














        //shadowEdges.load();



        std::mt19937 G(12);
        std::uniform_real_distribution<> D(-1.f, 1.f);
        std::uniform_real_distribution<> D2(0.7f, 1.4f);
        ribbonData = Buffer::createStructured(sizeof(unsigned int) * 6, 1024 * 1024 * 10); // just a nice amount for now

        _plantMaterial::static_materials_veg.sb_vegetation_Materials = Buffer::createStructured(sizeof(sprite_material), 1024 * 8);      // just a lot


        /*
        Texture::SharedPtr tex = Texture::createFromFile("F:\\terrains\\_resources\\textures\\bark\\Oak1_albedo.dds", false, true);
        ribbonTextures.emplace_back(tex);
        tex = Texture::createFromFile("F:\\terrains\\_resources\\textures\\bark\\Oak1_sprite_normal.dds", false, false);
        ribbonTextures.emplace_back(tex);

        tex = Texture::createFromFile("F:\\terrains\\_resources\\textures\\twigs\\dandelion_leaf1_albedo.dds", false, true);
        ribbonTextures.emplace_back(tex);
        tex = Texture::createFromFile("F:\\terrains\\_resources\\textures\\twigs\\dandelion_leaf1_normal.dds", false, false);
        ribbonTextures.emplace_back(tex);
        */

        ribbonShader.load("Samples/Earthworks_4/hlsl/terrain/render_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
        ribbonShader.Vars()->setBuffer("instanceBuffer", ribbonData);
        ribbonShader.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
        ribbonShader.Vars()->setBuffer("instances", split.buffer_clippedloddedplants);
        ribbonShader.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX

        ribbonShader_Bake.add("_BAKE", "");
        ribbonShader_Bake.load("Samples/Earthworks_4/hlsl/terrain/render_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
        ribbonShader_Bake.Vars()->setBuffer("instanceBuffer", ribbonData);
        ribbonShader_Bake.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
        ribbonShader_Bake.Vars()->setBuffer("instances", split.buffer_clippedloddedplants);
        ribbonShader_Bake.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX

        compute_bakeFloodfill.load("Samples/Earthworks_4/hlsl/terrain/compute_bakeFloodfill.hlsl");





        triangleData = Buffer::createStructured(sizeof(triangleVertex), 16384); // just a nice amount for now

        triangleShader.load("Samples/Earthworks_4/hlsl/terrain/render_triangles.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
        triangleShader.Vars()->setBuffer("instanceBuffer", triangleData);        // WHY BOTH
        triangleShader.Vars()->setBuffer("instances", split.buffer_clippedloddedplants);
        triangleShader.Vars()->setSampler("gSampler", sampler_ClampAnisotropic);

        vegetation.skyTexture = Texture::createFromFile("F:\\alps_bc.dds", false, true);
        vegetation.envTexture = Texture::createFromFile("F:\\alps_IR_bc.dds", false, true);
        triangleShader.Vars()->setTexture("gSky", vegetation.skyTexture);
        ribbonShader.Vars()->setTexture("gEnv", vegetation.envTexture);
        ribbonShader_Bake.Vars()->setTexture("gEnv", vegetation.envTexture);

        // Grass from disk ###########################################################################################################

        unsigned int flags =
            aiProcess_FlipUVs |
            aiProcess_Triangulate |
            aiProcess_PreTransformVertices |
            //aiProcess_JoinIdenticalVertices |
            aiProcess_GenBoundingBoxes;


        triangleVertex testribbonsFile[50 * 128];
        memset(testribbonsFile, 0, 50 * 128 * sizeof(triangleVertex));
        uint vertCount = 0;
        Assimp::Importer importer;
        const aiScene* scene = nullptr;
        char name[256];

        //for (int F = 1; F <= 16; F++)
        {
            //sprintf(name, "F:\\texturesDotCom\\wim\\blades\\%d.fbx", F);
            //sprintf(name, "F:\\terrains\\_resources\\textures_dot_com\\TexturesCom_3dplant_Nettle\\Individual_seeds\\A_256.fbx");
            sprintf(name, "F:\\terrains\\_resources\\cube.fbx");

            scene = importer.ReadFile(name, flags);
            if (scene)
            {
                aiMesh* M = scene->mMeshes[0];
                uint numSegments = M->mNumFaces;
                for (uint j = 0; j < numSegments; j++)
                {
                    aiFace face = M->mFaces[j];
                    for (int idx = 0; idx < 3; idx++)
                    {
                        aiVector3D V = M->mVertices[face.mIndices[idx]];
                        aiVector3D N = M->mNormals[face.mIndices[idx]];
                        aiVector3D U = M->mTextureCoords[0][face.mIndices[idx]];
                        testribbonsFile[vertCount].pos = float3(V.x, V.y, V.z) * 0.01f;
                        testribbonsFile[vertCount].norm = float3(N.x, N.y, N.z);
                        testribbonsFile[vertCount].u = U.x;
                        testribbonsFile[vertCount].v = U.y;
                        vertCount++;
                    }
                }
            }
        }


        triangleData->setBlob(testribbonsFile, 0, 50 * 128 * sizeof(triangleVertex));




        compute_TerrainUnderMouse.load("Samples/Earthworks_4/hlsl/terrain/compute_terrain_under_mouse.hlsl");
        compute_TerrainUnderMouse.Vars()->setSampler("gSampler", sampler_Clamp);
        compute_TerrainUnderMouse.Vars()->setTexture("gHeight", height_Array);
        compute_TerrainUnderMouse.Vars()->setBuffer("tiles", split.buffer_tiles);
        compute_TerrainUnderMouse.Vars()->setBuffer("groundcover_feedback", split.buffer_feedback);

        // clear
        split.compute_tileClear.load("Samples/Earthworks_4/hlsl/terrain/compute_tileClear.hlsl");
        split.compute_tileClear.Vars()->setBuffer("feedback", split.buffer_feedback);
        split.compute_tileClear.Vars()->setBuffer("DrawArgs_Terrain", split.drawArgs_tiles);
        split.compute_tileClear.Vars()->setBuffer("DrawArgs_Quads", split.drawArgs_quads);
        split.compute_tileClear.Vars()->setBuffer("DrawArgs_Plants", split.drawArgs_plants);
        split.compute_tileClear.Vars()->setBuffer("DrawArgs_ClippedLoddedPlants", split.drawArgs_clippedloddedplants);
        split.compute_tileClear.Vars()->setBuffer("DispatchArgs_Plants", split.dispatchArgs_plants);


        // plants clip lod animate
        split.compute_clipLodAnimatePlants.load("Samples/Earthworks_4/hlsl/terrain/compute_clipLodAnimatePlants.hlsl");
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("tileLookup", split.buffer_lookup_plants);
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("plantBuffer", split.buffer_instance_plants);
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("output", split.buffer_clippedloddedplants);
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("drawArgs_Plants", split.drawArgs_clippedloddedplants);
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("feedback", split.buffer_feedback);

        // split merge
        split.compute_tileSplitMerge.load("Samples/Earthworks_4/hlsl/terrain/compute_tileSplitMerge.hlsl");
        split.compute_tileSplitMerge.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileSplitMerge.Vars()->setBuffer("feedback", split.buffer_feedback);

        // generate
        split.compute_tileGenerate.load("Samples/Earthworks_4/hlsl/terrain/compute_tileGenerate.hlsl");
        split.compute_tileGenerate.Vars()->setBuffer("quad_instance", split.buffer_instance_quads);
        split.compute_tileGenerate.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileGenerate.Vars()->setTexture("gNoise", split.noise_u16);
        split.compute_tileGenerate.Vars()->setTexture("gHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileGenerate.Vars()->setTexture("gEct1", split.tileFbo->getColorTexture(4));
        split.compute_tileGenerate.Vars()->setTexture("gEct2", split.tileFbo->getColorTexture(5));
        split.compute_tileGenerate.Vars()->setTexture("gEct3", split.tileFbo->getColorTexture(6));
        split.compute_tileGenerate.Vars()->setTexture("gEct4", split.tileFbo->getColorTexture(7));

        // passthrough
        split.compute_tilePassthrough.load("Samples/Earthworks_4/hlsl/terrain/compute_tilePassthrough.hlsl");
        split.compute_tilePassthrough.Vars()->setBuffer("quad_instance", split.buffer_instance_quads);
        split.compute_tilePassthrough.Vars()->setBuffer("plant_instance", split.buffer_instance_plants);
        split.compute_tilePassthrough.Vars()->setBuffer("feedback", split.buffer_feedback);
        split.compute_tilePassthrough.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tilePassthrough.Vars()->setTexture("gHgt", split.tileFbo->getColorTexture(0));
        split.compute_tilePassthrough.Vars()->setTexture("gNoise", split.noise_u16);

        // build lookup
        split.compute_tileBuildLookup.load("Samples/Earthworks_4/hlsl/terrain/compute_tileBuildLookup.hlsl");
        split.compute_tileBuildLookup.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileBuildLookup.Vars()->setBuffer("tileLookup", split.buffer_lookup_quads);
        split.compute_tileBuildLookup.Vars()->setBuffer("plantLookup", split.buffer_lookup_plants);
        split.compute_tileBuildLookup.Vars()->setBuffer("terrainLookup", split.buffer_lookup_terrain);
        split.compute_tileBuildLookup.Vars()->setBuffer("DrawArgs_Quads", split.drawArgs_quads);
        split.compute_tileBuildLookup.Vars()->setBuffer("DrawArgs_Plants", split.drawArgs_plants);
        split.compute_tileBuildLookup.Vars()->setBuffer("DrawArgs_Terrain", split.drawArgs_tiles);
        split.compute_tileBuildLookup.Vars()->setBuffer("feedback", split.buffer_feedback);
        split.compute_tileBuildLookup.Vars()->setBuffer("DispatchArgs_Plants", split.dispatchArgs_plants);

        // bicubic
        split.compute_tileBicubic.load("Samples/Earthworks_4/hlsl/terrain/compute_tileBicubic.hlsl");
        split.compute_tileBicubic.Vars()->setSampler("linearSampler", sampler_Clamp);
        split.compute_tileBicubic.Vars()->setTexture("gOutput", split.tileFbo->getColorTexture(0));
        split.compute_tileBicubic.Vars()->setTexture("gDebug", split.debug_texture);
        //split.compute_tileBicubic.Vars()->setTexture("gOuthgt_TEMPTILLTR", split.tileFbo->getColorTexture(0));

        // ecotopes
        split.compute_tileEcotopes.load("Samples/Earthworks_4/hlsl/terrain/compute_tileEcotopes.hlsl");
        split.compute_tileEcotopes.Vars()->setSampler("linearSampler", sampler_Clamp);
        split.compute_tileEcotopes.Vars()->setTexture("gHeight", split.tileFbo->getColorTexture(0));
        split.compute_tileEcotopes.Vars()->setTexture("gAlbedo", split.tileFbo->getColorTexture(1));
        split.compute_tileEcotopes.Vars()->setTexture("gInPermanence", split.tileFbo->getColorTexture(3));
        split.compute_tileEcotopes.Vars()->setTexture("gInEct_0", split.tileFbo->getColorTexture(4));
        split.compute_tileEcotopes.Vars()->setTexture("gInEct_1", split.tileFbo->getColorTexture(5));
        split.compute_tileEcotopes.Vars()->setTexture("gInEct_2", split.tileFbo->getColorTexture(6));
        split.compute_tileEcotopes.Vars()->setTexture("gInEct_3", split.tileFbo->getColorTexture(7));
        split.compute_tileEcotopes.Vars()->setTexture("gNoise", split.noise_u16);
        split.compute_tileEcotopes.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileEcotopes.Vars()->setBuffer("quad_instance", split.buffer_instance_quads);
        split.compute_tileEcotopes.Vars()->setBuffer("feedback", split.buffer_feedback);


        // normals
        split.compute_tileNormals.load("Samples/Earthworks_4/hlsl/terrain/compute_tileNormals.hlsl");
        split.compute_tileNormals.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileNormals.Vars()->setTexture("gOutNormals", split.normals_texture);
        split.compute_tileNormals.Vars()->setTexture("gOutput", split.debug_texture);
        split.compute_tileNormals.Vars()->setBuffer("tiles", split.buffer_tiles);

        // vertices
        split.compute_tileVerticis.load("Samples/Earthworks_4/hlsl/terrain/compute_tileVertices.hlsl");
        split.compute_tileVerticis.Vars()->setSampler("linearSampler", sampler_Clamp);
        split.compute_tileVerticis.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileVerticis.Vars()->setTexture("gOutVerts", split.vertex_A_texture);
        split.compute_tileVerticis.Vars()->setTexture("gDebug", split.debug_texture);
        split.compute_tileVerticis.Vars()->setBuffer("tileCenters", split.buffer_tileCenters);
        split.compute_tileVerticis.Vars()->setBuffer("tiles", split.buffer_tiles);

        // jumpflood
        // It may even be faster to set this up twice and hop between the two
        split.compute_tileJumpFlood.load("Samples/Earthworks_4/hlsl/terrain/compute_tileJumpFlood.hlsl");
        split.compute_tileJumpFlood.Vars()->setTexture("gDebug", split.debug_texture);

        // delaunay
        split.compute_tileDelaunay.load("Samples/Earthworks_4/hlsl/terrain/compute_tileDelaunay.hlsl");
        split.compute_tileDelaunay.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileDelaunay.Vars()->setTexture("gInVerts", split.vertex_B_texture);
        split.compute_tileDelaunay.Vars()->setBuffer("VB", split.buffer_terrain);
        split.compute_tileDelaunay.Vars()->setBuffer("tiles", split.buffer_tiles);
        //split.compute_tileDelaunay.Vars()->setBuffer("tileDrawargsArray", split.drawArgs_tiles);

        // elevation mipmap
        /*
        split.compute_tileElevationMipmap.load("Samples/Earthworks_4/hlsl/terrain/compute_tileElevationMipmap.hlsl");
        split.compute_tileElevationMipmap.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileElevationMipmap.Vars()->setTexture("gDebug", split.debug_texture);
        const auto& mipmapReflector = split.compute_tileElevationMipmap.Reflector()->getDefaultParameterBlock();
        ParameterBlock::BindLocation bind_mip1 = mipmapReflector->getResourceBinding("gMip1");
        ParameterBlock::BindLocation bind_mip2 = mipmapReflector->getResourceBinding("gMip2");
        ParameterBlock::BindLocation bind_mip3 = mipmapReflector->getResourceBinding("gMip3");
        split.compute_tileElevationMipmap.Vars()->setUav(bind_mip1, split.tileFbo->getColorTexture(0)->getUAV(1));
        split.compute_tileElevationMipmap.Vars()->setUav(bind_mip2, split.tileFbo->getColorTexture(0)->getUAV(2));
        split.compute_tileElevationMipmap.Vars()->setUav(bind_mip3, split.tileFbo->getColorTexture(0)->getUAV(3));
        */
        // BC6H compressor
        split.bc6h_texture = Texture::create2D(tile_numPixels / 4, tile_numPixels / 4, Falcor::ResourceFormat::RGBA32Uint, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess);
        split.compute_bc6h.load("Samples/Earthworks_4/hlsl/terrain/compute_bc6h.hlsl");
        split.compute_bc6h.Vars()->setTexture("gOutput", split.bc6h_texture);

    }

    

    allocateTiles(numTiles);

    elevationCache.resize(45);
    loadElevationHash(pRenderContext);

    init_TopdownRender();

    mSpriteRenderer.onLoad();

    mEcosystem.terrainSize = settings.size;
    mEcosystem.load("F:/terrains/switserland_Steg/ecosystem/steg.ecosystem");    // FIXME MOVE To lastFILE
    terrafectorEditorMaterial::rootFolder = settings.dirResource + "/";
    terrafectors.loadPath(settings.dirRoot + "/terrafectors", settings.dirRoot + "/bake", false);
    mRoadNetwork.rootPath = settings.dirRoot + "/";



    AirSim.setup();
    paraBuilder.buildCp();
    paraBuilder.buildWing();
    paraBuilder.buildLines();
    paraBuilder.solveLines();
    paraBuilder.generateLines();

    paraBuilder.builWingConstraints();
    //paraBuilder.builLineConstraints();



    paraBuilder.visualsPack(paraRuntime.ribbon, paraRuntime.packedRibbons, paraRuntime.ribbonCount, paraRuntime.changed);

    paraRuntime.setup(paraBuilder.x, paraBuilder.w, paraBuilder.cdA, paraBuilder.cross, paraBuilder.spanSize, paraBuilder.chordSize, paraBuilder.constraints);
    paraRuntime.Cp = paraBuilder.Cp;
    memcpy(paraRuntime.CPbrakes, paraBuilder.CPbrakes, sizeof(float) * 6 * 11 * 25);
    //paraRuntime.CPbrakes = paraBuilder.CPbrakes;
    paraRuntime.linesLeft = paraBuilder.linesLeft;
    paraRuntime.linesRight = paraBuilder.linesRight;


    //_plantMaterial::static_materials_veg.materialVector.emplace_back();
    //_plantMaterial::static_materials_veg.materialVector[0].fullPath = terrafectorEditorMaterial::rootFolder + "//test";
    //_plantMaterial::static_materials_veg.selectedMaterial = 0;

    //_plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + "textures\\bark\\Oak1_albedo.dds", true);
    //_plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + "textures\\bark\\Oak1_sprite_normal.dds", false);
    //_plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + "textures\\twigs\\dandelion_leaf1_albedo.dds", true);
    //_plantMaterial::static_materials_veg.find_insert_texture(terrafectorEditorMaterial::rootFolder + "textures\\twigs\\dandelion_leaf1_normal.dds", false);
}


void terrainManager::init_TopdownRender()
{

    terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials = Buffer::createStructured(sizeof(TF_material), 2048); // FIXME hardcoded
    split.shader_spline3D.load("Samples/Earthworks_4/hlsl/terrain/render_spline.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
    split.shader_spline3D.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);
    split.shader_spline3D.Vars()->setSampler("gSmpLinear", sampler_Trilinear);
    //split.shader_spline3D.Program()->getReflector()
    split.shader_splineTerrafector.load("Samples/Earthworks_4/hlsl/terrain/render_splineTerrafector.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
    //split.shader_splineTerrafector.State()->setFbo(split.tileFbo);
    split.shader_splineTerrafector.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);


    // mesh terrafector shader
    split.shader_meshTerrafector.load("Samples/Earthworks_4/hlsl/terrain/render_meshTerrafector.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
    split.shader_meshTerrafector.Vars()->setSampler("gSmpLinear", sampler_Trilinear);
    //split.shader_meshTerrafector.Vars()["PerFrameCB"]["gConstColor"] = false;
    split.shader_meshTerrafector.State()->setFbo(split.tileFbo);
    split.shader_meshTerrafector.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);

    DepthStencilState::Desc depthDesc;
    depthDesc.setDepthEnabled(true);
    depthDesc.setDepthWriteMask(false);
    depthDesc.setStencilEnabled(false);

    depthDesc.setDepthFunc(DepthStencilState::Func::Greater);
    split.depthstateFuther = DepthStencilState::create(depthDesc);

    depthDesc.setDepthFunc(DepthStencilState::Func::LessEqual);
    split.depthstateCloser = DepthStencilState::create(depthDesc);

    depthDesc.setDepthFunc(DepthStencilState::Func::Always);
    split.depthstateAll = DepthStencilState::create(depthDesc);

    RasterizerState::Desc rsDesc;
    rsDesc.setFillMode(RasterizerState::FillMode::Solid).setCullMode(RasterizerState::CullMode::None);
    split.rasterstateSplines = RasterizerState::create(rsDesc);

    BlendState::Desc blendDesc;
    blendDesc.setRtBlend(0, true);
    blendDesc.setRtParams(0, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::Zero, BlendState::BlendFunc::Zero);
    split.blendstateSplines = BlendState::create(blendDesc);

    blendDesc.setIndependentBlend(true);
    for (int i = 0; i < 8; i++)
    {
        // clear all
        blendDesc.setRenderTargetWriteMask(i, true, true, true, true);
        blendDesc.setRtBlend(i, true);
        blendDesc.setRtParams(i, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha);
    }

    blendDesc.setRtParams(0, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::One, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::One, BlendState::BlendFunc::OneMinusSrcAlpha);
    split.blendstateRoadsCombined = BlendState::create(blendDesc);

    blendDesc.setIndependentBlend(true);
    for (int i = 0; i < 8; i++)
    {
        // clear all
        blendDesc.setRenderTargetWriteMask(i, true, true, true, true);
        blendDesc.setRtBlend(i, true);
        blendDesc.setRtParams(i, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::SrcAlphaSaturate, BlendState::BlendFunc::One);
    }
    blendDesc.setRtParams(3, BlendState::BlendOp::Add, BlendState::BlendOp::Add, BlendState::BlendFunc::SrcAlpha, BlendState::BlendFunc::OneMinusSrcAlpha, BlendState::BlendFunc::SrcAlphaSaturate, BlendState::BlendFunc::One);
    blendstateVegBake = BlendState::create(blendDesc);

    splines.bezierData = Buffer::createStructured(sizeof(cubicDouble), splines.maxBezier);
    splines.indexData = Buffer::createStructured(sizeof(bezierLayer), splines.maxIndex);
    splines.indexDataBakeOnly = Buffer::createStructured(sizeof(bezierLayer), splines.maxIndex);
    splines.indexData_LOD4 = Buffer::createStructured(sizeof(bezierLayer), splines.maxIndex * 2); //*2 for safety
    splines.indexData_LOD6 = Buffer::createStructured(sizeof(bezierLayer), splines.maxIndex * 2); //*2 for safety
    splines.indexData_LOD8 = Buffer::createStructured(sizeof(bezierLayer), splines.maxIndex * 2); //*2 for safety   // 8Mb for now 1M points 8bytes per bez

    splines.dynamic_bezierData = Buffer::createStructured(sizeof(cubicDouble), splines.maxDynamicBezier);
    splines.dynamic_indexData = Buffer::createStructured(sizeof(bezierLayer), splines.maxDynamicIndex);
}


void terrainManager::allocateTiles(uint numTiles)
{
    quadtree_tile tile;

    m_tiles.clear();
    m_tiles.reserve(numTiles);
    split.cpuTiles.clear();
    split.cpuTiles.resize(numTiles);


    for (uint i = 0; i < numTiles; i++)
    {
        tile.init(i);
        m_tiles.push_back(tile);
    }

    reset();
}

void terrainManager::reset(bool _fullReset)
{
    fullResetDoNotRender = _fullReset;

    m_free.clear();
    m_used.clear();

    for (uint i = 0; i < m_tiles.size(); i++)
    {
        m_free.push_back(&m_tiles[i]);
    }

    quadtree_tile* root = m_free.front();
    m_free.pop_front();
    root->set(0, 0, 0, settings.size, float4(-0.5f * settings.size, 0, -0.5f * settings.size, 0.0f), nullptr);

    m_used.push_back(root);

    // FIXME this could be done faster by changing the cache compute shader to alwas clear if its Tile zero thats caching
    // or add a special shader for that
    if (split.buffer_tiles) {
        for (uint i = 0; i < m_tiles.size(); i++)
        {
            split.cpuTiles[i].lod = 0;
            split.cpuTiles[i].Y = 0;
            split.cpuTiles[i].X = 0;
            split.cpuTiles[i].flags = 0;

            split.cpuTiles[i].origin = float3(0, 0, 0);
            split.cpuTiles[i].scale_1024 = 0;

            split.cpuTiles[i].numQuads = 0;
            split.cpuTiles[i].numPlants = 0;
            split.cpuTiles[i].numTriangles = 0;
            split.cpuTiles[i].numVerticis = 0;
        }
        split.buffer_tiles->setBlob(&split.cpuTiles, 0, m_tiles.size() * sizeof(gpuTile));
        // lets hope for now the upload is now automatic
        //split.buffer_tiles->>uploadToGPU();
    }
}

uint32_t getHashFromTileCoords(unsigned int lod, unsigned int y, unsigned int x) {
    return (lod << 28) + (y << 14) + (x);
}

void terrainManager::loadElevationHash(RenderContext* pRenderContext)
{
    std::string fullpath = settings.dirRoot + "/elevations.txt";
    elevationTileHashmap.clear();
    reset();


    FILE* pFileHgt = fopen(fullpath.c_str(), "r");
    if (pFileHgt)
    {
        heightMap map;
        int texSize;
        char filename[256];
        int items = 1;
        do {
            items = fscanf(pFileHgt, "%d %d %d %d %f %f %f %f %f %s\n", &map.lod, &map.y, &map.x, &texSize, &map.origin.x, &map.origin.y, &map.size, &map.hgt_offset, &map.hgt_scale, filename);
            if (items > 0)
            {
                uint32_t hash = getHashFromTileCoords(map.lod, map.y, map.x);

                fullpath = settings.dirRoot + "/" + filename;
                if (map.lod == 0)
                {
                    std::vector<float> data;
                    data.resize(texSize * texSize * 2);


                    FILE* pData = fopen(fullpath.c_str(), "rb");
                    if (pData)
                    {
                        fread(data.data(), sizeof(float), texSize * texSize, pData);
                        fclose(pData);
                    }

                    split.rootElevation = Texture::create2D(texSize, texSize, Falcor::ResourceFormat::R32Float, 1, 8, data.data(), Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget);
                    split.rootElevation->generateMips(pRenderContext);
                    map.hgt_offset = 0;
                    map.hgt_scale = 1;
                    elevationTileHashmap[hash] = map;
                }
                else
                {
                    map.filename = fullpath;
                    elevationTileHashmap[hash] = map;
                }
            }
        } while (items > 0);

        fclose(pFileHgt);
    }

}

void terrainManager::onShutdown()
{
}


std::string blockFromPositionB(glm::vec3 _pos)
{
    float halfsize = ecotopeSystem::terrainSize / 2.f;
    float blocksize = ecotopeSystem::terrainSize / 16.f;

    uint y = (uint)floor((_pos.z + halfsize) / blocksize);
    uint x = (uint)floor((_pos.x + halfsize) / blocksize);
    std::string answer = char(65 + x) + std::to_string(y);
    return answer;
}



void replaceAllterrain(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}



void terrainManager::onGuiRender(Gui* _gui)
{
    if (!showGUI) return;
    if (requestPopupSettings) {
        ImGui::OpenPopup("settings");
        requestPopupSettings = false;
    }
    if (ImGui::BeginPopup("settings"))      // modal
    {
        settings.renderGui(_gui);
        ImGui::EndPopup();
    }


    if (requestPopupTree) {
        ImGui::OpenPopup("tree");
        requestPopupTree = false;
    }
    if (ImGui::BeginPopup("tree"))      // modal
    {
        groveTree.renderGui(_gui);
        ImGui::EndPopup();
    }


    if (requestPopupDebug) {
        ImGui::OpenPopup("debug");
        requestPopupDebug = false;
    }
    if (ImGui::BeginPopup("debug"))      // modal
    {
        ImGui::Text("%d", m_used.size());
        ImGui::EndPopup();
    }

    Gui::Window rightPanel(_gui, "##rightPanel", { 200, 200 }, { 100, 100 });
    {
        ImGui::PushFont(_gui->getFont("roboto_20"));

        ImGui::PushItemWidth(ImGui::GetWindowWidth() - 20);
        if (ImGui::Combo("###modeSelector", &terrainMode, "Vegetation\0Ecotope\0Terrafector\0Bezier road network\0Glider Builder\0")) { ; }
        ImGui::PopItemWidth();
        ImGui::NewLine();

        auto& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_Button] = ImVec4(0.03f, 0.03f, 0.3f, 0.5f);

        switch (terrainMode)
        {
        case 0:
        {
            ImGui::Text("test");
            ImGui::DragInt("# instances", &ribbonInstanceNumber, 1, 1, 1000);
            ImGui::DragFloat("spacing", &ribbonSpacing, 0.01f, 0.1f, 10);
            ImGui::Checkbox("from extents", &spacingFromExtents);

            ImGui::Text("%d inst x %d x 2 = %3.1f M tris", ribbonInstanceNumber * ribbonInstanceNumber, groveTree.numBranchRibbons, ribbonInstanceNumber * ribbonInstanceNumber * groveTree.numBranchRibbons * 2.0f / 1000000.0f);
            groveTree.renderGui(_gui);
        }
        break;

        case 1:
        {
            mEcosystem.renderGUI(_gui);
            if (ImGui::BeginPopupContextWindow(false))
            {
                if (ImGui::Selectable("New Ecotope")) { mEcosystem.addEcotope(); }
                if (ImGui::Selectable("Load")) { mEcosystem.load(); }
                if (ImGui::Selectable("Save")) { mEcosystem.save(); }
                ImGui::EndPopup();

            }
        }
        break;

        case 2:
        {
            //terrafectors.renderGui(_gui);
            ImGui::NewLine();
            //roadMaterialCache::getInstance().renderGui(_gui);
        }
        break;

        case 3:
        {
            style.Colors[ImGuiCol_Button] = ImVec4(0.03f, 0.03f, 0.3f, 0.5f);
            mRoadNetwork.renderGUI(_gui);


            //if (ImGui::Button("bake - EVO", ImVec2(W, 0))) { bake(false); }
            //if (ImGui::Button("bake - MAX", ImVec2(W, 0))) { bake(true); }

            if (ImGui::Button("bezier -> lod4")) { bezierRoadstoLOD(4); }


            if (ImGui::Checkbox("show baked", &bSplineAsTerrafector)) { reset(true); }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("'b'");
            ImGui::Checkbox("show road icons", &showRoadOverlay);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("'n'");
            ImGui::Checkbox("show road splines", &showRoadSpline);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("'m'");

            ImGui::NewLine();

            auto& style = ImGui::GetStyle();
            style.ButtonTextAlign = ImVec2(0.0, 0.5);
            if (ImGui::DragFloat("overlay", &gis_overlay.strenght, 0.01f, 0, 1, "%1.2f strength")) {
                terrainShader.Vars()["PerFrameCB"]["gisOverlayStrength"] = gis_overlay.strenght;
            }

            if (ImGui::DragFloat("redStrength", &gis_overlay.redStrength, 0.01f, 0, 1)) {
                terrainShader.Vars()["PerFrameCB"]["redStrength"] = gis_overlay.redStrength;
            }
            if (ImGui::DragFloat("redScale", &gis_overlay.redScale, 0.01f, 0, 100)) {
                terrainShader.Vars()["PerFrameCB"]["redScale"] = gis_overlay.redScale;
            }
            if (ImGui::DragFloat("redOffset", &gis_overlay.redOffset, 0.01f, 0, 1)) {
                terrainShader.Vars()["PerFrameCB"]["redOffset"] = gis_overlay.redOffset;
            }

            if (ImGui::DragFloat("overlay Strength", &gis_overlay.terrafectorOverlayStrength, 0.01f, 0, 1)) {
                reset(true);
            }
            if (ImGui::DragFloat("roads alpha", &gis_overlay.splineOverlayStrength, 0.01f, 0, 1));

            ImGui::Checkbox("bakeBakeOnlyData", &gis_overlay.bakeBakeOnlyData);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Disabeling this will produce a WRONG result\nIt is only here for speed testing as it mimics EVO pipeline");





            ImGui::Separator();
            float W = ImGui::GetWindowWidth() - 5;
            ImGui::DragInt("# of splits", &bake.roadMaxSplits, 1, 8, 128);
            if (ImGui::Button("roads -> fbx", ImVec2(W, 0))) { mRoadNetwork.exportRoads(bake.roadMaxSplits); }

            if (ImGui::Button("bridges -> fbx", ImVec2(W, 0))) { mRoadNetwork.exportBridges(); }
            ImGui::DragInt2("lod", &exportLodMin, 1, 0, 20);
            //ImGui::DragInt("lod-MAX", &exportLodMax, 1, 0, 20);
            if (exportLodMax < exportLodMin) exportLodMax = exportLodMin;
            if (ImGui::Button("grab frame to fbx", ImVec2(W, 0))) { sceneToMax(); }


            if (ImGui::Button("roads/materials -> EVO", ImVec2(W, 0))) {


                //mRoadNetwork.exportBinary();
                terrafectors.exportMaterialBinary(settings.dirRoot + "/bake", lastfile.EVO + "/");


                char command[512];
                sprintf(command, "attrib -r %s/Terrafectors/TextureList.gpu", (settings.dirExport).c_str());
                std::string sCmd = command;
                replaceAllterrain(sCmd, "/", "\\");
                system(sCmd.c_str());

                sprintf(command, "%s/bake/TextureList.gpu %s/Terrafectors", settings.dirRoot.c_str(), (settings.dirExport).c_str());
                sCmd = command;
                replaceAllterrain(sCmd, "/", "\\");
                sCmd = "copy / Y " + sCmd;
                system(sCmd.c_str());

                sprintf(command, "attrib -r %s/Terrafectors/Materials.gpu", (settings.dirExport).c_str());
                sCmd = command;
                replaceAllterrain(sCmd, "/", "\\");
                system(sCmd.c_str());

                sprintf(command, "%s/bake/Materials.gpu %s/Terrafectors", settings.dirRoot.c_str(), (settings.dirExport).c_str());
                sCmd = command;
                replaceAllterrain(sCmd, "/", "\\");
                sCmd = "copy / Y " + sCmd;
                system(sCmd.c_str());

                // terrafectors and roads as well, might become one file in future
                sprintf(command, "attrib -r %s/Terrafectors/terrafector*", (settings.dirExport).c_str());
                sCmd = command;
                replaceAllterrain(sCmd, "/", "\\");
                system(sCmd.c_str());

                sprintf(command, "%s/bake/terrafector* %s/Terrafectors", settings.dirRoot.c_str(), (settings.dirExport).c_str());
                sCmd = command;
                replaceAllterrain(sCmd, "/", "\\");
                sCmd = "copy / Y " + sCmd;
                system(sCmd.c_str());


                sprintf(command, "attrib -r %s/Terrafectors/roadbezier*", (settings.dirExport).c_str());
                sCmd = command;
                replaceAllterrain(sCmd, "/", "\\");
                system(sCmd.c_str());

                sprintf(command, "%s/bake/roadbezier* %s/Terrafectors", settings.dirRoot.c_str(), (settings.dirExport).c_str());
                sCmd = command;
                replaceAllterrain(sCmd, "/", "\\");
                sCmd = "copy / Y " + sCmd;
                system(sCmd.c_str());
            }


            ImGui::DragFloat("jp2 quality", &bake.quality, 0.0001f, 0.0001f, 0.01f, "%3.5f");
            if (ImGui::Button("bake - EVO", ImVec2(W, 0))) { bake_start(false); }
            if (ImGui::Button("bake - fakeVeg", ImVec2(W, 0))) { bake_start(true); }




            ImGui::NewLine();
            ImGui::Separator();
            ImGui::Text("Artificial Intelligence");


            ImGui::Checkbox("AI mode", &mRoadNetwork.AI_path_mode);
            if (mRoadNetwork.AI_path_mode) {
                if (ImGui::Button("clear Path")) { mRoadNetwork.ai_clearPath(); }
                ImGui::Separator();
                if (ImGui::Button("currentRoad to AI")) {
                    mRoadNetwork.currentRoadtoAI();
                    updateDynamicRoad(true);
                }
                if (ImGui::Button("export AI")) { mRoadNetwork.exportAI(); }
                if (ImGui::Button("export CSV")) { mRoadNetwork.exportAI_CSV(); }
                if (ImGui::Button("load AI")) { mRoadNetwork.loadAI(); }
                ImGui::Text("start - %3.2f, %3.2f, %3.2f", mRoadNetwork.pathBezier.startpos.x, mRoadNetwork.pathBezier.startpos.y, mRoadNetwork.pathBezier.startpos.z);
                ImGui::Text("end - %3.2f, %3.2f, %3.2f", mRoadNetwork.pathBezier.endpos.x, mRoadNetwork.pathBezier.endpos.y, mRoadNetwork.pathBezier.endpos.z);
                for (uint i = 0; i < mRoadNetwork.pathBezier.roads.size(); i++) {
                    ImGui::Text("%d, %d", mRoadNetwork.pathBezier.roads[i].roadIndex, (int)mRoadNetwork.pathBezier.roads[i].bForward);
                }

                ImGui::DragInt("kevRepeats", &mRoadNetwork.pathBezier.kevRepeats, 1, 1, 200);
                ImGui::DragFloat("kevOutScale", &mRoadNetwork.pathBezier.kevOutScale, 1, 0, 200);
                ImGui::DragFloat("kevInScale", &mRoadNetwork.pathBezier.kevInScale, 1, 0, 200);
                ImGui::DragInt("kevSmooth", &mRoadNetwork.pathBezier.kevSmooth, 1, 0, 100);
                ImGui::DragInt("kevAvsCnt", &mRoadNetwork.pathBezier.kevAvsCnt, 1, 0, 10);
                ImGui::DragInt("kecMaxCnt", &mRoadNetwork.pathBezier.kecMaxCnt, 1, 0, 20);
                ImGui::DragFloat("kevSampleMinSpacing", &mRoadNetwork.pathBezier.kevSampleMinSpacing, 1, 1, 20);

            }
        }
        break;

        case 4:
            ImGui::Text("Wing designer");
            //ParaGlider.renderGui(_gui);

            if (ImGui::Button("restart"))
            {
                paraRuntime.setup(paraBuilder.x, paraBuilder.w, paraBuilder.cdA, paraBuilder.cross, paraBuilder.spanSize, paraBuilder.chordSize, paraBuilder.constraints);
                //paraRuntime.Cp = paraBuilder.Cp;
                //paraRuntime.linesLeft = paraBuilder.linesLeft;
                //paraRuntime.linesRight = paraBuilder.linesRight;
            }

            paraRuntime.renderGui(_gui);
            break;

        }
        ImGui::PopFont();
    }
    rightPanel.release();

    switch (terrainMode)
    {
    case 0:
    {
        Gui::Window vegetationPanel(_gui, "Vegetation##tfPanel", { 900, 900 }, { 100, 100 });
        {
            //sdfzdfh
        }
        vegetationPanel.release();

        Gui::Window vegmatPanel(_gui, "Vegetation material##tfPanel", { 900, 900 }, { 100, 100 });
        {

            if (_plantMaterial::static_materials_veg.renderGuiSelect(_gui)) {
                reset(true);
            }

        }
        vegmatPanel.release();

        Gui::Window vegcachePanel(_gui, "Vegetation cache##tfPanel", { 900, 900 }, { 100, 100 });
        {
            _plantMaterial::static_materials_veg.renderGui(_gui, vegcachePanel);
        }
        vegcachePanel.release();


        Gui::Window vegtexPanel(_gui, "Vegetation textures##tfPanel", { 900, 900 }, { 100, 100 });
        {
            _plantMaterial::static_materials_veg.renderGuiTextures(_gui, vegtexPanel);
        }
        vegtexPanel.release();


    }
    break;
    case 1:
    {
        Gui::Window ecotopePanel(_gui, "Ecotope##tfPanel", { 900, 900 }, { 100, 100 });
        {
            mEcosystem.renderSelectedGUI(_gui);
        }
        ecotopePanel.release();
    }
    break;
    case 2:
    {
        /*
        Gui::Window tfPanel(_gui, "##tfPanel", { 900, 900 }, { 100, 100 });
        {
            ImGui::PushFont(_gui->getFont("roboto_20"));
            if (terrafectorEditorMaterial::static_materials.renderGuiSelect(_gui)) {
                reset(true);
            }
            ImGui::PopFont();
        }
        tfPanel.release();
        */
    }
    break;
    case 3:

        //ImGui::PushFont(_gui->getFont("roboto_20"));
        //_gui->setActiveFont("roboto_20");
    {
        Gui::Window tfPanel(_gui, "Material##tfPanel", { 900, 900 }, { 100, 100 });
        {

            if (terrafectorEditorMaterial::static_materials.renderGuiSelect(_gui)) {
                reset(true);
            }

        }
        tfPanel.release();




        Gui::Window terrafectorMaterialPanel(_gui, "Terrafector materials", { 900, 900 }, { 100, 100 });
        {
            terrafectorEditorMaterial::static_materials.renderGui(_gui, terrafectorMaterialPanel);
        }
        terrafectorMaterialPanel.release();

        Gui::Window roadMaterialPanel(_gui, "Road materials", { 900, 900 }, { 100, 100 });
        {
            roadMaterialCache::getInstance().renderGui(_gui, roadMaterialPanel);
        }
        roadMaterialPanel.release();

        Gui::Window terrafectorPanel(_gui, "Terrafectors", { 900, 900 }, { 100, 100 });
        {
            terrafectors.renderGui(_gui, terrafectorPanel);
        }
        terrafectorPanel.release();

        Gui::Window texturePanel(_gui, "Textures", { 900, 900 }, { 100, 100 });
        {
            terrafectorEditorMaterial::static_materials.renderGuiTextures(_gui, texturePanel);
            //terrafectors.renderGui(_gui, terrafectorPanel);
        }
        texturePanel.release();


        if (mRoadNetwork.currentIntersection || mRoadNetwork.currentRoad)
        {
            Gui::Window roadPanel(_gui, "Road##roadPanel", { 200, 200 }, { 100, 100 });
            {
                ImGui::PushFont(_gui->getFont("roboto_20"));
                static bool fullWidth = false;
                roadPanel.windowSize(220 + fullWidth * 730, 0);

                if (mRoadNetwork.currentIntersection) {
                    fullWidth = mRoadNetwork.renderpopupGUI(_gui, mRoadNetwork.currentIntersection);

                    if (mRoadNetwork.currentRoad && (splineTest.bVertex || splineTest.bSegment)) {
                        ImGui::SetCursorPosY(300);
                        mRoadNetwork.renderpopupGUI(_gui, mRoadNetwork.intersectionSelectedRoad, splineTest.index);
                    }
                }

                if (mRoadNetwork.currentRoad) {
                    static uint _from = 0;
                    static uint _to = 0;
                    fullWidth = mRoadNetwork.renderpopupGUI(_gui, mRoadNetwork.currentRoad, splineTest.index);
                    if ((_from != mRoadNetwork.selectFrom) || (_to != mRoadNetwork.selectTo)) {
                        updateDynamicRoad(true);
                    }
                }

                ImGui::PopFont();
            }
            roadPanel.release();
        }
    }
    //ImGui::PopFont();
    break;
    }

    if (!ImGui::IsAnyWindowHovered())
    {
        if (splineTest.bVertex)
        {
            std::stringstream tooltip;

            if (mRoadNetwork.currentRoad)
            {
                tooltip << "camber   " << (int)(mRoadNetwork.currentRoad->points[splineTest.index].camber * 57.2958f) << "º     <-   ->\n";
                tooltip << "T   " << mRoadNetwork.currentRoad->points[splineTest.index].T << "      <-   ->\n";
                tooltip << "C   " << mRoadNetwork.currentRoad->points[splineTest.index].C << "      <-   ->\n";
                tooltip << "B   " << mRoadNetwork.currentRoad->points[splineTest.index].B << "      ctrl + left mouse + drag\n";

                ImGui::PushFont(_gui->getFont("roboto_26"));
                ImGui::SetTooltip(tooltip.str().c_str());
                ImGui::PopFont();
            }
        }
        else
        {
            char TTTEXT[1024];
            uint idx = split.feedback.tum_idx;
            sprintf(TTTEXT, "%s\n(%3.1f, %3.1f, %3.1f)\nlodyx %d %d %d\n%3.1fm", blockFromPositionB(split.feedback.tum_Position).c_str(),
                split.feedback.tum_Position.x, split.feedback.tum_Position.y, split.feedback.tum_Position.z,
                m_tiles[idx].lod, m_tiles[idx].y, m_tiles[idx].x, glm::length(split.feedback.tum_Position - this->cameraOrigin)
            );
            //sprintf(TTTEXT, "splinetest %d (%d, %d) \n", splineTest.index, splineTest.bVertex, splineTest.bSegment);
            auto& style = ImGui::GetStyle();
            style.Colors[ImGuiCol_Text] = ImVec4(0.50f, 0.5, 0.5, 1.f);
            style.Colors[ImGuiCol_PopupBg] = ImVec4(0.00f, 0.f, 0.f, 0.8f);

            ImGui::PushFont(_gui->getFont("roboto_26"));
            ImGui::SetTooltip(TTTEXT);
            ImGui::PopFont();
        }
    }
}



void terrainManager::bil_to_jp2()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"txt"} }; // select the filanemaes file
    if (openFileDialog(filters, path))
    {
        FILE* file, * summary;
        fopen_s(&file, path.string().c_str(), "r");
        int numread;
        std::string pathOnly = path.string().substr(0, path.string().find_last_of("/\\") + 1);
        fopen_s(&summary, (pathOnly + "elevations.txt").c_str(), "w");
        do
        {
            char filename[256];
            int lod, y, x, blockSize;
            float xstart, ystart, size;
            memset(filename, 0, 256);
            numread = fscanf_s(file, "%d %d %d %d %f %f %f %s", &lod, &y, &x, &blockSize, &xstart, &ystart, &size, filename, 256);
            if (numread > 0) {
                bil_to_jp2(pathOnly + filename, blockSize, summary, lod, y, x, xstart, ystart, size);
            }
        } while (numread > 0);

        fclose(file);
    }
}



void terrainManager::bil_to_jp2(std::string file, const uint size, FILE* summary, uint _lod, uint _y, uint _x, float _xstart, float _ystart, float _size)
{

    // Find the minimum and maximum
    float data_min = 9999999.0f;
    float data_max = -9999999.0f;
    float data[1024];

    FILE* bilData = fopen((file + ".bil").c_str(), "rb");
    if (bilData) {

        for (uint i = 0; i < size; i++) {
            fread(data, sizeof(float), size, bilData);
            for (uint j = 0; j < size; j++) {
                data_min = __min(data_min, data[j]);
                data_max = __max(data_max, data[j]);
            }
        }
        fclose(bilData);
    }
    // now add 5 meter either side to allow for possible modifications
    data_min -= 5.0f;
    data_max += 5.0f;
    float data_scale = 65536.0f / (data_max - data_min);


    ojph::codestream codestream;
    ojph::j2c_outfile j2c_file;
    j2c_file.open((file + ".jp2").c_str());
    {
        // set up
        ojph::param_siz siz = codestream.access_siz();
        siz.set_image_extent(ojph::point(size, size));
        siz.set_num_components(1);
        siz.set_component(0, ojph::point(1, 1), 16, false);		//??? unsure about the subsampling point()
        siz.set_image_offset(ojph::point(0, 0));
        siz.set_tile_size(ojph::size(size, size));
        siz.set_tile_offset(ojph::point(0, 0));

        ojph::param_cod cod = codestream.access_cod();
        cod.set_num_decomposition(5);
        cod.set_block_dims(64, 64);
        //if (num_precints != -1)
        //	cod.set_precinct_size(num_precints, precinct_size);
        cod.set_progression_order("RPCL");
        cod.set_color_transform(false);
        cod.set_reversible(false);
        codestream.access_qcd().set_irrev_quant(0.0001f);





        FILE* bilData = fopen((file + ".bil").c_str(), "rb");
        if (bilData)
        {
            codestream.write_headers(&j2c_file);

            int next_comp;
            ojph::line_buf* cur_line = codestream.exchange(NULL, next_comp);

            for (uint i = 0; i < size; ++i)
            {
                //base->read(cur_line, next_comp);
                float data[1024];
                unsigned short dataUint[1024];
                fread(data, sizeof(float), size, bilData);

                for (uint j = 0; j < size; j++)
                {
                    dataUint[j] = (uint)((data[j] - data_min) * data_scale);
                }

                int32_t* dp = cur_line->i32;
                for (uint j = 0; j < size; j++) {
                    *dp++ = (int32_t)dataUint[j];
                }
                cur_line = codestream.exchange(cur_line, next_comp);
            }
            fclose(bilData);
        }


    }
    codestream.flush();
    codestream.close();


    fprintf(summary, "%d %d %d %d %f %f %f %f %f elevation/hgt_%d_%d_%d.jp2\n", _lod, _y, _x, size, _xstart, _ystart, _size, data_min, (data_max - data_min), _lod, _y, _x);
}


void terrainManager::onGuiMenubar(Gui* pGui)
{
    bool b = false;

    ImGui::SetCursorPos(ImVec2(150, 0));
    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Text] = ImVec4(0.38f, 0.52f, 0.10f, 1);
    //ImGui::Text(presets.name.c_str());
    style.Colors[ImGuiCol_Text] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

    ImGui::SetCursorPos(ImVec2(200, 0));
    if (ImGui::BeginMenu("terrain"))
    {
        if (ImGui::MenuItem("load"))
        {
            std::filesystem::path path;
            FileDialogFilterVec filters = { {"terrainSettings.json"} };
            if (openFileDialog(filters, path))
            {
                std::ifstream is(path);
                lastfile.terrain = path.string();
            }
        }
        if (ImGui::MenuItem("process bil -> jp2"))
        {
            bil_to_jp2();
        }
        if (ImGui::MenuItem("settings"))
        {
            requestPopupSettings = true;
        }
        if (ImGui::MenuItem("tree"))
        {
            requestPopupTree = true;
        }
        if (ImGui::MenuItem("debug"))
        {
            requestPopupDebug = true;
        }
        ImGui::EndMenu();
    }

    float x = ImGui::GetCursorPosX();
    ImGui::SetCursorPos(ImVec2(screenSize.x - 700, 0));
    ImGui::Text(lastfile.terrain.c_str());
    ImGui::SetCursorPos(ImVec2(x, 0));
}

//mimic hlsl all()
bool all(float4 in)
{
    if (in.x == 0) return false;
    if (in.y == 0) return false;
    if (in.z == 0) return false;
    if (in.w == 0) return false;
    return true;
}


void terrainManager::testForSurfaceMain()
{
    bool bCM = true;
    for (auto& tile : m_used)
    {
        bool surface = tile->parent && tile->parent->main_ShouldSplit && tile->child[0] == nullptr;
        bool bCM2 = true;
        if (surface) {
            frustumFlags[tile->index] |= 1 << 20;
        }
    }
}

void terrainManager::testForSurfaceEnv()
{
}


#define mainMaxLod 15
bool terrainManager::testForSplit(quadtree_tile* _tile)
{
    _tile->main_ShouldSplit = false;
    _tile->env_ShouldSplit = false;

    if (_tile->lod > mainMaxLod)
        return false;

    float scale = _tile->size / glm::length(float3(_tile->boundingSphere) - cameraOrigin);
    float boundingSphereSize = _tile->size * 0.8f;

    for (int i = 0; i < CameraType_MAX; i++) {
        if (cameraViews[i].bUse) {

            float4 viewBS = cameraViews[i].view * _tile->boundingSphere;
            float4 test = saturate(viewBS * cameraViews[i].frustumMatrix + float4(boundingSphereSize, boundingSphereSize, boundingSphereSize, boundingSphereSize));
            bool inFrust = all(test);

            viewBS.w = 0;
            float distance = length(viewBS) + 0.01f;
            float fovscale = glm::length(cameraViews[i].proj[0]);
            float m_halfAngle_to_Pixels = cameraViews[i].resolution * fovscale / 5.0f;
            float lod_Pix = _tile->size / distance * m_halfAngle_to_Pixels;



            if (lod_Pix > 150 && inFrust)
            {
                _tile->main_ShouldSplit = true;
            }
            else if (lod_Pix > 300)
            {
                _tile->main_ShouldSplit = true;
            }
        }
    }

    if (_tile->main_ShouldSplit && !_tile->child[0]) {
        _tile->forSplit = true;
        return true;
    }



    return false;
}

bool terrainManager::testFrustum(quadtree_tile* _tile)
{
    return true;
}

void terrainManager::markChildrenForRemove(quadtree_tile* _tile)
{
    for (uint i = 0; i < 4; i++) {
        if (_tile->child[i]) {
            markChildrenForRemove(_tile->child[i]);
            _tile->child[i]->forRemove = true;
            _tile->child[i] = nullptr;
        }
    }
}


uint terrainManager::gisHash(glm::vec3 _position)
{
    uint z = (uint)floor((_position.z + (settings.size / 2.0f)) * 16.0f / settings.size);
    uint x = (uint)floor((_position.x + (settings.size / 2.0f)) * 16.0f / settings.size);

    return (z << 16) + x;
}

void terrainManager::gisReload(glm::vec3 _position)
{
    float halfsize = ecotopeSystem::terrainSize / 2.f;
    float blocksize = ecotopeSystem::terrainSize / 16.f;
    float buffersize = blocksize * (3200.f / 2500.f); // this ratio was set by eifel, keep it
    float edgesize = (buffersize - blocksize) / 2;

    uint hash = gisHash(_position);
    if (hash != gis_overlay.hash)
    {
        gis_overlay.hash = hash;
        uint x = hash & 0xffff;
        uint z = hash >> 16;

        std::filesystem::path path = settings.dirRoot + "/overlay/" + char(65 + hash & 0xff) + "_" + std::to_string(z) + "_image.dds";
        gis_overlay.texture = Texture::createFromFile(path, true, true, Resource::BindFlags::ShaderResource);

        gis_overlay.box = glm::vec4(-edgesize - halfsize + x * blocksize, -edgesize - halfsize + z * blocksize, buffersize, buffersize);    // fixme this sort of asumes 40 x 40 km

        terrainShader.Vars()->setTexture("gGISAlbedo", gis_overlay.texture);
        terrainShader.Vars()["PerFrameCB"]["showGIS"] = (int)gis_overlay.show;
        terrainShader.Vars()["PerFrameCB"]["gisBox"] = gis_overlay.box;
    }
}


void terrainManager::clearCameras()
{
    for (uint i = 0; i < CameraType_MAX; i++) {
        cameraViews[i].bUse = false;
    }

}

void terrainManager::setCamera(unsigned int _index, glm::mat4 viewMatrix, glm::mat4 projMatrix, float3 position, bool b_use, float _resolution)
{
    if (_index < CameraType_MAX)
    {
        cameraOrigin = position;	// SURE so last set camera does this, but they should just about all be the same right?

        cameraViews[_index].bUse = b_use;
        cameraViews[_index].resolution = _resolution;
        cameraViews[_index].view = viewMatrix;
        cameraViews[_index].proj = projMatrix;
        cameraViews[_index].viewProj = cameraViews[_index].view * cameraViews[_index].proj;
        cameraViews[_index].position = position;

        // remeber that these are transposed as well here
        cameraViews[_index].frustumPlane[0].x = cameraViews[_index].proj[0][3] + cameraViews[_index].proj[0][0];
        cameraViews[_index].frustumPlane[0].y = cameraViews[_index].proj[1][3] + cameraViews[_index].proj[1][0];
        cameraViews[_index].frustumPlane[0].z = cameraViews[_index].proj[2][3] + cameraViews[_index].proj[2][0];
        cameraViews[_index].frustumPlane[0].w = cameraViews[_index].proj[3][3] + cameraViews[_index].proj[3][0];

        cameraViews[_index].frustumPlane[1].x = cameraViews[_index].proj[0][3] - cameraViews[_index].proj[0][0];
        cameraViews[_index].frustumPlane[1].y = cameraViews[_index].proj[1][3] - cameraViews[_index].proj[1][0];
        cameraViews[_index].frustumPlane[1].z = cameraViews[_index].proj[2][3] - cameraViews[_index].proj[2][0];
        cameraViews[_index].frustumPlane[1].w = cameraViews[_index].proj[3][3] - cameraViews[_index].proj[3][0];

        cameraViews[_index].frustumPlane[2].x = cameraViews[_index].proj[0][3] + cameraViews[_index].proj[0][1];
        cameraViews[_index].frustumPlane[2].y = cameraViews[_index].proj[1][3] + cameraViews[_index].proj[1][1];
        cameraViews[_index].frustumPlane[2].z = cameraViews[_index].proj[2][3] + cameraViews[_index].proj[2][1];
        cameraViews[_index].frustumPlane[2].w = cameraViews[_index].proj[3][3] + cameraViews[_index].proj[3][1];

        cameraViews[_index].frustumPlane[3].x = cameraViews[_index].proj[0][3] - cameraViews[_index].proj[0][1];
        cameraViews[_index].frustumPlane[3].y = cameraViews[_index].proj[1][3] - cameraViews[_index].proj[1][1];
        cameraViews[_index].frustumPlane[3].z = cameraViews[_index].proj[2][3] - cameraViews[_index].proj[2][1];
        cameraViews[_index].frustumPlane[3].w = cameraViews[_index].proj[3][3] - cameraViews[_index].proj[3][1];

        cameraViews[_index].frustumMatrix = glm::mat4x4(
            cameraViews[_index].frustumPlane[0].x, cameraViews[_index].frustumPlane[0].y, cameraViews[_index].frustumPlane[0].z, cameraViews[_index].frustumPlane[0].w,
            cameraViews[_index].frustumPlane[1].x, cameraViews[_index].frustumPlane[1].y, cameraViews[_index].frustumPlane[1].z, cameraViews[_index].frustumPlane[1].w,
            cameraViews[_index].frustumPlane[2].x, cameraViews[_index].frustumPlane[2].y, cameraViews[_index].frustumPlane[2].z, cameraViews[_index].frustumPlane[2].w,
            cameraViews[_index].frustumPlane[3].x, cameraViews[_index].frustumPlane[3].y, cameraViews[_index].frustumPlane[3].z, cameraViews[_index].frustumPlane[3].w);
    }
}



void terrainManager::bakeVegetation()
{
    int superSample = 8;
    int baseSize = 64;
    int numBuffer = 32;

    // do width and height
    float W = groveTree.extents.x;      // this is half width
    float H = groveTree.extents.y;
    int iW = 4 * (int)(baseSize * W * 2.f / H / 4) * superSample;
    int iH = baseSize * superSample;

    Fbo::Desc desc;
    desc.setDepthStencilTarget(ResourceFormat::D24UnormS8);			// keep for now, not sure why, but maybe usefult for cuts
    desc.setColorTarget(0u, ResourceFormat::RGBA8Unorm, true);		// albedo
    desc.setColorTarget(1u, ResourceFormat::RGBA16Float, true);	    // normal_16
    desc.setColorTarget(2u, ResourceFormat::RGBA8Unorm, true);		// normal_8
    desc.setColorTarget(3u, ResourceFormat::RGBA8Unorm, true);	// pbr
    desc.setColorTarget(4u, ResourceFormat::RGBA8Unorm, true);	// extra
    Fbo::SharedPtr fbo = Fbo::create2D(iW, iH, desc, 1, 1);

    ribbonShader_Bake.State()->setFbo(fbo);
    viewportVegbake.originX = 0;
    viewportVegbake.originY = 0;
    viewportVegbake.minDepth = 0;
    viewportVegbake.maxDepth = 1;
    viewportVegbake.width = (float)iW;
    viewportVegbake.height = (float)iH;
    ribbonShader_Bake.State()->setViewport(0, viewportVegbake, true);


    glm::mat4 V, P, VP;
    V[0] = glm::vec4(1, 0, 0, 0);
    V[1] = glm::vec4(0, 1, 0, 0);
    V[2] = glm::vec4(0, 0, 1, 0);
    V[3] = glm::vec4(0, 0, 100, 1);

    float s = 0.2f;
    P = glm::orthoLH(-W, W, 0.f, H, -1000.0f, 1000.0f);

    VP = P * V;    //??? order

    rmcv::mat4 view, proj, viewproj;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            view[j][i] = V[j][i];
            proj[j][i] = P[j][i];
            viewproj[j][i] = VP[j][i];
        }
    }


    const glm::vec4 clearColor(0.5, 0.5f, 1.0f, 0.0f);
    bake.renderContext->clearFbo(fbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    bake.renderContext->clearRtv(fbo->getRenderTargetView(0).get(), glm::vec4(0.0, 0.0f, 0.0f, 0.0f));
    bake.renderContext->clearRtv(fbo->getRenderTargetView(1).get(), glm::vec4(0.5, 0.5f, 1.0f, 0.0f));
    bake.renderContext->clearRtv(fbo->getRenderTargetView(2).get(), glm::vec4(0.5, 0.5f, 1.0f, 0.0f));
    bake.renderContext->clearRtv(fbo->getRenderTargetView(3).get(), glm::vec4(0.0, 0.0f, 0.0f, 0.0f));
    bake.renderContext->clearRtv(fbo->getRenderTargetView(4).get(), glm::vec4(1.0, 1.0f, 1.0f, 1.0f));

    ribbonShader_Bake.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
    ribbonShader_Bake.Vars()["gConstantBuffer"]["eyePos"] = float3(0, 0, -100000);  // just very far sort of parallel
    ribbonShader.Vars()["gConstantBuffer"]["offset"] = float3(0, 0, 0);
    ribbonShader.Vars()["gConstantBuffer"]["repeatScale"] = 1.0f;
    ribbonShader.Vars()["gConstantBuffer"]["numSide"] = 1;


    auto& block = ribbonShader_Bake.Vars()->getParameterBlock("textures");
    ShaderVar& var = block->findMember("T");        // FIXME pre get
    _plantMaterial::static_materials_veg.setTextures(var);

    _plantMaterial::static_materials_veg.rebuildStructuredBuffer();

    ribbonData->setBlob(groveTree.packedRibbons, 0, groveTree.numBranchRibbons * sizeof(unsigned int) * 6);
    groveTree.bChanged = false;

    ribbonShader_Bake.Vars()["gConstantBuffer"]["fakeShadow"] = 4;
    ribbonShader_Bake.Vars()["gConstantBuffer"]["objectScale"] = float3(objectScale, objectScale, objectScale);
    ribbonShader_Bake.Vars()["gConstantBuffer"]["objectOffset"] = objectOffset;
    ribbonShader_Bake.Vars()["gConstantBuffer"]["radiusScale"] = radiusScale;

    // lighting
    ribbonShader_Bake.Vars()["gConstantBuffer"]["Ao_depthScale"] = static_Ao_depthScale;
    ribbonShader_Bake.Vars()["gConstantBuffer"]["sunTilt"] = static_sunTilt;
    ribbonShader_Bake.Vars()["gConstantBuffer"]["bDepth"] = static_bDepth;
    ribbonShader_Bake.Vars()["gConstantBuffer"]["bScale"] = static_bScale;

    ribbonShader_Bake.State()->setRasterizerState(split.rasterstateSplines);
    ribbonShader_Bake.State()->setBlendState(blendstateVegBake);//blendstateSplines


    ribbonShader_Bake.drawInstanced(bake.renderContext, groveTree.numBranchRibbons, 10000);

    bake.renderContext->flush(true);


    compute_bakeFloodfill.Vars()->setTexture("gAlbedo", fbo->getColorTexture(0));
    compute_bakeFloodfill.Vars()->setTexture("gNormal", fbo->getColorTexture(2));
    compute_bakeFloodfill.Vars()->setTexture("gTranslucency", fbo->getColorTexture(4));
    compute_bakeFloodfill.Vars()->setTexture("gpbr", fbo->getColorTexture(3));

    for (int i = 0; i < numBuffer; i++)
    {
        compute_bakeFloodfill.dispatch(bake.renderContext, iW / 4, iH / 4);
    }





    std::filesystem::path path = terrainManager::lastfile.vegMaterial;
    FileDialogFilterVec filters = { {"vegetationMaterial"} };
    if (saveFileDialog(filters, path))
    {
        std::ofstream os(path.string());
        cereal::JSONOutputArchive archive(os);

        _plantMaterial Mat;
        Mat._constData.translucency = 1;

        std::string shortPath = path.parent_path().string() + "\\";
        std::string shortNameExt = path.filename().string();
        std::string shortName = shortNameExt.substr(0, shortNameExt.length() - 19);

        char outName[512];
        sprintf(outName, "%s%s_albedo.png", shortPath.c_str(), shortName.c_str());
        fbo->getColorTexture(0).get()->captureToFile(0, 0, outName, Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::ExportAlpha);
        Mat.normalPath = shortPath + shortName + "_albedo.dds";
        Mat.normalName = shortName + "_albedo.dds";

        //sprintf(outName, "%s%s_normal_16.exr", shortPath.c_str(), shortName.c_str());
        //fbo->getColorTexture(1).get()->captureToFile(0, 0, outName, Bitmap::FileFormat::ExrFile, Bitmap::ExportFlags::None);
        //Mat.normalPath = shortPath + shortName + "_normal_16.dds";
        //Mat.normalName = shortName + "_normal_16.dds";

        sprintf(outName, "%s%s_normal.png", shortPath.c_str(), shortName.c_str());
        fbo->getColorTexture(2).get()->captureToFile(0, 0, outName, Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);
        Mat.normalPath = shortPath + shortName + "_normal.dds";
        Mat.normalName = shortName + "_normal.dds";


        sprintf(outName, "%s%s_translucency.png", shortPath.c_str(), shortName.c_str());
        fbo->getColorTexture(4).get()->captureToFile(0, 0, outName, Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);
        Mat.translucencyPath = shortPath + shortName + "_translucency.dds";
        Mat.translucencyName = shortName + "_translucency.dds";




        Mat.serialize(archive, _PLANTMATERIALVERSION);

        terrainManager::lastfile.vegMaterial = path.string();
    }
}



bool terrainManager::update(RenderContext* _renderContext)
{
    bake.renderContext = _renderContext;

    if (terrainMode == 0)
    {
        return false;
    }


    if (terrainMode == 4)
    {
    }

    


    


    FALCOR_PROFILE("update");
    bool dirty = false;

    {
        {
            FALCOR_PROFILE("update_bake");
            if (bake.inProgress)
            {
                bake.renderContext = _renderContext;
                bake_frame();
            }

            if (mEcosystem.change) {
                mEcosystem.change = false;
                reset(true);
            }
        }


        {
            FALCOR_PROFILE("update_roads");
            updateDynamicRoad(false);
            mRoadNetwork.testHit(split.feedback.tum_Position);


            if (mRoadNetwork.isDirty)
            {
                splines.numStaticSplines = __min((int)roadNetwork::staticBezierData.size(), splines.maxBezier);
                splines.numStaticSplinesIndex = __min((int)roadNetwork::staticIndexData.size(), splines.maxIndex);
                if (splines.numStaticSplines > 0)
                {
                    splines.bezierData->setBlob(roadNetwork::staticBezierData.data(), 0, splines.numStaticSplines * sizeof(cubicDouble));
                    splines.indexData->setBlob(roadNetwork::staticIndexData.data(), 0, splines.numStaticSplinesIndex * sizeof(bezierLayer));
                    mRoadNetwork.isDirty = false;
                }
                splines.numStaticSplinesBakeOnlyIndex = __min((int)roadNetwork::staticIndexData_BakeOnly.size(), splines.maxIndex);
                if (splines.numStaticSplinesBakeOnlyIndex > 0)
                {
                    splines.indexDataBakeOnly->setBlob(roadNetwork::staticIndexData_BakeOnly.data(), 0, splines.numStaticSplinesBakeOnlyIndex * sizeof(bezierLayer));
                    mRoadNetwork.isDirty = false;
                }

                bezierRoadstoLOD(4);
                mRoadNetwork.isDirty = false;
            }
        }

        {
            FALCOR_PROFILE("update_splitmerge");
            //split.compute_tileClear.dispatch(_renderContext, 1, 1);
            memset(frustumFlags, 0, sizeof(unsigned int) * 2048);		// clear all

            for (auto& tile : m_used)
            {
                tile->reset();
            }

            for (auto& tile : m_used)
            {
                dirty |= testForSplit(tile);

                if (!tile->main_ShouldSplit && tile->child[0]) {
                    markChildrenForRemove(tile);
                }
            }

            for (auto itt = m_used.begin(); itt != m_used.end();)               // do al merges
            {
                if ((*itt)->forRemove) {
                    (*itt)->forRemove = false;
                    m_free.push_back(*itt);
                    itt = m_used.erase(itt);
                }
                else {
                    ++itt;
                }
            }
        }
    }

    splitOne(_renderContext);

    {

        //if (dirty)
        {
            FALCOR_PROFILE("update_dirty");
            split.compute_tileClear.dispatch(_renderContext, 1, 1);



            testForSurfaceMain();
            for (auto& tile : m_used)
            {
                bool surface = tile->parent && tile->parent->main_ShouldSplit && tile->child[0] == nullptr;
                //bool surface = tile->parent && tile->child[0] == nullptr;
                bool bCM2 = true;
                frustumFlags[tile->index] = 1;
                if (surface) {
                    frustumFlags[tile->index] |= 1 << 20;
                }
            }

            auto pCB = split.compute_tileBuildLookup.Vars()->getParameterBlock("gConstants");
            pCB->setBlob(frustumFlags, 0, 2048 * sizeof(unsigned int));	// FIXME number of tiles
            uint cnt = (numTiles + 31) >> 5;
            split.compute_tileBuildLookup.dispatch(_renderContext, cnt, 1);
            //FIXME this of coarse adds it in teh worst fashion, interleaving 64 triangles or quads of diffirent tiles with one another
            // see what to do diffirent VERY bad for materials and texture reads

            {
                FALCOR_PROFILE("clip_lod_animate");
                rmcv::mat4 view, clip;
                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 4; j++) {
                        view[j][i] = cameraViews[1].view[j][i];
                        clip[j][i] = cameraViews[1].frustumMatrix[j][i];
                    }
                }
                split.compute_clipLodAnimatePlants.Vars()["gConstantBuffer"]["view"] = view;
                split.compute_clipLodAnimatePlants.Vars()["gConstantBuffer"]["clip"] = clip;
                split.compute_clipLodAnimatePlants.dispatchIndirect(_renderContext, split.dispatchArgs_plants.get(), 0);//225
            }
        }
    }





    //static gpuTile RB_tiles[1024];
    {
        FALCOR_PROFILE("update_readback");
        if (dirty)
        {
            // readback of tile centers
            _renderContext->copyResource(split.buffer_tileCenter_readback.get(), split.buffer_tileCenters.get());
            const float4* pData = (float4*)split.buffer_tileCenter_readback.get()->map(Buffer::MapType::Read);
            std::memcpy(&split.tileCenters, pData, sizeof(float4) * numTiles);
            split.buffer_tileCenter_readback.get()->unmap();
            for (int i = 0; i < m_tiles.size(); i++)
            {
                m_tiles[i].origin.y = split.tileCenters[i].x;           // THIS is very wrong, .x contains the middl;em but i also think its unused
                m_tiles[i].boundingSphere.y = split.tileCenters[i].x;
            }


            /*
            const void* tiledata = split.buffer_tiles.get()->map(Buffer::MapType::Read);
            std::memcpy(RB_tiles, tiledata, sizeof(gpuTile)* numTiles);
            split.buffer_tiles.get()->unmap();
            */
        }
    }

    if (hasChanged) {
        hasChanged = false;
        return true;
    }
    return false;
}






void terrainManager::hashAndCache(quadtree_tile* pTile)
{

    uint32_t hash = getHashFromTileCoords(pTile->lod, pTile->y, pTile->x);
    std::map<uint32_t, heightMap>::iterator it = elevationTileHashmap.find(hash);
    if (it != elevationTileHashmap.end()) {
        pTile->elevationHash = hash;
    }

    if (pTile->elevationHash > 0)
    {
        textureCacheElement map;

        if (!elevationCache.get(pTile->elevationHash, map))
        {
            std::array<unsigned short, 1048576> data;

            auto hashelement = elevationTileHashmap[pTile->elevationHash];
            ojph::codestream codestream;
            ojph::j2c_infile j2c_file;
            j2c_file.open(elevationTileHashmap[pTile->elevationHash].filename.c_str());
            codestream.enable_resilience();
            codestream.set_planar(false);
            codestream.read_headers(&j2c_file);
            codestream.create();
            int next_comp;

            for (int i = 0; i < 1024; ++i)
            {
                ojph::line_buf* line = codestream.pull(next_comp);
                int32_t* dp = line->i32;
                for (int j = 0; j < 1024; j++) {
                    int16_t val = (int16_t)*dp++;
                    data[i * 1024 + j] = val;
                }
            }
            codestream.close();

            map.texture = Texture::create2D(1024, 1024, Falcor::ResourceFormat::R16Unorm, 1, 1, data.data(), Resource::BindFlags::ShaderResource);
            elevationCache.set(pTile->elevationHash, map);
        }

        split.compute_tileBicubic.Vars()->setTexture("gInput", map.texture);
    }

}


void terrainManager::setChild(quadtree_tile* pTile, int y, int x)
{
    int childIdx = (y << 1) + x;
    float origX = pTile->size / 2.0f * x;
    float origY = pTile->size / 2.0f * y;

    pTile->child[childIdx] = m_free.front();
    //printf("%d, ",pTile->child[childIdx]->index);
    m_free.pop_front();
    pTile->child[childIdx]->set(pTile->lod + 1, pTile->x * 2 + x, pTile->y * 2 + y, pTile->size / 2.0f, pTile->origin + float4(origX, 0, origY, 0), pTile);
    m_used.push_back(pTile->child[childIdx]);
    pTile->child[childIdx]->elevationHash = pTile->elevationHash;
}


void terrainManager::splitOne(RenderContext* _renderContext)
{
    /* Bloody hell, pick the best one to do*/
    if (m_free.size() < 8) return;


    // FIXME PICK A BETTER ONE HERE
    for (auto& tile : m_used)
    {
        if (tile->forSplit)
        {
            hasChanged = true;
            hashAndCache(tile);

            if (true)	// antani, check for hashAndCache() return value is it ready
            {

                FALCOR_PROFILE("split");

                setChild(tile, 0, 0);
                setChild(tile, 0, 1);
                setChild(tile, 1, 0);
                setChild(tile, 1, 1);

                {
                    FALCOR_PROFILE("cs_tile_SplitMerge");
                    tileForSplit children[4];
                    for (int i = 0; i < 4; i++) {
                        children[i].index = tile->child[i]->index;
                        children[i].lod = tile->child[i]->lod;
                        children[i].y = tile->child[i]->y;
                        children[i].x = tile->child[i]->x;

                        children[i].origin = tile->child[i]->origin;
                        children[i].scale = tile->child[i]->size;
                    }

                    /*const auto& pReflector = split.compute_tileSplitMerge.Vars()->getReflection();
                    const auto& pDefaultBlock = pReflector->getDefaultParameterBlock();
                    const auto& mPerLightCbLoc = pDefaultBlock->getResourceBinding("gConstants");
                    auto pCB = split.compute_tileSplitMerge.Vars()->getParameterBlock(mPerLightCbLoc); */
                    auto pCB = split.compute_tileSplitMerge.Vars()->getParameterBlock("gConstants");
                    pCB->setBlob(children, 0, 4 * sizeof(tileForSplit));
                    split.compute_tileSplitMerge.dispatch(_renderContext, 1, 1);
                }


                {
                    FALCOR_PROFILE("extract_all");
                    for (int i = 0; i < 4; i++) {
                        splitChild(tile->child[i], _renderContext);
                        testForSplit(tile->child[i]);		// so its frustum flags are set
                    }
                }
                return;
            }

        }
    }


    {
        FALCOR_PROFILE("split");
        {
            FALCOR_PROFILE("cs_tile_SplitMerge");
        }
        {
            FALCOR_PROFILE("extract_all");
            {
                FALCOR_PROFILE("bicubic");
            }
            {
                FALCOR_PROFILE("renderTopdown");
            }
            {
                FALCOR_PROFILE("compute");
                {
                    FALCOR_PROFILE("copy_VERT");
                }
                {
                    FALCOR_PROFILE("compress_copy_Albedo");
                }
                {
                    FALCOR_PROFILE("normals");
                }
                {
                    FALCOR_PROFILE("verticies");
                }
                {
                    FALCOR_PROFILE("copy normals");
                }
                {
                    FALCOR_PROFILE("jumpflood");
                }
                {
                    FALCOR_PROFILE("copy_PBR");
                }
                {
                    FALCOR_PROFILE("delaunay");
                }
            }
        }
    }
}

void terrainManager::splitChild(quadtree_tile* _tile, RenderContext* _renderContext)
{
    const uint32_t cs_w = tile_numPixels / tile_cs_ThreadSize;
    const float2 origin = float2(_tile->origin.x, _tile->origin.z);
    const float outerSize = _tile->size * tile_numPixels / tile_InnerPixels;
    const float pixelSize = outerSize / tile_numPixels;
    float halfsize = ecotopeSystem::terrainSize / 2.f;
    float blocksize = ecotopeSystem::terrainSize / 16.f;


    {
        // Not nessesary but nice where we lack data for now
        _renderContext->clearFbo(split.tileFbo.get(), glm::vec4(0.3f, 0.3f, 0.3f, 1.0f), 1.0f, 0, FboAttachmentType::All);

        _renderContext->clearRtv(split.tileFbo.get()->getRenderTargetView(3).get(), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));

        _renderContext->clearRtv(split.tileFbo.get()->getRenderTargetView(4).get(), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
        _renderContext->clearRtv(split.tileFbo.get()->getRenderTargetView(5).get(), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
        _renderContext->clearRtv(split.tileFbo.get()->getRenderTargetView(6).get(), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f));
        _renderContext->clearRtv(split.tileFbo.get()->getRenderTargetView(7).get(), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    }


    {
        FALCOR_PROFILE("bicubic");
        heightMap& elevationMap = elevationTileHashmap[_tile->elevationHash];

        float2 bicubicOffset = (origin - elevationMap.origin) / elevationMap.size;
        float S = pixelSize / elevationMap.size;
        float2 bicubicSize = float2(S, S);

        if (_tile->elevationHash == 0)
        {
            split.compute_tileBicubic.Vars()->setTexture("gInput", split.rootElevation);
        }
        split.compute_tileBicubic.Vars()["gConstants"]["offset"] = bicubicOffset;
        split.compute_tileBicubic.Vars()["gConstants"]["size"] = bicubicSize;
        split.compute_tileBicubic.Vars()["gConstants"]["hgt_offset"] = elevationMap.hgt_offset;
        split.compute_tileBicubic.Vars()["gConstants"]["hgt_scale"] = elevationMap.hgt_scale;
        split.compute_tileBicubic.dispatch(_renderContext, cs_w, cs_w);
    }


    {
        splitRenderTopdown(_tile, _renderContext);
        _renderContext->copySubresource(height_Array.get(), _tile->index, split.tileFbo->getColorTexture(0).get(), 0);  // for picking only
    }

    {
        FALCOR_PROFILE("compute");





        {
            FALCOR_PROFILE("ecotope");

            heightMap& elevationMap = elevationTileHashmap[0];
            split.compute_tileEcotopes.Vars()->setTexture("gLowresHgt", split.rootElevation);
            split.compute_tileEcotopes.Vars()->setBuffer("plantIndex", mEcosystem.getPLantBuffer());
            auto pCB = split.compute_tileEcotopes.Vars()->getParameterBlock("gConstants");

            // bad herer but lets set the textures
            auto& block = split.compute_tileEcotopes.Vars()->getParameterBlock("gmyTextures");
            ShaderVar& var = block->findMember("T");        // FIXME pre get
            {
                for (size_t i = 0; i < mEcosystem.ecotopes.size(); i++)
                {
                    var[i] = mEcosystem.ecotopes[i].texAlbedo;
                    var[12 + i] = mEcosystem.ecotopes[i].texNoise;
                }
            }



            //mEcosystem.resetPlantIndex(_tile->lod);
            ecotopeGpuConstants C = *mEcosystem.getConstants();
            C.pixelSize = pixelSize;
            C.tileXY = float2(_tile->x, _tile->y);
            C.padd2 = float2(elevationMap.hgt_offset, elevationMap.hgt_scale);
            C.lowResSize = _tile->size / settings.size / 248.0f;
            C.lowResOffset = float2(_tile->origin.x + halfsize, _tile->origin.z + halfsize) / settings.size;
            C.lod = _tile->lod;
            C.tileIndex = _tile->index;
            pCB->setBlob(&C, 0, sizeof(ecotopeGpuConstants));
            if (C.numEcotopes > 0)
            {
                split.compute_tileEcotopes.dispatch(_renderContext, cs_w, cs_w);
            }
        }

        {
            FALCOR_PROFILE("passthrough");
            uint cnt = (numQuadsPerTile) >> 8;	  // FIXME - hiesdie oordoen is es dan stadig - dit behoort Compute indoirect te wees  en die regte getal te gebruik
            split.compute_tilePassthrough.Vars()["gConstants"]["parent_index"] = _tile->parent->index;
            split.compute_tilePassthrough.Vars()["gConstants"]["child_index"] = _tile->index;
            split.compute_tilePassthrough.Vars()["gConstants"]["dX"] = _tile->x & 0x1;
            split.compute_tilePassthrough.Vars()["gConstants"]["dY"] = _tile->y & 0x1;
            split.compute_tilePassthrough.dispatch(_renderContext, cnt, 1);
        }

        {
            // Do this early to avoid stalls
            FALCOR_PROFILE("copy_VERT");
            _renderContext->copyResource(split.vertex_B_texture.get(), split.vertex_clear.get());			// not 100% sure this clear is needed
            _renderContext->copyResource(split.vertex_A_texture.get(), split.vertex_preload.get());
        }

        {
            // compress and copy colour data
            FALCOR_PROFILE("compress_copy_Albedo");
            //split.compute_bc6h.Vars()->setTexture("gSource", split.tileFbo->getColorTexture(1));
            //split.compute_bc6h.dispatch(_renderContext, cs_w / 4, cs_w / 4);
            //_renderContext->copySubresource(compressed_Albedo_Array.get(), _tile->index, split.bc6h_texture.get(), 0);

            _renderContext->copySubresource(compressed_Albedo_Array.get(), _tile->index, split.tileFbo->getColorTexture(1).get(), 0);
        }

        {
            FALCOR_PROFILE("normals");
            split.compute_tileNormals.Vars()["gConstants"]["pixSize"] = pixelSize;
            split.compute_tileNormals.dispatch(_renderContext, cs_w, cs_w);
        }

        {
            FALCOR_PROFILE("verticies");
            float scale = 1.0f;
            if (_tile->lod < 7)  scale = 1.3f;
            if (_tile->lod == 13)  scale = 1.2f;
            if (_tile->lod == 14)  scale = 1.5f;
            if (_tile->lod == 15)  scale = 2.0f;
            if (_tile->lod >= 16)  scale = 3.2f;
            scale *= 2.5;

            split.compute_tileVerticis.Vars()->setSampler("linearSampler", sampler_Clamp);
            split.compute_tileVerticis.Vars()["gConstants"]["constants"] = float4(pixelSize * scale, 0, 0, _tile->index);
            split.compute_tileVerticis.dispatch(_renderContext, cs_w / 2, cs_w / 2);
        }

        {
            FALCOR_PROFILE("copy normals");
            _renderContext->copySubresource(compressed_Normals_Array.get(), _tile->index, split.normals_texture.get(), 0);
            /*
            PROFILE(normalsBC6H);
            csBC6H_compressor.getVars()->setTexture("gSource", mpNormals);
            csBC6H_compressor.dispatch(pRenderContext, w / 4, h / 4);
            pRenderContext->copySubresource(mpCompressed_Normals_Array.get(), pTile->m_idx, mpCompressed_TMP.get(), 0);
            */
        }

        /*
        // both of these need to chnage to work on child not parent
        {
          FALCOR_PROFILE("cs_tile_Generate");
          uint32_t w_gen = tile_numPixels / tile_cs_ThreadSize_Generate - 1;
          uint32_t h_gen = tile_numPixels / tile_cs_ThreadSize_Generate - 1;
          cs_tile_Generate.getCB()->setVariable<uint>(0, pTile->m_idx);
          cs_tile_Generate.dispatch(pRenderContext, w_gen, h_gen);
        }

        {
          FALCOR_PROFILE("cs_tile_Passthrough");
          uint cnt = (numQuadsPerTile) >> 8;	  // FIXME - hiesdie oordoen is es dan stadig - dit behoort Compute indoirect te wees  en die regte getal te gebruik
          cs_tile_Passthrough.getCB()->setVariable<uint>(0, pTile->m_idx);
          cs_tile_Passthrough.getCB()->setVariable<uint>(sizeof(int), pTile->m_X & 0x1);
          cs_tile_Passthrough.getCB()->setVariable<uint>(sizeof(int) * 2, pTile->m_Y & 0x1);
          cs_tile_Passthrough.dispatch(pRenderContext, cnt, 1);
        }
        */

        // jumpflood algorithm (1+JFA+1) tp build voroinoi diagram ------------------------------------------------------------------------
        // ek weet 32 en 6 loops is goed
        {
            FALCOR_PROFILE("jumpflood");

            uint step = 4;
            for (int j = 0; j < 3; j++) {
                split.compute_tileJumpFlood.Vars()["gConstants"]["step"] = step;
                if (j & 0x1) {
                    split.compute_tileJumpFlood.Vars()->setTexture("gInVerts", split.vertex_B_texture);
                    split.compute_tileJumpFlood.Vars()->setTexture("gOutVerts", split.vertex_A_texture);
                }
                else {
                    split.compute_tileJumpFlood.Vars()->setTexture("gInVerts", split.vertex_A_texture);
                    split.compute_tileJumpFlood.Vars()->setTexture("gOutVerts", split.vertex_B_texture);

                }

                split.compute_tileJumpFlood.dispatch(_renderContext, cs_w / 2, cs_w / 2);
                step /= 2;
                if (step < 1) step = 1;
            }

            //split.compute_tileJumpFlood.Vars()->setTexture("gInVerts", NULL);
            //split.compute_tileJumpFlood.Vars()->setTexture("gOutVerts", NULL);
        }

        {
            FALCOR_PROFILE("copy_PBR");
            split.compute_bc6h.Vars()->setTexture("gSource", split.tileFbo->getColorTexture(2));
            split.compute_bc6h.dispatch(_renderContext, cs_w / 4, cs_w / 4);
            _renderContext->copySubresource(compressed_PBR_Array.get(), _tile->index, split.bc6h_texture.get(), 0);
        }


        {
            FALCOR_PROFILE("delaunay");
            uint32_t firstVertex = _tile->index * numVertPerTile;
            uint32_t zero = 0;
            split.buffer_terrain->getUAVCounter()->setBlob(&zero, 0, sizeof(uint32_t));	// damn, I misuse increment, count in 1's but write in 3's
            split.compute_tileDelaunay.Vars()["gConstants"]["tile_Index"] = _tile->index;
            split.compute_tileDelaunay.dispatch(_renderContext, cs_w / 2, cs_w / 2);
            //mpRenderContext->copyResource(mpDefaultFBO->getColorTexture(0).get(), mpVertsMap.get());
            //mpVertsMap->captureToFile(0, 0, "e:/voroinoi_verts.png");
        }
    }


}



void terrainManager::splitRenderTopdown(quadtree_tile* _pTile, RenderContext* _renderContext)
{
    FALCOR_PROFILE("renderTopdown");

    // set up the camera -----------------------
    float s = _pTile->size / 2.0f;
    float x = _pTile->origin.x + s;
    float z = _pTile->origin.z + s;
    glm::mat4 V, P, VP;
    V[0] = glm::vec4(1, 0, 0, 0);
    V[1] = glm::vec4(0, 0, 1, 0);
    V[2] = glm::vec4(0, -1, 0, 0);
    V[3] = glm::vec4(-x, z, 0, 1);

    s *= 256.0f / 248.0f;
    P = glm::orthoLH(-s, s, -s, s, -10000.0f, 10000.0f);

    //viewproj = view * proj;
    VP = P * V;    //??? order

    rmcv::mat4 view, proj, viewproj;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            view[j][i] = V[j][i];
            proj[j][i] = P[j][i];
            viewproj[j][i] = VP[j][i];
        }
    }


    {
        split.shader_meshTerrafector.State()->setFbo(split.tileFbo);
        split.shader_meshTerrafector.State()->setRasterizerState(split.rasterstateSplines);
        split.shader_meshTerrafector.State()->setBlendState(split.blendstateRoadsCombined);
        split.shader_meshTerrafector.State()->setDepthStencilState(split.depthstateAll);

        split.shader_meshTerrafector.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        split.shader_meshTerrafector.Vars()["gConstantBuffer"]["overlayAlpha"] = 1.0f;

        auto& block = split.shader_meshTerrafector.Vars()->getParameterBlock("gmyTextures");
        ShaderVar& var = block->findMember("T");        // FIXME pre get
        terrafectorEditorMaterial::static_materials.setTextures(var);
    }

    if (bSplineAsTerrafector)           // Now render the roadNetwork
    {
        split.shader_splineTerrafector.State()->setFbo(split.tileFbo);
        split.shader_splineTerrafector.State()->setRasterizerState(split.rasterstateSplines);
        split.shader_splineTerrafector.State()->setDepthStencilState(split.depthstateAll);
        split.shader_splineTerrafector.State()->setBlendState(split.blendstateRoadsCombined);

        split.shader_splineTerrafector.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        split.shader_splineTerrafector.Vars()["gConstantBuffer"]["startOffset"] = 0;
        split.shader_splineTerrafector.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);

        auto& block = split.shader_splineTerrafector.Vars()->getParameterBlock("gmyTextures");
        ShaderVar& var = block->findMember("T");        // FIXME pre get
        terrafectorEditorMaterial::static_materials.setTextures(var);

        split.shader_splineTerrafector.Vars()->setBuffer("splineData", splines.bezierData);     // not created yet
    }


    // Mesh bake low
    if (gis_overlay.bakeBakeOnlyData)
    {
        if (_pTile->lod >= 4)
        {
            gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD4_bakeLow.getTile((_pTile->y >> (_pTile->lod - 4)) * 16 + (_pTile->x >> (_pTile->lod - 4)));
            if (tile)
            {
                if (tile->numBlocks > 0)
                {
                    split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                    split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                    split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
                }
            }
        }

        if (bSplineAsTerrafector)           // Now render the roadNetwork
        {
            split.shader_splineTerrafector.Vars()->setBuffer("indexData", splines.indexDataBakeOnly);
            split.shader_splineTerrafector.drawIndexedInstanced(_renderContext, 64 * 6, splines.numStaticSplinesBakeOnlyIndex);
        }

        if (_pTile->lod >= 4)
        {
            gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD4_bakeHigh.getTile((_pTile->y >> (_pTile->lod - 4)) * 16 + (_pTile->x >> (_pTile->lod - 4)));
            if (tile)
            {
                if (tile->numBlocks > 0)
                {
                    split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                    split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                    split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
                }
            }
        }
    }






    if (_pTile->lod >= 6)
    {
        gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD6.getTile((_pTile->y >> (_pTile->lod - 6)) * 64 + (_pTile->x >> (_pTile->lod - 6)));
        if (tile)
        {
            if (tile->numBlocks > 0)
            {
                split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
            }
        }
    }
    else if (_pTile->lod >= 4)
    {
        gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD4.getTile((_pTile->y >> (_pTile->lod - 4)) * 16 + (_pTile->x >> (_pTile->lod - 4)));
        if (tile)
        {
            if (tile->numBlocks > 0)
            {
                split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
            }
        }
    }
    else if (_pTile->lod >= 2)
    {
        gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD2.getTile((_pTile->y >> (_pTile->lod - 2)) * 4 + (_pTile->x >> (_pTile->lod - 2)));
        if (tile)
        {
            if (tile->numBlocks > 0)
            {
                split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
            }
        }
    }


    // OVER:AY ######################################################
    if (gis_overlay.terrafectorOverlayStrength > 0)
        if (_pTile->lod >= 4)
        {
            gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD4_overlay.getTile((_pTile->y >> (_pTile->lod - 4)) * 16 + (_pTile->x >> (_pTile->lod - 4)));
            if (tile)
            {
                if (tile->numBlocks > 0)
                {
                    split.shader_meshTerrafector.Vars()["gConstantBuffer"]["overlayAlpha"] = gis_overlay.terrafectorOverlayStrength;
                    split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                    split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                    split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
                }
            }
        }


    //??? should probably be in the roadnetwork code, but look at the optimize step first
    if (bSplineAsTerrafector)           // Now render the roadNetwork
    {
        if (_pTile->lod >= 8)
        {
            quadtree_tile* P8 = _pTile;
            while (P8->lod > 8) P8 = P8->parent;
            split.shader_splineTerrafector.Vars()["gConstantBuffer"]["startOffset"] = splines.startOffset_LOD8[P8->y][P8->x];
            split.shader_splineTerrafector.Vars()->setBuffer("indexData", splines.indexData_LOD8);
            split.shader_splineTerrafector.drawIndexedInstanced(_renderContext, 64 * 6, splines.numIndex_LOD8[P8->y][P8->x]);
        }
        else if (_pTile->lod >= 6)
        {
            quadtree_tile* P6 = _pTile;
            while (P6->lod > 6) P6 = P6->parent;
            split.shader_splineTerrafector.Vars()["gConstantBuffer"]["startOffset"] = splines.startOffset_LOD6[P6->y][P6->x];
            split.shader_splineTerrafector.Vars()->setBuffer("indexData", splines.indexData_LOD6);
            split.shader_splineTerrafector.drawIndexedInstanced(_renderContext, 64 * 6, splines.numIndex_LOD6[P6->y][P6->x]);
        }
        else if (_pTile->lod >= 4)
        {
            quadtree_tile* P4 = _pTile;
            while (P4->lod > 4) P4 = P4->parent;
            split.shader_splineTerrafector.Vars()["gConstantBuffer"]["startOffset"] = splines.startOffset_LOD4[P4->y][P4->x];
            split.shader_splineTerrafector.Vars()->setBuffer("indexData", splines.indexData_LOD4);
            split.shader_splineTerrafector.drawIndexedInstanced(_renderContext, 64 * 6, splines.numIndex_LOD4[P4->y][P4->x]);
        }
    }




    // TOP #################################################################################################################
    if (_pTile->lod >= 6)
    {
        gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD6_top.getTile((_pTile->y >> (_pTile->lod - 6)) * 64 + (_pTile->x >> (_pTile->lod - 6)));
        if (tile)
        {
            if (tile->numBlocks > 0)
            {
                split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
            }
        }
    }
    else if (_pTile->lod >= 4)
    {
        gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD4_top.getTile((_pTile->y >> (_pTile->lod - 4)) * 16 + (_pTile->x >> (_pTile->lod - 4)));
        if (tile)
        {
            if (tile->numBlocks > 0)
            {
                split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
            }
        }
    }
}



void terrainManager::onFrameRender(RenderContext* _renderContext, const Fbo::SharedPtr& _fbo, Camera::SharedPtr _camera, GraphicsState::SharedPtr _graphicsState, GraphicsState::Viewport _viewport, Texture::SharedPtr _hdrHalfCopy)
{
    //gisReload(_camera->getPosition());

   


    glm::mat4 V = toGLM(_camera->getViewMatrix());
    glm::mat4 P = toGLM(_camera->getProjMatrix());
    glm::mat4 VP = toGLM(_camera->getViewProjMatrix());
    rmcv::mat4 view, proj, viewproj;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            view[j][i] = V[j][i];
            proj[j][i] = P[j][i];
            viewproj[j][i] = VP[j][i];
        }
    }


    {
        /*
           mouseVegYaw += 0.002f;

           glm::vec3 camPos;
           camPos.y = sin(mouseVegPitch);
           camPos.x = cos(mouseVegPitch) * sin(mouseVegYaw);
           camPos.z = cos(mouseVegPitch) * cos(mouseVegYaw);
           _camera->setPosition(camPos * mouseVegOrbit);
           _camera->setTarget(glm::vec3(0, groveTree.treeHeight * 0.5f, 0));
          */
    }

    if (terrainMode == 4)
    {
        

        {
            FALCOR_PROFILE("SOLVE_glider");
            //ParaGlider.solve(0.001f);
            for (int i = 0; i < 1; i++)
            {
                paraRuntime.solve(0.0005f);
            }
        }
        //ParaGlider.pack();
        paraRuntime.pack(paraRuntime.ribbon, paraRuntime.packedRibbons, paraRuntime.ribbonCount, paraRuntime.changed);


        {
            FALCOR_PROFILE("SOLVE_AIR");
            //AirSim.update(0.001f);
        }
        //AirSim.pack();

        glm::vec3 camPos;
        camPos.y = sin(mouseVegPitch);
        camPos.x = cos(mouseVegPitch) * sin(mouseVegYaw + paraRuntime.pilotYaw);
        camPos.z = cos(mouseVegPitch) * cos(mouseVegYaw + paraRuntime.pilotYaw);
        _camera->setPosition(paraRuntime.ROOT + camPos * mouseVegOrbit);
        _camera->setTarget(paraRuntime.ROOT);
        _camera->setFarPlane(40000);
    }

    if ((terrainMode == 0) || (terrainMode == 4))
    {

        {
            FALCOR_PROFILE("skydome");

            triangleShader.State()->setFbo(_fbo);
            triangleShader.State()->setViewport(0, _viewport, true);
            triangleShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
            triangleShader.Vars()["gConstantBuffer"]["eye"] = _camera->getPosition();
            triangleShader.State()->setRasterizerState(split.rasterstateSplines);
            triangleShader.State()->setBlendState(split.blendstateSplines);
            //triangleShader.drawInstanced(_renderContext, 36, 1);
            //triangleShader.renderIndirect(_renderContext, split.drawArgs_clippedloddedplants);
        }



        if (_plantMaterial::static_materials_veg.modified)
        {
            auto& block = ribbonShader.Vars()->getParameterBlock("textures");
            ShaderVar& var = block->findMember("T");        // FIXME pre get
            _plantMaterial::static_materials_veg.setTextures(var);
            _plantMaterial::static_materials_veg.modified = false;
            groveTree.bChanged = true;
        }

        if (_plantMaterial::static_materials_veg.modifiedData)
        {
            _plantMaterial::static_materials_veg.rebuildStructuredBuffer();
            _plantMaterial::static_materials_veg.modifiedData = false;
            groveTree.bChanged = true;
        }

        if (groveTree.bChanged)
        {
            auto& block = ribbonShader.Vars()->getParameterBlock("textures");
            ShaderVar& var = block->findMember("T");        // FIXME pre get
            _plantMaterial::static_materials_veg.setTextures(var);

            _plantMaterial::static_materials_veg.rebuildStructuredBuffer();

            ribbonData->setBlob(groveTree.packedRibbons, 0, groveTree.numBranchRibbons * sizeof(unsigned int) * 6);
            groveTree.bChanged = false;

            ribbonShader.State()->setFbo(_fbo);
            ribbonShader.State()->setViewport(0, _viewport, true);

            ribbonShader.Vars()["gConstantBuffer"]["fakeShadow"] = 4;
            ribbonShader.Vars()["gConstantBuffer"]["objectScale"] = float3(objectScale, objectScale, objectScale);
            ribbonShader.Vars()["gConstantBuffer"]["objectOffset"] = objectOffset;
            ribbonShader.Vars()["gConstantBuffer"]["radiusScale"] = radiusScale;

            

            ribbonShader.State()->setRasterizerState(split.rasterstateSplines);
            ribbonShader.State()->setBlendState(split.blendstateSplines);
        }





        if (groveTree.numBranchRibbons > 1)
        {
            FALCOR_PROFILE("ribbonShader");
            ribbonShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
            ribbonShader.Vars()["gConstantBuffer"]["eyePos"] = _camera->getPosition();


            static float spacing = 1.0f;
            if (spacingFromExtents) {
                spacing = ribbonSpacing * groveTree.extents.x;
            }
            ribbonShader.Vars()["gConstantBuffer"]["offset"] = float3(-ribbonInstanceNumber * spacing * 0.5f, 0, -ribbonInstanceNumber * spacing * 0.5f);
            ribbonShader.Vars()["gConstantBuffer"]["repeatScale"] = spacing;
            ribbonShader.Vars()["gConstantBuffer"]["numSide"] = ribbonInstanceNumber;

            ribbonShader.Vars()["gConstantBuffer"]["objectScale"] = float3(objectScale, objectScale, objectScale);
            ribbonShader.Vars()["gConstantBuffer"]["objectOffset"] = objectOffset;
            ribbonShader.Vars()["gConstantBuffer"]["radiusScale"] = radiusScale;

            // lighting
            ribbonShader.Vars()["gConstantBuffer"]["Ao_depthScale"] = static_Ao_depthScale;
            ribbonShader.Vars()["gConstantBuffer"]["sunTilt"] = static_sunTilt;
            ribbonShader.Vars()["gConstantBuffer"]["bDepth"] = static_bDepth;
            ribbonShader.Vars()["gConstantBuffer"]["bScale"] = static_bScale;

            static float time = 0.0f;
            time += 0.01f;  // FIXME I NEED A Timer
            ribbonShader.Vars()["gConstantBuffer"]["time"] = time;

            ribbonShader.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);


            ribbonShader.drawInstanced(_renderContext, groveTree.numBranchRibbons, ribbonInstanceNumber * ribbonInstanceNumber);
        }


        // PARAGLIDER BUILDER
        // ########################################################################################################################

        if ((terrainMode == 4) && paraRuntime.changed)
            //if ((terrainMode == 4) && AirSim.changed)
        {
            paraRuntime.changed = false;
            ribbonInstanceNumber = 1;
            ribbonData->setBlob(paraRuntime.packedRibbons, 0, paraRuntime.ribbonCount * sizeof(unsigned int) * 6);
            //ribbonData->setBlob(AirSim.packedRibbons, 0, AirSim.ribbonCount * sizeof(unsigned int) * 6);


            ribbonShader.State()->setFbo(_fbo);
            ribbonShader.State()->setViewport(0, _viewport, true);

            ribbonShader.Vars()["gConstantBuffer"]["fakeShadow"] = 4;
            ribbonShader.Vars()["gConstantBuffer"]["objectScale"] = float3(objectScale, objectScale, objectScale);
            ribbonShader.Vars()["gConstantBuffer"]["objectOffset"] = objectOffset;
            ribbonShader.Vars()["gConstantBuffer"]["radiusScale"] = radiusScale;

            ribbonShader.State()->setRasterizerState(split.rasterstateSplines);
            ribbonShader.State()->setBlendState(split.blendstateSplines);
        }

        if ((terrainMode == 4) && paraRuntime.ribbonCount > 1)
            //if ((terrainMode == 4) && AirSim.ribbonCount > 1)
        {
            ribbonShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
            ribbonShader.Vars()["gConstantBuffer"]["eyePos"] = _camera->getPosition();

            static float spacing = 1.0f;

            ribbonShader.Vars()["gConstantBuffer"]["offset"] = float3(0, 0, 0);
            ribbonShader.Vars()["gConstantBuffer"]["repeatScale"] = spacing;
            ribbonShader.Vars()["gConstantBuffer"]["numSide"] = 1;

            ribbonShader.Vars()["gConstantBuffer"]["objectScale"] = float3(objectScale, objectScale, objectScale);
            ribbonShader.Vars()["gConstantBuffer"]["objectOffset"] = objectOffset;
            ribbonShader.Vars()["gConstantBuffer"]["radiusScale"] = radiusScale;

            // lighting
            ribbonShader.Vars()["gConstantBuffer"]["Ao_depthScale"] = static_Ao_depthScale;
            ribbonShader.Vars()["gConstantBuffer"]["sunTilt"] = static_sunTilt;
            ribbonShader.Vars()["gConstantBuffer"]["bDepth"] = static_bDepth;
            ribbonShader.Vars()["gConstantBuffer"]["bScale"] = static_bScale;

            ribbonShader.Vars()["gConstantBuffer"]["ROOT"] = paraRuntime.ROOT;

            static float time = 0.0f;
            time += 0.01f;  // FIXME I NEED A Timer
            ribbonShader.Vars()["gConstantBuffer"]["time"] = time;

            ribbonShader.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);


            ribbonShader.drawInstanced(_renderContext, paraRuntime.ribbonCount, 1);
            //ribbonShader.drawInstanced(_renderContext, AirSim.ribbonCount, 1);

        }


        //return;
    }

    {

        FALCOR_PROFILE("terrainManager");

        //terrainShader.State() = _graphicsState;
        terrainShader.State()->setFbo(_fbo);
        terrainShader.State()->setViewport(0, _viewport, true);
        terrainShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        terrainShader.Vars()["gConstantBuffer"]["eye"] = _camera->getPosition();
        terrainShader.renderIndirect(_renderContext, split.drawArgs_tiles);

    }

    {
        FALCOR_PROFILE("terrainQuadPlants");
        terrainSpiteShader.State()->setFbo(_fbo);
        terrainSpiteShader.State()->setViewport(0, _viewport, true);
        terrainSpiteShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        terrainSpiteShader.Vars()["gConstantBuffer"]["eye"] = _camera->getPosition();

        glm::vec3 D = glm::vec3(view[2][0], view[2][1], -view[2][2]);
        glm::vec3 R = glm::normalize(glm::cross(glm::vec3(0, 1, 0), D));
        terrainSpiteShader.Vars()["gConstantBuffer"]["right"] = R;
        terrainSpiteShader.Vars()["gConstantBuffer"]["alpha_pass"] = 0;

        terrainSpiteShader.State()->setRasterizerState(split.rasterstateSplines);
        terrainSpiteShader.State()->setBlendState(split.blendstateSplines);

        terrainSpiteShader.renderIndirect(_renderContext, split.drawArgs_quads);

        //terrainSpiteShader.Vars()["gConstantBuffer"]["alpha_pass"] = 1;
        //terrainSpiteShader.renderIndirect(_renderContext, split.drawArgs_quads);
    }

    {

        FALCOR_PROFILE("ribbonShader");

        ribbonShader.State()->setFbo(_fbo);
        ribbonShader.State()->setViewport(0, _viewport, true);
        ribbonShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        ribbonShader.Vars()["gConstantBuffer"]["eyePos"] = _camera->getPosition();

        ribbonShader.State()->setRasterizerState(split.rasterstateSplines);
        ribbonShader.State()->setBlendState(split.blendstateSplines);

        //ribbonShader.drawInstanced(_renderContext, 128, 10024);
//        ribbonShader.renderIndirect(_renderContext, split.drawArgs_clippedloddedplants);

        //buffer_lookup_plants

        /*
        triangleShader.State()->setFbo(_fbo);
        triangleShader.State()->setViewport(0, _viewport, true);
        triangleShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        triangleShader.Vars()["gConstantBuffer"]["eye"] = _camera->getPosition();
        triangleShader.State()->setRasterizerState(split.rasterstateSplines);
        triangleShader.State()->setBlendState(split.blendstateSplines);
        triangleShader.renderIndirect(_renderContext, split.drawArgs_clippedloddedplants);
        */

    }

    if ((splines.numStaticSplines || splines.numDynamicSplines) && showRoadSpline && !bSplineAsTerrafector)
    {
        FALCOR_PROFILE("splines");

        split.shader_spline3D.State()->setFbo(_fbo);
        split.shader_spline3D.State()->setRasterizerState(split.rasterstateSplines);
        split.shader_spline3D.State()->setBlendState(split.blendstateSplines);
        split.shader_spline3D.State()->setDepthStencilState(split.depthstateAll);
        split.shader_spline3D.State()->setViewport(0, _viewport, true);

        //split.shader_spline3D.Vars()["gConstantBuffer"]["view"] = view;
        //split.shader_spline3D.Vars()["gConstantBuffer"]["proj"] = proj;
        split.shader_spline3D.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        split.shader_spline3D.Vars()["gConstantBuffer"]["alpha"] = gis_overlay.splineOverlayStrength;

        split.shader_spline3D.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);

        auto& block = split.shader_spline3D.Vars()->getParameterBlock("gmyTextures");
        ShaderVar& var = block->findMember("T");        // FIXME pre get
        terrafectorEditorMaterial::static_materials.setTextures(var);

        if (splines.numDynamicSplines > 0)
        {
            split.shader_spline3D.Vars()->setBuffer("splineData", splines.dynamic_bezierData);
            split.shader_spline3D.Vars()->setBuffer("indexData", splines.dynamic_indexData);
            split.shader_spline3D.drawIndexedInstanced(_renderContext, 64 * 6, splines.numDynamicSplinesIndex);
        }
        else if (splines.numStaticSplines > 0)
        {
            split.shader_spline3D.Vars()->setBuffer("splineData", splines.bezierData);
            split.shader_spline3D.Vars()->setBuffer("indexData", splines.indexData);
            split.shader_spline3D.drawIndexedInstanced(_renderContext, 64 * 6, splines.numStaticSplinesIndex);
        }
    }

    {
        FALCOR_PROFILE("sprites");
        if (showRoadOverlay) {
            mSpriteRenderer.onRender(_camera, _renderContext, _fbo, _viewport, nullptr);
        }

    }

    {
        FALCOR_PROFILE("terrain_under_mouse");
        compute_TerrainUnderMouse.Vars()["gConstants"]["mousePos"] = mousePosition;
        compute_TerrainUnderMouse.Vars()["gConstants"]["mouseDir"] = mouseDirection;
        compute_TerrainUnderMouse.Vars()["gConstants"]["mouseCoords"] = mouseCoord;
        compute_TerrainUnderMouse.Vars()->setTexture("gHDRBackbuffer", _fbo->getColorTexture(0));

        compute_TerrainUnderMouse.dispatch(_renderContext, 32, 1);

        _renderContext->copyResource(split.buffer_feedback_read.get(), split.buffer_feedback.get());

        const uint8_t* pData = (uint8_t*)split.buffer_feedback_read->map(Buffer::MapType::Read);
        std::memcpy(&split.feedback, pData, sizeof(GC_feedback));
        split.buffer_feedback_read->unmap();

        mouse.hit = false;
        if (split.feedback.tum_idx > 0)
        {
            mouse.hit = true;
            mouse.terrain = split.feedback.tum_Position;
            mouse.cameraHeight = split.feedback.heightUnderCamera;
            mouse.toGround = mouse.terrain - _camera->getPosition();
            mouse.mouseToHeightRatio = glm::length(mouse.toGround) / (_camera->getPosition().y - mouse.cameraHeight);
            if (!ImGui::IsMouseDown(1)) {
                mouse.pan = mouse.terrain;
                mouse.panDistance = glm::length(mouse.toGround);
            }
            if (!ImGui::IsMouseDown(2)) mouse.orbit = mouse.terrain;

        }
    }

    if (debug)
    {
        glm::vec4 srcRect = glm::vec4(0, 0, tile_numPixels, tile_numPixels);
        glm::vec4 dstRect;

        for (int i = 0; i < 8; i++)
        {
            dstRect = glm::vec4(250 + i * 150, 60, 250 + i * 150 + 128, 60 + 128);
            _renderContext->blit(split.tileFbo->getColorTexture(i)->getSRV(0, 1, 0, 1), _fbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Linear);
        }

        dstRect = glm::vec4(250 + 8 * 150, 60, 250 + 8 * 150 + tile_numPixels * 2, 60 + tile_numPixels * 2);
        _renderContext->blit(split.debug_texture->getSRV(0, 1, 0, 1), _fbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Point);


        //dstRect = vec4(512, 612, 1024, 1124);
        //pRenderContext->blit(mpCompressed_Normals->getSRV(0, 1, 0, 1), _fbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Linear);
        char debugTxt[1024];
        TextRenderer::setColor(float3(1, 1, 1));
        sprintf(debugTxt, "%d of %d tiles used", (int)m_used.size(), (int)m_tiles.size());
        TextRenderer::render(_renderContext, debugTxt, _fbo, { 100, 300 });

        sprintf(debugTxt, "%d tiles / triangles  %d  {%d}max", split.feedback.numTerrainTiles, split.feedback.numTerrainVerts, split.feedback.maxTriangles);
        TextRenderer::render(_renderContext, debugTxt, _fbo, { 100, 320 });

        sprintf(debugTxt, "%d tiles with quads    %d total quads  {%d}max", split.feedback.numQuadTiles, split.feedback.numQuads, split.feedback.maxQuads);
        TextRenderer::render(_renderContext, debugTxt, _fbo, { 100, 340 });

        sprintf(debugTxt, "%d tiles with plants    %d total plants (%d)  {%d}max", split.feedback.numPlantTiles, split.feedback.numPlants, split.feedback.numPlantBlocks, split.feedback.maxPlants);
        TextRenderer::render(_renderContext, debugTxt, _fbo, { 100, 360 });

        sprintf(debugTxt, "%d clipped plants", split.feedback.numPostClippedPlants);
        TextRenderer::render(_renderContext, debugTxt, _fbo, { 100, 380 });



        for (uint L = 0; L < 18; L++)
        {
            sprintf(debugTxt, "%3d %5d %5d %5d %5d", L, split.feedback.numTiles[L], split.feedback.numSprite[L], split.feedback.numPlantsLOD[L], split.feedback.numTris[L]);
            TextRenderer::render(_renderContext, debugTxt, _fbo, { 100, 450 + L * 20 });
        }


    }
}



void terrainManager::bake_start(bool _toMAX)
{
    bakeToMax = _toMAX;
    char name[256];
    sprintf(name, "%s/bake/EVO/tiles.txt", settings.dirRoot.c_str());
    bake.txt_file = fopen(name, "w");
    bake.tileInfoForBinaryExport.clear();
    bake.inProgress = true;

    elevationMap oneTile;
    uint total = (uint)elevationTileHashmap.size();
    std::map<uint32_t, heightMap>::iterator itt;

    for (itt = elevationTileHashmap.begin(); itt != elevationTileHashmap.end(); itt++)
    {

        if (itt->second.lod < 3) {
            oneTile.lod = itt->second.lod;
            oneTile.y = itt->second.y;
            oneTile.x = itt->second.x;
            oneTile.origin = itt->second.origin;
            oneTile.tileSize = itt->second.size;
            oneTile.heightOffset = itt->second.hgt_offset;
            oneTile.heightScale = itt->second.hgt_scale;

            bake.tileInfoForBinaryExport.push_back(oneTile);
            fprintf(bake.txt_file, "%d %d %d %d %f %f, size %f, (%f, %f)\n", oneTile.lod, oneTile.y, oneTile.x, split.bakeSize, oneTile.heightOffset, oneTile.heightScale, oneTile.tileSize, oneTile.origin.x, oneTile.origin.y);
        }
    }

    bake.tileHash.clear();

    int highestLOD = 8;
    if (bakeToMax)
    {
        highestLOD = 7;
    }
    for (uint lod = 4; lod < highestLOD; lod++) {
        uint size = 1 << lod;
        unsigned char value;

        sprintf(name, "%s/bake/lod%d.raw", settings.dirRoot.c_str(), lod);
        FILE* tilemap = fopen(name, "rb");
        if (tilemap) {
            for (uint y = 0; y < size; y++) {
                for (uint x = 0; x < size; x++) {
                    fread(&value, 1, 1, tilemap);
                    if (value == 255) {
                        bake.tileHash.push_back(getHashFromTileCoords(lod, y, x));
                    }
                }
            }

            fclose(tilemap);
        }
    }
    bake.itterator = bake.tileHash.begin();
}

void replaceAlltm(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

void terrainManager::bake_frame()
{
    if (bake.inProgress)
    {
        uint32_t hash = *bake.itterator;
        uint32_t lod = hash >> 28;
        bake_Setup(0.0f, lod, (hash >> 14) & 0x3FFF, hash & 0x3FFF, bake.renderContext);

        bake.itterator++;
        if (bake.itterator == bake.tileHash.end()) {
            bake.inProgress = false;
            fclose(bake.txt_file);

            if (!bakeToMax)
            {
                // the end. Now save tileInfoForBinaryExport
                char name[256];
                sprintf(name, "%s/bake/EVO/tiles.list", settings.dirRoot.c_str());
                FILE* info_file = fopen(name, "wb");
                if (info_file) {
                    fwrite(&bake.tileInfoForBinaryExport[0], sizeof(elevationMap), bake.tileInfoForBinaryExport.size(), info_file);
                    fclose(info_file);
                }


                std::string command;
                sprintf(name, "attrib -r %s/Elevations/*.*", (settings.dirExport).c_str());
                command = name;
                replaceAlltm(command, "/", "\\");
                system(command.c_str());
                sprintf(name, "copy /Y %s/bake/EVO/*.* %s/Elevations/", settings.dirRoot.c_str(), (settings.dirExport).c_str());
                command = name;
                replaceAlltm(command, "/", "\\");
                replaceAlltm(command, "\\Y", "/Y");
                system(command.c_str());
            }
        }
    }
}




void terrainManager::bake_Setup(float _size, uint _lod, uint _y, uint _x, RenderContext* _renderContext)
{
    uint32_t hashParent = 0;
    uint32_t hashLod = _lod;
    uint32_t hashY = _y;
    uint32_t hashX = _x;
    std::map<uint32_t, heightMap>::iterator it_Bicubic;

    hashParent = getHashFromTileCoords(hashLod, hashY, hashX);
    it_Bicubic = elevationTileHashmap.find(hashParent);
    while (it_Bicubic == elevationTileHashmap.end())
    {
        hashLod -= 1;
        hashY = hashY >> 1;
        hashX = hashX >> 1;
        hashParent = getHashFromTileCoords(hashLod, hashY, hashX);
        it_Bicubic = elevationTileHashmap.find(hashParent);
    }

    if (it_Bicubic != elevationTileHashmap.end())
    {
        if (hashParent > 0)			// now search for it and cache it
        {
            textureCacheElement map;

            if (!elevationCache.get(hashParent, map))
            {
                std::array<unsigned short, 1048576> data;

                ojph::codestream codestream;
                ojph::j2c_infile j2c_file;
                j2c_file.open(elevationTileHashmap[hashParent].filename.c_str());
                codestream.enable_resilience();
                codestream.set_planar(false);
                codestream.read_headers(&j2c_file);
                codestream.create();
                int next_comp;

                for (int i = 0; i < 1024; ++i)
                {
                    ojph::line_buf* line = codestream.pull(next_comp);
                    int32_t* dp = line->i32;
                    for (int j = 0; j < 1024; j++) {
                        int16_t val = (int16_t)*dp++;
                        data[i * 1024 + j] = val;
                    }
                }
                codestream.close();

                map.texture = Texture::create2D(1024, 1024, Falcor::ResourceFormat::R16Unorm, 1, 1, data.data(), Resource::BindFlags::ShaderResource);
                elevationCache.set(hashParent, map);
            }

            split.compute_tileBicubic.Vars()->setTexture("gInput", map.texture);
        }

        const uint32_t cs_w = split.bakeSize / tile_cs_ThreadSize;

        const float size = (settings.size / (1 << _lod));
        const float outerSize = size * tile_numPixels / tile_InnerPixels;
        const float pixelSize = outerSize / split.bakeSize;
        float halfsize = ecotopeSystem::terrainSize / 2.f;
        float blocksize = ecotopeSystem::terrainSize / 16.f;
        const float2 origin = float2(-halfsize, -halfsize) + float2(_x * size, _y * size) - float2(pixelSize * 4 * 4, pixelSize * 4 * 4);

        {
            const glm::vec4 clearColor(0.1f, 0.1f, 0.9f, 1.0f);
            _renderContext->clearFbo(split.bakeFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
        }

        {
            textureCacheElement map;
            if (!elevationCache.get(hashParent, map)) {
                split.compute_tileBicubic.Vars()->setTexture("gInput", map.texture);
            }

            heightMap& elevationMap = elevationTileHashmap[hashParent];

            // FIXME this is ghorrible 
            // cs_tile_Bicubic takes the inner tile coordinates, and compensates for 4 pixerls internall
            // so if we compendate for 12 here the other 4 is in teh shader
            const float2 originBC = float2(-halfsize, -halfsize) + float2(_x * size, _y * size) - float2(pixelSize * 4 * 3, pixelSize * 4 * 3);
            float2 bicubicOffset = (originBC - elevationMap.origin) / elevationMap.size;
            float S = pixelSize / elevationMap.size;
            float2 bicubicSize = float2(S, S);



            split.compute_tileBicubic.Vars()["gConstants"]["offset"] = bicubicOffset;
            split.compute_tileBicubic.Vars()["gConstants"]["size"] = bicubicSize;
            split.compute_tileBicubic.Vars()["gConstants"]["hgt_offset"] = elevationMap.hgt_offset;
            split.compute_tileBicubic.Vars()["gConstants"]["hgt_scale"] = elevationMap.hgt_scale;

            //split.compute_tileBicubic.Vars()->setTexture("gOuthgt_TEMPTILLTR", split.bakeFbo->getColorTexture(0));
            split.compute_tileBicubic.Vars()->setTexture("gOutput", split.bakeFbo->getColorTexture(0));
            split.compute_tileBicubic.dispatch(_renderContext, cs_w, cs_w);
            //split.compute_tileBicubic.Vars()->setTexture("gOuthgt_TEMPTILLTR", split.tileFbo->getColorTexture(0));
        }

        bake_RenderTopdown(_size, _lod, _y, _x, _renderContext);

        // FIXME MAY HAVE TO ECOTOPE SHADER in future

        _renderContext->flush(true);

        char outName[512];

        if (bakeToMax)
        {
            sprintf(outName, "%s/bake/fakeVeg/%d_%d_%d_albedo.png", settings.dirRoot.c_str(), _lod, _y, _x);
            split.bakeFbo->getColorTexture(1).get()->captureToFile(0, 0, outName, Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);

            sprintf(outName, "%s/bake/fakeVeg/%d_%d_%d_ect_1.png", settings.dirRoot.c_str(), _lod, _y, _x);
            split.bakeFbo->getColorTexture(4).get()->captureToFile(0, 0, outName, Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);

            sprintf(outName, "%s/bake/fakeVeg/%d_%d_%d_ect_2.png", settings.dirRoot.c_str(), _lod, _y, _x);
            split.bakeFbo->getColorTexture(5).get()->captureToFile(0, 0, outName, Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);

            sprintf(outName, "%s/bake/fakeVeg/%d_%d_%d_ect_3.png", settings.dirRoot.c_str(), _lod, _y, _x);
            split.bakeFbo->getColorTexture(6).get()->captureToFile(0, 0, outName, Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);

            sprintf(outName, "%s/bake/fakeVeg/%d_%d_%d_ect_4.png", settings.dirRoot.c_str(), _lod, _y, _x);
            split.bakeFbo->getColorTexture(7).get()->captureToFile(0, 0, outName, Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);
        }


        _renderContext->resourceBarrier(split.bakeFbo->getColorTexture(0).get(), Resource::State::CopySource);
        uint32_t subresource = bake.copy_texture->getSubresourceIndex(0, 0);
        std::vector<glm::uint8> textureData = gpDevice->getRenderContext()->readTextureSubresource(split.bakeFbo->getColorTexture(0).get(), 0);
        float* pF = (float*)textureData.data();
        uint ret_size = (uint)textureData.size();
        float A = pF[0];
        float B = pF[1000];
        float C = pF[10000];
        float D = pF[30000];
        bool bCM = true;

        if (bakeToMax)
        {
            sprintf(outName, "%s/bake/fakeVeg/%d_%d_%d_hgt.raw", settings.dirRoot.c_str(), _lod, _y, _x);
            FILE* file = fopen(outName, "wb");
            if (file) {
                fwrite(pF, sizeof(float), split.bakeSize * split.bakeSize, file);
                fclose(file);
            }
        }

        // find minmax
        float max = 0;
        float min = 100000000;
        for (int y = 0; y < split.bakeSize * split.bakeSize; y++) {
            if (pF[y] < min) min = pF[y];
            if (pF[y] > max) max = pF[y];
        }

        min -= 0.5f;
        float range = max - min + 0.5f;

        fprintf(bake.txt_file, "%d %d %d %d %f %f %f, size %f, (%f, %f)\n", _lod, _y, _x, split.bakeSize, min, max, range, outerSize, origin.x, origin.y);

        elevationMap oneTile;
        oneTile.lod = _lod;
        oneTile.y = _y;
        oneTile.x = _x;
        oneTile.origin = origin;
        oneTile.tileSize = outerSize;
        oneTile.heightOffset = min;
        oneTile.heightScale = range;

        bake.tileInfoForBinaryExport.push_back(oneTile);

        // Now output JP2
        if (!bakeToMax)
        {
            //// FIXME load from settings file CEREAL
            sprintf(outName, "%s/bake/EVO/hgt_%d_%d_%d.jp2", settings.dirRoot.c_str(), _lod, _y, _x);
            ojph::codestream codestream;
            ojph::j2c_outfile j2c_file;
            j2c_file.open(outName);

            {
                // set up
                ojph::param_siz siz = codestream.access_siz();
                siz.set_image_extent(ojph::point(split.bakeSize, split.bakeSize));
                siz.set_num_components(1);
                siz.set_component(0, ojph::point(1, 1), 16, false);		//??? unsure about the subsampling point()
                siz.set_image_offset(ojph::point(0, 0));
                siz.set_tile_size(ojph::size(split.bakeSize, split.bakeSize));
                siz.set_tile_offset(ojph::point(0, 0));

                ojph::param_cod cod = codestream.access_cod();
                cod.set_num_decomposition(5);
                cod.set_block_dims(64, 64);
                //if (num_precints != -1)
                //	cod.set_precinct_size(num_precints, precinct_size);
                cod.set_progression_order("RPCL");
                cod.set_color_transform(false);
                cod.set_reversible(false);
                codestream.access_qcd().set_irrev_quant(bake.quality);


                {
                    codestream.write_headers(&j2c_file);

                    int next_comp;
                    ojph::line_buf* cur_line = codestream.exchange(NULL, next_comp);

                    for (uint i = 0; i < split.bakeSize; ++i)
                    {

                        int32_t* dp = cur_line->i32;
                        for (uint j = 0; j < split.bakeSize; j++) {
                            float fVal = pF[i * 1024 + j];
                            int32_t shortVal = (unsigned short)((fVal - min) / range * 65536.0f);
                            *dp++ = shortVal;
                        }
                        cur_line = codestream.exchange(cur_line, next_comp);
                    }
                }

            }
            codestream.flush();
            codestream.close();
        }
    }

}




void terrainManager::bake_RenderTopdown(float _size, uint _lod, uint _y, uint _x, RenderContext* _renderContext)
{
    float terrainSize = settings.size;
    uint gridSize = (uint)pow(2, _lod);
    float tileSize = terrainSize / gridSize;
    float tileOuterSize = tileSize * 256.0f / 248.0f;

    // set up the camera -----------------------
    float s = tileOuterSize / 2.0f;
    float x = (_x + 0.5f) * tileSize - (terrainSize / 2.0f);
    float z = (_y + 0.5f) * tileSize - (terrainSize / 2.0f);
    glm::mat4 V, P, VP;
    V[0] = glm::vec4(1, 0, 0, 0);
    V[1] = glm::vec4(0, 0, 1, 0);
    V[2] = glm::vec4(0, -1, 0, 0);
    V[3] = glm::vec4(-x, z, 0, 1);


    P = glm::orthoLH(-s, s, -s, s, -10000.0f, 10000.0f);

    //viewproj = view * proj;
    VP = P * V;    //??? order

    rmcv::mat4 view, proj, viewproj;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            view[j][i] = V[j][i];
            proj[j][i] = P[j][i];
            viewproj[j][i] = VP[j][i];
        }
    }


    {
        split.shader_meshTerrafector.State()->setFbo(split.bakeFbo);
        split.shader_meshTerrafector.State()->setRasterizerState(split.rasterstateSplines);
        split.shader_meshTerrafector.State()->setBlendState(split.blendstateRoadsCombined);
        split.shader_meshTerrafector.State()->setDepthStencilState(split.depthstateAll);

        split.shader_meshTerrafector.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        split.shader_meshTerrafector.Vars()["gConstantBuffer"]["overlayAlpha"] = 1.0f;



        auto& block = split.shader_meshTerrafector.Vars()->getParameterBlock("gmyTextures");
        ShaderVar& var = block->findMember("T");        // FIXME pre get
        terrafectorEditorMaterial::static_materials.setTextures(var);
    }

    //if (bSplineAsTerrafector)           // Now render the roadNetwork
    {
        split.shader_splineTerrafector.State()->setFbo(split.bakeFbo);
        split.shader_splineTerrafector.State()->setRasterizerState(split.rasterstateSplines);
        split.shader_splineTerrafector.State()->setDepthStencilState(split.depthstateAll);
        split.shader_splineTerrafector.State()->setBlendState(split.blendstateRoadsCombined);

        split.shader_splineTerrafector.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        split.shader_splineTerrafector.Vars()["gConstantBuffer"]["startOffset"] = 0;
        split.shader_splineTerrafector.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);

        auto& block = split.shader_splineTerrafector.Vars()->getParameterBlock("gmyTextures");
        ShaderVar& var = block->findMember("T");        // FIXME pre get
        terrafectorEditorMaterial::static_materials.setTextures(var);

        split.shader_splineTerrafector.Vars()->setBuffer("splineData", splines.bezierData);     // not created yet
    }


    // Mesh bake low
    if (gis_overlay.bakeBakeOnlyData)
    {
        if (_lod >= 4)
        {
            gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD4_bakeLow.getTile((_y >> (_lod - 4)) * 16 + (_x >> (_lod - 4)));
            if (tile)
            {
                if (tile->numBlocks > 0)
                {
                    split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                    split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                    split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
                }
            }
        }

        //if (bSplineAsTerrafector)           // Now render the roadNetwork
        {
            split.shader_splineTerrafector.Vars()->setBuffer("indexData", splines.indexDataBakeOnly);
            split.shader_splineTerrafector.drawIndexedInstanced(_renderContext, 64 * 6, splines.numStaticSplinesBakeOnlyIndex);
        }

        if (_lod >= 4)
        {
            gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD4_bakeHigh.getTile((_y >> (_lod - 4)) * 16 + (_x >> (_lod - 4)));
            if (tile)
            {
                if (tile->numBlocks > 0)
                {
                    split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                    split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                    split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
                }
            }
        }
    }






    if (_lod >= 6)
    {
        gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD6.getTile((_y >> (_lod - 6)) * 64 + (_x >> (_lod - 6)));
        if (tile)
        {
            if (tile->numBlocks > 0)
            {
                split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
            }
        }
    }
    else if (_lod >= 4)
    {
        gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD4.getTile((_y >> (_lod - 4)) * 16 + (_x >> (_lod - 4)));
        if (tile)
        {
            if (tile->numBlocks > 0)
            {
                split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
            }
        }
    }
    else if (_lod >= 2)
    {
        gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD2.getTile((_y >> (_lod - 2)) * 4 + (_x >> (_lod - 2)));
        if (tile)
        {
            if (tile->numBlocks > 0)
            {
                split.shader_meshTerrafector.Vars()->setBuffer("vertexData", tile->vertex);
                split.shader_meshTerrafector.Vars()->setBuffer("indexData", tile->index);
                split.shader_meshTerrafector.drawInstanced(_renderContext, 128 * 3, tile->numBlocks);
            }
        }
    }





    //??? should probably be in the roadnetwork code, but look at the optimize step first
    //if (bSplineAsTerrafector)           // Now render the roadNetwork
    {
        if (_lod >= 8)
        {
            split.shader_splineTerrafector.Vars()["gConstantBuffer"]["startOffset"] = splines.startOffset_LOD8[_y >> (_lod - 8)][_x >> (_lod - 8)];
            split.shader_splineTerrafector.Vars()->setBuffer("indexData", splines.indexData_LOD8);
            split.shader_splineTerrafector.drawIndexedInstanced(_renderContext, 64 * 6, splines.numIndex_LOD8[_y >> (_lod - 8)][_x >> (_lod - 8)]);
        }
        else if (_lod >= 6)
        {
            split.shader_splineTerrafector.Vars()["gConstantBuffer"]["startOffset"] = splines.startOffset_LOD6[_y >> (_lod - 6)][_x >> (_lod - 6)];
            split.shader_splineTerrafector.Vars()->setBuffer("indexData", splines.indexData_LOD6);
            split.shader_splineTerrafector.drawIndexedInstanced(_renderContext, 64 * 6, splines.numIndex_LOD6[_y >> (_lod - 6)][_x >> (_lod - 6)]);
        }
        else if (_lod >= 4)
        {
            split.shader_splineTerrafector.Vars()["gConstantBuffer"]["startOffset"] = splines.startOffset_LOD4[_y >> (_lod - 4)][_x >> (_lod - 4)];
            split.shader_splineTerrafector.Vars()->setBuffer("indexData", splines.indexData_LOD4);
            split.shader_splineTerrafector.drawIndexedInstanced(_renderContext, 64 * 6, splines.numIndex_LOD4[_y >> (_lod - 4)][_x >> (_lod - 4)]);
        }
    }

}



void terrainManager::sceneToMax()
{

    std::filesystem::path path;
    FileDialogFilterVec filters = { {"fbx"}, {"obj"} };
    if (saveFileDialog(filters, path))
    {

        char filename[1024];
        uint numMeshes = 0;
        for (auto& tile : m_used)
        {
            bool surface = tile->parent && tile->parent->main_ShouldSplit && tile->child[0] == nullptr && (tile->lod >= exportLodMin) && (tile->lod <= exportLodMax);
            surface = surface || (tile->lod == exportLodMax);   // add all top level
            if (surface)
            {
                numMeshes++;
                //sprintf(filename, "%s/albedo_%d.jpg", path.root_directory().string().c_str(), tile->index);
                //compressed_Albedo_Array->captureToFile(0, tile->index, filename, Bitmap::FileFormat::JpegFile, Bitmap::ExportFlags::None);

                //std::vector<uint8_t> textureData;
                //uint32_t subresource = compressed_Albedo_Array->getSubresourceIndex(tile->index, 0);
                //textureData = pContext->readTextureSubresource(this, subresource);
            }
        }

        aiScene* scene = new aiScene;
        scene->mRootNode = new aiNode();

        scene->mMaterials = new aiMaterial * [numMeshes];
        scene->mNumMaterials = numMeshes;

        scene->mMeshes = new aiMesh * [numMeshes];
        scene->mRootNode->mMeshes = new unsigned int[numMeshes];
        for (int i = 0; i < numMeshes; i++)
        {
            scene->mMeshes[i] = nullptr;
            scene->mRootNode->mMeshes[i] = i;
        }
        scene->mNumMeshes = numMeshes;
        scene->mRootNode->mNumMeshes = numMeshes;


        uint meshCount = 0;
        for (auto& tile : m_used)
        {
            bool surface = tile->parent && tile->parent->main_ShouldSplit && tile->child[0] == nullptr && (tile->lod >= exportLodMin) && (tile->lod <= exportLodMax);
            surface = surface || (tile->lod == exportLodMax);   // add all top level
            if (surface)
            {
                scene->mMaterials[meshCount] = new aiMaterial();
                scene->mMeshes[meshCount] = new aiMesh();
                scene->mMeshes[meshCount]->mMaterialIndex = meshCount;
                auto pMesh = scene->mMeshes[meshCount];

                pMesh->mFaces = new aiFace[248 * 248];
                pMesh->mNumFaces = 248 * 248;
                pMesh->mPrimitiveTypes = aiPrimitiveType_POLYGON;

                for (uint j = 0; j < 248; j++)
                {
                    for (uint i = 0; i < 248; i++)
                    {
                        aiFace& face = pMesh->mFaces[j * 248 + i];

                        face.mIndices = new unsigned int[4];
                        face.mNumIndices = 4;

                        face.mIndices[0] = ((j + 4) * 256) + (i + 4 + 1);
                        face.mIndices[1] = ((j + 4) * 256) + (i + 4);
                        face.mIndices[2] = ((j + 4) * 256) + 256 + (i + 4);
                        face.mIndices[3] = ((j + 4) * 256) + 256 + (i + 4 + 1);
                    }
                }

                pMesh->mVertices = new aiVector3D[256 * 256];
                pMesh->mNumVertices = 256 * 256;

                //pMesh->mTextureCoords[0] = new aiVector3D[256 * 256];
                //pMesh->mNumUVComponents[0] = 256 * 256;

                std::vector<glm::uint8> textureData = gpDevice->getRenderContext()->readTextureSubresource(height_Array.get(), tile->index);
                float* pF = (float*)textureData.data();
                uint ret_size = (uint)textureData.size();

                for (uint y = 0; y < 256; y++)
                {
                    for (uint x = 0; x < 256; x++)
                    {
                        uint index = y * 256 + x;
                        pMesh->mVertices[index] = aiVector3D(tile->origin.x + x * tile->size / 248.0f, pF[index], tile->origin.z + y * tile->size / 248.0f);
                        //pMesh->mTextureCoords[0][index] = aiVector3D((float)x / 255.0f, (float)y / 255.0f, 0);
                    }
                }
                /*
                sprintf(filename, "F:/_sceneTest/hgt_%d.raw", tile->index);
                FILE* file = fopen(filename, "wb");
                if (file) {
                    fwrite(pF, sizeof(float), tile_numPixels * tile_numPixels, file);
                    fclose(file);
                }
                */

                meshCount++;
            }
        }


        Exporter exp;
        if (path.string().find("fbx") != std::string::npos)
        {
            exp.Export(scene, "fbx", path.string());
        }
        else if (path.string().find("obj") != std::string::npos)
        {
            exp.Export(scene, "obj", path.string());
        }
    }


    //sprintf(filename, "F:/_sceneTest/test.obj");
    //exp.Export(scene, "obj", filename);
}



void terrainManager::updateDynamicRoad(bool _bezierChanged) {
    // active road ----------------------------------------------------------------------------------------------------------------
    splineTest.bSegment = false;
    splineTest.bVertex = false;
    splineTest.testDistance = 1000;
    splineTest.bCenter = false;
    splineTest.cornerNum = -1;
    splineTest.pos = split.feedback.tum_Position;
    splineTest.bStreetview = false;

    //mRoadNetwork.testHit(feedback.tum_Position);

    static bool bRefresh;
    if (_bezierChanged) { bRefresh = true; }
    if (mRoadNetwork.currentRoad || mRoadNetwork.currentIntersection)
    {
        if (bRefresh || mRoadNetwork.isDirty)
        {
            mRoadNetwork.updateDynamicRoad();
            splines.numDynamicSplines = __min(splines.maxDynamicBezier, (int)roadNetwork::staticBezierData.size());
            splines.numDynamicSplinesIndex = __min(splines.maxDynamicIndex, (int)roadNetwork::staticIndexData.size());
            if (splines.numDynamicSplines > 0)
            {
                splines.dynamic_bezierData->setBlob(roadNetwork::staticBezierData.data(), 0, splines.numDynamicSplines * sizeof(cubicDouble));
                splines.dynamic_indexData->setBlob(roadNetwork::staticIndexData.data(), 0, splines.numDynamicSplinesIndex * sizeof(bezierLayer));

                mRoadNetwork.isDirty = false;
            }
        }
        if (!_bezierChanged) { bRefresh = false; }
    }
    else
    {
        splines.numDynamicSplines = 0;
        splines.numDynamicSplinesIndex = 0;
    }

    mRoadNetwork.intersectionSelectedRoad = nullptr;


    if (mRoadNetwork.currentRoad && mRoadNetwork.currentRoad->points.size() >= 3)
    {
        mSpriteRenderer.clearDynamic();
        mRoadNetwork.currentRoad->testAgainstPoint(&splineTest);

        // mouse to spline markers ---------------------------------------------------------------------------------------

        if (splineTest.bVertex) {
            mSpriteRenderer.pushMarker(splineTest.returnPos, 4, 3.0f);
        }
        if (splineTest.bSegment) {
            mSpriteRenderer.pushMarker(splineTest.returnPos, 1);
        }

        // show selection
        for (auto& point : mRoadNetwork.currentRoad->points)
        {
            mSpriteRenderer.pushMarker(point.anchor, 2 - point.constraint, 1.5f);

        }

        if (mRoadNetwork.AI_path_mode)
        {

            int ssize = (int)mRoadNetwork.pathBezier.segments.size();

            for (int i = 0; i < ssize - 1; i++) {
                float3 O1 = mRoadNetwork.pathBezier.segments[i].optimalPos;
                float3 O2 = mRoadNetwork.pathBezier.segments[i + 1].optimalPos;
                mSpriteRenderer.pushLine(O1, O2, 1, 0.5f);

                O1 = mRoadNetwork.pathBezier.segments[i].A;
                O2 = mRoadNetwork.pathBezier.segments[i].B;
                mSpriteRenderer.pushLine(O1, O2, 1, 0.3f);
            }
        }

        mSpriteRenderer.loadDynamic();

    }






    if (mRoadNetwork.currentIntersection)
    {
        mRoadNetwork.updateDynamicRoad();

        mSpriteRenderer.clearDynamic();
        intersection* I = mRoadNetwork.currentIntersection;

        //mSpriteRenderer.pushMarker(I->anchor, 3 + I->buildQuality, 4.0);			// anchor
        float distAnchor = glm::length(I->anchor - splineTest.pos);
        if (distAnchor < splineTest.testDistance && distAnchor < 8.0f) {
            splineTest.testDistance = distAnchor;
            splineTest.bCenter = true;
            splineTest.bStreetview = false;
        }


        uint RLsize = (uint)I->roadLinks.size();
        for (uint i = 0; i < RLsize; i++) {
            intersectionRoadLink* R = &I->roadLinks[i];
            intersectionRoadLink* RB = &I->roadLinks[(i + RLsize - 1) % RLsize];


            // First test against all the roads attached, BUT EXCLUDE the first vertex
            if (R->roadPtr->testAgainstPoint(&splineTest, false)) {
                mRoadNetwork.intersectionSelectedRoad = R->roadPtr;
                splineTest.bCenter = false;
            }

            float3 Z = R->corner_A - splineTest.pos;
            Z.y = 0;
            float distCorner = glm::length(Z);
            if ((distCorner < 3.0) && (distCorner < splineTest.testDistance)) {
                splineTest.testDistance = distCorner;
                splineTest.bCenter = false;
                splineTest.bSegment = false;
                splineTest.bVertex = false;
                splineTest.cornerNum = i;
                splineTest.cornerFlag = 0;
                mRoadNetwork.intersectionSelectedRoad = nullptr;

            }


            float distA;
            if (R->cornerType == typeOfCorner::artistic) {
                Z = (R->corner_A + R->cornerTangent_A) - splineTest.pos;
                Z.y = 0;
                distA = glm::length(Z);
                if (distA < 3.0 && distA < splineTest.testDistance) {
                    splineTest.testDistance = distA;
                    splineTest.bCenter = false;
                    splineTest.bSegment = false;
                    splineTest.bVertex = false;
                    splineTest.cornerNum = i;
                    splineTest.cornerFlag = -1;
                    mRoadNetwork.intersectionSelectedRoad = nullptr;

                }
            }


            if (RB->cornerType == typeOfCorner::artistic) {
                Z = (R->corner_B + R->cornerTangent_B) - splineTest.pos;
                Z.y = 0;
                distA = glm::length(Z);
                if (distA < 5.0 && distA < splineTest.testDistance) {
                    splineTest.testDistance = distA;
                    splineTest.bCenter = false;
                    splineTest.bSegment = false;
                    splineTest.bVertex = false;
                    splineTest.cornerNum = i;
                    splineTest.cornerFlag = 1;
                    mRoadNetwork.intersectionSelectedRoad = nullptr;
                }
            }


            Z = (I->anchor + R->tangentVector) - splineTest.pos;
            Z.y = 0;
            distA = glm::length(Z);
            if (distA < 5.0 && distA < splineTest.testDistance) {
                splineTest.testDistance = distA;
                splineTest.bCenter = false;
                splineTest.cornerNum = i;
                splineTest.cornerFlag = 2;
                mRoadNetwork.intersectionSelectedRoad = nullptr;
            }




            mSpriteRenderer.pushMarker(R->corner_A, 2 - R->cornerType, 0.5f);						// corner anchor
            //if (R->cornerType == typeOfCorner::artistic)
            {
                mSpriteRenderer.pushLine(R->corner_A, R->corner_A + R->cornerTangent_A, R->cornerType, 0.2f);
                mSpriteRenderer.pushLine(R->corner_B, R->corner_B + R->cornerTangent_B, RB->cornerType, 0.2f);
                mSpriteRenderer.pushMarker(R->corner_A + R->cornerTangent_A, 2 - R->cornerType, 0.5f);			// anchor
                mSpriteRenderer.pushMarker(R->corner_B + R->cornerTangent_B, 2 - RB->cornerType, 0.5f);			// anchor
            }

            mSpriteRenderer.pushLine(I->anchor, I->anchor + R->tangentVector, 0, 0.3f);
            mSpriteRenderer.pushMarker(I->anchor + R->tangentVector, 0, 0.5);			// center tangent
        }






        // now show what we selected ----------------------------------------------------------------------------------
        if (mRoadNetwork.intersectionSelectedRoad) {
            mSpriteRenderer.pushMarker(splineTest.returnPos, 4, 1.0f);
        }
        else {
            if (splineTest.bCenter) {
                mSpriteRenderer.pushMarker(I->anchor, 4, 1.0);
            }
            else if (splineTest.cornerNum >= 0) {
                intersectionRoadLink* R = &I->roadLinks[splineTest.cornerNum];
                switch (splineTest.cornerFlag) {
                case -1:
                    mSpriteRenderer.pushMarker(R->corner_A + R->cornerTangent_A, 4, 1.0);
                    break;
                case 0:
                    mSpriteRenderer.pushMarker(R->corner_A, 4, 1.0);
                    break;
                case 1:
                    mSpriteRenderer.pushMarker(R->corner_B + R->cornerTangent_B, 4, 1.0);
                    break;
                case 2:
                    mSpriteRenderer.pushMarker(I->anchor + R->tangentVector, 4, 1.0);
                    break;
                }
            }
        }

        mSpriteRenderer.loadDynamic();
    }

}



bool terrainManager::onKeyEvent(const KeyboardEvent& keyEvent)
{
    bool keyPressed = (keyEvent.type == KeyboardEvent::Type::KeyPressed);

    splineTest.bCtrl = keyEvent.hasModifier(Input::Modifier::Ctrl);
    splineTest.bShift = keyEvent.hasModifier(Input::Modifier::Shift);
    splineTest.bAlt = keyEvent.hasModifier(Input::Modifier::Alt);

    if (keyEvent.type == KeyboardEvent::Type::KeyPressed)
    {
        if (keyEvent.key == Input::Key::Space)   paraRuntime.playpause();
        if (keyEvent.key == Input::Key::S)   paraRuntime.runstep();


        if (keyEvent.hasModifier(Input::Modifier::Ctrl))
        {
            if (keyEvent.key == Input::Key::D)          // debug
            {
                debug = !debug;
            }
            if (keyEvent.key == Input::Key::O)
            {
                gisReload(split.feedback.tum_Position);
            }
            if (keyEvent.key == Input::Key::G)
            {
                showGUI = !showGUI;
            }
            if (keyEvent.key == Input::Key::B)
            {
                //bakeOneVeg = true;
                bakeVegetation();
            }
        }

        switch (terrainMode)
        {
        case 3:


            // selection 
            if (mRoadNetwork.currentRoad && splineTest.bCtrl)
            {
                switch (keyEvent.key)
                {
                case Input::Key::A:	// select all
                    mRoadNetwork.currentRoad->selectAll();
                    mRoadNetwork.selectionType = 2;
                    return true;
                    break;
                case Input::Key::D:	// select all
                    mRoadNetwork.currentRoad->clearSelection();
                    mRoadNetwork.selectionType = 0;
                    return true;
                    break;
                }
            }

            if (keyPressed)
            {
                if (keyEvent.hasModifier(Input::Modifier::Ctrl))		// CTRL + key
                {
                    switch (keyEvent.key)
                    {
                    case Input::Key::R:
                        terrafectors.loadPath(settings.dirRoot + "/terrafectors", settings.dirRoot + "/bake");
                        break;
                    case Input::Key::C:		// copy
                        if (splineTest.bVertex) {
                            mRoadNetwork.copyVertex(splineTest.index);
                        }
                        break;
                    case Input::Key::V:		// paste
                        if (keyEvent.hasModifier(Input::Modifier::Shift))		// CTRL + SHIFT  + key
                        {
                            if (splineTest.bVertex) {
                                mRoadNetwork.pasteVertexMaterial(splineTest.index);
                            }
                        }
                        else {
                            if (splineTest.bVertex) {
                                mRoadNetwork.pasteVertexGeometry(splineTest.index);
                            }
                        }
                        break;
                    }
                }
                else
                {
                    switch (keyEvent.key)
                    {
                    case Input::Key::Del:
                        if (mRoadNetwork.currentIntersection) mRoadNetwork.deleteCurrentIntersection();
                        if (mRoadNetwork.currentRoad) mRoadNetwork.deleteCurrentRoad();
                        break;
                    case Input::Key::Space:
                        if (mRoadNetwork.currentRoad) mRoadNetwork.showMaterials = !mRoadNetwork.showMaterials;
                        break;

                    case Input::Key::Q:
                        if (mRoadNetwork.currentRoad) mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                        if (mRoadNetwork.currentIntersection) mRoadNetwork.solveIntersection();
                        break;
                    }


                }

                mRoadNetwork.updateAllRoads();
                updateDynamicRoad(true);
            }

            switch (keyEvent.key)
            {
            case Input::Key::Escape:	// deselect all
                if (keyPressed) {
                    {
                        if (mRoadNetwork.popupVisible) {
                            ImGui::CloseCurrentPopup();
                            // stuff that, dpoesnt work no key or mouse events, cunsumed by popup
                        }
                        else {
                            mRoadNetwork.currentRoad = nullptr;
                            mRoadNetwork.currentIntersection = nullptr;
                            mRoadNetwork.intersectionSelectedRoad = nullptr;
                            mRoadNetwork.updateAllRoads();
                            updateDynamicRoad(true);
                        }
                    }
                }
                return true;
                break;
            case Input::Key::S:
                if (keyPressed && keyEvent.hasModifier(Input::Modifier::Ctrl)) {
                    mRoadNetwork.quickSave();
                }
                break;
            case Input::Key::X:
                // road new spline
                if (keyPressed && keyEvent.hasModifier(Input::Modifier::Ctrl)) {
                    if (mRoadNetwork.currentIntersection) {
                        mRoadNetwork.currentRoad = nullptr;
                        mRoadNetwork.currentIntersection_findRoads();
                        //updateStaticRoad();
                        updateDynamicRoad(true);
                    }
                }
                return true;
                break;
            case Input::Key::G:
                // road new spline
                if (keyPressed) {
                    mRoadNetwork.newRoadSpline();
                    //updateStaticRoad();
                }
                return true;
                break;


            case Input::Key::F:
                // new node
                if (keyPressed) {
                    mRoadNetwork.newIntersection();
                    mRoadNetwork.currentIntersection->updatePosition(split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    mRoadNetwork.currentIntersection_findRoads();
                    updateDynamicRoad(true);
                }
                return true;
                break;
            case Input::Key::H:
                if (keyPressed && splineTest.bVertex) {
                    mRoadNetwork.breakCurrentRoad(splineTest.index);
                    //updateStaticRoad();
                    updateDynamicRoad(true);
                }
                return true;
                break;
            case Input::Key::J:
                if (keyPressed) {
                    mRoadNetwork.deleteCurrentRoad();
                    //updateStaticRoad();
                    updateDynamicRoad(true);
                }
                return true;
                break;
            case Input::Key::K:
                if (keyPressed) {
                    mRoadNetwork.deleteCurrentIntersection();
                    //updateStaticRoad();
                    updateDynamicRoad(true);
                }
                return true;
                break;
            case Input::Key::B:
                if (keyPressed) {
                    if (splineTest.bVertex && keyEvent.hasModifier(Input::Modifier::Ctrl)) {
                        mRoadNetwork.currentRoad->points[splineTest.index].isBridge = !mRoadNetwork.currentRoad->points[splineTest.index].isBridge;
                    }
                    else
                    {
                        bSplineAsTerrafector = !bSplineAsTerrafector;
                        reset(true);
                    }
                }
                return true;
                break;
            case Input::Key::N:
                if (keyPressed) {
                    showRoadOverlay = !showRoadOverlay;
                }
                return true;
                break;
            case Input::Key::M:
                if (keyPressed) {
                    showRoadSpline = !showRoadSpline;
                }
                return true;
                break;
            case Input::Key::Up:
                if (splineTest.bVertex) {
                    if (keyEvent.hasModifier(Input::Modifier::Ctrl)) {
                        mRoadNetwork.currentRoad->points[splineTest.index].addTangent(1.0f);
                        mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    }
                    else {
                        mRoadNetwork.currentRoad->points[splineTest.index].addHeight(0.1f);
                        mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    }
                }
                break;

            case Input::Key::Down:
                if (splineTest.bVertex) {
                    if (keyEvent.hasModifier(Input::Modifier::Ctrl)) {
                        mRoadNetwork.currentRoad->points[splineTest.index].addTangent(-1.0f);
                        mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    }
                    else {
                        mRoadNetwork.currentRoad->points[splineTest.index].addHeight(-0.1f);
                        mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    }
                }
                break;

            case Input::Key::Left:
                if (splineTest.bVertex) {
                    mRoadNetwork.currentRoad->points[splineTest.index].camber += 0.01f;
                    mRoadNetwork.currentRoad->points[splineTest.index].constraint = e_constraint::camber;
                    mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                }
                break;
            case Input::Key::Right:
                if (splineTest.bVertex) {
                    mRoadNetwork.currentRoad->points[splineTest.index].camber -= 0.01f;
                    mRoadNetwork.currentRoad->points[splineTest.index].constraint = e_constraint::camber;
                    mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                }
                break;
            }

            break;
        default:
            break;
        }
    }
    return false;
}


bool terrainManager::onMouseEvent(const MouseEvent& mouseEvent, glm::vec2 _screenSize, glm::vec2 _mouseScale, glm::vec2 _mouseOffset, Camera::SharedPtr _camera)
{
    if ((terrainMode == 0) || (terrainMode == 4))
    {
        glm::vec2 pos = (mouseEvent.pos * _mouseScale) + _mouseOffset;
        glm::vec2 diff;
        if (pos.x > 0 && pos.x < 1 && pos.y > 0 && pos.y < 1)
        {
            pos.y = 1.0 - pos.y;
            diff = pos - mousePositionOld;
            mousePositionOld = pos;
        }


        switch (mouseEvent.type)
        {
        case MouseEvent::Type::Move:
        {
            if (ImGui::IsMouseDown(1))
            {
                mouseVegPitch += diff.y * 10.0f;
                mouseVegPitch = glm::clamp(mouseVegPitch, -1.f, 1.5f);

                mouseVegYaw += diff.x * 20.0f;
                while (mouseVegYaw < 0) mouseVegYaw += 6.28318530718f;
                while (mouseVegYaw > 6.28318530718f) mouseVegYaw -= 6.28318530718f;
            }
        }
        break;
        case MouseEvent::Type::Wheel:
        {
            float scale = 1.0 - mouseEvent.wheelDelta.y / 6.0f;
            mouseVegOrbit *= scale;
            mouseVegOrbit = glm::clamp(mouseVegOrbit, 0.1f, 5000.f);
        }
        break;
        }

        //mouseVegYaw += 0.002f;
        if ((terrainMode == 0))
        {
            glm::vec3 camPos;
            camPos.y = sin(mouseVegPitch);
            camPos.x = cos(mouseVegPitch) * sin(mouseVegYaw);
            camPos.z = cos(mouseVegPitch) * cos(mouseVegYaw);
            _camera->setPosition(camPos * mouseVegOrbit);
            _camera->setTarget(glm::vec3(0, groveTree.treeHeight * 0.5f, 0));
        }
        if ((terrainMode == 4))
        {
            //_camera->setTarget(paraRuntime.ROOT);
        }
        return true;
    }
    else {
        bool bEdit = onMouseEvent_Roads(mouseEvent, _screenSize, _camera);

        if (!bEdit)
        {
            glm::vec2 pos = (mouseEvent.pos * _mouseScale) + _mouseOffset;
            if (pos.x > 0 && pos.x < 1 && pos.y > 0 && pos.y < 1)
            {
                pos.y = 1.0 - pos.y;
                glm::vec3 N = glm::unProject(glm::vec3(pos * _screenSize, 0.0f), toGLM(_camera->getViewMatrix()), toGLM(_camera->getProjMatrix()), glm::vec4(0, 0, _screenSize));
                glm::vec3 F = glm::unProject(glm::vec3(pos * _screenSize, 1.0f), toGLM(_camera->getViewMatrix()), toGLM(_camera->getProjMatrix()), glm::vec4(0, 0, _screenSize));
                mouseDirection = glm::normalize(F - N);
                screenSize = _screenSize;
                mousePosition = _camera->getPosition();
                mouseCoord = mouseEvent.pos * _screenSize;



                switch (mouseEvent.type)
                {
                case MouseEvent::Type::Move:
                {
                    //if (bRightButton)  // PAN
                    if (ImGui::IsMouseDown(1))
                    {
                        glm::vec3 newPos = mouse.pan - mouseDirection * (_camera->getPosition().y - mouse.pan.y) / fabs(mouseDirection.y);
                        glm::vec3 deltaPos = newPos - _camera->getPosition();

                        glm::vec3 newTarget = _camera->getTarget() + deltaPos;
                        _camera->setPosition(newPos);
                        _camera->setTarget(newTarget);
                        hasChanged = true;
                    }

                    // orbit
                    if (ImGui::IsMouseDown(2))
                    {
                        glm::vec3 D = _camera->getTarget() - _camera->getPosition();
                        glm::vec3 U = glm::vec3(0, 1, 0);
                        glm::vec3 R = glm::normalize(glm::cross(U, D));
                        glm::vec2 diff = pos - mousePositionOld;
                        glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), diff.x * 10.0f, glm::vec3(0, 1, 0));

                        glm::vec3 Dnorm = glm::normalize(D);
                        if ((Dnorm.y < -0.99f) && (diff.y < 0)) diff.y = 0;
                        if ((Dnorm.y > 0.0f) && (diff.y > 0)) diff.y = 0;

                        glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), diff.y * 10.0f, R);
                        mouse.toGround = glm::vec4(mouseDirection, 0) * mouse.orbitRadius * yaw * pitch;
                        glm::vec4 newDir = glm::vec4(D, 0) * yaw * pitch;

                        _camera->setPosition(mouse.orbit - mouse.toGround);
                        _camera->setTarget(mouse.orbit - mouse.toGround + glm::vec3(newDir));
                        hasChanged = true;
                    }
                    mousePositionOld = pos;
                }
                break;
                case MouseEvent::Type::Wheel:
                {
                    if (mouse.hit)
                    {
                        float scale = 1.0 - mouseEvent.wheelDelta.y / 6.0f;
                        mouse.toGround *= scale;
                        glm::vec3 newPos = mouse.terrain - mouse.toGround;
                        glm::vec3 deltaPos = newPos - _camera->getPosition();
                        glm::vec3 newTarget = _camera->getTarget() + deltaPos;

                        _camera->setPosition(newPos);
                        _camera->setTarget(newTarget);
                        hasChanged = true;
                    }
                }
                break;
                case MouseEvent::Type::ButtonDown:
                {
                    if (mouseEvent.button == Input::MouseButton::Middle)
                    {
                        mouse.orbitRadius = glm::length(mouse.toGround);
                    }
                }
                break;
                case MouseEvent::Type::ButtonUp:
                {
                }
                break;
                }


                // rebuild from new camera
                {
                    glm::vec3 N = glm::unProject(glm::vec3(pos * _screenSize, 0.0f), toGLM(_camera->getViewMatrix()), toGLM(_camera->getProjMatrix()), glm::vec4(0, 0, _screenSize));
                    glm::vec3 F = glm::unProject(glm::vec3(pos * _screenSize, 1.0f), toGLM(_camera->getViewMatrix()), toGLM(_camera->getProjMatrix()), glm::vec4(0, 0, _screenSize));
                    mouseDirection = glm::normalize(F - N);
                    screenSize = _screenSize;
                    mousePosition = _camera->getPosition();
                    mouseCoord = mouseEvent.pos * _screenSize;
                }

                return false;
            }
        }
        return true;
    }
}



bool terrainManager::onMouseEvent_Roads(const MouseEvent& mouseEvent, glm::vec2 _screenSize, Camera::SharedPtr _camera)
{
    static bool bDragEvent;
    static glm::vec2 prevPos;
    glm::vec2 diff = mouseEvent.pos - prevPos;
    prevPos = mouseEvent.pos;

    switch (mouseEvent.type)
    {
    case MouseEvent::Type::Move:
    {
        mRoadNetwork.testHit(split.feedback.tum_Position);

        bDragEvent = true;
        if (bLeftButton)
        {
            if (splineTest.bVertex) {
                if (mRoadNetwork.currentRoad) {
                    if (splineTest.bCtrl)
                    {
                        mRoadNetwork.currentRoad->points[splineTest.index].B += diff.x * 10.0f;
                        mRoadNetwork.currentRoad->points[splineTest.index].B = __min(1, __max(0, mRoadNetwork.currentRoad->points[splineTest.index].B));
                    }
                    else if (splineTest.bShift)
                    {
                        mRoadNetwork.currentRoad->points[splineTest.index].tangent_Offset += diff.x * 100.0f;
                        mRoadNetwork.currentRoad->points[splineTest.index].solveMiddlePos();
                    }
                    else if (splineTest.bAlt)
                    {
                    }
                    else
                    {
                        mRoadNetwork.currentRoad->points[splineTest.index].setAnchor(split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    }

                    mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                }
                else if (mRoadNetwork.intersectionSelectedRoad) {
                    mRoadNetwork.intersectionSelectedRoad->points[splineTest.index].setAnchor(split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    mRoadNetwork.intersectionSelectedRoad->solveRoad(splineTest.index);
                    mRoadNetwork.solveIntersection();
                }
            }

            if (mRoadNetwork.currentIntersection) {
                if (splineTest.bCenter) {
                    mRoadNetwork.currentIntersection->updatePosition(split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    {
                        FALCOR_PROFILE("solveIntersection");
                        mRoadNetwork.solveIntersection();
                    }
                }

                if (splineTest.cornerNum >= 0) {
                    mRoadNetwork.currentIntersection->updateCorner(split.feedback.tum_Position, split.feedback.tum_Normal, splineTest.cornerNum, splineTest.cornerFlag);
                    mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerType = typeOfCorner::artistic;
                }
            }
            updateDynamicRoad(true);
        }
    }
    break;
    case MouseEvent::Type::Wheel:

        if (splineTest.bShift) {
            if (splineTest.bVertex && splineTest.bCtrl) {
                if (mRoadNetwork.currentRoad) {
                    mRoadNetwork.incrementLane(-1, mouseEvent.wheelDelta.y / 10.0f, mRoadNetwork.currentRoad, splineTest.bAlt);
                    mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    updateDynamicRoad(true);
                }
                else if (mRoadNetwork.intersectionSelectedRoad) {
                    mRoadNetwork.incrementLane(-1, mouseEvent.wheelDelta.y / 10.0f, mRoadNetwork.intersectionSelectedRoad, splineTest.bAlt);
                    mRoadNetwork.intersectionSelectedRoad->solveRoad(splineTest.index);
                    mRoadNetwork.solveIntersection();
                    updateDynamicRoad(true);
                }
                return true;
            }
        }
        else {
            if (splineTest.bVertex && splineTest.bCtrl) {
                if (mRoadNetwork.currentRoad) {
                    mRoadNetwork.incrementLane(splineTest.index, mouseEvent.wheelDelta.y / 10.0f, mRoadNetwork.currentRoad, splineTest.bAlt);
                    mRoadNetwork.currentRoad->solveRoad(splineTest.index);
                    updateDynamicRoad(true);
                }
                else if (mRoadNetwork.intersectionSelectedRoad) {
                    mRoadNetwork.incrementLane(splineTest.index, mouseEvent.wheelDelta.y / 10.0f, mRoadNetwork.intersectionSelectedRoad, splineTest.bAlt);
                    mRoadNetwork.intersectionSelectedRoad->solveRoad(splineTest.index);
                    mRoadNetwork.solveIntersection();
                    updateDynamicRoad(true);
                }
                return true;
            }


            if (splineTest.bCtrl && mRoadNetwork.currentIntersection) {
                if (mRoadNetwork.currentIntersection && splineTest.cornerNum >= 0) {
                    if (splineTest.bAlt) {
                        mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerRadius += mouseEvent.wheelDelta.y / 20.0f;
                        mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerType = typeOfCorner::radius;
                    }
                    else {
                        mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerRadius += mouseEvent.wheelDelta.y / 5.0f;
                        mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerType = typeOfCorner::radius;
                    }
                    if (mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerRadius < 0.2f) mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerRadius = 0.2f;
                    mRoadNetwork.solveIntersection();
                    updateDynamicRoad(true);
                    return true;
                }
            }
        }
        break;

    case MouseEvent::Type::ButtonDown:
    {
        if (mouseEvent.button == Input::MouseButton::Left)
        {
            bLeftButton = true;

            // SUB selection
            if (splineTest.bCtrl) {
                if (splineTest.bVertex) {
                    mRoadNetwork.selectionType = 1;
                    int mid = (mRoadNetwork.selectFrom + mRoadNetwork.selectTo) / 2;
                    if (splineTest.index < mRoadNetwork.selectFrom) {
                        mRoadNetwork.selectFrom = splineTest.index;
                    }
                    else if (splineTest.index > mRoadNetwork.selectTo) {
                        mRoadNetwork.selectTo = splineTest.index;
                    }
                    else {
                        if (splineTest.index < mid) {
                            mRoadNetwork.selectFrom = splineTest.index;
                        }
                        else {
                            mRoadNetwork.selectTo = splineTest.index;
                        }
                    }

                }
            }


            // Selection but now add all the possible subselections
            if (splineTest.bCtrl) {								// selection


                if (mRoadNetwork.AI_path_mode) {
                    mRoadNetwork.addRoad();
                }
                else
                {
                    mRoadNetwork.doSelect(split.feedback.tum_Position);

                    if (mRoadNetwork.bHIT) {
                        mRoadNetwork.setEditRight(mRoadNetwork.hitRoadRight);
                        mRoadNetwork.setEditLane(mRoadNetwork.hitRoadLane);
                        //updateStaticRoad();

                    }
                    updateDynamicRoad(true);

                }
            }
            else if (mRoadNetwork.currentRoad) {
                if (!splineTest.bVertex && !splineTest.bSegment) {
                    mRoadNetwork.currentRoad->pushPoint(split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    updateDynamicRoad(true);
                }

                if (splineTest.bSegment) {
                    mRoadNetwork.currentRoad->insertPoint(splineTest.index, split.feedback.tum_Position, split.feedback.tum_Normal, m_tiles[split.feedback.tum_idx].lod);
                    updateDynamicRoad(true);
                }
            }

        }
        if (mouseEvent.button == Input::MouseButton::Middle)
        {
            bMiddelButton = true;
        }
        if (mouseEvent.button == Input::MouseButton::Right)
        {
            bRightButton = true;

            if (splineTest.bCtrl)
            {
                if (mRoadNetwork.currentRoad && splineTest.bVertex && mRoadNetwork.currentRoad->points.size() > splineTest.index) {
                    mRoadNetwork.currentRoad->deletePoint(splineTest.index);
                    updateDynamicRoad(true);
                }

                if (mRoadNetwork.currentIntersection) {
                    if (splineTest.cornerNum >= 0) {
                        mRoadNetwork.currentIntersection->roadLinks[splineTest.cornerNum].cornerType = typeOfCorner::automatic;
                        mRoadNetwork.solveIntersection();
                        updateDynamicRoad(true);
                    }
                    if (splineTest.bCenter) {
                        mRoadNetwork.currentIntersection->customNormal = false;
                    }
                }
            }
        }
    }
    break;


    case MouseEvent::Type::ButtonUp:
    {
        if (mouseEvent.button == Input::MouseButton::Left)
        {
            bLeftButton = false;
            bDragEvent = false;
        }
        if (mouseEvent.button == Input::MouseButton::Middle)
        {
            bMiddelButton = false;
            bDragEvent = false;
        }
        if (mouseEvent.button == Input::MouseButton::Right)
        {
            //bShowRoadPopup = true;
            bDragEvent = false;
            //popupPos = mouseEvent.pos * m_ScreenSize;
            bRightButton = false;
        }
    }
    break;
    }
    return false;
}


void terrainManager::onHotReload(HotReloadFlags reloaded)
{}



bool testBezier(cubicDouble& _bez, glm::vec3 _pos, float _size)
{
    return false;
}



void terrainManager::bezierRoadstoLOD(uint _lod)
{
#define outsideLine 	(roadNetwork::staticIndexData[i].A >> 31) & 0x1
#define insideLine 		(roadNetwork::staticIndexData[i].A >> 30) & 0x1
#define index  			roadNetwork::staticIndexData[i].A & 0x1ffff

    //std::vector<bezierLayer> lod4[16][16];
    //std::vector<bezierLayer> lod6[64][64];
    //std::vector<bezierLayer> lod8[256][256];
    // clear
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            splines.lod4[y][x].clear();
        }
    }
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            splines.lod6[y][x].clear();
        }
    }
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            splines.lod8[y][x].clear();
        }
    }

    fprintf(terrafectorSystem::_logfile, "\n\n\nbezierRoadstoLOD\n");

    for (uint i = 0; i < splines.numStaticSplinesIndex; i++)
    {
        cubicDouble& BEZ = roadNetwork::staticBezierData[index];

        float3 perpStart = glm::normalize(BEZ.data[1][0] - BEZ.data[0][0]);
        float3 perpEnd = glm::normalize(BEZ.data[1][3] - BEZ.data[0][3]);

        float w0 = ((roadNetwork::staticIndexData[i].B >> 14) & 0x3fff) * 0.002f - 16.0f;			// -32 .. 33.536m in mm resolution
        float w1 = (roadNetwork::staticIndexData[i].B & 0x3fff) * 0.002f - 16.0f;

        float3 startInside = float3(BEZ.data[insideLine][0]) + w0 * perpStart;
        float3 startOutside = float3(BEZ.data[outsideLine][0]) + w1 * perpStart;
        float3 endInside = float3(BEZ.data[insideLine][3]) + w0 * perpStart;
        float3 endOutside = float3(BEZ.data[outsideLine][3]) + w1 * perpStart;

        float splineWidth = __max(glm::length(startOutside - startInside), glm::length(endOutside - endInside));

        for (int lod = 4; lod <= 8; lod += 2)
        {
            float scale = 1.0f / pow(2, lod);
            float tileSize = settings.size * scale;
            float pixelSize = tileSize / 248.0f;
            float borderSize = (pixelSize * 4.0f) + splineWidth;    // add splineWidth to compensate for curve
            //??? How to boos tarmac since left right that one is double
            // boost lod4 a little since I dont k ow trarmact yert
            if (lod == 4) splineWidth *= 2.0f; // doesnt work, we need Special splines that includes runoff areas
            if ((lod == 8) || splineWidth > pixelSize)
            {
                float xMin = __min(__min(BEZ.data[0][0].x, BEZ.data[0][3].x), __min(BEZ.data[1][0].x, BEZ.data[1][3].x)) - 80;      // 39 is the buffer size for lod4
                float xMax = __max(__max(BEZ.data[0][0].x, BEZ.data[0][3].x), __max(BEZ.data[1][0].x, BEZ.data[1][3].x)) + 80;
                float yMin = __min(__min(BEZ.data[0][0].z, BEZ.data[0][3].z), __min(BEZ.data[1][0].z, BEZ.data[1][3].z)) - 80;
                float yMax = __max(__max(BEZ.data[0][0].z, BEZ.data[0][3].z), __max(BEZ.data[1][0].z, BEZ.data[1][3].z)) + 80;

                float halfsize = ecotopeSystem::terrainSize / 2.f;
                float blocksize = ecotopeSystem::terrainSize / 16.f;
                uint gMinX = (uint)floor((xMin - borderSize + halfsize) / tileSize);
                uint gMaxX = (uint)ceil((xMax + borderSize + halfsize) / tileSize);
                uint gMinY = (uint)floor((yMin - borderSize + halfsize) / tileSize);
                uint gMaxY = (uint)ceil((yMax + borderSize + halfsize) / tileSize);

                for (int y = gMinY; y < gMaxY; y++) {
                    for (int x = gMinX; x < gMaxX; x++)
                    {
                        switch (lod)
                        {
                        case 4: splines.lod4[y][x].push_back(roadNetwork::staticIndexData[i]); break;
                        case 6: splines.lod6[y][x].push_back(roadNetwork::staticIndexData[i]); break;
                        case 8: splines.lod8[y][x].push_back(roadNetwork::staticIndexData[i]); break;
                        }

                    }
                }
            }
        }
    }

    FILE* file = fopen((settings.dirRoot + "/bake/roadbeziers_lod4.gpu").c_str(), "wb");
    FILE* datafile = fopen((settings.dirRoot + "/bake/roadbeziers_lod4_data.gpu").c_str(), "wb");
    if (file && datafile)
    {
        //uint lod = 4;
        //fwrite(&lod, sizeof(uint), 1, file);

        uint start = 0;
        uint largest = 0;
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                int size = splines.lod4[y][x].size();
                fwrite(&size, sizeof(uint), 1, file);
                fwrite(&start, sizeof(uint), 1, file);

                if (size > 0)
                {
                    largest = __max(largest, size);
                    splines.indexData_LOD4->setBlob(splines.lod4[y][x].data(), start * sizeof(bezierLayer), size * sizeof(bezierLayer));
                    fwrite(splines.lod4[y][x].data(), sizeof(bezierLayer), size, datafile);
                }
                splines.startOffset_LOD4[y][x] = start;
                splines.numIndex_LOD4[y][x] = size;
                start += size;
                //fprintf(terrafectorSystem::_logfile, "%6d", size);
            }
            //fprintf(terrafectorSystem::_logfile, "\n");
        }
        fclose(file);
        fclose(datafile);

        fprintf(terrafectorSystem::_logfile, "\nLOD 4. Total beziers %d from %d.   Most beziers in a block = %d\n", start, splines.numStaticSplinesIndex, largest);
        fprintf(terrafectorSystem::_logfile, "using %3.1f Mb\n", ((float)(start * sizeof(bezierLayer)) / 1024.0f / 1024.0f));
    }



    file = fopen((settings.dirRoot + "/bake/roadbeziers_lod6.gpu").c_str(), "wb");
    datafile = fopen((settings.dirRoot + "/bake/roadbeziers_lod6_data.gpu").c_str(), "wb");
    if (file && datafile)
    {
        //uint lod = 6;
        //fwrite(&lod, sizeof(uint), 1, file);

        uint start = 0;
        uint largest = 0;
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                int size = splines.lod6[y][x].size();
                fwrite(&size, sizeof(uint), 1, file);
                fwrite(&start, sizeof(uint), 1, file);


                if (size > 0)
                {
                    largest = __max(largest, size);
                    splines.indexData_LOD6->setBlob(splines.lod6[y][x].data(), start * sizeof(bezierLayer), size * sizeof(bezierLayer));
                    fwrite(splines.lod6[y][x].data(), sizeof(bezierLayer), size, datafile);
                }
                splines.startOffset_LOD6[y][x] = start;
                splines.numIndex_LOD6[y][x] = size;
                start += size;
            }
        }
        fclose(file);
        fclose(datafile);

        fprintf(terrafectorSystem::_logfile, "\nLOD 6. Total beziers %d from %d.   Most beziers in a block = %d\n", start, splines.numStaticSplinesIndex, largest);
        fprintf(terrafectorSystem::_logfile, "using %3.1f Mb\n", ((float)(start * sizeof(bezierLayer)) / 1024.0f / 1024.0f));
    }


    file = fopen((settings.dirRoot + "/bake/roadbeziers_lod8.gpu").c_str(), "wb");
    datafile = fopen((settings.dirRoot + "/bake/roadbeziers_lod8_data.gpu").c_str(), "wb");
    if (file && datafile)
    {
        //uint lod = 8;
        //fwrite(&lod, sizeof(uint), 1, file);

        uint start = 0;
        uint largest = 0;
        for (int y = 0; y < 256; y++) {
            for (int x = 0; x < 256; x++) {
                int size = splines.lod8[y][x].size();
                fwrite(&size, sizeof(uint), 1, file);
                fwrite(&start, sizeof(uint), 1, file);

                if (size > 0)
                {
                    largest = __max(largest, size);
                    splines.indexData_LOD8->setBlob(splines.lod8[y][x].data(), start * sizeof(bezierLayer), size * sizeof(bezierLayer));
                    fwrite(splines.lod8[y][x].data(), sizeof(bezierLayer), size, datafile);
                }
                splines.startOffset_LOD8[y][x] = start;
                splines.numIndex_LOD8[y][x] = size;
                start += size;
            }
        }
        fclose(file);
        fclose(datafile);

        fprintf(terrafectorSystem::_logfile, "\nLOD 8. Total beziers %d from %d.   Most beziers in a block = %d\n", start, splines.numStaticSplinesIndex, largest);
        fprintf(terrafectorSystem::_logfile, "using %3.1f Mb\n", ((float)(start * sizeof(bezierLayer)) / 1024.0f / 1024.0f));
    }





    file = fopen((settings.dirRoot + "/bake/roadbeziers_bezier.gpu").c_str(), "wb");
    if (file)
    {
        fwrite(roadNetwork::staticBezierData.data(), sizeof(cubicDouble), splines.numStaticSplines, file);
        fclose(file);
    }


}
