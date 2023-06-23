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






void _GroveBranch::reset()
{
    nodes.clear();
}



void _GroveTree::renderGui(Gui* _gui)
{
    ImGui::PushFont(_gui->getFont("roboto_20"));
    {
        ImGui::DragInt("##size", &numFour, 1, 0, 100);
        if (ImGui::Button("load")) {
            load();
        }
        ImGui::Text("branches - %d", branches.size());
        ImGui::Text("branch leaves - %d", branchLeaves.size());
        ImGui::Text("# ribbons - %d", numBranchRibbons);
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
        if (ImGui::DragFloat("end", &endRadius, 0.003f, 0.0f, 1.f)) {
            rebuildRibbons();
        }
        if (ImGui::DragFloat("step", &stepFactor, 0.1f, 0.1f, 5.f)) {
            rebuildRibbons();
        }
        if (ImGui::DragFloat("bend", &bendFactor, 0.01f, 0.9f, 1.f)) {
            rebuildRibbons();
        }

        ImGui::Separator();
        ImGui::Text("Twigs");
        if (ImGui::DragFloat("twig scale", &twigScale, 0.01f, 0.001f, 1.f)) {
            rebuildRibbons();
        }
        if (ImGui::Button("import twig")) {
            importTwig();
            rebuildRibbons();
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
                for (int i = 0; i < numSegments; i++)
                {
                    float3 vin = readVertex();
                    float3 v0 = float3(vin.x, vin.z, -vin.y);
                    vin = readVertex();
                    float3 v1 = float3(vin.x, vin.z, -vin.y);
                    twig[twiglength].pos = (v0 + v1) * 0.5f;
                    twig[twiglength].right = v1 - v0;
                    twig[twiglength].B = 1 + ((i > 0) << 16);     // material + do not rotate
                    twig[twiglength].A = i > 0;
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
            branchLeaves.push_back(L);
            currentBranch->leaves.push_back(L);

            verts[0] = readVertex();
            read2();
            return;
        }
    }

    // we dont, this is the start of the next branc, and we have 3 already
    branchMode = true;
    branches.emplace_back();
    currentBranch = &branches.back();
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
                        if (numVerts == 8) {
                            // so when the verts increase we have hit the endcap of a branch
                            endLeaves.emplace_back();
                            endLeaves.back().pos = center;
                            endLeaves.back().dir = nodeDir;
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
            calcLight();
            rebuildRibbons();

            /*
            FILE* f = fopen("f:\\theGrove\\ash.ribbon", "wb");
            if (f) {
                fwrite(branchRibbons, sizeof(ribbonVertex), numBranchRibbons, f);
                fclose(f);
            }
            */
        }
    }
}

bool pointCylindar(const glm::vec3 &point, const glm::vec3& A, const glm::vec3& B, const float radius)
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

void _GroveTree::findSideBranches()
{
    numSideBranchesFound = 0;
    for (int B = 0; B < branches.size(); B++)
    {
        glm::vec3 P0 = branches[B].nodes[0].pos;
        float R0 = branches[B].nodes[0].radius;
        for (int C = B-1; C >= 0; C--)   // try <B again
        {
            if (branches[B].rootBranch >= 0) continue; // have al;ready found it

            if (branches[C].nodes[0].radius > R0)
            {
                for (int n = 0; n < branches[C].nodes.size() - 1; n++)
                {
                    //if (branches[C].nodes[n].radius < (R0 * 0.6)) break;

                    if (pointCylindar(P0, branches[C].nodes[n].pos, branches[C].nodes[n+1].pos, branches[C].nodes[n].radius * 4))
                    {
                        branches[B].rootBranch = C;
                        branches[B].sideNode = n;
                        branches[C].sideBranches.push_back(B);
                        numSideBranchesFound++;
                        break;
                    }
                }
            }
        }
    }
}



void _GroveTree::rebuildRibbons()
{
    numBranchRibbons = 0;
    for (int b = 0; b < branches.size(); b++)
    {
        if (showOnlyUnattached && branches[b].rootBranch >= 0) continue;
        if (showOnlyUnattached && b == 0) continue;

        if (branches[b].nodes.size() > 1)
        {
            for (int n = 0; n < branches[b].nodes.size(); n++)
            {
                branchRibbons[numBranchRibbons].pos = branches[b].nodes[n].pos;
                branchRibbons[numBranchRibbons].A = 1;
                float3 D;
                if (n == 0) {
                    branchRibbons[numBranchRibbons].A = 0;
                    D = glm::normalize(branches[b].nodes[1].pos - branches[b].nodes[0].pos);
                }
                else
                {
                    D = glm::normalize(branches[b].nodes[n].pos - branches[b].nodes[n - 1].pos);
                }

                float offset = glm::dot(D, branches[b].nodes[n].dir);
                if (offset < 0.9) {
                    bool bcm = true;
                }
                branchRibbons[numBranchRibbons].right = branches[b].nodes[n].radius * branches[b].nodes[n].dir * 2.0f;
                branchRibbons[numBranchRibbons].B = 0;
                if (n == branches[b].nodes.size() - 1) {
                    branchRibbons[numBranchRibbons].B = 1;
                }

                branchRibbons[numBranchRibbons].lighting = lightPos(branchRibbons[numBranchRibbons].pos);

                if (n == 0 && ((branches[b].nodes[n].radius > endRadius))) {
                    numBranchRibbons++; // fixme test start
                }
                else if (   (branches[b].nodes[n].radius < startRadius) &&
                            (branches[b].nodes[n].radius > endRadius) &&
                            (glm::length(branchRibbons[numBranchRibbons].pos - branchRibbons[numBranchRibbons-1].pos) / branches[b].nodes[n].radius > stepFactor) ){
                    numBranchRibbons++;
                }
                /*startRadius = 1000.f;
    float endRadius = 0.f;
    float stepFactor = 2.0f;
    float bendFactor = 0.95;*/
                
            }

        }
    }

    if (includeBranchLeaves) {
        for (int b = 0; b < branchLeaves.size(); b++)
        {
            
            float3 P = branchLeaves[b].pos;
            float3 D = normalize(branchLeaves[b].dir);
            float3 U = float3(0, 0, 1);
            float3 R = cross(U, D);
            U = cross(D, R);
            /*
            for (int i = 0; i < twiglength; i++)
            {
                branchRibbons[numBranchRibbons] = twig[i];
                branchRibbons[numBranchRibbons].pos = P + twig[i].pos.x * D + twig[i].pos.y * R + twig[i].pos.z * U;
                branchRibbons[numBranchRibbons].lighting = lightPos(branchRibbons[numBranchRibbons].pos);
                numBranchRibbons++;
            }
            */
            
            branchRibbons[numBranchRibbons].pos = branchLeaves[b].pos + branchLeaves[b].dir * 0.1f;
            branchRibbons[numBranchRibbons].right = 0.07f * branchLeaves[b].dir;
            branchRibbons[numBranchRibbons].A = 0;
            branchRibbons[numBranchRibbons].B = 1;
            branchRibbons[numBranchRibbons].lighting = lightPos(branchRibbons[numBranchRibbons].pos);
            numBranchRibbons++;

            branchRibbons[numBranchRibbons].pos = branchLeaves[b].pos + branchLeaves[b].dir * 0.25f;
            branchRibbons[numBranchRibbons].right = 0.07f * branchLeaves[b].dir;
            branchRibbons[numBranchRibbons].A = 1;
            branchRibbons[numBranchRibbons].B = 1;
            branchRibbons[numBranchRibbons].lighting = lightPos(branchRibbons[numBranchRibbons].pos);
            numBranchRibbons++;
            
        }
    }

    if (includeEndLeaves) {
        for (int b = 0; b < endLeaves.size(); b++)
        {
            float3 P = endLeaves[b].pos;
            float3 D = normalize(endLeaves[b].dir);
            float3 U = float3(0, 0, 1);
            float3 R = cross(U, D);
            U = cross(D, R);
            for (int i = 0; i < twiglength; i++)
            {
                branchRibbons[numBranchRibbons] = twig[i];
                branchRibbons[numBranchRibbons].pos = P + twig[i].pos.x * D + twig[i].pos.y * R + twig[i].pos.z * U;
                branchRibbons[numBranchRibbons].lighting = lightPos(branchRibbons[numBranchRibbons].pos);
                numBranchRibbons++;
            }
            /*
            branchRibbons[numBranchRibbons].pos = endLeaves[b].pos + endLeaves[b].dir * 0.0f;
            branchRibbons[numBranchRibbons].right = 0.2f * endLeaves[b].dir;
            branchRibbons[numBranchRibbons].A = 0;
            branchRibbons[numBranchRibbons].B = 2;
            branchRibbons[numBranchRibbons].lighting = lightPos(branchRibbons[numBranchRibbons].pos);
            numBranchRibbons++;

            branchRibbons[numBranchRibbons].pos = endLeaves[b].pos + endLeaves[b].dir * 0.4f;
            branchRibbons[numBranchRibbons].right = 0.2f * endLeaves[b].dir;
            branchRibbons[numBranchRibbons].A = 1;
            branchRibbons[numBranchRibbons].B = 2;
            branchRibbons[numBranchRibbons].lighting = lightPos(branchRibbons[numBranchRibbons].pos);
            numBranchRibbons++;
            */
        }
    }


    bChanged = true;
}



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
    light.center /= cnt;
    glm::vec3 extents = light.Max - light.Min;
    light.scale = 2.0f / extents;

    for (int a = 0; a < 6; a++)
    {
        for (int b = 0; b < 16; b++) {
            for (int c = 0; c < 16; c++) {
                light.distCube[a][b][c] = 0;
            }
        }
    }


    for (int b = 0; b < cnt; b++)
    {
        glm::int3 R = cubeLookup(endLeaves[b].pos);
        float L = glm::length(endLeaves[b].pos - light.center) + 0.2f;  // so that the tips are outside
        light.distCube[R.x][R.y][R.z] = __max(light.distCube[R.x][R.y][R.z], L);
    }

}


float4 _GroveTree::lightPos(glm::vec3 P)
{
    
    glm::int3 R = cubeLookup(P);
    float scale = glm::length(P - light.center) / light.distCube[R.x][R.y][R.z];
    glm::vec3 norm = (P - light.center) * light.scale;
    


    return float4(norm.x, norm.y, norm.z, glm::smoothstep(0.8f, 1.0f, scale));
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

        terrainShader.load("Samples/Earthworks_4/hlsl/render_Tiles.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
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

        terrainSpiteShader.load("Samples/Earthworks_4/hlsl/render_tile_sprite.hlsl", "vsMain", "psMain", Vao::Topology::PointList, "gsMain");
        terrainSpiteShader.Vars()->setBuffer("tiles", split.buffer_tiles);
        terrainSpiteShader.Vars()->setBuffer("tileLookup", split.buffer_lookup_quads);
        terrainSpiteShader.Vars()->setBuffer("instanceBuffer", split.buffer_instance_quads);        // WHY BOTH
        terrainSpiteShader.Vars()->setSampler("gSampler", sampler_ClampAnisotropic);
        terrainSpiteShader.Vars()->setTexture("gAlbedo", spriteTexture);

















        std::mt19937 G(12);
        std::uniform_real_distribution<> D(-1.f, 1.f);
        std::uniform_real_distribution<> D2(0.7f, 1.4f);
        ribbonData = Buffer::createStructured(sizeof(ribbonVertex), 1024 * 1024 * 20); // just a nice amount for now
        ribbonVertex testribbons[50 * 128];
        memset(testribbons, 0, 50 * 128 * sizeof(ribbonVertex)); //16x8
        // now fill it with stuff
        uint CNT = 0;
        for (int grass = 0; grass < 50; grass++)
        {
            float scale = D2(G);
            for (int blade = 0; blade < 10; blade++)
            {
                glm::vec3 start = glm::vec3(D(G), 0, D(G)) * 0.02f; // 2cm start radius
                glm::vec3 R = glm::normalize(glm::cross(glm::vec3(0, 1, 0), start)) * 1.f * (float)D2(G);
                glm::vec3 O = start * 0.2f * (float)D2(G);
                float down = 0.01f;
                float outer = glm::length(start) / 0.09f * (float)D2(G);

                testribbons[CNT].pos = start * scale;
                testribbons[CNT].A = 0;
                testribbons[CNT].right = R;
                testribbons[CNT].B = 0;
                CNT++;

                for (int segment = 1; segment < 12; segment++)
                {
                    start += float3(0, (0.04 * (1 - outer)) - down, 0) + O;
                    O *= 1.2;
                    down *= 1.2f;
                    testribbons[CNT].pos = start * scale;
                    testribbons[CNT].A = 1;
                    testribbons[CNT].right = R;
                    if (segment > 4) testribbons[CNT].right *= (12 - segment) * 0.33f;
                    testribbons[CNT].B = 0;
                    CNT++;
                }
            }
        }
        ribbonData->setBlob(testribbons, 0, 50 * 128 * sizeof(ribbonVertex));


        FILE* f = fopen("f:\\theGrove\\ash.ribbon", "rb");
        if (f) {
            fread(testribbons, sizeof(ribbonVertex), 2114, f);
            ribbonData->setBlob(testribbons, 0, 2114 * sizeof(ribbonVertex));
            fclose(f);
        }



        Texture::SharedPtr tex = Texture::createFromFile("F:\\terrains\\_resources\\textures_dot_com\\materials\\bark\\Oak1_albedo.dds", false, true);
        ribbonTextures.emplace_back(tex);
        tex = Texture::createFromFile("F:\\terrains\\_resources\\textures_dot_com\\materials\\bark\\Oak1_sprite_normal.dds", false, false);
        ribbonTextures.emplace_back(tex);

        tex = Texture::createFromFile("F:\\terrains\\_resources\\textures_dot_com\\materials\\twigs\\poplar_twig1_albedo.dds", false, true);
        ribbonTextures.emplace_back(tex);
        tex = Texture::createFromFile("F:\\terrains\\_resources\\textures_dot_com\\materials\\twigs\\poplar_twig1_normal.dds", false, false);
        ribbonTextures.emplace_back(tex);


        ribbonShader.load("Samples/Earthworks_4/hlsl/render_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
        ribbonShader.Vars()->setBuffer("instanceBuffer", ribbonData);        // WHY BOTH
        ribbonShader.Vars()->setBuffer("instances", split.buffer_clippedloddedplants);
        ribbonShader.Vars()->setSampler("gSampler", sampler_ClampAnisotropic);              // fixme only cvlamlX

        auto& block = ribbonShader.Vars()->getParameterBlock("gribbonTextures");
        ShaderVar& var = block->findMember("T");
        for (size_t i = 0; i < ribbonTextures.size(); i++)
        {
            var[i] = ribbonTextures[i];
        }



        triangleData = Buffer::createStructured(sizeof(triangleVertex), 16384); // just a nice amount for now

        triangleShader.load("Samples/Earthworks_4/hlsl/render_triangles.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
        triangleShader.Vars()->setBuffer("instanceBuffer", triangleData);        // WHY BOTH
        triangleShader.Vars()->setBuffer("instances", split.buffer_clippedloddedplants);
        triangleShader.Vars()->setSampler("gSampler", sampler_ClampAnisotropic);

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
            sprintf(name, "F:\\terrains\\_resources\\textures_dot_com\\TexturesCom_3dplant_Nettle\\Individual_seeds\\A_256.fbx");

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




        compute_TerrainUnderMouse.load("Samples/Earthworks_4/hlsl/compute_terrain_under_mouse.hlsl");
        compute_TerrainUnderMouse.Vars()->setSampler("gSampler", sampler_Clamp);
        compute_TerrainUnderMouse.Vars()->setTexture("gHeight", height_Array);
        compute_TerrainUnderMouse.Vars()->setBuffer("tiles", split.buffer_tiles);
        compute_TerrainUnderMouse.Vars()->setBuffer("groundcover_feedback", split.buffer_feedback);

        // clear
        split.compute_tileClear.load("Samples/Earthworks_4/hlsl/compute_tileClear.hlsl");
        split.compute_tileClear.Vars()->setBuffer("feedback", split.buffer_feedback);
        split.compute_tileClear.Vars()->setBuffer("DrawArgs_Terrain", split.drawArgs_tiles);
        split.compute_tileClear.Vars()->setBuffer("DrawArgs_Quads", split.drawArgs_quads);
        split.compute_tileClear.Vars()->setBuffer("DrawArgs_Plants", split.drawArgs_plants);
        split.compute_tileClear.Vars()->setBuffer("DrawArgs_ClippedLoddedPlants", split.drawArgs_clippedloddedplants);
        split.compute_tileClear.Vars()->setBuffer("DispatchArgs_Plants", split.dispatchArgs_plants);


        // plants clip lod animate
        split.compute_clipLodAnimatePlants.load("Samples/Earthworks_4/hlsl/compute_clipLodAnimatePlants.hlsl");
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("tileLookup", split.buffer_lookup_plants);
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("plantBuffer", split.buffer_instance_plants);
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("output", split.buffer_clippedloddedplants);
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("drawArgs_Plants", split.drawArgs_clippedloddedplants);
        split.compute_clipLodAnimatePlants.Vars()->setBuffer("feedback", split.buffer_feedback);

        // split merge
        split.compute_tileSplitMerge.load("Samples/Earthworks_4/hlsl/compute_tileSplitMerge.hlsl");
        split.compute_tileSplitMerge.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileSplitMerge.Vars()->setBuffer("feedback", split.buffer_feedback);

        // generate
        split.compute_tileGenerate.load("Samples/Earthworks_4/hlsl/compute_tileGenerate.hlsl");
        split.compute_tileGenerate.Vars()->setBuffer("quad_instance", split.buffer_instance_quads);
        split.compute_tileGenerate.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tileGenerate.Vars()->setTexture("gNoise", split.noise_u16);
        split.compute_tileGenerate.Vars()->setTexture("gHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileGenerate.Vars()->setTexture("gEct1", split.tileFbo->getColorTexture(4));
        split.compute_tileGenerate.Vars()->setTexture("gEct2", split.tileFbo->getColorTexture(5));
        split.compute_tileGenerate.Vars()->setTexture("gEct3", split.tileFbo->getColorTexture(6));
        split.compute_tileGenerate.Vars()->setTexture("gEct4", split.tileFbo->getColorTexture(7));

        // passthrough
        split.compute_tilePassthrough.load("Samples/Earthworks_4/hlsl/compute_tilePassthrough.hlsl");
        split.compute_tilePassthrough.Vars()->setBuffer("quad_instance", split.buffer_instance_quads);
        split.compute_tilePassthrough.Vars()->setBuffer("plant_instance", split.buffer_instance_plants);
        split.compute_tilePassthrough.Vars()->setBuffer("feedback", split.buffer_feedback);
        split.compute_tilePassthrough.Vars()->setBuffer("tiles", split.buffer_tiles);
        split.compute_tilePassthrough.Vars()->setTexture("gHgt", split.tileFbo->getColorTexture(0));
        split.compute_tilePassthrough.Vars()->setTexture("gNoise", split.noise_u16);

        // build lookup
        split.compute_tileBuildLookup.load("Samples/Earthworks_4/hlsl/compute_tileBuildLookup.hlsl");
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
        split.compute_tileBicubic.load("Samples/Earthworks_4/hlsl/compute_tileBicubic.hlsl");
        split.compute_tileBicubic.Vars()->setSampler("linearSampler", sampler_Clamp);
        split.compute_tileBicubic.Vars()->setTexture("gOutput", split.tileFbo->getColorTexture(0));
        split.compute_tileBicubic.Vars()->setTexture("gDebug", split.debug_texture);
        //split.compute_tileBicubic.Vars()->setTexture("gOuthgt_TEMPTILLTR", split.tileFbo->getColorTexture(0));

        // ecotopes
        split.compute_tileEcotopes.load("Samples/Earthworks_4/hlsl/compute_tileEcotopes.hlsl");
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
        split.compute_tileNormals.load("Samples/Earthworks_4/hlsl/compute_tileNormals.hlsl");
        split.compute_tileNormals.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileNormals.Vars()->setTexture("gOutNormals", split.normals_texture);
        split.compute_tileNormals.Vars()->setTexture("gOutput", split.debug_texture);
        split.compute_tileNormals.Vars()->setBuffer("tiles", split.buffer_tiles);

        // vertices
        split.compute_tileVerticis.load("Samples/Earthworks_4/hlsl/compute_tileVertices.hlsl");
        split.compute_tileVerticis.Vars()->setSampler("linearSampler", sampler_Clamp);
        split.compute_tileVerticis.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileVerticis.Vars()->setTexture("gOutVerts", split.vertex_A_texture);
        split.compute_tileVerticis.Vars()->setTexture("gDebug", split.debug_texture);
        split.compute_tileVerticis.Vars()->setBuffer("tileCenters", split.buffer_tileCenters);
        split.compute_tileVerticis.Vars()->setBuffer("tiles", split.buffer_tiles);

        // jumpflood
        // It may even be faster to set this up twice and hop between the two
        split.compute_tileJumpFlood.load("Samples/Earthworks_4/hlsl/compute_tileJumpFlood.hlsl");
        split.compute_tileJumpFlood.Vars()->setTexture("gDebug", split.debug_texture);

        // delaunay
        split.compute_tileDelaunay.load("Samples/Earthworks_4/hlsl/compute_tileDelaunay.hlsl");
        split.compute_tileDelaunay.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileDelaunay.Vars()->setTexture("gInVerts", split.vertex_B_texture);
        split.compute_tileDelaunay.Vars()->setBuffer("VB", split.buffer_terrain);
        split.compute_tileDelaunay.Vars()->setBuffer("tiles", split.buffer_tiles);
        //split.compute_tileDelaunay.Vars()->setBuffer("tileDrawargsArray", split.drawArgs_tiles);

        // elevation mipmap
        split.compute_tileElevationMipmap.load("Samples/Earthworks_4/hlsl/compute_tileElevationMipmap.hlsl");
        split.compute_tileElevationMipmap.Vars()->setTexture("gInHgt", split.tileFbo->getColorTexture(0));
        split.compute_tileElevationMipmap.Vars()->setTexture("gDebug", split.debug_texture);
        const auto& mipmapReflector = split.compute_tileElevationMipmap.Reflector()->getDefaultParameterBlock();
        ParameterBlock::BindLocation bind_mip1 = mipmapReflector->getResourceBinding("gMip1");
        ParameterBlock::BindLocation bind_mip2 = mipmapReflector->getResourceBinding("gMip2");
        ParameterBlock::BindLocation bind_mip3 = mipmapReflector->getResourceBinding("gMip3");
        split.compute_tileElevationMipmap.Vars()->setUav(bind_mip1, split.tileFbo->getColorTexture(0)->getUAV(1));
        split.compute_tileElevationMipmap.Vars()->setUav(bind_mip2, split.tileFbo->getColorTexture(0)->getUAV(2));
        split.compute_tileElevationMipmap.Vars()->setUav(bind_mip3, split.tileFbo->getColorTexture(0)->getUAV(3));

        // BC6H compressor
        split.bc6h_texture = Texture::create2D(tile_numPixels / 4, tile_numPixels / 4, Falcor::ResourceFormat::RGBA32Uint, 1, 1, nullptr, Falcor::Resource::BindFlags::UnorderedAccess);
        split.compute_bc6h.load("Samples/Earthworks_4/hlsl/compute_bc6h.hlsl");
        split.compute_bc6h.Vars()->setTexture("gOutput", split.bc6h_texture);

    }

    allocateTiles(numTiles);

    elevationCache.resize(45);
    loadElevationHash(pRenderContext);

    init_TopdownRender();

    mSpriteRenderer.onLoad();

    mEcosystem.terrainSize = settings.size;
    terrafectorEditorMaterial::rootFolder = settings.dirResource + "/";
    terrafectors.loadPath(settings.dirRoot + "/terrafectors", settings.dirRoot + "/bake", false);
    mRoadNetwork.rootPath = settings.dirRoot + "/";


}


void terrainManager::init_TopdownRender()
{

    terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials = Buffer::createStructured(sizeof(TF_material), 2048); // FIXME hardcoded
    split.shader_spline3D.load("Samples/Earthworks_4/hlsl/render_spline.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
    split.shader_spline3D.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);
    split.shader_spline3D.Vars()->setSampler("gSmpLinear", sampler_Trilinear);
    //split.shader_spline3D.Program()->getReflector()
    split.shader_splineTerrafector.load("Samples/Earthworks_4/hlsl/render_splineTerrafector.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
    //split.shader_splineTerrafector.State()->setFbo(split.tileFbo);
    split.shader_splineTerrafector.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);


    // mesh terrafector shader
    split.shader_meshTerrafector.load("Samples/Earthworks_4/hlsl/render_meshTerrafector.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
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
        if (ImGui::Combo("###modeSelector", &terrainMode, "Vegetation\0Ecotope\0Terrafector\0Bezier road network\0")) { ; }
        ImGui::PopItemWidth();
        ImGui::NewLine();

        auto& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_Button] = ImVec4(0.03f, 0.03f, 0.3f, 0.5f);

        switch (terrainMode)
        {
        case 0:
            groveTree.renderGui(_gui);
            break;
        case 1:
            mEcosystem.renderGUI(_gui);
            if (ImGui::BeginPopupContextWindow(false))
            {
                if (ImGui::Selectable("New Ecotope")) { mEcosystem.addEcotope(); }
                if (ImGui::Selectable("Load")) { mEcosystem.load(); }
                if (ImGui::Selectable("Save")) { mEcosystem.save(); }
                ImGui::EndPopup();
            }
            break;
        case 2:
            //terrafectors.renderGui(_gui);
            ImGui::NewLine();
            //roadMaterialCache::getInstance().renderGui(_gui);
            break;
        case 3:
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
            break;

        }
        ImGui::PopFont();
    }
    rightPanel.release();

    switch (terrainMode)
    {
    case 0: break;
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


bool terrainManager::update(RenderContext* _renderContext)
{
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
            //if (_tile->lod == 13)  scale = 1.2f;
            //if (_tile->lod == 14)  scale = 1.5f;
            //if (_tile->lod == 15)  scale = 2.0f;
            //if (_tile->lod >= 16)  scale = 3.2f;
            //scale *= 1.5;

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



void terrainManager::onFrameRender(RenderContext* _renderContext, const Fbo::SharedPtr& _fbo, const Camera::SharedPtr _camera, GraphicsState::SharedPtr _graphicsState, GraphicsState::Viewport _viewport)
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

    if (terrainMode == 0)
    {
        

        if (groveTree.bChanged)
        {
            ribbonData->setBlob(groveTree.branchRibbons, 0, groveTree.numBranchRibbons * sizeof(ribbonVertex));
            groveTree.bChanged = false;
        }

        ribbonShader.State()->setFbo(_fbo);
        ribbonShader.State()->setViewport(0, _viewport, true);
        ribbonShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        ribbonShader.Vars()["gConstantBuffer"]["eye"] = _camera->getPosition();

        ribbonShader.State()->setRasterizerState(split.rasterstateSplines);
        ribbonShader.State()->setBlendState(split.blendstateSplines);

        {
            FALCOR_PROFILE("ribbonShader");
            ribbonShader.drawInstanced(_renderContext, groveTree.numBranchRibbons, 100);
            //ribbonShader.renderIndirect(_renderContext, split.drawArgs_clippedloddedplants);
        }

        return;
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

        glm::vec3 D = glm::vec3(view[2][0], view[2][1], -view[2][2]);
        glm::vec3 R = glm::normalize(glm::cross(glm::vec3(0, 1, 0), D));
        terrainSpiteShader.Vars()["gConstantBuffer"]["right"] = R;
        terrainSpiteShader.Vars()["gConstantBuffer"]["alpha_pass"] = 0;

        terrainSpiteShader.State()->setRasterizerState(split.rasterstateSplines);
        terrainSpiteShader.State()->setBlendState(split.blendstateSplines);

        terrainSpiteShader.renderIndirect(_renderContext, split.drawArgs_quads);

        terrainSpiteShader.Vars()["gConstantBuffer"]["alpha_pass"] = 1;
        terrainSpiteShader.renderIndirect(_renderContext, split.drawArgs_quads);
    }

    {

        FALCOR_PROFILE("ribbonShader");

        ribbonShader.State()->setFbo(_fbo);
        ribbonShader.State()->setViewport(0, _viewport, true);
        ribbonShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        ribbonShader.Vars()["gConstantBuffer"]["eye"] = _camera->getPosition();

        ribbonShader.State()->setRasterizerState(split.rasterstateSplines);
        ribbonShader.State()->setBlendState(split.blendstateSplines);

        //ribbonShader.drawInstanced(_renderContext, 128, 10024);
        ribbonShader.renderIndirect(_renderContext, split.drawArgs_clippedloddedplants);

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
    if (terrainMode == 0)
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
                mouseVegPitch = glm::clamp(mouseVegPitch, 0.f, 1.5f);

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
            mouseVegOrbit = glm::clamp(mouseVegOrbit, 1.f, 5000.f);
        }
        break;
        }

        glm::vec3 camPos;
        camPos.y = sin(mouseVegPitch);
        camPos.x = cos(mouseVegPitch) * sin(mouseVegYaw);
        camPos.z = cos(mouseVegPitch) * cos(mouseVegYaw);
        _camera->setPosition(camPos * mouseVegOrbit);
        _camera->setTarget(glm::vec3(0, 6, 0));
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
            if (lod == 4) splineWidth *= 2.0f;
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
