#include "glider.h"
#include "imgui.h"
#include <imgui_internal.h>

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



void _lineBuilder::renderGui(char* search, int tab)
{
    if (((name.find(search) == 0) || (name.find("Carabiner") != std::string::npos) || (name.find("strap") != std::string::npos)) && (name.find("L_") == std::string::npos))
    {
        ImGui::NewLine();
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
    AllSummedForce += tencileVector;


    float3 lineNormal = glm::normalize(_x[end_Idx] - _x[parent_Idx]);
    tencileRequest = __max(0.0001f, glm::dot(lineNormal, tencileVector));

    tencileForce = __min(tencileRequest, maxTencileForce);
    tencileVector = tencileForce * lineNormal;
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
    maxTencileForce = maxT(stretchRatio, rho_Line * v * v * 4.f);
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
                if (c == 11 || c == 12)chordPush = 0.f;
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


        auto& S1 = lB.pushLine("S1", 4.914f / 5.f, 0.0013f, green, uint2(0, 0), float3(0, 0, 0), 5);
        {
            S1.pushLine("SAG1", 1.418f / 2.f, 0.0009f, green, uint2(2, 9), x[index(2, 9)], 2);
            S1.pushLine("SBG1", 1.437f / 2.f, 0.0009f, green, uint2(2, 5), x[index(2, 5)], 2);
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
            auto& F1 = FF2.pushLine("F1", (2.451f) / 3.f, 0.002f, red, uint2(0, 0), float3(0, 0, 0), 3);
            {
                auto& FM1 = F1.pushLine("FM1", 2.468f / 3.f, 0.0012f, red, uint2(0, 0), float3(0, 0, 0), 3);
                {
                    FM1.pushLine("FG01", 1.474f, 0.0009f, red, uint2(22, 0), x[index(22, 0)]);
                    FM1.pushLine("FG02", 1.138f, 0.0009f, red, uint2(20, 0), x[index(20, 0)]);
                }
                auto& FM2 = F1.pushLine("FM2", 2.309f / 3.f, 0.0012f, red, uint2(0, 0), float3(0, 0, 0), 3);
                {
                    FM2.pushLine("FG03", 1.087f, 0.0009f, red, uint2(16, 0), x[index(16, 0)]);
                    FM2.pushLine("FG04", 0.923f, 0.0009f, red, uint2(14, 0), x[index(14, 0)]);
                }
            }
            auto& F2 = FF2.pushLine("F2", 2.984f / 3.f, 0.002f, red, uint2(0, 0), float3(0, 0, 0), 3);
            {
                auto& FM3 = F2.pushLine("FM3", 1.674f / 2.f, 0.0012f, red, uint2(0, 0), float3(0, 0, 0), 2);
                {
                    FM3.pushLine("FG05", 0.927f, 0.0009f, red, uint2(11, 0), x[index(11, 0)]);
                    FM3.pushLine("FG06", 0.723f, 0.0009f, red, uint2(8, 0), x[index(8, 0)]);
                }
                auto& FM4 = F2.pushLine("FM4", 1.389f / 2.f, 0.0012f, red, uint2(0, 0), float3(0, 0, 0), 2);
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
        ImGui::Text("%2d : %4.1f, %4.1f, %4d%%, %4.1f, %2.1f ", i, cells[i].cellVelocity, cells[i].brakeSetting, (int)(cells[i].pressure * 99.f), cells[i].ragdoll, cells[i].AoA * 57 );
    }

    ImGui::NextColumn();

    static char SEARCH[256];
    ImGui::InputText("search", SEARCH, 256);
    linesLeft.renderGui(SEARCH);
    linesRight.renderGui(SEARCH);

}


// it gets parabuilder data
void _gliderRuntime::setup(std::vector<float3>& _x, std::vector<float>& _w, std::vector<float>& _cd, std::vector<uint4>& _cross, uint _span, uint _chord, std::vector<_constraint>& _constraints)
{
    x = _x;
    w = _w;
    cd = _cd;
    cross = _cross;

    v.resize(x.size());
    memset(&v[0], 0, sizeof(float3) * v.size());
    n.resize(_span * _chord);
    t.resize(_span * _chord);

    x_old.resize(x.size());
    l_old.resize(x.size());

    constraints = _constraints;

    spanSize = _span;
    chordSize = _chord;

    cells.resize(_span);



    //ROOT = float3(5500, 1900, 11500);   // walensee
    ROOT = float3(-7500, 1000, -10400);// hulftegg

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
}



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

    {
        FALCOR_PROFILE("wing");
        for (int i = 0; i < spanSize; i++)
        {
            cells[i].old_pressure = cells[i].pressure;
        }

        for (int i = 0; i < spanSize; i++)
        {
            cells[i].front = glm::normalize(x[index(i, 12)] - x[index(i, 3)]);  // From C forward

            // up vector ####################################################################
            cells[i].up = float3(0, 0, 0);
            for (int j = 0; j < 10; j++)
            {
                cells[i].up -= n[index(i, j)];
                cells[i].up += n[index(i, 24 - j)];
            }
            float3 r = glm::cross(cells[i].up, cells[i].front);
            cells[i].up = glm::normalize(glm::cross(cells[i].front, r));

            float3 frontBrake = glm::normalize(x[index(i, 3)] - x[index(i, 0)]);
            float3 upBrake = glm::normalize(glm::cross(frontBrake, r));

            cells[i].AoBrake = -glm::dot(upBrake, cells[i].front) * 0.8f;
            cells[i].AoBrake = __min(1.f, cells[i].AoBrake);
            cells[i].AoBrake = __max(0.f, cells[i].AoBrake);

            cells[i].v = float3(0, 0, 0);
            for (int j = 0; j < chordSize; j++)
            {
                cells[i].v += v[index(i, j)];
            }
            cells[i].v /= chordSize;

            cells[i].rel_wind = wind - cells[i].v;
            cells[i].cellVelocity = glm::length(cells[i].rel_wind);
            cells[i].rho = 0.5 * 1.11225f * cells[i].cellVelocity * cells[i].cellVelocity;       // dynamic pressure

            float3 windDir = glm::normalize(cells[i].rel_wind);
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
                cells[i].AoA += (float)(i - (spanSize - 11)) * 0.005f;
            }
            
            cells[i].reynolds = (1.125f * cells[i].cellVelocity * cells[i].chordLength) / (0.0000186);     // dynamic viscocity 25C air
            cells[i].cD = 0.027f / pow(cells[i].reynolds, 1.f / 7.f);

            // cell pressure
            float AoAlookup = ((cells[i].AoA * 57.f) + 6.f) / 3.f;
            int idx = __max(0, __min(9, (int)floor(AoAlookup)));
            float dL = __min(1, AoAlookup - idx);
            if (idx == 0) dL = __max(0, dL);
            float pressure10 = __max(0, glm::lerp(CPbrakes[0][idx][10], CPbrakes[0][idx + 1][10], dL));
            float pressure11 = __max(0, glm::lerp(CPbrakes[0][idx][11], CPbrakes[0][idx + 1][11], dL));
            float P = __max(pressure10, pressure11);
            float windPscale = __min(1.f, cells[i].cellVelocity / 4.f);
            P *= pow(windPscale, 4);

            cells[i].old_pressure = glm::lerp(cells[i].old_pressure, P, 0.01f);
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
            float CP_B0 = glm::lerp(CPbrakes[idxBrake][idx][chord], CPbrakes[idxBrake + 1][idx][chord], dB);
            float CP_B1 = glm::lerp(CPbrakes[idxBrake][idx + 1][chord], CPbrakes[idxBrake + 1][idx + 1][chord], dB);
            cp = glm::lerp(CP_B0, CP_B1, dL);

            // rag doll pressures
            cells[span].ragdoll = 0;
            if (idx > 8) cells[span].ragdoll = __min(0.6f, __max(0, AoAlookup - 8));
            if (cells[span].AoA <= -0.1) cells[span].ragdoll = __min(1, __max(0, (0.1f - cells[span].AoA) * 40.f));
            float cpRagdoll = -glm::dot(glm::normalize(n[i]), glm::normalize(cells[span].rel_wind));
            cp = glm::lerp(cp, __max(0, cpRagdoll), cells[span].ragdoll);


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


