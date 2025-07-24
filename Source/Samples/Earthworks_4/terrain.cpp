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
#include <chrono>
using namespace std::chrono;

#pragma optimize("", off)

#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}




_lastFile terrainManager::lastfile;



void setupVert(ribbonVertex* r, int start, float3 pos, float radius, int _mat = 0)
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
        ribbonVertex* rprev = r--;
        float3 tangent = glm::normalize(r->position - rprev->position);
        r->bitangent = tangent;
        rprev->bitangent = tangent;
    }
}













#include <iostream>
#include <fstream>
void _shadowEdges::solveThread()
{
    while (1)
    {
        if (requestNewShadow)
        {
            sunAngle += dAngle;
            sunAng.x = -cos(sunAngle);
            sunAng.y = -sin(sunAngle);
            sunAng.z = 0;
            solve(-sunAng.y, sunAng.x > 0);
            shadowReady = true;
            requestNewShadow = false;
        }
        else
        {
            Sleep(10);
        }
    }
}

void _shadowEdges::solve(float _angle, bool dx)
{
    for (int y = 0; y < 4095; y++)
    {
        for (int x = 0; x < 4095; x++)
        {
            //shadowH[y][x] = float2(height[y][x] - 15.f, 0.f);
            shadowH[y][x] = float2(-5000.f, 0.f);   // when splittign the two
        }
    }

    float a_min = _angle + 0.02f;
    float a_max = _angle - 0.02f;

    for (int y = 1; y < 4094; y++)
    {
        for (int x = 1; x < 4094; x++)
        {

            if (dx && (Nx[y][x - 1] < a_min) && (Nx[y][x] > a_max))
            {
                float H = height[y][x];

                for (int j = x + 1; j < 4096; j++)
                {
                    H -= _angle * 9.765625f;
                    if (H > shadowH[y][j].x)
                    {
                        float softDepth = (float)(j - x) * 9.765625f / 10.f;
                        shadowH[y][j] = float2(H - 0.f, softDepth);
                    }
                    else break;
                }
            }
            else if (!dx && (-Nx[y][x + 1] < a_min) && (-Nx[y][x] > a_max))
            {
                float H = height[y][x];

                for (int j = x - 1; j > 0; j--)
                {
                    H -= _angle * 9.765625f;
                    if (H > shadowH[y][j].x)
                    {
                        float softDepth = (float)(x - j) * 9.765625f / 10.f;
                        shadowH[y][j] = float2(H - 0.f, softDepth);
                    }
                    else break;
                }
            }
        }
    }
}

void _shadowEdges::load(std::string filename, float _angle)
{
    int edgeCnt = 0;
    int shadowEdge = 0;

    std::ifstream ifs;
    ifs.open(filename, std::ios::binary);

    if (ifs)
    {
        ifs.read((char*)height, 4096 * 4096 * 4);
        ifs.close();

        for (int y = 0; y < 4095; y++)
        {
            for (int x = 0; x < 4095; x++)
            {
                Nx[y][x] = (height[y][x] - height[y][x + 1]) / 9.765625f;    // 10 meter between pixels SHIT NOT TRUE

                shadowH[y][x] = float2(height[y][x] - 5.f, 0.f);
                // remove this and pass terrein height seperate
            }
        }

        /*
        float angle = _angle;  // about 10 degrees
        float a_min = angle;
        float a_max = angle -0.01f;

        memset(edge, 0, 4096 * 4096);
        for (int y = 1; y < 4094; y++)
        {
            for (int x = 1; x < 4094; x++)
            {
                if (Nx[y][x] > a_min)
                {
                    edge[y][x] = (unsigned char)(128.f * saturate(Nx[y][x] - a_min));  // hill shade
                }


                if ((Nx[y][x-1] < a_min) && (Nx[y][x] > a_max))
                {
                    edge[y][x] = 255;
                    edgeCnt++;


                    float H = height[y][x];
                    for (int j = x + 1; j < 4096; j++)
                    {
                        H -= angle * 9.765625f;
                        if (H > shadowH[y][j].x)
                        {
                            edge[y][j] = 0;

                            float softDepth = (float)(j-x) * 9.765625f / 10.f;
                            shadowH[y][j] = float2(H - 0.f, softDepth);
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

                //  }
              //}
              /*
              std::ofstream ofs;
              ofs.open(filename + ".edge.raw", std::ios::binary);
              if (ofs)
              {
                  ofs.write((char*)edge, 4096 * 4096);
                  ofs.close();
              }
              */
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
    std::uniform_int_distribution<> distributionAlbedo(-50, 50);

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

    int albRnd = distributionAlbedo(generator);

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
        //ribbon[ribbonLength].albedoScale = (unsigned char)(glm::lerp(albedoScaleNew, albedoScale, _age) * 127.f) + albRnd;
        //ribbon[ribbonLength].translucencyScale = (unsigned char)(glm::lerp(translucencyScaleNew, translucencyScale, _age) * 127.f);
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
        ImGui::Text("(%3.2f, %3.2f)m, (radius, length)", extents.x * 2, extents.y);

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
        if (ImGui::DragFloat("##age", &numSegments, 0.1f, 1.2f, 20, "%2.1f")) changed = true;
        startSegment = __min(startSegment, (int)floor(numSegments));

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
        if (ImGui::DragFloat("##leafRotation", &stemRotation, 0.01f, 0, 2, "%3.2f")) changed = true;
        TOOLTIP("spin around the axis from one leaf cluster to the next");

        ImGui::Text("age factor");
        ImGui::SameLine(80, 0);
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##leaf_age_power", &leaf_age_power, 0.1f, 0.1f, 10, "%3.2f")) changed = true;
        TOOLTIP("scale the age along the twig, 0 is newest growth");

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

    if (_age == 0) _age = this->numSegments;
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

                    float rot = l * 6.14f / numLeaves + (i * stemRotation) + distribution(generator) * 0.05f;// +randStartLeafRotation;
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
        extents = float2(0, 0);
        for (int i = 0; i < ribbonLength; i++)
        {
            float2 XZ = ribbon[i].position.xz;
            float r = glm::length(XZ);
            extents.x = __max(extents.x, r + ribbon[i].radius);
            //extents.x = __max(extents.x, abs(ribbon[i].position.x) + ribbon[i].radius);
            extents.y = __max(extents.y, abs(ribbon[i].position.y) + ribbon[i].radius);
            //extents.z = __max(extents.z, abs(ribbon[i].position.z) + ribbon[i].radius);
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
    if (ImGui::DragFloat("##age", &numSegments, 0.1f, 1.2f, 20, "%2.1f")) changed = true;
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
            float A = numSegments;// *(1.0f + T.rnd * (distribution(generator)))* ageRange;

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
        extents = float2(0, 0);
        for (int i = 0; i < ribbonLength; i++)
        {
            float2 XZ = ribbon[i].position.xz;
            float r = glm::length(XZ);
            extents.x = __max(extents.x, r + ribbon[i].radius);
            extents.y = __max(extents.y, abs(ribbon[i].position.y) + ribbon[i].radius);
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

            ImGui::Text("packing");
            if (ImGui::DragFloat("size", &objectSize, 0.1f, 0.01f, 100.f)) { rebuildRibbons(); }
            if (ImGui::DragFloat("radius", &radiusScale, 0.001f, 0.01f, 10.f)) { rebuildRibbons(); }
            if (ImGui::DragFloat3("offset", &objectOffset.x, 0.01f, 0.0f, 1.f)) { rebuildRibbons(); }


            ImGui::NewLine();


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
                if (ImGui::DragFloat("##Ao_depthScale", &gpuPlant.Ao_depthScale, 0.01f, 0.001f, 1.0f));

                ImGui::Text("sunTilt");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##sunTilt", &gpuPlant.sunTilt, 0.01f, -1.0f, 1.0f));

                ImGui::Text("bDepth");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##bDepth", &gpuPlant.bDepth, 0.1f, 0.1f, 50.0f));

                ImGui::Text("bScale");
                ImGui::SameLine(80, 0);
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##bScale", &gpuPlant.bScale, 0.01f, 0.01f, 1.0f));


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

void _GroveTree::toPlant()
{
    gpuPlant.size = extents;    // hafl width but full height

    gpuPlant.scale = getScale();
    gpuPlant.radiusScale = radiusScale;
    gpuPlant.offset = getOffset();

    //gpuPlant.Ao_depthScale = Ao_depthScale;
    //gpuPlant.sunTilt = sunTilt;
    //gpuPlant.bDepth = bDepth;
    //gpuPlant.bScale = bScale;

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


    extents = float2(0, 0);
    for (int i = 0; i < numBranchRibbons; i++)
    {
        float2 XZ = branchRibbons[i].position.xz;
        float r = glm::length(XZ);
        extents.x = __max(extents.x, r);
        extents.y = __max(extents.y, abs(branchRibbons[i].position.y));
    }


    toPlant();
    ribbonVertex::setup(gpuPlant.scale, gpuPlant.radiusScale, gpuPlant.offset);
    for (int i = 0; i < numBranchRibbons; i++)
    {
        packedRibbons[i] = branchRibbons[i].pack();
    }

    bChanged = true;
}

void _GroveTree::rebuildRibbons_Leaf()
{
    toPlant();
    ribbonVertex::setup(gpuPlant.scale, gpuPlant.radiusScale, gpuPlant.offset);

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
    toPlant();
    ribbonVertex::setup(gpuPlant.scale, gpuPlant.radiusScale, gpuPlant.offset);

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
    toPlant();
    ribbonVertex::setup(gpuPlant.scale, gpuPlant.radiusScale, gpuPlant.offset);

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

    gpuPlant.Ao_depthScale = weedBuilder.L.Ao_depthScale;
    gpuPlant.sunTilt = weedBuilder.L.sunTilt;
    gpuPlant.bDepth = weedBuilder.L.bDepth;
    gpuPlant.bScale = weedBuilder.L.bScale;
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

}



terrainManager::~terrainManager()
{
    std::ofstream os("lastFile.xml");
    if (os.good()) {
        cereal::XMLOutputArchive archive(os);
        lastfile.road = mRoadNetwork.lastUsedFilename.string();
        lastfile.stamps = mRoadStampCollection.lastUsedFilename.string();
        archive(CEREAL_NVP(lastfile));
    }
}



void terrainManager::onLoad(RenderContext* pRenderContext, FILE* _logfile)
{
    std::filesystem::path currentPath = std::filesystem::current_path();
    fprintf(_logfile, "root directory - %s\n\n", currentPath.string().c_str());

    // Move the constructor code here
    std::ifstream is("lastFile.xml");
    if (is.good()) {
        cereal::XMLInputArchive archive(is);
        archive(CEREAL_NVP(lastfile));

        mRoadNetwork.lastUsedFilename = lastfile.road;
        mRoadStampCollection.lastUsedFilename = lastfile.stamps;
    }
    else
    {
        fprintf(_logfile, "ERROR - unable to load lastfile.xml, shutting down\n");
        gpFramework->getWindow()->shutdown();
        return;
    }

    fprintf(_logfile, "terrain name - %s\n\n", lastfile.terrain.c_str());
    std::string appendedName = currentPath.string() + lastfile.terrain;
    std::ifstream isT(lastfile.terrain);
    std::ifstream isT_2(appendedName);
    if (isT.good()) {
        fprintf(_logfile, "loading absolute terrain {%s}\n\n", lastfile.terrain.c_str());
        cereal::JSONInputArchive archive(isT);
        settings.serialize(archive, 100);
    }
    else if (isT_2.good())
    {
        cereal::JSONInputArchive archive(isT_2);
        settings.serialize(archive, 100);

        fprintf(_logfile, "loading relative terrain, prepending root directory - %s\n", appendedName.c_str());
        fprintf(_logfile, "root - %s\n", settings.dirRoot.c_str());
        fprintf(_logfile, "gis - %s\n", settings.dirGis.c_str());
        fprintf(_logfile, "resource - %s\n\n", settings.dirResource.c_str());


        settings.dirRoot = currentPath.string() + settings.dirRoot;
        settings.dirGis = currentPath.string() + settings.dirGis;
        settings.dirResource = currentPath.string() + settings.dirResource;


        fprintf(_logfile, "root - %s\n", settings.dirRoot.c_str());
        fprintf(_logfile, "gis - %s\n", settings.dirGis.c_str());
        fprintf(_logfile, "resource - %s\n\n", settings.dirResource.c_str());
    }
    else
    {
        std::filesystem::path path;
        FileDialogFilterVec filters = { {"terrainSettings.json"} };
        if (openFileDialog(filters, path))
        {
            lastfile.terrain = path.string();
            std::ifstream isT(lastfile.terrain);
            if (isT.good()) {
                cereal::JSONInputArchive archive(isT);
                settings.serialize(archive, 100);
            }
            else
            {
                fprintf(_logfile, "ERROR - unable to load a terrain  (/terrain/__somewhere__.terrainSettings.json), shutting down\n");
                fprintf(_logfile, "shutting down\n");
                gpFramework->getWindow()->shutdown();
                return;
            }
        }
    }


    terrafectorSystem::pEcotopes = &mEcosystem;
    ecotopeSystem::pVegetation = &plants_Root;


    fprintf(_logfile, "terrain.onLoad()\n");
    fflush(_logfile);

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
        terrainShader.Vars()->setSampler("gSmpLinearClamp", sampler_Clamp);

        spriteTexture = Texture::createFromFile(settings.dirRoot + "/ecosystem/sprite_diff.DDS", true, true);
        spriteNormalsTexture = Texture::createFromFile(settings.dirRoot + "/ecosystem/sprite_norm.DDS", true, false);

        terrainSpiteShader.load("Samples/Earthworks_4/hlsl/terrain/render_tile_sprite.hlsl", "vsMain", "psMain", Vao::Topology::PointList, "gsMain");
        terrainSpiteShader.Vars()->setBuffer("tiles", split.buffer_tiles);
        terrainSpiteShader.Vars()->setBuffer("tileLookup", split.buffer_lookup_quads);
        terrainSpiteShader.Vars()->setBuffer("instanceBuffer", split.buffer_instance_quads);        // WHY BOTH
        terrainSpiteShader.Vars()->setSampler("gSampler", sampler_ClampAnisotropic);
        terrainSpiteShader.Vars()->setSampler("gSmpLinearClamp", sampler_Clamp);
        //terrainSpiteShader.Vars()->setTexture("gAlbedo", spriteTexture);
        //terrainSpiteShader.Vars()->setTexture("gNorm", spriteNormalsTexture);
        //terrainSpiteShader.Vars()->setTexture("gEnv", );
        //terrainSpiteShader.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);

        

        









        std::mt19937 G(12);
        std::uniform_real_distribution<> D(-1.f, 1.f);
        std::uniform_real_distribution<> D2(0.7f, 1.4f);
        ribbonData[0] = Buffer::createStructured(sizeof(unsigned int) * 6, 1024 * 1024 * 10); // just a nice amount for now
        ribbonData[1] = Buffer::createStructured(sizeof(unsigned int) * 6, 1024 * 1024 * 10); // just a nice amount for now

        ribbonDataVegBuilder = Buffer::createStructured(sizeof(unsigned int) * 6, 1024 * 1024 * 10); // just a nice amount for now

        



        /*
        Texture::SharedPtr tex = Texture::createFromFile(settings.dirResource + "/textures/bark/Oak1_albedo.dds", false, true);
        ribbonTextures.emplace_back(tex);
        tex = Texture::createFromFile(settings.dirResource + "/textures/bark/Oak1_sprite_normal.dds", false, false);
        ribbonTextures.emplace_back(tex);

        tex = Texture::createFromFile(settings.dirResource + "/textures/twigs/dandelion_leaf1_albedo.dds", false, true);
        ribbonTextures.emplace_back(tex);
        tex = Texture::createFromFile(settings.dirResource + "/textures/twigs/dandelion_leaf1_normal.dds", false, false);
        ribbonTextures.emplace_back(tex);
        */
        {
            fprintf(_logfile, "load %s\n", (settings.dirRoot + "/buildings/rappersville").c_str());
            fflush(_logfile);

            std::string filerappersville = settings.dirRoot + "/buildings/rappersville";
            std::ifstream ifs;
            ifs.open(filerappersville + ".info.txt");
            int numVerts = 0;
            if (ifs)
            {
                ifs >> numVerts;
                ifs.close();
            }
            numVerts = __max(numVerts, 128);    // jusy not zeo

            std::vector<_buildingVertex> VERTS;
            VERTS.resize(numVerts);
            ifs.open(filerappersville + ".raw", std::ios::binary);
            if (ifs)
            {
                ifs.read((char*)VERTS.data(), VERTS.size() * sizeof(_buildingVertex));
                ifs.close();
            }

            rappersvilleData = Buffer::createStructured(sizeof(_buildingVertex), numVerts); // just a nice amount for now
            rappersvilleData->setBlob(VERTS.data(), 0, numVerts * sizeof(_buildingVertex));


            rappersvilleShader.load("Samples/Earthworks_4/hlsl/terrain/render_Buildings_Far.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
            rappersvilleShader.Vars()->setBuffer("vertexBuffer", rappersvilleData);

            numrapperstri = numVerts / 3;
            //drawArgs_rappersville = Buffer::createStructured(sizeof(t_DrawArguments), 1, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::IndirectArg);
        }


        {
            gliderwingData[0] = Buffer::createStructured(sizeof(_buildingVertex), 16384); // will be arounf 1300 so just enough
            gliderwingData[1] = Buffer::createStructured(sizeof(_buildingVertex), 16384); // will be arounf 1300 so just enough

            gliderwingShader.load("Samples/Earthworks_4/hlsl/terrain/render_GliderWing.hlsl", "vsMain", "psMain", Vao::Topology::TriangleStrip);


            RasterizerState::Desc rsDesc;
            rsDesc.setFillMode(RasterizerState::FillMode::Solid).setCullMode(RasterizerState::CullMode::Front);
            rasterstateGliderWing = RasterizerState::create(rsDesc);
        }


        vegetation.plantData = Buffer::createStructured(sizeof(plant), 256);
        vegetation.instanceData = Buffer::createStructured(sizeof(plant_instance), 16384);
        vegetation.blockData = Buffer::createStructured(sizeof(block_data), 16384);        // big enough to house inatnces * blocks per instance   8 Mb for now
        vegetation.vertexData = Buffer::createStructured(sizeof(ribbonVertex8), 256 * 128);

        vegetation.plantBuf[0].radiusScale = ribbonVertex::radiusScale;
        vegetation.plantBuf[0].scale = ribbonVertex::objectScale;
        vegetation.plantBuf[0].offset = ribbonVertex::objectOffset;
        vegetation.plantBuf[0].Ao_depthScale = 10;
        vegetation.plantBuf[0].bDepth = 1;
        vegetation.plantBuf[0].bScale = 1;
        vegetation.plantBuf[0].sunTilt = -0.2f;

        //std::mt19937 generator(100);
        std::uniform_real_distribution<> RND(-1.f, 1.f);

        for (int i = 0; i < 16384; i++)
        {
            vegetation.instanceBuf[i].plant_idx = 0;
            vegetation.instanceBuf[i].position = { RND(generator) * 5, 0, RND(generator) * 5 };
            vegetation.instanceBuf[i].scale = 1.f + RND(generator) * 0.5f;
            vegetation.instanceBuf[i].rotation = RND(generator) * 3.14f;
            vegetation.instanceBuf[i].time_offset = RND(generator) * 100;
        }
        vegetation.instanceBuf[0].position = { 0, 0, 0 };
        vegetation.instanceBuf[0].scale = 1.f;
        vegetation.instanceBuf[0].rotation = 0;

        for (int i = 0; i < 16384; i++)
        {
            vegetation.blockBuf[i].vertex_offset = 0;
            vegetation.blockBuf[i].instance_idx = i;
            vegetation.blockBuf[i].section_idx = 0;
        }

        //std::array<ribbonVertex8, 128 * 256> vertexBuf;

        vegetationShader.load("Samples/Earthworks_4/hlsl/terrain/render_vegetation_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
        vegetationShader.Vars()->setBuffer("plant_buffer", vegetation.plantData);
        vegetationShader.Vars()->setBuffer("instance_buffer", vegetation.instanceData);
        vegetationShader.Vars()->setBuffer("block_buffer", vegetation.blockData);
        vegetationShader.Vars()->setBuffer("vertex_buffer", vegetation.vertexData);
        //vegetationShader.Vars()->setBuffer("instanceBuffer", ribbonDataVegBuilder);
        vegetationShader.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
        //vegetationShader.Vars()->setBuffer("instances", split.buffer_clippedloddedplants);
        vegetationShader.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX
        vegetationShader.Vars()->setSampler("gSamplerClamp", sampler_ClampAnisotropic);              // fixme only cvlamlX


                   // fixme only cvlamlX

        

        vegetationShader_Bake.add("_BAKE", "");
        vegetationShader_Bake.load("Samples/Earthworks_4/hlsl/terrain/render_vegetation_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
        vegetationShader_Bake.Vars()->setBuffer("plant_buffer", vegetation.plantData);
        vegetationShader_Bake.Vars()->setBuffer("instance_buffer", vegetation.instanceData);
        vegetationShader_Bake.Vars()->setBuffer("block_buffer", vegetation.blockData);
        vegetationShader_Bake.Vars()->setBuffer("vertex_buffer", vegetation.vertexData);
        //vegetationShader_Bake.Vars()->setBuffer("instanceBuffer", ribbonDataVegBuilder);
        vegetationShader_Bake.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
        //vegetationShader_Bake.Vars()->setBuffer("instances", split.buffer_clippedloddedplants);
        vegetationShader_Bake.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX




        ribbonShader.load("Samples/Earthworks_4/hlsl/terrain/render_ribbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
        ribbonShader.Vars()->setBuffer("instanceBuffer", ribbonData[0]);
        ribbonShader.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);
        ribbonShader.Vars()->setBuffer("instances", split.buffer_clippedloddedplants);
        ribbonShader.Vars()->setSampler("gSampler", sampler_Ribbons);              // fixme only cvlamlX
        ribbonShader.Vars()->setSampler("gSamplerClamp", sampler_ClampAnisotropic);              // fixme only cvlamlX




        compute_bakeFloodfill.load("Samples/Earthworks_4/hlsl/terrain/compute_bakeFloodfill.hlsl");





        triangleData = Buffer::createStructured(sizeof(triangleVertex), 16384); // just a nice amount for now

        triangleShader.load("Samples/Earthworks_4/hlsl/terrain/render_triangles.hlsl", "vsMain", "psMain", Vao::Topology::TriangleList);
        triangleShader.Vars()->setBuffer("instanceBuffer", triangleData);        // WHY BOTH
        triangleShader.Vars()->setBuffer("instances", split.buffer_clippedloddedplants);
        triangleShader.Vars()->setSampler("gSampler", sampler_ClampAnisotropic);
        triangleShader.Vars()->setSampler("gSmpLinearClamp", sampler_ClampAnisotropic);


        {
            thermalsData = Buffer::createStructured(sizeof(float4), numThermals * 100); // just a nice amount for now
            //            thermalsData->setBlob(thermals.data(), 0, numThermals * 100 * sizeof(float4));

            thermalsShader.load("Samples/Earthworks_4/hlsl/terrain/render_thermalRibbons.hlsl", "vsMain", "psMain", Vao::Topology::LineStrip, "gsMain");
            thermalsShader.Vars()->setBuffer("vertexBuffer", thermalsData);        // WHY BOTH
            thermalsShader.Vars()->setSampler("gSampler", sampler_ClampAnisotropic);
        }


        std::cout << "      cfd\n";
        {
            cfd.sliceVTexture[0] = Texture::create2D(128, 128, Falcor::ResourceFormat::RGB32Float, 1, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVTexture[1] = Texture::create2D(128, 128, Falcor::ResourceFormat::RGB32Float, 1, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceDataTexture[0] = Texture::create2D(128, 128, Falcor::ResourceFormat::RGB32Float, 1, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceDataTexture[1] = Texture::create2D(128, 128, Falcor::ResourceFormat::RGB32Float, 1, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);

            // FIXME kry die afmetinsg van .cfd af
            cfd.sliceVolumeTexture[0][0] = Texture::create3D(128, 32, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[0][1] = Texture::create3D(128, 32, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[0][2] = Texture::create3D(128, 32, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);

            cfd.sliceVolumeTexture[1][0] = Texture::create3D(128, 64, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[1][1] = Texture::create3D(128, 64, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[1][2] = Texture::create3D(128, 64, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);

            cfd.sliceVolumeTexture[2][0] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[2][1] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[2][2] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);

            cfd.sliceVolumeTexture[3][0] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[3][1] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[3][2] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);

            cfd.sliceVolumeTexture[4][0] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[4][1] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[4][2] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);

            cfd.sliceVolumeTexture[5][0] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[5][1] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);
            cfd.sliceVolumeTexture[5][2] = Texture::create3D(128, 128, 128, Falcor::ResourceFormat::R5G6B5Unorm, 1, nullptr, Falcor::Resource::BindFlags::ShaderResource);

            cfdSliceShader.load("Samples/Earthworks_4/hlsl/terrain/render_cfdSlice.hlsl", "vsMain", "psMain", Vao::Topology::TriangleStrip);
            cfdSliceShader.Vars()->setSampler("gSampler", sampler_Trilinear);
            cfdSliceShader.Vars()->setSampler("gSamplerClamp", sampler_ClampAnisotropic);
            cfdSliceShader.Vars()->setTexture("gV", cfd.sliceVTexture[0]);
            cfdSliceShader.Vars()->setTexture("gData", cfd.sliceDataTexture[0]);
            cfdSliceShader.Vars()->setTexture("gV_1", cfd.sliceVTexture[1]);
            cfdSliceShader.Vars()->setTexture("gData_1", cfd.sliceDataTexture[1]);
        }



        vegetation.skyTexture = Texture::createFromFile(settings.dirResource + "/skies/alps_bc.dds", false, true);
        vegetation.envTexture = Texture::createFromFile(settings.dirResource + "/skies/alps_IR_bc.dds", false, true);
        vegetation.dappledLightTexture = Texture::createFromFile(settings.dirResource + "/vegetation/dappled_noise_01.jpg", false, true);
        triangleShader.Vars()->setTexture("gSky", vegetation.skyTexture);
        ribbonShader.Vars()->setTexture("gEnv", vegetation.envTexture);
        vegetationShader.Vars()->setTexture("gEnv", vegetation.envTexture);
        vegetationShader_Bake.Vars()->setTexture("gEnv", vegetation.envTexture);

        

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
            sprintf(name, (settings.dirResource + "/cube.fbx").c_str());

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



        std::cout << "      shaders\n";

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
        split.compute_tileBicubic.Vars()->setTexture("gOutputAlbedo", split.tileFbo->getColorTexture(1));
        split.compute_tileBicubic.Vars()->setTexture("gOutputPermanence", split.tileFbo->getColorTexture(3));
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

    imageCache.resize(45);
    loadImageHash(pRenderContext);

    init_TopdownRender();

    mSpriteRenderer.onLoad();

    mEcosystem.terrainSize = settings.size;
    mEcosystem.load(settings.dirRoot + "/ecosystem/steg.ecosystem", settings.dirResource + "/");    // FIXME MOVE To lastFILE
    terrafectorEditorMaterial::rootFolder = settings.dirResource + "/";
    terrafectors.loadPath(settings.dirRoot + "/terrafectors", settings.dirRoot + "/bake", false);
    mRoadNetwork.rootPath = settings.dirRoot + "/";


    _plantMaterial::static_materials_veg.sb_vegetation_Materials = Buffer::createStructured(sizeof(sprite_material), 1024 * 8);      // just a lot
    plants_Root.onLoad();
    plants_Root.vegetationShader.Vars()->setTexture("gEnv", vegetation.envTexture);
    plants_Root.billboardShader.Vars()->setTexture("gEnv", vegetation.envTexture);
    plants_Root.vegetationShader.Vars()->setTexture("gDappledLight", vegetation.dappledLightTexture);

    
    terrainSpiteShader.Vars()->setBuffer("plant_buffer", plants_Root.plantData);
    terrainSpiteShader.Vars()->setBuffer("materials", _plantMaterial::static_materials_veg.sb_vegetation_Materials);


    std::cout << "      paraglider\n";

    AirSim.setup();
    paraBuilder.setxfoilDir(settings.dirResource + "/xfoil");
    paraBuilder.xfoil_shape("naca4415");
    paraBuilder.buildCp();
    paraBuilder.buildWing();

    //paraBuilder.buildLinesFILE_NOVA_AONIC_medium();
    paraBuilder.buildLines();
    paraBuilder.generateLines();

    paraBuilder.builWingConstraints();
    //paraBuilder.builLineConstraints();

    paraBuilder.visualsPack(paraRuntime.ribbon, paraRuntime.packedRibbons, paraRuntime.ribbonCount, paraRuntime.changed);

    paraRuntime.setxfoilDir(settings.dirResource + "/xfoil");
    paraRuntime.setWing("naca4415");

    //paraRuntime.setWind(settings.dirRoot + "/gis/_export/root4096.bil", glm::normalize(float3(-1, 0, 0.4f)));
    paraRuntime.loadWind(settings.dirRoot + "\\cfd\\");

    paraRuntime.setup(paraBuilder.x, paraBuilder.w, paraBuilder.cross, paraBuilder.spanSize, paraBuilder.chordSize, paraBuilder.constraints, false, cameraViews[CameraType_Main_Center].view);
    paraRuntime.Cp = paraBuilder.Cp;
    memcpy(paraRuntime.CPbrakes, paraBuilder.CPbrakes, sizeof(float) * 6 * 11 * 25);
    //paraRuntime.CPbrakes = paraBuilder.CPbrakes;
    paraRuntime.linesLeft = paraBuilder.linesLeft;
    paraRuntime.linesRight = paraBuilder.linesRight;
    glider.loaded = true;



    newGliderRuntime.importBin();
    newGliderRuntime.solve(0.000f, -1);
    //newGliderRuntime.process_xfoil_Cp(settings.dirResource + "/xfoil/Omega/");
    newGliderRuntime.loadXFoil_Cp(settings.dirResource + "/xfoil/Omega/", 52);  // FIXXXME rib numer is a little hardcoded, and process this to a singel file
    glider.newGliderLoaded = true;


    _swissBuildings buildings;
    //buildings.process(settings.dirRoot + "/buildings/testQGISDXF.dxf");
    //buildings.processGeoJSON(settings.dirRoot + "/buildings/QGIS.geojson");
    //buildings.processGeoJSON("E:/rappersville/", "walls");
    //buildings.processGeoJSON("E:/rappersville/", "roofs");
    //buildings.processWallRoof(settings.dirRoot + "/buildings/");
    //buildings.processWallRoof("E:/rappersville/");
    //buildings.processWallRoof("E:/rappersville/");


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


void terrainManager::loadImageHash(RenderContext* pRenderContext)
{
    /*
    heightMap map;
    int texSize;
    std::ifstream ifs;
    std::string fullpath = settings.dirRoot + "/orthophotos.txt";

    imageTileHashmap.clear();
    ifs.open(fullpath);
    if (ifs)
    {
        while (ifs >> map.lod >> map.y >> map.x >> texSize >> map.origin.x >> map.origin.y >> map.size >> map.filename)
        {
            uint32_t hash = getHashFromTileCoords(map.lod, map.y, map.x);
            map.filename = settings.dirRoot + "/" + map.filename;
            imageTileHashmap[hash] = map;
        }
        ifs.close();
    }
    */

    std::string fullpath = settings.dirRoot + "/orthophotos.json";
    imageDirectory.load(fullpath);
    imageDirectory.cache0(settings.dirRoot + "/orthophoto/");
    //and load 0, 0, 0

    //imageDirectory.cache.set(imageDirectory.files[0].hash, )


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


void terrainManager::onGuiRendercfd(Gui::Window& _window, Gui* pGui, float2 _screen)
{
    cfd.clipmap.slicelod = __max(0, cfd.clipmap.slicelod);
    cfd.clipmap.slicelod = __min(5, cfd.clipmap.slicelod);
    auto& lod = cfd.clipmap.lods[cfd.clipmap.slicelod];

    ImDrawList* draw_list = ImGui::GetWindowDrawList();



    //_renderContext->updateTextureData(cfd.sliceVTexture.get(), cfd.clipmap.sliceV);


    for (int y = 0; y < lod.height; y++)
    {
        for (int x = 0; x < lod.width; x++)
        {
            uint nIndex = (cfd.clipmap.sliceIndex + lod.offset.z) * lod.smap.width + (x + lod.offset.x);
            float4 N = lod.normals[nIndex];
            float Hcell = N.w - lod.offset.y;

            uint idx = (y * 128 + x);

            if (y > Hcell - 1)
            {
                if ((cfd.clipmap.arrayVisualize[idx] >> 24) > 10)
                {
                    ImVec2 bl = { x * 8.f + 400 ,  _screen.y - 50 - y * 8.f };
                    ImVec2 tr = bl;
                    tr.x += 8;
                    tr.y -= 8;

                    draw_list->AddRectFilled(bl, tr, cfd.clipmap.arrayVisualize[idx]);
                }
            }
        }
    }





    {
        ImVec4 col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        const ImU32 col32 = ImColor(col);

        draw_list->AddLine(ImVec2(400, _screen.y - 50), ImVec2(400 + 1024, _screen.y - 50), col32);

        uint nIndex = (cfd.clipmap.sliceIndex + lod.offset.z) * lod.smap.width + (0 + lod.offset.x);
        float4 N = lod.normals[nIndex];
        float Hcell = N.w - lod.offset.y;
        float Hcell_old = Hcell;

        for (int x = 1; x < lod.width; x++)
        {
            nIndex = (cfd.clipmap.sliceIndex + lod.offset.z) * lod.smap.width + (x + lod.offset.x);
            N = lod.normals[nIndex];
            Hcell = N.w - lod.offset.y;

            draw_list->AddLine(ImVec2(404 + (x - 1) * 8.f, _screen.y - 50 - Hcell_old * 8), ImVec2(404 + x * 8.f, _screen.y - 50 - Hcell * 8), col32, 1);

            Hcell_old = Hcell;

        }
    }
}



void terrainManager::onGuiRendercfd_skewT(Gui::Window& _window, Gui* pGui, float2 _screen)
{
    ImVec2 offset = { 400, 200 };
    ImGui::SetCursorPos(offset);

    static bool IsHovered = false;
    static int editMode = 0;

    const ImU32 col32 = ImColor(ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    const ImU32 col32R = ImColor(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    const ImU32 col32B = ImColor(ImVec4(0.0f, 0.0f, 1.0f, 1.0f));
    const ImU32 col32GR = ImColor(ImVec4(0.4f, 0.4f, 0.4f, 0.2f));
    const ImU32 col32YEL = ImColor(ImVec4(0.4f, 0.4f, 0.0f, 1.0f));

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.f, 1.f, 1.f, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
    ImGui::BeginChildFrame(987651, ImVec2(1000, 1120));
    ImGui::PopStyleVar();//ImGuiStyleVar_FramePadding
    ImGui::PushFont(pGui->getFont("roboto_20_bold"));
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        for (int T = -60; T <= 40; T += 2)
        {
            ImVec2 start = { 300.f + 20.f * T, 1000.f };
            ImGui::SetCursorPos(start);
            ImGui::Text("%d", T);

            start.x += offset.x;
            start.y += offset.y;
            ImVec2 end = start;
            end.x += 500;
            end.y -= 1000;
            float width = 1.f;
            if (T % 10 == 0) width = 2.f;
            if (T == 0) width = 3.f;
            draw_list->AddLine(start, end, col32, width);

            // dry lapse
            float Tmp = (float)T;
            ImVec2 P = start;
            ImVec2 P1 = P;
            for (int alt = 0; alt < 5000; alt += 100)
            {
                Tmp -= 100 * 0.0098f;
                P1 = P;
                P.y -= 20;
                P.x = offset.x + 300 + (alt + 100) * 0.1f + Tmp * 20.f;
                draw_list->AddLine(P1, P, col32GR, 3);
            }

            // moist lapse
            Tmp = (float)T;
            P = start;
            P1 = P;
            for (int alt = 0; alt < 5000; alt += 100)
            {
                Tmp -= moistLapse(to_K(Tmp), (float)alt) * 100.f;
                P1 = P;
                P.y -= 20;
                P.x = offset.x + 300 + (alt + 100) * 0.1f + Tmp * 20.f;
                draw_list->AddLine(P1, P, col32GR, 3);
            }
        }

        float3 data = cfd.skewT_data[0];
        ImVec2 P = { offset.x + 300 + data.x * 20.f, offset.y + 1000 };


        for (int j = 1; j < 100; j++)
        {
            data = cfd.skewT_data[j];
            ImVec2 P1 = { offset.x + 300 + data.x * 20.f + j * 5, offset.y + 1000 - j * 10 };
            draw_list->AddLine(P, P1, col32R, 2);
            P = P1;
        }

        data = cfd.skewT_data[0];
        P = { offset.x + 300 + data.y * 20.f, offset.y + 1000 };
        for (int j = 1; j < 100; j++)
        {
            data = cfd.skewT_data[j];
            ImVec2 P1 = { offset.x + 300 + data.y * 20.f + j * 5, offset.y + 1000 - j * 10 };
            draw_list->AddLine(P, P1, col32B, 2);
            P = P1;
        }

        // velocity
        data = cfd.skewT_V[0];
        float V = glm::length(data);
        P = { offset.x + 10 + V * 20.f, offset.y + 1000 };
        for (int j = 1; j < 100; j++)
        {
            data = cfd.skewT_V[j];
            V = glm::length(data);
            ImVec2 P1 = { offset.x + 10 + V * 20.f, offset.y + 1000 - j * 10 };
            draw_list->AddLine(P, P1, col32B, 2);
            P = P1;

            if (j % 2 == 0)
            {
                float3 dir = glm::normalize(data);
                draw_list->AddCircle(P, 3, col32B, 5);
                P1.x += dir.x * pow(V, 0.5f) * 10;
                P1.y += dir.z * pow(V, 0.5f) * 10;
                draw_list->AddLine(P, P1, col32B, 2);
            }
        }

        // wind direction
        data = cfd.skewT_V[0];
        float3 dir = glm::normalize(data);
        int angle = (int)(360.f - atan2(dir.x, dir.z) * 57.2958f) % 360;
        P = { offset.x + 10 + angle * 2.f, offset.y + 1000 };
        for (int j = 1; j < 100; j++)
        {
            data = cfd.skewT_V[j];
            dir = glm::normalize(data);
            angle = (int)(360.f - atan2(dir.x, dir.z) * 57.2958f) % 360;
            ImVec2 P1 = { offset.x + 10 + angle * 2.f, offset.y + 1000 - j * 10 };
            draw_list->AddLine(P, P1, col32YEL, 2);
            P = P1;
        }


        // Mouse overlay
        ImVec2 mouse = ImGui::GetMousePos();
        mouse.x += offset.x;
        draw_list->AddLine(ImVec2(offset.x, mouse.y), ImVec2(offset.x + 1000, mouse.y), col32, 1);

        int alt = (int)(5000 - 5 * (mouse.y - offset.y));
        if (alt > 0)
        {
            ImGui::SetCursorPos(ImVec2(950, mouse.y - offset.y));
            ImGui::Text("%d m", alt);

            ImVec2 p = ImGui::GetMousePos();
            float dy = 1000 + offset.y - p.y;
            uint idx = __max(0, __min(99, (int)(dy / 10)));


            float vspeed = glm::length(cfd.skewT_V[idx]);
            int angle = (int)(360.f - atan2(cfd.skewT_V[idx].x, cfd.skewT_V[idx].z) * 57.2958f) % 360;
            float C_Temp = cfd.skewT_data[idx].x;
            float C_Dew = cfd.skewT_data[idx].y;
            ImVec2 PVel = { 50 + vspeed * 20.f, mouse.y - offset.y };

            ImVec2 PTemp = { 310 + C_Temp * 20.f + idx * 5, mouse.y - offset.y - 20 };
            ImVec2 PDew = { 310 + C_Dew * 20.f + idx * 5, mouse.y - offset.y - 20 };
            ImGui::SetCursorPos(PTemp);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
            ImGui::Text("%dºC", (int)C_Temp);
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(PDew);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 1.f, 1.f));
            ImGui::Text("%dºC", (int)C_Dew);
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(PVel);
            ImGui::Text("%2.1fkt %dº", vspeed * 1.94384f, (int)angle);


            ImVec2 cursor = ImGui::GetMousePos();
            cursor.x -= offset.x;
            cursor.y -= offset.y;
            IsHovered = false;
            if ((cursor.x > 0) && (cursor.x < 1000) && (cursor.y > 0) && (cursor.y < 1000))
            {
                IsHovered = true;
            }
            //ImGui::SetTooltip("%f %f", cursor.x, cursor.y);

            // mouse click
            if (cfd.editMode && IsHovered)
            {


                float T = (p.x - offset.x - 300 - (dy * 0.5f)) / 20.f;


                if (ImGui::IsMouseDown(0))
                {
                    if (ImGui::IsKeyDown((int)Input::Key::LeftControl))     // Smooth
                    {
                        uint idA = __max(1, idx) - 1;
                        uint idB = __min(99, idx + 1);
                        switch (editMode)                                   // Normal
                        {
                        case 0:
                            cfd.skewT_data[idx].x = (cfd.skewT_data[idA].x + cfd.skewT_data[idx].x + cfd.skewT_data[idB].x) / 3;
                            cfd.editChanged = true;
                            break;
                        case 1:
                            cfd.skewT_data[idx].y = (cfd.skewT_data[idA].y + cfd.skewT_data[idx].y + cfd.skewT_data[idB].y) / 3;
                            cfd.editChanged = true;
                            break;
                        case 2:
                        case 3:
                            cfd.skewT_V[idx] = (cfd.skewT_V[idA] + cfd.skewT_V[idx] + cfd.skewT_V[idB]) / 3.f;
                            cfd.editChanged = true;
                            break;
                        }
                    }
                    else
                    {
                        switch (editMode)                                   // Normal
                        {
                        case 0:
                            cfd.skewT_data[idx].x = T;
                            cfd.editChanged = true;
                            break;
                        case 1:
                            cfd.skewT_data[idx].y = T;
                            cfd.editChanged = true;
                            break;
                        case 2:
                        {
                            float V = __max(0.001f, (p.x - offset.x - 10) / 20.f);
                            float3 newV = glm::normalize(cfd.skewT_V[idx]) * V;
                            cfd.skewT_V[idx] = newV;
                            cfd.editChanged = true;
                        }
                        break;
                        case 3:
                        {
                            float angle = (p.x - offset.x - 10) / 114.591f;
                            float v = glm::length(cfd.skewT_V[idx]);
                            cfd.skewT_V[idx] = v * float3(sin(-angle), 0, cos(-angle));
                            cfd.editChanged = true;
                        }
                        break;
                        }
                    }
                }
            }
        }

        if (!cfd.editMode)
        {
            // move to edit, copy data
            cfd.skewT_data = cfd.clipmap.skewTData;
            cfd.skewT_V = cfd.clipmap.skewTV;
            for (int i = 0; i < 100; i++)
            {
                if (glm::length(cfd.skewT_V[i]) < 0.001f)
                {
                    cfd.skewT_V[i].x += 0.001f;
                }
            }
        }




        ImGui::SetCursorPos(ImVec2(10, 1030));
        ImGui::Text(cfd.rootFile.c_str());
        ImGui::NewLine();
        ImGui::SameLine(10, 0);
        if (!cfd.editMode) {
            if (ImGui::Button("Edit Mode"))
            {
                cfd.editMode = true;
                cfd.editChanged = false;
            }
        }
        else {
            if (ImGui::Button("Cancel Edit"))
            {
                cfd.editMode = false;
                cfd.editChanged = false;
            }
            if (cfd.editChanged) {
                TOOLTIP("There are unsaved changes\nClicking this button will discard them");
            }

            ImGui::SameLine(180, 0);
            if (ImGui::Button("Load"))
            {
                std::filesystem::path path = cfd.rootPath;

                FileDialogFilterVec filters = { {"skewT_5000"} };
                if (openFileDialog(filters, path))
                {
                    std::ifstream ifs;
                    ifs.open(path, std::ios::binary);
                    if (ifs)
                    {
                        ifs.read((char*)&cfd.skewT_data, sizeof(float3) * 100);
                        ifs.read((char*)&cfd.skewT_V, sizeof(float3) * 100);
                        ifs.close();
                    }
                }
                cfd.editChanged = false;
                cfd.rootPath = path.parent_path().string() + "\\";
                cfd.rootFile = path.filename().string();
                fprintf(terrafectorSystem::_logfile, "\n%s\n", path.string().c_str());
                fprintf(terrafectorSystem::_logfile, "%s\n", path.parent_path().string().c_str());
                fprintf(terrafectorSystem::_logfile, "%s\n", path.root_path().string().c_str());
                fprintf(terrafectorSystem::_logfile, "path %s   name %s\n", cfd.rootPath.c_str(), cfd.rootFile.c_str());
            }
            ImGui::SameLine(0, 20);
            if (ImGui::Button("Save"))
            {
                std::filesystem::path path = cfd.rootPath;
                FileDialogFilterVec filters = { {"skewT_5000"} };
                if (saveFileDialog(filters, path))
                {
                    std::ofstream ofs;
                    ofs.open(path, std::ios::binary);
                    if (ofs)
                    {
                        ofs.write((char*)&cfd.skewT_data, sizeof(float3) * 100);
                        ofs.write((char*)&cfd.skewT_V, sizeof(float3) * 100);
                        ofs.close();
                    }
                }
                cfd.editChanged = false;
                cfd.rootPath = path.parent_path().string() + "\\";
                cfd.rootFile = path.filename().string();
            }
            ImGui::SameLine(0, 50);
            if (ImGui::Button("Set atmosphere"))
            {
                cfd.clipmap.loadSkewT(cfd.rootPath + cfd.rootFile);
                cfd.clipmap.windrequest = true;
            }
            TOOLTIP("Set this to atmosphere and restart cfd\nThis will only work of the file was saved, cannot set it from memory.");

            ImGui::SameLine(0, 50);
            ImGui::PushItemWidth(200);
            if (ImGui::Combo("###modeSelector", &editMode, "Temperature\0Humidity\0Wind speed\0Wind direction\0")) { ; }
            ImGui::PopItemWidth();

        }


    }
    ImGui::PopFont();
    ImGui::EndChildFrame();
    IsHovered = ImGui::IsItemHovered() || ImGui::IsItemClicked();//EndChildFrame();

    ImVec2 rectMin = ImGui::GetItemRectMin();
    ImVec2 rectMax = ImGui::GetItemRectMax();
    ImVec2 cursor = ImGui::GetCursorPos();
    IsHovered = false;
    if (cursor.x > 0 && cursor.x < 1000 && cursor.y > 0 && cursor.y < 1000)
    {
        IsHovered = true;
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    //ImGui::PopStyleVar(); // frame padding
}


void terrainManager::onGuiRendercfd_params(Gui::Window& _window, Gui* pGui, float2 _screen)
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.75f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 2));
    ImGui::BeginChildFrame(98765, ImVec2(300, _screen.y));
    ImGui::PushFont(pGui->getFont("roboto_20"));
    {
        //ImGui::NewLine();
        //ImGui::Checkbox("profile", &cfd.clipmap.profile);
        //ImGui::NewLine();

        ImGui::Text("      time   maxV  vert  stp   maxP");
        for (int i = 0; i <= 5; i++)
        {
            ImGui::Text("%d - %4d s, %2.1f m/s, %2.1f m/s, %2.1f, %3d", i, (int)cfd.clipmap.lods[i].timer, cfd.clipmap.lods[i].maxSpeed, cfd.clipmap.lods[i].maxSpeed_vertical, cfd.clipmap.lods[i].maxStep, (int)(cfd.clipmap.lods[i].maxP * 1000.f));
        }


        ImGui::NewLine();
        if (ImGui::Button("launch glider from here"))
        {
            paraRuntime.requestRestart = true;
            paraRuntime.usePosDir = true;
            paraRuntime.restartPos = cameraOrigin;
            paraRuntime.restartDir = cameraViews[CameraType_Main_Center].view[2];
            useFreeCamWhileGliding = false;
        }

        ImGui::NewLine();

        ImGui::Checkbox("custom wind", &cfd.clipmap.windSeperateSkewt);
        if (cfd.clipmap.windSeperateSkewt)
        {
            ImGui::Text("Wind speed - %2.1f km/h", glm::length(cfd.clipmap.newWind) * 3.6f);
            if (ImGui::DragFloat3("m/s", &cfd.clipmap.newWind.x, 1, -10, 10, "%1.1f"))
            {
                cfd.clipmap.newWind.y = 0;
            }
        }


        if (ImGui::Button("re-start cfd"))
        {
            cfd.clipmap.windrequest = true;
        }
        TOOLTIP("Set the new wind speed if custom wind is selected above\nand re-start the cfd simulation");





        //ImGui::NewLine();
        //ImGui::DragFloat3("ORIGIN", &cfd.originRequest.x, 10.f, -20000.f, 20000.f, "%4.0f");
        //ImGui::Checkbox("PAUSE cfd", &cfd.pause);


        ImGui::NewLine();
        ImGui::Checkbox("show streamlines", &cfd.clipmap.showStreamlines);
        ImGui::DragFloat("scale", &cfd.clipmap.stremlineScale, 0.001f, 0.0001f, 1);
        ImGui::DragFloat("area", &cfd.clipmap.streamlineAreaScale, 0.01f, 0.01f, 10);

        ImGui::NewLine();
        ImGui::Checkbox("show viz", &cfd.clipmap.showSlice);
        ImGui::Checkbox("skewT", &cfd.clipmap.showskewT);       // shouldn tbe in clipmap

        ImGui::SliderInt("lod", &cfd.clipmap.slicelod, 0, 5);
        ImGui::SliderInt("slice", &cfd.clipmap.sliceIndex, 0, 127);
        ImGui::Text("%d, %d %d", (int)cfd.clipmap.lodOffsets[5][0].x, (int)cfd.clipmap.lodOffsets[5][0].y, (int)cfd.clipmap.lodOffsets[5][0].z);
        ImGui::Text("%2.3f, %2.3f %2.3f", cfd.clipmap.lodScales[5].x, cfd.clipmap.lodScales[5].y, cfd.clipmap.lodScales[5].z);

        ImGui::NewLine();
        ImGui::DragFloat("relax", &cfd.clipmap.incompres_relax, 0.01f, 1.0f, 2.0f);


        ImGui::DragInt("num Inc", &cfd.clipmap.incompres_loop, 1, 1, 100);
        ImGui::DragFloat("vort_confine", &cfd.clipmap.vort_confine, 0.01f, 0.0f, 1.0f);

        ImGui::NewLine();
        ImGui::DragFloat("sun heat", &cfd.clipmap.sun_heat_scale, 0.01f, 0.0f, 1.0f);
        TOOLTIP("amount of sun heat to apply to the earth\nPlease not that this is testing phase still and applies it as if its 3pm\nit ignores the time of day settings");



        ImGui::Text("%d, %f", cfd.clipmap.sliceOrder[3], cfd.lodLerp[3]);

        /*
        if (cfd.clipmap.showSlice)
        {
            cfd.clipmap.slicelod = __max(0, cfd.clipmap.slicelod);
            cfd.clipmap.slicelod = __min(5, cfd.clipmap.slicelod);
            auto& lod = cfd.clipmap.lods[cfd.clipmap.slicelod];


            ImGui::Text("(%d, %d, %d) abs", lod.offset.x, lod.offset.y, lod.offset.z);
            ImGui::Text("(%d, %d, %d) rel", lod.offset.x % lod.width, lod.offset.y % lod.height, lod.offset.z % lod.width);

            ImGui::NewLine();
            ImGui::Text("%d ms - total", (int)lod.solveTime_ms);
            ImGui::Text("fromR %d ms", (int)lod.simTimeLod_fromRoot_ms);
            ImGui::Text("to R %d ms", (int)lod.simTimeLod_toRoot_ms);
            //ImGui::Text("blocks %d ms (%d / 4096)", (int)lod.simTimeLod_blocks_ms, lod.numBlocks);
            ImGui::Text("boyancy %d ms", (int)lod.simTimeLod_boyancy_ms);
            ImGui::Text("icmpres %d ms", (int)lod.simTimeLod_incompress_ms);
            ImGui::Text("advect  %d ms", (int)lod.simTimeLod_advect_ms);
            ImGui::Text("diffuse %d ms", (int)lod.simTimeLod_edges_ms);

        }
        */
    }

    ImGui::EndChildFrame();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    if (cfd.clipmap.showskewT)
    {

        //_window.image("testImage", cfd.sliceDataTexture, float2(1024, 1024));
        //onGuiRendercfd(_window, pGui, _screen);
        onGuiRendercfd_skewT(_window, pGui, _screen);
    }
    ImGui::PopFont();
}


// The game GUI
//fogAtmosphericParams params;
void terrainManager::onGuiRenderParaglider(Gui::Window& _window, Gui* pGui, float2 _screen, fogAtmosphericParams* pAtmosphere)
{
    if (renderGui_Menu)
    {
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.22f, 0.21f, 0.2f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        {
            ImGui::PushFont(pGui->getFont("H1"));
            {
                ImGui::BeginChildFrame(12734, ImVec2(400, _screen.y));
                {
                    //ImGui::SetCursorPos(ImVec2(screenSize.x - 100, 0));
                    ImGui::NewLine();
                    ImGui::SameLine(30, 0);
                    ImGui::Text("%3.1f fps", 1000.0 / gpFramework->getFrameRate().getAverageFrameTime());

                    ImGui::NewLine();
                    if (ImGui::Button("Glider", ImVec2(400, 0))) {
                    }
                    if (ImGui::Button("Wind", ImVec2(400, 0))) {
                        ImGui::OpenPopup("WindPopup");
                    }
                    if (ImGui::Button("Time", ImVec2(400, 0))) {
                        ImGui::OpenPopup("TodPopup");
                    }
                    if (ImGui::Button("Launch", ImVec2(400, 0))) {
                        paraRuntime.requestRestart = true;
                    }
                    if (ImGui::Button("Save path", ImVec2(400, 0))) {
                        std::ofstream ofs;
                        ofs.open("e:/flightpath.raw", std::ios::binary);
                        if (ofs)
                        {
                            ofs.write((char*)&paraRuntime.recordIndex, sizeof(int));
                            ofs.write((char*)&paraRuntime.pathRecord, sizeof(float3) * paraRuntime.recordIndex);
                            ofs.close();
                        }
                    }
                    if (ImGui::Button("Settings", ImVec2(400, 0))) {
                    }
                    ImGui::NewLine();
                    if (ImGui::Button("Quit", ImVec2(400, 0))) {
                        gpFramework->getWindow()->shutdown();   // fixme stop aal threads first, do gently
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushFont(pGui->getFont("roboto_20"));
                    {
                        ImGui::SetCursorPos(ImVec2(30, _screen.y - 65));
                        ImGui::Text("© 2024 Johan Hammes.");
                        ImGui::SetCursorPos(ImVec2(30, _screen.y - 40));
                        ImGui::Text("hammes.jc@gmail.com");
                    }
                    ImGui::PopFont();
                    ImGui::PopStyleColor();


                    if (ImGui::BeginPopup("WindPopup")) {
                        // Draw popup contents.
                        ImGui::PushFont(pGui->getFont("roboto_48"));
                        {
                            /*
                            ImGui::Text("Wind speed - %2.1f km/h", glm::length(cfd.clipmap.newWind) * 3.6f);
                            ImGui::SetNextItemWidth(300);
                            if (ImGui::DragFloat3("m/s", &cfd.clipmap.newWind.x, 1, -10, 10, "%1.1f"))
                            {
                                cfd.clipmap.newWind.y = 0;
                            }
                            ImGui::NewLine();
                            if (ImGui::Button("change"))
                            {
                                cfd.clipmap.windrequest = true;
                            }
                            */

                            ImGui::NewLine();
                            ImGui::Checkbox("show streamlines", &cfd.clipmap.showStreamlines);


                        }
                        ImGui::PopFont();

                        ImGui::EndPopup();
                    }

                    if (ImGui::BeginPopup("TodPopup")) {
                        // Draw popup contents.
                        ImGui::PushFont(pGui->getFont("roboto_48"));
                        {
                            ImGui::Text("Time of day");
                            ImGui::SetNextItemWidth(300);
                            ImGui::DragFloat("angle", &shadowEdges.sunAngle, 0.01f, 0, 3.14f, "%1.2f");
                            ImGui::NewLine();
                            if (ImGui::Button("change"))
                            {
                                shadowEdges.requestNewShadow = true;
                            }

                            ImGui::SetNextItemWidth(300);
                            ImGui::DragFloat("haze turbidity", &pAtmosphere->haze_Turbidity, 0.01f, 1.f, 15.f, "%1.2f");

                            ImGui::SetNextItemWidth(300);
                            ImGui::DragFloat("fog turbidity", &pAtmosphere->fog_Turbidity, 0.01f, 1.f, 125.f, "%1.2f");

                            ImGui::SetNextItemWidth(300);
                            ImGui::DragFloat("fog base", &pAtmosphere->fog_BaseAltitudeKm, 0.001f, 0.f, 1.f, "%1.2fkm");

                            ImGui::SetNextItemWidth(300);
                            ImGui::DragFloat("fog height", &pAtmosphere->fog_AltitudeKm, 0.001f, 0.05f, 1.f, "%1.2f");


                        }
                        ImGui::PopFont();

                        ImGui::EndPopup();
                    }
                }
                ImGui::EndChildFrame();
            }
            ImGui::PopFont();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    }

    else if (renderGui_Hud)
    {
        if (useFreeCamWhileGliding)
        {
            // cfd
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.4f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.2f, 0.2f, 0.2f, 0.1f));


            onGuiRendercfd_params(_window, pGui, _screen);



            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
        }
        else
        {
            paraRuntime.renderHUD(pGui, _screen, !GliderDebugVisuals);

            if (GliderDebugVisuals)
            {

                if (glider.newGliderLoaded)
                {
                    newGliderRuntime.renderDebug(pGui, _screen);
                    //paraRuntime.renderDebug(pGui, _screen);
                }
                else
                {
                    paraRuntime.renderDebug(pGui, _screen);
                }
            }
        }
    }
}




void terrainManager::onGuiRender(Gui* _gui, fogAtmosphericParams* pAtmosphere)
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
        if (ImGui::Combo("###modeSelector", (int*)&terrainMode, "Vegetation\0Ecotope\0Terrafector\0Bezier road network\0Glider Builder\0")) { ; }
        ImGui::PopItemWidth();
        ImGui::NewLine();

        auto& oldStyle = ImGui::GetStyle();
        auto& style = ImGui::GetStyle();
        //style.Colors[ImGuiCol_Button] = ImVec4(0.03f, 0.03f, 0.3f, 0.5f);

        switch (terrainMode)
        {
        case _terrainMode::vegetation:
        {
            ImGui::Text("test");
            ImGui::DragInt("# instances", &ribbonInstanceNumber, 1, 1, 1000);
            ImGui::DragFloat("spacing", &ribbonSpacing, 0.01f, 0.1f, 10);
            ImGui::Checkbox("from extents", &spacingFromExtents);

            ImGui::Text("%d inst x %d x 2 = %3.1f M tris", ribbonInstanceNumber * ribbonInstanceNumber, groveTree.numBranchRibbons, ribbonInstanceNumber * ribbonInstanceNumber * groveTree.numBranchRibbons * 2.0f / 1000000.0f);
            groveTree.renderGui(_gui);
            plants_Root.renderGui(_gui);
            if (ImGui::Button("Bake Billboard")) { bakeVegetation(); }

            ImGui::PushFont(_gui->getFont("roboto_26"));
            {
                ImGui::Text("Time of day");
                ImGui::SetNextItemWidth(300);
                ImGui::DragFloat("angle", &shadowEdges.sunAngle, 0.01f, 0, 3.14f, "%1.2f");
                ImGui::NewLine();
                if (ImGui::Button("change"))
                {
                    shadowEdges.requestNewShadow = true;
                }

                ImGui::SetNextItemWidth(300);
                ImGui::DragFloat("haze turbidity", &pAtmosphere->haze_Turbidity, 0.01f, 1.f, 15.f, "%1.2f");

                ImGui::SetNextItemWidth(300);
                ImGui::DragFloat("fog turbidity", &pAtmosphere->fog_Turbidity, 0.01f, 1.f, 125.f, "%1.2f");

                ImGui::SetNextItemWidth(300);
                ImGui::DragFloat("fog base", &pAtmosphere->fog_BaseAltitudeKm, 0.001f, 0.f, 1.f, "%1.2fkm");

                ImGui::SetNextItemWidth(300);
                ImGui::DragFloat("fog height", &pAtmosphere->fog_AltitudeKm, 0.001f, 0.05f, 1.f, "%1.2f");


            }
            ImGui::PopFont();
        }
        break;

        case _terrainMode::ecotope:
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

        case _terrainMode::terrafector:
        {
            //terrafectors.renderGui(_gui);
            ImGui::NewLine();
            //roadMaterialCache::getInstance().renderGui(_gui);

            ImGui::Text("Stamps");
            ImGui::Text("%d - stamps", mRoadStampCollection.stamps.size());
            ImGui::Text("%d - materials", mRoadStampCollection.materialMap.size());


            if (ImGui::Button("Load - Previous"))
            {
                loadStamp();
            }
            if (ImGui::Button("Load"))
            {
                FileDialogFilterVec filters = { {"stamps"} };
                if (openFileDialog(filters, mRoadStampCollection.lastUsedFilename))
                {
                    loadStamp();
                    reset(true);
                }
            }
            if (ImGui::Button("Save"))
            {
                FileDialogFilterVec filters = { {"stamps"} };
                if (saveFileDialog(filters, mRoadStampCollection.lastUsedFilename))
                {
                    saveStamp();
                }
            }

        }
        break;

        case _terrainMode::roads:
        {
            style.Colors[ImGuiCol_Button] = ImVec4(0.03f, 0.03f, 0.3f, 0.5f);
            mRoadNetwork.renderGUI(_gui);


            //if (ImGui::Button("bake - EVO", ImVec2(W, 0))) { bake(false); }
            //if (ImGui::Button("bake - MAX", ImVec2(W, 0))) { bake(true); }

            if (ImGui::Button("bezier -> lod4")) { bezierRoadstoLOD(4); }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Forces update of road GPU data");

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

        case _terrainMode::glider:
            ImGui::Text("Wing designer");

            if (ImGui::Button("restart"))
            {
                paraRuntime.requestRestart = true;
                //paraRuntime.setup(paraBuilder.x, paraBuilder.w, paraBuilder.cross, paraBuilder.spanSize, paraBuilder.chordSize, paraBuilder.constraints);
                //paraRuntime.Cp = paraBuilder.Cp;
                //paraRuntime.linesLeft = paraBuilder.linesLeft;
                //paraRuntime.linesRight = paraBuilder.linesRight;
            }
            ImGui::Text("simtime  %3.2fms       pack(%3.1f)", glider.frameTime, glider.packTime);
            ImGui::Text("cells  A - %3.2f    B - %3.2f", glider.cellsTime[0], glider.cellsTime[1]);
            ImGui::Text("pre    A - %3.2f    B - %3.2f", glider.preTime[0], glider.preTime[1]);
            ImGui::Text("wing   A - %3.2f    B - %3.2f", glider.wingTime[0], glider.wingTime[1]);
            ImGui::Text("post   A - %3.2f    B - %3.2f", glider.postTime[0], glider.postTime[1]);
            paraRuntime.renderGui(_gui);
            break;

        }
        ImGui::PopFont();

        style = oldStyle;   // reset it
    }
    rightPanel.release();

    switch (terrainMode)
    {
    case _terrainMode::vegetation:
    {
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
    case _terrainMode::ecotope:
    {
        Gui::Window ecotopePanel(_gui, "Ecotope##tfPanel", { 900, 900 }, { 100, 100 });
        {
            mEcosystem.renderSelectedGUI(_gui);
        }
        ecotopePanel.release();
    }
    break;
    case _terrainMode::terrafector:
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
        Gui::Window terrafectorMaterialPanel_2(_gui, "Terrafector materials##2", { 900, 900 }, { 100, 100 });
        {
            terrafectorEditorMaterial::static_materials.renderGui(_gui, terrafectorMaterialPanel_2);
        }
        terrafectorMaterialPanel_2.release();
    }
    break;
    case _terrainMode::roads:

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
                roadPanel.windowSize(180 + fullWidth * 460, 0);

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

            if (terrainMode == _terrainMode::terrafector)
            {
                sprintf(TTTEXT, "(%3.1f, %3.1f, %3.1f)\nheight %2.2fm\nscale %2.2f %2.2f",
                    split.feedback.tum_Position.x, split.feedback.tum_Position.y, split.feedback.tum_Position.z,
                    mCurrentStamp.height, mCurrentStamp.scale.x, mCurrentStamp.scale.y);
            }
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


// Fixme input files to a list on disk
void terrainManager::writeGdal(jp2Map _map, std::ofstream& _gdal, std::string _input)
{
    std::string imageName = "img_" + std::to_string(_map.lod) + "_" + std::to_string(_map.y) + "_" + std::to_string(_map.x);
    //_gdal << "gdalwarp -t_srs \"+proj=tmerc +lat_0=47.27 +lon_0=9.07 +k_0=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m\" -te ";
    _gdal << "gdalwarp -t_srs " << settings.projection;
    _gdal << " -te " << _map.origin.x << " " << (-1 * _map.origin.y) - _map.size << " " << _map.origin.x + _map.size << " " << (-1 * _map.origin.y) << " ";
    _gdal << "-ts 1024 1024 -r cubicspline -multi -overwrite -ot byte ";
    _gdal << _input << " ";
    _gdal << "../_temp/" << imageName << ".tif \n";
    _gdal << "gdal_translate -ot byte ../_temp/" << imageName << ".tif ../_temp/" << imageName << ".bil \n\n";
}

void terrainManager::writeGdalClear(std::ofstream& _gdal)
{
    _gdal << "\n\n";
    _gdal << "del \"../_temp/*.tif\"\n";
    _gdal << "del \"../_temp/*.hdr\"\n";
    _gdal << "del \"../_temp/*.prj\"\n";
    _gdal << "del \"../_temp/*.xml\"\n";
    _gdal << "\n\n";
}


void jp2Map::set(uint _lod, uint _y, uint _x, float _wSize, float _wOffset)
{
    lod = _lod;
    y = _y;
    x = _x;

    float scale = 1.f / (float)pow(2, lod);
    float sizeT = _wSize * scale;
    float sizeTotal = sizeT * tile_toBorder;
    float sizeBorder = (sizeTotal - sizeT) / 2.f;

    size = sizeTotal;
    origin.x = _wOffset - sizeBorder + (x * sizeT);
    origin.y = _wOffset - sizeBorder + (y * sizeT);
}


void jp2Map::save(std::ofstream& _os)
{
    _os << lod << " " << y << " " << x << " " << origin.x << " " << origin.y << " " << size << " ";
    _os << hgt_offset << " " << hgt_scale << " " << fileOffset << "\n";
}

void jp2Map::saveBinary(std::ofstream& _os)
{
    _os << lod << y << x << origin.x << origin.y << size << hgt_offset << hgt_scale << fileOffset;
}

void jp2Map::loadBinary(std::ifstream& _is)
{
    _is >> lod >> y >> x >> origin.x >> origin.y >> size >> hgt_offset >> hgt_scale >> fileOffset;
}




void jp2File::save(std::ofstream& _os)
{
    _os << filename << "\n";
    for (auto& T : tiles)
    {
        T.save(_os);
    }
}

void jp2File::saveBinary(std::ofstream& _os)
{
    _os << filename << "\n";
    uint numTiles = tiles.size();
    _os << numTiles;
    for (auto& T : tiles)
    {
        T.saveBinary(_os);
    }
}

void jp2File::loadBinary(std::ifstream& _is)
{
    uint numTiles;
    _is >> filename;
    _is >> numTiles;
    for (int i = 0; i < numTiles; i++)
    {
        tiles.emplace_back();
        tiles.back().loadBinary(_is);
    }
}





void jp2Dir::save(std::string _name)
{
    std::ofstream os(_name.c_str());
    if (os.good()) {
        cereal::JSONOutputArchive archive(os);
        serialize(archive);
        /*
        for (auto F : files)
        {
            F.save(os);
        }*/
    }
}

void jp2Dir::load(std::string _name)
{
    std::ifstream is(_name.c_str());
    if (is.good()) {
        cereal::JSONInputArchive archive(is);
        serialize(archive);
    }

    fileHashmap.clear();
    tileHash.clear();
    for (int i = 0; i < files.size(); i++)
    {
        fileHashmap[files[i].hash] = i;


        //if (i == 0)// TMEP totdat ek .bin strem en cache
        {
            for (int j = 0; j < files[i].tiles.size(); j++)
            {
                uint32_t hash = getHashFromTileCoords(files[i].tiles[j].lod, files[i].tiles[j].y, files[i].tiles[j].x);
                tileHash[hash] = uint2(i, j);
            }
        }
    }

    cache.resize(40);   // can add up if I get this wrong
}


//#pragma optimize("", off)
void jp2Dir::cache0(std::string _path)
{
    path = _path;   // save for rest of session

    dataRoot.clear();
    dataRoot.resize(files[0].sizeInBytes);
    std::ifstream is(_path + files[0].filename, std::ios::binary);
    if (is.good()) {
        is.read((char*)dataRoot.data(), files[0].sizeInBytes);
        is.close();
    }
    /*
    std::vector<unsigned char> data(files[0].sizeInBytes);
    std::ifstream is(files[0].filename, std::ios::binary);
    if (is.good()) {
        is.read((char *)data.data(), files[0].sizeInBytes);
        auto sharedPtr = std::make_shared<std::vector<uint8_t>>(data);
        cache.set(files[0].hash, sharedPtr);
        is.close();
    }
    */
}

//std::thread t1(imageDirectory.cacheHash(), hash);
void jp2Dir::cacheHash(uint32_t hash)
{
    if (hash == 0) return;  // because that is all pre loaded

    std::map<uint32_t, uint>::iterator file_it = fileHashmap.find(hash);
    if (file_it != fileHashmap.end())
    {
        // such a file exists
        uint idx = file_it->second;
        std::shared_ptr<std::vector<unsigned char>> data;
        if (!cache.get(hash, data))
        {
            // but not cahched, so cache it
            std::vector<unsigned char> dataRoot;
            dataRoot.resize(files[idx].sizeInBytes);
            std::ifstream is(path + files[idx].filename, std::ios::binary);
            if (is.good()) {
                is.read((char*)dataRoot.data(), files[idx].sizeInBytes);
                is.close();
            }
            auto share = std::make_shared<std::vector<unsigned char>>(dataRoot);
            cache.set(hash, share);
        }
    }
}


void jp2Dir::saveBinary(std::string _name)
{
    std::ofstream os(_name.c_str(), std::ios::binary);
    if (os.good()) {
        uint numFiles = files.size();
        os << numFiles;
        for (auto F : files)
        {
            F.saveBinary(os);
        }
    }
}

void jp2Dir::loadBinary(std::string _name)
{
    std::ifstream is(_name.c_str(), std::ios::binary);
    if (is.good()) {
        uint numFiles;
        is >> numFiles;
        for (int i = 0; i < numFiles; i++)
        {
            files.emplace_back();
            files.back().loadBinary(is);
        }
    }
}



void terrainManager::generateGdalPhotos()
{
    jp2Dir data;
    jp2Map _mapElement;
    jp2Dir jp2;
    std::ofstream of_gdal;
    std::string inLow = "Images_2m.tif";
    std::string inAll = "Images_2m.tif A1.tif A2.tif A3.tif B1.tif B2.tif B3.tif C1.tif C2.tif C3.tif D1.tif D2.tif D3.tif ";

    std::ifstream ifLOD68map;
    unsigned char map[256][256];

    ifLOD68map.open("E:/terrains/switserland_Steg/gis/photos/LOD_6_8.raw", std::ios::binary);
    if (ifLOD68map)
    {
        ifLOD68map.read((char*)&map[0][0], 256 * 256);
        ifLOD68map.close();
    }

    of_gdal.open("E:/terrains/switserland_Steg/gis/photos/gdal_photos_NEW.bat");
    if (of_gdal)
    {
        jp2.files.emplace_back();
        jp2.files.back().filename = "0_0_0.bin";
        jp2.files.back().hash = getHashFromTileCoords(0, 0, 0);

        _mapElement.set(0, 0, 0);
        jp2.files.back().tiles.push_back(_mapElement);
        writeGdal(_mapElement, of_gdal, inLow);

        for (uint y = 0; y < 4; y++) {
            for (uint x = 0; x < 4; x++) {
                _mapElement.set(2, y, x);
                jp2.files.back().tiles.push_back(_mapElement);
                writeGdal(_mapElement, of_gdal, inLow);
            }
        }

        for (uint y = 0; y < 16; y++) {
            for (uint x = 0; x < 16; x++) {
                _mapElement.set(4, y, x);
                jp2.files.back().tiles.push_back(_mapElement);
                writeGdal(_mapElement, of_gdal, inLow);
            }
        }

        writeGdalClear(of_gdal);



        std::vector<jp2Map> block_files;
        for (uint ty = 0; ty < 16; ty++)
        {
            for (uint tx = 0; tx < 16; tx++)
            {
                block_files.clear();

                for (uint y = ty * 4; y < ty * 4 + 4; y++) {
                    for (uint x = tx * 4; x < tx * 4 + 4; x++) {
                        if (map[y * 4][x * 4] > 100)
                        {
                            _mapElement.set(6, y, x);
                            block_files.push_back(_mapElement);
                            writeGdal(_mapElement, of_gdal, inAll);
                        }
                    }
                }

                for (uint y = ty * 16; y < ty * 16 + 16; y++) {
                    for (uint x = tx * 16; x < tx * 16 + 16; x++) {
                        if (map[y][x] > 200)
                        {
                            _mapElement.set(8, y, x);
                            block_files.push_back(_mapElement);
                            writeGdal(_mapElement, of_gdal, inAll);
                        }
                    }
                }

                // push if we have any
                if (block_files.size() > 0)
                {
                    jp2.files.emplace_back();
                    jp2.files.back().filename = "4_" + std::to_string(ty) + "_" + std::to_string(tx) + ".bin";
                    jp2.files.back().hash = getHashFromTileCoords(4, ty, tx);
                    for (auto& t : block_files) {
                        jp2.files.back().tiles.push_back(t);
                    }

                    writeGdalClear(of_gdal);
                }
            }
        }



        of_gdal.close();

        jp2.save("E:/terrains/switserland_Steg/gis/photos/photo_tiles.json");
        //jp2.saveBinary("E:/terrains/switserland_Steg/gis/photos/photo_tiles.bin");

        //jp2Dir jp2Test;
        //jp2Test.loadBinary("E:/terrains/switserland_Steg/gis/photos/photo_tiles.bin");
        /*
        std::ofstream os("E:/terrains/switserland_Steg/gis/photos/photo_tiles.json");
        if (os.good()) {
            cereal::JSONOutputArchive archive(os);
            jp2.serialize(archive);
        }*/
    }

    return;


    float leftT, bottomT, rightT, topT, sizeT, sizeBrd, sizeTotal;
    int lod = 0;
    int x = 0;
    int y = 0;
    float sizeWorld = 40000;
    float offset = -20000;
    float scale;




    std::ofstream ofs, ofSummary;
    //ofs.open(settings.dirGis + "/photos/gdal_photos.bat");
    ofSummary.open("E:/terrains/switserland_Steg/gis/photos/gdal_photos_0_2_4.txt");
    ofs.open("E:/terrains/switserland_Steg/gis/photos/gdal_photos_0_2_4.bat");
    if (ofs)
    {
        jp2Map _mapElement;
        lod = 0;
        scale = 1.f / (float)pow(2, lod);
        sizeT = sizeWorld * scale;
        sizeTotal = sizeT * tile_toBorder;
        sizeBrd = (sizeTotal - sizeT) / 2.f;

        leftT = offset - sizeBrd;
        bottomT = offset - sizeBrd;
        rightT = leftT + sizeTotal;
        topT = bottomT + sizeTotal;

        std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
        ofs << "gdalwarp -t_srs \"+proj=tmerc +lat_0=47.27 +lon_0=9.07 +k_0=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m\" -te ";
        ofs << leftT << " " << bottomT << " " << rightT << " " << topT << " ";
        ofs << "-ts 1024 1024 -r cubicspline -multi -overwrite -ot byte ";
        ofs << "Images_2m.tif ";
        //ofs << "Images_2m.tif A1.tif  A2.tif  A3.tif B1.tif  B2.tif  B3.tif ";
        //ofs << "C1.tif  C2.tif  C3.tif D1.tif  D2.tif  D3.tif ";
        ofs << "../_temp/" << imageName << ".tif \n";
        ofs << "gdal_translate -ot byte ../_temp/" << imageName << ".tif ../_temp/" << imageName << ".bil \n\n\n";
        ofs << "\n\n\n\n\n";

        ofSummary << lod << " " << y << " " << x << " 1024 " << leftT << " " << topT * -1 << " " << sizeTotal;
        ofSummary << " orthophoto/" << imageName << ".jp2\n";


        lod = 2;
        scale = 1.f / (float)pow(2, lod);
        sizeT = sizeWorld * scale;
        sizeTotal = sizeT * tile_toBorder;
        sizeBrd = (sizeTotal - sizeT) / 2.f;

        for (y = 0; y < 4; y++)
        {
            for (x = 0; x < 4; x++)
            {
                leftT = offset + x * sizeT - sizeBrd;
                bottomT = offset + (3 - y) * sizeT - sizeBrd;
                rightT = leftT + sizeTotal;
                topT = bottomT + sizeTotal;

                std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                ofs << "gdalwarp -t_srs \"+proj=tmerc +lat_0=47.27 +lon_0=9.07 +k_0=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m\" -te ";
                ofs << leftT << " " << bottomT << " " << rightT << " " << topT << " ";
                ofs << "-ts 1024 1024 -r cubicspline -multi -overwrite -ot byte ";
                ofs << "Images_2m.tif ";
                //ofs << "Images_2m.tif A1.tif  A2.tif  A3.tif B1.tif  B2.tif  B3.tif ";
                //ofs << "C1.tif  C2.tif  C3.tif D1.tif  D2.tif  D3.tif ";
                ofs << "../_temp/" << imageName << ".tif \n";
                ofs << "gdal_translate -ot byte ../_temp/" << imageName << ".tif ../_temp/" << imageName << ".bil \n\n\n";

                ofSummary << lod << " " << y << " " << x << " 1024 " << leftT << " " << topT * -1 << " " << sizeTotal;
                ofSummary << " orthophoto/" << imageName << ".jp2\n";
            }
        }


        lod = 4;
        scale = 1.f / (float)pow(2, lod);
        sizeT = sizeWorld * scale;
        sizeTotal = sizeT * tile_toBorder;
        sizeBrd = (sizeTotal - sizeT) / 2.f;

        for (y = 0; y < 16; y++)
        {
            for (x = 0; x < 16; x++)
            {
                leftT = offset + x * sizeT - sizeBrd;
                bottomT = offset + (15 - y) * sizeT - sizeBrd;
                rightT = leftT + sizeTotal;
                topT = bottomT + sizeTotal;

                std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                ofs << "gdalwarp -t_srs \"+proj=tmerc +lat_0=47.27 +lon_0=9.07 +k_0=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m\" -te ";
                ofs << leftT << " " << bottomT << " " << rightT << " " << topT << " ";
                ofs << "-ts 1024 1024 -r cubicspline -multi -overwrite -ot byte ";
                ofs << "Images_2m.tif ";
                //ofs << "Images_2m.tif A1.tif  A2.tif  A3.tif B1.tif  B2.tif  B3.tif ";
                //ofs << "C1.tif  C2.tif  C3.tif D1.tif  D2.tif  D3.tif ";
                ofs << "../_temp/" << imageName << ".tif \n";
                ofs << "gdal_translate -ot byte ../_temp/" << imageName << ".tif ../_temp/" << imageName << ".bil \n\n\n";

                ofSummary << lod << " " << y << " " << x << " 1024 " << leftT << " " << topT * -1 << " " << sizeTotal;
                ofSummary << " orthophoto/" << imageName << ".jp2\n";
            }
        }

        ofs.close();
        ofSummary.close();
    }


    //std::ifstream ifLOD68map;
    //unsigned char map[256][256];

    ifLOD68map.open("E:/terrains/switserland_Steg/gis/photos/LOD_6_8.raw", std::ios::binary);
    if (ifLOD68map)
    {
        ifLOD68map.read((char*)&map[0][0], 256 * 256);
        ifLOD68map.close();

        // Step through at LOD 4
        for (uint ty = 0; ty < 16; ty++)
        {
            for (uint tx = 0; tx < 16; tx++)
            {
                std::string name = "E:/terrains/switserland_Steg/gis/photos/gdal_photos_lod4_";
                name += std::to_string(ty) + "_" + std::to_string(tx) + ".txt";
                ofSummary.open(name.c_str());
                name += ".bat";
                ofs.open(name.c_str());
                if (ofs)
                {
                    lod = 6;
                    scale = 1.f / (float)pow(2, lod);
                    sizeT = sizeWorld * scale;
                    sizeTotal = sizeT * tile_toBorder;
                    sizeBrd = (sizeTotal - sizeT) / 2.f;

                    for (y = ty * 4; y < ty * 4 + 4; y++)
                    {
                        for (x = tx * 4; x < tx * 4 + 4; x++)
                        {
                            if (map[y * 4][x * 4] > 100)
                            {
                                leftT = offset + x * sizeT - sizeBrd;
                                bottomT = offset + (63 - y) * sizeT - sizeBrd;
                                rightT = leftT + sizeTotal;
                                topT = bottomT + sizeTotal;

                                std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                                ofs << "gdalwarp -t_srs \"+proj=tmerc +lat_0=47.27 +lon_0=9.07 +k_0=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m\" -te ";
                                ofs << leftT << " " << bottomT << " " << rightT << " " << topT << " ";
                                ofs << "-ts 1024 1024 -r cubicspline -multi -overwrite -ot byte ";
                                ofs << "Images_2m.tif ";
                                ofs << "A1.tif  A2.tif  A3.tif B1.tif  B2.tif  B3.tif ";
                                ofs << "C1.tif  C2.tif  C3.tif D1.tif  D2.tif  D3.tif ";
                                ofs << "../_temp/" << imageName << ".tif \n";
                                ofs << "gdal_translate -ot byte ../_temp/" << imageName << ".tif ../_temp/" << imageName << ".bil \n\n\n";


                                ofSummary << lod << " " << y << " " << x << " 1024 " << leftT << " " << topT * -1 << " " << sizeTotal;
                                ofSummary << " orthophoto/" << imageName << ".jp2\n";
                            }
                        }
                    }




                    lod = 8;
                    scale = 1.f / (float)pow(2, lod);
                    sizeT = sizeWorld * scale;
                    sizeTotal = sizeT * tile_toBorder;
                    sizeBrd = (sizeTotal - sizeT) / 2.f;

                    for (y = ty * 16; y < ty * 16 + 16; y++)
                    {
                        for (x = tx * 16; x < tx * 16 + 16; x++)
                        {
                            if (map[y][x] > 200)
                            {
                                leftT = offset + x * sizeT - sizeBrd;
                                bottomT = offset + (255 - y) * sizeT - sizeBrd;
                                rightT = leftT + sizeTotal;
                                topT = bottomT + sizeTotal;

                                std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                                ofs << "gdalwarp -t_srs \"+proj=tmerc +lat_0=47.27 +lon_0=9.07 +k_0=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m\" -te ";
                                ofs << leftT << " " << bottomT << " " << rightT << " " << topT << " ";
                                ofs << "-ts 1024 1024 -r cubicspline -multi -overwrite -ot byte ";
                                ofs << "Images_2m.tif ";
                                ofs << "A1.tif  A2.tif  A3.tif B1.tif  B2.tif  B3.tif ";
                                ofs << "C1.tif  C2.tif  C3.tif D1.tif  D2.tif  D3.tif ";
                                ofs << "../_temp/" << imageName << ".tif \n";
                                ofs << "gdal_translate -ot byte ../_temp/" << imageName << ".tif ../_temp/" << imageName << ".bil \n\n\n";

                                ofSummary << lod << " " << y << " " << x << " 1024 " << leftT << " " << topT * -1 << " " << sizeTotal;
                                ofSummary << " orthophoto/" << imageName << ".jp2\n";
                            }
                        }
                    }


                    ofSummary.close();
                    ofs.close();
                }
            }
        }
    }
    /*

        // steg 6 y 12-20  x 12-24
        // steg 8 y 32-40  x 29-33

        // walensee 6  y 48-64 x 20-64
        // walensee 8  y 112-120  x 120-128
        ofs.open("E:/terrains/switserland_Steg/gis/photos/gdal_photos_lod_steg.bat");
        if (ofs)
        {
            lod = 6;
            scale = 1.f / (float)pow(2, lod);
            sizeT = sizeWorld * scale;
            sizeTotal = sizeT * tile_toBorder;
            sizeBrd = (sizeTotal - sizeT) / 2.f;

            for (y = 12; y < 20; y++)
            {
                for (x = 12; x < 24; x++)
                {
                    leftT = offset + x * sizeT - sizeBrd;
                    bottomT = offset + (63 - y) * sizeT - sizeBrd;
                    rightT = leftT + sizeTotal;
                    topT = bottomT + sizeTotal;

                    std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                    ofs << "gdalwarp -t_srs \"+proj=tmerc +lat_0=47.27 +lon_0=9.07 +k_0=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m\" -te ";
                    ofs << leftT << " " << bottomT << " " << rightT << " " << topT << " ";
                    ofs << "-ts 1024 1024 -r cubicspline -multi -overwrite -ot byte ";
                    ofs << "Images_2m.tif ";
                    ofs << "A1.tif  A2.tif  A3.tif B1.tif  B2.tif  B3.tif ";
                    ofs << "C1.tif  C2.tif  C3.tif D1.tif  D2.tif  D3.tif ";
                    ofs << "../_temp/" << imageName << ".tif \n";
                    ofs << "gdal_translate -ot byte ../_temp/" << imageName << ".tif ../_temp/" << imageName << ".bil \n\n\n";


                }
            }

            // steg 8 y 32-40  x 29-33  // Fre
            lod = 8;
            scale = 1.f / (float)pow(2, lod);
            sizeT = sizeWorld * scale;
            sizeTotal = sizeT * tile_toBorder;
            sizeBrd = (sizeTotal - sizeT) / 2.f;

            for (y = 64; y < 80; y++)
            {
                for (x = 58; x < 66; x++)
                {
                    leftT = offset + x * sizeT - sizeBrd;
                    bottomT = offset + (255 - y) * sizeT - sizeBrd;
                    rightT = leftT + sizeTotal;
                    topT = bottomT + sizeTotal;

                    std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                    ofs << "gdalwarp -t_srs \"+proj=tmerc +lat_0=47.27 +lon_0=9.07 +k_0=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m\" -te ";
                    ofs << leftT << " " << bottomT << " " << rightT << " " << topT << " ";
                    ofs << "-ts 1024 1024 -r cubicspline -multi -overwrite -ot byte ";
                    ofs << "Images_2m.tif ";
                    ofs << "A1.tif  A2.tif  A3.tif B1.tif  B2.tif  B3.tif ";
                    ofs << "C1.tif  C2.tif  C3.tif D1.tif  D2.tif  D3.tif ";
                    ofs << "../_temp/" << imageName << ".tif \n";
                    ofs << "gdal_translate -ot byte ../_temp/" << imageName << ".tif ../_temp/" << imageName << ".bil \n\n\n";


                }
            }
            ofs.close();

        }


        // walensee 6  y 48-64 x 20-64
        // walensee 8  y 112-120  x 120-128
        ofs.open("E:/terrains/switserland_Steg/gis/photos/gdal_photos_lod_walensee.bat");
        if (ofs)
        {
            lod = 6;
            scale = 1.f / (float)pow(2, lod);
            sizeT = sizeWorld * scale;
            sizeTotal = sizeT * tile_toBorder;
            sizeBrd = (sizeTotal - sizeT) / 2.f;

            for (y = 48; y < 64; y++)
            {
                for (x = 20; x < 64; x++)
                {
                    leftT = offset + x * sizeT - sizeBrd;
                    bottomT = offset + (63 - y) * sizeT - sizeBrd;
                    rightT = leftT + sizeTotal;
                    topT = bottomT + sizeTotal;

                    std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                    ofs << "gdalwarp -t_srs \"+proj=tmerc +lat_0=47.27 +lon_0=9.07 +k_0=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m\" -te ";
                    ofs << leftT << " " << bottomT << " " << rightT << " " << topT << " ";
                    ofs << "-ts 1024 1024 -r cubicspline -multi -overwrite -ot byte ";
                    ofs << "Images_2m.tif ";
                    ofs << "A1.tif  A2.tif  A3.tif B1.tif  B2.tif  B3.tif ";
                    ofs << "C1.tif  C2.tif  C3.tif D1.tif  D2.tif  D3.tif ";
                    ofs << "../_temp/" << imageName << ".tif \n";
                    ofs << "gdal_translate -ot byte ../_temp/" << imageName << ".tif ../_temp/" << imageName << ".bil \n\n\n";


                }
            }

            // walensee 8  y 112-120  x 120-128
            lod = 8;
            scale = 1.f / (float)pow(2, lod);
            sizeT = sizeWorld * scale;
            sizeTotal = sizeT * tile_toBorder;
            sizeBrd = (sizeTotal - sizeT) / 2.f;

            for (y = 224; y < 240; y++)
            {
                for (x = 240; x < 256; x++)
                {
                    leftT = offset + x * sizeT - sizeBrd;
                    bottomT = offset + (255 - y) * sizeT - sizeBrd;
                    rightT = leftT + sizeTotal;
                    topT = bottomT + sizeTotal;

                    std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                    ofs << "gdalwarp -t_srs \"+proj=tmerc +lat_0=47.27 +lon_0=9.07 +k_0=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m\" -te ";
                    ofs << leftT << " " << bottomT << " " << rightT << " " << topT << " ";
                    ofs << "-ts 1024 1024 -r cubicspline -multi -overwrite -ot byte ";
                    ofs << "Images_2m.tif ";
                    ofs << "A1.tif  A2.tif  A3.tif B1.tif  B2.tif  B3.tif ";
                    ofs << "C1.tif  C2.tif  C3.tif D1.tif  D2.tif  D3.tif ";
                    ofs << "../_temp/" << imageName << ".tif \n";
                    ofs << "gdal_translate -ot byte ../_temp/" << imageName << ".tif ../_temp/" << imageName << ".bil \n\n\n";


                }
            }
            ofs.close();
        }



        ofSummary.open("E:/terrains/switserland_Steg/gis/photos/gdal_photos_Summary_lod7.txt");
        {
            // steg 6 y 12-20  x 12-24
        // steg 8 y 32-40  x 29-33

        // walensee 6  y 48-64 x 20-64
        // walensee 8  y 112-120  x 120-128

            lod = 6;
            scale = 1.f / (float)pow(2, lod);
            sizeT = sizeWorld * scale;
            sizeTotal = sizeT * tile_toBorder;
            sizeBrd = (sizeTotal - sizeT) / 2.f;


            // Steg
            for (y = 12; y < 20; y++)
            {
                for (x = 12; x < 24; x++)
                {
                    leftT = offset + x * sizeT - sizeBrd;
                    bottomT = offset + (63 - y) * sizeT - sizeBrd;
                    rightT = leftT + sizeTotal;
                    topT = bottomT + sizeTotal;

                    std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                    ofSummary << lod << " " << y << " " << x << " 1024 " << leftT << " " << topT * -1 << " " << sizeTotal;
                    ofSummary << " orthophoto/" << imageName << ".jp2\n";
                }
            }


            // Walensee
            for (y = 48; y < 64; y++)
            {
                for (x = 20; x < 64; x++)
                {
                    leftT = offset + x * sizeT - sizeBrd;
                    bottomT = offset + (63 - y) * sizeT - sizeBrd;
                    rightT = leftT + sizeTotal;
                    topT = bottomT + sizeTotal;

                    std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                    ofSummary << lod << " " << y << " " << x << " 1024 " << leftT << " " << topT * -1 << " " << sizeTotal;
                    ofSummary << " orthophoto/" << imageName << ".jp2\n";
                }
            }

            // steg 6 y 12-20  x 12-24
       // steg 8 y 32-40  x 29-33

       // walensee 6  y 48-64 x 20-64
       // walensee 8  y 112-120  x 120-128
            lod = 8;
            scale = 1.f / (float)pow(2, lod);
            sizeT = sizeWorld * scale;
            sizeTotal = sizeT * tile_toBorder;
            sizeBrd = (sizeTotal - sizeT) / 2.f;


            // Steg
            for (y = 64; y < 80; y++)
            {
                for (x = 58; x < 66; x++)
                {
                    leftT = offset + x * sizeT - sizeBrd;
                    bottomT = offset + (255 - y) * sizeT - sizeBrd;
                    rightT = leftT + sizeTotal;
                    topT = bottomT + sizeTotal;

                    std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                    ofSummary << lod << " " << y << " " << x << " 1024 " << leftT << " " << topT * -1 << " " << sizeTotal;
                    ofSummary << " orthophoto/" << imageName << ".jp2\n";
                }
            }


            // Walensee
            for (y = 224; y < 240; y++)
            {
                for (x = 240; x < 256; x++)
                {
                    leftT = offset + x * sizeT - sizeBrd;
                    bottomT = offset + (255 - y) * sizeT - sizeBrd;
                    rightT = leftT + sizeTotal;
                    topT = bottomT + sizeTotal;

                    std::string imageName = "img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
                    ofSummary << lod << " " << y << " " << x << " 1024 " << leftT << " " << topT * -1 << " " << sizeTotal;
                    ofSummary << " orthophoto/" << imageName << ".jp2\n";
                }
            }

            ofSummary.close();
        }
        */
}
#pragma optimize("", off)
void terrainManager::bil_to_jp2Photos()
{
    float leftT, topT, size;
    int lod, y, x, pixSize;
    //char name[256];
    std::string name;
    std::string filename;
    std::ifstream ifSummary;

    jp2Dir jp2;
    jp2.load("E:/terrains/switserland_Steg/gis/photos/photo_tiles.json");
    for (auto& F : jp2.files)
    {
        std::string binfile = "E:/terrains/switserland_Steg/gis/_temp/" + F.filename;
        std::ofstream of_jp2;
        of_jp2.open(binfile, std::ios::binary);
        if (of_jp2.good())
        {
            F.sizeInBytes = 0;
            for (auto& T : F.tiles)
            {
                T.fileOffset = F.sizeInBytes;
                filename = "E:/terrains/switserland_Steg/gis/_temp/img_" + std::to_string(T.lod) + "_" + std::to_string(T.y) + "_" + std::to_string(T.x);
                T.sizeInBytes = bil_to_jp2PhotosMemory(of_jp2, filename, 1024, T.lod, T.y, T.x);
                F.sizeInBytes += T.sizeInBytes;
            }
            of_jp2.close();
        }
    }
    jp2.save("E:/terrains/switserland_Steg/gis/photos/photo_tiles_JP2.json");
    return;


    ifSummary.open("E:/terrains/switserland_Steg/gis/photos/gdal_photos_Summary_lod7.txt");
    if (ifSummary)
    {
        ifSummary >> lod >> y >> x >> pixSize >> leftT >> topT >> size >> name;
        filename = "E:/terrains/switserland_Steg/gis/_temp/img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
        bil_to_jp2Photos(filename, 1024, lod, y, x);
        while (!ifSummary.eof())
        {
            ifSummary >> lod >> y >> x >> pixSize >> leftT >> topT >> size >> name;
            filename = "E:/terrains/switserland_Steg/gis/_temp/img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
            bil_to_jp2Photos(filename, 1024, lod, y, x);
        }

        ifSummary.close();
    }

    /*
    FILE* file;
    fopen_s(&file, "E:/terrains/switserland_Steg/gis/photos/gdal_photos_Summary.txt", "r");
    int numread;
    do
    {
        char filename[256];
        int lod, y, x, blockSize;
        float xstart, ystart, size;
        memset(filename, 0, 256);
        //numread = fscanf_s(file, "%d %d %d %d %f %f %f %s", &lod, &y, &x, &blockSize, &xstart, &ystart, &size, filename, 256);
        numread = fscanf_s(file, "%d %d %d %d %f %f %f %s", &lod, &y, &x, &blockSize, &xstart, &ystart, &size, filename, 256);
        filename = "E:/terrains/switserland_Steg/gis/_temp/img_" + std::to_string(lod) + "_" + std::to_string(y) + "_" + std::to_string(x);
        if (numread > 0) {
            bil_to_jp2Photos(pathOnly + filename, blockSize, summary, lod, y, x, xstart, ystart, size);
        }
    } while (numread > 0);

    fclose(file);
    */

}


//#pragma optimize("", off)
uint terrainManager::bil_to_jp2PhotosMemory(std::ofstream& _file, std::string filename, const uint size, uint _lod, uint _y, uint _x)
{
    unsigned char data[1024][3][1024];
    unsigned char data1[1024][1024];


    ojph::codestream codestream;
    ojph::mem_outfile j2c_file;
    j2c_file.open();
    {
        // set up
        ojph::param_siz siz = codestream.access_siz();
        siz.set_image_extent(ojph::point(size, size));
        siz.set_num_components(3);
        siz.set_component(0, ojph::point(1, 1), 8, false);
        siz.set_component(1, ojph::point(1, 1), 8, false);
        siz.set_component(2, ojph::point(1, 1), 8, false);
        siz.set_image_offset(ojph::point(0, 0));
        siz.set_tile_size(ojph::size(size, size));
        siz.set_tile_offset(ojph::point(0, 0));

        ojph::param_cod cod = codestream.access_cod();
        cod.set_num_decomposition(5);
        cod.set_block_dims(64, 64);
        cod.set_progression_order("CPRL");      // ??? "RPCL"
        cod.set_color_transform(true);
        cod.set_reversible(false);

        codestream.access_qcd().set_irrev_quant(0.05f);
        codestream.set_planar(false);

        FILE* bilData = fopen((filename + ".bil").c_str(), "rb");
        if (bilData)
        {
            fread(data, sizeof(char), size * size * 3, bilData);
            codestream.write_headers(&j2c_file);

            int next_comp;
            ojph::line_buf* cur_line = codestream.exchange(NULL, next_comp);

            //for (uint cmp = 0; cmp < 3; cmp++)
            {
                for (uint i = 0; i < size; i++)
                {
                    //base->read(cur_line, next_comp);
                    //fread(data, sizeof(float), size, bilData);

                    for (uint cmp = 0; cmp < 3; cmp++)
                    {

                        if (cmp != next_comp)
                        {
                            bool cm = true;
                        }
                        int32_t* dp = cur_line->i32;
                        for (uint j = 0; j < size; j++) {
                            *dp++ = (int32_t)data[i][cmp][j];
                        }
                        cur_line = codestream.exchange(cur_line, next_comp);
                    }
                }
            }
            fclose(bilData);
        }
    }
    codestream.flush();


    _file.write((char*)j2c_file.get_data(), j2c_file.tell());
    uint numBytes = j2c_file.tell();

    codestream.close();
    return numBytes;
}



void terrainManager::bil_to_jp2Photos(std::string file, const uint size, uint _lod, uint _y, uint _x)
{
    unsigned char data[1024][3][1024];
    unsigned char data1[1024][1024];


    ojph::codestream codestream;
    ojph::j2c_outfile j2c_file;
    j2c_file.open((file + ".jp2").c_str());
    {
        // set up
        ojph::param_siz siz = codestream.access_siz();
        siz.set_image_extent(ojph::point(size, size));
        siz.set_num_components(3);
        siz.set_component(0, ojph::point(1, 1), 8, false);
        siz.set_component(1, ojph::point(1, 1), 8, false);
        siz.set_component(2, ojph::point(1, 1), 8, false);
        siz.set_image_offset(ojph::point(0, 0));
        siz.set_tile_size(ojph::size(size, size));
        siz.set_tile_offset(ojph::point(0, 0));

        ojph::param_cod cod = codestream.access_cod();
        cod.set_num_decomposition(5);
        cod.set_block_dims(64, 64);
        //if (num_precints != -1)
        //	cod.set_precinct_size(num_precints, precinct_size);
        //cod.set_progression_order("RPCL");
        cod.set_progression_order("CPRL");
        cod.set_color_transform(true);
        cod.set_reversible(false);

        codestream.access_qcd().set_irrev_quant(0.05f);
        codestream.set_planar(false);





        FILE* bilData = fopen((file + ".bil").c_str(), "rb");
        if (bilData)
        {
            fread(data, sizeof(char), size * size * 3, bilData);
            codestream.write_headers(&j2c_file);

            int next_comp;
            ojph::line_buf* cur_line = codestream.exchange(NULL, next_comp);

            //for (uint cmp = 0; cmp < 3; cmp++)
            {
                for (uint i = 0; i < size; i++)
                {
                    //base->read(cur_line, next_comp);
                    //fread(data, sizeof(float), size, bilData);

                    for (uint cmp = 0; cmp < 3; cmp++)
                    {

                        if (cmp != next_comp)
                        {
                            bool cm = true;
                        }
                        int32_t* dp = cur_line->i32;
                        for (uint j = 0; j < size; j++) {
                            *dp++ = (int32_t)data[i][cmp][j];
                        }
                        cur_line = codestream.exchange(cur_line, next_comp);
                    }
                }
            }
            fclose(bilData);

            if (_lod == 4 && _y == 4 && _x == 13)
            {
                std::ofstream ofs;
                ofs.open("E:/terrains/switserland_Steg/gis/temp_rgb.raw", std::ios::binary);
                if (ofs)
                {
                    for (uint i = 0; i < size; i++) {
                        for (uint j = 0; j < size; j++) {
                            ofs.write((char*)&data[i][0][j], 1);
                            ofs.write((char*)&data[i][1][j], 1);
                            ofs.write((char*)&data[i][2][j], 1);
                        }
                    }
                    ofs.close();
                }


                ofs.open("E:/terrains/switserland_Steg/gis/temp_red.raw", std::ios::binary);
                if (ofs)
                {
                    for (uint i = 0; i < size; i++) {
                        for (uint j = 0; j < size; j++) {
                            data1[i][j] = data[i][0][j];
                        }
                    }

                    ofs.write((char*)data1, 1024 * 1024);

                    ofs.close();
                }

                ofs.open("E:/terrains/switserland_Steg/gis/temp_green.raw", std::ios::binary);
                if (ofs)
                {
                    for (uint i = 0; i < size; i++) {
                        for (uint j = 0; j < size; j++) {
                            ofs.write((char*)&data[i][1][j], 1);
                        }
                    }

                    ofs.close();
                }

                ofs.open("E:/terrains/switserland_Steg/gis/temp_blue.raw", std::ios::binary);
                if (ofs)
                {
                    for (uint i = 0; i < size; i++) {
                        for (uint j = 0; j < size; j++) {
                            ofs.write((char*)&data[i][2][j], 1);
                        }
                    }

                    ofs.close();
                }
            }
        }


    }
    codestream.flush();
    codestream.close();


    //fprintf(summary, "%d %d %d %d %f %f %f %f %f elevation/hgt_%d_%d_%d.jp2\n", _lod, _y, _x, size, _xstart, _ystart, _size, data_min, (data_max - data_min), _lod, _y, _x);
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
        if (ImGui::MenuItem("generatePhotoGdal"))
        {
            generateGdalPhotos();
        }
        if (ImGui::MenuItem("process bil -> jp2 - Photos"))
        {
            bil_to_jp2Photos();
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
            float m_halfAngle_to_Pixels = cameraViews[i].resolution * fovscale / 4.0f;
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



void replaceAll_TM(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

void terrainManager::bakeVegetation(int baseSize, float clipCenter, std::string _ext)
{
    int superSample = 8;

    float W = groveTree.extents.x * clipCenter;      // this is half width
    float H = groveTree.extents.y;
    int iW = 4 * (int)(baseSize * W * 2.f / H / 4) * superSample;
    int iH = baseSize * superSample;

    Fbo::Desc desc;
    desc.setDepthStencilTarget(ResourceFormat::D24UnormS8);			// keep for now, not sure why, but maybe usefult for cuts
    desc.setColorTarget(0u, ResourceFormat::RGBA8Unorm, true);		// albedo
    desc.setColorTarget(1u, ResourceFormat::RGBA16Float, true);	    // normal_16
    desc.setColorTarget(2u, ResourceFormat::RGBA8Unorm, true);		// normal_8
    desc.setColorTarget(3u, ResourceFormat::RGBA8Unorm, true);	    // pbr
    desc.setColorTarget(4u, ResourceFormat::RGBA8Unorm, true);	    // extra
    Fbo::SharedPtr fbo = Fbo::create2D(iW, iH, desc, 1, 4);
    const glm::vec4 clearColor(0.5, 0.5f, 1.0f, 0.0f);
    bake.renderContext->clearFbo(fbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    bake.renderContext->clearRtv(fbo->getRenderTargetView(0).get(), glm::vec4(0.0, 0.0f, 0.0f, 0.0f));
    bake.renderContext->clearRtv(fbo->getRenderTargetView(1).get(), glm::vec4(0.5, 0.5f, 1.0f, 0.0f));
    bake.renderContext->clearRtv(fbo->getRenderTargetView(2).get(), glm::vec4(0.5, 0.5f, 1.0f, 0.0f));
    bake.renderContext->clearRtv(fbo->getRenderTargetView(3).get(), glm::vec4(0.0, 0.0f, 0.0f, 0.0f));
    bake.renderContext->clearRtv(fbo->getRenderTargetView(4).get(), glm::vec4(1.0, 1.0f, 1.0f, 1.0f));

    vegetationShader_Bake.State()->setFbo(fbo);
    vegetationShader_Bake.State()->setViewport(0, GraphicsState::Viewport(0, 0, (float)iW, (float)iH, 0, 1), true);
    vegetationShader_Bake.State()->setRasterizerState(split.rasterstateSplines);
    vegetationShader_Bake.State()->setBlendState(blendstateVegBake);//blendstateSplines

    glm::mat4 V, VP;
    V = { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, W, 1} };
    VP = glm::orthoLH(-W, W, 0.f, H, -1000.0f, 1000.0f) * V;
    rmcv::mat4 viewproj;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            viewproj[j][i] = VP[j][i];
        }
    }


    

    vegetationShader_Bake.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
    vegetationShader_Bake.Vars()["gConstantBuffer"]["eyePos"] = float3(0, 0, -100000);  // just very far sort of parallel
    vegetationShader_Bake.Vars()["gConstantBuffer"]["offset"] = float3(0, 0, 0);
    vegetationShader_Bake.Vars()["gConstantBuffer"]["repeatScale"] = 1.0f;
    vegetationShader_Bake.Vars()["gConstantBuffer"]["numSide"] = 1;


    auto& block = vegetationShader_Bake.Vars()->getParameterBlock("textures");
    ShaderVar& var = block->findMember("T");        // FIXME pre get
    _plantMaterial::static_materials_veg.setTextures(var);

    _plantMaterial::static_materials_veg.rebuildStructuredBuffer();

    ribbonDataVegBuilder->setBlob(groveTree.packedRibbons, 0, groveTree.numBranchRibbons * sizeof(unsigned int) * 6);
    groveTree.bChanged = false;



    


    vegetationShader_Bake.drawInstanced(bake.renderContext, groveTree.numBranchRibbons, 1);

    bake.renderContext->flush(true);


    compute_bakeFloodfill.Vars()->setTexture("gAlbedo", fbo->getColorTexture(0));
    compute_bakeFloodfill.Vars()->setTexture("gNormal", fbo->getColorTexture(2));
    compute_bakeFloodfill.Vars()->setTexture("gTranslucency", fbo->getColorTexture(4));
    compute_bakeFloodfill.Vars()->setTexture("gpbr", fbo->getColorTexture(3));

    int numBuffer = 32;
    for (int i = 0; i < numBuffer; i++)
    {
        compute_bakeFloodfill.dispatch(bake.renderContext, iW / 4, iH / 4);
    }



    // FIXME remove weedbuolde dependency
    std::filesystem::path full_path = groveTree.weedBuilder.filepath;// +".billboard.vegMaterial";
    std::string name = full_path.stem().string() + _ext;
    std::string newPath = full_path.parent_path().string() + "\\" + name;
    std::string resource = terrafectorEditorMaterial::rootFolder;
    replaceAll_TM(resource, "/", "\\");
    {
        _plantMaterial Mat;
        Mat._constData.translucency = 1;
        Mat.albedoPath = newPath + "_albedo.dds";
        Mat.albedoName = name + "_albedo.dds";
        Mat.normalPath = newPath + "_normal.dds";
        Mat.normalName = name + "_normal.dds";
        Mat.translucencyPath = newPath + "_translucency.dds";
        Mat.translucencyName = name + "_translucency.dds";

        fbo->getColorTexture(0).get()->generateMips(bake.renderContext);
        fbo->getColorTexture(1).get()->generateMips(bake.renderContext);
        fbo->getColorTexture(2).get()->generateMips(bake.renderContext);
        fbo->getColorTexture(3).get()->generateMips(bake.renderContext);
        fbo->getColorTexture(4).get()->generateMips(bake.renderContext);

        fbo->getColorTexture(0).get()->captureToFile(3, 0, newPath + "_albedo.png", Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::ExportAlpha);
        fbo->getColorTexture(2).get()->captureToFile(3, 0, newPath + "_normal.png", Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);
        fbo->getColorTexture(4).get()->captureToFile(3, 0, newPath + "_translucency.png", Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);

        {
            std::string png = newPath + "_albedo.png";
            std::string cmdExp = resource + "Compressonator\\CompressonatorCLI -miplevels 6 \"" + png + "\" " + resource + "Compressonator\\temp_mip.dds";
            system(cmdExp.c_str());
            std::string cmdExp2 = resource + "Compressonator\\CompressonatorCLI -fd BC7 " + resource + "Compressonator\\temp_mip.dds \"" + Mat.albedoPath + "\"";
            system(cmdExp2.c_str());
            //??? Shall I not rather use my compute compressor here?
        }

        {
            std::string png = newPath + "_normal.png";
            std::string cmdExp = resource + "Compressonator\\CompressonatorCLI -miplevels 6 \"" + png + "\" " + resource + "Compressonator\\temp_mip.dds";
            system(cmdExp.c_str());
            std::string cmdExp2 = resource + "Compressonator\\CompressonatorCLI -fd BC6H " + resource+ "Compressonator\\temp_mip.dds \"" + Mat.normalPath + "\"";
            system(cmdExp2.c_str());
            //??? Shall I not rather use my compute compressor here?
        }

        {
            std::string png = newPath + "_translucency.png";
            std::string cmdExp = resource + "Compressonator\\CompressonatorCLI -miplevels 6 \"" + png + "\" " + resource + "Compressonator\\temp_mip.dds";
            system(cmdExp.c_str());
            std::string cmdExp2 = resource + "Compressonator\\CompressonatorCLI -fd BC7 " + resource + "Compressonator\\temp_mip.dds \"" + Mat.translucencyPath + "\"";
            system(cmdExp2.c_str());
            //??? Shall I not rather use my compute compressor here?
        }


        std::ofstream os(name + "_billboard.vegMaterial");
        cereal::JSONOutputArchive archive(os);
        archive(Mat);
        //Mat.serialize(archive, _PLANTMATERIALVERSION);
    }
}



bool terrainManager::update(RenderContext* _renderContext)
{
    bake.renderContext = _renderContext;

    if (terrainMode == _terrainMode::vegetation)
    {
        fullResetDoNotRender = false;
        return false;
    }


    if (terrainMode == _terrainMode::glider)
    {
        //fprintf(terrafectorSystem::_logfile, "cfd\n");
        //fflush(terrafectorSystem::_logfile);
        {
            static std::chrono::steady_clock::time_point prev;
            std::chrono::steady_clock::time_point current = high_resolution_clock::now();
            float dT = (float)duration_cast<microseconds>(current - prev).count() / 1000000.;
            prev = current;

            dT *= 1.2f; // not quite hitting 1 second 
            /*/cfd.sliceTime[0] = __min(cfd.sliceTime[0] + dT / 32, 1.f);
            cfd.sliceTime[1] = __min(cfd.sliceTime[1] + dT / 16, 1.f);
            cfd.sliceTime[2] = __min(cfd.sliceTime[2] + dT / 8, 1.f);
            cfd.sliceTime[3] = __min(cfd.sliceTime[3] + dT / 4, 1.f);
            cfd.sliceTime[4] = __min(cfd.sliceTime[4] + dT / 2, 1.f);
            cfd.sliceTime[5] = __min(cfd.sliceTime[5] + dT, 1.f);
            */

            if (cfd.clipmap.sliceOrder[0] == 0)      cfd.sliceTime[0] = __max(cfd.sliceTime[0] - dT / 32, 0.f);
            else                                     cfd.sliceTime[0] = __min(cfd.sliceTime[0] + dT / 32, 1.f);

            if (cfd.clipmap.sliceOrder[1] == 0)      cfd.sliceTime[1] = __max(cfd.sliceTime[1] - dT / 16, 0.f);
            else                                     cfd.sliceTime[1] = __min(cfd.sliceTime[1] + dT / 16, 1.f);

            if (cfd.clipmap.sliceOrder[2] == 0)      cfd.sliceTime[2] = __max(cfd.sliceTime[2] - dT / 8, 0.f);
            else                                     cfd.sliceTime[2] = __min(cfd.sliceTime[2] + dT / 8, 1.f);

            if (cfd.clipmap.sliceOrder[3] == 0)      cfd.sliceTime[3] = __max(cfd.sliceTime[3] - dT / 4, 0.f);
            else                                     cfd.sliceTime[3] = __min(cfd.sliceTime[3] + dT / 4, 1.f);

            if (cfd.clipmap.sliceOrder[4] == 0)      cfd.sliceTime[4] = __max(cfd.sliceTime[4] - dT / 2, 0.f);
            else                                     cfd.sliceTime[4] = __min(cfd.sliceTime[4] + dT / 2, 1.f);

            if (cfd.clipmap.sliceOrder[5] == 0)      cfd.sliceTime[5] = __max(cfd.sliceTime[5] - dT / 1, 0.f);
            else                                     cfd.sliceTime[5] = __min(cfd.sliceTime[5] + dT / 1, 1.f);


            static bool NowSwap = false;
            static int swapLOD = 0;
            static int swapOrder = 0;
            if (cfd.clipmap.sliceNew)
            {
                //FALCOR_PROFILE("cfd slice update");

                uint LOD = cfd.clipmap.slicelod;
                uint toChange = cfd.clipmap.sliceOrder[LOD];
                //uint oldSlice = cfd.sliceOrder[cfd.clipmap.slicelod];
                //cfd.clipmap.sliceOrder[cfd.clipmap.slicelod] = (cfd.clipmap.sliceOrder[cfd.clipmap.slicelod] + 1) % 2;


                // always load to 3, then swap
                _renderContext->updateTextureData(cfd.sliceVolumeTexture[LOD][2].get(), cfd.clipmap.volumeData);
                //Texture::SharedPtr SWP = cfd.sliceVolumeTexture[LOD][3];
                //cfd.sliceVolumeTexture[LOD][3] = cfd.sliceVolumeTexture[LOD][toChange];
                //cfd.sliceVolumeTexture[LOD][toChange] = SWP;
                std::swap(cfd.sliceVolumeTexture[LOD][2], cfd.sliceVolumeTexture[LOD][toChange]);
                //cfd.sliceTime[LOD] = 0;
                NowSwap = true;
                swapLOD = cfd.clipmap.slicelod;
                swapOrder = cfd.clipmap.sliceOrder[LOD];

                //_renderContext->updateTextureData(cfd.sliceVolumeTexture[cfd.clipmap.slicelod][cfd.clipmap.sliceOrder[cfd.clipmap.slicelod]].get(), cfd.clipmap.volumeData);
                //updateSubresourceData
                cfd.clipmap.sliceNew = false;
            }

            if (NowSwap)
            {
                //std::swap(cfd.sliceVolumeTexture[swapLOD][2], cfd.sliceVolumeTexture[swapLOD][swapOrder]);
                //cfd.sliceTime[swapLOD] = 0;
                NowSwap = false;
            }

            cfd.lodLerp[0] = cfd.sliceTime[0];
            cfd.lodLerp[1] = cfd.sliceTime[1];
            cfd.lodLerp[2] = cfd.sliceTime[2];
            cfd.lodLerp[3] = cfd.sliceTime[3];
            cfd.lodLerp[4] = cfd.sliceTime[4];
            cfd.lodLerp[5] = cfd.sliceTime[5];

            /*
            if (cfd.clipmap.sliceOrder[0] == 0) cfd.lodLerp[0] = 1.f - cfd.sliceTime[0];
            if (cfd.clipmap.sliceOrder[1] == 0) cfd.lodLerp[1] = 1.f - cfd.sliceTime[1];
            if (cfd.clipmap.sliceOrder[2] == 0) cfd.lodLerp[2] = 1.f - cfd.sliceTime[2];
            if (cfd.clipmap.sliceOrder[3] == 0) cfd.lodLerp[3] = 1.f - cfd.sliceTime[3];
            if (cfd.clipmap.sliceOrder[4] == 0) cfd.lodLerp[4] = 1.f - cfd.sliceTime[4];
            if (cfd.clipmap.sliceOrder[5] == 0) cfd.lodLerp[5] = 1.f - cfd.sliceTime[5];
            */

            cfd.clipmap.lodOffsets[2][0].w = cfd.lodLerp[2];
            cfd.clipmap.lodOffsets[3][0].w = cfd.lodLerp[3];
            cfd.clipmap.lodOffsets[4][0].w = cfd.lodLerp[4];
            cfd.clipmap.lodOffsets[5][0].w = cfd.lodLerp[5];

            cfd.clipmap.lodOffsets[2][1].w = cfd.lodLerp[2];
            cfd.clipmap.lodOffsets[3][1].w = cfd.lodLerp[3];
            cfd.clipmap.lodOffsets[4][1].w = cfd.lodLerp[4];
            cfd.clipmap.lodOffsets[5][1].w = cfd.lodLerp[5];
        }
    }



    if (glider.newGliderLoaded)
    {
        //newGliderRuntime.solve(0.001f, 1);
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
            //fprintf(terrafectorSystem::_logfile, "update_roads\n");
            //fflush(terrafectorSystem::_logfile);
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

            if (!dirty) fullResetDoNotRender = false;

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


/*  The weird carsh comes from my cache being too small, so tiles gets deleted tehn needed at another resolution
*/
void terrainManager::hashAndCacheImages(quadtree_tile* pTile)
{


    uint32_t hash = getHashFromTileCoords(pTile->lod, pTile->y, pTile->x);

    // First load the JP2 Data, BUT 0,0,0 IS  PRELOADED, thisis n own thread so should look like no time
    imageDirectory.cacheHash(hash);
    //std::thread T0(&jp2Dir::cacheHash, &imageDirectory, hash);


    std::map<uint32_t, uint2>::iterator it = imageDirectory.tileHash.find(hash);
    if (it != imageDirectory.tileHash.end()) {
        pTile->imageHash = hash;
    }
    else
    {
        //fprintf(terrafectorSystem::_logfile, "FAILED %d %d %d   imageDirectory.tileHash.find(hash);\n", pTile->lod, pTile->y, pTile->x);
        //fflush(terrafectorSystem::_logfile);
    }

    //if (pTile->imageHash > 0)
    {
        textureCacheElement map;

        if (!imageCache.get(pTile->imageHash, map))     // so we dont have teh image on GPU
        {
            if (pTile->imageHash == 0)
            {
                bool CM = true;
            }

            std::array<unsigned char, 1024 * 1024 * 4> data;
            std::shared_ptr<std::vector<unsigned char>> dataCache;

            ojph::codestream codestream;
            ojph::mem_infile j2c_file;
            //j2c_file.open(imageTileHashmap[pTile->imageHash].filename.c_str());
            std::map<uint32_t, uint2>::iterator itH = imageDirectory.tileHash.find(pTile->imageHash);
            if (itH == imageDirectory.tileHash.end()) {
                fprintf(terrafectorSystem::_logfile, "FAILED tileHash.find\n");
                fflush(terrafectorSystem::_logfile);
            }
            jp2File& file = imageDirectory.files[itH->second.x];
            jp2Map& mapTile = imageDirectory.files[itH->second.x].tiles[itH->second.y];

            if (itH->second.x == 0)
            {
                j2c_file.open(&imageDirectory.dataRoot[mapTile.fileOffset], mapTile.sizeInBytes);
            }
            else
            {
                if (imageDirectory.cache.get(file.hash, dataCache))
                {
                    j2c_file.open(dataCache->data() + mapTile.fileOffset, mapTile.sizeInBytes);
                }
                else
                {

                    bool bCM = true;
                    fprintf(terrafectorSystem::_logfile, "FIX imageCache.resize(55);  its still too small\n");
                    fprintf(terrafectorSystem::_logfile, "offset %d, lod %d, %s\n", mapTile.fileOffset, mapTile.lod, file.filename.c_str());
                    fprintf(terrafectorSystem::_logfile, "itH file %d, tile %d\n", itH->second.x, itH->second.y);
                    fprintf(terrafectorSystem::_logfile, "Bug is liekly here at teh ned void jp2Dir::load(std::string _name). The thing that changed is that I changed FOV, so diffirent amount fo tiles vissible\n");


                    fflush(terrafectorSystem::_logfile);
                    // should never get here but handle it somehow
                    // split.compute_tileBicubic.Vars()->setTexture("gInputAlbedo", map.texture);
                    // I think it was with very fast movement, not sure how that was possible
                }
            }
            codestream.enable_resilience();
            codestream.set_planar(false);
            codestream.read_headers(&j2c_file);
            codestream.create();
            int next_comp;

            for (int i = 0; i < 1024; ++i)
            {
                for (int j = 0; j < 3; j++)
                {
                    ojph::line_buf* line = codestream.pull(next_comp);
                    int32_t* dp = line->i32;
                    for (int j = 0; j < 1024; j++) {
                        data[(i * 1024 * 4) + (j * 4) + next_comp] = (unsigned char)*dp++;
                    }
                }
            }
            codestream.close();

            map.texture = Texture::create2D(1024, 1024, Falcor::ResourceFormat::RGBA8UnormSrgb, 1, 1, data.data(), Resource::BindFlags::ShaderResource);
            imageCache.set(pTile->imageHash, map);
        }

        split.compute_tileBicubic.Vars()->setTexture("gInputAlbedo", map.texture);
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
    pTile->child[childIdx]->imageHash = pTile->imageHash;
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

            hashAndCacheImages(tile);

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
        split.compute_tileBicubic.Vars()["gConstants"]["isHeight"] = 1;
        split.compute_tileBicubic.dispatch(_renderContext, cs_w, cs_w);
    }

    {
        // copy the image tiles to diffuse
        //heightMap& imageMap = imageTileHashmap[_tile->imageHash];
        //heightMap& imageMap = imageDirectory.tileHash

        std::map<uint32_t, uint2>::iterator itH = imageDirectory.tileHash.find(_tile->imageHash);
        //jp2File& file = imageDirectory.files[itH->second.x];
        jp2Map& mapTile = imageDirectory.files[itH->second.x].tiles[itH->second.y];

        float S = pixelSize / mapTile.size;
        split.compute_tileBicubic.Vars()["gConstants"]["offset"] = (origin - mapTile.origin) / mapTile.size;
        split.compute_tileBicubic.Vars()["gConstants"]["size"] = float2(S, S);
        split.compute_tileBicubic.Vars()["gConstants"]["isHeight"] = 0;
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


    // STAMPS #################################################################################################################
    if (_pTile->lod >= 7)
    {
        gpuTileTerrafector* tile = terrafectorSystem::loadCombine_LOD7_stamps.getTile((_pTile->y >> (_pTile->lod - 7)) * 128 + (_pTile->x >> (_pTile->lod - 7)));
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

    rmcv::mat4 view_ribbon, proj_ribbon, viewproj_ribbon;
    rmcv::mat4 view, proj, viewproj;
    {
        if (terrainMode == _terrainMode::glider)
        {
            {
                //FALCOR_PROFILE("SOLVE_glider");
                //paraRuntime.solve(0.0005f);
            }

            if (!useFreeCamWhileGliding)
            {

                cfd.originRequest = paraRuntime.pilotPos();
                cfd.velocityRequets[0] = paraRuntime.pilotPos();
                cfd.velocityRequets[1] = paraRuntime.wingPos(0);
                cfd.velocityRequets[2] = paraRuntime.wingPos(25);
                cfd.velocityRequets[3] = paraRuntime.wingPos(49);

                paraRuntime.setPilotWind(cfd.velocityAnswers[0]);
                paraRuntime.setWingWind(cfd.velocityAnswers[1], cfd.velocityAnswers[2], cfd.velocityAnswers[3]);

                //paraRuntime.setPilotWind(float3(0, 0, 0));
                //paraRuntime.setWingWind(float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0));
            }




            /*
            _camera->setFarPlane(40000);
            _camera->setUpVector(paraRuntime.camUp);
            _camera->setTarget(paraEyeLocal + paraRuntime.camDir * 100.f);
            _camera->setPosition(paraEyeLocal);

            glm::mat4 V = toGLM(_camera->getViewMatrix());
            glm::mat4 P = toGLM(_camera->getProjMatrix());
            glm::mat4 VP = toGLM(_camera->getViewProjMatrix());
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    view_ribbon[j][i] = V[j][i];
                    proj_ribbon[j][i] = P[j][i];
                    viewproj_ribbon[j][i] = VP[j][i];
                }
            }

            _camera->setTarget(paraCamPos + paraRuntime.camDir * 100.f);
            _camera->setPosition(paraCamPos);
            */
        }
        else
        {
            _camera->setUpVector(float3(0, 1, 0));
        }




        if ((terrainMode == _terrainMode::glider) && requestParaPack == false)
            //if ((terrainMode == 4) && AirSim.changed)
        {
            //FALCOR_PROFILE("setBlob");


            //cfd.originRequest = float3(3185, 900, -14377);  //A
            //cfd.originRequest = float3(3190, 800, -15900);  //A_A  test for air video in the north
            //cfd.originRequest = float3(-5000, 900, -14302);  //B

            /*
            //cfd.originRequest = paraRuntime.pilotPos();
            cfd.originRequest = float3(-1425 + 2800, 425 + 140 + 800, 12533);
            //cfd.originRequest = float3(1184, 422 + 20, 14829);
            cfd.originRequest = float3(9224 + 580, 1458, 4291);     // my smoke in teh mifddle
            //cfd.originRequest = float3(11500, 1818, 11800);     // south better mounrains


            cfd.originRequest = float3(12500, 1362 + 300, 4701);
            cfd.originRequest = float3(8753, 1257 + 300, 6619);
            //cfd.originRequest = float3(15033, 1498 + 300, 7446);



            //cfd.originRequest = float3(1852, 1500, 10910);
            */

            cfd.velocityRequets[0] = paraRuntime.pilotPos();
            cfd.velocityRequets[1] = paraRuntime.wingPos(0);
            cfd.velocityRequets[2] = paraRuntime.wingPos(25);
            cfd.velocityRequets[3] = paraRuntime.wingPos(49);

            paraRuntime.setPilotWind(cfd.velocityAnswers[0]);
            paraRuntime.setWingWind(cfd.velocityAnswers[1], cfd.velocityAnswers[2], cfd.velocityAnswers[3]);

            if (!useFreeCamWhileGliding)
            {
                _camera->setFarPlane(40000);
                _camera->setUpVector(float3(0, 1, 0));
                //_camera->setUpVector(paraRuntime.camUp);
                _camera->setTarget(paraEyeLocal + paraRuntime.camDir * 100.f);
                _camera->setPosition(paraEyeLocal);

                glm::mat4 V = toGLM(_camera->getViewMatrix());
                glm::mat4 P = toGLM(_camera->getProjMatrix());
                glm::mat4 VP = toGLM(_camera->getViewProjMatrix());
                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 4; j++) {
                        view_ribbon[j][i] = V[j][i];
                        proj_ribbon[j][i] = P[j][i];
                        viewproj_ribbon[j][i] = VP[j][i];
                    }
                }


                _camera->setTarget(paraCamPos + paraRuntime.camDir * 100.f);
                _camera->setPosition(paraCamPos);
            }
            /*
            V = toGLM(_camera->getViewMatrix());
            P = toGLM(_camera->getProjMatrix());
            VP = toGLM(_camera->getViewProjMatrix());

            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    view[j][i] = V[j][i];
                    proj[j][i] = P[j][i];
                    viewproj[j][i] = VP[j][i];
                }
            }
            */


            //paraRuntime.pack_canopy();
            //paraRuntime.pack_lines();
            //paraRuntime.pack_feedback();
            //paraRuntime.pack();

            paraRuntime.changed = false;
            ribbonInstanceNumber = 1;
            bufferidx = (bufferidx + 1) % 2;
            ribbonData[bufferidx]->setBlob(paraRuntime.packedRibbons, 0, paraRuntime.ribbonCount * sizeof(unsigned int) * 6);
            numLoadedRibbons = paraRuntime.ribbonCount;

            //ribbonData->setBlob(AirSim.packedRibbons, 0, AirSim.ribbonCount * sizeof(unsigned int) * 6);

            ribbonShader.Vars()->setBuffer("instanceBuffer", ribbonData[bufferidx]);

            ribbonShader.State()->setFbo(_fbo);
            ribbonShader.State()->setViewport(0, _viewport, true);

            ribbonShader.Vars()["gConstantBuffer"]["fakeShadow"] = 4;
            ribbonShader.Vars()["gConstantBuffer"]["objectScale"] = float3(ribbonVertex::objectScale, ribbonVertex::objectScale, ribbonVertex::objectScale);
            ribbonShader.Vars()["gConstantBuffer"]["objectOffset"] = ribbonVertex::objectOffset;
            ribbonShader.Vars()["gConstantBuffer"]["radiusScale"] = ribbonVertex::radiusScale;

            ribbonShader.State()->setRasterizerState(split.rasterstateSplines);
            ribbonShader.State()->setBlendState(split.blendstateSplines);


            gliderwingData[bufferidx]->setBlob(paraRuntime.packedWing, 0, paraRuntime.wingVertexCount * sizeof(_gliderwingVertex));
            wingloadedCnt = paraRuntime.wingVertexCount;
            gliderwingShader.Vars()->setBuffer("vertexBuffer", gliderwingData[bufferidx]);

            requestParaPack = true;
        }


        {
            glm::mat4 V = toGLM(_camera->getViewMatrix());
            glm::mat4 P = toGLM(_camera->getProjMatrix());
            glm::mat4 VP = toGLM(_camera->getViewProjMatrix());

            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    view[j][i] = V[j][i];
                    proj[j][i] = P[j][i];
                    viewproj[j][i] = VP[j][i];
                }
            }
        }
    }

    if (terrainMode == _terrainMode::vegetation)
    {
        {
            FALCOR_PROFILE("skydome");

            triangleShader.State()->setFbo(_fbo);
            triangleShader.State()->setViewport(0, _viewport, true);
            triangleShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
            triangleShader.Vars()["gConstantBuffer"]["eye"] = _camera->getPosition();
            triangleShader.Vars()["gConstantBuffer"]["useSkyDome"] = 0;
            triangleShader.State()->setRasterizerState(split.rasterstateSplines);
            triangleShader.State()->setBlendState(split.blendstateSplines);
            triangleShader.drawInstanced(_renderContext, 36, 1);
        }

        rmcv::mat4 clip;
        //rmcv::mat4 view, clip;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                clip[j][i] = cameraViews[CameraType_Main_Center].frustumMatrix[j][i];
            }
        }
        plants_Root.render(_renderContext, _fbo, _viewport, _hdrHalfCopy, viewproj, _camera->getPosition(), view, clip);


        /*
        if (_plantMaterial::static_materials_veg.modified)
        {
            auto& block = vegetationShader.Vars()->getParameterBlock("textures");
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
            auto& block = vegetationShader.Vars()->getParameterBlock("textures");
            ShaderVar& var = block->findMember("T");        // FIXME pre get
            _plantMaterial::static_materials_veg.setTextures(var);

            _plantMaterial::static_materials_veg.rebuildStructuredBuffer();

            vegetation.plantBuf[0] = groveTree.gpuPlant;
            vegetation.plantData->setBlob(vegetation.plantBuf.data(), 0, 256 * sizeof(plant));
            vegetation.instanceData->setBlob(vegetation.instanceBuf.data(), 0, 16384 * sizeof(plant_instance));
            vegetation.blockData->setBlob(vegetation.blockBuf.data(), 0, 16384 * sizeof(block_data));
            vegetation.vertexData->setBlob(vegetation.vertexBuf.data(), 0, 128 * 256 * sizeof(ribbonVertex8));                // FIXME uploads should be smaller

            ribbonDataVegBuilder->setBlob(groveTree.packedRibbons, 0, groveTree.numBranchRibbons * sizeof(unsigned int) * 6);
            groveTree.bChanged = false;

            vegetationShader.State()->setFbo(_fbo);
            vegetationShader.State()->setViewport(0, _viewport, true);


            vegetationShader.Vars()->setBuffer("instanceBuffer", ribbonDataVegBuilder);



            vegetationShader.State()->setRasterizerState(split.rasterstateSplines);
            vegetationShader.State()->setBlendState(split.blendstateSplines);
        }





        if (groveTree.numBranchRibbons > 1)
        {
            FALCOR_PROFILE("ribbonShader");
            vegetationShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
            vegetationShader.Vars()["gConstantBuffer"]["eyePos"] = _camera->getPosition();

            vegetationShader.State()->setFbo(_fbo);
            //vegetationShader.State()->setViewport(0, _viewport, true);

            static float spacing = 1.0f;
            if (spacingFromExtents) {
                spacing = ribbonSpacing * groveTree.extents.x;
            }
            vegetationShader.Vars()["gConstantBuffer"]["offset"] = float3(-ribbonInstanceNumber * spacing * 0.5f, 0, -ribbonInstanceNumber * spacing * 0.5f);
            vegetationShader.Vars()["gConstantBuffer"]["repeatScale"] = spacing;
            vegetationShader.Vars()["gConstantBuffer"]["numSide"] = ribbonInstanceNumber;

            //vegetationShader.Vars()["gConstantBuffer"]["objectScale"] = float3(ribbonVertex::objectScale, ribbonVertex::objectScale, ribbonVertex::objectScale);
            //vegetationShader.Vars()["gConstantBuffer"]["objectOffset"] = ribbonVertex::objectOffset;
            //vegetationShader.Vars()["gConstantBuffer"]["radiusScale"] = ribbonVertex::radiusScale;

            // lighting
            //vegetationShader.Vars()["gConstantBuffer"]["Ao_depthScale"] = groveTree.gpuPlant.Ao_depthScale;
            //vegetationShader.Vars()["gConstantBuffer"]["sunTilt"] = groveTree.gpuPlant.sunTilt;
            //vegetationShader.Vars()["gConstantBuffer"]["bDepth"] = groveTree.gpuPlant.bDepth;
            //vegetationShader.Vars()["gConstantBuffer"]["bScale"] = groveTree.gpuPlant.bScale;

            static float time = 0.0f;
            time += 0.01f;  // FIXME I NEED A Timer
            vegetationShader.Vars()["gConstantBuffer"]["time"] = time;

            vegetationShader.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);


            vegetationShader.drawInstanced(_renderContext, groveTree.numBranchRibbons, ribbonInstanceNumber * ribbonInstanceNumber);
        }
        */
        return;
    }

    if (terrainMode == _terrainMode::glider)
    {

        {
            FALCOR_PROFILE("skydome");

            triangleShader.State()->setFbo(_fbo);
            triangleShader.State()->setViewport(0, _viewport, true);
            triangleShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
            triangleShader.Vars()["gConstantBuffer"]["eye"] = _camera->getPosition();
            triangleShader.Vars()["gConstantBuffer"]["useSkyDome"] = 0;
            triangleShader.State()->setRasterizerState(split.rasterstateSplines);
            triangleShader.State()->setBlendState(split.blendstateSplines);
            triangleShader.drawInstanced(_renderContext, 36, 1);
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

            ribbonData[0]->setBlob(groveTree.packedRibbons, 0, groveTree.numBranchRibbons * sizeof(unsigned int) * 6);
            groveTree.bChanged = false;

            ribbonShader.State()->setFbo(_fbo);
            ribbonShader.State()->setViewport(0, _viewport, true);

            ribbonShader.Vars()["gConstantBuffer"]["fakeShadow"] = 4;
            //ribbonShader.Vars()["gConstantBuffer"]["objectScale"] = float3(ribbonVertex::objectScale, ribbonVertex::objectScale, ribbonVertex::objectScale);
            //ribbonShader.Vars()["gConstantBuffer"]["objectOffset"] = ribbonVertex::objectOffset;
            //ribbonShader.Vars()["gConstantBuffer"]["radiusScale"] = ribbonVertex::radiusScale;



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

            ribbonShader.Vars()["gConstantBuffer"]["objectScale"] = float3(ribbonVertex::objectScale, ribbonVertex::objectScale, ribbonVertex::objectScale);
            ribbonShader.Vars()["gConstantBuffer"]["objectOffset"] = ribbonVertex::objectOffset;
            ribbonShader.Vars()["gConstantBuffer"]["radiusScale"] = ribbonVertex::radiusScale;


            static float time = 0.0f;
            time += 0.01f;  // FIXME I NEED A Timer
            ribbonShader.Vars()["gConstantBuffer"]["time"] = time;

            ribbonShader.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);


            ribbonShader.drawInstanced(_renderContext, groveTree.numBranchRibbons, ribbonInstanceNumber * ribbonInstanceNumber);
        }



        if (!useFreeCamWhileGliding && (terrainMode == _terrainMode::glider) && numLoadedRibbons > 1)
            //if ((terrainMode == 4) && AirSim.ribbonCount > 1)
        {
            ribbonShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj_ribbon;
            ribbonShader.Vars()["gConstantBuffer"]["eyePos"] = paraEyeLocal;// _camera->getPosition();

            static float spacing = 1.0f;

            ribbonShader.Vars()["gConstantBuffer"]["offset"] = float3(0, 0, 0);
            ribbonShader.Vars()["gConstantBuffer"]["repeatScale"] = spacing;
            ribbonShader.Vars()["gConstantBuffer"]["numSide"] = 1;

            ribbonShader.Vars()["gConstantBuffer"]["objectScale"] = float3(ribbonVertex::objectScale, ribbonVertex::objectScale, ribbonVertex::objectScale);
            ribbonShader.Vars()["gConstantBuffer"]["objectOffset"] = ribbonVertex::objectOffset;
            ribbonShader.Vars()["gConstantBuffer"]["radiusScale"] = ribbonVertex::radiusScale;


            ribbonShader.Vars()["gConstantBuffer"]["ROOT"] = float3(0, 0, 0);// paraRuntime.ROOT;

            static float time = 0.0f;
            time += 0.01f;  // FIXME I NEED A Timer
            ribbonShader.Vars()["gConstantBuffer"]["time"] = time;

            ribbonShader.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);



            // ribbonShader.drawInstanced(_renderContext, paraRuntime.ribbonCount, 1);
            ribbonShader.drawInstanced(_renderContext, numLoadedRibbons, 1);
            //ribbonShader.drawInstanced(_renderContext, AirSim.ribbonCount, 1);

            if (paraRuntime.ribbonCount != 456)
            {
                bool cm = true;
            }

        }


        //return;
    }

    {
        FALCOR_PROFILE("terrainManager");

        //terrainShader.State() = _graphicsState;
        terrainShader.State()->setFbo(_fbo);
        terrainShader.State()->setViewport(0, _viewport, true);
        terrainShader.Vars()["gConstantBuffer"]["view"] = view;
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
        //D = paraRuntime.camDir;
        glm::vec3 R = glm::normalize(glm::cross(glm::vec3(0, 1, 0), D));
        terrainSpiteShader.Vars()["gConstantBuffer"]["right"] = R;
        terrainSpiteShader.Vars()["gConstantBuffer"]["alpha_pass"] = 0;

        terrainSpiteShader.Vars()->setTexture("gEnv", vegetation.envTexture);
        terrainSpiteShader.Vars()->setTexture("gHalfBuffer", _hdrHalfCopy);

        // FIXME need a way to do only on change
        auto& blockBB = terrainSpiteShader.Vars()->getParameterBlock("textures");
        ShaderVar varBBTextures = blockBB->findMember("T");
        _plantMaterial::static_materials_veg.setTextures(varBBTextures);
        //terrainSpiteShader.State()->setRasterizerState(split.rasterstateSplines);
        //terrainSpiteShader.State()->setBlendState(split.blendstateSplines);

        terrainSpiteShader.renderIndirect(_renderContext, split.drawArgs_quads);

        //terrainSpiteShader.Vars()["gConstantBuffer"]["alpha_pass"] = 1;
        //terrainSpiteShader.renderIndirect(_renderContext, split.drawArgs_quads);
    }

    {

        FALCOR_PROFILE("buildings");

        rappersvilleShader.State()->setFbo(_fbo);
        rappersvilleShader.State()->setViewport(0, _viewport, true);
        rappersvilleShader.Vars()["PerFrameCB"]["view"] = view;
        rappersvilleShader.Vars()["PerFrameCB"]["viewproj"] = viewproj;
        rappersvilleShader.Vars()["PerFrameCB"]["eye"] = _camera->getPosition();
        rappersvilleShader.drawInstanced(_renderContext, 3, numrapperstri);

    }

    {
        FALCOR_PROFILE("skydome");

        triangleShader.State()->setFbo(_fbo);
        triangleShader.State()->setViewport(0, _viewport, true);
        triangleShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        triangleShader.Vars()["gConstantBuffer"]["eye"] = _camera->getPosition();
        triangleShader.Vars()["gConstantBuffer"]["useSkyDome"] = 0;
        triangleShader.State()->setRasterizerState(split.rasterstateSplines);
        triangleShader.State()->setBlendState(split.blendstateSplines);
        triangleShader.drawInstanced(_renderContext, 36, 1);
        //triangleShader.renderIndirect(_renderContext, split.drawArgs_clippedloddedplants);
    }


    /*
    if (cfd.recordingCFD)
    {
        cfd.recordingCFD = cfd.clipmap.import_V(settings.dirRoot + "/cfd/dump/east3ms__" + std::to_string(cfd.cfd_play_k) + "sec");
        cfd.clipmap.streamlines(float3(-2800, 450, 12500), cfd.flowlines.data(), float3(1, 0, 0));
        thermalsData->setBlob(cfd.flowlines.data(), 0, numThermals * 100 * sizeof(float4));
        cfd.cfd_play_k++;
    } */

    if (cfd.clipmap.showStreamlines)
    {
        FALCOR_PROFILE("CFD");

        if (cfd.newFlowLines)
        {
            //FALCOR_PROFILE("CFD update");
            thermalsData->setBlob(cfd.flowlines.data(), 0, numThermals * 100 * sizeof(float4));
            cfd.newFlowLines = false;
        }


        thermalsShader.State()->setFbo(_fbo);
        thermalsShader.State()->setViewport(0, _viewport, true);
        thermalsShader.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        thermalsShader.Vars()["gConstantBuffer"]["eyePos"] = _camera->getPosition();
        thermalsShader.State()->setRasterizerState(rasterstateGliderWing);
        thermalsShader.drawInstanced(_renderContext, 100, numThermals);
    }






    {
        FALCOR_PROFILE("glider wing");
        gliderwingShader.State()->setFbo(_fbo);
        gliderwingShader.State()->setViewport(0, _viewport, true);
        gliderwingShader.Vars()["PerFrameCB"]["viewproj"] = viewproj;
        gliderwingShader.Vars()["PerFrameCB"]["eye"] = _camera->getPosition();
        gliderwingShader.State()->setRasterizerState(rasterstateGliderWing);
        gliderwingShader.drawInstanced(_renderContext, wingloadedCnt, 1);
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

    if (terrainMode == _terrainMode::terrafector)
    {
        split.shader_spline3D.State()->setFbo(_fbo);
        split.shader_spline3D.State()->setRasterizerState(split.rasterstateSplines);
        split.shader_spline3D.State()->setBlendState(split.blendstateSplines);
        split.shader_spline3D.State()->setDepthStencilState(split.depthstateAll);
        split.shader_spline3D.State()->setViewport(0, _viewport, true);

        split.shader_spline3D.Vars()["gConstantBuffer"]["viewproj"] = viewproj;
        split.shader_spline3D.Vars()["gConstantBuffer"]["alpha"] = gis_overlay.splineOverlayStrength;

        split.shader_spline3D.Vars()->setBuffer("materials", terrafectorEditorMaterial::static_materials.sb_Terrafector_Materials);

        auto& block = split.shader_spline3D.Vars()->getParameterBlock("gmyTextures");
        ShaderVar& var = block->findMember("T");        // FIXME pre get
        terrafectorEditorMaterial::static_materials.setTextures(var);

        split.shader_spline3D.Vars()->setBuffer("splineData", splines.dynamic_bezierData);
        split.shader_spline3D.Vars()->setBuffer("indexData", splines.dynamic_indexData);
        split.shader_spline3D.drawIndexedInstanced(_renderContext, 64 * 6, splines.numDynamicStampIndex);
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
        /*
        if (terrainMode == _terrainMode::terrafector)
        {
            // render static stamps
            split.shader_spline3D.Vars()->setBuffer("splineData", splines.bezierData);
            split.shader_spline3D.Vars()->setBuffer("indexData", splines.indexData);
            split.shader_spline3D.drawIndexedInstanced(_renderContext, 64 * 6, splines.numStaticSplinesIndex);
        }
        */

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
        FALCOR_PROFILE("debug");
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

        sprintf(debugTxt, "%d tiles with quads    %d total quads, %d blocks, {%d}max", split.feedback.numQuadTiles, split.feedback.numQuads, split.feedback.numQuadBlocks, split.feedback.maxQuads);
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
                    }
                }
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
}


void terrainManager::updateDynamicStamp()
{
}


void terrainManager::stamp_to_Bezier(stamp& S, cubicDouble* BEZ, bezierLayer* IDX, int _index)
{
    float3 A = S.pos - S.right - S.dir;
    float3 B = S.pos + S.right - S.dir;
    float3 C = S.pos - S.right + S.dir;
    float3 D = S.pos + S.right + S.dir;

    BEZ->data[0][0] = { A, 0 };
    BEZ->data[0][1] = { A, 0 };
    BEZ->data[0][2] = { C, 1 };
    BEZ->data[0][3] = { C, 1 };

    BEZ->data[1][0] = { B, 0 };
    BEZ->data[1][1] = { B, 0 };
    BEZ->data[1][2] = { D, 1 };
    BEZ->data[1][3] = { D, 1 };

    *IDX = bezierLayer(bezier_edge::outside, bezier_edge::center, S.material, _index, true, 0, 0);
    IDX->B |= 0x1 << 29; // isQuad
}


void terrainManager::currentStamp_to_Bezier()
{
    splineTest.bSegment = false;
    splineTest.bVertex = false;
    splineTest.testDistance = 1000;
    splineTest.bCenter = false;
    splineTest.cornerNum = -1;
    splineTest.pos = split.feedback.tum_Position;
    splineTest.bStreetview = false;

    mCurrentStamp.material = terrafectorEditorMaterial::static_materials.selectedMaterial;
    int matIdx = mCurrentStamp.material;
    if (matIdx >= 0)
    {
        terrafectorEditorMaterial mat = terrafectorEditorMaterial::static_materials.materialVector[matIdx];

        float3 N = split.feedback.tum_Normal;
        mCurrentStamp.right = { cos(mCurrentStamp.rotation), 0, -sin(mCurrentStamp.rotation) };
        mCurrentStamp.dir = glm::normalize(glm::cross(mCurrentStamp.right, N)) * 0.5f * mat.stampHeight * mCurrentStamp.scale.y;
        mCurrentStamp.right = glm::normalize(glm::cross(N, mCurrentStamp.dir)) * 0.5f * mat.stampWidth * mCurrentStamp.scale.x;

        mCurrentStamp.pos = split.feedback.tum_Position + (N * mCurrentStamp.height);

        // FIXME scale dir and right here

        cubicDouble BEZ;
        bezierLayer IDX;
        stamp_to_Bezier(mCurrentStamp, &BEZ, &IDX, 0);

        splines.numDynamicStampIndex = 1;
        splines.dynamic_bezierData->setBlob(&BEZ, 0, sizeof(cubicDouble));
        splines.dynamic_indexData->setBlob(&IDX, 0, sizeof(bezierLayer));
    }
    else
    {
        splines.numDynamicStampIndex = 0;
    }

}


void terrainManager::allStamps_to_Bezier()
{
    /*
    int size = mRoadStampCollection.stamps.size();
    roadNetwork::staticBezierData.resize(size);
    roadNetwork::staticIndexData.resize(size);

    for (int i = 0; i < size; i++)
    {
        stamp_to_Bezier(mRoadStampCollection.stamps[i], &roadNetwork::staticBezierData.at(i), &roadNetwork::staticIndexData.at(i), i);
    }

    splines.numStaticSplines = size;
    splines.numStaticSplinesIndex = size;
    splines.bezierData->setBlob(roadNetwork::staticBezierData.data(), 0, splines.numStaticSplines * sizeof(cubicDouble));
    splines.indexData->setBlob(roadNetwork::staticIndexData.data(), 0, splines.numStaticSplinesIndex * sizeof(bezierLayer));

    allStamps_to_Terrafector();
    */
}

void terrainManager::allStamps_to_Terrafector()
{
    int size = mRoadStampCollection.stamps.size();
    terrafectorSystem::loadCombine_LOD7_stamps.create(7);   // the create clears it
    lodTriangleMesh lodder_stamp;
    lodder_stamp.create(7);

    float3 pos[3];
    float2 uv[2];
    for (auto& S : mRoadStampCollection.stamps)
    {
        if (S.scale.x != 0)
        {
            pos[0] = S.pos - S.right - S.dir;
            pos[1] = S.pos + S.right - S.dir;
            pos[2] = S.pos + S.right + S.dir;
            uv[0] = { 0, 0 };
            uv[1] = { 1, 0 };
            uv[2] = { 1, 1 };
            lodder_stamp.insertTriangle(S.material, pos, uv);

            pos[0] = S.pos - S.right - S.dir;
            pos[1] = S.pos + S.right + S.dir;
            pos[2] = S.pos - S.right + S.dir;
            uv[0] = { 0, 0 };
            uv[1] = { 1, 1 };
            uv[2] = { 0, 1 };
            lodder_stamp.insertTriangle(S.material, pos, uv);
        }
    }

    terrafectorSystem::loadCombine_LOD7_stamps.addMesh("", lodder_stamp, false);
    terrafectorSystem::loadCombine_LOD7_stamps.loadToGPU("", true);
}



void terrainManager::updateDynamicRoad(bool _bezierChanged) {

    if (this->terrainMode == _terrainMode::terrafector)
    {
        currentStamp_to_Bezier();
        return;
    }

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

    // terrain mode
    if (keyPressed && keyEvent.key == Input::Key::Key0) terrainMode = _terrainMode::vegetation;
    if (keyPressed && keyEvent.key == Input::Key::Key1) terrainMode = _terrainMode::ecotope;
    if (keyPressed && keyEvent.key == Input::Key::Key2) terrainMode = _terrainMode::terrafector;
    if (keyPressed && keyEvent.key == Input::Key::Key3) terrainMode = _terrainMode::roads;
    //if (keyPressed && keyEvent.key == Input::Key::Key4) terrainMode = _terrainMode::glider;

    splineTest.bCtrl = keyEvent.hasModifier(Input::Modifier::Ctrl);
    splineTest.bShift = keyEvent.hasModifier(Input::Modifier::Shift);
    splineTest.bAlt = keyEvent.hasModifier(Input::Modifier::Alt);

    if (terrainMode == _terrainMode::glider)   // Paragliding
    {
        if (keyPressed && keyEvent.key == Input::Key::Escape) {
            renderGui_Menu = !renderGui_Menu;
        }
        if (keyPressed && keyEvent.key == Input::Key::H) {
            renderGui_Hud = !renderGui_Hud;
        }
        if (keyPressed && keyEvent.key == Input::Key::J) {
            useFreeCamWhileGliding = !useFreeCamWhileGliding;
        }
        if (keyPressed && keyEvent.key == Input::Key::K) {
            GliderDebugVisuals = !GliderDebugVisuals;
        }

    }




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
        case 2:
            // Terrafector - placinf stamps in here
            if (keyPressed)
            {
                if (keyEvent.key == Input::Key::B)
                {
                    bSplineAsTerrafector = !bSplineAsTerrafector;
                    reset(true);
                    return true;
                }
            }
            break;


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
                        mRoadNetwork.updateAllRoads();
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

            case Input::Key::Y:
                // road new spline
                if (keyPressed) {
                    mRoadNetwork.newRoadSplineBasic();
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

    if ((terrainMode == _terrainMode::vegetation) || (terrainMode == _terrainMode::glider && !useFreeCamWhileGliding))
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
                mouseVegPitch += diff.y * 3.0f;
                mouseVegPitch = glm::clamp(mouseVegPitch, -1.f, 1.5f);

                mouseVegYaw += diff.x * 10.0f;
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
        if ((terrainMode == _terrainMode::vegetation))
        {
            glm::vec3 camPos;
            camPos.y = sin(mouseVegPitch);
            camPos.x = cos(mouseVegPitch) * sin(mouseVegYaw);
            camPos.z = cos(mouseVegPitch) * cos(mouseVegYaw);
            _camera->setPosition((camPos * mouseVegOrbit) + float3(0, 1000, 0));
            _camera->setTarget(glm::vec3(0, 1000.f + groveTree.treeHeight * 0.5f, 0));
        }
        if ((terrainMode == _terrainMode::glider))
        {
            //_camera->setTarget(paraRuntime.ROOT);
        }
        return true;
    }
    else {
        bool bEdit = false;
        if ((terrainMode == _terrainMode::terrafector))
        {
            bEdit = onMouseEvent_Stamps(mouseEvent, _screenSize, _camera);
        }
        if ((terrainMode == _terrainMode::roads))
        {
            bEdit = onMouseEvent_Roads(mouseEvent, _screenSize, _camera);
        }


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



bool terrainManager::onMouseEvent_Stamps(const MouseEvent& mouseEvent, glm::vec2 _screenSize, Camera::SharedPtr _camera)
{
    switch (mouseEvent.type)
    {
    case MouseEvent::Type::Move:
    {
    }
    break;
    case MouseEvent::Type::Wheel:
    {
        if (splineTest.bCtrl)
        {
            if (splineTest.bAlt)
            {
                mCurrentStamp.rotation += mouseEvent.wheelDelta.y / 30.0f;
            }
            else if (splineTest.bShift)
            {
                mCurrentStamp.height += mouseEvent.wheelDelta.y / 30.0f;
            }
            else
            {
                mCurrentStamp.rotation += mouseEvent.wheelDelta.y / 3.0f;
            }


            return true;
        }

        if (splineTest.bShift)
        {
            if (splineTest.bAlt)
            {
                mCurrentStamp.scale.x *= 1.f + mouseEvent.wheelDelta.y / 30.0f;
            }
            else
            {
                mCurrentStamp.scale.y *= 1.f + mouseEvent.wheelDelta.y / 30.0f;
            }
            return true;
        }


    }
    case MouseEvent::Type::ButtonDown:
    {
        if (mouseEvent.button == Input::MouseButton::Left)
        {
            if (stampEditPosisiton >= mRoadStampCollection.stamps.size())
            {
                mRoadStampCollection.add(mCurrentStamp);
                stampEditPosisiton = mRoadStampCollection.stamps.size();
            }
            else
            {
                mRoadStampCollection.stamps[stampEditPosisiton] = mCurrentStamp;    // FIXME does nto add material could cause a problem
                stampEditPosisiton = mRoadStampCollection.stamps.size();
            }

            //allStamps_to_Bezier();
            allStamps_to_Terrafector();
            reset(true);
        }

        if (mouseEvent.button == Input::MouseButton::Right)
        {
            if (splineTest.bAlt)
            {
                mCurrentStamp.height = 0;
                mCurrentStamp.scale = { 1, 1 };
            }
            if (splineTest.bCtrl)
            {
                // delete closest but set current stamp to it
                int idx = mRoadStampCollection.find(split.feedback.tum_Position);
                if (idx >= 0)
                {
                    mCurrentStamp = mRoadStampCollection.stamps[idx];
                    terrafectorEditorMaterial::static_materials.selectedMaterial = mCurrentStamp.material;
                    mRoadStampCollection.stamps[idx].scale = { 0, 0 };  // then set scale to 0 if this is justa delte
                    stampEditPosisiton = idx;
                    allStamps_to_Terrafector();
                    reset(true);
                }
            }
            if (splineTest.bShift)
            {
                // Just copy current stamp
                int idx = mRoadStampCollection.find(split.feedback.tum_Position);
                if (idx >= 0)
                {
                    mCurrentStamp = mRoadStampCollection.stamps[idx];
                    terrafectorEditorMaterial::static_materials.selectedMaterial = mCurrentStamp.material;
                }
            }
        }
    }
    }

    return false;
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




void terrainManager::cfdStart()
{

}

void terrainManager::cfdThread()
{
    bool firstStart = true;
    cfd.numLines = 200;
    cfd.flowlines.resize(cfd.numLines * 100);

    //cfd.clipmap.heightToSmap(settings.dirRoot + "/gis/_export/root4096.bil");
    cfd.rootPath = settings.dirRoot + "\\cfd\\";
    cfd.rootFile = "Walenstad_August5_1500.skewT_5000";
    cfd.clipmap.loadSkewT(cfd.rootPath + cfd.rootFile);
    cfd.clipmap.build(settings.dirRoot + "/cfd");

    //uint seconds = 0;// 27040;
    //cfd.clipmap.import_V(settings.dirRoot + "/cfd/east3ms__" + std::to_string(seconds) + "sec");

    cfd.clipmap.setWind(float3(2, 0, 0), float3(5, 0, 0));
    cfd.clipmap.simulate_start(32.0f);


    fprintf(terrafectorSystem::_logfile, "cfdThread()\n");
    fprintf(terrafectorSystem::_logfile, "T %f %f %f\n", cfd.clipmap.skewT_corners_Data[0][0].x, cfd.clipmap.skewT_corners_Data[0][50].x, cfd.clipmap.skewT_corners_Data[0][99].x);
    fprintf(terrafectorSystem::_logfile, "H %f %f %f\n", cfd.clipmap.skewT_corners_Data[0][0].y, cfd.clipmap.skewT_corners_Data[0][50].y, cfd.clipmap.skewT_corners_Data[0][99].y);
    fprintf(terrafectorSystem::_logfile, "V %f %f %f\n", glm::length(cfd.clipmap.skewT_corners_V[0][0]), glm::length(cfd.clipmap.skewT_corners_V[0][50]), glm::length(cfd.clipmap.skewT_corners_V[0][99]));
    fflush(terrafectorSystem::_logfile);
    /*
    uint saveidx = 0;
    for (int k = 0; k < 256 * 64 * 100; k++)   //1500 = 937 seconds in 210
    //for (int k = 0; k < 256 * 300; k++)   //1500 = 937 seconds in 210
    {
        cfd.clipmap.simulate(20.0f);
        if (k % 64 == 0)
        {
            uint seconds = (k * 20) / 32;
            cfd.clipmap.export_V(settings.dirRoot + "/cfd/dump/east3ms__" + std::to_string(saveidx) + "sec");
            saveidx++;
        }
    } /**/

    // infinite loop
    static int k = 0;
    while (1)
    {
        if (cfd.pause)
        {
            Sleep(10);
        }
        else
        {
            cfd.clipmap.shiftOrigin(cfd.originRequest);

            if (firstStart || cfd.clipmap.windrequest)
            {
                if (cfd.clipmap.windSeperateSkewt)
                {
                    cfd.clipmap.setWind(cfd.clipmap.newWind, cfd.clipmap.newWind);
                }
                else
                {
                    cfd.clipmap.loadSkewT(cfd.rootPath + cfd.rootFile);
                }
                cfd.clipmap.simulate_start(32.0f);
                cfd.clipmap.windrequest = false;
                firstStart = false;
                fprintf(terrafectorSystem::_logfile, "cfd.clipmap.setWind on %s\n", cfd.rootPath.c_str());
                if (cfd.clipmap.windSeperateSkewt) {
                    fprintf(terrafectorSystem::_logfile, "windSeperateSkewt (%2.1f, %2.1f, %2.1f) m/s\n", cfd.clipmap.newWind.x, cfd.clipmap.newWind.y, cfd.clipmap.newWind.z);
                }
            }



            cfd.clipmap.simulate(32.0f);

            if (k % 2 == 0)
            {
                float3 R = glm::normalize(glm::cross(float3(0, 1, 0), cfd.velocityAnswers[0]));
                cfd.clipmap.streamlines(cfd.originRequest, cfd.flowlines.data(), R);
                cfd.newFlowLines = true;
            }

            for (int i = 0; i < 4; i++) // Look yp my glider velocity requests
            {
                float rootAltitude = 350.f;   // in voxel space
                float3 origin = float3(-20000, rootAltitude, -20000);
                float3 P = (cfd.velocityRequets[i] - origin) * cfd.clipmap.lods[0].oneOverSize;
                float3 p5 = P * 32.f;
                p5 -= cfd.clipmap.lods[5].offset;
                cfd.velocityAnswers[i] = cfd.clipmap.lods[5].sample(cfd.clipmap.lods[5].v, p5);
            }


        }
        k++;
    }
}



void terrainManager::paragliderThread(BarrierThrd& bar)
{
    float _dT = 0.002f;

    while (1)
    {
        if (glider.loaded)
        {
            auto a = high_resolution_clock::now();

            if (paraRuntime.runcount == 0) _dT = 0.0002f;
            else _dT = 0.002f;

            if (paraRuntime.requestRestart)
            {
                //bool requestRestart = false;
                paraRuntime.setup(paraBuilder.x, paraBuilder.w, paraBuilder.cross, paraBuilder.spanSize, paraBuilder.chordSize, paraBuilder.constraints, true, cameraViews[CameraType_Main_Center].view);
                paraRuntime.requestRestart = false;
            }


            paraRuntime.solve(_dT);

            //while (paraRuntime.runcount == 0)
            //{
                //Sleep(1);
              //  auto xb = high_resolution_clock::now();
            //}
            bar.wait();


            paraRuntime.solve_air(_dT, true);
            bar.wait();
            paraRuntime.solve_aoa(_dT, true);
            bar.wait();
            paraRuntime.solve_pressure(_dT, true);
            bar.wait();
            paraRuntime.solve_surface(_dT, true);
            auto glide = high_resolution_clock::now();
            glider.cellsTime[0] = (float)duration_cast<microseconds>(glide - a).count() / 1000.;
            bar.wait();//??? is it nessesary, check code



            paraRuntime.solve_PRE(_dT);
            paraRuntime.solve_linesTensions(_dT);
            paraRuntime.solve_triangle(_dT);
            auto pre = high_resolution_clock::now();
            glider.preTime[0] = (float)duration_cast<microseconds>(pre - glide).count() / 1000.;





            for (int k = 0; k < 3; k++)
            {
                bar.wait();
                for (int l = 0; l < 10; l++)
                {
                    bool last = false;
                    if (l == 9) last = true;
                    paraRuntime.linesLeft.solveUp(paraRuntime.x, last);
                }
                bar.wait();
                paraRuntime.solve_wing(_dT, true);
            }
            auto wing = high_resolution_clock::now();
            glider.wingTime[0] = (float)duration_cast<microseconds>(wing - pre).count() / 1000.;

            bar.wait();

            //if ((paraRuntime.runcount > 0))
            {
                paraRuntime.solve_POST(_dT);
                paraRuntime.movingROOT();
            }
            auto post = high_resolution_clock::now();
            glider.postTime[0] = (float)duration_cast<microseconds>(post - wing).count() / 1000.;


            auto endTime = a + 2000us;
            auto b = high_resolution_clock::now();

            paraRuntime.CPU_usage = glm::lerp(paraRuntime.CPU_usage, (float)(2.f - ((float)duration_cast<microseconds>(endTime - b).count() / 1000.)) / 2.f, 0.01f);

            while (b < endTime)
            {
                b = high_resolution_clock::now();
            }


            glider.frameTime = (float)duration_cast<microseconds>(b - a).count() / 1000.;



            if (requestParaPack)
            {
                auto pack_a = high_resolution_clock::now();
                paraRuntime.eye_andCamera();

                paraRuntime.pack_canopy();
                paraRuntime.pack_lines();
                paraRuntime.pack_feedback();
                paraRuntime.pack();

                paraCamPos = paraRuntime.camPos;
                paraEyeLocal = paraRuntime.EyeLocal;

                requestParaPack = false;

                auto pack_b = high_resolution_clock::now();
                glider.packTime = (float)duration_cast<microseconds>(pack_b - pack_a).count() / 1000.;
            }
        }
    }
}



void terrainManager::paragliderThread_B(BarrierThrd& bar)
{
    float _dT = 0.002f;

    while (1)
    {
        if (glider.loaded)
        {
            auto a = high_resolution_clock::now();


            if (paraRuntime.runcount == 0) _dT = 0.0002f;
            else _dT = 0.002f;

            //paraRuntime.solve(_dT);

            bar.wait();



            paraRuntime.solve_air(_dT, false);
            bar.wait();
            paraRuntime.solve_aoa(_dT, false);
            bar.wait();
            paraRuntime.solve_pressure(_dT, false);
            bar.wait();
            paraRuntime.solve_surface(_dT, false);
            auto glide = high_resolution_clock::now();
            glider.cellsTime[1] = (float)duration_cast<microseconds>(glide - a).count() / 1000.;
            bar.wait();

            //paraRuntime.solve_PRE(_dT);
            //paraRuntime.solve_linesTensions(_dT);
            //paraRuntime.solve_triangle(_dT);
            auto pre = high_resolution_clock::now();
            glider.preTime[1] = (float)duration_cast<microseconds>(pre - glide).count() / 1000.;




            for (int k = 0; k < 3; k++)
            {
                bar.wait();
                for (int l = 0; l < 10; l++)
                {
                    bool last = false;
                    if (l == 9) last = true;
                    paraRuntime.linesRight.solveUp(paraRuntime.x, last);
                }
                bar.wait();
                paraRuntime.solve_wing(_dT, false);
            }
            auto wing = high_resolution_clock::now();
            glider.wingTime[1] = (float)duration_cast<microseconds>(wing - pre).count() / 1000.;

            bar.wait();
            //paraRuntime.solve_POST(_dT);
            //paraRuntime.movingROOT();
            auto post = high_resolution_clock::now();
            glider.postTime[1] = (float)duration_cast<microseconds>(post - wing).count() / 1000.;

        }
    }
}
