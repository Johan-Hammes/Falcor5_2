#include "glider.h"
#include "imgui.h"
#include <imgui_internal.h>

#pragma optimize("", off)


extern float3 AllSummedForce;
extern float3 AllSummedLinedrag;

void setupVert(rvB_Glider* r, int start, float3 pos, float radius, int _mat = 0)
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
        rvB_Glider* rprev = r--;
        float3 tangent = glm::normalize(r->position - rprev->position);
        r->bitangent = tangent;
        rprev->bitangent = tangent;
    }
}









rvPackedGlider rvB_Glider::pack()
{
    float objectScale = 0.002f;  //0.002 for trees
    float radiusScale = 2.0f;//  so biggest radius now objectScale / 2.0f;
    float O = 16384.0f * objectScale * 0.5f;
    float3 objectOffset = float3(O, O * 0.5f, O);

    float static_Ao_depthScale = 0.3f;
    float static_sunTilt = -0.2f;
    float static_bDepth = 20.0f;
    float static_bScale = 0.5f;

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




    rvPackedGlider p;

    p.a = ((type & 0x1) << 31) + (startBit << 30) + (x14 << 16) + y16;
    p.b = (z14 << 18) + ((material & 0x3ff) << 8) + anim8;
    p.c = (up_yaw9 << 23) + (up_pitch8 << 15) + v15;
    p.d = (left_yaw9 << 23) + (left_pitch8 << 15) + (u7 << 8) + radius8;
    p.e = (coneYaw9 << 23) + (conePitch8 << 15) + (cone7 << 8) + depth8;
    p.f = (ao << 24) + (shadow << 16) + (albedoScale << 8) + translucencyScale;
    return p;
}



// AIR
// ###############################################################################################################################################
#include <random>
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


void _lineBuilder::addVerts(std::vector<float3>& _x, std::vector<float>& _w, uint& _idx, uint _parentidx, float _spacing)
{
    isLineEnd = children.size() == 0;
    isCarabiner = name.find("carabiner") != std::string::npos;
    isCarabiner |= name.find("ROOT") != std::string::npos;

    numSegments = 1;// (uint)ceil((float)length / _spacing);
    parent_Idx = _parentidx;
    start_Idx = _idx;
    end_Idx = start_Idx + numSegments - 1;

    for (int i = 0; i < numSegments; i++)
    {
        float t = (float)(i + 1) / (float)numSegments;
 //       _x[start_Idx + i] = glm::lerp(_x[parent_Idx], endPos, t);
 //       _w[start_Idx + i] = 1.f / (_spacing * 1000.f * diameter * diameter);            // EDELRID SPEC
        //_cd[start_Idx + i] = 1.f;
    }

    _idx += numSegments;
    for (auto& Ch : children) {
        Ch.addVerts(_x, _w, _idx, end_Idx, _spacing);
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
    float3 red = float3(0.8f, 0.01f, 0.01f);
    float3 yellow = float3(0.8f, 0.8f, 0.01f);
    float3 green = float3(0.01f, 0.8f, 0.01f);
    float3 blue = float3(0.01f, 0.01f, 0.7f);
    float3 turqoise = float3(0.01f, 0.7f, 0.7f);
    float3 orange = float3(0.7f, 0.3f, 0.01f);
    float3 black = float3(0.01f, 0.01f, 0.01f);

    if (name.find("A") != std::string::npos)    color = red;
    if (name.find("B") != std::string::npos)    color = yellow;
    if (name.find("C") != std::string::npos)    color = turqoise;
    if (name.find("S") != std::string::npos)    color = green;
    if (name.find("F") != std::string::npos)    color = orange;
    if (name.find("strap") != std::string::npos)    color = black;
    if (name.find("carabiner") != std::string::npos)    color = float3(1, 1, 1);
    

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



void _lineBuilder::renderGui(char* search, int tab)
{
    if (((name.find(search) == 0) || (name.find("carabiner") != std::string::npos) || (name.find("strap") != std::string::npos)) && (name.find("L_") == std::string::npos))
    {
        ImGui::NewLine();
        if (stretchRatio > 1.05f)
        {
            ImGui::Button(name.c_str(), ImVec2(200, 30));
            ImGui::SameLine(0, 0);
        }
        ImGui::SameLine(0, (float)(tab * 10));
        ImGui::Text("%s", name.c_str());
        ImGui::SameLine(150, 0);
        ImGui::Text("%1.5f %2d%%, %2d%%     %3dN    %3dN ", stretchRatio, (int)(t_ratio * 99), (int)(t_combined * 99), (int)maxTencileForce, (int)tencileForce);
        //ImGui::Text("%dmm, %1.2fmm %3.1fg   %3.2fg   %3.2fmm", (int)(length * 1000.f), diameter * 1000.f, 1000.f * m_End, 1000.f * m_Line, f_Line);
    }

    for (auto& C : children) {
        C.renderGui(search, tab + 1);
    }
}


void _lineBuilder::addRibbon(rvB_Glider* _r, std::vector<float3>& _x, int& _c)
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
        setupVert(&_r[_c], 0, _x[parent_Idx], diameter* 0.5f + 0.004f, cIndex);     _c++;
        setupVert(&_r[_c], 1, _x[end_Idx], diameter * 0.5f + 0.004f, cIndex);     _c++;
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




void _lineBuilder::wingLoading(std::vector<float3>& _x, std::vector<float3>& _n)
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
    //tencileVector *= 0.5f;
    AllSummedForce += tencileVector;


    float3 lineNormal = glm::normalize(_x[end_Idx] - _x[parent_Idx]);
    tencileRequest = __max(0.0001f, glm::dot(lineNormal, tencileVector));

    

    tencileForce = __min(tencileRequest, maxTencileForce);
    tencileVector = tencileForce * lineNormal;
}

float _lineBuilder::maxT(float _r, float _dT)
{
    float K = __max(0, (_r - 0.98f) * 50.f);

    

    if (diameter < 0.005)   return K * 200.f;
    else                    return K * 400.f;

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

void _lineBuilder::windAndTension(std::vector<float3>& _x, std::vector<float>& _w, std::vector<float3>& _n, std::vector<float3>& _v, float3 _wind, float _dtSquare)
{
    float3 V = _v[end_Idx] - _wind;
    float v = glm::length(V);
    float rho = rho_End * v * v;
    float3 LINEn = _x[end_Idx] - _x[parent_Idx];
    stretchRatio = glm::length(LINEn) / length;
    LINEn = glm::normalize(LINEn);
    LINEn.y = 0;
    float gravityScale = 0;// glm::length(LINEn);
    maxTencileForce = maxT(stretchRatio, rho_Line * v * v * 1.f);
    float3 fWind = -glm::normalize(V) * rho;
    _x[end_Idx] += (fWind / m_End + float3(0, -9.8f * gravityScale, 0)) * _dtSquare;
    AllSummedLinedrag += fWind;

    f_Line = rho_Line * v * v;

    if (isLineEnd)
    {
        wingLoading(_x, _n);
        t_ratio = tencileForce / tencileRequest;
    }

    for (auto& C : children) {
        C.windAndTension(_x, _w, _n, _v, _wind, _dtSquare);
    }

    // now on the way down ---------------------------------------------------------
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
        }
        else
        {
            tencileForce = glm::length(tencileVector);
            t_ratio = 1;
        }
    }
}



// also do tension here for next round
void _lineBuilder::solveUp(std::vector<float3>& _x, bool _lastRound, float _ratio, float _w0)
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
        C.solveUp(_x, _lastRound, t_combined, w1);
    }

    if (_lastRound && isLineEnd)
    {
        float3 Pfinal = glm::lerp(_x[wingIdx], _x[end_Idx], t_combined);
        //float3 Pfinal = _x[end_Idx];
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



_lineBuilder_FILE& _lineBuilder_FILE::pushLine(std::string _name, float _length, float _diameter, int2 _attach, int _segments)
{
    children.emplace_back();
    children.back().name = _name;
    children.back().length = _length;
    children.back().diameter_mm = _diameter;
    children.back().attachment = _attach;
    children.back().segments = _segments;
    return children.back();
}


void _lineBuilder_FILE::generate(std::vector<float3>& _x, int chordSize, _lineBuilder& line)
{
    
    float3 black = float3(0.01f, 0.01f, 0.01f);
    float segLength = length / segments;
    float3 endpos = _x[attachment.x * chordSize + attachment.y];
    
    _lineBuilder& newLine = line.pushLine(name, segLength, diameter_mm * 0.001f, black, attachment, endpos, segments);

    for (auto& c : children)
    {
        c.generate(_x, chordSize, newLine);
    }
}





void wingShape::load(std::string _root)
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"wingShape"} };
    if (openFileDialog(filters, path))
    {
        load(_root, path.string());
    }
}


void wingShape::load(std::string _root, std::string _path)
{
    std::ifstream is(_path);
    cereal::JSONInputArchive archive(is);
    serialize(archive);
}

void wingShape::save()
{
    leadingEdge.set(float3(0, 0, 0), float3(3, 0, 0), float3(8, 0, 0), float3(10, 0, 1.43f));
    trailingEdge.set(float3(0, 0, 0), float3(3, 0, 0), float3(7, 0, 0), float3(10, 0, 0.94f));
    curve.set(float3(0, 7.3, 0), float3(0.9, 7.3, 0), float3(4.2f, 7.3, 0), float3(4.7f, 4.55f, 0));

    float SPAN[25] = { 1, 0.976f, 0.949f, 0.918f, 0.887f, 0.854f, 0.819f, 0.783f, 0.745f, 0.706f, 0.666f, 0.624f, 0.581f, 0.539f, 0.495f, 0.449f, 0.404f, 0.358f, 0.311f, 0.264f, 0.216f, 0.169f, 0.121f, 0.073f, 0.025f };
    chordSpacing.resize(25);
    for (int i = 0; i < 25; i++)
    {
        chordSpacing[i] = SPAN[i];
    }

    uint braced[14] = { 5, 8, 11, 14, 17, 20, 23, 26, 29, 32, 35, 38, 41, 44 };

    linebraceSpan.resize(14);;
    for (int i = 0; i < 14; i++)
    {
        linebraceSpan[i] = braced[i];
    }

    uint bracedC[3] = { 3, 6, 9 };
    linebraceChord.resize(3);
    for (int i = 0; i < 3; i++)
    {
        linebraceChord[i] = bracedC[i];
    }

    std::filesystem::path path;
    FileDialogFilterVec filters = { {"wingShape"} };
    if (saveFileDialog(filters, path))
    {
        std::ofstream os(path.string());
        cereal::JSONOutputArchive archive(os);
        serialize(archive);
    }
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
    //shape.save();
    shape.load(xfoilDir, xfoilDir + "/NOVA_AONIC_medium.wingShape");

    uint numV = spanSize * chordSize;
    x.resize(numV);
    w.resize(numV);
    //cdA.resize(numV);
    cross.resize(numV);
    area.resize(numV);
    cellWidth.resize(spanSize);
    cellChord.resize(spanSize);
    ribNorm.resize(spanSize);

    std::vector<float3> lead, mid, trail;
    lead.resize(spanSize);
    mid.resize(spanSize);
    trail.resize(spanSize);

    shape.curve.solveArcLenght();
    shape.leadingEdge.solveArcLenght();
    shape.trailingEdge.solveArcLenght();
    flatSpan = shape.curve.arcLength * 2;
    spanSize = shape.spanSize;
    chordSize = shape.chordSize;

    uint halfSpan = spanSize >> 1;
    //  build leading and trailing edge
    /// #######################################################################################################
    for (int s = 0; s < spanSize; s++)
    {
        float t;
        if (s < halfSpan)   t = shape.chordSpacing[s];
        else t = shape.chordSpacing[spanSize - 1 - s];

        float3 LEAD = shape.leadingEdge.pos(t);
        float3 TRAIL = shape.trailingEdge.pos(t);
        mid[s] = shape.curve.pos(t);

        if (s >= halfSpan) {
            mid[s].x *= -1;         // mirror x for second half
        }

        // shift moves it rouhly overhead of the pilot
        float shift = shape.maxChord * 0.5f;
        lead[s] = mid[s];
        lead[s].z = LEAD.z - shift;
        trail[s] = mid[s];
        trail[s].z += shape.maxChord - TRAIL.z - shift;

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
            float wingY = wingshape[c].y * chordLength * 0.8;
            // now crimp edges
            if (s == 0 || s == (spanSize - 1))
            {
                wingY = 0;
                if (c < (chordSize / 2 - 1)) wingY = -0.01f;
                if (c >= (chordSize / 2 + 1)) wingY = 0.01f;
            }
            float3 P = glm::lerp(lead[s], trail[s], wingshape[c].x);
            x[index(s, c)] = P + ribNorm[s] * (wingY);     // scaled with chord
            cross[index(s, c)] = crossIdx(s, c);
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
            float m = (shape.wingWeight * area[index(s, c)] / totalArea);
            w[index(s, c)] = 1.f / m;
        }
    }
}


#define L(a,b) glm::length(x[a]-x[b])
void _gliderBuilder::builWingConstraints()
{
    uint4 idx;
    uint a, b;

    // chord
    for (int s = 0; s < spanSize; s++)
    {
        for (int c = 0; c < chordSize - 1; c++)
        {
            idx = quadIdx(s, c);
            float3 push = cnst.chord;
            //if (c < 5) push.z = 0;
            constraints.push_back(_constraint(idx.x, idx.y, s, L(idx.x, idx.y), push));
        }

        // stitch sides
        if (s == 0 || s == (spanSize - 1))
        {
            for (int c = 0; c < chordSize / 2 - 1; c++)
            {
                a = index(s, c);
                b = index(s, chordSize - 1 - c);
                constraints.push_back(_constraint(a, b, s, L(a, b), cnst.chord_verticals));
            }
        }
        else
        {
            // verticals
            a = index(s, 0);
            b = index(s, chordSize - 1);
            constraints.push_back(_constraint(a, b, s, L(a, b), cnst.chord_verticals));  // trail

            a = index(s, 1);
            b = index(s, chordSize - 2);
            constraints.push_back(_constraint(a, b, s, L(a, b), cnst.chord_verticals));  // trail-1

            a = index(s, 11);
            b = index(s, 13);
            constraints.push_back(_constraint(a, b, s, L(a, b), cnst.chord_verticals));  // lead-1
        }

        // cross members
        for (int c = 1; c < chordSize / 2 - 1; c++)
        {
            a = index(s, c);
            b = index(s, chordSize - 2 - c);
            constraints.push_back(_constraint(a, b, s, L(a, b), cnst.chord_diagonals));

            a = index(s, c + 1);
            b = index(s, chordSize - 1 - c);
            constraints.push_back(_constraint(a, b, s, L(a, b), cnst.chord_diagonals));
        }

        //leadingEdge
        a = index(s, 12);
        b = index(s, 15);
        constraints.push_back(_constraint(a, b, s, L(a, b), cnst.leadingEdge));

        //trailingEdge
        a = index(s, 0);
        b = index(s, 5);
        constraints.push_back(_constraint(a, b, s, L(a, b), cnst.trailingEdge));
    }

    // span
    for (int s = 0; s < spanSize - 1; s++)
    {
        uint cell = s;
        if (s < spanSize / 2) cell += 1;    // use the inside cell
        for (int c = 0; c < chordSize - 1; c++)
        {
            idx = quadIdx(s, c);
            constraints.push_back(_constraint(idx.x, idx.w, cell, L(idx.x, idx.w), cnst.span));

            // surface crossBracing
            constraints.push_back(_constraint(idx.x, idx.z, cell, L(idx.x, idx.z), cnst.surface));  // diagonal and zigzag them
            constraints.push_back(_constraint(idx.y, idx.w, cell, L(idx.y, idx.w), cnst.surface));  // diagonal and zigzag them
        }
    }


    // volume pressure push - simulates the bulging effect thats missing in geometry
    for (int s = 0; s < spanSize - 1; s++)
    {
        uint cell = s;
        if (s < spanSize / 2) cell += 1;    // use the inside cell

        //for (int c = 1; c < chordSize / 2 - 1; c++)
        for (int c = 12; c < chordSize - 4; c++)
        {
            idx = quadIdx(s, c);
            uint4 idxTop = quadIdx(s, chordSize - 2 - c);
            
            //cross
            constraints.push_back(_constraint(idx.x, idxTop.z, cell, L(idx.x, idxTop.z), cnst.pressure_volume));  // diagonal and zigzag them
            constraints.push_back(_constraint(idx.y, idxTop.w, cell, L(idx.y, idxTop.w), cnst.pressure_volume));  // diagonal and zigzag them

            constraints.push_back(_constraint(idx.w, idxTop.x, cell, L(idx.w, idxTop.x), cnst.pressure_volume));  // diagonal and zigzag them
            constraints.push_back(_constraint(idx.z, idxTop.y, cell, L(idx.z, idxTop.y), cnst.pressure_volume));  // diagonal and zigzag them
            

            // just span wise
            //constraints.push_back(_constraint(idx.x, idxTop.z, s, L(idx.x, idxTop.z), cnst.pressure_volume));  // diagonal and zigzag them
            //constraints.push_back(_constraint(idx.y, idxTop.w, s, L(idx.y, idxTop.w), cnst.pressure_volume));  // diagonal and zigzag them
        }
    }


    // Line attacment cross bracing
    for (int& s : shape.linebraceSpan)
    {
        for (int& c : shape.linebraceChord)
        {
            uint cTop = chordSize - 1 - c;
            a = index(s, c);
            b = index(s, cTop);
            constraints.push_back(_constraint(a, b, s, L(a, b), cnst.line_bracing));  // striaght up

            for (uint dc = cTop - 1; dc <= cTop + 1; dc++)
            {
                b = index(s - 1, dc);
                constraints.push_back(_constraint(a, b, s, L(a, b), cnst.line_bracing));  // diagonal right
                b = index(s + 1, dc);
                constraints.push_back(_constraint(a, b, s, L(a, b), cnst.line_bracing));  // diagonal left
            }
        }
    }

    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(constraints.begin()+11, constraints.end(), g);
}



void _gliderBuilder::buildLinesFILE_NOVA_AONIC_medium()
{
    _lineBuilder_FILE file;
    file.length = 0.1f;
    file.diameter_mm = 50.f;
    file.name = "carabiner";

    float3 red = float3(0.8f, 0.01f, 0.01f);
    float3 yellow = float3(0.8f, 0.8f, 0.01f);
    float3 green = float3(0.01f, 0.8f, 0.01f);
    float3 blue = float3(0.01f, 0.01f, 0.7f);
    float3 turqoise = float3(0.01f, 0.7f, 0.7f);
    float3 orange = float3(0.7f, 0.3f, 0.01f);
    float3 black = float3(0.01f, 0.01f, 0.01f);

    // This is for Nova size medium
    auto& lA = file.pushLine("A_strap", 0.4f, 5.f); {
        auto& A1 = lA.pushLine("A1", 4.707f, 1.3f, uint2(0, 0), 3); {
            A1.pushLine("AG01", 2.438f, 0.9f, uint2(23, 9), 2);
            A1.pushLine("AG02", 2.405f, 0.9f, uint2(20, 9), 2);
        }
        auto& A2 = lA.pushLine("A2", 4.742f, 1.3f, uint2(0, 0), 3); {
            A2.pushLine("AG03", 2.332f, 0.9f, uint2(17, 9), 2);
            A2.pushLine("AG04", 2.302f, 0.9f, uint2(14, 9), 2);
        }
    }

    auto& lA3 = file.pushLine("A3_strap", 0.4f, 5.f); {
        auto& A3 = lA3.pushLine("A_ears", 3.932f, 1.3f, uint2(0, 0), 2); {
            A3.pushLine("AG05", 3.041f, 0.9f, uint2(11, 9), 2);
            A3.pushLine("AG06", 2.927f, 0.9f, uint2(8, 9), 2);
            A3.pushLine("AG07", 2.801f, 0.9f, uint2(5, 9), 2);
        }
    }

    auto& lB = file.pushLine("B_strap", 0.47f, 5.f); {
        auto& B1 = lB.pushLine("B1", 4.663f, 1.3f, uint2(0, 0), 3); {
            B1.pushLine("BG01", 2.386f, 0.9f, uint2(23, 6), 2);
            B1.pushLine("BG02", 2.356f, 0.9f, uint2(20, 6), 2);
        }
        auto& B2 = lB.pushLine("B2", 4.71f, 1.3f, uint2(0, 0), 3); {
            B2.pushLine("BG03", 2.278f, 0.9f, uint2(17, 6), 2);
            B2.pushLine("BG04", 2.256f, 0.9f, uint2(14, 6), 2);
        }
        auto& B3 = lB.pushLine("B3", 3.879f, 1.3f, uint2(0, 0), 3); {
            B3.pushLine("BG05", 3.026f, 0.9f, uint2(11, 6), 2);
            B3.pushLine("BG06", 2.926f, 0.9f, uint2(8, 6), 2);
            B3.pushLine("BG07", 2.817f, 0.9f, uint2(5, 6), 2);
        }


        auto& S1 = lB.pushLine("S1", 4.914f, 1.3f, uint2(0, 0), 5); {
            S1.pushLine("SAG1", 1.418f, 0.9f, uint2(2, 9), 2);
            S1.pushLine("SBG1", 1.437f, 0.9f, uint2(2, 5), 2);
            auto& SM1 = S1.pushLine("SM1", 0.699f, 0.9f, uint2(23, 4)); {
                SM1.pushLine("SG01", 0.451f, 0.9f, uint2(0, 9));
                SM1.pushLine("SG02", 0.471f, 0.9f, uint2(0, 7));
            }
            auto& SM2 = S1.pushLine("SM2", 0.898f, 0.9f, uint2(20, 4)); {
                SM2.pushLine("SG03", 0.346f, 0.9f, uint2(0, 3));
                SM2.pushLine("SG04", 0.431f, 0.9f, uint2(0, 1));
            }
        }
    }

    auto& lC = file.pushLine("C_strap", 0.54f, 5.f); {
        auto& C1 = lC.pushLine("C1", 4.663f, 1.3f, uint2(0, 0), 2); {
            C1.pushLine("CG01", 2.386f, 0.9f, uint2(23, 3), 2);
            C1.pushLine("CG02", 2.356f, 0.9f, uint2(20, 3), 2);
        }
        auto& C2 = lC.pushLine("C2", 4.71f, 1.3f, uint2(0, 0), 2); {
            C2.pushLine("CG03", 2.278f, 0.9f, uint2(17, 3), 2);
            C2.pushLine("CG04", 2.256f, 0.9f, uint2(14, 3), 2);
        }
        auto& C3 = lC.pushLine("C3", 3.879f, 1.3f, uint2(0, 0), 2); {
            C3.pushLine("CG05", 3.026f, 0.9f, uint2(11, 3), 3);
            C3.pushLine("CG06", 2.926f, 0.9f, uint2(8, 3), 3);
            C3.pushLine("CG07", 2.817f, 0.9f, uint2(5, 3), 3);
        }
    }


    auto& strapF = lC.pushLine("F_strap", 0.08f, 5.f);

    auto& FF2 = strapF.pushLine("FF1+2", 1.2f + 0.464f, 2.f, uint2(0, 0), 2); {
        //auto& FF2 = lFF1.pushLine("FF2", , 0.002f);
        {
            auto& F1 = FF2.pushLine("F1", 2.451f, 2.f, uint2(0, 0), 3); {
                auto& FM1 = F1.pushLine("FM1", 2.468f, 1.3f, uint2(0, 0), 3); {
                    FM1.pushLine("FG01", 1.474f, 0.9f, uint2(22, 0));
                    FM1.pushLine("FG02", 1.138f, 0.9f, uint2(20, 0));
                }
                auto& FM2 = F1.pushLine("FM2", 2.309f, 1.3f, uint2(0, 0)); {
                    FM2.pushLine("FG03", 1.087f, 0.9f, uint2(16, 0));
                    FM2.pushLine("FG04", 0.923f, 0.9f, uint2(14, 0));
                }
            }
            auto& F2 = FF2.pushLine("F2", 2.984f, 2.f, uint2(0, 0), 3); {
                auto& FM3 = F2.pushLine("FM3", 1.674f, 1.3f, uint2(0, 0), 2); {
                    FM3.pushLine("FG05", 0.927f, 0.9f, uint2(11, 0));
                    FM3.pushLine("FG06", 0.723f, 0.9f, uint2(8, 0));
                }
                auto& FM4 = F2.pushLine("FM4", 1.389f, 1.3f, uint2(0, 0), 2); {
                    FM4.pushLine("FG07", 0.821f, 0.9f, uint2(5, 0));
                    FM4.pushLine("FG08", 0.519f, 0.9f, uint2(3, 0));
                }
            }
        }
    }




    std::filesystem::path path;
    FileDialogFilterVec filters = { {"lines"} };
    if (saveFileDialog(filters, path))
    {
        std::ofstream os(path.string());
        cereal::JSONOutputArchive archive(os);
        file.serialize(archive);
    }

}


void _gliderBuilder::buildLines()
{
    _lineBuilder_FILE lines;
    std::ifstream is(xfoilDir + "/NOVA_IONIC_medium.lines");
    cereal::JSONInputArchive archive(is);
    lines.serialize(archive);

    _lineBuilder genLines;
    lines.generate(x, chordSize, genLines);
    linesRight = genLines.children.back();

    linesLeft = linesRight;
    linesLeft.mirror(spanSize);

    linesLeft.solveLayout(float3(-0.1f, 0, 0), chordSize);        // this does a basic layout on lenths
    linesRight.solveLayout(float3(0.1f, 0, 0), chordSize);
    return;
}


void _gliderBuilder::generateLines()
{
    float spacing = 0.2f;

    // calc numVerts;
    uint startVert = x.size();
    uint numVerts = 15;   //The two caribiners
    numVerts += linesLeft.calcVerts(spacing) * 2;

    uint totalVerts = startVert + numVerts;
    x.resize(totalVerts);
    w.resize(totalVerts);
    //cdA.resize(totalVerts);

    // two caribiners
    /*
    // FIXME in future start from CG
    x[startVert + 0] = float3(0.0f, -0.2f, 0);
    w[startVert + 0] = 1.f / 90.f;
    //cdA[startVert + 0] = 0.7f * 0.4f;      // Cd * A

    x[startVert + 1] = float3(-0.25f, 0, 0);
    x[startVert + 2] = float3(0.25f, 0, 0);
    w[startVert + 1] = 1.f / 3.f;
    w[startVert + 2] = 1.f / 3.f;
    // cdA[startVert + 1] = 0;
     //cdA[startVert + 2] = 0;

    constraints.push_back(_constraint(startVert + 1, startVert + 2, 0, glm::length(x[startVert + 1] - x[startVert + 2]), 1.f, 1.f, 0.f));
    constraints.push_back(_constraint(startVert + 0, startVert + 1, 0, glm::length(x[startVert + 1] - x[startVert]), 1.f, 1.f, 0.f));
    constraints.push_back(_constraint(startVert + 0, startVert + 2, 0, glm::length(x[startVert + 2] - x[startVert]), 1.f, 1.f, 0.f));
    */

    // now for intermediate 4 weight view of world
    float pilotW = 1.f / (90.f / 4.f);    // 90 kg
    x[startVert + 0] = float3(-0.15f, -0.3f, 0.15); // bacl left 
    w[startVert + 0] = pilotW;

    x[startVert + 1] = float3(0.15f, -0.3f, 0.15);  // back right
    w[startVert + 1] = pilotW;

    x[startVert + 2] = float3(0.20f, -0.3f, -0.15);  // front right
    w[startVert + 2] = pilotW;

    x[startVert + 3] = float3(-0.20f, -0.3f, -0.15); // front left
    w[startVert + 3] = pilotW;


    x[startVert + 4] = float3(-0.29f, 0, 0);    // 50 cm pacing on carabiners
    w[startVert + 4] = 1.f / 3.f;

    x[startVert + 5] = float3(0.29f, 0, 0);
    w[startVert + 5] = 1.f / 3.f;

    // 6 bottom contraints
    constraints.push_back(_constraint(startVert + 0, startVert + 1, 0, glm::length(x[startVert + 0] - x[startVert + 1]), 1.f, 1.f, 0.f));
    constraints.push_back(_constraint(startVert + 1, startVert + 2, 0, glm::length(x[startVert + 1] - x[startVert + 2]), 1.f, 1.f, 0.f));
    constraints.push_back(_constraint(startVert + 2, startVert + 3, 0, glm::length(x[startVert + 2] - x[startVert + 3]), 1.f, 1.f, 0.f));
    constraints.push_back(_constraint(startVert + 3, startVert + 0, 0, glm::length(x[startVert + 3] - x[startVert + 0]), 1.f, 1.f, 0.f));

    constraints.push_back(_constraint(startVert + 0, startVert + 2, 0, glm::length(x[startVert + 0] - x[startVert + 2]), 1.f, 1.f, 0.f));   // diagonales
    constraints.push_back(_constraint(startVert + 1, startVert + 3, 0, glm::length(x[startVert + 1] - x[startVert + 3]), 1.f, 1.f, 0.f));

    // 4 verticals bottom contraints
    constraints.push_back(_constraint(startVert + 0, startVert + 4, 0, glm::length(x[startVert + 0] - x[startVert + 4]), 1.f, 1.f, 0.f));
    constraints.push_back(_constraint(startVert + 1, startVert + 5, 0, glm::length(x[startVert + 1] - x[startVert + 5]), 1.f, 1.f, 0.f));
    constraints.push_back(_constraint(startVert + 2, startVert + 5, 0, glm::length(x[startVert + 2] - x[startVert + 5]), 1.f, 1.f, 0.f));
    constraints.push_back(_constraint(startVert + 3, startVert + 4, 0, glm::length(x[startVert + 3] - x[startVert + 4]), 1.f, 1.f, 0.f));


    // 1 chest
    constraints.push_back(_constraint(startVert + 4, startVert + 5, 0, glm::length(x[startVert + 4] - x[startVert + 5]), 1.f, 0.01f, 0.f));

    // 4 cross braces
    constraints.push_back(_constraint(startVert + 0, startVert + 5, 0, glm::length(x[startVert + 0] - x[startVert + 5]), 0.001f, 0.001f, 0.f));
    constraints.push_back(_constraint(startVert + 1, startVert + 4, 0, glm::length(x[startVert + 1] - x[startVert + 4]), 0.001f, 0.001f, 0.f));
    constraints.push_back(_constraint(startVert + 2, startVert + 4, 0, glm::length(x[startVert + 2] - x[startVert + 4]), 0.001f, 0.001f, 0.f));
    constraints.push_back(_constraint(startVert + 3, startVert + 5, 0, glm::length(x[startVert + 3] - x[startVert + 5]), 0.001f, 0.001f, 0.f));


    uint idx = startVert + 15;
    linesLeft.addVerts(x, w, idx, startVert + 4, spacing);
    linesRight.addVerts(x, w, idx, startVert + 5, spacing);
}




void _gliderBuilder::visualsPack(rvB_Glider* ribbon, rvPackedGlider* packed, int& count, bool& changed)
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
        sprintf(name, "%s/naca4415_A_%d.txt", xfoilDir.c_str(), i);
        analizeCp(name, i);
    }

    std::ofstream outfile(xfoilDir + "/wing_10.txt");    // 9 degrees
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

    std::ofstream out2file(xfoilDir + "/wing_30.txt");    // 9 degrees
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

    std::ofstream outcpfile(xfoilDir + "/wing_CP.txt");    // 9 degrees
    for (int j = 0; j < 11; j++)
    {
        sprintf(name, "%s/naca4415_A_%d_FLAP10.txt", xfoilDir.c_str(), j);
        std::ifstream flap10(name);
        std::getline(flap10, line);
        std::getline(flap10, line);
        std::getline(flap10, line);

        sprintf(name, "%s/naca4415_A_%d_FLAP20.txt", xfoilDir.c_str(), j);
        std::ifstream flap20(name);
        std::getline(flap20, line);
        std::getline(flap20, line);
        std::getline(flap20, line);

        sprintf(name, "%s/naca4415_A_%d_FLAP30.txt", xfoilDir.c_str(), j);
        std::ifstream flap30(name);
        std::getline(flap30, line);
        std::getline(flap30, line);
        std::getline(flap30, line);

        sprintf(name, "%s/naca4415_A_%d_FLAP40.txt", xfoilDir.c_str(), j);
        std::ifstream flap40(name);
        std::getline(flap40, line);
        std::getline(flap40, line);
        std::getline(flap40, line);

        sprintf(name, "%s/naca4415_A_%d_FLAP50.txt", xfoilDir.c_str(), j);
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



    std::ofstream outTfile(xfoilDir + "/T.txt");    // 9 degrees
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





void _gliderBuilder::xfoil_shape(std::string _name)
{
    wingName = _name;
    std::string line;
    std::string folder = xfoilDir + "/" + wingName;
    wingshapeFull.clear();

    std::ifstream infile(folder + "/" + wingName + ".txt");     // read teh wing shape
    if (infile.good())
    {
        std::getline(infile, line);
        float x, y;
        while (infile >> x >> y)
        {
            wingshapeFull.push_back(float2(x, y));
        }
        infile.close();
    }


    for (int i = 0; i < 6; i++)
    {
        std::string flapName = folder + "/" + wingName + "_flaps_" + std::to_string(i) + ".txt";
        float flapDst = (float(i) * 0.2f * 0.2f);   // 20% of chord at max
        xfoil_createFlaps(flapDst, flapName);

        std::ofstream xfoilcmd(xfoilDir + "/" + wingName + "_solveFlaps_" + std::to_string(i) + ".txt");
        if (xfoilcmd.good())
        {
            xfoilcmd << "LOAD " << flapName << "\n";
            xfoilcmd << "OPER\n";
            xfoilcmd << "Visc 1000000\n";
            xfoilcmd << "Iter 5000\n";

            for (int j = 0; j <= 36; j += 1)
            {
                int aoa = j - 6;
                xfoilcmd << "Alfa" << aoa << "\n";
                xfoilcmd << "CPWR " << folder << "/cp/cp_" << i << "_" << j << ".txt\n";
            }
            xfoilcmd << "\n";

            xfoilcmd.close();
        }
    }

    xfoil_simplifyWing();

    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j <= 36; j += 1)
        {
            std::string flapName = folder + "/cp/cp_" + std::to_string(i) + "_" + std::to_string(j) + ".txt";
            xfoil_buildCP(i, j, flapName);
        }
    }

    // now save the shape and CP data, while flipping around            //??? CEREAL
    std::string name = folder + "/wingshape.txt";
    std::ofstream shape(name);
    if (shape.good())
    {
        for (int i = 0; i < 25; i++)
        {
            shape << wingshape[24 - i].x << " " << wingshape[24 - i].y << "\n";
        }
        shape.close();
    }

    name = folder + "/cp.txt";
    std::ofstream CP(name);
    if (CP.good())
    {

        for (int i = 0; i < 6; i++)
        {
            for (int j = 0; j <= 36; j += 1)
            {
                for (int k = 0; k < 25; k++)
                {
                    CP << cp_temp[i][j][k] << " ";  // already flipped
                }
                CP << "\n";
            }
            CP << "\n\n";
        }

        CP.close();
    }

    //std::string folder = xfoilDir + "/" + wingName;
    name = folder + "/cp.bin";
    std::ofstream CPbin(name, std::ios::binary);
    if (CPbin.good())
    {
        CPbin.write(reinterpret_cast<char*>(&cp_temp), sizeof(float) * 6 * 36 * 25);
        CPbin.close();
    }


}



void _gliderBuilder::xfoil_createFlaps(float _dst, std::string _name)
{
    std::ofstream flap(_name);
    if (flap.good())
    {
        flap << _name.c_str() << "\n";
        for (auto p : wingshapeFull)
        {
            float pOut = p.y;
            // air intake
            if (p.y < 0 && p.x>0.02f && p.x < 0.06f)
            {
                float d = 1.f - fabs((p.x - 0.04f) * 50.f);
                //pOut += 0.01f * d;
                //pOut += 0.02f;
            }

            // general inflate
            float backScale = glm::clamp(1.f - (p.x - 0.9f) * 10.f, 0.f, 1.f);
            //if (p.x < 0.98)
            {
                if (p.y > 0)    pOut += 0.0051f * backScale;
                else            pOut -= 0.0051f * backScale;
            }

            float r = 0;
            float zero = 1.f - (2.f * _dst);
            if (_dst > 0)
            {
                r = (p.x - zero) / (2.f * _dst);
            }
            r = pow(__max(0, r), 2.f);    // 0-1 at trailng edge
            float y = pOut - r * _dst;
            flap << p.x << "   " << y << "\n";
        }

        flap.close();
    }
}


void _gliderBuilder::xfoil_simplifyWing()
{
    float lineChord[8] = { 1.f, 0.73f, 0.37f, 0.09f, 0.09f, 0.37f, 0.73f, 1.f };  //FIXME from file
    uint mainline[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    mainline[7] = wingshapeFull.size() - 1;
    uint idx = 1;
    uint wIdx = 0;
    for (auto& w : wingshapeFull)
    {
        if (idx < 4)
        {
            if (w.x < lineChord[idx])
            {
                mainline[idx] = wIdx;
                idx++;
            }
        }
        else
        {
            if (w.x > lineChord[idx])
            {
                mainline[idx] = wIdx;
                idx++;
            }
        }
        wIdx++;
    }

    wingshape.resize(25);
    uint start = 0;
    for (int j = 0; j < 7; j++)
    {
        uint steps = 3;
        if (j == 3) steps = 6;
        uint a = mainline[j];
        uint diff = (mainline[j + 1] - mainline[j]) / steps;
        for (int k = 0; k < steps; k++) {
            wingIdx[start] = a + diff * k;
            wingshape[start] = wingshapeFull[a + diff * k];
            start++;
        }
    }
    wingIdx[24] = mainline[7];
    wingshape[24] = wingshapeFull[mainline[7]];

    for (int i = 0; i < 24; i++)
    {
        wingIdxB[i] = (wingIdx[i] + wingIdx[i + 1]) / 2;
    }
    wingIdxB[24] = wingIdx[24];
}



void _gliderBuilder::xfoil_buildCP(int _flaps, int _aoa, std::string _name)
{
    std::string line;
    uint idx = 0;
    uint idxW = 0;
    float count = 0;
    float cp = 0;

    std::ifstream infile(_name);     // read the cp
    if (infile.good())
    {
        std::getline(infile, line);
        std::getline(infile, line);
        std::getline(infile, line);
        float x, y, cp_in;
        float cp = 0.f;
        while (infile >> x >> y >> cp_in)
        {
            cp += cp_in;
            if (idx >= wingIdxB[idxW])
            {
                cp_temp[_flaps][_aoa][24 - idxW] = cp / count;
                cp = 0.f;
                idxW++;
                count = 0.f;
            }

            idx++;
            count += 1.f;
        }
        infile.close();
    }
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



