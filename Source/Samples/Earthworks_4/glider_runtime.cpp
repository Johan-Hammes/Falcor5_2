#include "glider.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <random>


#include"terrafector.h" // for logging
using namespace std::chrono;
//#pragma optimize("", off)

#define GLM_FORCE_SSE2
#define GLM_FORCE_ALIGNED

extern void setupVert(rvB_Glider* r, int start, float3 pos, float radius, int _mat = 0);
float3 AllSummedForce;
float3 AllSummedLinedrag;



#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}

void _cp::load(std::string _name)
{
    std::ifstream CPbin(_name, std::ios::binary);
    if (CPbin.good())
    {
        CPbin.read(reinterpret_cast<char*>(&cp), sizeof(float) * 6 * 36 * 25);
        CPbin.close();
    }
    else
    {
        fprintf(terrafectorSystem::_logfile, "_gliderRuntime Pressure - failed to load %s\n", _name.c_str());
        fflush(terrafectorSystem::_logfile);
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






void _rib::calcIndices()
{
    uint size = ribVerts.size();

    R_index.x = 999;
    R_index.y = ribVerts.size();

    int cnt = 0;
    int v_cnt = 0;
    float3 A, B, C, D;
    for (auto& rv : ribVerts)
    {
        float3 V2 = rv - trailEdge;
        float DOT = glm::dot(V2, front) / chord;    //%
        if (cnt == 0 && DOT > 0.4f) {
            cnt++;
            R_index.x = v_cnt;  // do local first add v_start +  later
            A = rv;
        }
        if (cnt == 1 && DOT > 0.75f) {
            cnt++;
            R_index.z = v_cnt;
            C = rv;
        }
        if (cnt == 2 && DOT < 0.75f) {
            cnt++;
            R_index.w = v_cnt - 1;
            D = rv;
        }
        if (cnt == 3 && DOT < 0.4f) {
            cnt++;
            R_index.y = v_cnt - 1;
            B = rv;
        }

        v_cnt++;
    }

    R_index.y = size - 1 - R_index.x;
    R_index.w = size - 1 - R_index.z;
    B = ribVerts[R_index.y];
    D = ribVerts[R_index.w];

    float da = glm::dot(up, A - trailEdge);
    float db = glm::dot(up, B - trailEdge);
    float dc = glm::dot(up, C - trailEdge);
    float dd = glm::dot(up, D - trailEdge);

    aLerp = -da / (db - da);
    bLerp = -dc / (dd - dc);

    float3 newA = glm::lerp(ribVerts[R_index.x], ribVerts[R_index.y], aLerp);
    float3 newB = glm::lerp(ribVerts[R_index.z], ribVerts[R_index.w], bLerp);

    float dca = glm::dot(front, newA - trailEdge) / chord;
    float dcb = glm::dot(front, newB - trailEdge) / chord;
    chordLerp = (0.33f - dca) / (dcb - dca);

    R_index.x += v_start;
    R_index.y += v_start;
    R_index.z += v_start;
    R_index.w += v_start;

}



void _rib::verticalMirror()
{
    int N = ribIndex.size() / 2;
    int R = ribIndex.size() - 1;
    int MAX = lower.size() - 1;

    for (int i = 0; i < N; i++)
    {
        ribIndex[R - i] = MAX - ribIndex[i];
    }
}


void _rib::interpolate(_rib& _a, _rib& _b, float _billow, bool _finalInterp)
{
    leadEdge = glm::lerp(_a.leadEdge, _b.leadEdge, 0.5f);
    trailEdge = glm::lerp(_a.trailEdge, _b.trailEdge, 0.5f);
    chord = glm::length(leadEdge - trailEdge);

    leadindex = _a.leadindex;

    float width = glm::length(_a.trailEdge - _b.trailEdge);

    front = glm::lerp(_a.front, _b.front, 0.5f);
    up = glm::lerp(_a.up, _b.up, 0.5f);

    lower.resize(_a.lower.size());
    for (int i = 0; i < _a.lower.size(); i++)
    {
        lower[i] = glm::lerp(_a.lower[i], _b.lower[i], 0.5f);
        //but billow it

        float scale = 1.f;
        float trail = (trailEdge.x - lower[i].x) / (chord * 0.1f);        // really mneeds new chord inm middle, will suck at the edges
        float lead = (lower[i].x - leadEdge.x) / (chord * 0.1f);
        if (trail < 1) scale *= pow(trail, 0.5);
        if (lead < 1) scale *= pow(lead, 0.5);
        float sign = 1;
        if (glm::dot(lower[i] - trailEdge, up) < 0) sign = -1;
        lower[i] += sign * _billow * width * scale * up; // Shouldd really be out rather than up
    }

    // new ribverts
    ribVerts.clear();
    // step 1 just mirror - _a


    if (_finalInterp)
    {
        ribIndex.clear();       // should really become int again, just fuxking soimplify
        ribIndex.push_back(0);

        if (_a.ribIndex.size() > _b.ribIndex.size())        // choose the smalelr one
        {
            for (int i = 0; i < _b.ribIndex.size() - 1; i++)
            {
                float idx = (_b.ribIndex[i] + _b.ribIndex[i + 1]) / 2;
                if (idx < (lower.size() / 2)) idx += 1;
                if (i == 0) idx = _b.ribIndex[i + 1] - 2;
                if (i == _b.ribIndex.size() - 2) idx = _b.ribIndex[i] + 3;
                ribIndex.push_back(idx);
            }
        }
        else
        {
            for (int i = 0; i < _a.ribIndex.size() - 1; i++)
            {
                float idx = (_a.ribIndex[i] + _a.ribIndex[i + 1]) / 2;
                if (idx < (lower.size() / 2)) idx += 1;
                if (i == 0) idx = _a.ribIndex[i + 1] - 2;
                if (i == _a.ribIndex.size() - 2) idx = _a.ribIndex[i] + 3;
                ribIndex.push_back(idx);
            }
        }

        ribIndex.push_back((float)(lower.size() - 1));   // trailing edge top
        //verticalMirror();
    }
    else
    {
        if (_a.ribIndex.size() > _b.ribIndex.size())        // choose the biggest one
        {
            ribIndex = _a.ribIndex;
        }
        else
        {
            ribIndex = _b.ribIndex;
        }
    }

    for (auto& idx : ribIndex)
    {
        int vIdx = (int)floor(idx);
        ribVerts.push_back(lower[vIdx]);
        //ribVerts_uv.push_back(float2(0.f, v_lookup[vIdx]));
    }
}



void _rib::mirror_Y()
{
    for (auto& v : outerV)    v.y *= -1;
    for (auto& v : lower)     v.y *= -1;
    for (auto& v : ribVerts)  v.y *= -1;
    leadEdge.y *= -1;
    trailEdge.y *= -1;
    front.y *= -1;
    up.y *= -1;
}


void _rib::calc_V()
{
    circumference = 0;
    v_circ.resize(lower.size());
    v_lookup.resize(lower.size());
    v_circ[0] = 0;
    float circLead;

    for (int i = 0; i < lower.size() - 1; i++)
    {
        circumference += glm::length(lower[i] - lower[i + 1]);
        v_circ[i + 1] = circumference;
        if ((i + 1) == leadindex) {
            circLead = circumference;
        }
    }

    // bottom V
    for (int i = 0; i < leadindex; i++)
    {
        v_lookup[i] = 0.5f * v_circ[i] / circLead;
    }

    // top V
    for (int i = leadindex; i < lower.size(); i++)
    {
        v_lookup[i] = 0.5f + (0.5f * (v_circ[i] - circLead) / (circumference - circLead));
    }
}








// At this stage it inverts, i.e. start at rigth edge move inwards instead
void _gliderImporter::halfRib_to_Full()
{
    full_ribs.clear();


    for (int i = 0; i < half_ribs.size(); i++)
    {
        full_ribs.push_back(half_ribs[i]);
    }

    for (int i = half_ribs.size() - 1; i >= 0; i--)
    {
        _rib r = half_ribs[i];
        r.mirror_Y();
        full_ribs.push_back(r);
    }
}



void _gliderImporter::interpolateRibs()
{
    // Only supports 2 and 4 at the moment, so clean up
    if (numRibScale < 4)
    {
        for (int i = 0; i < full_ribs.size() - 1; i += 2)     // jump over the newly inserted rib as well, so +2
        {
            _rib newR;
            newR.interpolate(full_ribs[i], full_ribs[i + 1], 0.1f, true);
            full_ribs.insert(full_ribs.begin() + i + 1, newR);
        }
    }
    else if (numRibScale < 8)
    {
        for (int i = 0; i < full_ribs.size() - 1; i += 2)     // jump over the newly inserted rib as well, so +2
        {
            _rib newR;
            newR.interpolate(full_ribs[i], full_ribs[i + 1], 0.1f, false);
            full_ribs.insert(full_ribs.begin() + i + 1, newR);
        }

        for (int i = 0; i < full_ribs.size() - 1; i += 2) // jump over teh noew one as well
        {
            _rib newR;
            newR.interpolate(full_ribs[i], full_ribs[i + 1], 0.04f, true);
            full_ribs.insert(full_ribs.begin() + i + 1, newR);
        }
    }
    else
    {
        for (int i = 0; i < full_ribs.size() - 1; i += 2)     // jump over the newly inserted rib as well, so +2
        {
            _rib newR;
            newR.interpolate(full_ribs[i], full_ribs[i + 1], 0.1f, false);
            full_ribs.insert(full_ribs.begin() + i + 1, newR);
        }

        for (int i = 0; i < full_ribs.size() - 1; i += 2)     // jump over the newly inserted rib as well, so +2
        {
            _rib newR;
            newR.interpolate(full_ribs[i], full_ribs[i + 1], 0.04f, false);
            full_ribs.insert(full_ribs.begin() + i + 1, newR);
        }

        for (int i = 0; i < full_ribs.size() - 1; i += 2) // jump over teh noew one as well
        {
            _rib newR;
            newR.interpolate(full_ribs[i], full_ribs[i + 1], 0.04f, true);
            full_ribs.insert(full_ribs.begin() + i + 1, newR);
        }
    }



}


/*
    Expand on edge, either store stiffness in z of an int3, or use z to store the type of edge that this is - I think I like that more
*/
bool _gliderImporter::insertEdge(float3 _a, float3 _b, bool _mirror, constraintType _type, float _mass, float _area, float _search)
{
    _CNST C;
    C.type = _type;
    C.mass = _mass;
    C.area = _area;

    bool found = false;
    int va = 0;
    int vb = 0;
    for (int j = 0; j < verts.size(); j++)
    {
        if (glm::length(verts[j]._x - _a) < _search) va = j;
        if (glm::length(verts[j]._x - _b) < _search) vb = j;

        if (va > 0 && vb > 0)
        {
            C.v_0 = va;
            C.v_1 = vb;
            constraints.push_back(C);
            //constraints.push_back(glm::ivec3(va, vb, (int)_type));
            found = true;
            break;
        }
    }


    if (_mirror)
    {
        _a.y *= -1;
        _b.y *= -1;
        va = 0;
        vb = 0;
        for (int j = 0; j < verts.size(); j++)
        {
            if (glm::length(verts[j]._x - _a) < _search) va = j;
            if (glm::length(verts[j]._x - _b) < _search) vb = j;

            if (va > 0 && vb > 0)
            {
                C.v_0 = va;
                C.v_1 = vb;
                constraints.push_back(C);
                //constraints.push_back(glm::ivec3(va, vb, (int)_type));
                found = true;
                break;
            }
        }
    }
    return found;
}

int _gliderImporter::findBottomTrailingEdge(int idx)
{
    if (verts[idx].uv.y == 1.f) {
        int R = (int)verts[idx].uv.x;
        if (R >= 0) // ignore wingtips
        {
            return full_ribs[R].v_start;
        }
    }

    return idx; // its not trailing edge, leave it alone
}

void _gliderImporter::insertSkinTriangle(int _a, int _b, int _c)
{
    tris.push_back(glm::ivec3(_a, _b, _c));

    // WATERTIGHT
    tris_airtight.push_back(glm::ivec3(findBottomTrailingEdge(_a), findBottomTrailingEdge(_b), findBottomTrailingEdge(_c)));

    _CNST C;
    C.v_0 = _a;
    C.v_1 = _b;
    C.type = constraintType::skin;
    constraints.push_back(C);
    //constraints.push_back(glm::ivec3(_a, _b, constraintType::skin));

    float3 a = verts[_a]._x;
    float3 b = verts[_b]._x;
    float3 c = verts[_c]._x;

    // build teh join lists
    // now we have waht we need to build the normals index
    if (std::find(verts[_a].joint_verts.begin(), verts[_a].joint_verts.end(), _b) != verts[_a].joint_verts.end()) verts[_a].joint_verts.push_back(_b);
    if (std::find(verts[_a].joint_verts.begin(), verts[_a].joint_verts.end(), _c) != verts[_a].joint_verts.end()) verts[_a].joint_verts.push_back(_c);

    if (std::find(verts[_b].joint_verts.begin(), verts[_b].joint_verts.end(), _a) != verts[_b].joint_verts.end()) verts[_b].joint_verts.push_back(_a);
    if (std::find(verts[_b].joint_verts.begin(), verts[_b].joint_verts.end(), _c) != verts[_b].joint_verts.end()) verts[_b].joint_verts.push_back(_c);

    if (std::find(verts[_c].joint_verts.begin(), verts[_c].joint_verts.end(), _a) != verts[_c].joint_verts.end()) verts[_c].joint_verts.push_back(_a);
    if (std::find(verts[_c].joint_verts.begin(), verts[_c].joint_verts.end(), _b) != verts[_c].joint_verts.end()) verts[_c].joint_verts.push_back(_b);


    float3 cross = glm::cross(b - a, c - a);
    float area = 0.5f * glm::length(cross);      // 0.5 because....

    //totalWingSurface += area;
    totalWingSurface += area;
    totalWingWeight += area * wingWeightPerSqrm;    // FIXME do for all tri pushes
    totalSurfaceTriangles++;

    area /= 3.f;

    verts[tris.back().x].area += area;
    verts[tris.back().y].area += area;
    verts[tris.back().z].area += area;

    verts[tris.back().x].temp_norm += cross;
    verts[tris.back().y].temp_norm += cross;
    verts[tris.back().z].temp_norm += cross;


    verts[tris.back().x].mass += area * wingWeightPerSqrm;
    verts[tris.back().y].mass += area * wingWeightPerSqrm;
    verts[tris.back().z].mass += area * wingWeightPerSqrm;
}


void _gliderImporter::exportXfoil(int r)
{
    auto& R = full_ribs[r];

    std::ofstream outfile("E:/Omega.txt");
    for (int i = R.lower.size() - 1; i >= 0; i--)
    {
        float x, y;
        float3 V = R.lower[i] - R.lower[0];
        x = 1.f - glm::dot(V, R.front) / R.chord;
        y = glm::dot(V, R.up) / R.chord;
        outfile << x << " " << y << "\n";
    }
    outfile.close();


    std::ofstream outfile2("E:/Omega_V.txt");
    for (int i = 0; i < R.lower.size(); i++)
    {
        float y;
        y = R.v_lookup[i];
        outfile2 << y << "\n";
    }
    outfile2.close();
}


void _gliderImporter::fullWing_to_obj()
{
    halfRib_to_Full();
    //numRibScale == 4;
    interpolateRibs();





    verts.clear();
    tris.clear();
    tris_airtight.clear();
    constraints.clear();
    totalLineMeters = 0;
    totalLineWeight = 0;
    totalWingSurface = 0;
    totalWingWeight = 0;
    totalSurfaceTriangles = 0;

    trailsSpan = 0;
    for (int i = 0; i < full_ribs.size() - 1; i++)
    {
        trailsSpan += glm::length(full_ribs[i].trailEdge - full_ribs[i + 1].trailEdge);
    }

    _VTX newVert;

    int cnt = 0;
    int v_start = 0;
    for (auto& r : full_ribs)
    {
        r.v_start = v_start;
        r.calc_V();

        int idx = 0;
        for (auto v : r.ribVerts)
        {
            newVert._x = v;
            newVert.area = 0;
            newVert.mass = 0;

            int vIdx = (int)floor(r.ribIndex[idx]);
            newVert.uv.y = r.v_lookup[vIdx];
            newVert.uv.x = (float)cnt;// / ((float)full_ribs.size() - 1.f); // just save rib numbers for now, convert to 0-1 for render inside shader - also endcaps
            newVert.type = vtx_surface;
            idx++;
            verts.push_back(newVert);
        }

        int numV = r.ribVerts.size();
        for (int i = 0; i < numV; i++)
        {
            bool isNitonol = false;
            int start = (int)(r.ribVerts.size() * 0.375f);
            if (cnt % numRibScale == 0 && (i >= start) && (i < r.ribVerts.size() - numMiniRibs - 2))
            {
                isNitonol = true;
            }

            if (isNitonol)
            {
                _CNST C;
                C.v_0 = v_start + i;
                C.v_1 = v_start + ((i + 1) % numV);
                C.type = constraintType::nitinol;
                constraints.push_back(C);
                //                constraints.push_back(glm::ivec3(v_start + i, v_start + ((i + 1) % numV), constraintType::nitinol));
            }
            else
            {
                _CNST C;
                C.v_0 = v_start + i;
                C.v_1 = v_start + ((i + 1) % numV);
                C.type = constraintType::skin;
                constraints.push_back(C);
                //                constraints.push_back(glm::ivec3(v_start + i, v_start + ((i + 1) % numV), constraintType::skin));
            }
        }

        // extra nitonol
        if (cnt % numRibScale == 0)
        {
            int start = (int)(r.ribVerts.size() * 0.375f);
            int end = r.ribVerts.size() - numMiniRibs - 3;

            for (int i = start; i < end - 1; i++)
            {
                //constraints.push_back(glm::ivec3(v_start + i, v_start + i + 1, constraintType::nitinol));

                _CNST C;
                C.v_0 = v_start + i;
                C.v_1 = v_start + i + 2;
                C.type = constraintType::nitinol;
                constraints.push_back(C);
                //constraints.push_back(glm::ivec3(v_start + i, v_start + i + 2, constraintType::nitinol));
            }
        }

        // internal ribs
        // Start at 1. ) is already included in teh circumference
        if (cnt % numRibScale == 0)
        {
            _CNST C;
            C.type = constraintType::ribs;

            for (int i = 1; i < (numV / 2) - 1; i++)
            {
                C.v_0 = v_start + i;
                C.v_1 = v_start + numV - 1 - i;
                constraints.push_back(C);

                C.v_0 = v_start + i + 1;
                C.v_1 = v_start + numV - 1 - i;
                constraints.push_back(C);

                C.v_0 = v_start + i;
                C.v_1 = v_start + numV - 2 - i;
                constraints.push_back(C);

                //constraints.push_back(glm::ivec3(v_start + i, v_start + numV - 1 - i, constraintType::ribs));
                //constraints.push_back(glm::ivec3(v_start + i + 1, v_start + numV - 1 - i, constraintType::ribs));   // CROSS - I think this results in duplicates at leadign edge
                //constraints.push_back(glm::ivec3(v_start + i, v_start + numV - 2 - i, constraintType::ribs));
            }

            C.v_0 = v_start + ((numV / 2) - 1);
            C.v_1 = v_start + numV - 1 - ((numV / 2) - 1);
            constraints.push_back(C);
            //constraints.push_back(glm::ivec3(v_start + ((numV / 2) - 1), v_start + numV - 1 - ((numV / 2) - 1), constraintType::ribs));
        }
        else if (cnt % (numRibScale / 2) == 0)
        {
            _CNST C;
            C.type = constraintType::half_ribs;

            // miniribs
            for (int i = 1; i < numMiniRibs; i++)
            {
                C.v_0 = v_start + i;
                C.v_1 = v_start + numV - 1 - i;
                constraints.push_back(C);

                C.v_0 = v_start + i + 1;
                C.v_1 = v_start + numV - 1 - i;
                constraints.push_back(C);

                C.v_0 = v_start + i;
                C.v_1 = v_start + numV - 2 - i;
                constraints.push_back(C);

                //constraints.push_back(glm::ivec3(v_start + i, v_start + numV - 1 - i, constraintType::half_ribs));
                //constraints.push_back(glm::ivec3(v_start + i + 1, v_start + numV - 1 - i, constraintType::ribs));   // CROSS - I think this results in duplicates at leadign edge
                //constraints.push_back(glm::ivec3(v_start + i, v_start + numV - 2 - i, constraintType::ribs));
            }
        }
        else
        {
            _CNST C;
            C.type = constraintType::half_ribs;
            // Just o 1 minirin to simulat stich stiffneess at trrailign edge and foldover of materials
            C.v_0 = v_start + 1;
            C.v_1 = v_start + numV - 1 - 1;
            constraints.push_back(C);
            //constraints.push_back(glm::ivec3(v_start + 1, v_start + numV - 1 - 1, constraintType::half_ribs));
        }




        v_start = verts.size();
        cnt++;
    }

    numSkinVerts = verts.size();


    // has to hampeen after v_start calc
    for (auto& r : full_ribs)
    {
        r.calcIndices();
        //r.calc_V(); 
    }

    //VECS
    int totalSize = full_ribs[0].lower.size();
    for (auto& vec : VECS)
    {
        if (vec.isVec)
        {
            for (int j = 0; j < vec.idx.size() - 1; j++)
            {
                bool mirror = true;
                int k = vec.idx[j];
                if (k < 0) k += totalSize;
                int l = vec.idx[j + 1];
                if (l < 0) l += totalSize;
                insertEdge(half_ribs[vec.from + j].lower[k], half_ribs[vec.from + j + 1].lower[l], mirror, constraintType::vecs);
            }
            // special case for middle
            if (vec.from + vec.idx.size() == half_ribs.size())
            {
                float3 p1 = half_ribs.back().lower[vec.idx.back()];
                float3 p2 = p1;
                p2.y *= -1;
                insertEdge(p1, p2, false, constraintType::vecs);
            }
        }
    }

    // diagonals
    // Asumes all ribs have equal number of points, otherwise other things break
    int numRibVerts = full_ribs[0].lower.size();

    fprintf(terrafectorSystem::_logfile, "diagonals\n");
    for (int d = 0; d < diagonals.size(); d++)
    {
        int ri = diagonals[d].from_rib * numRibScale;
        int r2 = diagonals[d].to_rib * numRibScale;
        float3 ps = full_ribs[ri].lower[diagonals[d].from_idx];

        int ia = diagonals[d].range_from;
        int ib = diagonals[d].range_to;
        if (ia < 0) ia = numRibVerts + ia;
        if (ib < 0) ib = numRibVerts + ib;

        std::vector<float3> diagonalTopSpots;
        diagonalTopSpots.clear();
        for (int l = 0; l < full_ribs[r2].ribVerts.size(); l++)
        {
            for (int k = ia; k < ib; k++)
            {
                if (glm::length(full_ribs[r2].ribVerts[l] - full_ribs[r2].lower[k]) < 0.02f)
                {
                    diagonalTopSpots.push_back(full_ribs[r2].lower[k]);
                    break;
                }
            }
        }
        // ALSO add Onw RIB here
        for (int l = 0; l < full_ribs[ri].ribVerts.size(); l++)
        {
            for (int k = ia; k < ib; k++)
            {
                if (glm::length(full_ribs[ri].ribVerts[l] - full_ribs[ri].lower[k]) < 0.02f)
                {
                    diagonalTopSpots.push_back(full_ribs[ri].lower[k]);
                    break;
                }
            }
        }


        float3 newDvert;
        for (auto& D : diagonalTopSpots)
        {
            newDvert += glm::lerp(ps, D, 0.15f);
        }
        newDvert /= (float)diagonalTopSpots.size();
        _VTX newV;
        newV.area = 0.01f;
        newV.mass = 0.01f;  // FIXME just veryu light for now
        newV._x = newDvert;
        newV.type = vtx_internal;
        //newV.mass = ???   
        verts.push_back(newV);
        newV._x.y *= -1;
        verts.push_back(newV);

        bool mirror = true;
        insertEdge(ps, newDvert, mirror, constraintType::diagonals, 0.02f);
        for (auto& D : diagonalTopSpots)
        {
            insertEdge(newDvert, D, mirror, constraintType::diagonals, 0.02f);
        }
    }
    numDiagonalVerts = verts.size() - numSkinVerts;


    // BUld teh surface
    v_start = 0;
    for (int idx = 0; idx < full_ribs.size() - 1; idx++)
    {
        auto& a = full_ribs[idx];
        auto& b = full_ribs[idx + 1];
        int cnt_a = 0;
        int cnt_b = 0;
        bool flip = false;
        int numTris = 0;
        int vA = v_start;
        int vB = v_start + a.ribVerts.size();

        while (numTris < (a.ribVerts.size() + b.ribVerts.size()))
        {
            if ((cnt_a < a.ribVerts.size() - 2) && (cnt_b < b.ribVerts.size() - 2))
            {
                float dotA = glm::dot(a.ribVerts[cnt_a + 1] - a.ribVerts[cnt_a], b.ribVerts[cnt_b] - a.ribVerts[cnt_a]);
                float dotA1 = glm::dot(b.ribVerts[cnt_b + 1] - a.ribVerts[cnt_a + 1], a.ribVerts[cnt_a] - a.ribVerts[cnt_a + 1]);
                float dotB = glm::dot(b.ribVerts[cnt_b + 1] - b.ribVerts[cnt_b], a.ribVerts[cnt_a] - b.ribVerts[cnt_b]);
                float dotB1 = glm::dot(a.ribVerts[cnt_a + 1] - b.ribVerts[cnt_b + 1], b.ribVerts[cnt_b] - b.ribVerts[cnt_b + 1]);

                float angleA = acos(dotA1) + acos(dotB);
                float angleB = acos(dotA) + acos(dotB1);


                if (angleA > angleB)
                {
                    insertSkinTriangle(vA + cnt_a, vB + cnt_b, vA + cnt_a + 1);
                    cnt_a++;
                }
                else
                {
                    insertSkinTriangle(vA + cnt_a, vB + cnt_b, vB + cnt_b + 1);
                    cnt_b++;
                }

                numTris++;
            }
            else
            {
                break;
            }
        }
        // now we are exactly 3 short start at the longer side and add them in
        if (cnt_a == a.ribVerts.size() - 2)
        {
            insertSkinTriangle(vA + cnt_a, vB + cnt_b, vB + cnt_b + 1);
            insertSkinTriangle(vA + cnt_a, vB + cnt_b + 1, vA + cnt_a + 1);
            insertSkinTriangle(vA + cnt_a + 1, vB + cnt_b + 1, vB + cnt_b + 2);
        }
        else
        {
            insertSkinTriangle(vA + cnt_a, vB + cnt_b, vA + cnt_a + 1);
            insertSkinTriangle(vA + cnt_a + 1, vB + cnt_b, vB + cnt_b + 1);
            insertSkinTriangle(vA + cnt_a + 1, vB + cnt_b + 1, vA + cnt_a + 2);
        }

        v_start += a.ribVerts.size();
    }

    addEndCaps();
    calculateNormals();
    calculateMass();
    calculateConstraintBuckets();

    toObj("E:/Advance/omega_xpdb_surface.obj", constraintType::skin);
    toObj("E:/Advance/omega_xpdb_ribs.obj", constraintType::ribs);
    toObj("E:/Advance/omega_xpdb_vecs.obj", constraintType::vecs);
    toObj("E:/Advance/omega_xpdb_diagonals.obj", constraintType::diagonals);

    exportXfoil(full_ribs.size() / 2);

}

void _gliderImporter::calculateConstraintBuckets()
{

    int numC = constraints.size();
    int numV = verts.size();
    int minBuckets = (numC / BUCKET_SIZE) + 1;
    //int bucketNum = 0;
    int totalC = 0;

    ConstraintBuckets.clear();

    std::vector<char> ConstraintUsed;
    ConstraintUsed.resize(numC);
    memset(ConstraintUsed.data(), 0, numC);  // CLEAR

    std::vector<char> VertsUsed;
    VertsUsed.resize(numV);
    memset(VertsUsed.data(), 0, numV);  // CLEAR

    while (totalC < numC)
    {
        ConstraintBuckets.emplace_back();
        auto& B = ConstraintBuckets.back();
        for (int i = 0; i < BUCKET_SIZE; i++)       B[i] = -1;   // clear to -1
        int numCurrent = 0;

        memset(VertsUsed.data(), 0, numV);  // CLEAR

        for (int j = 0; j < numC; j++)
        {
            if ((ConstraintUsed[j] == 0) && (VertsUsed[constraints[j].v_0] == 0) && (VertsUsed[constraints[j].v_1] == 0))
            {
                B[numCurrent] = j;
                ConstraintUsed[j] = 1;
                VertsUsed[constraints[j].v_0] = 1;
                VertsUsed[constraints[j].v_1] = 1;
                numCurrent++;
                totalC++;

                if (numCurrent == BUCKET_SIZE)
                {
                    break;
                }
            }
        }

        fprintf(terrafectorSystem::_logfile, "Bucket %d - numC %d, totalC %d / %d\n", (int)ConstraintBuckets.size(), numCurrent, totalC, numC);

    }
}

void _gliderImporter::calculateMass()
{
    float wingWeight = 5.0f;    // 5kg, maybe 10 toi account for air
    for (auto& v : verts)
    {
        if (v.mass == 0.f)
        {
            float weight = wingWeight * v.area / totalWingSurface;
            weight = __max(0.001f, weight); // 1 gram minimum weiwght PLUS this does lines BAD
            v._w = 1.f / weight;
            v.mass = weight;
        }
        else
        {
            v._w = 1.f / v.mass;
        }

    }
}

void _gliderImporter::exportWing()
{
    calculateMass();
    calculateConstraintBuckets();

    uint _dot = 28;
    uint _scale = 1 << _dot;

    std::ofstream ofs;
    ofs.open("E:/Advance/omega_export_low.bin", std::ios::binary);
    if (ofs)
    {
        ofs.write((const char*)&carabinerRight, sizeof(int));
        ofs.write((const char*)&carabinerLeft, sizeof(int));
        ofs.write((const char*)&brakeRight, sizeof(int));
        ofs.write((const char*)&brakeLeft, sizeof(int));


        int numV = verts.size();
        int numT = tris.size();
        int numC = constraints.size();
        int numR = full_ribs.size();
        int bucketSize = BUCKET_SIZE;
        int numBuckets = ConstraintBuckets.size();
        ofs.write((const char*)&numV, sizeof(int));
        ofs.write((const char*)&numT, sizeof(int));
        ofs.write((const char*)&numC, sizeof(int));
        ofs.write((const char*)&numR, sizeof(int));
        ofs.write((const char*)&bucketSize, sizeof(int));
        ofs.write((const char*)&numBuckets, sizeof(int));

        for (int i = 0; i < numV; i++)              //_x
        {
            int3 x;
            x.x = -(int)((double)verts[i]._x.x * _scale);
            x.y = (int)((double)verts[i]._x.z * _scale);
            x.z = (int)((double)verts[i]._x.y * _scale);
            ofs.write((const char*)&x, sizeof(int3));
        }

        for (int i = 0; i < numV; i++)              // cross_idx
        {
            ofs.write((const char*)&verts[i].cross_idx, sizeof(uint4));
            if (verts[i].cross_idx.x >= numV)  fprintf(terrafectorSystem::_logfile, "VTX %d, cross.x >= type.size(), %d / %d\n", i, verts[i].cross_idx.x, numV);
            if (verts[i].cross_idx.y >= numV)  fprintf(terrafectorSystem::_logfile, "VTX %d, cross.y >= type.size(), %d / %d\n", i, verts[i].cross_idx.y, numV);

        }

        for (int i = 0; i < numV; i++)              // type
        {
            char type = verts[i].type;  // FIXME
            ofs.write((const char*)&type, sizeof(char));
        }

        for (int i = 0; i < numV; i++)              // area * w
        {
            float areasW = verts[i].area * verts[i]._w;
            ofs.write((const char*)&areasW, sizeof(float));
        }

        for (int i = 0; i < numV; i++)              // uv
        {
            ofs.write((const char*)&verts[i].uv, sizeof(float2));
        }


        // triangles
        ofs.write((const char*)tris.data(), sizeof(glm::ivec3) * numT);


        // air tight triangles - thy share bottom trainling edge
        ofs.write((const char*)tris_airtight.data(), sizeof(glm::ivec3) * numT);

        if (tris.size() != tris_airtight.size())
        {
            fprintf(terrafectorSystem::_logfile, "AAAAAAAAAAAAAAAAAAAAAAAAHHHHHHHHHHHHHHHHHHHHHHHHHH airtight\n");
        }

        // Constraint buckets
        for (int i = 0; i < numBuckets; i++)
        {
            ofs.write((const char*)ConstraintBuckets[i].data(), sizeof(int) * bucketSize);
        }

        // constraints
        for (int i = 0; i < numC; i++)              // uv
        {
            _new_constraint NC;
            NC.idx_0 = constraints[i].v_0;
            NC.idx_1 = constraints[i].v_1;
            float wSum = verts[NC.idx_0]._w + verts[NC.idx_1]._w;
            NC.w_rel_0 = verts[NC.idx_0]._w / wSum;
            NC.w_rel_1 = verts[NC.idx_1]._w / wSum;

            NC.w0 = verts[NC.idx_0]._w;
            NC.w1 = verts[NC.idx_1]._w;
            NC.wsum = wSum;
            NC.name[0] = 0;

            if (constraints[i].type >= lines)
            {
                wSum = constraints[i].w0 + constraints[i].w1;
                NC.w_rel_0 = constraints[i].w0 / wSum;
                NC.w_rel_1 = constraints[i].w1 / wSum;

                NC.w0 = constraints[i].w0;
                NC.w1 = constraints[i].w1;
                NC.wsum = wSum;
                memcpy(NC.name, constraints[i].name, 8);

            }

            NC.type = constraints[i].type;
            NC.l = __max(0.001f, glm::length(verts[NC.idx_0]._x - verts[NC.idx_1]._x));
            NC.old_L = NC.l;
            //NC.current_L = NC.l;
            NC.tensileStiff = 1.f;
            NC.compressStiff = 0.01f;
            NC.damping = 0.01f;

            if (NC.type == constraintType::lines) {
                NC.compressStiff = 0.f;
            }

            if (NC.type == constraintType::smallLines) {
                NC.compressStiff = 0.5f;
                //NC.w_0 = 0.5f;
                //NC.w_1 = 0.5f;    // FIXME do this only when lien verts not when skin or carabiner
                if (verts[NC.idx_0].type == vtx_line || verts[NC.idx_1].type == vtx_line)
                {
                    //NC.w_0 = 0.5f;
                    //NC.w_1 = 0.5f;    // FIXME do this only when lien verts not when skin or carabiner
                }

                // FIXME find a way to save this, at the moment I save ivec3 at creation, no way to know how many small lines to big line
                //NC.l *= 0.996f; // 0.4% shorter but do in export its a factor f the number of splits
            }

            if (NC.type == constraintType::nitinol) {
                NC.compressStiff = 0.005f;
            }


            ofs.write((const char*)&NC, sizeof(_new_constraint));

            switch (NC.type)
            {
            case constraintType::skin:
                fprintf(terrafectorSystem::_logfile, "skin   %d, %dmm, (%d, %d), %d (%2.2f, %2.2f)\n", i, (int)(1000.f * NC.l), NC.idx_0, NC.idx_1, NC.type, NC.w_rel_0, NC.w_rel_1);
                break;
            case constraintType::ribs:
                fprintf(terrafectorSystem::_logfile, "ribs   %d, %dmm, (%d, %d), %d (%2.2f, %2.2f)\n", i, (int)(1000.f * NC.l), NC.idx_0, NC.idx_1, NC.type, NC.w_rel_0, NC.w_rel_1);
                break;
            case constraintType::half_ribs:
                fprintf(terrafectorSystem::_logfile, "hlf_ri %d, %dmm, (%d, %d), %d (%2.2f, %2.2f)\n", i, (int)(1000.f * NC.l), NC.idx_0, NC.idx_1, NC.type, NC.w_rel_0, NC.w_rel_1);
                break;
            case constraintType::vecs:
                fprintf(terrafectorSystem::_logfile, "vecs   %d, %dmm, (%d, %d), %d (%2.2f, %2.2f)\n", i, (int)(1000.f * NC.l), NC.idx_0, NC.idx_1, NC.type, NC.w_rel_0, NC.w_rel_1);
                break;
            case constraintType::diagonals:
                fprintf(terrafectorSystem::_logfile, "diagon %d, %dmm, (%d, %d), %d (%2.2f, %2.2f)\n", i, (int)(1000.f * NC.l), NC.idx_0, NC.idx_1, NC.type, NC.w_rel_0, NC.w_rel_1);
                break;
            case constraintType::nitinol:
                fprintf(terrafectorSystem::_logfile, "nitinol %d, %dmm, (%d, %d), %d (%2.2f, %2.2f)\n", i, (int)(1000.f * NC.l), NC.idx_0, NC.idx_1, NC.type, NC.w_rel_0, NC.w_rel_1);
                break;
            case constraintType::lines:
                fprintf(terrafectorSystem::_logfile, "lines  %d, %dmm, (%d, %d), %d (%2.2f, %2.2f)\n", i, (int)(1000.f * NC.l), NC.idx_0, NC.idx_1, NC.type, NC.w_rel_0, NC.w_rel_1);
                break;
            case constraintType::straps:
                fprintf(terrafectorSystem::_logfile, "straps %d, %dmm, (%d, %d), %d (%2.2f, %2.2f)\n", i, (int)(1000.f * NC.l), NC.idx_0, NC.idx_1, NC.type, NC.w_rel_0, NC.w_rel_1);
                break;
            case constraintType::harnass:
                fprintf(terrafectorSystem::_logfile, "harnas %d, %dmm, (%d, %d), %d (%2.2f, %2.2f)\n", i, (int)(1000.f * NC.l), NC.idx_0, NC.idx_1, NC.type, NC.w_rel_0, NC.w_rel_1);
                break;
            case constraintType::smallLines:
                fprintf(terrafectorSystem::_logfile, "sml_ln %d, %dmm, (%d, %d), %d (%2.2f, %2.2f)\n", i, (int)(1000.f * NC.l), NC.idx_0, NC.idx_1, NC.type, NC.w_rel_0, NC.w_rel_1);
                break;
            }

        }


        // ribs
        for (int i = 0; i < numR; i++)
        {
            _new_rib newRib;

            newRib.aLerp = full_ribs[i].aLerp;
            newRib.bLerp = full_ribs[i].bLerp;
            newRib.chordLerp = full_ribs[i].chordLerp;
            newRib.startVertex = full_ribs[i].v_start;
            newRib.numVerts = full_ribs[i].ribVerts.size();
            newRib.rightIdx = full_ribs[i].R_index;
            newRib.inlet_uv = float2((float)i / (float)(numR - 1), 0.95f);  //??? Is that the right spot - MEasure it
            newRib.chordLength = full_ribs[i].chord;

            ofs.write((const char*)&newRib, sizeof(_new_rib));
        }





        // ########################################################################################################
        // now write the debug infor at the end of the file where the loader can ignore it
        // ########################################################################################################
        for (int i = 0; i < numV; i++)              // area * w
        {
            ofs.write((const char*)&verts[i].area, sizeof(float));
        }
        for (int i = 0; i < numV; i++)              // area * w
        {
            ofs.write((const char*)&verts[i]._w, sizeof(float));
        }

        ofs.close();
    }
}


/* Add new cap verts*/
void _gliderImporter::addEndCaps()
{
    int numV = full_ribs[0].ribVerts.size();
    int numSegments = (numV - 1) / 2;
    int numNew = numSegments - 2;

    std::vector<float3> capVerts;

    uint vCaps = verts.size();
    uint vRight = full_ribs[0].v_start;
    uint vLeft = full_ribs.back().v_start;
    numEndcapVerts = 0;

    for (int i = 0; i < numNew; i++)
    {
        float3 avs = { 0, 0, 0 };
        avs += full_ribs[0].ribVerts[i + 1];
        avs += full_ribs[0].ribVerts[i + 2];
        avs += full_ribs[0].ribVerts[numV - i - 2];
        avs += full_ribs[0].ribVerts[numV - i - 3];
        avs *= 0.25f;
        float l = glm::length(full_ribs[0].ribVerts[i + 1] - full_ribs[0].ribVerts[numV - i - 3]) * endcapBuldge;

        avs -= full_ribs[0].plane.xyz * l;
        capVerts.push_back(avs);

        _VTX vert;
        vert.area = 0.01f;
        vert.mass = 0.01f;  // FIXME just veryu light for now  ENDCAPS
        vert._x = avs;

        vert.uv.y = (float)i / (float)(numNew - 1);
        vert.uv.x = -1;

        vert.type = vtx_surface;
        verts.push_back(vert);

        vert.uv.x = -2;
        vert._x.y *= -1;
        verts.push_back(vert);
        numEndcapVerts += 2;
    }



    // constriants downt he middle
    bool mirror = true;
    insertEdge(full_ribs[0].ribVerts[0], capVerts[0], mirror, constraintType::skin);
    for (int i = 0; i < capVerts.size() - 1; i++)
    {
        insertEdge(capVerts[i], capVerts[i + 1], mirror, constraintType::skin);
    }
    insertEdge(capVerts.back(), full_ribs[0].ribVerts[numSegments], mirror, constraintType::skin);

    // triangles
    int numTri = numNew * 2 + 1;
    int center = 0;
    int edge = 0;

    // right
    int vLast = vRight + numV - 1;
    insertSkinTriangle(vRight, vRight + 1, vCaps);  // FIXME _a, _b is inserted intot he edge list, so make sure my oders are correct
    insertSkinTriangle(vLast, vCaps, vLast - 1);
    for (int i = 0; i < numNew; i++)
    {
        insertSkinTriangle(vCaps + (i * 2), vRight + i + 1, vRight + i + 2);
        insertSkinTriangle(vLast - i - 1, vCaps + (i * 2), vLast - i - 2);
        if (i < numNew - 1)
        {
            insertSkinTriangle(vCaps + (i * 2), vRight + i + 2, vCaps + (i + 1) * 2);
            insertSkinTriangle(vLast - i - 2, vCaps + (i * 2), vCaps + (i + 1) * 2);
        }
        else
        {
            // last one
            insertSkinTriangle(vCaps + (i * 2), vRight + i + 2, vRight + i + 3);
            insertSkinTriangle(vLast - i - 2, vCaps + (i * 2), vLast - i - 3);
        }
    }


    // lef6t
    vCaps += 1;
    vLast = vLeft + numV - 1;
    insertSkinTriangle(vLeft, vCaps, vLeft + 1);
    insertSkinTriangle(vLast, vLast - 1, vCaps);
    for (int i = 0; i < numNew; i++)
    {
        insertSkinTriangle(vCaps + (i * 2), vLeft + i + 2, vLeft + i + 1);
        insertSkinTriangle(vLast - i - 1, vLast - i - 2, vCaps + (i * 2));
        if (i < numNew - 1)
        {
            insertSkinTriangle(vCaps + (i * 2), vCaps + (i + 1) * 2, vLeft + i + 2);
            insertSkinTriangle(vLast - i - 2, vCaps + (i + 1) * 2, vCaps + (i * 2));
        }
        else
        {
            // last one
            insertSkinTriangle(vCaps + (i * 2), vLeft + i + 3, vLeft + i + 2);
            insertSkinTriangle(vLast - i - 2, vLast - i - 3, vCaps + (i * 2));
        }
    }
}



void _gliderImporter::calculateNormals()
{
    std::vector<uint> neighbourVerts;
    neighbourVerts.reserve(100);

    // AND FOR SMALL LINES
    for (auto& V : verts)
    {
        V.cross_idx = int4(0, 0, 0, 0);
    }

    for (auto& C : constraints)
    {
        if (C.type == constraintType::smallLines)
        {
            //verts[C.x].cross_idx.x = C.y;   // FIXME THIS IS WRONG 
            //verts[C.y].cross_idx.y = C.x;

            if (verts[C.v_0].cross_idx.x == 0)
            {
                verts[C.v_0].cross_idx.x = C.v_1;
            }
            else if (verts[C.v_0].cross_idx.y == 0)
            {
                verts[C.v_0].cross_idx.y = C.v_1;
            }


            if (verts[C.v_1].cross_idx.x == 0)
            {
                verts[C.v_1].cross_idx.x = C.v_0;
            }
            else if (verts[C.v_1].cross_idx.y == 0)
            {
                verts[C.v_1].cross_idx.y = C.v_0;
            }
        }
    }

    for (int i = 0; i < verts.size(); i++)
    {
        if (verts[i].type == vertexType::vtx_line)

        {
            if (verts[i].cross_idx.x == 0)  fprintf(terrafectorSystem::_logfile, "VTX %d, cross.x == 0\n", i);
            else if (verts[i].cross_idx.y == 0)  fprintf(terrafectorSystem::_logfile, "VTX %d, cross.y == 0\n", i);
            else if (verts[i].cross_idx.x >= verts.size())  fprintf(terrafectorSystem::_logfile, "VTX %d, cross.x >= verts.size()\n", i);
            else if (verts[i].cross_idx.y >= verts.size())  fprintf(terrafectorSystem::_logfile, "VTX %d, cross.y >= verts.size()\n", i);
            //else fprintf(terrafectorSystem::_logfile, "VTX %d, OK %d, %d\n", i, verts[i].cross_idx.x, verts[i].cross_idx.y);
        }

        verts[i].temp_norm = glm::normalize(verts[i].temp_norm);



        neighbourVerts.clear();
        for (auto& T : tris)
        {
            if (i == T.x) {
                neighbourVerts.push_back(T.y);
                neighbourVerts.push_back(T.z);
            }
            if (i == T.y) {
                neighbourVerts.push_back(T.x);
                neighbourVerts.push_back(T.z);
            }
            if (i == T.z) {
                neighbourVerts.push_back(T.x);
                neighbourVerts.push_back(T.y);
            }
        }

        std::sort(neighbourVerts.begin(), neighbourVerts.end());
        neighbourVerts.erase(unique(neighbourVerts.begin(), neighbourVerts.end()), neighbourVerts.end());


        int numV = neighbourVerts.size();
        if (numV > 0)
        {
            float best = 0;
            uint4 idx;

            for (int a = 0; a < numV - 3; a++)
            {
                for (int b = a + 1; b < numV - 2; b++)
                {
                    for (int c = b + 1; c < numV - 1; c++)
                    {
                        for (int d = c + 1; d < numV; d++)
                        {
                            float3 v1 = verts[neighbourVerts[b]]._x - verts[neighbourVerts[a]]._x;
                            float3 v2 = verts[neighbourVerts[d]]._x - verts[neighbourVerts[c]]._x;
                            float3 n = glm::cross(v1, v2);
                            float dot = glm::dot(n, verts[i].temp_norm);
                            if (fabs(dot) > best)
                            {
                                best = fabs(dot);
                                if (dot > 0)
                                {
                                    verts[i].cross_idx = uint4(neighbourVerts[a], neighbourVerts[b], neighbourVerts[c], neighbourVerts[d]);
                                }
                                else
                                {
                                    verts[i].cross_idx = uint4(neighbourVerts[a], neighbourVerts[b], neighbourVerts[d], neighbourVerts[c]);
                                }
                            }
                        }
                    }
                }
            }

            if (numV == 3)
            {
                float3 v1 = verts[neighbourVerts[1]]._x - verts[neighbourVerts[0]]._x;
                float3 v2 = verts[neighbourVerts[2]]._x - verts[neighbourVerts[0]]._x;
                float3 n = glm::normalize(glm::cross(v1, v2));
                float dot = glm::dot(n, verts[i].temp_norm);
                if (dot > 0) {
                    verts[i].cross_idx = uint4(neighbourVerts[0], neighbourVerts[1], neighbourVerts[0], neighbourVerts[2]);
                }
                else
                {
                    verts[i].cross_idx = uint4(neighbourVerts[0], neighbourVerts[1], neighbourVerts[2], neighbourVerts[0]);
                }
            }
        }
        /*
        if (numV == 3)
        {
            verts[i].cross_idx = uint4(neighbourVerts[0], neighbourVerts[1], neighbourVerts[0], neighbourVerts[2]);
        }

        if (numV == 2)  // its a line
        {
            verts[i].cross_idx = uint4(neighbourVerts[0], neighbourVerts[1], neighbourVerts[0], neighbourVerts[1]);
        }
        */
    }
}




void _gliderImporter::toObj(char* filename, constraintType _type)
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
        pMesh->mVertices = new aiVector3D[verts.size()];        //??? Can two meshes share verts in assimp
        int vidx = 0;
        for (auto& V : verts)
        {
            pMesh->mVertices[vidx].x = V._x.x;
            pMesh->mVertices[vidx].y = V._x.y;
            pMesh->mVertices[vidx].z = V._x.z;
            vidx++;
        }

        pMesh->mNumVertices = verts.size();

        pMesh->mNumFaces = constraints.size() + tris.size();
        pMesh->mFaces = new aiFace[pMesh->mNumFaces];
        pMesh->mPrimitiveTypes = aiPrimitiveType_LINE | aiPrimitiveType_TRIANGLE;

        int fidx = 0;

        // surface
        if (_type == constraintType::skin)
        {
            for (auto& T : tris)
            {
                aiFace& face = pMesh->mFaces[fidx];
                face.mIndices = new unsigned int[3];
                face.mNumIndices = 3;
                face.mIndices[0] = T.x;
                face.mIndices[1] = T.y;
                face.mIndices[2] = T.z;
                fidx++;
            }
        }
        else
        {
            for (auto& E : constraints)
            {
                if (E.type == _type)
                {
                    aiFace& face = pMesh->mFaces[fidx];
                    face.mIndices = new unsigned int[2];
                    face.mNumIndices = 2;
                    face.mIndices[0] = E.v_0;
                    face.mIndices[1] = E.v_1;
                    fidx++;
                }
            }
        }
    }

    Assimp::Exporter exp;

    exp.Export(scene, "obj", filename);
    //exp.Export(scene, "obj", "E:/Advance/omega_xpdb_surface.obj");

    if (_type == constraintType::skin)
    {
        exp.Export(scene, "stl", filename);
    }

}





void _gliderImporter::sanityChecks()
{
    for (int i = 0; i < maxCTypes; i++)
    {
        numConstraints[i] = 0;
        numDuplicateConstraints[i] = 0;
        num1C_perType[i] = 0;
    }
    //numSkinVerts = 0;
    numDuplicateVerts = 0;
    min = float3(1000, 1000, 1000);
    max = float3(-1000, -1000, -1000);

    for (int i = 0; i < 100; i++) numVconstraints[i] = 0;

    std::vector<int> v_c_count;
    v_c_count.resize(verts.size());
    for (int i = 0; i < verts.size(); i++) v_c_count[i] = 0;

    for (auto& C : constraints)
    {
        v_c_count[C.v_0]++;
        v_c_count[C.v_1]++;
        numConstraints[C.type]++;
    }



    for (int j = 0; j < constraints.size(); j++)
    {
        for (int k = j + 1; k < constraints.size(); k++)
        {
            // compare j, k
            if (((constraints[j].v_0 == constraints[k].v_0) && (constraints[j].v_1 == constraints[k].v_1)) ||
                ((constraints[j].v_0 == constraints[k].v_1) && (constraints[j].v_1 == constraints[k].v_0)))
            {
                numDuplicateConstraints[constraints[j].type]++;
                numDuplicateConstraints[constraints[k].type]++;

                if (numDuplicateConstraints[constraints[j].type] > 1)
                {
                    fprintf(terrafectorSystem::_logfile, "DUPLICATE constraint %d, %d (%2.2f, %2.2f, %2.2f)\n", constraints[j].v_0, constraints[j].v_1, verts[constraints[j].v_0]._x.x, verts[constraints[j].v_0]._x.y, verts[constraints[j].v_0]._x.z);
                    fprintf(terrafectorSystem::_logfile, "DUPLICATE constraint %d, %d (%2.2f, %2.2f, %2.2f)\n", constraints[j].v_0, constraints[j].v_1, verts[constraints[j].v_1]._x.x, verts[constraints[j].v_1]._x.y, verts[constraints[j].v_1]._x.z);
                }

                if (numDuplicateConstraints[constraints[k].type] > 1)
                {
                }
            }
        }
    }


    for (auto& V : v_c_count)
    {
        numVconstraints[V]++;
    }




    for (int j = 0; j < verts.size(); j++)
    {
        for (int k = j + 1; k < verts.size(); k++)
        {
            float L = glm::length(verts[j]._x - verts[k]._x);
            if (L < 0.005f)
            {
                numDuplicateVerts++;
            }
        }

        min = glm::min(min, verts[j]._x);
        max = glm::max(max, verts[j]._x);

    }
    numDuplicateVerts -= (int)full_ribs.size();


    // find carabiners and f-lines
    // -------------------------------------------------------------------------------------------------
    for (int i = 0; i < verts.size(); i++) v_c_count[i] = 0;
    for (auto& C : constraints)
    {
        if (C.type != constraintType::lines)
        {
            v_c_count[C.v_0]++;
            v_c_count[C.v_1]++;
        }
    }

    // BRAKES
    /*
    for (int idx = 0; idx < v_c_count.size(); idx++)
    {
        if (v_c_count[idx] <= 1)
        {
            if (verts[idx]._x.z < min.z + 0.1f)
            {
                //if (verts[idx]._x.y > 0)    carabinerRight = idx;
                //else                        carabinerLeft = idx;
            }
            else
            {
                if (verts[idx]._x.y > 0)    brakeRight = idx;
                else                        brakeLeft = idx;
            }

            //verts[idx].mass = 1000;
            //verts[idx]._w = 0.001f;
        }
    }

    // CARABINER
    for (int idx = 0; idx < v_c_count.size(); idx++)
    {
        //if (v_c_count[idx] <= 2)
        {
            if (verts[idx]._x.z < min.z + 0.01f)
            {
                if (verts[idx]._x.y > 0)    carabinerRight = idx;
                else                        carabinerLeft = idx;

                verts[idx].mass = 1000;
                verts[idx]._w = 0.001f;
            }
        }
    }
    */

    verts[carabinerRight].mass = 1000;
    verts[carabinerRight]._w = 0.001f;
    verts[carabinerLeft].mass = 1000;
    verts[carabinerLeft]._w = 0.001f;

    verts[brakeRight].mass = 1000;
    verts[brakeRight]._w = 0.001f;
    verts[brakeLeft].mass = 1000;
    verts[brakeLeft]._w = 0.001f;

    //for (auto& V : v_c_count)
    for (int idx = 0; idx < v_c_count.size(); idx++)
    {
        if (v_c_count[idx] == 1)
        {
            for (auto& C : constraints)
            {
                if (C.v_0 == idx || C.v_1 == idx)
                {
                    num1C_perType[C.type]++;
                }
            }
        }
    }
}



void _gliderImporter::ExportLineShape()
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
        pMesh->mVertices = new aiVector3D[totalVertexCount];
        int vidx = 0;
        for (auto& R : half_ribs)
        {
            for (auto& V : R.ribVerts)
            {
                pMesh->mVertices[vidx].x = V.x;
                pMesh->mVertices[vidx].y = V.y;
                pMesh->mVertices[vidx].z = V.z;
                vidx++;
            }
        }

        pMesh->mNumVertices = totalVertexCount;

        pMesh->mNumFaces = totalVertexCount * 5;    // just big for now
        pMesh->mFaces = new aiFace[pMesh->mNumFaces];
        pMesh->mPrimitiveTypes = aiPrimitiveType_LINE;

        int fidx = 0;
        int vStart = 0;
        // surface
        for (auto& R : half_ribs)
        {
            int numV = R.ribVerts.size();;
            for (int i = 0; i < numV; i++)
            {
                aiFace& face = pMesh->mFaces[fidx];
                face.mIndices = new unsigned int[2];
                face.mNumIndices = 2;
                face.mIndices[0] = vStart + i;
                face.mIndices[1] = vStart + ((i + 1) % numV);
                fidx++;
            }
            vStart += numV;
        }

        // rib _internal vertical.
        vStart = 0;
        for (auto& R : half_ribs)
        {
            int numV = R.ribVerts.size();;
            for (int i = 1; i < numV / 2; i++)
            {
                aiFace& face = pMesh->mFaces[fidx];
                face.mIndices = new unsigned int[2];
                face.mNumIndices = 2;
                face.mIndices[0] = vStart + i;
                face.mIndices[1] = vStart + numV - i - 1;
                fidx++;
            }
            vStart += R.ribVerts.size();
        }
    }

    Assimp::Exporter exp;
    exp.Export(scene, "obj", "E:/Advance/omega_xpdb.obj");
}


void _gliderImporter::saveLineSet()
{
    std::ofstream ofs;
    ofs.open("E:/Advance/omega_LINESET.bin", std::ios::binary);
    if (ofs)
    {
        int numV = line_verts.size();
        int numS = line_segments.size();
        ofs.write((char*)&numV, sizeof(int));
        ofs.write((char*)&numS, sizeof(int));

        ofs.write((char*)line_verts.data(), sizeof(float3) * numV);
        ofs.write((char*)line_segments.data(), sizeof(_line) * numS);

        ofs.close();
    }
}

void _gliderImporter::loadLineSet()
{
    lineTypes.clear();
    lineTypes.push_back({ "A-9200-035" , 0.00035f, 0.00012f });
    lineTypes.push_back({ "A-8001-040" , 0.00045f, 0.0002f });
    lineTypes.push_back({ "A-8001-050" , 0.0005f, 0.00026f });
    lineTypes.push_back({ "A-8001-070" , 0.0007f, 0.00044f });
    lineTypes.push_back({ "A-8001-090" , 0.0008f, 0.00053f });
    lineTypes.push_back({ "A-8001-130" , 0.001f, 0.0009f });
    lineTypes.push_back({ "A-8001-190" , 0.0012f, 0.00109f });
    lineTypes.push_back({ "A-8001-230" , 0.0014f, 0.00135f });
    lineTypes.push_back({ "A-8001-280" , 0.0015f, 0.00172f });
    lineTypes.push_back({ "A-8001-340" , 0.0018f, 0.0021f });
    lineTypes.push_back({ "A-7850-240" , 0.0019f, 0.0024f });
    lineTypes.push_back({ "STRAP_A" , 0.019f, 0.16f });

    std::ifstream ifs;
    ifs.open("E:/Advance/omega_LINESET.bin", std::ios::binary);
    if (ifs)
    {
        int numV, numS;
        ifs.read((char*)&numV, sizeof(int));
        ifs.read((char*)&numS, sizeof(int));

        line_verts.resize(numV);
        line_segments.resize(numS);
        ifs.read((char*)line_verts.data(), sizeof(float3) * numV);
        ifs.read((char*)line_segments.data(), sizeof(_line) * numS);

        ifs.close();
    }
}


void _gliderImporter::mergeLines(uint a, uint b)
{
    // find common vertex
    if (line_segments[a].v_0 == line_segments[b].v_0)
    {
        line_segments[a].v_0 = line_segments[b].v_1;
    }
    else if (line_segments[a].v_0 == line_segments[b].v_1)
    {
        line_segments[a].v_0 = line_segments[b].v_0;
    }

    else if (line_segments[a].v_1 == line_segments[b].v_0)
    {
        line_segments[a].v_1 = line_segments[b].v_1;
    }
    else if (line_segments[a].v_1 == line_segments[b].v_1)
    {
        line_segments[a].v_1 = line_segments[b].v_0;
    }

    line_segments.erase(line_segments.begin() + b);
}


int _gliderImporter::insertVertex(float3 _p)
{
    for (int i = 0; i < line_verts.size(); i++)
    {
        if (glm::length(line_verts[i] - _p) < 0.001f) return i;
    }

    line_verts.push_back(_p);
    return line_verts.size() - 1;
}


void _gliderImporter::importCSV()
{
    lineTypes.clear();
    lineTypes.push_back({ "A-9200-035" , 0.00035f, 0.00012f });
    lineTypes.push_back({ "A-8001-040" , 0.00045f, 0.0002f });
    lineTypes.push_back({ "A-8001-050" , 0.0005f, 0.00026f });
    lineTypes.push_back({ "A-8001-070" , 0.0007f, 0.00044f });
    lineTypes.push_back({ "A-8001-090" , 0.0008f, 0.00053f });
    lineTypes.push_back({ "A-8001-130" , 0.001f, 0.0009f });
    lineTypes.push_back({ "A-8001-190" , 0.0012f, 0.00109f });
    lineTypes.push_back({ "A-8001-230" , 0.0014f, 0.00135f });
    lineTypes.push_back({ "A-8001-280" , 0.0015f, 0.00172f });
    lineTypes.push_back({ "A-8001-340" , 0.0018f, 0.0021f });
    lineTypes.push_back({ "A-7850-240" , 0.0019f, 0.0024f });
    lineTypes.push_back({ "STRAP" , 0.019f, 0.07f });
    lineTypes.push_back({ "STRAP_BYPASS" , 0.019f, 0.07f });

    line_segments.clear();
    line_verts.clear();
    char name[8];
    int level;
    char riser[256];
    int counter;
    float3 p1, p2;
    float length, diameter, mass;
    char material[256];
    uint idx1, idx2;

    std::ifstream ifs;
    ifs.open("E:/Advance/OmegaXA5_23_Johan_A01_LineData.csv");
    if (ifs)
    {
        //while (1)
        for (int i = 0; i < 104; i++)
        {
            ifs >> name >> level >> riser >> counter >> p1.x >> p1.y >> p1.z >> p2.x >> p2.y >> p2.z >> length >> material >> diameter >> mass;
            idx1 = insertVertex(p1);
            idx2 = insertVertex(p2);
            _line L = { idx1, idx2, 0 };
            memset(L.name, 0, 8);
            memcpy(L.name, name, 8);
            L.diameter = diameter;
            L.mass = mass;
            L.material = 0;
            for (int j = 0; j < lineTypes.size(); j++)
            {
                if (std::strstr(material, lineTypes[j].name.c_str()))
                {
                    L.material = j;
                }
            }

            line_segments.push_back(L);

            //fprintf(terrafectorSystem::_logfile, "%s %d %s %d, (%2.2f, %2.2f, %2.2f), %2.2fm, %s, %2.8fm, %2.8fkg/m\n", name, level, riser, counter, p1.x, p1.y, p1.z, length, material, diameter, mass);


            //if (ifs.eof()) break;
        }

        ifs.clear();
        ifs.seekg(0);

        {
        }

        ifs.close();

        mergeLines(24, 25);
        mergeLines(4, 5);
    }

}



void _gliderImporter::cleanupLineSet()
{
    lineTypes.clear();
    lineTypes.push_back({ "A-9200-035" , 0.00035f, 0.00012f });
    lineTypes.push_back({ "A-8001-040" , 0.00045f, 0.0002f });
    lineTypes.push_back({ "A-8001-050" , 0.0005f, 0.00026f });
    lineTypes.push_back({ "A-8001-070" , 0.0007f, 0.00044f });
    lineTypes.push_back({ "A-8001-090" , 0.0008f, 0.00053f });
    lineTypes.push_back({ "A-8001-130" , 0.001f, 0.0009f });
    lineTypes.push_back({ "A-8001-190" , 0.0012f, 0.00109f });
    lineTypes.push_back({ "A-8001-230" , 0.0014f, 0.00135f });
    lineTypes.push_back({ "A-8001-280" , 0.0015f, 0.00172f });
    lineTypes.push_back({ "A-8001-340" , 0.0018f, 0.0021f });
    lineTypes.push_back({ "A-7850-240" , 0.0019f, 0.0024f });
    lineTypes.push_back({ "STRAP_A" , 0.019f, 0.16f });





    Assimp::Importer importer;
    unsigned int flags = aiProcess_PreTransformVertices | aiProcess_JoinIdenticalVertices;
    const aiScene* sceneLines = importer.ReadFile(line_file.c_str(), flags);
    uint numfaces = 0;

    numLineVerts = 0;
    uint carabinerIndex;
    float carabinerZ = 100000;  // find the carabiner
    if (sceneLines)
    {
        for (int m = 0; m < sceneLines->mNumMeshes; m++)
        {
            aiMesh* M = sceneLines->mMeshes[m];

            line_verts.clear();
            line_verts.resize(M->mNumVertices);
            for (uint i = 0; i < M->mNumVertices; i++)
            {
                line_verts[i] = float3(M->mVertices[i].x, M->mVertices[i].y, M->mVertices[i].z);
                carabinerZ = __min(carabinerZ, line_verts[i].z);
            }

            for (uint i = 0; i < M->mNumFaces; i++)
            {
                aiFace& F = M->mFaces[i];
                if (F.mNumIndices == 2)
                {
                    line_segments.push_back({ F.mIndices[0], F.mIndices[1], 0 });
                }
            }
        }
    }

    mergeLines(112, 113);
    mergeLines(107, 108);
    mergeLines(106, 107);
    mergeLines(24, 25);
    mergeLines(4, 5);
}

void _gliderImporter::SubdivideLine(float3 a, float3 b, float mass, float area)
{
    mass /= 2.f;
    area /= 2.f;
    float L = glm::length(a - b);
    float3 NEW = (a + b) * 0.5f;

    _VTX v;
    v.area = area;
    v.mass = mass;
    v._w = 1.f / v.mass;
    v._x = NEW;
    v.type = vtx_line;
    //if (L < 0.4f)          v.type = vtx_small_line;

    verts.push_back(v);
    v._x.y *= -1;
    verts.push_back(v);



    if (L > 0.2f)
    {
        bool mirror = true;
        if (L < 0.4f)
        {
            insertEdge(a, NEW, mirror, constraintType::smallLines, 0.005f);
            insertEdge(NEW, b, mirror, constraintType::smallLines, 0.005f);
        }
        else
        {
            insertEdge(a, NEW, mirror, constraintType::lines, 0.005f);
            insertEdge(NEW, b, mirror, constraintType::lines, 0.005f);
        }

        SubdivideLine(a, NEW, mass, area);
        SubdivideLine(NEW, b, mass, area);
    }
}

//startLineIdx
void _gliderImporter::pushLine(float3 _a, float3 _b, float _mass, float _area, constraintType _type, float _l)
{
    float L = glm::length(_a - _b);

    if (L < _l)
    {
        bool mirror = true;
        pushVertex(_a);
        pushVertex(_b);
        insertEdge(_a, _b, mirror, _type, _mass, _area, 0.01f);
    }
    else
    {
        float3 NEW = (_a + _b) * 0.5f;
        pushLine(_a, NEW, _mass / 2, _area / 2, _type, _l);
        pushLine(NEW, _b, _mass / 2, _area / 2, _type, _l);
    }
}


void _gliderImporter::pushVertex(float3 _a)
{
    int i = 0;
    for (auto& V : verts)
    {
        if (glm::length(_a - V._x) < 0.01f)
        {
            //fprintf(terrafectorSystem::_logfile, "found and existing vertex # %d   type - %d\n", i, V.type);
            return;
        }
        i++;
    }


    _VTX v;
    v.area = 0.0f;
    v.mass = 0;
    v._x = _a;
    v._w = 0;

    v.type = vtx_line;

    verts.push_back(v);
    v._x.y *= -1;
    verts.push_back(v);

    numLineVerts += 2;
    endLineIdx = verts.size();
}

void _gliderImporter::findBrakesCarabiners()
{
    for (auto& L : line_segments)
    {
        if (strstr(L.name, "AC"))   // carabiner
        {
            float3 a = line_verts[L.v_0];
            float3 b = line_verts[L.v_1];
            if (a.z > b.z)
            {
                a = b;
            }
            b = a;
            b.y *= -1;

            for (int vx = 0; vx < verts.size(); vx++)
            {
                if (glm::length(verts[vx]._x - a) < 0.001f) carabinerRight = vx;
                if (glm::length(verts[vx]._x - b) < 0.001f) carabinerLeft = vx;
            }

        }

        if (strstr(L.name, "5Br1"))   // carabiner
        {
            float3 a = line_verts[L.v_0];
            float3 b = line_verts[L.v_1];
            if (a.z > b.z)
            {
                a = b;
            }
            b = a;
            b.y *= -1;

            for (int vx = 0; vx < verts.size(); vx++)
            {
                if (glm::length(verts[vx]._x - a) < 0.001f) brakeRight = vx;
                if (glm::length(verts[vx]._x - b) < 0.001f) brakeLeft = vx;
            }
        }
    }
}


void _gliderImporter::printLineTests()
{
    for (auto& L : line_segments)
    {
        if (strstr(L.name, "1A1") || strstr(L.name, "2A1") || strstr(L.name, "3A1") || strstr(L.name, "AG")
            || strstr(L.name, "EG") || strstr(L.name, "AE"))
        {
            float3 a = line_verts[L.v_0];
            float3 b = line_verts[L.v_1];
            int i0, i1, cidx;
            for (int i = 0; i < verts.size(); i++)
            {
                if (glm::length(a - verts[i]._x) < 0.005f) i0 = i;
                if (glm::length(b - verts[i]._x) < 0.005f) i1 = i;
            }

            for (int i = 0; i < constraints.size(); i++)
            {
                if ((constraints[i].v_0 == i0) && (constraints[i].v_1 == i1) ||
                    (constraints[i].v_0 == i1) && (constraints[i].v_1 == i0)) {
                    cidx = i;
                }
            }

            double _dt = 0.00208333333 / 4;
            double _dt2 = _dt * _dt;
            float Lng = glm::length(a - b);
            float Lng2 = Lng * 0.02f;
            float wSum = verts[i0]._w + verts[i1]._w;
            float force2 = Lng2 / wSum / _dt2;
            float K = force2 / Lng2;

            float gpm = L.mass * 1000;
            float g = gpm * Lng;
            float diam = L.diameter * 1000;
            float area = diam * Lng;
            fprintf(terrafectorSystem::_logfile, "%s, %2.3f m, %2.2f N, wsum - %2.2f, k - %2.2f, linemass %2.2f g (%2.2f g), diameter %2.2f mm (area %2.2f g) \n", L.name, Lng, force2, wSum, K, gpm, g, diam, area);

            float m10 = L.mass * 0.1f;
            float d2_10 = 0.002f;
            float wsum10 = 2.f * (1.f / (m10 / 2.f));
            float f10 = d2_10 / wsum10 / _dt2;
            float k10 = f10 / d2_10;
            fprintf(terrafectorSystem::_logfile, "10 cm line, %2.2f N, wsum - %2.2f, k - %2.2f\n", f10, wsum10, k10);

            fprintf(terrafectorSystem::_logfile, "%d, Z %2.2f m, _w %2.2f, m - %2.2f g, type %d \n", i0, verts[i0]._x.z, verts[i0]._w, verts[i0].mass * 1000, verts[i0].type);
            fprintf(terrafectorSystem::_logfile, "%d, Z %2.2f m, _w %2.2f, m - %2.2f g, type %d  \n\n", i1, verts[i1]._x.z, verts[i1]._w, verts[i1].mass * 1000, verts[i1].type);
        }
    }

    //fprintf(terrafectorSystem::_logfile, "\n\n from %d to %d\n\n", startLineIdx, endLineIdx);
    for (auto& L : line_segments)
    {
        if (strstr(L.name, "1C3") || strstr(L.name, "2C2") || strstr(L.name, "3C2") || strstr(L.name, "4C1") || strstr(L.name, "AF")
            || strstr(L.name, "CF") || strstr(L.name, "AC"))
        {
            float3 a = line_verts[L.v_0];
            float3 b = line_verts[L.v_1];
            int i0, i1, cidx;
            for (int i = 0; i < verts.size(); i++)
            {
                if (glm::length(a - verts[i]._x) < 0.005f) i0 = i;
                if (glm::length(b - verts[i]._x) < 0.005f) i1 = i;
            }

            for (int i = 0; i < constraints.size(); i++)
            {
                if ((constraints[i].v_0 == i0) && (constraints[i].v_1 == i1) ||
                    (constraints[i].v_0 == i1) && (constraints[i].v_1 == i0)) {
                    cidx = i;
                }
            }

            double _dt = 0.00208333333 / 4;
            double _dt2 = _dt * _dt;
            float Lng = glm::length(a - b);
            float Lng2 = Lng * 0.02f;
            float wSum = verts[i0]._w + verts[i1]._w;
            float force2 = Lng2 / wSum / _dt2;
            float K = force2 / Lng2;

            float gpm = L.mass * 1000;
            float g = gpm * Lng;
            float diam = L.diameter * 1000;
            float area = diam * Lng;
            fprintf(terrafectorSystem::_logfile, "%s, %2.3f m, %2.2f N, wsum - %2.2f, k - %2.2f, linemass %2.2f g (%2.2f g), diameter %2.2f mm (area %2.2f g) \n", L.name, Lng, force2, wSum, K, gpm, g, diam, area);

            fprintf(terrafectorSystem::_logfile, "%d, Z %2.2f m, _w %2.2f, m - %2.2f g, type %d \n", i0, verts[i0]._x.z, verts[i0]._w, verts[i0].mass * 1000, verts[i0].type);
            fprintf(terrafectorSystem::_logfile, "%d, Z %2.2f m, _w %2.2f, m - %2.2f g, type %d  \n\n", i1, verts[i1]._x.z, verts[i1]._w, verts[i1].mass * 1000, verts[i1].type);

        }
    }
}

void _gliderImporter::pushLines(constraintType _type, float _cut_length)
{
    float SumLineMass = 0;
    float SumLineMassV = 0;
    float SumLineAREA = 0;
    float SumLineLENGTH = 0;

    fprintf(terrafectorSystem::_logfile, "Pushing lines at %2.3f m\n", _cut_length);
    startCIdx = constraints.size();
    for (auto& L : line_segments)
    {
        float3 a = line_verts[L.v_0];
        float3 b = line_verts[L.v_1];
        float Length = glm::length(b - a);
        //fprintf(terrafectorSystem::_logfile, "pushLine %s %2.2f g, %2.3f m\n", L.name, L.mass * 1000, Length);
        //if(_cut_length >= 100.f || )
        pushLine(a, b, L.mass * Length / 2 * 3, L.diameter * Length, _type, _cut_length);
        memcpy(constraints.back().name, L.name, 8);
        memcpy(constraints[constraints.size() - 2].name, L.name, 8);


    }
    endCIdx = constraints.size();

    findBrakesCarabiners();

    // clear verts
    for (int i = startLineIdx; i < endLineIdx; i++)
    {
        verts[i].area = 0;
        verts[i].mass = 0;
        verts[i]._w = 0;
    }

    // sum lines
    fprintf(terrafectorSystem::_logfile, "sum lines %d to %d\n", startCIdx, endCIdx);
    for (int i = startCIdx; i < endCIdx; i++)
    {
        auto& C = constraints[i];


        if (verts[C.v_0].type == vtx_line)
        {
            verts[C.v_0].area += C.area * 0.5f;
            verts[C.v_0].mass += C.mass * 0.5f;
        }

        if (verts[C.v_1].type == vtx_line)
        {
            verts[C.v_1].area += C.area * 0.5f;
            verts[C.v_1].mass += C.mass * 0.5f;
        }
        SumLineMass += C.mass;
        SumLineAREA += C.area;
        SumLineLENGTH += glm::length(verts[C.v_1]._x - verts[C.v_0]._x);
    }

    verts[carabinerRight].mass = 1000;
    verts[carabinerLeft].mass = 1000;
    verts[brakeRight].mass = 1000;
    verts[brakeLeft].mass = 1000;

    for (int i = startLineIdx; i < endLineIdx; i++)
    {
        verts[i]._w = 1.f / verts[i].mass;
    }

    // write to C
    for (int i = startCIdx; i < endCIdx; i++)
    {
        auto& C = constraints[i];

        C.w0 = verts[C.v_0]._w;
        C.w1 = verts[C.v_1]._w;
    }

    fprintf(terrafectorSystem::_logfile, "SumLineMass % 2.2f g, % 2.2f mm*m, %2.3f m \n", SumLineMass * 1000, SumLineAREA * 1000, SumLineLENGTH);
}


void _gliderImporter::processLineAttachmentPoints_LineSet()
{
    loadLineSet();

    startLineIdx = verts.size();


    pushLines(constraintType::lines, 100.f);


    // Do reports on lines %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    fprintf(terrafectorSystem::_logfile, "\n\n from %d to %d\n\n", startLineIdx, endLineIdx);
    printLineTests();


    pushLines(constraintType::lines, 0.9f);
    pushLines(constraintType::lines, 0.2f);
    pushLines(constraintType::smallLines, 0.05f);



    calculateConstraintBuckets();


    toObj("E:/Advance/omega_xpdb_lines.obj", constraintType::lines);
    toObj("E:/Advance/omega_xpdb_smalllines.obj", constraintType::smallLines);
}


void  _gliderImporter::processLineAttachmentPoints()
{
    /*
    totalLineMeters = 0;
    totalLineWeight = 0;
    totalLineWeight_small = 0;
    totalLineLength_small = 0;

    Assimp::Importer importer;
    unsigned int flags = aiProcess_PreTransformVertices | aiProcess_JoinIdenticalVertices;
    const aiScene* sceneLines = importer.ReadFile(line_file.c_str(), flags);
    uint numfaces = 0;

    numLineVerts = 0;
    uint carabinerIndex;
    float3 CAR_POS;
    float carabinerZ = 100000;  // find the carabiner
    if (sceneLines)
    {
        for (int m = 0; m < sceneLines->mNumMeshes; m++)
        {
            aiMesh* M = sceneLines->mMeshes[m];

            line_verts.clear();
            line_verts.resize(M->mNumVertices);
            for (uint i = 0; i < M->mNumVertices; i++)
            {
                line_verts[i] = float3(M->mVertices[i].x, M->mVertices[i].y, M->mVertices[i].z);
                if (line_verts[i].z < carabinerZ)
                {
                    carabinerZ = line_verts[i].z;
                    carabinerIndex = i;
                    CAR_POS = line_verts[i];
                }
            }
        }
    }

    CAR_POS.y = 0;  // center it

    numLineVertsOnSurface = (int)line_verts.size();
    for (int i = 0; i < line_verts.size(); i++)
    {
        bool isOnSurface = false;
        for (auto& R : half_ribs)
        {
            float closest = 1000;// just big
            int vIdx;
            for (int j = 0; j < R.lower.size(); j++)
            {
                float l = glm::length(line_verts[i] - R.lower[j]);
                if (l < closest)
                {
                    closest = l;
                    vIdx = j;
                }
            }
            if (closest < 0.02f) // 2cm, should be fine
            {
                R.ribLineAttach.push_back(vIdx);
                isOnSurface = true;
                //numLineVertsOnSurface++;
            }
        }

        // if not on surface add to total verts
        if (!isOnSurface)
        {
            bool found = false;
            for (auto& V : verts)
            {
                float L = glm::length(line_verts[i] - V._x);
                if (L < 0.01f)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                float dst = glm::length(line_verts[i] - CAR_POS);
                float scale = glm::clamp(1.f / dst, 0.1f, 1.0f);
                _VTX v;
                v.area = 0.01f;
                v.mass = 0;// 0.01f * scale;  // FIXME just veryu light for now  LINES
                v._x = line_verts[i];
                v.area = 0;
                v._w = 0;// 1.f / v.mass;   // then let the segments add this later

                v.type = vtx_line;
                if (line_verts[i].z < (carabinerZ + 1.f))
                {
                    //v.type = vtx_straps;
                }
                verts.push_back(v);
                v._x.y *= -1;
                verts.push_back(v);
                numLineVerts += 2;
            }
        }
    }


    numSmallLineVerts = 0;
    if (sceneLines)
    {
        for (int m = 0; m < sceneLines->mNumMeshes; m++)
        {
            aiMesh* M = sceneLines->mMeshes[m];

            for (uint i = 0; i < M->mNumFaces; i++)
            {
                aiFace& F = M->mFaces[i];
                if (F.mNumIndices == 2)
                {
                    bool mirror = true;
                    float3 a = line_verts[F.mIndices[0]];
                    float3 b = line_verts[F.mIndices[1]];
                    if (!insertEdge(a, b, mirror, constraintType::lines, 0.001f))
                    {
                        insertEdge(a, b, mirror, constraintType::lines);    // At the top to the wing we need wider seacrh
                    }
                    float L = glm::length(b - a);
                    verts[(constraints.back() - 1).x].mass += lineWeightPerMeter * L / 2;
                    verts[(constraints.back() - 1).y].mass += lineWeightPerMeter * L / 2;
                    verts[(constraints.back()).x].mass += lineWeightPerMeter * L / 2;
                    verts[(constraints.back()).y].mass += lineWeightPerMeter * L / 2;
                    totalLineMeters += L * 2;
                    totalLineWeight += lineWeightPerMeter * L * 2;



                    // Now add the small inbetween steps
                    //float L = glm::length(b - a);
                    int numSteps = (int)(L / lineSpacing) + 1;
                    float dL = 1.f / (float)numSteps;
                    float3 first = a;

                    for (int j = 1; j <= numSteps; j++)
                    {
                        float3 newV = glm::lerp(a, b, j * dL);

                        _VTX v;
                        v._x = newV;
                        v.area = 0.001f * dL;
                        v.mass = lineWeightPerMeter * dL;
                        v.type = vtx_small_line;
                        // I dont think we need cross or uv

                        if (j < numSteps)   // dont add right at the end its already there, just add teh constraint
                        {
                            verts.push_back(v);
                            v._x.y *= -1;
                            verts.push_back(v);
                            numSmallLineVerts += 2;
                        }

                        if (!insertEdge(first, newV, mirror, constraintType::smallLines, 0.005f)) // high acuracy since we are acurqte
                        {
                            insertEdge(first, newV, mirror, constraintType::smallLines);    // FIXND THE SURFACE
                        }
                        totalLineLength_small += dL * 2;
                        totalLineWeight_small += lineWeightPerMeter * dL * 2;

                        first = newV;
                    }

                    // rather do this by taking the line constrainta aand splitting them
                    // that surface attachemnt is a mess
                }
            }
        }
        // We need to sort out widtsh and straps - purely as function of Z i guess ??? or somethign better
        // we could always split the file and do this in steps, or see if we can do some material assignement thing
        // And tehn add teh inbetween lines
    }



    toObj("E:/Advance/omega_xpdb_lines.obj", constraintType::lines);
    toObj("E:/Advance/omega_xpdb_smalllines.obj", constraintType::smallLines);
    */
}



void  _gliderImporter::processRib()
{
    Assimp::Importer importer;
    unsigned int flags = aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_JoinIdenticalVertices;
    const aiScene* sceneRibs = importer.ReadFile(rib_file.c_str(), flags);
    uint numfaces = 0;



    if (sceneRibs)
    {
        for (int m = 0; m < sceneRibs->mNumMeshes; m++)
        {
            aiMesh* M = sceneRibs->mMeshes[m];

            rib_verts.clear();
            rib_verts.resize(M->mNumVertices);
            for (uint i = 0; i < M->mNumVertices; i++)
            {
                rib_verts[i] = float3(M->mVertices[i].x, M->mVertices[i].y, M->mVertices[i].z);
            }
            half_ribs.clear();
            half_ribs.reserve(50);

            float4 currentPlane = { -100, -100, -100, -100 }; // just wewird

            for (uint j = 0; j < M->mNumFaces; j++)
            {
                aiFace face = M->mFaces[j];

                // are we in plane
                float dP = glm::dot((float3)currentPlane.xyz, rib_verts[face.mIndices[0]]) - currentPlane.w;
                if (abs(dP) > 0.01f)
                {
                    float3 a = rib_verts[face.mIndices[1]] - rib_verts[face.mIndices[0]];
                    float3 b = rib_verts[face.mIndices[2]] - rib_verts[face.mIndices[0]];
                    float3 norm = glm::normalize(glm::cross(a, b));
                    currentPlane.xyz = norm;
                    currentPlane.w = glm::dot(norm, rib_verts[face.mIndices[0]]);
                    half_ribs.emplace_back();
                    half_ribs.back().plane = currentPlane;
                    half_ribs.back().edges.clear();
                    half_ribs.back().outer_edge.clear();
                }

                _rib& R = half_ribs.back();

                for (int idx = 0; idx < face.mNumIndices; idx++)
                {
                    int i2 = (idx + 1) % face.mNumIndices;
                    int v1 = face.mIndices[idx];
                    int v2 = face.mIndices[i2];
                    R.edges.push_back(glm::ivec2(v1, v2));
                }
            }

            // now find the outer edge
            for (auto& R : half_ribs)
            {
                for (int i = 0; i < R.edges.size(); i++)
                {
                    bool edge = true;
                    for (int j = 0; j < R.edges.size(); j++)
                    {
                        if (i != j)
                        {
                            auto a = R.edges[i];
                            auto b = R.edges[j];
                            if (((a.x == b.x) && (a.y == b.y)) || ((a.x == b.y) && (a.y == b.x)))
                            {
                                edge = false;
                            }
                        }
                    }

                    if (edge)
                    {
                        R.outer_edge.push_back(R.edges[i]);
                        bool inEdgeList_x = false;
                        bool inEdgeList_y = false;
                        for (auto OI : R.outer_idx)
                        {
                            if (R.edges[i].x == OI) inEdgeList_x = true;
                            if (R.edges[i].y == OI) inEdgeList_y = true;
                        }
                        if (!inEdgeList_x)
                        {
                            R.outer_idx.push_back(R.edges[i].x);
                            R.outerV.push_back(rib_verts[R.edges[i].x]);
                        }
                        if (!inEdgeList_y)
                        {
                            R.outer_idx.push_back(R.edges[i].y);
                            R.outerV.push_back(rib_verts[R.edges[i].y]);
                        }
                    }
                }


                std::sort(R.outerV.begin(), R.outerV.end(), [](float3 a, float3 b) {
                    // Custom comparison logic 
                    return a.x > b.x; // this sorts in ascending order 
                    });


                R.trailEdge = R.outerV[0];
                R.lower.push_back(R.trailEdge);
                for (int i = 1; i < R.outerV.size(); i++)
                {
                    if (R.outerV[i].z < R.trailEdge.z)
                    {
                        R.lower.push_back(R.outerV[i]);
                    }
                }

                R.leadindex = R.lower.size();

                std::sort(R.outerV.begin(), R.outerV.end(), [](float3 a, float3 b) {
                    // Custom comparison logic 
                    return a.x < b.x; // this sorts in ascending order 
                    });

                R.leadEdge = R.outerV[0];
                R.chord = glm::length(R.leadEdge - R.trailEdge);
                for (int i = 1; i < R.outerV.size(); i++)
                {
                    if (R.outerV[i].z >= R.trailEdge.z)
                    {
                        R.lower.push_back(R.outerV[i]);
                    }
                }

                R.circumference = 0;
                for (int i = 0; i < R.lower.size() - 1; i++)
                {
                    R.circumference += glm::length(R.lower[i] - R.lower[i + 1]);
                }


                R.front = glm::normalize(R.leadEdge - R.trailEdge);
                R.up = glm::normalize(glm::cross(R.front, (float3)R.plane.xyz));
            }



            // Now find the trailing edge - largest x value
        }
    }


    if (reverseRibs)
    {
        std::reverse(half_ribs.begin(), half_ribs.end());
    }
}


void _gliderImporter::placeSingleRibVerts(_rib& R, int numR, float _scale)
{
    float skewCircumference = 0;
    for (int i = 0; i < R.lower.size() - 1; i++)
    {
        float scale = 1.f;
        float trail = (R.trailEdge.x - R.lower[i].x) / (R.chord * 0.25f);
        float lead = (R.lower[i].x - R.leadEdge.x) / (R.chord * 0.25f);
        if (trail < 1) scale = 1 + trailingBias * (1 - trail);
        if (lead < 1) scale = 1 + leadBias * (1 - lead);
        skewCircumference += glm::length(R.lower[i] - R.lower[i + 1]) * scale;
    }

    R.ribVerts.clear();
    R.ribIndex.clear();
    float step = 0.f;
    float distance = 0;
    int numBaseVerts = ribVertCount;
    if (!fixedRibcount) numBaseVerts = (int)(skewCircumference / ribSpacing);
    numBaseVerts = __max(numBaseVerts, 16);//clamp at 16

    float spacing = 0.3f;// skewCircumference / numBaseVerts;


    R.ribVerts.push_back(R.lower.front());        // trailing edge
    R.ribIndex.push_back(0);

    //std::vector<int> fixedVerts = forceRibVerts[numR];
    //fixedVerts.push_back(R.leadindex);

    // Build fixedVerts from VERTS
    std::vector<int> fixedVerts;
    for (auto& vec : VECS)
    {
        for (int j = 0; j < vec.idx.size(); j++)
        {
            if (vec.from + j == numR)
            {
                fixedVerts.push_back(vec.idx[j]);
                if (fixedVerts.back() < 0)
                {
                    fixedVerts.back() = abs(fixedVerts.back()) - 1;
                }
            }
        }
    }
    fixedVerts.push_back(R.leadindex);
    std::sort(fixedVerts.begin(), fixedVerts.end());


    int baseVertex = 0;
    for (int i = 0; i < fixedVerts.size(); i++)
    {
        //fixedVerts[i] = (fixedVerts[i] + R.lower.size()) % R.lower.size(); // move top ones to the bottom
        //fixedVerts[i] = abs(fixedVerts[i]);
        if (fixedVerts[i] < 0) fixedVerts[i] = abs(fixedVerts[i]) - 1;
        spacing = _scale;

        float seglength = 0;
        for (int j = baseVertex; j < fixedVerts[i]; j++)
        {
            float scale = 1.f;
            float trail = (R.trailEdge.x - R.lower[j].x) / (R.chord * 0.25f);
            float lead = (R.lower[j].x - R.leadEdge.x) / (R.chord * 0.15f);
            if (trail < 1) scale = 1 + trailingBias * (1 - trail);
            if (lead < 1) scale = 1 + leadBias * (1 - lead);

            seglength += glm::length(R.lower[j] - R.lower[j + 1]) * scale;
        }
        int numV = (int)ceil(seglength / spacing);
        float segSpacing = seglength / numV;
        float segStep = 0;

        for (int j = baseVertex; j < fixedVerts[i] - 1; j++)
        {
            float scale = 1.f;
            float trail = (R.trailEdge.x - R.lower[j].x) / (R.chord * 0.25f);
            float lead = (R.lower[j].x - R.leadEdge.x) / (R.chord * 0.15f);
            if (trail < 1) scale = 1 + trailingBias * (1 - trail);
            if (lead < 1) scale = 1 + leadBias * (1 - lead);

            float l = glm::length(R.lower[j] - R.lower[j + 1]) * scale;
            float step2 = segStep + l;
            if (step2 > segSpacing)
            {
                float ds = ((step2 - segSpacing) / l);
                R.ribVerts.push_back(glm::lerp(R.lower[j], R.lower[j + 1], 1 - ds));
                R.ribIndex.push_back(j + (1 - ds));
                segStep -= segSpacing;
            }
            segStep += l;
        }

        R.ribVerts.push_back(R.lower[fixedVerts[i]]);
        R.ribIndex.push_back((float)fixedVerts[i]);
        baseVertex = fixedVerts[i];
    }



    // Now invert for the top, use X search - THIS PART WORKS FOR NOW
    int start = R.ribVerts.size() - 2;
    for (int j = start; j >= 0; j--)
    {
        float targetX = R.ribVerts[j].x;

        for (int k = baseVertex + 1; k < R.lower.size(); k++)
        {
            if ((R.lower[k - 1].x < targetX) && (R.lower[k].x >= targetX))
            {
                float ds = (targetX - R.lower[k - 1].x) / (R.lower[k].x - R.lower[k - 1].x);
                R.ribVerts.push_back(glm::lerp(R.lower[k - 1], R.lower[k], ds));
                R.ribIndex.push_back((k - 1) + ds);
                break;
            }
        }
    }

    totalVertexCount += R.ribVerts.size();;
}



void _gliderImporter::placeRibVerts()
{
    totalVertexCount = 0;


    int minimum = 20;

    for (int i = half_ribs.size() - 1; i >= 0; i--)
    {
        float scale = ribSpacing;
        int cnt = 0;
        do
        {
            placeSingleRibVerts(half_ribs[i], i, scale);
            scale *= 0.99f;
            cnt++;
            if (cnt == 200) break;
        } while (half_ribs[i].ribVerts.size() < minimum);

        minimum = half_ribs[i].ribVerts.size() - 2;

    }
}



ImVec2 _gliderImporter::project(float3 p, float3 root, float3 right, ImVec2 _pos, float _scale)
{
    float3 up = { 0, 0, 1 };
    float3 vec = (p - root);
    float x = glm::dot(right, vec) * _scale;
    float y = glm::dot(up, vec) * _scale;
    return ImVec2(_pos.x + x, _pos.y - y);
}

void _gliderImporter::renderImport_Lines(Gui* mpGui, float2 _screen)
{
    const ImU32 col32 = ImColor(ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    const ImU32 col32R = ImColor(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    const ImU32 col32B = ImColor(ImVec4(0.0f, 0.3f, 1.0f, 1.0f));
    const ImU32 col32BT = ImColor(ImVec4(0.0f, 0.0f, 1.0f, 0.5f));
    const ImU32 col32G = ImColor(ImVec4(0.0f, 0.6f, 0.0f, 1.0f));
    const ImU32 col32GR = ImColor(ImVec4(0.4f, 0.4f, 0.4f, 0.2f));
    const ImU32 col32YEL = ImColor(ImVec4(0.4f, 0.4f, 0.0f, 1.0f));
    const ImU32 col32W = ImColor(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));


    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.01f, 0.003f, 0.005f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 2));
    ImGui::BeginChildFrame(11977865, ImVec2(_screen.x, _screen.y));
    ImGui::PushFont(mpGui->getFont("roboto_26"));
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        if (ImGui::Button("importLineSet")) {
            //cleanupLineSet();
            importCSV();
        }
        ImGui::Text("%d verts", line_verts.size());
        ImGui::Text("%d segments", line_segments.size());
        ImGui::NewLine();
        if (ImGui::Button("load")) {
            loadLineSet();
        }


        if (line_segments.size() > 0)
        {
            if (ImGui::Button("save")) {
                saveLineSet();
            }

            ImGui::NewLine();
            ImGui::SetNextItemWidth(300);
            static int SEG = 0;
            ImGui::DragInt("segment", &SEG, 0.1f, 0, line_segments.size() - 1);

            ImGui::SetNextItemWidth(300);
            ImGui::InputText("name", &line_segments[SEG].name[0], 8);
            ImGui::NewLine();

            ImGui::SetNextItemWidth(300);
            ImGui::DragInt("type", &line_segments[SEG].material, 0.1f, 0, lineTypes.size() - 1);
            int mat = line_segments[SEG].material;
            ImGui::Text(" %s, % 2.2f mm, % 2.2f g", lineTypes[mat].name.c_str(), lineTypes[mat].width * 1000.f, lineTypes[mat].weight * 1000.f);




            ImGui::NewLine();
            static float scale = 200;
            static ImVec2 offset = { 1600, 1000 };
            static float YAW = 0;

            ImGui::SetNextItemWidth(300);
            ImGui::DragFloat("scale", &scale, 5.f, 100, 5000);

            ImGui::SetNextItemWidth(300);
            ImGui::DragFloat("rot", &YAW, 0.01f, 0, 6);

            offset.x = _screen.x / 2.f;
            offset.y = _screen.y / 2.f;
            float3 R = float3(sin(YAW), cos(YAW), 0);
            float3 start = (line_verts[line_segments[SEG].v_0] + line_verts[line_segments[SEG].v_1]) / 2.f;



            for (int i = 0; i < line_verts.size(); i++)
            {
                ImVec2 a = project(line_verts[i], start, R, offset, scale);
                draw_list->AddCircle(a, 3.f, ImColor(ImVec4(0.3f, 0.3f, 0.3f, 0.5f)));
            }

            for (int i = 0; i < line_segments.size(); i++)
            {
                auto& L = line_segments[i];
                ImVec2 a = project(line_verts[L.v_0], start, R, offset, scale);
                ImVec2 b = project(line_verts[L.v_1], start, R, offset, scale);
                draw_list->AddLine(a, b, col32G, 1);
                float W = 2;
                if (i == SEG)
                {
                    W = 15;
                }
                else
                {
                    //draw_list->AddLine(a, b, ImColor(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)), W);

                    switch (L.material)
                    {
                    case 0: draw_list->AddLine(a, b, ImColor(ImVec4(0.0f, 0.0f, 0.1f, 1.0f)), W);     break;
                    case 1: draw_list->AddLine(a, b, ImColor(ImVec4(0.0f, 0.0f, 0.3f, 1.0f)), W);     break;
                    case 2: draw_list->AddLine(a, b, ImColor(ImVec4(0.0f, 0.0f, 0.7f, 1.0f)), W);     break;
                    case 3: draw_list->AddLine(a, b, ImColor(ImVec4(0.0f, 0.3f, 0.3f, 1.0f)), W);     break;
                    case 4: draw_list->AddLine(a, b, ImColor(ImVec4(0.0f, 0.7f, 0.3f, 1.0f)), W);     break;
                    case 5: draw_list->AddLine(a, b, ImColor(ImVec4(0.0f, 0.7f, 0.0f, 1.0f)), W);     break;
                    case 6: draw_list->AddLine(a, b, ImColor(ImVec4(0.1f, 0.1f, 0.0f, 1.0f)), W);     break;
                    case 7: draw_list->AddLine(a, b, ImColor(ImVec4(0.3f, 0.3f, 0.0f, 1.0f)), W);     break;
                    case 8: draw_list->AddLine(a, b, ImColor(ImVec4(0.7f, 0.3f, 0.0f, 1.0f)), W);     break;
                    case 9: draw_list->AddLine(a, b, ImColor(ImVec4(0.7f, 0.0f, 0.0f, 1.0f)), W);     break;
                    case 10: draw_list->AddLine(a, b, ImColor(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)), W);     break;
                    }
                }

                ImGui::SetCursorPos(project((line_verts[L.v_0] + line_verts[L.v_1]) * 0.5f, start, R, offset, scale));
                ImGui::Text("%s", line_segments[i].name);

            }
        }
    }
    ImGui::PopFont();
    ImGui::EndChildFrame();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

}
















ImVec2 projPos = { 350, 300 };
ImVec2 projPos_B = { 350, 800 };
float projScale = 650;
ImVec2 project(float3 p, _rib& R)
{
    float x = glm::dot(R.front, p - R.trailEdge) * projScale;
    float y = glm::dot(R.up, p - R.trailEdge) * projScale;
    return ImVec2(projPos.x + x, projPos.y - y);
};

ImVec2 project(int i, _rib& R)
{
    i = (i + R.lower.size()) % R.lower.size();
    float x = glm::dot(R.front, R.lower[i] - R.trailEdge) * projScale;
    float y = glm::dot(R.up, R.lower[i] - R.trailEdge) * projScale;
    return ImVec2(projPos.x + x, projPos.y - y);
};


ImVec2 project_B(float3 p, _rib& R)
{
    float x = glm::dot(R.front, p - R.trailEdge) * projScale;
    float y = glm::dot(R.up, p - R.trailEdge) * projScale;
    return ImVec2(projPos_B.x + x, projPos_B.y - y);
};

void _gliderRuntime::renderImport(Gui* pGui, float2 _screen)
{
    const ImU32 col32 = ImColor(ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    const ImU32 col32R = ImColor(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    const ImU32 col32B = ImColor(ImVec4(0.0f, 0.0f, 1.0f, 1.0f));
    const ImU32 col32GR = ImColor(ImVec4(0.4f, 0.4f, 0.4f, 0.2f));
    const ImU32 col32YEL = ImColor(ImVec4(0.4f, 0.4f, 0.0f, 1.0f));
    const ImU32 col32Y = ImColor(ImVec4(0.8f, 0.8f, 0.0f, 1.0f));
    const ImU32 col32W = ImColor(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

    const ImU32 col32Bh = ImColor(ImVec4(0.0f, 0.0f, 0.7f, 0.3f));
    const ImU32 col32Gh = ImColor(ImVec4(0.0f, 0.5f, 0.0f, 0.3f));

    ImVec2 start = { 100.f, 100.f };
    ImVec2 end = { 200.f, 200.f };

    ImVec2 chord[100];  // just big enough
    ImVec2 p[100];  // just big enough

    static int viewChord = 20;

    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.75f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 2));
    ImGui::BeginChildFrame(165, ImVec2(_screen.x, _screen.y));
    ImGui::PushFont(pGui->getFont("roboto_20"));
    {
        ImGui::NewLine();
        ImGui::Text("import");
        ImDrawList* draw_list = ImGui::GetWindowDrawList();


        if (ImGui::Button("close"))
        {
            importGui = false;
        }

        ImGui::NewLine();
        ImGui::Checkbox("reverse ribs", &importer.reverseRibs);
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("spacing", &importer.ribSpacing, 0.01f, 0.05f, 0.4f, "%2.2fm");
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("trailingBias", &importer.trailingBias, 0.1f, 1.f, 5.f);
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("leadBias", &importer.leadBias, 0.1f, 1.f, 5.f);

        ImGui::SetNextItemWidth(200);
        ImGui::DragInt("numRibScale", &importer.numRibScale, 0.1f, 2, 8);
        TOOLTIP("2, 4, or 8 \nThe number of times the cells gets subdivided")
            ImGui::SetNextItemWidth(200);
        ImGui::DragInt("numMiniRibs", &importer.numMiniRibs, 0.1f, 0, 50);
        TOOLTIP("In teh half rib position its the number \nof full res vertoicies that will connect to form a mini trailing edge rib")


            ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("lineSpacing", &importer.lineSpacing, 0.1f, 0.01f, 0.3f);




        ImGui::SetNextItemWidth(400);
        if (ImGui::Button((importer.rib_file + "##1").c_str(), ImVec2(20, 0)))
        {
        }
        TOOLTIP(importer.rib_file.c_str());
        ImGui::SameLine();
        if (ImGui::Button("process..."))
        {
            importer.processRib();
            importer.placeRibVerts();
            importer.ExportLineShape();
            importer.fullWing_to_obj();                 // FIXME move sdave to OBJ out of this I think
            //importer.processLineAttachmentPoints();
            importer.processLineAttachmentPoints_LineSet();
            importer.calculateNormals();    // since it now does the lines as well
            importer.sanityChecks();
            importer.exportWing();


        }

        if (ImGui::Button((importer.line_file + "##2").c_str(), ImVec2(20, 0)))
        {
        }

        if (ImGui::Button((importer.diagonals_file + "##3").c_str(), ImVec2(200, 0)))
        {
        }

        uint numR = 0;
        uint totalRibVerts = 0;
        ImGui::NewLine();
        ImGui::Text("  cord  circ  verts");
        for (auto& R : importer.half_ribs)
        {
            ImGui::Text("%2d:  %2.2fm, %2.2fm  %d", numR, R.chord, R.circumference, R.ribVerts.size());
            numR++;
            totalRibVerts += R.ribVerts.size();
        }
        ImGui::Text("totalRibVerts - %d", totalRibVerts);





        ImGui::SetCursorPos(ImVec2(_screen.x - 850, 50));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.75f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 2));
        ImGui::BeginChildFrame(1273994, ImVec2(800, 600));
        {
            if (importer.half_ribs.size() > 0)
            {
                float shortL = 0;
                float longL = 0;
                for (auto& C : importer.constraints)
                {
                    if (C.type == constraintType::lines) longL += glm::length(importer.verts[C.v_0]._x - importer.verts[C.v_1]._x);
                    if (C.type == constraintType::smallLines) shortL += glm::length(importer.verts[C.v_0]._x - importer.verts[C.v_1]._x);
                }
                ImGui::Text("Lines");
                ImGui::Text("# attachents - %d", importer.numLineVertsOnSurface);
                ImGui::Text("length %2.2fm, weight %2.2fkg, small (%2.2fm, %2.2fkg)", importer.totalLineMeters, importer.totalLineWeight, importer.totalLineLength_small, importer.totalLineWeight_small);
                ImGui::Text("length C %2.2fm, small (%2.2fm)", longL, shortL);
                ImGui::Text("total surface %2.2fm^2, weight %2.2fkg, tris - %d,  span %2.2fm", importer.totalWingSurface, importer.totalWingWeight, importer.totalSurfaceTriangles, importer.trailsSpan);
                ImGui::NewLine();
                ImGui::Text("carabiners  L %d, R %d", importer.carabinerLeft, importer.carabinerRight);
                ImGui::Text("brake lines L %d, R %d", importer.brakeLeft, importer.brakeRight);

                ImGui::NewLine();
                ImGui::Text("#verts skin %d, diagonal - %d, endcap - %d, line %d, smallLines - %d, total %d", importer.numSkinVerts, importer.numDiagonalVerts, importer.numEndcapVerts, importer.numLineVerts, importer.numSmallLineVerts, (int)importer.verts.size());
                if (importer.numDuplicateVerts > 0)
                {
                    ImGui::Text("ERROR - %d duplicate verts", importer.numDuplicateVerts);
                }

                ImGui::NewLine();
                ImGui::Text("total constraints %d", (int)importer.constraints.size());
                float Y = ImGui::GetCursorPosY();
                ImGui::SetCursorPos(ImVec2(100, Y));       ImGui::Text("skin");
                ImGui::SetCursorPos(ImVec2(170, Y));       ImGui::Text("ribs");
                ImGui::SetCursorPos(ImVec2(240, Y));       ImGui::Text("s_rib");
                ImGui::SetCursorPos(ImVec2(310, Y));       ImGui::Text("vecs");
                ImGui::SetCursorPos(ImVec2(380, Y));       ImGui::Text("diag");
                ImGui::SetCursorPos(ImVec2(450, Y));       ImGui::Text("stiff");
                ImGui::SetCursorPos(ImVec2(520, Y));       ImGui::Text("lines");
                ImGui::SetCursorPos(ImVec2(590, Y));       ImGui::Text("straps");
                ImGui::SetCursorPos(ImVec2(660, Y));       ImGui::Text("hrns");
                ImGui::SetCursorPos(ImVec2(730, Y));       ImGui::Text("small-L");
                //ImGui::Text("TOTAL : skin, ribs, semiribs, vecs, diagonals, stiffners, lines, straps, harnass, smallLines");

                ImGui::NewLine();   Y = ImGui::GetCursorPosY();
                int col = 0;
                ImGui::Text("# cnst");
                for (auto& C : importer.numConstraints)
                {
                    ImGui::SetCursorPos(ImVec2(100 + 70.f * col, Y));
                    ImGui::Text("%d", C);
                    col++;
                }
                Y = ImGui::GetCursorPosY();

                ImGui::Text("# dupl");
                col = 0;
                for (auto& C : importer.numDuplicateConstraints)
                {
                    ImGui::SetCursorPos(ImVec2(100 + 70.f * col, Y));
                    ImGui::Text("%d", C);
                    col++;
                }

                Y = ImGui::GetCursorPosY();
                col = 0;
                ImGui::Text("#1c");
                for (auto& C : importer.num1C_perType)
                {
                    ImGui::SetCursorPos(ImVec2(100 + 70.f * col, Y));
                    ImGui::Text("%d", C);
                    col++;
                }
                ImGui::NewLine();


                ImGui::Text("vertex constraints");
                for (int i = 0; i < 20; i++)
                {
                    ImGui::Text("%d,   ", importer.numVconstraints[i]);
                    ImGui::SameLine();
                }
            }
        }
        ImGui::EndChildFrame();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();


        ImGui::NewLine();



        ImGui::NewLine();
        ImGui::NewLine();



        if (importer.half_ribs.size() > 0)
        {
            static int viewChord = 0;
            auto& R = importer.half_ribs[viewChord];




            ImGui::PushFont(pGui->getFont("roboto_48"));
            {
                ImGui::SetCursorPos(ImVec2(600, 20));
                ImGui::SetNextItemWidth(200);
                ImGui::DragInt("rib #", &viewChord, 0.1f, 0, importer.half_ribs.size() - 1);
            }
            ImGui::PopFont();

            uint drawSize = 0;
            ImVec2 chord[10000];  // just big enough
            for (drawSize = 0; drawSize < R.lower.size(); drawSize++)
            {
                chord[drawSize] = project(drawSize, R);
            }
            draw_list->AddPolyline(chord, drawSize, col32R, false, 2);


            for (int j = 0; j < R.ribVerts.size(); j++)
            {
                draw_list->AddCircle(project(R.ribVerts[j], R), 5, col32R);

                if (j < R.ribVerts.size() / 2)
                {
                    draw_list->AddLine(project(R.ribVerts[j], R), project(R.ribVerts[R.ribVerts.size() - 1 - j], R), col32R);
                }
            }

            for (int j = 0; j < R.ribLineAttach.size(); j++)    // Where the lines atatch
            {
                ImVec2 p = project(R.ribLineAttach[j], R);
                draw_list->AddCircle(p, 9, col32B, 12, 3);
                ImGui::SetCursorPos(ImVec2(p.x + 5, p.y + 5));
                ImGui::Text("%d", R.ribLineAttach[j]);
            }


            // Diagonals
            int numDiag = 0;
            ImGui::SetCursorPos(ImVec2(600, 400 + (float)numDiag * 30));
            ImGui::Text("Diagonals");
            numDiag++;

            for (auto& diag : importer.diagonals)
            {
                if (diag.from_rib == viewChord)
                {
                    draw_list->AddTriangleFilled(project(diag.from_idx, R), project(diag.range_from, R), project(diag.range_to, R), col32Gh);

                    int size = (int)R.lower.size() - 1;
                    ImGui::PushID(123456 + numDiag);
                    {
                        ImGui::SetCursorPos(ImVec2(600, 400 + (float)numDiag * 30));

                        ImGui::SetNextItemWidth(100);
                        ImGui::DragInt("start", &diag.from_idx, 0.1f, 0, size);
                        ImGui::SameLine(0, 20);

                        ImGui::SetNextItemWidth(100);
                        ImGui::DragInt("rib#", &diag.to_rib, 0.1f, 0, size);
                        ImGui::SameLine(0, 20);

                        ImGui::SetNextItemWidth(200);
                        ImGui::DragInt2("range", &diag.range_from, 0.1f, -size, size);
                        ImGui::SameLine(0, 20);
                    }
                    ImGui::PopID();
                    numDiag++;
                }
            }


            for (auto& vec : importer.VECS)
            {
                for (int j = 0; j < vec.idx.size(); j++)
                {
                    if (vec.from + j == viewChord)
                    {
                        if (vec.isVec)
                        {
                            draw_list->AddCircle(project(vec.idx[j], R), 5, col32Y, 12, 3);
                        }
                        else
                        {
                            draw_list->AddCircle(project(vec.idx[j], R), 5, col32W, 12, 3);
                        }
                    }
                }
            }


            // Vecs
            /*
            int numVecs = 0;

            auto& vecs = importer.forceRibVerts[viewChord];
            for (auto& V : vecs)
            {
                draw_list->AddCircle(project(V, R), 4, col32Y, 12, 3);
            }*/
        }




        if (importer.full_ribs.size() > 0)
        {
            static int viewFullChord = 0;
            auto& R = importer.full_ribs[viewFullChord];


            ImGui::PushFont(pGui->getFont("roboto_48"));
            {
                ImGui::SetCursorPos(ImVec2(600, 520));
                ImGui::SetNextItemWidth(200);
                ImGui::DragInt("fullrib #", &viewFullChord, 0.1f, 0, importer.full_ribs.size() - 1);
            }
            ImGui::PopFont();

            int start = R.v_start;
            int numV = R.ribVerts.size();
            if (viewFullChord < importer.full_ribs.size() - 2)
            {
                numV = importer.full_ribs[viewFullChord + 1].v_start - start;
            }

            for (int i = start; i < start + numV; i++)
            {
                ImVec2 p = project_B(importer.verts[i]._x, R);
                float3 N = glm::normalize(importer.verts[i].temp_norm);
                ImVec2 pn = project_B(importer.verts[i]._x + N * 0.1f, R);

                draw_list->AddLine(p, pn, ImColor(ImVec4(0.0f, 0.0f, 0.3f, 1.0f)), 2);
                // now for computed normal
                uint4 idx = importer.verts[i].cross_idx;
                float3 a = importer.verts[idx.y]._x - importer.verts[idx.x]._x;
                float3 b = importer.verts[idx.w]._x - importer.verts[idx.z]._x;
                float3 Ncmp = glm::normalize(glm::cross(a, b));
                ImVec2 pncmpt = project_B(importer.verts[i]._x + Ncmp * 0.1f, R);
                draw_list->AddLine(p, pncmpt, ImColor(ImVec4(0.0f, 0.3f, 0.0f, 1.0f)), 2);

                int numC = 0;
                int numC_skin = 0;
                //int numCType[maxCTypes];
                for (auto& c : importer.constraints)
                {
                    if (c.v_0 == i || c.v_1 == i)
                    {
                        numC++;
                        if (c.type == constraintType::skin) numC_skin++;
                        ImVec2 c1 = project_B(importer.verts[c.v_0]._x, R);
                        ImVec2 c2 = project_B(importer.verts[c.v_1]._x, R);

                        ImU32 clr = ImColor(ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                        switch (c.type)
                        {
                        case constraintType::ribs:
                        case constraintType::lines:
                        case constraintType::half_ribs: clr = ImColor(ImVec4(0.7f, 0.0f, 0.0f, 1.0f)); break;
                        case constraintType::diagonals: clr = ImColor(ImVec4(0.0f, 0.6f, 0.0f, 1.0f)); break;
                        case constraintType::nitinol: clr = ImColor(ImVec4(1.0f, 0.0f, 1.0f, 1.0f)); break;
                        }
                        draw_list->AddLine(c1, c2, clr, 1);
                    }


                }
                ImGui::SetCursorPos(pn);
                ImGui::Text("%d [%d]", numC, numC_skin);

                draw_list->AddCircle(p, 5, col32Y, 12, 3);
            }


            //SIT NOU DIE extra inligiting hier in
            //R.calcIndices();
            float3 A = glm::lerp(importer.verts[R.R_index.x]._x, importer.verts[R.R_index.y]._x, R.aLerp);
            float3 B = glm::lerp(importer.verts[R.R_index.z]._x, importer.verts[R.R_index.w]._x, R.bLerp);

            float3 C = glm::lerp(A, B, R.chordLerp);
            draw_list->AddLine(project_B(importer.verts[R.R_index.x]._x, R), project_B(importer.verts[R.R_index.y]._x, R), col32R, 3);
            draw_list->AddLine(project_B(importer.verts[R.R_index.z]._x, R), project_B(importer.verts[R.R_index.w]._x, R), col32R, 3);

            draw_list->AddLine(project_B(R.trailEdge, R), project_B(R.trailEdge + R.up, R), col32R, 3);
            draw_list->AddLine(project_B(R.trailEdge, R), project_B(R.trailEdge + R.front, R), col32R, 3);

            draw_list->AddLine(project_B(A, R), project_B(B, R), col32R, 3);
            draw_list->AddCircle(project_B(C, R), 9, col32B, 12, 5);
            ImGui::SetCursorPos(ImVec2(800, 800));
            ImGui::Text("%d, %d, %d, %d", R.R_index.x, R.R_index.y, R.R_index.z, R.R_index.w);
            ImGui::SetCursorPos(ImVec2(800, 820));
            ImGui::Text("%2.2f, %2.2f, %2.2f", R.aLerp, R.bLerp, R.chordLerp);


        }



        //ImGui::Text("numLineVerts %d", importer.line_verts.size());
        //ImGui::Text("numberOfFullRibs %d", importer.full_ribs.size());



    }

    ImGui::EndChildFrame();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopFont();
}









void _gliderRuntime::renderDebug(Gui* pGui, float2 _screen)
{
    if (cleanLineSet)
    {
        importer.renderImport_Lines(pGui, _screen);
        return;
    }
    else if (importGui)
    {
        renderImport(pGui, _screen);
        return;
    }

    const ImU32 col32 = ImColor(ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    const ImU32 col32R = ImColor(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    const ImU32 col32B = ImColor(ImVec4(0.0f, 0.0f, 1.0f, 1.0f));
    const ImU32 col32GR = ImColor(ImVec4(0.4f, 0.4f, 0.4f, 0.2f));
    const ImU32 col32YEL = ImColor(ImVec4(0.4f, 0.4f, 0.0f, 1.0f));
    const ImU32 col32W = ImColor(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

    ImVec2 start = { 100.f, 100.f };
    ImVec2 end = { 200.f, 200.f };

    ImVec2 chord[100];  // just big enough
    ImVec2 p[100];  // just big enough

    static int viewChord = 20;

    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.75f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 2));
    ImGui::BeginChildFrame(119865, ImVec2(800, _screen.y));
    ImGui::PushFont(pGui->getFont("roboto_20"));
    {
        ImGui::NewLine();
        ImGui::Text("wing gui");
        ImDrawList* draw_list = ImGui::GetWindowDrawList();


        if (ImGui::Button("import"))
        {
            importGui = true;
        }

        ImGui::DragInt("rib", &viewChord, 0.1f, 0, spanSize - 1);

        // AddPolyline
        // AddConvexPolyFilled
        // AddBezierCurve

        int base = viewChord * chordSize;
        float3 c_base = (x[base + 6] + x[base + 10] + x[base + 14] + x[base + 18]) * 0.25f;
        float3 f = cells[viewChord].front;
        float3 u = cells[viewChord].up;
        for (int i = 0; i < chordSize; i++)
        {
            float3 c = x[base + i] - c_base;
            chord[i] = ImVec2(280 - glm::dot(c, f) * 200, 300 - glm::dot(c, u) * 200);

            float cp = Pressure.get(cells[viewChord].AoBrake, cells[viewChord].AoA, i);
            float3 c_n = c - glm::normalize(n[base + i]) * 0.2f * cp;
            p[i] = ImVec2(700 - i * 50.f, 700 + cp * 50);
            if (i > chordSize / 2)
            {
                p[i].x = 700 - (chordSize - 1 - i) * 50.f;
            }

            p[i].x = chord[i].x;

            ImVec2 pres = ImVec2(280 - glm::dot(c_n, f) * 200, 300 - glm::dot(c_n, u) * 200);
            draw_list->AddLine(chord[i], pres, col32YEL, 2);

        }
        draw_list->AddPolyline(chord, chordSize, col32R, false, 2);
        draw_list->AddPolyline(p, chordSize, col32GR, false, 2);


        // Do the inside constraints
        for (auto& c : constraints)
        {
            if (c.idx_0 > base && c.idx_0 < base + chordSize && c.idx_1 > base && c.idx_1 < base + chordSize)
            {
                float3 c1 = x[c.idx_0] - c_base;
                float3 c2 = x[c.idx_1] - c_base;
                ImVec2 a = ImVec2(280 - glm::dot(c1, f) * 200, 300 - glm::dot(c1, u) * 200);
                ImVec2 b = ImVec2(280 - glm::dot(c2, f) * 200, 300 - glm::dot(c2, u) * 200);

                ImU32 col;
                float l = glm::length(x[c.idx_0] - x[c.idx_1]);
                if (c.l < l)
                {
                    float dl = __min(1, c.tensileStiff * (l - c.l) / c.l * 100.f);
                    col = ImColor(ImVec4(dl, 0.0f, 0.1f, 1.0f));
                }
                else
                {
                    float dl = __min(1, c.compressStiff * (c.l - l) / c.l * 100.f);
                    col = ImColor(ImVec4(0.0f, dl, 0.1f, 1.0f));
                }

                draw_list->AddLine(a, b, col, 2);
            }
        }



        ImGui::SetCursorPos(ImVec2(220, 280));
        ImGui::Text("p %2.2f", cells[viewChord].inlet_pressure);

        ImGui::SetCursorPos(ImVec2(320, 280));
        ImGui::Text("p %2.2f", cells[viewChord].pressure);

        float3 flow = cells[viewChord].flow_direction;
        start = { 400.f, 100.f };
        end = ImVec2(400 - glm::dot(flow, f) * 200, 100 - glm::dot(flow, u) * 200);
        draw_list->AddLine(start, end, col32B, 2);
        ImGui::SetCursorPos(ImVec2(420, 120));
        ImGui::Text("AoA %2.1fº", cells[viewChord].AoA * 57.f);

        ImGui::SetCursorPos(ImVec2(420, 180));
        float _brake = glm::clamp(cells[viewChord].AoBrake * 5.f, 0.f, 4.99f);
        ImGui::Text("AoB %2.1fº (%2.1f)", cells[viewChord].AoBrake * 57, _brake);


        ImGui::Text("num Constraints %d", constraints.size());

        if (ImGui::Button("re-start"))
        {
            requestRestart = true;
        }

    }

    ImGui::EndChildFrame();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopFont();
}




void _gliderRuntime::renderHUD(Gui* pGui, float2 _screen, bool allowMouseCamControl)
{
    uint triangle = chordSize * spanSize;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.2f, 0.2f, 0.2f, 0.1f));


    if (allowMouseCamControl)
    {
        if (ImGui::IsMouseClicked(2))
        {
            cameraType = (cameraType + 1) % 3;
        }

        ImVec2 mouseDrag = ImGui::GetMouseDragDelta(1);    // return the delta from the initial clicking position while the mouse button is pressed or was just released. This is locked and return 0.0f until the mouse moves past a distance threshold at least once. If lock_threshold < -1.0f uses io.MouseDraggingThreshold.
        ImGui::ResetMouseDragDelta(1);
        //
        cameraYaw[cameraType] -= mouseDrag.x * 0.001f;
        cameraPitch[cameraType] -= mouseDrag.y * 0.0015f;
        cameraPitch[cameraType] = glm::clamp(cameraPitch[cameraType], -1.3f, 1.3f);

        ImVec2 mouseDragLeft = ImGui::GetMouseDragDelta(0);    // return the delta from the initial clicking position while the mouse button is pressed or was just released. This is locked and return 0.0f until the mouse moves past a distance threshold at least once. If lock_threshold < -1.0f uses io.MouseDraggingThreshold.
        ImGui::ResetMouseDragDelta(0);

        cameraDistance[cameraType] += 0.1f * mouseDragLeft.y;
        cameraDistance[cameraType] = glm::clamp(cameraDistance[cameraType], 0.01f, 10.f);
    }



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

        ImGui::SetCursorPos(ImVec2(_screen.x - 500, _screen.y / 2 + 2 * dLine));
        ImGui::Text("ground %d", (int)Vgrnd);
        ImGui::SetCursorPos(ImVec2(_screen.x - 170, _screen.y / 2 + 2 * dLine));
        ImGui::Text("km/h");

        ImGui::SetCursorPos(ImVec2(_screen.x - 400, _screen.y / 2 + 3 * dLine));
        ImGui::Text("air %d", (int)Vair);
        ImGui::SetCursorPos(ImVec2(_screen.x - 170, _screen.y / 2 + 3 * dLine));
        ImGui::Text("km/h");
    }
    ImGui::PopFont();



    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::PushFont(pGui->getFont("roboto_32"));
    {
        // weight shift

        ImGui::SetCursorPos(ImVec2(_screen.x / 2 - 150, _screen.y - 150));
        ImGui::SetNextItemWidth(300);
        if (ImGui::SliderFloat("weight shift", &weightShift, 0.1f, 0.9f, "%.2f"));

        ImGui::SetCursorPos(ImVec2(_screen.x / 2 - 150, _screen.y - 100));
        ImGui::SetNextItemWidth(300);
        if (ImGui::SliderFloat("speed bar", &speedbar, 0.f, 1.f, "%.2f"));




        // bake forcves
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.f));
        {
            if (pBrakeLeft)
            {
                float leftFPix = pBrakeLeft->tencileForce * 10;
                ImGui::SetCursorPos(ImVec2(_screen.x / 2 - 400, _screen.y - 100 - leftFPix));
                ImGui::BeginChildFrame(3331, ImVec2(30, leftFPix));
                ImGui::EndChildFrame();
            }

            if (pBrakeRight)
            {
                float rightFPix = pBrakeRight->tencileForce * 10;
                ImGui::SetCursorPos(ImVec2(_screen.x / 2 + 370, _screen.y - 100 - rightFPix));
                ImGui::BeginChildFrame(3332, ImVec2(30, rightFPix));
                ImGui::EndChildFrame();
            }
        }
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.02f, 0.02f, 0.02f, 1.f));
        {
            if (pBrakeLeft)
            {
                float leftFPix = (maxBrake - pBrakeLeft->length) * 300;
                ImGui::SetCursorPos(ImVec2(_screen.x / 2 - 410, _screen.y - 400 + leftFPix));
                ImGui::BeginChildFrame(3301, ImVec2(60, 10 + rumbleLeft * 50));
                ImGui::EndChildFrame();
            }

            if (pBrakeRight)
            {
                float rightFPix = (maxBrake - pBrakeRight->length) * 300;
                ImGui::SetCursorPos(ImVec2(_screen.x / 2 + 360, _screen.y - 400 + rightFPix));
                ImGui::BeginChildFrame(3302, ImVec2(60, 10 + rumbleRight * 50));
                ImGui::EndChildFrame();
            }
        }
        ImGui::PopStyleColor();




        // warnings

        if (CPU_usage < 0.9f)   ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
        else if (CPU_usage < 1.0f)   ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.1f, 1.0f));
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.1f, 0.1f, 1.0f));

        }
        if (CPU_usage > 1.f)
        {
            ImGui::SetCursorPos(ImVec2(_screen.x - 180, 20));
            ImGui::Text("CPU  %d %%", (int)(CPU_usage * 100.f));
        }
        ImGui::PopStyleColor();

        if (controllerId == -1)
        {
            ImGui::SetCursorPos(ImVec2(_screen.x - 180, 60));
            ImGui::Text("NO XBOX");
            TOOLTIP("No XBox controller found.\nPlease plug it in or contact me\ndirectly or via Discord");
        }

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


void _gliderRuntime::saveSettings()
{
    std::ofstream os("gliderSettings.xml");
    if (os.good()) {
        cereal::XMLOutputArchive archive(os);
        archive(CEREAL_NVP(settings));
    }
}

// it gets parabuilder data
// this should load from disk
void _gliderRuntime::setup(std::vector<float3>& _x, std::vector<float>& _w, std::vector<uint4>& _cross, uint _span, uint _chord, std::vector<_constraint>& _constraints, bool useView, glm::mat4x4 view)
{
    std::ifstream is("gliderSettings.xml");
    if (is.good()) {
        cereal::XMLInputArchive archive(is);
        archive(CEREAL_NVP(settings));
    }


    x = _x;
    w = _w;

    //exportGliderShape();

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

    ROOT = float3(9000, 1800, 8000);// middlke
    //
    //ROOT = float3(15500, 1300, 14400);// windy south
    //ROOT = float3(-500, 500, 15400);// windy south landings

    //ROOT = float3(-1425, 700, 14500);// turb test


    float3 DIR = float3(0, -1.5, -7);
    if (useView)
    {
        glm::mat4x4 VIEW_INV = glm::inverse(view);
        float3 DIR = VIEW_INV[2];
        DIR.y = 0;
        //DIR *= -1;
        DIR = glm::normalize(DIR);
        float3 R = glm::cross(float3(0, 1, 0), DIR);

        //DIR = float3(-1, 0, 0);
        //R = float3(0, 0, 1);

        ROOT = VIEW_INV[3];
        fprintf(terrafectorSystem::_logfile, "_gliderRuntime::setup( %d, %d, %d)\n", (int)ROOT.x, (int)ROOT.y, (int)ROOT.z);
        fprintf(terrafectorSystem::_logfile, "direction ( %2.1f, %2.1f, %2.1f)\n", DIR.x, DIR.y, DIR.z);
        fprintf(terrafectorSystem::_logfile, "right ( %2.1f, %2.1f, %2.1f)\n", R.x, R.y, R.z);
        fflush(terrafectorSystem::_logfile);
        /*
        for (auto& pos : x)
        {
            float3 newPos = pos;
            newPos.x = glm::dot(pos, R);
            newPos.z = glm::dot(pos, DIR);

            pos = newPos;
        }

        DIR *= 8.f;
        DIR.y = -1.5f;
        */
        fprintf(terrafectorSystem::_logfile, "wind ( %2.1f, %2.1f, %2.1f)\n", DIR.x, DIR.y, DIR.z);
    }



    for (int i = 0; i < spanSize; i++)
    {
        cells[i].v = DIR;// float3(0, -1.5, -7);
        cells[i].pressure = 1;
        cells[i].chordLength = glm::length(x[index(i, 12)] - x[index(i, 0)]); // fixme
    }

    for (int i = 0; i < x.size(); i++)
    {
        x[i] += float3(0, 0, 0);
        v[i] = DIR;// float3(0, -1.5, -7);
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

    recordIndex = 0;


    std::string folder = xfoilDir + "/" + wingName + "/cp.bin";
    Pressure.load(folder);

}


void _gliderRuntime::setupLines()
{
    fprintf(terrafectorSystem::_logfile, "_gliderRuntime::setupLines()\n");
    fflush(terrafectorSystem::_logfile);
    static bool first = true;
    if (first)
    {
        pBrakeLeft = linesLeft.getLine("FF1");
        pBrakeRight = linesRight.getLine("FF1");
        pEarsLeft = linesLeft.getLine("ears");
        pEarsRight = linesRight.getLine("ears");
        pSLeft = linesLeft.getLine("S1");
        pSRight = linesRight.getLine("S1");

        pA_strap_left = linesLeft.getLine("A_strap");
        pA_strap_right = linesRight.getLine("A_strap");
        pB_strap_left = linesLeft.getLine("B_strap");
        pB_strap_right = linesRight.getLine("B_strap");


        if (!pBrakeLeft)
        {
            fprintf(terrafectorSystem::_logfile, "pBrakeLeft   NOT FOUND\n");
            fflush(terrafectorSystem::_logfile);
        }
        if (!pBrakeRight)
        {
            fprintf(terrafectorSystem::_logfile, "pBrakeRight   NOT FOUND\n");
            fflush(terrafectorSystem::_logfile);
        }

        if (pBrakeLeft)
        {
            first = false;
            if (pBrakeLeft)      maxBrake = pBrakeLeft->length;
            if (pEarsLeft)  maxEars = pEarsLeft->length;
            if (pSLeft)  maxS = pSLeft->length;
            if (pA_strap_left)  maxAStrap = pA_strap_left->length;
            if (pB_strap_left)  maxBStrap = pB_strap_left->length;



            fprintf(terrafectorSystem::_logfile, "max brake length %f\n", maxBrake);
            fflush(terrafectorSystem::_logfile);

            fprintf(terrafectorSystem::_logfile, "speedbar A %fm   B %fm\n", maxAStrap, maxBStrap);
            fflush(terrafectorSystem::_logfile);
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
        //cells[i].AoA = 0;
        //cells[i].AoBrake = 0;

        if (cells[i].cellVelocity > 0.01f)
        {
            float3 ra = x[index(i, 3)] - x[index(i, 15)];
            float3 rb = x[index(i, 9)] - x[index(i, 21)];
            cells[i].front = glm::normalize(x[index(i, 10)] - x[index(i, 3)]);              // from C forward
            cells[i].right = glm::normalize(glm::cross(ra, rb));
            cells[i].up = glm::normalize(glm::cross(cells[i].front, -cells[i].right));      // -right because of right handed system 

            float3 frontBrake = glm::normalize(x[index(i, 3)] - x[index(i, 0)]);
            float3 upBrake = glm::normalize(glm::cross(frontBrake, -cells[i].right));
            float sintheta = glm::clamp(-1.0f * glm::dot(upBrake, cells[i].front), 0.f, 1.f);
            cells[i].AoBrake = glm::lerp(cells[i].AoBrake, sintheta, 0.005f);    // quite smoothed AoBrake

            float aoa = asin(glm::dot(cells[i].up, cells[i].flow_direction));
            if (glm::dot(cells[i].flow_direction, cells[i].front) > 0) {
                aoa = 3.14159f - aoa;
            }
            cells[i].AoA = glm::lerp(cells[i].AoA, aoa + 0.0021f, 0.982f);                          // very slightly smoothed AoA
            // +0.21f compensates for line lengths
            // Can we cheat wingtips here wit AoA on;y
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
        cells[i].inlet_pressure = P;
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
        float Area = glm::length(n[i]);     //??? pre calculate
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
        //n[i] = n[i] * (-cp) * cells[span].rho * 0.6f;
        n[i] = n[i] * (cells[span].pressure - cp) * cells[span].rho * 0.5f;
        sumFwing += n[i];

        float dragIncrease = pow(2.f - cells[span].pressure, 2.f);
        //dragIncrease = glm::lerp(dragIncrease, dragIncrease * 2, 1 - cells[span].ragdoll);
        float drag = 0.0039 * cells[span].rho * 2.0f * dragIncrease;    // double it for rib turbulece
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
        else if (i < spanSize * chordSize + 6)
        {
            weightPilot += 1.f / w[i];
        }
        else if (w[i] > 0)
        {
            //weightLines += 1.f / w[i];
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
    solve_rectanglePilot(_dT);
    /*
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
    */
}


void _gliderRuntime::solve_rectanglePilot(float _dT)
{
    uint start = chordSize * spanSize;
    float airDensity = 1.11225f;    // Fixme form CFD later

    // weiwgth shift
    float mleft = (1.f - weightShift) * 90.f / 2.f;
    float mright = (weightShift) * 90.f / 2.f;
    w[start + 0] = 1.f / mleft;
    w[start + 1] = 1.f / mright;
    w[start + 2] = 1.f / mright;
    w[start + 3] = 1.f / mleft;

    // Solve the body and carabiners
    x[start + 4] += w[start + 4] * tensionCarabinerLeft * _dT * _dT;
    x[start + 5] += w[start + 5] * tensionCarabinerRight * _dT * _dT;
    // drag on the body
    vBody = glm::length(v[start] - pilotWind);
    float rho = 0.5 * airDensity * vBody * vBody;
    if (vBody > 1) {
        pilotDrag = -glm::normalize(v[start] - pilotWind) * rho * 0.6f * 0.9f;
        x[start + 0] += _dT * w[start + 0] * pilotDrag * _dT * 0.35f;
        x[start + 1] += _dT * w[start + 1] * pilotDrag * _dT * 0.35f;       // more drag at the back due to body
        x[start + 2] += _dT * w[start + 2] * pilotDrag * _dT * 0.15f;
        x[start + 3] += _dT * w[start + 3] * pilotDrag * _dT * 0.15f;
    }

    for (int k = 0; k < 20; k++)            // Now solve the first 3 constraints multiple times for weight shift and pilot gravity
    {
        for (int i = 0; i < 15; i++)
        {
            solveConstraint(i, 0.5f);
        }
    }
}


void _gliderRuntime::solve_wing(float _dT, bool _left)
{
    // solve
    float dS = 0.7f;    // also pass in
    uint numS = (constraints.size() - 15) / 2;

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
        for (int i = 15; i < numS + 15; i++)
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
        for (int i = numS + 15; i < constraints.size(); i++)
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

    static int saveCount = 50;
    saveCount--;
    if (saveCount < 0)
    {
        uint start = chordSize * spanSize;
        pathRecord[recordIndex] = ROOT + x[start];    // 30 minutes at 10 per second
        recordIndex++;
        saveCount = 50;     // fropm 2ms, this giives every 100ms or ten per second
    }
}


void _gliderRuntime::eye_andCamera()
{
    uint start = chordSize * spanSize;
    uint midWing = chordSize * spanSize / 2 + 12;
    // NOW reset all x to keep human at 0
    float3 pilotRight = glm::normalize(x[start + 0] - x[start + 1]);
    //float3 pilotback = glm::cross(x[start + 1] - x[start], x[start + 2] - x[start]);
    //float3 pilotUp = glm::normalize(glm::cross(pilotback, pilotRight));

    //pilotUp = glm::lerp(pilotUp, float3(0, 1, 0), 0.1f);
    //float3 pilotUp = glm::normalize(glm::cross(x[start + 2] - x[start], x[start + 3] - x[start + 1]));
    float3 pilotUp = glm::normalize(x[midWing] - glm::lerp(x[start + 0], x[start + 1], 0.5f));
    float3 pilotback = glm::normalize(glm::cross(pilotRight, pilotUp));
    pilotback = glm::normalize(pilotback);





    static float3 smoothUp = { 0, 1, 0 };
    static float3 smoothBack = { 0, 0, -1 };
    static float3 dragShute = ROOT;
    static float3 dragShuteBack = { 0, 0, -1 };;

    switch (cameraType)
    {
    case 0:
        smoothUp = glm::lerp(smoothUp, pilotUp, 0.2f);
        smoothBack = glm::lerp(smoothBack, pilotback, 0.04f);
        //EyeLocal = x[start] - smoothBack * 0.3f + smoothUp * 0.4f;
        EyeLocal = glm::lerp(x[start + 0], x[start + 1], 0.5f) - smoothBack * 0.3f + smoothUp * 0.7f;

        // now add rotations
    // yaw
        camDir = smoothBack;// pilotback;// glm::normalize(smoothCamDir* cos(cameraYaw) + smoothCamRight * sin(cameraYaw));
        camUp = smoothUp;// pilotUp;
        camRight = glm::cross(camUp, camDir);
        camDir = glm::normalize(camDir * cos(cameraYaw[cameraType]) + camRight * sin(cameraYaw[cameraType]));
        camRight = glm::cross(camUp, camDir);

        // pitch
        camDir = glm::normalize(camDir * cos(cameraPitch[cameraType]) + camUp * sin(cameraPitch[cameraType]));
        camUp = glm::normalize(glm::cross(camDir, camRight));
        camRight = glm::cross(camUp, camDir);

        EyeLocal -= camDir * cameraDistance[0];
        break;



    case 1:

        // // rotate up

        //smoothUp = glm::lerp(smoothUp, pilotUp, 0.95f);
        pilotback.y *= 0.5f;
        pilotback = glm::normalize(pilotback);
        smoothBack = glm::lerp(smoothBack, pilotback, 0.05f);

        //dragShute = glm::lerp(dragShute, x[start] + ROOT - glm::normalize(v[start]) * (5.f + 2.f * cameraDistance), 0.04f);
        dragShute = glm::lerp(dragShute, glm::lerp(x[start], x[midWing], 0.6f) + ROOT - smoothBack * (5.f + 2.f * cameraDistance[1]), 0.10f);
        EyeLocal = dragShute - ROOT;

        dragShuteBack = glm::lerp(dragShuteBack, smoothBack, 0.15f);
        pilotRight = glm::lerp(pilotRight, glm::cross(float3(0, 1, 0), dragShuteBack), 0.99f);
        smoothUp = glm::normalize(glm::cross(dragShuteBack, pilotRight));
        // now add rotations

    // yaw
        camDir = dragShuteBack;// pilotback;// glm::normalize(smoothCamDir* cos(cameraYaw) + smoothCamRight * sin(cameraYaw));
        camUp = smoothUp;// pilotUp;
        camRight = glm::cross(camUp, camDir);
        camDir = glm::normalize(camDir * cos(cameraYaw[cameraType]) + camRight * sin(cameraYaw[cameraType]));
        camRight = glm::cross(camUp, camDir);

        // pitch
        camDir = glm::normalize(camDir * cos(cameraPitch[cameraType]) + camUp * sin(cameraPitch[cameraType]));
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
        camDir = glm::normalize(camDir * cos(cameraYaw[cameraType]) + camRight * sin(cameraYaw[cameraType]));
        camRight = glm::cross(camUp, camDir);

        // pitch
        camDir = glm::normalize(camDir * cos(cameraPitch[cameraType]) + camUp * sin(cameraPitch[cameraType]));
        camUp = glm::normalize(glm::cross(camDir, camRight));
        camRight = glm::cross(camUp, camDir);

        EyeLocal -= camDir * cameraDistance[2] * 20.f;
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
#define Cnst constraints[_i]
#define idx0 Cnst.idx_0
#define idx1 Cnst.idx_1

    float Wsum = w[idx0] + w[idx1];
    float3 S = x[idx1] - x[idx0];
    float L = glm::length(S);
    float C = (L - Cnst.l) / L * _step;
    if (C > 0) {
        C *= Cnst.tensileStiff;
    }
    else {
        C *= __max(Cnst.compressStiff, Cnst.pressureStiff * cells[Cnst.cell].pressure * cells[Cnst.cell].pressure);
    }

    //   if (std::isnan(C))
    //   {
    //       bool bcm = true;
    //   }

    x[idx0] += S * C / Wsum * w[idx0];
    x[idx1] -= S * C / Wsum * w[idx1];
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

    // For 3 lines
    /*
    setupVert(&ribbon[ribbonCount], 0, x[triangle], 0.01f, 7);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 1], 0.01f, 7);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle], 0.01f, 7);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 2], 0.01f, 7);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle + 1], 0.01f, 7);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 2], 0.01f, 7);     ribbonCount++;
    */

    setupVert(&ribbon[ribbonCount], 0, x[triangle], 0.01f, 6);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 1], 0.01f, 6);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle + 1], 0.01f, 6);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 2], 0.01f, 6);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle + 2], 0.01f, 6);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 3], 0.01f, 6);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle + 3], 0.01f, 6);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 0], 0.01f, 6);     ribbonCount++;


    setupVert(&ribbon[ribbonCount], 0, x[triangle + 0], 0.01f, 5);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 4], 0.01f, 5);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle + 1], 0.01f, 5);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 5], 0.01f, 5);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle + 2], 0.01f, 5);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 5], 0.01f, 5);     ribbonCount++;

    setupVert(&ribbon[ribbonCount], 0, x[triangle + 3], 0.01f, 5);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 4], 0.01f, 5);     ribbonCount++;

    // chest strap
    setupVert(&ribbon[ribbonCount], 0, x[triangle + 4], 0.01f, 7);     ribbonCount++;
    setupVert(&ribbon[ribbonCount], 1, x[triangle + 5], 0.01f, 7);     ribbonCount++;



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


void _gliderRuntime::setJoystick_bandarra()
{
    fprintf(terrafectorSystem::_logfile, "setJoystick_bandarra\n");
    static bool gamePadLeft;
    XINPUT_STATE state;

    if (XInputGetState(controllerId, &state) == ERROR_SUCCESS)
    {
        if ((!gamePadLeft && state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0)
        {
            cameraType = (cameraType + 1) % 3;
        }
        gamePadLeft = state.Gamepad.wButtons & XINPUT_GAMEPAD_A;

        static SHORT normXZero = 0;
        static SHORT normYZero = 0;
        /*
        if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0)
        {
            normXZero = state.Gamepad.sThumbRX;
            normYZero = state.Gamepad.sThumbRY;
        }*/

        // weigth shift AXIS 2 maps to turmRX
        float normRX = fmaxf(-1, (float)state.Gamepad.sThumbRX / 32767);
        weightShift = glm::lerp(weightShift, normRX * 0.4f + 0.5f, 0.01f);

        /*
        float normLX = fmaxf(-1, (float)(state.Gamepad.sThumbRX - normXZero) / 32767);
        float normLY = fmaxf(-1, (float)(state.Gamepad.sThumbRY - normYZero) / 32767);
        if (abs(normLX) < 0.05f)normLX = 0;
        if (abs(normLY) < 0.05f)normLY = 0;

        if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) cameraDistance[cameraType] -= 0.01f;
        if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) cameraDistance[cameraType] += 0.01f;
        cameraDistance[cameraType] = glm::clamp(cameraDistance[cameraType], 0.01f, 10.f);

        cameraYaw[cameraType] -= normLX * 0.001f;
        cameraPitch[cameraType] -= normLY * 0.0015f;
        cameraPitch[cameraType] = glm::clamp(cameraPitch[cameraType], -1.3f, 1.3f);
        */




        if (pBrakeLeft)
        {
            float leftTrigger = (float)(state.Gamepad.sThumbLY + 32768) / 65536;
            float rightTrigger = (float)(state.Gamepad.sThumbLX + 32768) / 65536;

            if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0)
            {
                pEarsLeft->length = maxEars - leftTrigger * 1.2f;
                pEarsRight->length = maxEars - rightTrigger * 1.2f;
            }
            else if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0)
            {
                pSLeft->length = maxS - leftTrigger;
                pSRight->length = maxS - rightTrigger;
            }
            else
            {
                pBrakeLeft->length = glm::lerp(pBrakeLeft->length, maxBrake - leftTrigger * 0.7f, 0.1f);
                pBrakeRight->length = glm::lerp(pBrakeRight->length, maxBrake - rightTrigger * 0.7f, 0.1f);
            }


            pA_strap_left->length = maxAStrap - speedbar * maxAStrap * 0.9f;
            pA_strap_right->length = maxAStrap - speedbar * maxAStrap * 0.9f;
            pB_strap_left->length = maxBStrap - speedbar * maxBStrap * 0.9f;
            pB_strap_right->length = maxBStrap - speedbar * maxBStrap * 0.9f;


            if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_B) == 0)       // these help release slowly
            {
                pEarsLeft->length = glm::lerp(pEarsLeft->length, maxEars, 0.01f);
                pEarsRight->length = glm::lerp(pEarsRight->length, maxEars, 0.01f);
            }

            if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_X) == 0)
            {
                pSLeft->length = glm::lerp(pSLeft->length, maxS, 0.01f);
                pSRight->length = glm::lerp(pSRight->length, maxS, 0.01f);
            }
        }
        else
        {
            setupLines();
        }
    }
}

//#include <GLFW/glfw3.h>

void _gliderRuntime::setJoystick()
{

    static bool gamePadLeft;

    if (controllerId == -1)
    {
        //int present = glfwJoystickPresent(GLFW_JOYSTICK_1);
        //fprintf(terrafectorSystem::_logfile, "searching setJoystick_bandarra\n");
        for (DWORD i = 0; i < XUSER_MAX_COUNT && controllerId == -1; i++)
        {
            _XINPUT_CAPABILITIES ic;
            XInputGetCapabilities(i, 0, &ic);
            //ic.

            XINPUT_STATE state;
            ZeroMemory(&state, sizeof(XINPUT_STATE));

            if (XInputGetState(i, &state) == ERROR_SUCCESS)
            {
                controllerId = i;
                //fprintf(terrafectorSystem::_logfile, "foubnd setJoystick_bandarra - %d\n", i);
            }


        }
    }
    else
    {
        if (settings.controller == controllerType::bandarra)
        {
            setJoystick_bandarra();
            return;
        }


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
            // FIXME do this better with 
            /*
            if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0)
            {
                normXZero = state.Gamepad.sThumbRX;
                normYZero = state.Gamepad.sThumbRY;
            }
            */




            float normLX = fmaxf(-1, (float)(state.Gamepad.sThumbRX - normXZero) / 32767);
            float normLY = fmaxf(-1, (float)(state.Gamepad.sThumbRY - normYZero) / 32767);
            //cameraDistance -= ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0) * 0.2001f;
            //cameraDistance += ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0) * 0.2001f;
            if (abs(normLX) < 0.05f)normLX = 0;
            if (abs(normLY) < 0.05f)normLY = 0;

            if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) cameraDistance[cameraType] -= 0.01f;
            if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) cameraDistance[cameraType] += 0.01f;
            cameraDistance[cameraType] = glm::clamp(cameraDistance[cameraType], 0.01f, 10.f);

            cameraYaw[cameraType] -= normLX * 0.001f;
            cameraPitch[cameraType] -= normLY * 0.0015f;
            cameraPitch[cameraType] = glm::clamp(cameraPitch[cameraType], -1.3f, 1.3f);

            //if (runcount == 0)  return;


            float normRX = fmaxf(-1, (float)state.Gamepad.sThumbLX / 32767);
            weightShift = glm::lerp(weightShift, normRX * 0.4f + 0.5f, 0.01f);

            float normRY = fmaxf(-1, (float)state.Gamepad.sThumbLY / 32767);
            if (!speedBarLock)
            {
                speedbar = glm::lerp(speedbar, normRY, 0.01f);
            }

            static bool Y_DEBOUNCE = false;
            if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0)
            {
                if (Y_DEBOUNCE) {
                    speedBarLock = !speedBarLock;
                }
                Y_DEBOUNCE = false;
            }
            else
            {
                Y_DEBOUNCE = true;
            }
            /*
            * // triangle version
            constraints[1].l = weightShift * weightLength;
            constraints[2].l = (1.0f - weightShift) * weightLength;
            */
            // rectangle version
            //uint start = chordSize * spanSize;



            if (pBrakeLeft)
            {

                leftTrigger = state.Gamepad.bLeftTrigger;
                rightTrigger = state.Gamepad.bRightTrigger;
                float leftTrigger = pow((float)state.Gamepad.bLeftTrigger / 255, 1.0f);
                float rightTrigger = pow((float)state.Gamepad.bRightTrigger / 255, 1.0f);

                if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0)
                {
                    pEarsLeft->length = maxEars - leftTrigger * 1.0f;
                    pEarsRight->length = maxEars - rightTrigger * 1.0f;
                }
                else if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0)
                {
                    pSLeft->length = maxS - leftTrigger;
                    pSRight->length = maxS - rightTrigger;
                }
                else
                {
                    //pBrakeLeft->length = glm::lerp(pBrakeLeft->length, maxBrake - leftTrigger * 0.7f, 0.1f);
                    //pBrakeRight->length = glm::lerp(pBrakeRight->length, maxBrake - rightTrigger * 0.7f, 0.1f);
                    pBrakeLeft->length = maxBrake - leftTrigger * 0.7f;
                    pBrakeRight->length = maxBrake - rightTrigger * 0.7f;
                }


                pA_strap_left->length = maxAStrap - speedbar * maxAStrap * 0.6f;
                pA_strap_right->length = maxAStrap - speedbar * maxAStrap * 0.6f;
                pB_strap_left->length = maxBStrap - speedbar * maxBStrap * 0.3f;
                pB_strap_right->length = maxBStrap - speedbar * maxBStrap * 0.3f;


                if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_A) == 0)
                {
                    pEarsLeft->length = glm::lerp(pEarsLeft->length, maxEars, 0.1f);
                    pEarsRight->length = glm::lerp(pEarsRight->length, maxEars, 0.1f);
                }

                if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_B) == 0)
                {
                    pSLeft->length = glm::lerp(pSLeft->length, maxS, 0.1f);
                    pSRight->length = glm::lerp(pSRight->length, maxS, 0.1f);
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



//#pragma optimize("", off)



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
    else
    {
        fprintf(terrafectorSystem::_logfile, "_windTerrain::load(%s)    - failed to load\n", filename.c_str());
        fflush(terrafectorSystem::_logfile);
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



void _windTerrain::loadRaw(std::string _path)
{
    std::ifstream ifs;
    /*
    ifs.open("E:/wind.bin", std::ios::binary);
    if (ifs)
    {
        ifs.read((char*)wind, 4096 * 4096 * sizeof(float3));
        ifs.read((char*)windTop, 4096 * 4096 * sizeof(float3));
        ifs.close();
    }
    */

    ifs.open(_path + "height.bin", std::ios::binary);
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
    int2 idx = glm::clamp(uv, 0.f, 4095.f);
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



























void _new_gliderRuntime::solve(float _dT, int _repeats)
{
    auto a = high_resolution_clock::now();
    //solve_airFlow();
    auto b = high_resolution_clock::now();
    //solve_wingShape();
    auto c = high_resolution_clock::now();


    if (_repeats >= 0)
    {
        //solve_internalPressure();
        auto d = high_resolution_clock::now();

        solve_pre();
        auto e = high_resolution_clock::now();

        solve_intersection();
        auto f = high_resolution_clock::now();

        for (int i = 0; i < _repeats; i++) {
            solve_constraints();
        }

        

        /*
        for (auto& C : constraints)
        {
            int3 vint = x[C.idx_1] - x[C.idx_0];
            float3 vec = { vint.x * _inv, vint.y * _inv , vint.z * _inv };
            C.old_L = C.current_L;
            C.current_L = glm::length(vec);
        }
        for (auto& C : line_constraints)
        {
            int3 vint = x[C.idx_1] - x[C.idx_0];
            float3 vec = { vint.x * _inv, vint.y * _inv , vint.z * _inv };
            C.old_L = C.current_L;
            C.current_L = glm::length(vec);
        }
        */
        auto g = high_resolution_clock::now();


        time_internal_us = (float)duration_cast<microseconds>(d - c).count();
        time_pre_us = (float)duration_cast<microseconds>(e - d).count();
        time_intersect_us = (float)duration_cast<microseconds>(f - e).count();
        time_constraints_us = (float)duration_cast<microseconds>(g - f).count();
        time_total_us = (float)duration_cast<microseconds>(e - a).count();
    }



    time_air_us = (float)duration_cast<microseconds>(b - a).count();
    time_wing_us = (float)duration_cast<microseconds>(c - b).count();


    // root
    //int3 vC = x[carabinerRight];
    //int3 vT = x[2000];              // FIXME also save this during export

    // root for Cube
    int3 vC = x[0];
    int3 vT = x[x.size() - 1];              // FIXME also save this during export

    float3 fC = { vC.x * _inv, vC.y * _inv , vC.z * _inv };
    float3 fT = { vT.x * _inv, vT.y * _inv , vT.z * _inv };
    float3 fRoot = (fC + fT) * 0.5f;
    float3 offset = fRoot - ROOT;
    int3 iOff = offset;
    ROOT += iOff;
    iOff.x *= (1 << _dot);
    iOff.y *= (1 << _dot);
    iOff.z *= (1 << _dot);
    for (auto& _x : x)
    {
        _x -= iOff;
    }
    for (auto& _xPrev : x_prev)
    {
        _xPrev -= iOff;
    }
    posFixed[0] -= iOff;
    posFixed[1] -= iOff;
    posFixed[2] -= iOff;
    posFixed[3] -= iOff;

    rope_i_start -= iOff;
    rope_i_end -= iOff;







    // not block Y from going below zero simulate ground
    /*
    for (auto& _x : x)
    {
        float H = _x.y * _inv + ROOT.y;
        if (H < 0)
        {
            _x.y -= (int)(H * _scale);
        }
    }
    */
}

void _new_gliderRuntime::solve_airFlow()
{
    for (auto& r : ribs)
    {
        r.v = float3(0, 0, 0);

        for (int j = 0; j < r.numVerts; j++)    // too much but time it first
        {
            r.v += x[r.startVertex + j] - x_prev[r.startVertex + j];
        }
        r.v *= (1.f / (float)r.numVerts) * _inv / _dt;        // this ribs velocity


        float3 tempWind = { -10, 1.0, 0 };    // placeholder wind, interpolated wind + wing speed at cell
        //tempWind *= 0.3f;
        //r.v += tempWind;
        float3 sum = tempWind - r.v;
        r.cellVelocity = glm::length(sum);
        r.flow_direction = glm::normalize(sum);


        float AIRDENSITY = 1.11225f;    // Fixme form CFD later
        r.rho = __min(100.f, 0.5f * AIRDENSITY * (r.cellVelocity * r.cellVelocity));       // dynamic pressure , nut sure why its clamped
        r.reynolds = (1.125f * r.cellVelocity * r.chordLength) / (0.0000186f);
        r.cD = 0.027f / pow(r.reynolds, 1.f / 7.f);

        // and clear the debug values;
        r._debug.forcePressure = float3(0, 0, 0);
        r._debug.forceSkin = float3(0, 0, 0);
        r._debug.forceGravity = float3(0, 0, 0);
        r._debug.forceInternal = float3(0, 0, 0);

    }
}


void _new_gliderRuntime::solve_wingShape()
{
    float v_aoa[900];
    float v_aob[900];

    for (int i = 0; i < ribs.size(); i++)
    {
        auto& r = ribs[i];

        float3 vx = x[r.rightIdx.x];
        float3 vy = x[r.rightIdx.y];
        float3 vz = x[r.rightIdx.z];
        float3 vw = x[r.rightIdx.w];
        float3 trail = x[r.startVertex];

        float3 v1 = vw - vx;
        float3 v2 = vy - vz;
        r.right = glm::normalize(glm::cross(v1, v2));

        float3 A = glm::lerp(vx, vy, r.aLerp);
        float3 B = glm::lerp(vz, vw, r.bLerp);
        float3 C = glm::lerp(A, B, r.chordLerp);
        r.front = glm::normalize(B - A);
        r.up = glm::normalize(glm::cross(r.right, r.front));

        float aoa = asin(glm::dot(r.up, r.flow_direction));
        if (glm::dot(r.flow_direction, r.front) > 0) {
            aoa = 3.14159f - aoa;
        }
        v_aoa[i] = aoa;
        if (r.cellVelocity < 0.01f)
        {
            v_aoa[i] = r.AoA;
        }
        //r.AoA = glm::lerp(r.AoA, aoa, 0.982f);                          // very slightly smoothed AoA - experiment here
        //r.AoA = aoa;

        float3 vBrake = C - trail;
        float aob = asin(glm::dot(r.up, glm::normalize(vBrake)));
        // r.AoBrake = glm::lerp(r.AoBrake, aob, 0.5f);                  // more smoothed AoB - experiment here
        v_aob[i] = aob;
    }

    /*
    int NoAoA = 4;  // FIXED complete wingtim Aoa Bug
    for (int i = 0; i < NoAoA; i++)
    {
        v_aoa[i] = v_aoa[NoAoA];
        v_aoa[ribs.size() - 1 - i] = v_aoa[ribs.size() - NoAoA - 1];
    }
    */


    // Wintip Vortices
    /*
    int numWintip = 30;
    float twist = -0.1f;
    for (int i = 0; i <= numWintip; i++)
    {
        float dt = 1.f - ((float)i / (float)numWintip);
        dt = pow(dt, 2.f);
        v_aoa[i] += twist * dt;
        v_aoa[ribs.size() - 1 - i] += twist * dt;
    }
    */

    //smooth span wise
    for (int i = 0; i < ribs.size(); i++)
    {
        auto& r = ribs[i];

        int ia = __max(0, i - 1);
        int ib = __min(ribs.size() - 1, i + 1);

        int iaa = __max(0, i - 2);
        int ibb = __min(ribs.size() - 1, i + 2);

        //float aoa = (v_aoa[ia] + v_aoa[i] + v_aoa[ib] + v_aoa[iaa] + v_aoa[ibb]) / 5.f;
        float aoa = (v_aoa[ia] + v_aoa[i] + v_aoa[ib]) / 3.f;
        float aob = (v_aob[ia] + v_aob[i] + v_aob[ib]) / 3.f;

        r.AoA = glm::lerp(r.AoA, aoa, AoA_smooth);
        r.AoBrake = glm::lerp(r.AoBrake, aob, AoB_smooth);
    }

}

float _new_gliderRuntime::sampleCp(float _AoA, float _AoB, float _v, float _v2)
{
    float dv = (_v2 - _v) / 6.f;
    float V = _v + dv * 0.5f;

    float smp = 0;
    for (int i = 0; i < 5; i++)
    {
        smp += sampleCp(_AoA, _AoB, V);
        V += dv;
    }

    return smp / 5.f;
}

float _new_gliderRuntime::sampleCp(float _AoA, float _AoB, float _v)
{
    //return -1.f;
    _v = glm::clamp((_v * 60.f), 0.f, 59.99f);
    int i_v = (int)_v;
    float dv = _v - i_v;



    _AoA = glm::clamp((_AoA * 57.2958f) + 5.f, 0.f, 23.99f);
    int i_aoa = (int)_AoA;
    float d_aoa = _AoA - i_aoa;

    _AoB = glm::clamp(((_AoB * 57.2958f) + 0.f) / 5.f, 0.f, 3.99f);
    int i_AoB = (int)_AoB;
    float d_aob = _AoB - i_AoB;

    //if (i_v < 0 || i_v > (cp_V - 1)) fprintf(terrafectorSystem::_logfile, "bad_v  %d, (%2.4f %2.4f %2.4f) (%d, %d, %d)\n", i_v, _AoA, _AoB, _v, cp_Flaps, cp_AoA, cp_V);
    //if (i_aoa < 0 || i_aoa > (cp_AoA - 1)) fprintf(terrafectorSystem::_logfile, "bad_A  %d, (%2.4f %2.4f %2.4f)\n", i_aoa, _AoA, _AoB, _v);
    //if (i_AoB < 0 || i_AoB > (cp_Flaps - 1)) fprintf(terrafectorSystem::_logfile, "bad_B  %d, (%2.4f %2.4f %2.4f)\n", i_AoB, _AoA, _AoB, _v);

    int idx_A1 = cp_V * i_aoa;
    int idx_A2 = cp_V * i_aoa + cp_V;

    int idx_B1 = cp_V * cp_AoA * i_AoB;
    int idx_B2 = cp_V * cp_AoA * i_AoB + (cp_V * cp_AoA);

    float a = glm::mix(Cp[idx_B1 + idx_A1 + i_v], Cp[idx_B1 + idx_A1 + i_v + 1], dv);
    float b = glm::mix(Cp[idx_B1 + idx_A2 + i_v], Cp[idx_B1 + idx_A2 + i_v + 1], dv);
    float bottom = glm::mix(a, b, d_aoa);

    float c = glm::mix(Cp[idx_B2 + idx_A1 + i_v], Cp[idx_B2 + idx_A1 + i_v + 1], dv);
    float d = glm::mix(Cp[idx_B2 + idx_A2 + i_v], Cp[idx_B2 + idx_A2 + i_v + 1], dv);
    float top = glm::mix(c, d, d_aoa);

    return glm::mix(bottom, top, d_aob);
}

void _new_gliderRuntime::solve_internalPressure()
{
    float p[900];   // just big - should really be allocated elsewere

    for (int i = 0; i < ribs.size(); i++)
    {
        auto& r = ribs[i];

        //int Vaoa = glm::clamp(6 + (int)(r.AoA * 57.f), 0, 35);
        //int v = (int)(127.f * 0.475);   // FIXME 0.475 is hardcoded for the opening as is teh 12 ribs below
        //p[i] = Cp[Vaoa * 128 + v];
        //r.Cp = Cp[Vaoa * 128 + v];
        p[i] = sampleCp(r.AoA, r.AoBrake, 0.47f);
    }

    int NoIntakes = 12 * 1;
    for (int i = 0; i < NoIntakes; i++)
    {
        p[i] = p[NoIntakes];
        p[ribs.size() - 1 - i] = p[ribs.size() - NoIntakes - 1];
    }

    int smoothWidth = 10;
    for (int i = 0; i < ribs.size(); i++)
    {
        auto& r = ribs[i];

        float Cp = 0;
        for (int j = -smoothWidth; j <= smoothWidth; j++)
        {
            int idx = glm::clamp(i + j, 0, (int)ribs.size() - 1);
            Cp += p[idx];
        }
        Cp /= smoothWidth * 2 + 1;
        r.Cp = glm::lerp(r.Cp, Cp, 1.f);

        r.Cp = p[i];
        /*
        int ia = __max(0, i - 1);
        int ib = __min(ribs.size() - 1, i + 1);

        float Cp = (p[ia] + p[i] + p[ib]) / 3.f;
        r.Cp = glm::lerp(r.Cp, Cp, 0.9f);
        */
    }

    /*
    float Cp = 0;
    for (int i = 0; i < ribs.size(); i++)
    {
        Cp += p[i];
    }
    Cp /= (float)ribs.size();
    for (int i = 0; i < ribs.size(); i++)
    {
        ribs[i].Cp = Cp;
    }*/
}


void _new_gliderRuntime::solve_constraints()
{
    auto a = high_resolution_clock::now();
    /*
    x[carabinerRight] = posFixed[0];
    x[carabinerLeft] = posFixed[1];
    x[brakeRight] = posFixed[2];
    x[brakeLeft] = posFixed[3];

    x_prev[carabinerRight] = posFixed[0];
    x_prev[carabinerLeft] = posFixed[1];
    x_prev[brakeRight] = posFixed[2];
    x_prev[brakeLeft] = posFixed[3];
    */
    static int PULLCOUNT = 0;
    if (PULLCOUNT < PULL_DEPTH)
    {
        posFixed[2].y -= (int)(0.001 * _scale);
        posFixed[3].y -= (int)(0.001 * _scale);
        PULLCOUNT++;
    }
    else if (PULLCOUNT > PULL_DEPTH)
    {
        posFixed[2].y += (int)(0.001 * _scale);
        posFixed[3].y += (int)(0.001 * _scale);
        PULLCOUNT--;
    }



    for (int i = 0; i < 1; i++)
    {
        for (auto& C : line_constraints)
        {
            //apply_constraint(C);
        }
        /*
        x[carabinerRight] = posFixed[0];
        x[carabinerLeft] = posFixed[1];
        x[brakeRight] = posFixed[2];
        x[brakeLeft] = posFixed[3];

        x_prev[carabinerRight] = posFixed[0];
        x_prev[carabinerLeft] = posFixed[1];
        x_prev[brakeRight] = posFixed[2];
        x_prev[brakeLeft] = posFixed[3];
        */
    }

    for (auto& C : constraints)
    {
        apply_constraint(C);
    }
    /*
    x[carabinerRight] = posFixed[0];
    x[carabinerLeft] = posFixed[1];
    x[brakeRight] = posFixed[2];
    x[brakeLeft] = posFixed[3];

    x_prev[carabinerRight] = posFixed[0];
    x_prev[carabinerLeft] = posFixed[1];
    x_prev[brakeRight] = posFixed[2];
    x_prev[brakeLeft] = posFixed[3];
    for (auto& C : line_constraints)
    {
        //apply_constraint(C);
    }

    x[carabinerRight] = posFixed[0];
    x[carabinerLeft] = posFixed[1];
    x[brakeRight] = posFixed[2];
    x[brakeLeft] = posFixed[3];

    x_prev[carabinerRight] = posFixed[0];
    x_prev[carabinerLeft] = posFixed[1];
    x_prev[brakeRight] = posFixed[2];
    x_prev[brakeLeft] = posFixed[3];
    */
    auto b = high_resolution_clock::now();
    time_constraints_us = (float)duration_cast<microseconds>(b - a).count();
}


void _new_gliderRuntime::solve_intersection()
{
}

void _new_gliderRuntime::solveAcurateSkinNormals()
{
    auto a = high_resolution_clock::now();
    memset(N.data(), 0, N.size() * sizeof(float3)); // clear
#ifdef DEBUG_GLIDER
    wingArea = 0;
    sumWingArea = float3(0, 0, 0);
    sumWingN = float3(0, 0, 0);
    sumWingNAll = float3(0, 0, 0);
#endif

    for (auto& T : tris)
    {
        float3 va = x[T.x];
        float3 vb = x[T.y];
        float3 vc = x[T.z];
        float3 n = glm::cross((vb - va) * (float)_inv, (vc - va) * (float)_inv);
        N[T.x] += n / 6.f;
        N[T.y] += n / 6.f;
        N[T.z] += n / 6.f;
#ifdef DEBUG_GLIDER
        wingArea += glm::length(n) / 2;
        sumWingArea += n / 2.f;
#endif
    }

    for (int i = 0; i < N.size(); i++)
    {
        if (type[i] == vertexType::vtx_surface) {
            //N[i] = glm::normalize(N[i]);
        }

        if (type[i] == vertexType::vtx_line || type[i] == vertexType::vtx_small_line) {
            //N[i] = glm::normalize(N[i]);
            //float3 line = x[cross_idx[i].x] - x[cross_idx[i].y];    // BUT wrong on teh 3 line sections Like teh tris we can do this from the constraints intead
            //N[i] = glm::normalize(line);
        }
    }

    auto b = high_resolution_clock::now();
    time_norms_us = (float)duration_cast<microseconds>(b - a).count();

#ifdef DEBUG_GLIDER
    for (int i = 0; i < N.size(); i++)
    {
        if (type[i] == vertexType::vtx_surface) {
            //N[i] = glm::normalize(N[i]);
            sumWingN += N[i];
        }
        sumWingNAll += N[i];
    }

#endif
}

void _new_gliderRuntime::solve_pre()
{
    float3 tempWind = { -10, 1.0, 0 };    // placeholder wind, interpolated wind + wing speed at cell
    //tempWind *= 0.3f;
    float V = glm::length(tempWind);
    float tempQ = 0.5f * 1.05f * V * V;        // dynamic pressure 1/2 rho v^2
    float tempCd_surface = 0.0039f;  // surface coeficient of drag, vary with wrincles, lots of tests needed
    float tempCd_lines = 0.6f;
    float tempCd_pilot = 0.6f;
    float tempArea_pilot = 1.0f;    // 1m^2

    debug_wing_F = float3(0, 0, 0);
    debug_wing_Skin = float3(0, 0, 0);

#ifdef DEBUG_GLIDER
    lineDrag = float3(0, 0, 0);

    for (int i = 0; i < ribs.size(); i++)
    {
        ribs[i].CL = 0;
        ribs[i].CD = 0;
        ribs[i].AREA = 0;
    }
#endif

    float3 t;
    //int3 a_dt2;
    float scale = (float)_time_scale;
    float Qscale = (float)(_inv / _dt);

    num_vtx_line = 0;
    /*
    for (int i = 0; i < x.size(); i++)
    {
        t = glm::normalize(tempWind - glm::dot(tempWind, N[i]) * N[i]);

        // all the forces
        {
            // gravity
            a_dt2[i] = float3(0, -i_g_dt, 0);


            // skin surface
            if (type[i] == vertexType::vtx_surface) // FIXME Rtype is still exportwed wrong everythign si surface
            {
                a_dt2[i] = float3(0, -(i_g_dt >> 1), 0);   // Only allpy half of teh gravity

                int r = (int)uv[i].x;

                float skinCp = -1;  // for wingtips for now
                float vCp = ribs[0].Cp - skinCp;
                if (r == -2)
                {
                    vCp = ribs.back().Cp - skinCp;
                }

                if (r >= 0)
                {
                    //int Vaoa = glm::clamp(6 + (int)(ribs[r].AoA * 57.f), 0, 35);
                    int v = (int)(127.f * uv[i].y);   // FIXME 0.475 is hardcoded for the opening as is teh 12 ribs below
                    //skinCp = sampleCp(ribs[r].AoA, ribs[r].AoBrake, uv[i].y);
                    skinCp = sampleCp(ribs[r].AoA, ribs[r].AoBrake, uv[i].y);
                    vCp = ribs[r].Cp - skinCp;
                }

                if (r == -1) r = 0;
                if (r == -2) r = (int)ribs.size() - 1;

                //a_dt2 += area_x_w[i] * vCp * ribs[r].rho * scale * N[i];
                //a_dt2 += area_x_w[i] * tempCd_surface * tempQ * scale * t;
                a_dt2[i] += w[i] * vCp * ribs[r].rho * scale * N[i];
                a_dt2[i] += area_x_w[i] * tempCd_surface * tempQ * scale * t * 0.1f;


                int3 v1 = x[i] - x_prev[i];
                int3 v2 = x[cross_idx[i].x] - x_prev[cross_idx[i].x];
                int3 v3 = x[cross_idx[i].y] - x_prev[cross_idx[i].y];
                int3 dV = (v2 + v3) / 2 - v1;
                a_dt2[i] += dV / 2;


#ifdef DEBUG_GLIDER
                ribs[r]._debug.forcePressure += (-skinCp) * ribs[r].rho * N[i];
                ribs[r]._debug.forceSkin += area[i] * tempCd_surface * tempQ * t * 0.1f;
                ribs[r]._debug.forceGravity += float3(0, -9.8f / w[i], 0);
                ribs[r]._debug.forceInternal += ribs[r].Cp * ribs[r].rho * N[i];

                float3 windN = glm::normalize(tempWind);
                float3 UP = glm::normalize(glm::cross(windN, ribs[r].right));
                float3 CPN = N[i] * (-skinCp);
                ribs[r].CL += glm::dot(CPN, UP);
                ribs[r].CD -= glm::dot(CPN, windN);
                ribs[r].AREA += glm::length(N[i]);

                debug_wing_F += (-skinCp) * ribs[r].rho * N[i]; //N[i] contains area as well its scaled
                debug_wing_Skin += area_x_w[i] * tempCd_surface * tempQ * t; // skin friction
                //debug_wing_F += ribs[r]._debug.forceSkin;
                //debug_wing_F += ribs[r]._debug.forceGravity;
                //debug_wing_F += ribs[r]._debug.forceInternal;
#endif
            }

            // line drag
            if (type[i] == vertexType::vtx_line || type[i] == vertexType::vtx_small_line)
            {
                //a_dt2 = float3(0, -(i_g_dt >> 1), 0);   // Only allpy half of teh gravity TEMP

                float3 QV = (x[i] - x_prev[i]);
                float3 lineV = tempWind -QV * Qscale;
                float lineV2 = (lineV.x * lineV.x) + (lineV.y * lineV.y) + (lineV.z * lineV.z);
                float lineQ = 0.5f * 1.05f * lineV2;        // dynamic pressure 1/2 rho v^2             60us cost

                t = glm::normalize(lineV);
                //t = glm::normalize(tempWind);
                //a_dt2 += 100.f * tempCd_lines * area_x_w[i] * tempQ * scale * t;
                a_dt2[i] += 1.0f * tempCd_lines * lineQ * area_x_w[i] * scale * t;
                num_vtx_line++;

                //if (type[i] == vertexType::vtx_small_line)
                {
                    //if (cross_idx[i].x <= 0)  fprintf(terrafectorSystem::_logfile, "VTX %d, cross.x == 0\n", i);
                    //if (cross_idx[i].y <= 0)  fprintf(terrafectorSystem::_logfile, "VTX %d, cross.y == 0\n", i);
                    //if (cross_idx[i].x >= type.size())  fprintf(terrafectorSystem::_logfile, "VTX %d, cross.x >= type.size(), %d / %d\n", i, cross_idx[i].x, (int)type.size());
                    //if (cross_idx[i].y >= type.size())  fprintf(terrafectorSystem::_logfile, "VTX %d, cross.y >= type.size(), %d / %d\n", i, cross_idx[i].y, (int)type.size());

                    int3 v1 = x[i] - x_prev[i];
                    int3 v2 = x[cross_idx[i].x] - x_prev[cross_idx[i].x];
                    int3 v3 = x[cross_idx[i].y] - x_prev[cross_idx[i].y];
                    int3 dV = (v2 + v3) / 2 - v1;
                    a_dt2[i] += dV / SMALL_LINE_SMOOTH;

                }


#ifdef DEBUG_GLIDER
                //if (w[i] > 0.1f)
                {
                    lineDrag += 1.0f * tempCd_lines * lineQ * area_x_w[i] * t / w[i];
                }
#endif
            }

            // pilot drag
            if (type[i] == vertexType::vtx_pilot)
            {
                t = glm::normalize(tempWind);
                a_dt2[i] += tempCd_pilot * tempArea_pilot * area_x_w[i] * tempQ * scale * t;
            }
        }


    }
    */

    // Line damping
    // for ropes hold the two ends
    //
    x[0] = rope_i_start;
    x_prev[0] = rope_i_start;
    if (grabEnd)
    {
        x[x.size() - 1] = rope_i_end;
        x_prev[x.size() - 1] = rope_i_end;
    }



    for (int i = 0; i < x.size(); i++)
    {
        // smoothing
        int3 v1 = x[i] - x_prev[i];
        int3 v2 = x[cross_idx[i].x] - x_prev[cross_idx[i].x];
        int3 v3 = x[cross_idx[i].y] - x_prev[cross_idx[i].y];
        int3 v4 = x[cross_idx[i].z] - x_prev[cross_idx[i].z];
        int3 v5 = x[cross_idx[i].w] - x_prev[cross_idx[i].w];
        //int3 dV = (v2 + v3 + v4 + v5) / 4 - v1;
        int3 dV = (v3 + v4) / 2 - v1;
        a_dt2[i] = glm::ivec3(0, -i_g_dt, 0) + (dV / SMALL_LINE_SMOOTH);
    }

    // add a force at the end
    a_dt2[x.size() - 1].y -= (int)(w[x.size() - 1] * addedForceEnd * _time_scale); // 100 nweton

    for (int i = 0; i < x.size(); i++)
    {
        int3 oldX = x[i];
        int3 dx = (x[i] - x_prev[i]) + a_dt2[i];
        int3 vx = (x[i] - x_prev[i]);
        x[i] += dx;   // calc v_dt[i] on the spot

        if (testGround)
        {
            float H = x[i].y * _inv + ROOT.y;
            if (H < 1)
            {


                float3 N = { 0, 1, 0 };
                int a = (int)(N.x * dx.x) + (int)(N.y * dx.y) + (int)(N.z * dx.z);
                int3 aN = { N.x * a, N.y * a, N.z * a };
                int3 aP = dx - aN;



                float3 nNormal = dx;
                nNormal = glm::normalize(nNormal);
                float fricDot = -glm::dot(nNormal, N);

                if (fricDot < 0.65)
                {
                    friction[i] = 1;    // sliding
                    fricDot = 0.02f;
                    x[i] -= int3(aP.x * fricDot, aP.y * fricDot, aP.z * fricDot);
                    // mover UP last
                    x[i] -= aN; // Move out but should really be in the movement direction but this is ok for now
                }
                else
                {
                    friction[i] = 2; // static
                    x[i] = oldX;
                    fricDot = 1.0f;
                }


                // bounce it 50% werk nog nei reg nie, dink hieroor, split velocity update en acelleration update
                //x[i].y += -vx.y;


                
            }
            else
            {
                friction[i] = 0;
            }
        }
    

        x_prev[i] = oldX;
    }

    x[0] = rope_i_start;
    x_prev[0] = rope_i_start;
    if (grabEnd)
    {
        x[x.size() - 1] = rope_i_end;
        x_prev[x.size() - 1] = rope_i_end;
    }

}





/*
    For now I cant find a way out of float 4.28 fixed will overrun when calculating length()
    Less acuracy, but likely enough for what we need with 6 decimal bits
    and not classy with that if, but maybe not the bottelneck either
*/
void _new_gliderRuntime::apply_constraint(_new_constraint& cnst)  // allow softness and stretch later
{
    float scale = 0.7f;
    if (cnst.type == constraintType::lines) scale = 1.f;
    if (cnst.type == constraintType::smallLines) scale = 1.f;

    //if (cnst.type == constraintType::skin || cnst.type == constraintType::ribs || cnst.type == constraintType::half_ribs)
    //if (cnst.type != constraintType::lines)
    {
        int3 vint = x[cnst.idx_1] - x[cnst.idx_0];
        float3 vec = { vint.x * _inv, vint.y * _inv , vint.z * _inv };
        //float3 vec = x[cnst.idx_1] - x[cnst.idx_0];
        float L = __max(0.0001f, glm::length(vec));
        //..L += L - cnst.old_L;
        //float C = (L - cnst.l - 0.3f * (cnst.current_L - cnst.old_L)) / L; // devide by L because we scale by vec
        float C = (L - cnst.l) / L; // devide by L because we scale by vec


        cnst.force = (cnst.l - L) / (cnst.wsum) / _dt2;// / (w[cnst.idx_0] + w[cnst.idx_1]);
        cnst.old_L = L;

        if (C > 0) {
            C *= cnst.tensileStiff;// *scale;
            cnst.force *= cnst.tensileStiff;
        }
        else {
            C *= cnst.compressStiff;
            cnst.force *= cnst.compressStiff;
        }

        // velocity damping
       // float v = (L - cnst.old_L) / cnst.l;
       // C *= 1.f + (v);
        //C += glm::clamp(v * 0.1f, -0.1f, 0.1f);// *cnst.damping;

        // Lamda
        //float lamda = (cnst.l - L) / (w[cnst.idx_0] + w[cnst.idx_1])


        x[cnst.idx_0] += (C * cnst.w_rel_0 * (float)_scale) * vec;
        x[cnst.idx_1] -= (C * cnst.w_rel_1 * (float)_scale) * vec;

        //cnst.old_L = L;
    }
}


// horiontal between two points
void _new_gliderRuntime::importBin_generateRope()
{
    bool DistributeWeight = true;
    uint numLOD = 6;
    ROOT = float3(0, 0, 0);
    int numV = (1<< numLOD) + 1;// (int)(rope_L / rope_sub) + 1;
    int numT = 0;   //triangles
    int numC = numV * 10;    //jst large we trim after adding
    int numR = 0; // ribs

    x.resize(numV);
    x_prev.resize(numV);
    a_dt2.resize(numV);
    w.resize(numV);

    // unnesesary
    cross_idx.resize(numV);
    type.resize(numV);
    area_x_w.resize(numV);
    uv.resize(numV);
    N.resize(numV);     // for temp normals
    friction.resize(numV);

    constraints.resize(numC);
    int C_idx = 0;

    fprintf(terrafectorSystem::_logfile, "_new_gliderRuntime::importBin_generateRope()\n");
    fprintf(terrafectorSystem::_logfile, "####################################################################\n");
    fprintf(terrafectorSystem::_logfile, "numV %d\n", numV);
    fprintf(terrafectorSystem::_logfile, "numC %d, %d\n", numC, C_idx);

    float smallLength = rope_L / (numV - 1);
    float sectionMass = rope_M_per_meter * smallLength;

    rope_i_start = glm::ivec3(rope_Start.x * _scale, rope_Start.y * _scale, rope_Start.z * _scale);
    rope_i_end = glm::ivec3(rope_End.x * _scale, rope_End.y * _scale, rope_End.z * _scale);

    for (int i = 0; i < numV; i++)
    {
        float3 pos = glm::lerp(rope_Start, rope_End, (float)i / (float)(numV - 1));
        pos.z += sin((float)i / (float)(numV - 1) * 6.283185f) * 0.03f;
        x[i] = glm::ivec3(pos.x * _scale, pos.y * _scale, pos.z * _scale);
        x_prev[i] = x[i];
        a_dt2[i] = float3(0, -i_g_dt, 0);
        w[i] = 1.0f / sectionMass;// vir nou ignoireer dat die randte ligter weeg
        cross_idx[i] = uint4(__max(0, i - 2), __max(0, i - 1), __min((numV - 1), i + 1), __min((numV - 1), i + 2));
        friction[i] = 0;
    }

    w[0] = 0;
    //w[w.size() - 1] = 0.1f;    // 10 kg


    float totalMass = rope_M_per_meter * rope_L;
    for (int lod = 0; lod <= numLOD; lod++)
    {
        uint Tp = constraintType::smallLines;
        int num = 1 << lod;
        int step = (numV - 1) >> lod;
        float m = totalMass / (num);
        float wtemp = 1.0f / sectionMass; //1.f / m;
        float l = rope_L / (float)num;

        if (lod < numLOD)
        {
            float extra = 0.01f / (float)num;
            l = l * (1.f + extra );
            Tp = constraintType::lines;
        }
    
        float k = 1.f / (wtemp + wtemp) / _dt2;


        float kdt = 20000.f * pow(2.f, ((float)lod * 1.5f));
        float adt2 = 1.f / kdt / _dt2;
        if (lod >= 4) adt2 = 0;
        adt2 = 0;
        fprintf(terrafectorSystem::_logfile, "\nlod %d   l %2.6f m,   w %2.3f,  m %2.3f kg,  k %2.3f,   adt2  %2.3f,   step %d\n", lod, l, wtemp, m, k, adt2, step);

        float d2 = l * 1.02f - l;
        float f_raw = d2 / (wtemp + wtemp) / _dt2;
        float f_a = d2 / (wtemp + wtemp + adt2) / _dt2;
        fprintf(terrafectorSystem::_logfile, "f raw 2%% %2.3f N      f_a 2%% %2.3f N\n", f_raw, f_a);

        float stiff = 0.f;
        if (step == 1) stiff = 0.1f;
        for (int i = 0; i < numV - 1; i += step)
        {
            constraints[C_idx] = _new_constraint(i, i + step, l, 1.f, stiff, 0.f, w[i], w[i+step], adt2, Tp);
            sprintf(constraints[C_idx].name, "lod-%d", lod);
            //fprintf(terrafectorSystem::_logfile, "C :  %d  %d  l %2.3f m,  wsum %2.3f\n", i, i + step, constraints[C_idx].l, constraints[C_idx].wsum);
            C_idx++;

            if (step == 1 && i < numV - 2)
            {
                constraints[C_idx] = _new_constraint(i, i + 2, l * 2.f, 1.f, 0.001f, 0.f, w[i], w[i + 2], adt2, constraintType::lines);
                sprintf(constraints[C_idx].name, "bend", lod);
                C_idx++;
            }
        }
    }

    constraints.resize(C_idx);
    std::random_device rd;
    std::mt19937 g(rd());
    //std::shuffle(constraints.begin(), constraints.end(), g); // baie klein verskil


    DistributeWeight = false;
    if (DistributeWeight)
    {
        for (int i = 0; i < numV; i++)
        {
            w[i] = 0;
        }

        float m = 0.1f / 64.f / 2.f;
        for (auto& C : constraints)
        {
            w[C.idx_0] += m;// 1.0f / (C.w0) * 0.5f;
            w[C.idx_1] += m;//1.0f / (C.w1) * 0.5f;
        }

        fprintf(terrafectorSystem::_logfile, "\n\n g - ");
        adjustedMass = 0.f;
        for (int i = 0; i < numV; i++)
        {
            fprintf(terrafectorSystem::_logfile, "  %2.1f ", w[i] * 1000.f);
            if (i > 0) adjustedMass += w[i];
            w[i] = 1.0f / w[i];
        }
        fprintf(terrafectorSystem::_logfile, "adjustedMass %2.3f kg\n", adjustedMass);


        for (auto& C : constraints)
        {
            C.w0 = w[C.idx_0];
            C.w1 = w[C.idx_1];
            C.wsum = C.w0 + C.w1;
            C.w_rel_0 = C.w0 / C.wsum;
            C.w_rel_1 = C.w1 / C.wsum;
        }
    }




  

    fprintf(terrafectorSystem::_logfile, "\n\n");
}







void _new_gliderRuntime::importBin_generateCube()
{
    ROOT = float3(0, 3, 0);
    int grid = 32;
    int numV = grid * grid * grid;
    int numT = 0;   //triangles
    int numC = (grid) * (grid) * (grid) * 6 * 2;    //???
    int numR = 0; // ribs

    int stepScale = (int)(_scale / 6);
    float total_mass = 100.0f;      //kg
    float blockMass = total_mass / ((grid - 1) * (grid - 1) * (grid - 1));

    x.resize(numV);
    x_prev.resize(numV);
    a_dt2.resize(numV);
    w.resize(numV);

    // unnesesary
    cross_idx.resize(numV);
    type.resize(numV);
    area_x_w.resize(numV);
    uv.resize(numV);
    N.resize(numV);     // for temp normals



    for (int dz = 0; dz < grid; dz++)
    {
        for (int dy = 0; dy < grid; dy++)
        {
            for (int dx = 0; dx < grid; dx++)
            {
                int idx = (dz * grid * grid) + (dy * grid) + dx;


                x[idx] = glm::ivec3(stepScale * dx, stepScale * dy, stepScale * dz);
                x_prev[idx] = x[idx];
                a_dt2[idx] = float3(0, -i_g_dt, 0);



                int sum = 0;
                if (dx == 0 || dx == grid - 1) sum++;
                if (dy == 0 || dy == grid - 1) sum++;
                if (dz == 0 || dz == grid - 1) sum++;
                float scale = pow(2, sum);

                w[idx] = 1.0f / blockMass;// *scale; // vir nou ignoireer dat die randte ligter weeg

                uint c1 = idx;
                uint c2 = idx;
                uint c3 = idx;
                uint c4 = idx;

                if (dz > 0) {   // onder
                    if (dy > 0 && dx > 0)
                    {
                        c1 += -(grid * grid) - grid - 1;
                    }

                    if (dy < grid - 1 && dx < grid - 1)
                    {
                        c2 += -(grid * grid) + grid + 1;
                    }
                }

                if (dz < grid - 1)  // bo
                {
                    if (dy > 0 && dx < grid - 1)
                    {
                        c3 += (grid * grid) - grid + 1;
                    }

                    if (dy < grid - 1 && dx > 0)
                    {
                        c4 += (grid * grid) + grid - 1;
                    }
                }

                cross_idx[idx] = uint4(c1, c2, c3, c4);

                if (c1 >= numV) fprintf(terrafectorSystem::_logfile, "cross_idx ERROR %d\n", c1);
                if (c2 >= numV) fprintf(terrafectorSystem::_logfile, "cross_idx ERROR %d\n", c2);
                if (c3 >= numV) fprintf(terrafectorSystem::_logfile, "cross_idx ERROR %d\n", c3);
                if (c4 >= numV) fprintf(terrafectorSystem::_logfile, "cross_idx ERROR %d\n", c4);
            }
        }
    }


    constraints.resize(numC);

    int C_idx = 0;
    float straightL = 1.f / 6.f;
    float skewL = straightL * sqrt(3.0f);

    // sgtraight
    for (int dz = 0; dz < grid; dz++)
    {
        for (int dy = 0; dy < grid; dy++)
        {
            for (int dx = 0; dx < grid; dx++)
            {
                int idx = (dz * grid * grid) + (dy * grid) + dx;
                int idx_x = idx + 1;
                int idx_y = idx + grid;
                int idx_z = idx + grid * grid;

                constraintType type = constraintType::smallLines;
                constraintType typeSide = constraintType::straps;

                if (dx < grid - 1)
                {
                    constraints[C_idx] = _new_constraint(idx, idx_x, straightL, 1.f, 1.f, 0.f);
                    constraints[C_idx].type = type;
                    //if (dx == 0 || dx == grid - 2) constraints[C_idx].type = typeSide;
                    if (dz == 0 || dz == grid - 1) constraints[C_idx].type = typeSide;
                    C_idx++;
                }

                if (dy < grid - 1)
                {
                    constraints[C_idx] = _new_constraint(idx, idx_y, straightL, 1.f, 1.f, 0.f);
                    constraints[C_idx].type = type;
                    //if (dy == 0 || dy == grid - 2) constraints[C_idx].type = typeSide;
                    if (dz == 0 || dz == grid - 1) constraints[C_idx].type = typeSide;
                    C_idx++;
                }

                if (dz < grid - 1)
                {
                    constraints[C_idx] = _new_constraint(idx, idx_z, straightL, 1.f, 1.f, 0.f);
                    constraints[C_idx].type = type;
                    //if (dz == 0 || dz == grid - 2) constraints[C_idx].type = typeSide;
                    //if (dz == 0) constraints[C_idx].type = typeSide;
                    C_idx++;
                }

                // Double STEP
                // ####################################################################################
                if (dx < grid - 2)
                {
                    idx_x = idx + 2;
                    constraints[C_idx] = _new_constraint(idx, idx_x, straightL * 2, 1.f, 1.f, 0.f);
                    constraints[C_idx].type = type;
                    //if (dx == 0 || dx == grid - 2) constraints[C_idx].type = typeSide;
                    if (dz == 0 || dz == grid - 1) constraints[C_idx].type = typeSide;
                    C_idx++;
                }

                if (dy < grid - 2)
                {
                    idx_y = idx + grid * 2;
                    constraints[C_idx] = _new_constraint(idx, idx_y, straightL * 2, 1.f, 1.f, 0.f);
                    constraints[C_idx].type = type;
                    //if (dy == 0 || dy == grid - 2) constraints[C_idx].type = typeSide;
                    if (dz == 0 || dz == grid - 1) constraints[C_idx].type = typeSide;
                    C_idx++;
                }

                if (dz < grid - 2)
                {
                    int idx_z = idx + grid * grid * 2;
                    constraints[C_idx] = _new_constraint(idx, idx_z, straightL * 2, 1.f, 1.f, 0.f);
                    constraints[C_idx].type = type;
                    //if (dz == 0 || dz == grid - 2) constraints[C_idx].type = typeSide;
                    //if (dz == 0) constraints[C_idx].type = typeSide;
                    C_idx++;
                }
            }
        }
    }



    // face diagonal




    // diagonal
    for (int dz = 0; dz < grid; dz++)
    {
        for (int dy = 0; dy < grid; dy++)
        {
            for (int dx = 0; dx < grid; dx++)
            {
                int idx = (dz * grid * grid) + (dy * grid) + dx;

                if ((dx < grid - 1) && (dy < grid - 1) && (dz < grid - 1))
                {
                    int idx_x = idx + grid * grid + grid + 1;
                    constraints[C_idx] = _new_constraint(idx, idx_x, skewL, 1.f, 1.f, 0.f);
                    constraints[C_idx].type = constraintType::lines;
                    C_idx++;
                }

                if ((dx > 0) && (dy < grid - 1) && (dz < grid - 1))
                {
                    int idx_y = idx + grid * grid + grid - 1;
                    constraints[C_idx] = _new_constraint(idx, idx_y, skewL, 1.f, 1.f, 0.f);
                    constraints[C_idx].type = constraintType::lines;
                    C_idx++;
                }

                if ((dx < grid - 1) && (dy > 0) && (dz < grid - 1))
                {
                    int idx_z = idx + grid * grid - grid + 1;
                    constraints[C_idx] = _new_constraint(idx, idx_z, skewL, 1.f, 1.f, 0.f);
                    constraints[C_idx].type = constraintType::lines;
                    C_idx++;
                }


            }
        }
    }

    fprintf(terrafectorSystem::_logfile, "_new_gliderRuntime::importBin_generateCube()\n");
    fprintf(terrafectorSystem::_logfile, "####################################################################\n");
    fprintf(terrafectorSystem::_logfile, "grid %d\n", grid);
    fprintf(terrafectorSystem::_logfile, "straightL %f\n", straightL);
    fprintf(terrafectorSystem::_logfile, "skewL %f\n", skewL);
    fprintf(terrafectorSystem::_logfile, "numV %d\n", numV);
    fprintf(terrafectorSystem::_logfile, "numC %d, %d\n", numC, C_idx);
    constraints.resize(C_idx);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(constraints.begin(), constraints.end(), g); // baie klein verskil

    // straight K
    float wtest = 1.0f / blockMass;
    float onePercent = straightL * 0.01f;
    float f = onePercent / (wtest + wtest) / _dt2;
    float straightK = f / onePercent;
    fprintf(terrafectorSystem::_logfile, "f %f N at 1.0 percent, or %f N f lying flat\n", f, f * grid * grid);
    fprintf(terrafectorSystem::_logfile, "straightK %f\n", straightK);



    fprintf(terrafectorSystem::_logfile, "\n\n");
}



void _new_gliderRuntime::importBin()
{
    importBin_generateRope();
    //importBin_generateCube();
    return;


    std::ifstream ifs;
    ifs.open("E:/Advance/omega_export_low.bin", std::ios::binary);
    if (ifs)
    {
        fprintf(terrafectorSystem::_logfile, "_new_gliderRuntime::importBin()\n");

        ifs.read((char*)&carabinerRight, sizeof(int));
        ifs.read((char*)&carabinerLeft, sizeof(int));
        ifs.read((char*)&brakeRight, sizeof(int));
        ifs.read((char*)&brakeLeft, sizeof(int));

        int numV;
        int numT;
        int numC;
        int numR;
        ifs.read((char*)&numV, sizeof(int));
        ifs.read((char*)&numT, sizeof(int));
        ifs.read((char*)&numC, sizeof(int));
        ifs.read((char*)&numR, sizeof(int));
        ifs.read((char*)&bucketSize, sizeof(int));
        ifs.read((char*)&numBuckets, sizeof(int));



        fprintf(terrafectorSystem::_logfile, "V %d, T %d, C %d, R %d, bucketSize %d %d\n", numV, numT, numC, numR, bucketSize, numBuckets);



        x.resize(numV);
        x_prev.resize(numV);
        a_dt2.resize(numV);
        cross_idx.resize(numV);
        type.resize(numV);
        area_x_w.resize(numV);
        uv.resize(numV);
        N.resize(numV);     // for temp normals


        ifs.read((char*)x.data(), sizeof(int3) * numV);
        memcpy(x_prev.data(), x.data(), sizeof(int3) * numV);

        ifs.read((char*)cross_idx.data(), sizeof(uint4) * numV);
        ifs.read((char*)type.data(), sizeof(char) * numV);
        ifs.read((char*)area_x_w.data(), sizeof(float) * numV);
        ifs.read((char*)uv.data(), sizeof(float2) * numV);


        // triangles
        tris.resize(numT);
        ifs.read((char*)tris.data(), sizeof(glm::ivec3) * numT);

        tris_airtight.resize(numT);
        ifs.read((char*)tris_airtight.data(), sizeof(glm::ivec3) * numT);

        // constraint buckets
        ConstraintBuckets.resize(numBuckets * bucketSize);
        ifs.read((char*)ConstraintBuckets.data(), sizeof(int) * numBuckets * bucketSize);

        // constraints
        constraints.resize(numC);
        ifs.read((char*)constraints.data(), sizeof(_new_constraint) * numC);

        // ribs
        ribs.resize(numR);
        ifs.read((char*)ribs.data(), sizeof(_new_rib) * numR);


        // Debug data at the end where it can be ignored
//#ifdef DEBUG_GLIDER
        area.resize(numV);
        w.resize(numV);
        ifs.read((char*)area.data(), sizeof(float) * numV);
        ifs.read((char*)w.data(), sizeof(float) * numV);
        //#endif

        ifs.close();
    }







    // reset constraints
    line_constraints.clear();
    for (auto& cnst : constraints)
    {

        if (cnst.type == constraintType::nitinol)
        {
            cnst.tensileStiff = 1.f;

            if (cnst.idx_1 - cnst.idx_0 == 1) {
                cnst.compressStiff = 1.0f;          // this is the straigth part
            }
            else {
                cnst.compressStiff = 0.1f;
            }
        }



        if (cnst.type == constraintType::smallLines)
        {
            cnst.l *= 0.993f;   // FIXME ratehr shoirten lines, anddependent on actual length, i.e. number of splits. I knwo it there
            //cnst.w_0 *= 3.01f;
            //cnst.w_1 *= 3.01f;
            /*
            if (type[cnst.idx_0] == vertexType::vtx_line)
            {
                cnst.w_0 = 0.333f * (cnst.w_0 + cnst.w_1);
                cnst.w_1 = 0.667f * (cnst.w_0 + cnst.w_1);
            }
            if (type[cnst.idx_1] == vertexType::vtx_line)
            {
                cnst.w_0 = 0.667f * (cnst.w_0 + cnst.w_1);
                cnst.w_1 = 0.333f * (cnst.w_0 + cnst.w_1);
            }
            */
            cnst.compressStiff = 0.0f;
            cnst.tensileStiff = 1.f;

        }

        if (cnst.type == constraintType::lines)
        {
            cnst.compressStiff = 0;
            cnst.tensileStiff = 1.0f;

            int segments = (int)(ceil(cnst.l / 0.1f));
            if (segments > 1)
            {
                float P = pow((float)(segments - 1), 1.4f);
                //cnst.l *= 1.f + (P * 0.01f * 0.005f);

                P = pow((float)(segments - 1), 1.3f);
                //cnst.tensileStiff = 1.f / P;
            }
            //cnst.l *= 1.001f;

            line_constraints.push_back(cnst);
            //cnst.w_0 *= 0.01f;
            //cnst.w_1 *= 0.01f;

            fprintf(terrafectorSystem::_logfile, "LINES - %s w(%2.2f, %2.2f), wrel(%2.2f, %2.2f)\n", cnst.name, cnst.w0, cnst.w1, cnst.w_rel_0, cnst.w_rel_1);
        }

        if (cnst.type == constraintType::ribs)
        {
            // length dependent
            float max = 0.01f;
            float min = 0.001f;
            float a = glm::clamp(1.f - 0.01f / cnst.l, 0.f, 1.f);
            cnst.compressStiff = glm::mix(min, max, a);    // VERY little
        }

        if (cnst.type == constraintType::half_ribs)
        {
            float max = 0.01f;
            float min = 0.001f;
            float a = glm::clamp(1.f - 0.01f / cnst.l, 0.f, 1.f);
            cnst.compressStiff = glm::mix(min, max, a);    // VERY little
        }

        if (cnst.type == constraintType::skin)
        {
            cnst.tensileStiff = 1.f;
            cnst.compressStiff = 0.0083f;    // VERY little
            if (uv[cnst.idx_0].y == 0 || uv[cnst.idx_0].y == 1 || uv[cnst.idx_1].y == 0 || uv[cnst.idx_1].y == 1)
            {
                cnst.compressStiff = 0.07f; // trailing edge
            }
        }

        if (cnst.type == constraintType::diagonals) {
            cnst.compressStiff = 0.00f;
        }

    }

#ifdef DEBUG_GLIDER
    for (int i = 0; i < vtx_MAX; i++)
    {
        mass[i] = 0;
    }

    for (int i = 0; i < type.size(); i++)
    {
        mass[type[i]] += 1.f / w[i];
    }
#endif


    posFixed[0] = x[carabinerRight];
    posFixed[1] = x[carabinerLeft];
    posFixed[2] = x[brakeRight];
    posFixed[3] = x[brakeLeft];

    //posFixed[2].y -= (int)(0.1f * (float)_scale);



    // Shuffle the vector
    std::random_device rd;
    std::mt19937 g(rd());
    // BAD BAD BAD std::shuffle(constraints.begin(), constraints.end(), g); // baie klein verskil

    // sanity checks
#ifdef DEBUG_GLIDER
    fprintf(terrafectorSystem::_logfile, "mass\n");
    fprintf(terrafectorSystem::_logfile, "surface %2.4f kg\n", mass[vtx_surface]);
    fprintf(terrafectorSystem::_logfile, "internal %2.4f kg\n", mass[vtx_internal]);
    fprintf(terrafectorSystem::_logfile, "line %2.4f kg\n", mass[vtx_line]);
    fprintf(terrafectorSystem::_logfile, "small line %2.4f kg\n", mass[vtx_small_line]);
#endif

    for (int i = 0; i < x.size(); i++)
    {
        if (type[i] == vtx_surface) // I just want 3 here nicely spread
        {
            if (cross_idx[i].x >= x.size())  fprintf(terrafectorSystem::_logfile, "vtx_surface [%d] - cross_idx[i].x >= x.size()\n", i);
            if (cross_idx[i].y >= x.size())  fprintf(terrafectorSystem::_logfile, "vtx_surface [%d] - cross_idx[i].y >= x.size()\n", i);
            if (cross_idx[i].z >= x.size())  fprintf(terrafectorSystem::_logfile, "vtx_surface [%d] - cross_idx[i].z >= x.size()\n", i);
        }
        if (type[i] == vtx_small_line)
        {
            if (cross_idx[i].x <= 0)  fprintf(terrafectorSystem::_logfile, "vtx_small_line [%d] - cross_idx[i].x <= 0\n", i);
            if (cross_idx[i].y <= 0)  fprintf(terrafectorSystem::_logfile, "vtx_small_line [%d] - cross_idx[i].y <= 0\n", i);
            if (cross_idx[i].x >= (uint)x.size())  fprintf(terrafectorSystem::_logfile, "vtx_small_line [%d] - cross_idx[i].x >= x.size()\n", i);
            if (cross_idx[i].y >= (uint)x.size())  fprintf(terrafectorSystem::_logfile, "vtx_small_line [%d] - cross_idx[i].y >= x.size()\n", i);
        }

    }

    fprintf(terrafectorSystem::_logfile, "\n\n\n");

}


float3  _new_gliderRuntime::projectXFoil(float3 p, _new_rib& R)
{
    float3 vx = x[R.rightIdx.x];
    float3 vy = x[R.rightIdx.y];
    float3 vz = x[R.rightIdx.z];
    float3 vw = x[R.rightIdx.w];
    float3 trail = x[R.startVertex];
    float3 lead = x[R.startVertex + R.numVerts / 2];
    float3 A = glm::lerp(vx, vy, R.aLerp);
    float3 B = glm::lerp(vz, vw, R.bLerp);
    float3 C = glm::lerp(A, B, R.chordLerp);
    float3 ORIGIN = B;// C + (glm::normalize(A - B) * R.chordLength * 0.5f * _scale);
    float3 FRONT = R.front;
    //FRONT.y = 0;
    FRONT = glm::normalize(FRONT);
    float3 UP = glm::normalize(glm::cross(R.right, FRONT));



    float3 vec = (p - lead) * (float)_inv;
    float x = glm::dot(FRONT, vec) / R.chordLength;
    float y = glm::dot(UP, vec) / R.chordLength;
    return float3(-x, y, 0);
}

ImVec2 _new_gliderRuntime::project(float3 p, _new_rib& R, ImVec2 _pos, float _scale)
{
    float3 vx = x[R.rightIdx.x];
    float3 vy = x[R.rightIdx.y];
    float3 vz = x[R.rightIdx.z];
    float3 vw = x[R.rightIdx.w];
    float3 trail = x[R.startVertex];
    float3 A = glm::lerp(vx, vy, R.aLerp);
    float3 B = glm::lerp(vz, vw, R.bLerp);
    float3 C = glm::lerp(A, B, R.chordLerp);
    float3 ORIGIN = B;// C + (glm::normalize(A - B) * R.chordLength * 0.5f * _scale);
    float3 FRONT = R.front;
    FRONT.y = 0;
    FRONT = glm::normalize(FRONT);
    float3 UP = glm::normalize(glm::cross(R.right, FRONT));


    //float3 trail = x[R.startVertex];
    float3 vec = (p - ORIGIN) * (float)_inv;
    float x = glm::dot(FRONT, vec) * _scale;
    float y = glm::dot(UP, vec) * _scale;
    return ImVec2(_pos.x + x, _pos.y - y);
};

ImVec2 _new_gliderRuntime::project(float3 p, float3 root, float3 right, ImVec2 _pos, float _scale)
{
    float3 up = { 0, 1, 0 };
    float3 D = glm::cross(right, float3(0, 1, 0));
    D.y += 0.3f;
    up = glm::normalize(glm::cross(D, right));
    float3 vec = (p - root) * (float)_inv;
    float x = glm::dot(right, vec) * _scale;
    float y = glm::dot(up, vec) * _scale;
    return ImVec2(_pos.x + x, _pos.y - y);
}


const ImU32 col32 = ImColor(ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
const ImU32 col32R = ImColor(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
const ImU32 col32B = ImColor(ImVec4(0.0f, 0.3f, 1.0f, 1.0f));
const ImU32 col32BT = ImColor(ImVec4(0.0f, 0.0f, 1.0f, 0.5f));
const ImU32 col32G = ImColor(ImVec4(0.0f, 0.6f, 0.0f, 1.0f));
const ImU32 col32GR = ImColor(ImVec4(0.4f, 0.4f, 0.4f, 0.2f));
const ImU32 col32YEL = ImColor(ImVec4(0.4f, 0.4f, 0.0f, 1.0f));
const ImU32 col32YEL_A = ImColor(ImVec4(0.7f, 0.7f, 0.0f, 0.3f));
const ImU32 col32W = ImColor(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
const ImU32 col32W_A = ImColor(ImVec4(0.7f, 0.7f, 0.7f, 0.01f));



void _new_gliderRuntime::renderDebug_ROPE(Gui* pGui, float2 _screen)
{
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.02f, 0.01f, 0.01f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 2));
    ImGui::BeginChildFrame(119865, ImVec2(_screen.x, _screen.y));
    ImGui::PushFont(pGui->getFont("roboto_20"));
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();


        static float ROT = 1.f;
        static float3 VIEW = { -0.707, 0.0, -0.707 };
        static ImVec2 OFFSET = ImVec2(1300, 900);
        static float SCALE = 160.f;
        int numSteps = 0;

        ImGui::SetCursorPos(ImVec2(0, 700));
        ImGui::NewLine();
        ImGui::Text("ROOT %d, %d, %d", (int)ROOT.x, (int)ROOT.y, (int)ROOT.z);

        ImGui::Text("i_g_dt %d", i_g_dt);
        ImGui::Text("num_vtx_line %d", num_vtx_line);

        ImGui::SetNextItemWidth(300);
        ImGui::DragInt("SMALL_LINE_SMOOTH", &SMALL_LINE_SMOOTH, 0.1f, 1, 100);


        ImGui::SetNextItemWidth(300);
        if (ImGui::DragFloat("ROT", &ROT, 0.01f, 0, 6))
        {
            VIEW = float3(sin(ROT), 0.0, -cos(ROT));
        }

        ImGui::SetNextItemWidth(300);
        ImGui::DragFloat("SCALE", &SCALE, 20.0f, 100, 3000);

        ImGui::SetNextItemWidth(300);
        ImGui::DragFloat("OFFSET Y", &OFFSET.y, 50.0f, 100, 8000);

        static bool PLAY = true;
        ImGui::Checkbox("play", &PLAY);
        if (PLAY || ImGui::Button("Step")) {
            for (int i = 0; i < 1; i++)
            {
                solve(0.001f, 1);
                numSteps++;
            }
        }

        if (ImGui::Button("Restart")) { importBin(); numSteps = 0; }


        float3 start = float3(x[0].x * _inv, x[0].y * _inv, x[0].z * _inv) + ROOT;
        ImGui::Text("start %2.2f, %2.2f, %2.2f", start.x, start.y, start.z);


        int last = x.size() - 1;
        float Fsum = 0;
        float totalLength = 0;
        for (auto& C : constraints)
        {
            if ((C.idx_0 == 0) || (C.idx_1 == 0))
                //if ((C.idx_0 == 10))
            {
                Fsum += C.force;
                ImGui::Text("%2.3f N  %s", C.force, C.name);
            }

            if (C.type == constraintType::smallLines)
            {
                totalLength += C.old_L;
            }
        }

        ImGui::Text("%2.3f N total Force at START", Fsum);
        ImGui::NewLine();

        float ropeWeight = 0;
        for (int i = 1; i < last - 1; i++)
        {
            ropeWeight += 1.0f / w[i];
        }
        ImGui::Text("%2.3f kg rope Weight", ropeWeight);
        ImGui::Text("%2.3f kg end Weight", 1.0f / w[last]);

        float ropeWeightfromC = 0;
        for (auto& C : constraints)
        {
            float maxW = __max(C.w0, C.w1);
            ropeWeightfromC += 1.0f / maxW;
        }
        ImGui::Text("%2.3f kg rope Weight calculated from constraint weights, so actual ??? How to ignore the ends", ropeWeightfromC);



        ImGui::NewLine();
        ImGui::Text("%2.3f m stretch  %2.6f %%", totalLength, (totalLength - rope_L) / rope_L * 100.f);

        // Stetch test
        static float N_stretch[1001];
        static ImVec2 F[1001];
        static bool testing = false;
        static int testPos = 0;
        static float stretch = 0;
        {
            ImGui::SetCursorPos(ImVec2(0, 50));
            ImGui::NewLine();
            ImGui::Text("Stretch Test");
            if (ImGui::Button("Start Stretch")) { testing = true; testPos = 0; }

            if (testing)
            {
                stretch = (float)testPos / 1000.f;
                rope_End = glm::lerp(rope_Start + float3(rope_L, 0, 0), rope_Start + float3(rope_L * 1.04f, 0, 0), stretch) - ROOT;
                rope_i_end = glm::ivec3(rope_End.x * _scale, rope_End.y * _scale, rope_End.z * _scale);
                for (int i = 0; i < 1000; i++)
                {
                    solve(0.001f, 1);
                    numSteps++;
                }

                float Fsum = 0;
                for (auto& C : constraints)
                {
                    if ((C.idx_0 == 0) || (C.idx_1 == 0))
                    {
                        Fsum += C.force;
                    }
                }
                N_stretch[testPos] = Fsum;
                F[testPos] = ImVec2((float)(200 + testPos), (float)(500 - Fsum));

                testPos++;
                if (testPos >= 1001) testing = false;
            }


            ImGui::SetNextItemWidth(800);
            ImGui::DragFloat("Stretch", &stretch, 0.01f, -2.f, 2.f);
            rope_End = glm::lerp(rope_Start + float3(rope_L, 0, 0), rope_Start + float3(rope_L * 1.04f, 0, 0), stretch) - ROOT;
            rope_i_end = glm::ivec3(rope_End.x * _scale, rope_End.y * _scale, rope_End.z * _scale);
            rope_End += ROOT;

            ImGui::DragFloat("addedForceEnd", &addedForceEnd, 1.0f, 0, 1000);
            ImGui::Checkbox("grabEnd", &grabEnd);
            ImGui::Checkbox("testGround", &testGround);


            draw_list->AddPolyline(F, 1001, col32R, false, 2.5f);

            ImGui::Text("start %2.3f, %2.3f, %2.3f m", rope_Start.x, rope_Start.y, rope_Start.z);
            ImGui::Text("end   %2.3f, %2.3f, %2.3f m", rope_End.x, rope_End.y, rope_End.z);
            ImGui::Text("rope length  %2.3f N", rope_L);


            ImGui::Text("0%%  %2.3f N", N_stretch[0]);
            ImGui::Text("1%%  %2.3f N", N_stretch[250]);
            ImGui::Text("2%%  %2.3f N", N_stretch[500]);
            ImGui::Text("3%%  %2.3f N", N_stretch[750]);
            ImGui::Text("4%%  %2.3f N", N_stretch[999]);
        }

        rope_i_end = glm::ivec3(rope_End.x * _scale, rope_End.y * _scale, rope_End.z * _scale);


        for (auto& C : constraints)
        {
            float width = 1.f;

            ImVec2 a = project(x[C.idx_0], glm::ivec3(0, 0, 0), VIEW, OFFSET, SCALE);
            ImVec2 b = project(x[C.idx_1], glm::ivec3(0, 0, 0), VIEW, OFFSET, SCALE);

            float scale = saturate(C.force / 30);
            draw_list->AddLine(a, b, ImColor(ImVec4(scale, 0.01 + scale, 0.0f, 1.0f)), 1.0f);
        }

        int idx = 0;
        for (auto& C : constraints)
        {
            float width = 1.f;

            ImVec2 a = project(x[C.idx_0], glm::ivec3(0, 0, 0), VIEW, OFFSET, SCALE);
            ImVec2 b = project(x[C.idx_1], glm::ivec3(0, 0, 0), VIEW, OFFSET, SCALE);

            //if (C.type == constraintType::lines || C.type == constraintType::smallLines || C.type == constraintType::straps || C.type == constraintType::skin)
            if (C.type == constraintType::smallLines)
            {
                int3 vint = x[C.idx_1] - x[C.idx_0];
                float3 vec = { vint.x * _inv, vint.y * _inv , vint.z * _inv };
                float L = glm::length(vec);
                float dl = (L - C.l) / L;   // Have to devide by L because we scale by vec which is L long
                if (dl < -0.00f)
                {
                    float scale = -dl * 30;
                    draw_list->AddLine(a, b, ImColor(ImVec4(scale, 0.0f, 1.f - scale, 1.0f)), 2.5f);
                }
                else
                {
                    draw_list->AddLine(a, b, col32BT, 2.5f);
                }


                idx++;
            }


        }

        for (int i = 0; i < x.size(); i++)
        {
            auto& X = x[i];
            ImVec2 a = project(X, glm::ivec3(0, 0, 0), VIEW, OFFSET, SCALE);
            draw_list->AddCircle(a, 5, col32BT);

            if (friction[i] == 1)   draw_list->AddCircle(a, 7, col32R);
            if (friction[i] == 2)   draw_list->AddCircleFilled(a, 5, col32GR );
        }
    }

    ImGui::EndChildFrame();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopFont();
}



void _new_gliderRuntime::renderDebug(Gui* pGui, float2 _screen)
{

    renderDebug_ROPE(pGui, _screen);
    return;

    ImVec2 start = { 100.f, 100.f };
    ImVec2 end = { 200.f, 200.f };

    ImVec2 chord[100];  // just big enough
    ImVec2 chord_cp[100];  // just big enough
    ImVec2 p[100];  // just big enough

    ImVec2 AoA[900];  // just big enough
    ImVec2 AoB[900];  // just big enough
    ImVec2 spanCp[900];  // just big enough
    ImVec2 V[900];  // just big enough
    ImVec2 CL[900];  // just big enough
    ImVec2 CD[900];  // just big enough

    static int viewChord = 100;
    static bool ShowLines = true;
    static bool ShowConstraints = false;
    static bool ShowCP = false;
    static int debugConstraint = 18800;
    static int debugVertex = 5000;
    static int NUMREPEATS = 1;
    static int numSteps = 0;

    //enum constraintType { skin, ribs, half_ribs, vecs, diagonals, nitinol, lines, straps, harnass, smallLines, maxCTypes };
    static bool show_C_skin = false;
    static float ROT = 1.f;
    static float3 VIEW = { -0.707, 0, -0.707 };
    static ImVec2 OFFSET = ImVec2(1300, 900);
    static float SCALE = 160.f;
    static int3 BASE;


    static float CP_dbg[1024];
    ImVec2 dbg_C[1024];  // just big enough


    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.95f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 2));
    ImGui::BeginChildFrame(119865, ImVec2(_screen.x, _screen.y));
    ImGui::PushFont(pGui->getFont("roboto_20"));
    {

        //;ImGui::NewLine();
        //ImGui::Text("_new_gliderRuntime");
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        /*

        // GRAPHS ########################################################
        {
            float x_space = 700.f / (float)(ribs.size() - 1);
            for (int i = 0; i < ribs.size(); i++)
            {
                AoA[i] = ImVec2(50 + i * x_space, 200 - ribs[i].AoA * 500.f);
                AoB[i] = ImVec2(50 + i * x_space, 200 + ribs[i].AoBrake * 200.f);
                spanCp[i] = ImVec2(50 + i * x_space, 200 - ribs[i].Cp * 100.f);
                V[i] = ImVec2(50 + i * x_space, 200 - ribs[i].cellVelocity * 10.f);


                CL[i] = ImVec2(50 + i * x_space, 200 - ribs[i].CL / ribs[i].AREA * 50.f);
                CD[i] = ImVec2(50 + i * x_space, 200 - ribs[i].CD / ribs[i].AREA * 500.f);
            }

            draw_list->AddLine(ImVec2(50, 200), ImVec2(750, 200), col32GR);
            draw_list->AddLine(ImVec2(50, 100), ImVec2(50, 300), col32GR);
            draw_list->AddLine(ImVec2(750, 100), ImVec2(750, 300), col32GR);
            float chordLine = 50 + 700.f * ((float)viewChord / (float)(ribs.size() - 1));
            draw_list->AddLine(ImVec2(chordLine, 100), ImVec2(chordLine, 300), col32W);

            draw_list->AddPolyline(AoA, ribs.size(), col32R, false, 2);
            draw_list->AddPolyline(AoB, ribs.size(), col32G, false, 2);
            draw_list->AddPolyline(spanCp, ribs.size(), col32B, false, 2);

            draw_list->AddPolyline(CL, ribs.size(), col32W, false, 2);
            draw_list->AddPolyline(CD, ribs.size(), col32W, false, 2);
        }


        // show hide ##########################################################################
        ImGui::SetCursorPos(ImVec2(0, _screen.y - 350));
        {
            //ImGui::Checkbox("lines", &ShowLines);
            //ImGui::Checkbox("constraints", &ShowConstraints);
            //ImGui::Checkbox("Cp", &ShowCP);
        }

        ImGui::PushFont(pGui->getFont("roboto_32"));
        ImGui::SetCursorPos(ImVec2(_screen.x / 2 - 400, 10));
        ImGui::SetNextItemWidth(800);
        ImGui::DragInt("rib", &viewChord, 0.1f, 0, ribs.size() - 1);
        ImGui::PopFont();

        if (ShowCP)
        {
            ImGui::SetCursorPos(ImVec2(_screen.x - 800, _screen.y - 400));
            ImGui::BeginChildFrame(154322, ImVec2(700, 300));
            ImGui::PushFont(pGui->getFont("roboto_26"));
            {
                static int _A = 0;
                static int _B = 0;
                ImGui::DragInt("Cp - AoA", &_A, 0.1f, 0, cp_AoA - 1);
                ImGui::DragInt("Cp - AoB", &_B, 0.1f, 0, cp_Flaps - 1);
                for (int j = 0; j < cp_V; j++)
                {
                    AoA[j] = ImVec2(_screen.x - 800 + j * 5.f, _screen.y - 400 + 200 + Cp[(_B * cp_V * cp_AoA) + (_A * cp_V) + j] * 50.f);

                    if (j > cp_V / 2)
                    {
                        AoA[j].x = _screen.x - 800 + (cp_V - 1 - j) * 5.f;
                    }
                    float smp = sampleCp((float)_A / 57.2958f, (float)_B * 5.f / 57.2958f, (float)j / 115.f);
                }
                draw_list->AddLine(ImVec2(_screen.x - 800 + 337, 0), ImVec2(_screen.x - 800 + 337, 400), col32GR, 5);
                draw_list->AddLine(ImVec2(_screen.x - 800, _screen.y - 400 + 200), ImVec2(_screen.x - 800 + 700, _screen.y - 400 + 200), col32GR, 5);
                draw_list->AddPolyline(AoA, cp_V, col32W, false, 2);
            }
            ImGui::PopFont();
            ImGui::EndChildFrame();
        }





        auto& R = ribs[viewChord];


        //if (!ShowLines)
        {
            ImGui::SetCursorPos(ImVec2(_screen.x - 350, 800));
            ImGui::BeginChildFrame(139865, ImVec2(300, 700));
            ImGui::PushFont(pGui->getFont("roboto_26"));
            {
                ImGui::Text("Rib %d", viewChord);
                ImGui::Text("chord %2.2fm", R.chordLength);
                ImGui::Text("V %2.2f (%2.2f, %2.2f, %2.2f) m/s", glm::length(R.v), R.v.x, R.v.y, R.v.z);
                ImGui::Text("front (%2.2f, %2.2f, %2.2f) m/s", R.front.x, R.front.y, R.front.z);
                ImGui::Text("up     (%2.2f, %2.2f, %2.2f) m/s", R.up.x, R.up.y, R.up.z);
                ImGui::Text("Rho %2.2f pascals", R.rho);
                ImGui::Text("Reynolds %2.2f M", R.reynolds * 0.000001f);
                int firstC = 0;
                for (int i = 0; i < constraints.size(); i++)
                {
                    if (constraints[i].idx_0 == R.startVertex || constraints[i].idx_1 == R.startVertex)
                    {
                        firstC = i;
                        break;// fuind teh first one
                    }
                }
                ImGui::Text("C-trail %d", firstC);
                ImGui::DragInt("debug C #", &debugConstraint, 0.3f, 0, (int)constraints.size() - 1);

                ImGui::DragInt("debug Vertex", &debugVertex, 1, 0, (int)x.size() - 1);
                debugVertex = clamp(debugVertex, 0, (int)x.size() - 1);

                ImGui::Text("%d [%d], %d, %d", debugVertex, type[debugVertex], cross_idx[debugVertex].x, cross_idx[debugVertex].y);

                ImGui::NewLine();

#ifdef DEBUG_GLIDER
                ImGui::Text("surface %2.2f N", glm::length(R._debug.forcePressure));
                ImGui::Text("skin    %2.2f N", glm::length(R._debug.forceSkin));
                ImGui::Text("gravity %2.2f N", glm::length(R._debug.forceGravity));
                ImGui::Text("internal%2.2f N", glm::length(R._debug.forceInternal));

                float A = 0;
                float M = 0;
                for (int j = R.startVertex; j < R.startVertex + R.numVerts; j++)
                {
                    A += area[j];
                    M += 1.f / w[j];
                }
                ImGui::Text("area %2.2f m^2", A);
                ImGui::Text("mass %2.2f g", M * 1000.f);

#endif
            }
            ImGui::PopFont();
            ImGui::EndChildFrame();
        }


        {
            ImGui::SetCursorPos(ImVec2(_screen.x - 350, 50));
            ImGui::BeginChildFrame(139867, ImVec2(300, 700));
            ImGui::PushFont(pGui->getFont("roboto_20"));
            {
                ImGui::Text("F %2.2f N (%2.2f, %2.2f, %2.2f)", glm::length(debug_wing_F), debug_wing_F.x, debug_wing_F.y, debug_wing_F.z);
                ImGui::Text("skin %2.2f N (%2.2f, %2.2f, %2.2f)", glm::length(debug_wing_Skin), debug_wing_Skin.x, debug_wing_Skin.y, debug_wing_Skin.z);


#ifdef DEBUG_GLIDER
                ImGui::Text("area %2.2f m^2", wingArea);
                ImGui::Text("SUM area %2.4f m^2 (%2.2f, %2.2f, %2.2f)", glm::length(sumWingArea), sumWingArea.x, sumWingArea.y, sumWingArea.z);
                ImGui::Text("SUM N %2.4f, %2.4f ", glm::length(sumWingN), glm::length(sumWingNAll));

                ImGui::Text("skin %2.2f kg", mass[vtx_surface]);
                ImGui::Text("internal %2.2f kg", mass[vtx_internal]);
                ImGui::Text("line %2.2f kg", mass[vtx_line]);
                ImGui::Text("small %2.2f kg", mass[vtx_small_line]);
                ImGui::Text("drag line %2.2f N (%2.2f, %2.2f, %2.2f)", glm::length(lineDrag), lineDrag.x, lineDrag.y, lineDrag.z);
#endif


                ImGui::NewLine();
                ImGui::Text("wing - %2.1fus ", time_air_us);
                ImGui::Text("internal - %2.1fus ", time_internal_us);
                ImGui::Text("pre - %2.1fus ", time_pre_us);
                ImGui::Text("constraints - %2.1fus [%dx] %d constraints", time_constraints_us, NUMREPEATS, NUMREPEATS * (int)constraints.size());
                ImGui::Text("acurate N - %2.1fus", time_norms_us);


                // Force in teh carabiners
                float carabinerLeftNewton = 0;
                float carabinerRightNewton = 0;
                float brakeLeftNewton = 0;
                float brakeRightNewton = 0;
                int sumB = 0;

                for (auto& C : constraints)
                {
                    if (C.idx_0 == carabinerLeft || C.idx_1 == carabinerLeft)
                    {
                        carabinerLeftNewton += C.force;
                        sumB++;
                    }
                    if (C.idx_0 == carabinerRight || C.idx_1 == carabinerRight)
                    {
                        carabinerRightNewton += C.force;
                        sumB++;
                    }

                    if (C.idx_0 == brakeLeft || C.idx_1 == brakeRight)
                    {
                        brakeLeftNewton += C.force;
                    }
                    if (C.idx_0 == brakeLeft || C.idx_1 == brakeRight)
                    {
                        brakeRightNewton += C.force;
                    }
                }
                ImGui::Text("Carabiner Force");
                ImGui::Text("%2.2f N (%2.2f, %2.2f) [%d]", carabinerLeftNewton + carabinerRightNewton, carabinerLeftNewton, carabinerRightNewton, sumB);
                ImGui::Text("Brake Force");
                ImGui::Text("%2.2f N (%2.2f, %2.2f)", brakeLeftNewton + brakeRightNewton, brakeLeftNewton, brakeRightNewton);
            }
            ImGui::PopFont();
            ImGui::EndChildFrame();
        }
        */

        /*
        static int dbg_idx = 0;
        if (debugConstraint > 0)
        {
            for (int j = 0; j < 1024; j++)
            {
                dbg_C[j] = ImVec2(100.f + j, 900.f - CP_dbg[(dbg_idx + j) % 1024] * 10000.f);
            }
            draw_list->AddLine(ImVec2(100, 900), ImVec2(1124, 900), col32W);
            draw_list->AddLine(ImVec2(100, 700), ImVec2(1124, 700), col32G);
            draw_list->AddLine(ImVec2(100, 1100), ImVec2(1124, 1100), col32G);
            draw_list->AddPolyline(dbg_C, 1024, col32G, false, 1);

            ImGui::SetCursorPos(ImVec2(1124, 900));
            switch (constraints[debugConstraint].type)
            {
            case constraintType::skin:  ImGui::Text("skin"); break;
            case constraintType::ribs:  ImGui::Text("ribs"); break;
            case constraintType::half_ribs:  ImGui::Text("half_ribs"); break;
            case constraintType::vecs:  ImGui::Text("vecs"); break;
            case constraintType::diagonals:  ImGui::Text("diagonals"); break;
            case constraintType::nitinol:  ImGui::Text("nitinol"); break;
            case constraintType::lines:  ImGui::Text("lines"); break;
            case constraintType::straps:  ImGui::Text("straps"); break;
            case constraintType::harnass:  ImGui::Text("harnass"); break;
            case constraintType::smallLines:  ImGui::Text("smallLines"); break;
            }
            ImGui::SameLine(0, 50);
            ImGui::Text("%2.2f, %2.2f", constraints[debugConstraint].force, (constraints[debugConstraint].force / constraints[debugConstraint].l - 1.f) * 100.f);
        }
        */




        float ribScale = 800;
        if (!ShowLines)
        {
            /*
            for (int i = 0; i < R.numVerts; i++)
            {
                float3 p = x[R.startVertex + i];
                chord[i] = project(p, R, ImVec2(1500, 500), ribScale);

                //float3 c1 = x[cross_idx[R.startVertex + i].x] - x[cross_idx[R.startVertex + i].y];  //BUG us acureate
                //float3 c2 = x[cross_idx[R.startVertex + i].z] - x[cross_idx[R.startVertex + i].w];
                float3 n = glm::normalize(N[i]);
                float3 t = glm::normalize(R.flow_direction - glm::dot(R.flow_direction, n) * n);


                int r = viewChord;
                int Vaoa = glm::clamp(6 + (int)(R.AoA * 57.f), 0, 35);
                int v = (int)(127.f * uv[R.startVertex + i].y);
                float skinCp = sampleCp(R.AoA, R.AoBrake, uv[R.startVertex + i].y);
                float vCp = R.Cp - skinCp;// Cp[Vaoa * 128 + v];
                //float vCp_only = -Cp[Vaoa * 128 + v];

                ImVec2 pn = project(p, R, ImVec2(1500, 500), ribScale);
                ImVec2 pn_acc = project(p + N[R.startVertex + i] * (float)_scale * 20.1f * vCp, R, ImVec2(1500, 500), ribScale);
                ImVec2 pn_acc_CP = project(p + N[R.startVertex + i] * (float)_scale * 20.1f * (-skinCp), R, ImVec2(1500, 500), ribScale);
                ImVec2 pt = project(p + t * (float)_scale * 10.1f, R, ImVec2(1500, 500), ribScale);
                //draw_list->AddLine(chord[i], pn, col32B, 1);

                draw_list->AddLine(chord[i], pn_acc_CP, col32YEL, 3);
                draw_list->AddLine(chord[i], pn_acc, col32B, 1);
                //draw_list->AddLine(chord[i], pt, col32YEL, 1);

                float tempQ = 50.f;       // dynamic pressure 1/2 rho v^2
                float tempCp = 2.f;       // (Cp_internal - Cp)
                float tempCd_surface = 0.0039f;
                float scale = (float)_time_scale;
                int an = (int)(area_x_w[R.startVertex + i] * tempCp * tempQ * scale);           //area_x_w is ZERO
                int at = (int)(area_x_w[R.startVertex + i] * tempCd_surface * tempQ * scale);

                ImGui::SetCursorPos(pn);
                ImGui::Text("%2.3f", uv[R.startVertex + i].y);
                // constraints
                if (ShowConstraints)
                {
                    int counter = 0;
                    int Ccount = 0;
                    for (auto& C : constraints)
                    {
                        if (Ccount == debugConstraint)
                        {
                            ImVec2 a = project(x[C.idx_0], R, ImVec2(1500, 500), ribScale);
                            ImVec2 b = project(x[C.idx_1], R, ImVec2(1500, 500), ribScale);
                            draw_list->AddLine(a, b, col32W, 3);
                        }

                        //if (C.type == constraintType::ribs || C.type == constraintType::half_ribs)
                        if (C.type != constraintType::skin)
                        {
                            if (C.idx_0 == R.startVertex + i || C.idx_1 == R.startVertex + i)
                            {
                                ImVec2 a = project(x[C.idx_0], R, ImVec2(1500, 500), ribScale);
                                ImVec2 b = project(x[C.idx_1], R, ImVec2(1500, 500), ribScale);

                                int3 vint = x[C.idx_1] - x[C.idx_0];
                                float3 vec = { vint.x * _inv, vint.y * _inv , vint.z * _inv };
                                float L = glm::length(vec);
                                float dl = (L - C.l) / L;   // Have to devide by L because we scale by vec which is L long
                                if (dl > 0)
                                {
                                    draw_list->AddLine(a, b, col32YEL, 3);
                                }
                                else
                                {
                                    draw_list->AddLine(a, b, col32G, 3);
                                }
                                if (abs(dl) > 0.01f)
                                {
                                    ImGui::SetCursorPos(ImVec2((a.x + b.x) / 2, (a.y + b.y) / 2));
                                    ImGui::Text("%2.1f", dl * 100.f, C.w_rel_0, C.w_rel_1);
                                }
                                //
                                //float3 d_A = (dl * C.w_0 * (float)_scale) * vec;
                                //float3 d_B = (dl * C.w_1 * (float)_scale) * vec;
                                //int3 A = x[C.idx_0] + int3(d_A);
                                //int3 B = x[C.idx_1] - int3(d_B);

                                //a = project(A, R, ImVec2(50, 500), 1000.f);
                                //b = project(B, R, ImVec2(50, 500), 1000.f);
                                //draw_list->AddCircle(a, 5, col32B);
                                //draw_list->AddCircle(b, 5, col32G);

                                counter++;
                            }
                        }


                        Ccount++;
                    }
                }

            }


            static int Vaoa = 0;
            //ImGui::DragInt("aoa", &Vaoa, 0.1f, 0, 35);
            Vaoa = 6 + (int)(R.AoA * 57.f);
            Vaoa = __max(0, Vaoa);
            Vaoa = __min(35, Vaoa);

            for (int i = 0; i < R.numVerts; i++)
            {
                float3 p = x[R.startVertex + i];
                int v = (int)(127.f * uv[R.startVertex + i].y);
                float skinCp = sampleCp(R.AoA, R.AoBrake, uv[R.startVertex + i].y);
                chord_cp[i] = project(p, R, ImVec2(1500, 500), ribScale);
                chord_cp[i].y = 1100 + 300 * (skinCp);
            }
            draw_list->AddLine(ImVec2(50, 1100), ImVec2(2500, 1100), col32GR, 2);
            draw_list->AddPolyline(chord_cp, R.numVerts, col32R, false, 2);




            float3 vx = x[R.rightIdx.x];
            float3 vy = x[R.rightIdx.y];
            float3 vz = x[R.rightIdx.z];
            float3 vw = x[R.rightIdx.w];
            float3 trail = x[R.startVertex];
            float3 A = glm::lerp(vx, vy, R.aLerp);
            float3 B = glm::lerp(vz, vw, R.bLerp);
            float3 C = glm::lerp(A, B, R.chordLerp);


#ifdef DEBUG_GLIDER

            draw_list->AddLine(project(B, R, ImVec2(1500, 500), ribScale), project(B + R._debug.forcePressure * (float)_scale * 0.1f, R, ImVec2(1500, 500), ribScale), col32GR, 8);
            draw_list->AddLine(project(B, R, ImVec2(1500, 500), ribScale), project(B + R._debug.forceSkin * (float)_scale * 0.1f, R, ImVec2(1500, 500), ribScale), col32YEL, 4);
            draw_list->AddLine(project(B, R, ImVec2(1500, 500), ribScale), project(B + R._debug.forceGravity * (float)_scale * 0.1f, R, ImVec2(1500, 500), ribScale), col32W, 4);
            draw_list->AddLine(project(B, R, ImVec2(1500, 500), ribScale), project(B + R._debug.forceInternal * (float)_scale * 0.1f, R, ImVec2(1500, 500), ribScale), col32B, 4);
#endif

            float3 front = C + 8.f * (A - C);
            draw_list->AddLine(project(C, R, ImVec2(1500, 500), ribScale), project(front, R, ImVec2(1500, 500), ribScale), col32GR, 2);
            draw_list->AddLine(project(C, R, ImVec2(1500, 500), ribScale), project(trail, R, ImVec2(1500, 500), ribScale), col32G, 2);

            float3 wind = -R.flow_direction * (float)_scale;
            draw_list->AddLine(project(B, R, ImVec2(1500, 500), ribScale), project(B + wind, R, ImVec2(1500, 500), ribScale), col32G, 2);

            float3 cellV = R.v * (float)_scale;
            draw_list->AddLine(project(B, R, ImVec2(1500, 500), ribScale), project(B + cellV, R, ImVec2(1500, 500), ribScale), col32W, 2);

            draw_list->AddCircle(project(C, R, ImVec2(1500, 500), ribScale), 5, col32W);

            draw_list->AddPolyline(chord, R.numVerts, col32R, false, 2);

            ImVec2 B2 = project(B, R, ImVec2(1500, 500), ribScale);
            ImGui::SetCursorPos(ImVec2(B2.x - 50, B2.y - 20));
            ImGui::Text("AoA %2.1fº %2.1fm/s", R.AoA * 57.f, R.cellVelocity);

            ImGui::SetCursorPos(ImVec2(B2.x - 50, B2.y + 5));
            ImGui::Text("Cp %2.2f", R.Cp);

            ImVec2 C2 = project(C, R, ImVec2(1500, 500), ribScale);
            ImGui::SetCursorPos(ImVec2(C2.x + 8, C2.y - 20));
            ImGui::Text("AoB %2.1fº", R.AoBrake * 57.f);

            */
        }





        ImGui::SetCursorPos(ImVec2(0, 700));
        ImGui::NewLine();
        ImGui::Text("ROOT %d, %d, %d", (int)ROOT.x, (int)ROOT.y, (int)ROOT.z);

        ImGui::Text("i_g_dt %d", i_g_dt);
        ImGui::Text("num_vtx_line %d", num_vtx_line);
        ImGui::Text("%d steps, %2.2f s", numSteps, (float)numSteps * (float)_dt);

        ImGui::SetNextItemWidth(300);
        ImGui::DragInt("Pull depth", &PULL_DEPTH);

        ImGui::SetNextItemWidth(300);
        ImGui::DragFloat("AoA_smooth", &AoA_smooth, 0.01f, 0, 1);
        ImGui::SetNextItemWidth(300);
        ImGui::DragFloat("AoB_smooth", &AoB_smooth, 0.01f, 0, 1);

        ImGui::SetNextItemWidth(300);
        ImGui::DragInt("NUMREPEATS", &NUMREPEATS, 0.1f, 1, 50);

        ImGui::SetNextItemWidth(300);
        ImGui::DragInt("SMALL_LINE_SMOOTH", &SMALL_LINE_SMOOTH, 0.1f, 1, 20);


        ImGui::SetNextItemWidth(300);
        if (ImGui::DragFloat("ROT", &ROT, 0.01f, 0, 6))
        {
            VIEW = float3(sin(ROT), 0, -cos(ROT));
        }

        ImGui::SetNextItemWidth(300);
        ImGui::DragFloat("SCALE", &SCALE, 20.0f, 100, 3000);

        ImGui::SetNextItemWidth(300);
        ImGui::DragFloat("OFFSET Y", &OFFSET.y, 50.0f, 100, 8000);



        static bool PLAY = false;
        ImGui::Checkbox("play", &PLAY);
        if (PLAY || ImGui::Button("Step")) {
            for (int i = 0; i < 11; i++)
            {
                //solveAcurateSkinNormals();
                solve(0.001f, NUMREPEATS);
                numSteps++;

                /*
                if (debugConstraint > 0)
                {
                    //CP_dbg[dbg_idx] = constraints[debugConstraint].current_L / constraints[debugConstraint].l - 1.f;
                    dbg_idx++;
                    dbg_idx %= 1024;
                }
                */
            }
        }

        ImGui::Checkbox("lines", &ShowLines);
        ImGui::Checkbox("constraints", &ShowConstraints);
        ImGui::Checkbox("Cp", &ShowCP);

        if (ImGui::Button("Constraints")) {
            this->solve_constraints();
        }

        if (ImGui::Button("Restart")) {
            importBin();
            numSteps = 0;
        }

        if (ImGui::Button("EXPORT")) {
            toObj("E:/SIM_lines.obj", "obj", constraintType::smallLines);
            toObj("E:/SIM_skin.stl", "stl", constraintType::skin);
            toObj("E:/SIM_skin.stp", "stp", constraintType::skin);
        }
        if (ImGui::Button("XFOIL")) {
            //toXFoil("", viewChord);   // 268 ija;f rib wiyth line
        }

        /*
        //int xfoil_ribnum[3] = { 104, 105, 106 };
        int xfoil_ribnum[3] = { 52, 53, 53 };
        //static int xfoil_AoB[3] = {0, 0, 0};
        for (int i = 0; i < 3; i++)
        {
            float aob = ribs[xfoil_ribnum[i]].AoBrake * 57.f;
            int iaoB = (int)aob;
            if ((iaoB % 5 == 0) && aob - iaoB < 0.1f)
            {
                std::string xfoilPath = "E:/terrains/_resources/xfoil/Omega/rib_";
                xfoilPath += std::to_string(xfoil_ribnum[i]) + "_" + std::to_string(iaoB) + ".txt";
                toXFoil(xfoilPath, xfoil_ribnum[i]);

                for (int aoB = 0; aoB <= 30; aoB += 5)
                {
                    xfoilPath = "E:/terrains/_resources/xfoil/Solve_Omega_";
                    xfoilPath += std::to_string(xfoil_ribnum[i]) + "_" + std::to_string(aoB) + ".txt";
                    toXFoil_scrip(xfoilPath, xfoil_ribnum[i], aoB);
                }

                //xfoil_AoB[i]++;
            }

        }
        */










        // Lines
        if (ShowLines)
        {
            /*
            for (int i = 0; i < R.numVerts; i++)
            {
                float3 p = x[R.startVertex + i];
                chord[i] = project(p, posFixed[0], VIEW, OFFSET, SCALE);
            }
            draw_list->AddPolyline(chord, R.numVerts, col32YEL, false, 2);

            // My 4 fixed points
            for (int i = 0; i < 4; i++)
            {
                ImVec2 a = project(posFixed[i], posFixed[0], VIEW, OFFSET, SCALE);
                draw_list->AddCircle(a, 5, col32W);
            }

            {
                ImVec2 a = project(x[debugVertex], posFixed[0], VIEW, OFFSET, SCALE);
                ImVec2 b = project(x[cross_idx[debugVertex].x], posFixed[0], VIEW, OFFSET, SCALE);
                ImVec2 c = project(x[cross_idx[debugVertex].y], posFixed[0], VIEW, OFFSET, SCALE);

                draw_list->AddCircle(a, 8, col32G);
                draw_list->AddCircle(b, 5, col32W);
                draw_list->AddCircle(c, 5, col32W);

                ImGui::SetCursorPos(a);

                ImGui::Text("w - %2.4f", w[debugVertex]);


            }
            */

            int idx = 0;
            for (auto& C : constraints)
            {
                bool cR = false;
                bool bR = false;
                if (C.idx_0 == carabinerRight || C.idx_1 == carabinerRight) cR = true;
                if (C.idx_0 == brakeRight || C.idx_1 == brakeRight) bR = true;

                float width = 1.f;
                /*
                if ((cR || bR) && C.type == constraintType::smallLines)
                {
                    width = 5.f;
                    ImVec2 a = project(x[C.idx_0], posFixed[0], VIEW, OFFSET, SCALE);
                    ImVec2 b = project(x[C.idx_1], posFixed[0], VIEW, OFFSET, SCALE);
                    draw_list->AddLine(a, b, col32W, width);
                }


                if (idx == debugConstraint)
                {
                    ImVec2 a = project(x[C.idx_0], posFixed[0], VIEW, OFFSET, SCALE);
                    ImVec2 b = project(x[C.idx_1], posFixed[0], VIEW, OFFSET, SCALE);
                    draw_list->AddLine(a, b, col32W, 3);

                    ImGui::SetCursorPos(a);
                    ImGui::Text(".    .    .    .    .    %2.2f N,  %2.5f m, W(%2.1f %2.1f). WC(%2.2f %2.2f), t - %2.1f, c - %2.1f", C.force, C.l - C.old_L, w[C.idx_0], w[C.idx_1], C.w_rel_0, C.w_rel_1, C.tensileStiff, C.compressStiff);
                }
                */

                //if (C.type == constraintType::lines || C.type == constraintType::smallLines || C.type == constraintType::straps || C.type == constraintType::skin)
                if (C.type == constraintType::straps)
                    //if (C.type == constraintType::lines)
                {
                    ImVec2 a = project(x[C.idx_0], posFixed[0], VIEW, OFFSET, SCALE);
                    ImVec2 b = project(x[C.idx_1], posFixed[0], VIEW, OFFSET, SCALE);

                    /*
                    if (C.type == constraintType::lines)
                    {
                       // draw_list->AddLine(a, b, col32YEL_A, width * 2);

                        int3 vint = x[C.idx_1] - x[C.idx_0];
                        float3 vec = { vint.x * _inv, vint.y * _inv , vint.z * _inv };
                        float L = glm::length(vec);
                        float dl = (L - C.l) / L;   // Have to devide by L because we scale by vec which is L long
                        if (dl > 0.02f)
                        {
                            ImGui::SetCursorPos(ImVec2((a.x + b.x) / 2, (a.y + b.y) / 2));
                            ImGui::Text("%d", (int)(dl * 100.f));
                        }
                        if (dl > 0.0f)
                        {
                            draw_list->AddLine(a, b, col32YEL_A, width * 2);
                        }

                    }
                    else if (C.type == constraintType::smallLines)
                    {
                        draw_list->AddLine(a, b, col32R, width * 1);
                    }
                    else */ //if (C.idx_0 > 0 && C.idx_0 < 32 * 32 * 2)
                    {
                        int3 vint = x[C.idx_1] - x[C.idx_0];
                        float3 vec = { vint.x * _inv, vint.y * _inv , vint.z * _inv };
                        float L = glm::length(vec);
                        float dl = (L - C.l) / L;   // Have to devide by L because we scale by vec which is L long
                        if (dl < -0.00f)
                        {
                            float scale = -dl * 30;
                            draw_list->AddLine(a, b, ImColor(ImVec4(scale, 0.0f, 1.f - scale, 1.0f)), 0.5f);
                        }
                        else
                        {
                            draw_list->AddLine(a, b, col32BT, 0.5f);
                        }
                    }
                }

                idx++;
            }
        }
    }

    ImGui::EndChildFrame();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopFont();
}




void _new_gliderRuntime::process_xfoil_Cp(std::string _folder)
{
    cp_Flaps = 1;
    cp_AoA = 37;
    cp_V = 128;
    Cp.resize(cp_Flaps * cp_AoA * cp_V);

    std::string flapName = _folder + "profileV.txt";
    load_xfoil_V(flapName);

    for (int i = 0; i < cp_Flaps; i++)
    {
        for (int j = 0; j < cp_AoA; j += 1)
        {
            flapName = _folder + "/cp/cp_" + std::to_string(i) + "_" + std::to_string(j) + ".txt";
            //xfoil_buildCP(i, j, flapName);
            load_xfoil_Cp(i, j, flapName);
        }
    }
}


void _new_gliderRuntime::load_xfoil_V(std::string _file)
{
    float v;
    xfoil_V.clear();

    std::ifstream infile(_file);     // read the cp
    if (infile.good())
    {
        //fprintf(terrafectorSystem::_logfile, "_new_gliderRuntime::load_xfoil_V %s\n", _file.c_str());
        //fflush(terrafectorSystem::_logfile);
        while (infile >> v)
        {
            xfoil_V.push_back(v);
        }
        infile.close();
    }

    std::reverse(std::begin(xfoil_V), std::end(xfoil_V));
}

void _new_gliderRuntime::load_xfoil_Cp(int _flaps, int _aoa, std::string _file)
{
    std::string line;
    uint idxW = 0;
    uint idx = 0;
    float count = 0;
    float cp = 0;

    float allCp[500];
    float allCount[500];
    for (int i = 0; i < cp_V; i++)
    {
        allCp[i] = 0;
        allCount[i] = 0;
    }

    int V_Aoa = cp_V * cp_AoA;

    std::ifstream infile(_file);     // read the cp
    if (infile.good())
    {
        //fprintf(terrafectorSystem::_logfile, "\n_new_gliderRuntime::load_xfoil_Cp %s\n", _file.c_str());
        //fflush(terrafectorSystem::_logfile);
        std::getline(infile, line);
        std::getline(infile, line);
        std::getline(infile, line);
        float x, y, cp_in;
        float cp = 0.f;
        while (infile >> x >> y >> cp_in)
        {
            float v = xfoil_V[idx];
            int v_idx = (int)__min((float)cp_V - 1.f, v * (float)cp_V);
            allCp[v_idx] += cp_in;
            allCount[v_idx] += 1.f;
            /*
            cp += cp_in;

            if (idx >= v_idx)
            {
                fprintf(terrafectorSystem::_logfile, "%d, %f \n", v_idx, v);
                Cp[_flaps * V_Aoa  + _aoa * cp_V + (127 - v_idx)] = cp / count;
                cp = 0.f;
                count = 0.f;
            }

            count += 1.f;
            */
            idx++;

        }
        infile.close();
    }

    for (int i = 0; i < cp_V; i++)
    {
        Cp[_flaps * V_Aoa + _aoa * cp_V + i] = allCp[i] / allCount[i];
        //fprintf(terrafectorSystem::_logfile, "%d, %f \n", i, allCp[i] / allCount[i]);
    }
}

void _new_gliderRuntime::loadXFoil_Cp(std::string _folder, int _rib)
{
    cp_Flaps = 5;   // 0 .. 35 evey 5
    cp_AoA = 25;    // 0 -- 15
    cp_V = 60; // 256 / 4;
    Cp.resize(cp_Flaps * cp_AoA * cp_V);

    for (int i = 0; i < cp_Flaps; i++)
    {
        for (int j = 0; j < cp_AoA; j++)
        {
            std::string flapName = _folder + "/cp/cp_" + std::to_string(_rib) + "_AoB_" + std::to_string(i * 5) + "_AoA_" + std::to_string(j) + ".txt";
            loadXFoil_file(i, j, flapName);
        }
    }
}

void _new_gliderRuntime::loadXFoil_file(int _AoB, int _AoA, std::string _file)
{
    std::string line;
    int offset = (_AoB * cp_V * cp_AoA) + (_AoA * cp_V);

    std::ifstream infile(_file);     // read the cp
    if (infile.good())
    {
        std::getline(infile, line);
        std::getline(infile, line);
        std::getline(infile, line);
        float x, y, cp_in;

        for (int i = 0; i < 60; i++)
        {
            float cp = 0.f;
            for (int j = 0; j < 8; j++)
            {
                infile >> x >> y >> cp_in;
                cp += cp_in;
                if (cp_in > 1.01f) fprintf(terrafectorSystem::_logfile, "AAAHHH  AoA %d, AoB %d, %2.3f\n", _AoA, _AoB, cp_in);
                if (cp_in < -8.f) fprintf(terrafectorSystem::_logfile, "AAAHHH  AoA %d, AoB %d, %2.3f\n", _AoA, _AoB, cp_in);
            }
            Cp[offset + (cp_V - i - 1)] = cp / 8.f;
            //Cp[offset + i] = cp / 4.f;
        }

        infile.close();
    }
    else
    {
        fprintf(terrafectorSystem::_logfile, "NOT FOUND %s\n", _file.c_str());
    }
}


void _new_gliderRuntime::toXFoil_scrip(std::string filename, int _rib, int _aob)
{
    //for (int aoB = 0; aoB <= 20; aoB++)
    {
        std::ofstream ofs;
        ofs.open(filename);
        if (ofs)
        {
            ofs << "LOAD E:/terrains/_resources/xfoil/Omega/rib_" << _rib << "_" << _aob << ".txt\n";
            ofs << "rib_" << _rib << "_aoB_" << _aob << "\n";
            ofs << "OPER\n";
            //ofs << "Visc " << (int)ribs[_rib].reynolds << "\n";
            ofs << "Visc 2000000\n";
            ofs << "Mach 0.04\n";
            ofs << "Iter 1000\n";

            for (int j = 7; j <= 20; j++)
            {
                ofs << "a " << j << "\n";
                ofs << "CPWR E:/terrains/_resources/xfoil/Omega/cp/cp_" << _rib << "_AoB_" << _aob << "_AoA_" << j + 5 << ".txt\n";
            }
            for (int j = 7; j >= -5; j--)
            {
                ofs << "a " << j << "\n";
                ofs << "CPWR E:/terrains/_resources/xfoil/Omega/cp/cp_" << _rib << "_AoB_" << _aob << "_AoA_" << j + 5 << ".txt\n";
            }
            ofs.close();
        }
    }
}

void _new_gliderRuntime::toXFoil(std::string filename, int _rib)
{
    auto& R = ribs[_rib];
    float3 v[1024];
    float3 t[1024];
    float l[1024];
    float _uv[1024];


    std::ofstream ofs;
    //ofs.open("E:/terrains/_resources/xfoil/Omega/RIB.txt");
    ofs.open(filename);
    if (ofs)
    {
        for (int i = 0; i < R.numVerts; i++)
        {
            float3 p = x[R.startVertex + i];
            v[i] = projectXFoil(p, R);
            _uv[i] = uv[R.startVertex + i].y;

            //ofs << v[i].x << " " << v[i].y << "\n";
        }
        _uv[R.numVerts] = 1;
        v[R.numVerts - 1].y = v[0].y + 0.001f;
        v[0].y -= 0.001f;

        for (int i = 0; i < R.numVerts - 1; i++)
        {
            l[i] = glm::length(v[i + 1] - v[i]);
        }

        t[0] = glm::normalize(v[1] - v[0]);
        t[R.numVerts - 1] = glm::normalize(v[R.numVerts - 1] - v[R.numVerts - 2]);
        for (int i = 1; i < R.numVerts - 1; i++)
        {
            t[i] = glm::normalize(v[i + 1] - v[i - 1]);
        }

        //ofs << "\n\n";

        float dv = 1.f / 479.01f;
        float V_cnt = 0;
        int idx = 0;
        float3 BZ[4];
        BZ[0] = v[0];
        BZ[1] = v[0] + t[0] * l[0] * 0.25f;
        BZ[2] = v[1] - t[1] * l[0] * 0.25f;
        BZ[3] = v[1];


        while (V_cnt < 1.0f)
        {
            float dt = (V_cnt - _uv[idx]) / (_uv[idx + 1] - _uv[idx]);
            float3 A = glm::lerp(BZ[0], BZ[1], dt);
            float3 B = glm::lerp(BZ[1], BZ[2], dt);
            float3 C = glm::lerp(BZ[2], BZ[3], dt);

            float3 D = glm::lerp(A, B, dt);
            float3 E = glm::lerp(B, C, dt);

            float3 F = glm::lerp(D, E, dt);
            ofs << F.x << " " << F.y << "\n";

            V_cnt += dv;
            if (V_cnt > _uv[idx + 1])
            {
                idx++;
                BZ[0] = v[idx];
                BZ[1] = v[idx] + t[idx] * l[idx] * 0.25f;
                BZ[2] = v[idx + 1] - t[idx + 1] * l[idx] * 0.25f;
                BZ[3] = v[idx + 1];
            }
        }

        ofs.close();
    }
}


void _new_gliderRuntime::toObj(char* filename, char* _ext, constraintType _type)
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
        pMesh->mVertices = new aiVector3D[x.size()];        //??? Can two meshes share verts in assimp
        int vidx = 0;
        for (auto& V : x)
        {
            pMesh->mVertices[vidx].x = (float)((double)V.x * _inv);
            pMesh->mVertices[vidx].y = (float)((double)V.y * _inv);
            pMesh->mVertices[vidx].z = (float)((double)V.z * _inv);
            vidx++;
        }

        pMesh->mNumVertices = x.size();


        pMesh->mPrimitiveTypes = aiPrimitiveType_LINE | aiPrimitiveType_TRIANGLE;

        int fidx = 0;

        // surface
        if (_type == constraintType::skin)
        {
            pMesh->mNumFaces = tris_airtight.size();
            pMesh->mFaces = new aiFace[pMesh->mNumFaces];

            for (auto& T : tris_airtight)
            {
                aiFace& face = pMesh->mFaces[fidx];
                face.mIndices = new unsigned int[3];
                face.mNumIndices = 3;
                face.mIndices[0] = T.x;
                face.mIndices[1] = T.y;
                face.mIndices[2] = T.z;
                fidx++;
            }
        }
        else
        {
            pMesh->mNumFaces = constraints.size();  // still too much but so what
            pMesh->mFaces = new aiFace[pMesh->mNumFaces];

            for (auto& E : constraints)
            {
                if (E.type == _type)
                {
                    aiFace& face = pMesh->mFaces[fidx];
                    face.mIndices = new unsigned int[2];
                    face.mNumIndices = 2;
                    face.mIndices[0] = E.idx_0;
                    face.mIndices[1] = E.idx_1;
                    fidx++;
                }
            }
        }
    }

    Assimp::Exporter exp;

    exp.Export(scene, _ext, filename);
    //exp.Export(scene, "obj", "E:/Advance/omega_xpdb_surface.obj");



}

/*
    Velocity flow
    left - middle - right    or  per cell, likely overkill
    wing velocity and air velocity combined.
    then calc dynamic pressure.

    Wing shape calculation
    very likely per cell
    AoA, Brake, B-stall, Collapse

    Cp lookup from wing shape
    Internal pressure, per cell, and averaged / flow just 0.6 for a start
    External per vertex UV lookup
    This could move to Gaelerkin panel method in future

    Constraint solver.

    Intersection solver
    lines / surface
    top / bottom surfaces
    far surfaces - collapse
*/
