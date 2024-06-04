#include "cfd.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <iostream>
#include <fstream>


//#pragma optimize("", off)


uint _cfd::idx(int x, int y, int z)
{
    return (y * cfdW * cfdW) + (z * cfdW) + (x);
}

uint _cfd::idx(uint3 p)
{
    return (p.y * cfdW * cfdW) + (p.z * cfdW) + (p.x);
}


void _cfd::init()
{
    s.resize(cfdH * cfdW * cfdW);
    v.resize(cfdH * cfdW * cfdW);
    v2.resize(cfdH * cfdW * cfdW);
}


void _cfd::loadHeight(std::string filename)
{
    std::ifstream ifs;
    ifs.open(filename, std::ios::binary);
    if (ifs)
    {
        ifs.read((char*)height, 4096 * 4096 * 4);
        ifs.close();
    }



    // depth of each vertex
    for (uint h = 0; h < cfdH; h++)
    {
        float hgt = cfdMinAlt + h * cfdSize;
        for (uint y = 0; y < cfdW; y++) {
            for (uint x = 0; x < cfdW; x++)
            {
                float3 sum = float3(0, 0, 0);
                for (uint i = 0; i < 4; i++)
                {
                    // edgees
                    float Hy = hgt - height[y * 4 + 0][x * 4 + i];
                    float Hx = hgt - height[y * 4 + i][x * 4 + 0];
                    float dhx = glm::clamp(Hx / cfdSize, 0.f, 1.f);
                    float dhy = glm::clamp(Hy / cfdSize, 0.f, 1.f);
                    sum.x += dhx * 0.25f;
                    sum.z += dhy * 0.25f;

                    for (uint j = 0; j < 4; j++)
                    {
                        // bottom
                        if (hgt - height[y * 4 + i][x * 4 + j] > 0) sum.y += 0.0625f;
                    }
                }
                if (sum.y > 0.1f && sum.y < 0.9f)
                {
                    bool cm = true;
                }

                s[idx(x, h, y)] = sum;
            }
        }
    }
}

void _cfd::export_S(std::string filename)
{
    std::ofstream ofs;
    ofs.open(filename, std::ios::binary);
    if (ofs)
    {
        ofs.write((char*)s.data(), cfdW * cfdW * cfdH * sizeof(float3));
        ofs.close();
    }
}

void _cfd::import_S(std::string filename)
{
    std::ifstream ifs;
    ifs.open(filename, std::ios::binary);
    if (ifs)
    {
        ifs.read((char*)s.data(), cfdW * cfdW * cfdH * sizeof(float3));
        ifs.close();
    }
}

void _cfd::export_V(std::string filename)
{
    std::ofstream ofs;
    ofs.open(filename, std::ios::binary);
    if (ofs)
    {
        ofs.write((char*)v.data(), cfdW * cfdW * cfdH * sizeof(float3));
        ofs.close();
    }
}

void _cfd::import_V(std::string filename)
{
    std::ifstream ifs;
    ifs.open(filename, std::ios::binary);
    if (ifs)
    {
        ifs.read((char*)v.data(), cfdW * cfdW * cfdH * sizeof(float3));
        ifs.close();
    }
}


void _cfd::setWind(float3 _windBottom, float3 _windTop)
{
    for (uint h = 0; h < cfdH; h++) {
        float3 wind = glm::lerp(_windBottom, _windTop, (float)h / (float)cfdH);
        for (uint y = 0; y < cfdW; y++) {
            for (uint x = 0; x < cfdW; x++)
            {
                uint i = idx(x, h, y);
                v[i].x = s[i].x * wind.x;
                v[i].y = s[i].y * wind.y;
                v[i].z = s[i].z * wind.z;
            }
        }
    }
}



// simulate
void _cfd::gavity(float _dt)
{
    for (uint h = 0; h < cfdH; h++) {
        for (uint y = 0; y < cfdW; y++) {
            for (uint x = 0; x < cfdW; x++) {
                uint i = idx(x, h, y);
                v[i].y -= 9.8f * _dt * s[i].y;  // not 100% but the general flow shoul fix it where we  touch
            }
        }
    }
}


// without pressure for now - SPEED ;-)
void _cfd::solveIncompressibility(uint _num)
{
    float3 smin, smax;
    for (uint n = 0; n < _num; n++)
    {
        for (uint h = 0; h < cfdH - 1; h++) {
            for (uint y = 1; y < cfdW - 1; y++) {
                for (uint x = 1; x < cfdW - 1; x++) {
                    smin = s[idx(x, h, y)];
                    smax.x = s[idx(x + 1, h, y)].x;
                    smax.y = s[idx(x, h + 1, y)].y;
                    smax.z = s[idx(x, h, y + 1)].z;
                    float sum = smin.x + smin.y + smin.z + smax.x + smax.y + smax.z;
                    if (sum > 0.00001f)
                    {
                        // not sdure I like this but lets see
                        // isntivntively WROGN scale with S to gel flow rate
                        float vx0 = smin.x * v[idx(x, h, y)].x;
                        float vx1 = smax.x * v[idx(x + 1, h, y)].x;
                        float vy0 = smin.y * v[idx(x, h, y)].y;
                        float vy1 = smax.y * v[idx(x, h + 1, y)].y;
                        float vz0 = smin.z * v[idx(x, h, y)].z;
                        float vz1 = smax.z * v[idx(x, h, y + 1)].z;
                        float div = vx0 - vx1 + vy0 - vy1 + vz0 - vz1;
                        float P = div / sum * 1.9f;    // overrelaxed

                        // gauss seidel verwag oorskryf van waardes maat ons propagate in een rigting
                        v[idx(x, h, y)].x -= smin.x * P;
                        v[idx(x, h, y)].y -= smin.y * P;
                        v[idx(x, h, y)].z -= smin.z * P;

                        v[idx(x + 1, h, y)].x += smax.x * P;
                        v[idx(x, h + 1, y)].y += smax.y * P;
                        v[idx(x, h, y + 1)].z += smax.z * P;

                        float vv = glm::length(v[idx(x, h, y)]);
                        bool cm = true;
                    }
                }
            }
        }
    }
}


void _cfd::edges()
{
}

/*  While the edge metod is interesting is does m,ale 3 times the work in here , is it really worth it for that
*   etra info?
*   I doubt a GPU will love it
*/
void _cfd::advect(float _dt)
{
    float3 V, P;

    for (uint h = 0; h < cfdH - 1; h++) {
        for (uint y = 2; y < cfdW - 2; y++) {
            for (uint x = 2; x < cfdW - 2; x++) {
                //x
                V = (v[idx(x, h, y)] + v[idx(x + 1, h, y)] + v[idx(x, h + 1, y)] + v[idx(x + 1, h + 1, y)]) * 0.25f;
                P = float3(x, h + 0.5f, y + 0.5f);    // in texture pixel space
                P -= V * _dt / cfdSize;
                v2[idx(x, h, y)].x = sampleVx(P);

                //y
                V = (v[idx(x, h, y)] + v[idx(x + 1, h, y)] + v[idx(x, h, y + 1)] + v[idx(x + 1, h, y + 1)]) * 0.25f;
                P = float3(x + 0.5f, h, y + 0.5f);
                P -= V * _dt / cfdSize;
                v2[idx(x, h, y)].y = sampleVy(P);

                //z
                V = (v[idx(x, h, y)] + v[idx(x, h, y + 1)] + v[idx(x, h + 1, y)] + v[idx(x, h + 1, y + 1)]) * 0.25f;
                P = float3(x + 0.5f, h + 0.5f, y);
                P -= V * _dt / cfdSize;
                v2[idx(x, h, y)].z = sampleVz(P);
            }
        }
    }

    memcpy(v.data(), v2.data(), cfdW * cfdW * cfdH * sizeof(float3));
}

float _cfd::sampleVx(float3 _p)
{
    int3 i = _p;
    float3 fract = _p - (float3)i;

    float a = glm::lerp(v[idx(i.x, i.y, i.z)].x, v[idx(i.x + 1, i.y, i.z)].x, fract.x);
    float b = glm::lerp(v[idx(i.x, i.y, i.z + 1)].x, v[idx(i.x + 1, i.y, i.z + 1)].x, fract.x);
    float bottom = glm::lerp(a, b, fract.z);

    a = glm::lerp(v[idx(i.x, i.y + 1, i.z)].x, v[idx(i.x + 1, i.y + 1, i.z)].x, fract.x);
    b = glm::lerp(v[idx(i.x, i.y + 1, i.z + 1)].x, v[idx(i.x + 1, i.y + 1, i.z + 1)].x, fract.x);
    float top = glm::lerp(a, b, fract.z);

    return glm::lerp(bottom, top, fract.y);
}

float _cfd::sampleVy(float3 _p)
{
    int3 i = _p;
    float3 fract = _p - (float3)i;

    float a = glm::lerp(v[idx(i.x, i.y, i.z)].y, v[idx(i.x + 1, i.y, i.z)].y, fract.x);
    float b = glm::lerp(v[idx(i.x, i.y, i.z + 1)].y, v[idx(i.x + 1, i.y, i.z + 1)].y, fract.x);
    float bottom = glm::lerp(a, b, fract.z);

    a = glm::lerp(v[idx(i.x, i.y + 1, i.z)].y, v[idx(i.x + 1, i.y + 1, i.z)].y, fract.x);
    b = glm::lerp(v[idx(i.x, i.y + 1, i.z + 1)].y, v[idx(i.x + 1, i.y + 1, i.z + 1)].y, fract.x);
    float top = glm::lerp(a, b, fract.z);

    return glm::lerp(bottom, top, fract.y);
}

float _cfd::sampleVz(float3 _p)
{
    int3 i = _p;
    float3 fract = _p - (float3)i;

    float a = glm::lerp(v[idx(i.x, i.y, i.z)].z, v[idx(i.x + 1, i.y, i.z)].z, fract.x);
    float b = glm::lerp(v[idx(i.x, i.y, i.z + 1)].z, v[idx(i.x + 1, i.y, i.z + 1)].z, fract.x);
    float bottom = glm::lerp(a, b, fract.z);

    a = glm::lerp(v[idx(i.x, i.y + 1, i.z)].z, v[idx(i.x + 1, i.y + 1, i.z)].z, fract.x);
    b = glm::lerp(v[idx(i.x, i.y + 1, i.z + 1)].z, v[idx(i.x + 1, i.y + 1, i.z + 1)].z, fract.x);
    float top = glm::lerp(a, b, fract.z);

    return glm::lerp(bottom, top, fract.y);
}







// without pressure for now - SPEED ;-)
void _cfd::solveIncompressibilityCenter(uint _num)
{
    float3 smin, smax;
    for (uint n = 0; n < _num; n++)
    {
        for (uint h = 1; h < 120; h++) {
            for (uint y = 5; y < 1020; y++) {
                for (uint x = 5; x < 1020; x++)
                {
                    uint i = idx(x, h, y);
                    smin = s[i];
                    smax.x = s[idx(x + 1, h, y)].x;
                    smax.y = s[idx(x, h + 1, y)].y;
                    smax.z = s[idx(x, h, y + 1)].z;
                    float sum = smin.x + smin.y + smin.z + smax.x + smax.y + smax.z;
                    if (sum > 0.00001f)
                    {
                        // not sdure I like this but lets see
                        // isntivntively WROGN scale with S to gel flow rate
                        /*
                        float vx0 = smin.x * v[idx(x - 1, h, y)].x;
                        float vx1 = smax.x * v[idx(x + 1, h, y)].x;
                        float vy0 = smin.y * v[idx(x, h - 1, y)].y;
                        float vy1 = smax.y * v[idx(x, h + 1, y)].y;
                        float vz0 = smin.z * v[idx(x, h, y - 1)].z;
                        float vz1 = smax.z * v[idx(x, h, y + 1)].z;
                        */
                        // odd tweede weergawe werk, nie seker hoekom nie
                        float vx0 = v[idx(x - 1, h, y)].x;
                        float vx1 = v[idx(x + 1, h, y)].x;

                        float vy0 = v[idx(x, h - 1, y)].y;
                        float vy1 = v[idx(x, h + 1, y)].y;

                        float vz0 = v[idx(x, h, y - 1)].z;
                        float vz1 = v[idx(x, h, y + 1)].z;

                        float div = vx0 - vx1 + vy0 - vy1 + vz0 - vz1;
                        float P = div / sum * 1.9f / 2;    // overrelaxed

                        // gauss seidel verwag oorskryf van waardes maar ons propagate in een rigting
                        // random or black / red
                        v[idx(x - 1, h, y)].x -= smin.x * P;
                        v[idx(x + 1, h, y)].x += smax.x * P;

                        v[idx(x, h - 1, y)].y -= smin.y * P;
                        v[idx(x, h + 1, y)].y += smax.y * P;

                        v[idx(x, h, y - 1)].z -= smin.z * P;
                        v[idx(x, h, y + 1)].z += smax.z * P;
                    }
                }
            }
        }
    }
}


void _cfd::advectCenter(float _dt)
{
    float3 V, P;

    //    for (uint h = 0; h < cfdH - 1; h++) {
    //        for (uint y = 2; y < cfdW - 2; y++) {
    //            for (uint x = 2; x < cfdW - 2; x++) {
    for (uint h = 0; h < 120; h++) {
        for (uint y = 5; y < 1020; y++) {
            for (uint x = 5; x < 1020; x++) {
                uint i = idx(x, h, y);
                P = float3(x, h, y) - v[i] / cfdSize * _dt;
                v2[i] = sample(P);                // we can alwasy do more smaller steps for betet acuracy, migth work great
            }
        }
    }

    // copy back
    for (uint h = 0; h < 120; h++) {
        for (uint y = 5; y < 1020; y++) {
            for (uint x = 5; x < 1020; x++) {
                uint i = idx(x, h, y);
                v[i] = v2[i];
            }
        }
    }

    //memcpy(v.data(), v2.data(), cfdW * cfdW * cfdH * sizeof(float3));
}


float3 _cfd::sample(float3 _p)
{
    _p.y = __max(_p.y, 0);


    int3 i = _p;
    float3 fract = _p - (float3)i;



    float3 a = glm::lerp(v[idx(i.x, i.y, i.z)], v[idx(i.x + 1, i.y, i.z)], fract.x);
    float3 b = glm::lerp(v[idx(i.x, i.y, i.z + 1)], v[idx(i.x + 1, i.y, i.z + 1)], fract.x);
    float3 bottom = glm::lerp(a, b, fract.z);

    a = glm::lerp(v[idx(i.x, i.y + 1, i.z)], v[idx(i.x + 1, i.y + 1, i.z)], fract.x);
    b = glm::lerp(v[idx(i.x, i.y + 1, i.z + 1)], v[idx(i.x + 1, i.y + 1, i.z + 1)], fract.x);
    float3 top = glm::lerp(a, b, fract.z);

    return glm::lerp(bottom, top, fract.y);
}

void _cfd::simulate(float _dt)
{
    //gavity(_dt);
    //gavity(0.01f);// this is messing me up
    solveIncompressibilityCenter(5); // Mathias uses 100, excessive for this slow air, as LOW as possible, look at feedback
    //edges();
    advectCenter(_dt);        // just do smkoe temperature etc as well
    tickCount++;
}


void _cfd::exportStreamlines(std::string filename, uint3 _p)
{
    std::ofstream ofs;
    ofs.open(filename, std::ios::binary);
    if (ofs)
    {
        for (int h = 0; h < 10; h++)
        {
            for (int y = 0; y < 20; y++)
            {
                float3 P = (float3)_p;
                float4 P_save;
                P.y += h * 5;
                P.z += y * 8;

                for (int j = 0; j < 100; j++)
                {
                    float3 V = sample(P);

                    P_save.xyz = P * cfdSize + float3(-20000, cfdMinAlt, -20000);
                    P_save.w = glm::length(V);
                    ofs.write((char*)&P_save, sizeof(float4));

                    P += V * 30.f / cfdSize;
                }

            }
        }


        ofs.close();
    }

}




































void _cfd_Smap::init(uint _w)
{
    width = _w;
    map.resize(_w * _w);
    S.resize(0);

    normals.resize(_w * _w);
}

void _cfd_Smap::saveNormals(std::string filename)
{
    std::ofstream ofs;
    ofs.open(filename, std::ios::binary);
    if (ofs)
    {
        uint msize = width * width;
        ofs.write((char*)&msize, sizeof(uint));
        ofs.write((char*)normals.data(), msize * sizeof(float4));
        ofs.close();
    }
}

void _cfd_Smap::save(std::string filename)
{
    std::ofstream ofs;
    ofs.open(filename, std::ios::binary);
    if (ofs)
    {
        uint msize = width * width;
        uint ssize = S.size();
        ofs.write((char*)&msize, sizeof(uint));
        ofs.write((char*)&ssize, sizeof(uint));
        ofs.write((char*)map.data(), msize * sizeof(_cfd_map));
        ofs.write((char*)S.data(), ssize * sizeof(glm::u8vec4));
        ofs.close();
    }
}

void _cfd_Smap::load(std::string filename)
{
    std::ifstream ifs;
    ifs.open(filename, std::ios::binary);
    if (ifs)
    {
        uint msize;
        uint ssize;
        ifs.read((char*)&msize, sizeof(uint));
        ifs.read((char*)&ssize, sizeof(uint));
        map.resize(msize);
        S.resize(ssize);
        ifs.read((char*)map.data(), msize * sizeof(_cfd_map));
        ifs.read((char*)S.data(), ssize * sizeof(glm::u8vec4));
        ifs.close();
    }
}

glm::u8vec4 _cfd_Smap::getS(uint3 _p)
{
    const auto& m = map[_p.z * width + _p.x];
    if (_p.y < m.bottom) return solid;                   // solid ground
    if (_p.y >= m.bottom + m.size) return freeair;        // free air
    return S[m.offset + (_p.y - m.bottom)];              // somewhere between the two
}


void _cfd_Smap::mipDown(_cfd_Smap& top_map)
{
    uint SIZE = width;
    uint S_TOP = width * 2;
    const auto& M = top_map.map;
    uint offset = 0;
    //map.resize(width * width);
    S.resize(0);

    for (uint z = 0; z < SIZE; z++)
    {
        for (uint x = 0; x < SIZE; x++)
        {
            uint i0 = z * SIZE + x;
            uint idx = z * 2 * (S_TOP)+(x * 2);
            uint bottom = __min(__min(M[idx].bottom, M[idx + 1].bottom),
                __min(M[idx + S_TOP].bottom, M[idx + S_TOP + 1].bottom));
            uint top = __max(__max(M[idx].bottom + M[idx].size,
                M[idx + 1].bottom + M[idx + 1].size),
                __max(M[idx + S_TOP].bottom + M[idx + S_TOP].size,
                    M[idx + S_TOP + 1].bottom + M[idx + S_TOP + 1].size));

            map[i0].bottom = bottom >> 1;
            map[i0].size = (top - bottom + 1) >> 1;
            map[i0].offset = offset;

            // normals
            normals[i0] = (top_map.normals[idx] + top_map.normals[idx + 1] + top_map.normals[idx + S_TOP] + top_map.normals[idx + S_TOP + 1]) * 0.25f;
            normals[i0].xyz = glm::normalize((float3)normals[i0].xyz);
            normals[i0].w /= 2.f;

            for (int j = 0; j < map[i0].size; j++)
            {
                uint4 sfloat = { 0, 0, 0, 0 };
                glm::u8vec4 s[8];
                uint h0 = (map[i0].bottom + j) * 2;
                uint h1 = h0 + 1;
                uint x0 = x * 2;
                uint x1 = x0 + 1;
                uint z0 = z * 2;
                uint z1 = z0 + 1;
                s[0] = top_map.getS(uint3(x0, h0, z0));
                s[1] = top_map.getS(uint3(x1, h0, z0));
                s[2] = top_map.getS(uint3(x1, h0, z1));
                s[3] = top_map.getS(uint3(x0, h0, z1));

                s[4] = top_map.getS(uint3(x0, h1, z0));
                s[5] = top_map.getS(uint3(x1, h1, z0));
                s[6] = top_map.getS(uint3(x1, h1, z1));
                s[7] = top_map.getS(uint3(x0, h1, z1));

                sfloat.x = s[0].x + s[3].x + s[4].x + s[7].x;
                sfloat.y = s[0].y + s[1].y + s[2].y + s[3].y;
                sfloat.z = s[0].z + s[1].z + s[4].z + s[5].z;
                sfloat.w = s[0].w + s[1].w + s[2].w + s[3].w + s[4].w + s[5].w + s[6].w + s[7].w;

                glm::u8vec4 Sval;
                Sval.x = sfloat.x >> 2;
                Sval.y = sfloat.y >> 2;
                Sval.z = sfloat.z >> 2;
                Sval.w = sfloat.w / 8;    // volume
                S.push_back(Sval);
                offset++;
            }
        }
    }
}









void _cfd_lod::init(uint _w, uint _h, float _worldSize)
{
    width = _w;
    w2 = _w * _w;
    height = _h;

    cellSize = _worldSize / _w;
    oneOverSize = 1.f / cellSize;

    v.resize(_h * _w * _w);
    v2.resize(_h * _w * _w);

    curl.resize(_h * _w * _w);
    mag.resize(_h * _w * _w);
    vorticity.resize(_h * _w * _w);

}

void _cfd_lod::loadNormals(std::string filename)
{
    std::ifstream ifs;
    ifs.open(filename, std::ios::binary);
    if (ifs)
    {
        uint msize;
        ifs.read((char*)&msize, sizeof(uint));
        normals.resize(msize);
        ifs.read((char*)normals.data(), msize * sizeof(float4));
        ifs.close();
    }
}

void _cfd_lod::setV(uint3 _p, float3 _v)
{
    //glm::u8vec4 s = smap.getS(_p);
    //v[idx(_p)] = _v * ((float)s.x / 255.f);
    //v2[idx(_p)] = _v * ((float)s.x / 255.f);

    v[idx(_p)] = _v;
    v2[idx(_p)] = _v;
}





// lodding
void _cfd_lod::fromRoot()
{
    if (!root) return;

    to_Root = offset;
    to_Root *= 0.5f;
    to_Root -= root->offset;

    for (uint h = 0; h < height; h++) {
        for (uint z = 0; z < width; z++) {
            for (uint x = 0; x < width; x++)
            {
                //glm::u8vec4 s = smap.getS(uint3(x, h, z) + offset);
                //v[idx(x, h, z)] = root->sample(float3(x, h, z) * 0.5f + to_Root) * ((float)s.y / 255.f);
                v[idx(x, h, z)] = root->sample(float3(x, h, z) * 0.5f + to_Root);
            }
        }
    }
}


void _cfd_lod::fromRoot_Alpha()
{
    if (!root) return;

    to_Root = offset;
    to_Root *= 0.5f;
    to_Root -= root->offset;

    for (uint h = 0; h < height; h++) {
        for (uint z = 0; z < width; z++) {
            for (uint x = 0; x < width; x++)
            {
                float alpha = __min(x / 10.f, z / 10.f);
                alpha = __min(alpha, (width - x - 1) / 10.f);
                alpha = __min(alpha, (width - z - 1) / 10.f);
                alpha = __min(1.f, alpha);
                alpha = __max(0.f, alpha);

                glm::u8vec4 s = smap.getS(uint3(x, h, z) + offset);
                float3 rootV = root->sample(float3(x, h, z) * 0.5f + to_Root) * ((float)s.y / 255.f);
                v[idx(x, h, z)] = glm::lerp(v[idx(x, h, z)], rootV, 1.f - alpha);
            }
        }
    }
}

void _cfd_lod::export_V(std::string filename)
{
    std::ofstream ofs;
    ofs.open(filename, std::ios::binary);
    if (ofs)
    {
        ofs.write((char*)v.data(), width * width * height * sizeof(float3));
        ofs.close();
    }
}


bool _cfd_lod::import_V(std::string filename)
{
    std::ifstream ifs;
    ifs.open(filename, std::ios::binary);
    if (ifs)
    {
        ifs.read((char*)v.data(), width * width * height * sizeof(float3));
        ifs.close();
        return true;
    }
    return false;
}



//#pragma optimize("", off)
void _cfd_lod::toRoot_Alpha()
{
    return;
    if (!root) return;
    int3 to = offset;
    to /= 2;
    to -= root->offset;

    uint w2 = (width >> 1) - 1;

    for (uint h = 0; h < height >> 1; h++) {
        for (uint z = 0; z <= w2; z++) {
            for (uint x = 0; x <= w2; x++)
            {
                float alpha = __min(x / 10.f, z / 10.f);
                alpha = __min(alpha, (w2 - x) / 10.f);
                alpha = __min(alpha, (w2 - z) / 10.f);
                alpha = __min(1.f, alpha);
                alpha = __max(0.f, alpha);

                float3 p = { x, h, z };
                p += 0.5f;
                float3 topV = sample(float3(x, h, z) * 2.f + 0.5f);
                uint i = idx(int3(x, h, z) + to);
                root->v[i] = glm::lerp(root->v[i], topV, alpha);
            }
        }
    }

    root->incompressibilityNormal(10);
}



float3 _cfd_lod::fromRoot(float3 _r)
{
}

float3 _cfd_lod::toRoot(float3 _r)
{
    //_r = (_r + offset) * 0.5f - root->offset;
   // _r = _r * 0.5f + offset * 0.5ff - root->offset;

}


// also bottom if origin no 0
void _cfd_lod::fromRootEdges()
{
    if (!root) return;

    to_Root = offset;
    to_Root *= 0.5f;
    to_Root -= root->offset;

    for (int h = 0; h < height; h++)
    {
        for (int i = 0; i < width; i++)
        {
            glm::u8vec4 s = smap.getS(uint3(0, h, i) + offset);
            v[idx(0, h, i)] = root->sample(float3(0, h, i) * 0.5f + to_Root) * ((float)s.y / 255.f);

            s = smap.getS(uint3(width - 1, h, i) + offset);
            v[idx(width - 1, h, i)] = root->sample(float3(width - 1, h, i) * 0.5f + to_Root) * ((float)s.y / 255.f);

            s = smap.getS(uint3(i, h, 0) + offset);
            v[idx(i, h, 0)] = root->sample(float3(i, h, 0) * 0.5f + to_Root) * ((float)s.y / 255.f);

            s = smap.getS(uint3(i, h, width - 1) + offset);
            v[idx(i, h, width - 1)] = root->sample(float3(i, h, width - 1) * 0.5f + to_Root) * ((float)s.y / 255.f);

            if (offset.y > 0)
            {
            }
        }
    }
}


//#pragma optimize("", off)
void _cfd_lod::toRoot()
{
    if (!root) return;
    int3 to = offset;
    to /= 2;
    to -= root->offset;

    for (uint h = 0; h < height >> 1; h++) {
        for (uint z = 0; z < width >> 1; z++) {
            for (uint x = 0; x < width >> 1; x++)
            {
                float3 p = { x, h, z };
                p += 0.5f;
                root->v[idx(int3(x, h, z) + to)] = sample(float3(x, h, z) * 2.f + 0.5f);
            }
        }
    }

    root->incompressibilityNormal(10);
}
//#pragma optimize("", on)





// simulate
void _cfd_lod::incompressibility(uint _num)
{
    glm::u8vec4 smin, smax;
    float3 v0, v1;
    uint3 i0, i1;

    for (uint n = 0; n < _num; n++)
    {
        for (uint h = 0; h < height - 1; h++) {
            for (uint z = 0; z < width - 1; z++) {
                for (uint x = 0; x < width - 1; x++) {
                    smin = smap.getS(uint3(x, h, z) + offset);
                    smax.x = smap.getS(uint3(x + 1, h, z) + offset).x;
                    smax.y = smap.getS(uint3(x, h + 1, z) + offset).y;
                    smax.z = smap.getS(uint3(x, h, z + 1) + offset).z;
                    uint sum = smin.x + smin.y + smin.z + smax.x + smax.y + smax.z;


                    if (width == 256)
                    {
                        bool cm = true;
                    }
                    // this gets better if we split V and move to boundaries I think
                    // really look at it
                    if (sum > 1)
                    {
                        i0.x = idx(x - 0, h, z);
                        i1.x = idx(x + 1, h, z);

                        i0.y = idx(x, h - 0, z);
                        i1.y = idx(x, h + 1, z);

                        i0.z = idx(x, h, z - 0);
                        i1.z = idx(x, h, z + 1);

                        v0.x = v[i0.x].x;
                        v1.x = v[i1.x].x;

                        v0.y = v[i0.y].y;
                        v1.y = v[i1.y].y;

                        v0.z = v[i0.z].z;
                        v1.z = v[i1.z].z;

                        float3 div = v0 - v1;

                        float P = (div.x + div.y + div.z) / sum * 1.9f;    // overrelaxed ??? 2

                        // gauss seidel verwag oorskryf van waardes maat ons propagate in een rigting
                        // black red
                        v[i0.x].x -= smin.x * P;
                        v[i1.x].x += smax.x * P;

                        //Play here with resitance to heigth chnage
                        // mgh vs 0.5 mv^2
                        v[i0.y].y -= smin.y * P;
                        v[i1.y].y += smax.y * P;

                        v[i0.z].z -= smin.z * P;
                        v[i1.z].z += smax.z * P;
                    }
                }
            }
        }
    }
};

#pragma optimize("", off)
void _cfd_lod::Normal()
{

    for (uint z = 0; z < width - 1; z++) {
        for (uint x = 0; x < width - 1; x++)
        {
            uint nIndex = (z + offset.z) * smap.width + (x + offset.x);

            float3 N = normals[nIndex].xyz;
            float Hcell = 0.5f  - normals[nIndex].w;

            for (uint h = 0; h < height - 1; h++)
            {
                float scale = 1.f - __min(1.f, abs(Hcell) / 2.f);
                
                if (scale > 0)
                {
                    uint i = idx(x, h, z);
                    float3 normWind = N * glm::dot(v[i], N);
                    v[i] -= normWind * scale;
                }
                //if (Hcell < 0) v[idx(x, h, z)] *= 0.f;
                Hcell += 1.f;
            }
        }
    }
}
#pragma optimize("", on)

// Stepping heigthlast will simplify normal chacking, almos means packing height last will simplify memory
void _cfd_lod::incompressibilityNormal(uint _num)
{
    float3 v0, v1;
    uint3 i0, i1;

    for (uint n = 0; n < _num; n++)
    {
        Normal();

        for (uint h = 0; h < height - 1; h++) {
            for (uint z = 0; z < width - 1; z++) {
                for (uint x = 0; x < width - 1; x++)
                {
                    uint i = idx(x, h, z);
                    uint iX = idx(x + 1, h, z);
                    uint iY = idx(x, h + 1, z);
                    uint iZ = idx(x, h, z + 1);

                    v0 = v[i];
                    v1.x = v[iX].x;
                    v1.y = v[iY].y;
                    v1.z = v[iZ].z;

                    float3 div = v0 - v1;
                    float P = (div.x + div.y + div.z) * 1.9f * 0.1666666667;    // overrelaxed ???

                    // doen black red ...   gauss seidel verwag oorskryf van waardes maat ons propagate in een rigting
                    v[i] -= P;
                    v[iX].x += P;
                    v[iY].y += P;
                    v[iZ].z += P;
                }
            }
        }
    }
};

void _cfd_lod::edges()
{
    return;
    for (int h = 0; h < height; h++)
    {
        for (int i = 0; i < width; i++)
        {

            // 2 deep
            v[idx(1, h, i)] = v[idx(2, h, i)]; // left
            v[idx(0, h, i)] = v[idx(2, h, i)];

            v[idx(width - 1, h, i)] = v[idx(width - 3, h, i)];  // rigth
            v[idx(width - 2, h, i)] = v[idx(width - 3, h, i)];

            v[idx(i, h, 0)] = v[idx(i, h, 2)]; // zmin
            v[idx(i, h, 1)] = v[idx(i, h, 2)];

            v[idx(i, h, width - 1)] = v[idx(i, h, width - 3)];  // zmax
            v[idx(i, h, width - 2)] = v[idx(i, h, width - 3)];

            /*
            v[idx(0, h, i)] = v[idx(1, h, i)]; // left
            v[idx(width - 1, h, i)] = v[idx(width - 2, h, i)];  // rigth
            v[idx(i, h, 0)] = v[idx(1, h, i)]; // zmin
            v[idx(i, h, width - 1)] = v[idx(width - 2, h, i)];  // zmax
            */
        }
    }
};


// we can always do more smaller steps for better acuracy, migth work great
void _cfd_lod::advect(float _dt)
{
    float3 P;
    float3 ox = { 0, 0.5f, 0.5f };
    float3 oy = { 0.5f, 0, 0.5f };
    float3 oz = { 0.5f, 0.5f, 0 };
    float scale = oneOverSize * _dt;

    for (uint h = 0; h < height - 1; h++) {
        for (uint z = 2; z < width - 2; z++) {
            for (uint x = 2; x < width - 2; x++)
            {
                uint i = idx(x, h, z);
                P = float3(x, h, z);
                /*
                float3 vX = { sample_x(P + ox), sample_y(P + ox), sample_z(P + ox) };
                float3 vY = { sample_x(P + oy), sample_y(P + oy), sample_z(P + oy) };
                float3 vZ = { sample_x(P + oz), sample_y(P + oz), sample_z(P + oz) };

                v2[i].x = sample_x(P + ox - (vX * scale));
                v2[i].y = sample_y(P + oy - (vY * scale));
                v2[i].z = sample_z(P + oz - (vZ * scale));
                */

                glm::u8vec4 smin = smap.getS(uint3(x, h, z) + offset);
                float3 V = v[idx(x, h, z)];
                v2[i] = sample(P - V * scale) * (smin.y / 255.f);
            }
        }
    }

    memcpy(v.data(), v2.data(), width * width * height * sizeof(float3));
};


void _cfd_lod::vorticty_confine(float _dt)
{

    // curl from velocity
    for (uint h = 0; h < height - 1; h++) {
        for (uint z = 0; z < width - 1; z++) {
            for (uint x = 0; x < width - 1; x++)
            {
                uint i = idx(x, h, z);
                float xx_c = v[idx(x, h + 1, z)].z - v[i].z - v[idx(x, h, z + 1)].y + v[i].y;
                float yy_c = v[idx(x, h, z + 1)].x - v[i].x - v[idx(x + 1, h, z)].z + v[i].z;
                float zz_c = v[idx(x + 1, h, z)].y - v[i].y - v[idx(x, h + 1, z)].x + v[i].x;
                curl[i] = { xx_c, yy_c, zz_c };
                curl[i] *= oneOverSize;
                mag[i] = glm::length(curl[i]);
            }
        }
    }

    // -2 because last mag does not exists, or uplicater mag
    // Calculate Vorticty Direction From Curl Length Gradient, Cross and Apply Vortex Confinement Force -
    for (uint h = 0; h < height - 2; h++) {
        for (uint z = 0; z < width - 2; z++) {
            for (uint x = 0; x < width - 2; x++)
            {
                uint i = idx(x, h, z);
                float3 gradient = { mag[idx(x + 1, h, z)] , mag[idx(x, h + 1, z)] , mag[idx(x, h, z + 1)] };  // gradient
                gradient -= mag[i];
                gradient *= oneOverSize;
                gradient += 0.000001f;
                vorticity[i] = glm::normalize(gradient);

                float3 f = glm::cross(vorticity[i], curl[i]) * oneOverSize;
                glm::u8vec4 smin = smap.getS(uint3(x, h, z) + offset);
                v[i] -= 1.f * f * _dt;// *(smin.y / 255.f);
            }
        }
    }
}



float3 _cfd_lod::sample(float3 _p)
{
    clamp(_p);
    int3 i = _p;
    float3 f1 = _p - (float3)i;
    float3 f0 = float3(1, 1, 1) - f1;

    float3 a = glm::lerp(v[idx(i.x, i.y, i.z)], v[idx(i.x + 1, i.y, i.z)], f1.x);
    float3 b = glm::lerp(v[idx(i.x, i.y, i.z + 1)], v[idx(i.x + 1, i.y, i.z + 1)], f1.x);
    float3 bottom = glm::lerp(a, b, f1.z);

    a = glm::lerp(v[idx(i.x, i.y + 1, i.z)], v[idx(i.x + 1, i.y + 1, i.z)], f1.x);
    b = glm::lerp(v[idx(i.x, i.y + 1, i.z + 1)], v[idx(i.x + 1, i.y + 1, i.z + 1)], f1.x);
    float3 top = glm::lerp(a, b, f1.z);

    return glm::lerp(bottom, top, f1.y);
};


float3 _cfd_lod::sampleCurl(float3 _p)
{
    clamp(_p);
    int3 i = _p;
    float3 f1 = _p - (float3)i;
    float3 f0 = float3(1, 1, 1) - f1;

    float3 a = glm::lerp(curl[idx(i.x, i.y, i.z)], curl[idx(i.x + 1, i.y, i.z)], f1.x);
    float3 b = glm::lerp(curl[idx(i.x, i.y, i.z + 1)], curl[idx(i.x + 1, i.y, i.z + 1)], f1.x);
    float3 bottom = glm::lerp(a, b, f1.z);

    a = glm::lerp(curl[idx(i.x, i.y + 1, i.z)], curl[idx(i.x + 1, i.y + 1, i.z)], f1.x);
    b = glm::lerp(curl[idx(i.x, i.y + 1, i.z + 1)], curl[idx(i.x + 1, i.y + 1, i.z + 1)], f1.x);
    float3 top = glm::lerp(a, b, f1.z);

    return glm::lerp(bottom, top, f1.y);
};


// split to faces
void _cfd_lod::clamp(float3& _p)
{
    _p.x = glm::clamp(_p.x, 0.f, (float)(width - 2));       // wrong reference in palce and inline
    _p.y = glm::clamp(_p.y, 0.f, (float)(height - 2));
    _p.z = glm::clamp(_p.z, 0.f, (float)(width - 2));
}

float _cfd_lod::sample_x(float3 _p)
{
    clamp(_p);
    int3 i = _p;
    float3 f1 = _p - (float3)i;
    float3 f0 = float3(1, 1, 1) - f1;

    float Vx = 0;
    Vx += (f0.y) * (f0.z) * (f0.x) * v[idx(i.x, i.y, i.z)].x;
    Vx += (f0.y) * (f0.z) * (f1.x) * v[idx(i.x + 1, i.y, i.z)].x;
    Vx += (f0.y) * (f1.z) * (f1.x) * v[idx(i.x + 1, i.y, i.z + 1)].x;
    Vx += (f0.y) * (f1.z) * (f0.x) * v[idx(i.x, i.y, i.z + 1)].x;

    Vx += (f1.y) * (f0.z) * (f0.x) * v[idx(i.x, i.y + 1, i.z)].x;
    Vx += (f1.y) * (f0.z) * (f1.x) * v[idx(i.x + 1, i.y + 1, i.z)].x;
    Vx += (f1.y) * (f1.z) * (f1.x) * v[idx(i.x + 1, i.y + 1, i.z + 1)].x;
    Vx += (f1.y) * (f1.z) * (f0.x) * v[idx(i.x, i.y + 1, i.z + 1)].x;

    return Vx;
}

float _cfd_lod::sample_y(float3 _p)
{
    clamp(_p);
    int3 i = _p;
    float3 f1 = _p - (float3)i;
    float3 f0 = float3(1, 1, 1) - f1;

    float Vy = 0;
    Vy += (f0.y) * (f0.z) * (f0.x) * v[idx(i.x, i.y, i.z)].y;
    Vy += (f0.y) * (f0.z) * (f1.x) * v[idx(i.x + 1, i.y, i.z)].y;
    Vy += (f0.y) * (f1.z) * (f1.x) * v[idx(i.x + 1, i.y, i.z + 1)].y;
    Vy += (f0.y) * (f1.z) * (f0.x) * v[idx(i.x, i.y, i.z + 1)].y;

    Vy += (f1.y) * (f0.z) * (f0.x) * v[idx(i.x, i.y + 1, i.z)].y;
    Vy += (f1.y) * (f0.z) * (f1.x) * v[idx(i.x + 1, i.y + 1, i.z)].y;
    Vy += (f1.y) * (f1.z) * (f1.x) * v[idx(i.x + 1, i.y + 1, i.z + 1)].y;
    Vy += (f1.y) * (f1.z) * (f0.x) * v[idx(i.x, i.y + 1, i.z + 1)].y;

    return Vy;
}

float _cfd_lod::sample_z(float3 _p)
{
    clamp(_p);
    int3 i = _p;
    float3 f1 = _p - (float3)i;
    float3 f0 = float3(1, 1, 1) - f1;

    float Vz = 0;
    Vz += (f0.y) * (f0.z) * (f0.x) * v[idx(i.x, i.y, i.z)].z;
    Vz += (f0.y) * (f0.z) * (f1.x) * v[idx(i.x + 1, i.y, i.z)].z;
    Vz += (f0.y) * (f1.z) * (f1.x) * v[idx(i.x + 1, i.y, i.z + 1)].z;
    Vz += (f0.y) * (f1.z) * (f0.x) * v[idx(i.x, i.y, i.z + 1)].z;

    Vz += (f1.y) * (f0.z) * (f0.x) * v[idx(i.x, i.y + 1, i.z)].z;
    Vz += (f1.y) * (f0.z) * (f1.x) * v[idx(i.x + 1, i.y + 1, i.z)].z;
    Vz += (f1.y) * (f1.z) * (f1.x) * v[idx(i.x + 1, i.y + 1, i.z + 1)].z;
    Vz += (f1.y) * (f1.z) * (f0.x) * v[idx(i.x, i.y + 1, i.z + 1)].z;

    return Vz;
}

float3 _cfd_lod::sample_xyz(float3 _p)
{
    float3 ox = { 0, 0.5f, 0.5f };
    float3 oy = { 0.5f, 0, 0.5f };
    float3 oz = { 0.5f, 0.5f, 0 };

    float3 v = { sample_x(_p + ox), sample_y(_p + oy), sample_z(_p + oz) };

    return v;
}



void _cfd_lod::simulate(float _dt)
{
    vorticty_confine(_dt);
    incompressibilityNormal(10); // Mathias uses 100, excessive for this slow air, as LOW as possible, look at feedback
    edges();
    advect(_dt);        // just do smkoe temperature etc as well
    tickCount++;
    frameTime += _dt;
};


void _cfd_lod::streamlines(float3 _p, float4* _data)
{
    float rootAltitude = 350.f;   // in voxel space
    float3 origin = float3(-20000, rootAltitude, -20000);

    for (int h = 0; h < 10; h++)
    {
        for (int y = 0; y < 20; y++)
        {
            float3 P = (_p - origin) * oneOverSize;
            P.y += h * 1;
            P.z += y * 2;

            for (int j = 0; j < 100; j++)
            {
                float3 V = sample(P);

                _data->xyz = P * cellSize + origin;
                _data->w = glm::length(V);
                P += V * 0.4f;// *300.f * oneOverSize;
                _data++;
            }
        }
    }
}







void _cfdClipmap::build(std::string _path)
{
    // normals instead f smap
    lods[0].init(128, 32, 40000);
    lods[0].offset = { 0, 0, 0 };
    lods[0].smap.init(128);
    lods[0].smap.load(_path + "/S_128.bin");

    lods[1].init(128, 64, 40000);  // already clipped
    lods[1].offset = { 80, 0, 128 };
    lods[1].root = &lods[0];
    lods[1].smap.init(256);
    lods[1].smap.load(_path + "/S_256.bin");

    lods[2].init(128, 128, 40000);
    lods[2].offset = { 160 + 40, 0, 256 + 80 };
    lods[2].root = &lods[1];
    lods[2].smap.init(512);
    lods[2].smap.load(_path + "/S_512.bin");

    lods[3].init(128, 128, 40000);
    lods[3].offset = { (160 + 64) * 2 + 32, 0, (256 + 80) * 2 + 90 };
    lods[3].root = &lods[2];
    lods[3].smap.init(1024);
    lods[3].smap.load(_path + "/S_1024.bin");

    lods[4].init(128, 128, 40000);
    lods[4].offset = lods[3].offset + uint3(32, 0, 32);
    lods[4].offset *= 2;
    lods[4].root = &lods[3];
    lods[4].smap.init(2048);
    lods[4].smap.load(_path + "/S_2048.bin");

    lods[5].init(128, 128, 40000);
    lods[5].offset = lods[4].offset + uint3(32, 20, 32);
    lods[5].offset *= 2;
    lods[5].root = &lods[4];
    lods[5].smap.init(4096);
    lods[5].smap.load(_path + "/S_4096.bin");

    lods[0].loadNormals(_path + "/S_128.normals.bin");
    lods[1].loadNormals(_path + "/S_256.normals.bin");
    lods[2].loadNormals(_path + "/S_512.normals.bin");
    lods[3].loadNormals(_path + "/S_1024.normals.bin");
    lods[4].loadNormals(_path + "/S_2048.normals.bin");
    lods[5].loadNormals(_path + "/S_4096.normals.bin");

}


void _cfdClipmap::setWind(float3 _bottom, float3 _top)
{
    for (uint y = 0; y < 32; y++)
    {
        float3 wind = glm::lerp(_bottom, _top, (float)y / 32.f);
        for (uint z = 0; z < 128; z++)
        {
            for (uint x = 0; x < 128; x++)
            {
                lods[0].setV(uint3(x, y, z), wind);
            }
        }
    }
}


void _cfdClipmap::simulate_start(float _dt)
{
    for (int k = 0; k < 100; k++)
    {
        lods[0].simulate(_dt);
    }
    //lods[0].incompressibilityNormal(10);

    for (int lod = 0; lod < 6; lod++)
    {
        lods[lod].fromRoot();
        lods[lod].incompressibilityNormal(20);
        //lods[lod].edges();
        //for (int k = 0; k < 10; k++)         lods[lod].simulate(_dt / 2.f);
        //lods[lod].fromRootEdges();
        //lods[lod].toRoot();
    }
}


void _cfdClipmap::simulate(float _dt)
{
    for (int l = 1; l <= 6; l++)
    {
        uint start = (int)pow(2, l);
        uint step = start * 2;
        uint test = (counter - start) % step;
        if (test == 0)
        {
            uint lod = 6 - l;
            float time = _dt / pow(2, lod);
            //lods[lod].fromRootEdges();
            lods[lod].fromRoot_Alpha();
            lods[lod].simulate(time);
            //lods[lod].toRoot();
            lods[lod].toRoot_Alpha();
        }
    }

    counter++;
}

//#pragma optimize("", off)
void _cfdClipmap::streamlines(float3 _p, float4* _data)
{
    float rootAltitude = 350.f;   // in voxel space
    float3 origin = float3(-20000, rootAltitude, -20000);

    for (int h = 0; h < 10; h++)
    {
        for (int y = 0; y < 20; y++)
        {
            float3 P = (_p - origin) * lods[0].oneOverSize;
            P.y += h * 0.4;
            P.z += y * 0.5f;

            for (int j = 0; j < 100; j++)
            {
                float3 Psample = P;
                float L = 0;
                float scale = 1;
                _cfd_lod* pLOD = &lods[0];

                float3 p1 = P * 2.f;
                p1 -= lods[1].offset;

                float3 p2 = P * 4.f;
                p2 -= lods[2].offset;

                float3 p3 = P * 8.f;
                p3 -= lods[3].offset;

                float3 p4 = P * 16.f;
                p4 -= lods[4].offset;

                float3 p5 = P * 32.f;
                p5 -= lods[5].offset;

                if ((p5.x > 2 && p5.y > 0 && p5.z > 2) &&
                    (p5.x < 125 && p5.y < 125 && p5.z < 125))
                {
                    pLOD = &lods[5];
                    Psample = p5;
                    L = 3;
                    scale = 0.4f;
                }

                else if ((p4.x > 2 && p4.y > 0 && p4.z > 2) &&
                    (p4.x < 125 && p4.y < 125 && p4.z < 125))
                {
                    pLOD = &lods[4];
                    Psample = p4;
                    L = 0;
                    scale = 0.5f;
                }

                else if ((p3.x > 2 && p3.y > 0 && p3.z > 2) &&
                    (p3.x < 125 && p3.y < 125 && p3.z < 125))
                {
                    pLOD = &lods[3];
                    Psample = p3;
                    L = 5;
                    scale = 0.6f;
                }

                else if ((p2.x > 2 && p2.y > 0 && p2.z > 2) &&
                    (p2.x < 125 && p2.y < 125 && 125))
                {
                    pLOD = &lods[2];
                    Psample = p2;
                    L = 0;
                    scale = 0.7f;
                }
                else if ((p1.x > 2 && p1.y > 0 && p1.z > 2) && (p1.x < 125 && p1.y < 61 && p1.z < 125))
                {
                    pLOD = &lods[1];
                    Psample = p1;
                    L = 2;
                    scale = 0.8f;
                }


                //Psample = P;
                //scale = 1;
                //pLOD = &lods[0];


                pLOD->clamp(Psample);
                float3 V = pLOD->sample(Psample);

                _data->xyz = P * lods[0].cellSize + origin;
                _data->w = glm::length(V);
                float lv = glm::length(V);
                //V = glm::normalize(V) * 3.f;
                //if (lv < 0.01f) V = { 0, 0, 0 };
                P += V * 0.3f * scale;// *300.f * oneOverSize;

                float3 c = pLOD->sampleCurl(Psample);
                //_data->w = glm::length(c);

                _data++;
            }
        }
    }
}

void _cfdClipmap::export_V(std::string _name)
{
    for (int lod = 0; lod < 6; lod++)
    {
        lods[lod].export_V(_name + "__" + std::to_string(lod) + ".velocity_bin");
    }
}

bool _cfdClipmap::import_V(std::string _name)
{
    bool loaded = false;
    for (int lod = 0; lod < 6; lod++)
    {
        loaded |= lods[lod].import_V(_name + "__" + std::to_string(lod) + ".velocity_bin");
    }
    return loaded;
}










//#pragma optimize("", off)
void _cfdClipmap::heightToSmap(std::string filename)
{

    float scale4K = 40000.f / 4096.f;   // single voxel at 4k
    float oneOver = 4096.f / 40000.f;
    float rootAltitude = 350.f;   // in voxel space

    std::vector<float> hgt;
    hgt.resize(4096 * 4096);
    std::vector<float> corners;
    corners.resize(4097 * 4097);
    std::ifstream ifs;
    ifs.open(filename, std::ios::binary);
    if (ifs)
    {
        ifs.read((char*)hgt.data(), 4096 * 4096 * sizeof(float));
        ifs.close();
    }

    // scale, its faster
    for (uint z = 0; z < 4096; z++)
    {
        for (uint x = 0; x < 4096; x++)
        {
            hgt[z * 4096 + x] -= rootAltitude;
            hgt[z * 4096 + x] *= oneOver;
        }
    }

    // average to corners - repeat edge  BUG somehow look close
    for (int z = 0; z < 4097; z++)
    {
        int z0 = __min(4095, __max(0, z - 1));
        int z1 = __min(4095, __max(0, z));
        for (int x = 0; x < 4097; x++)
        {
            int x0 = __min(4095, __max(0, x - 1));
            int x1 = __min(4095, __max(0, x));
            corners[z * 4097 + x] = 0.25f * (hgt[z1 * 4096 + x1] + hgt[z1 * 4096 + x0] +
                hgt[z0 * 4096 + x1] + hgt[z0 * 4096 + x0]);
        }
    }

    _cfd_Smap smap;
    smap.init(4096);
    uint offset = 0;

    for (uint z = 0; z < 4096; z++)
    {
        for (uint x = 0; x < 4096; x++)
        {
            // scale into space
            //float a = hgt[z * 4096 + x];
            float b = corners[z * 4097 + x];
            float c = corners[z * 4097 + (x + 1)];
            float d = corners[(z + 1) * 4097 + (x + 1)];
            float e = corners[(z + 1) * 4097 + x];

            float fTop = __max(__max(b, c), __max(d, e));       // ignoe A for now, I have to export nice corner data 4097 for thsi
            float fBottom = __min(__min(b, c), __min(d, e));
            uint top = (uint)(fTop);
            uint bottom = (uint)(fBottom);
            uint size = 1 + (top - bottom);

            
            float3 vA = { 1 , d - b, 1 };
            float3 vB = { -1 , e - c, 1 };
            smap.normals[z * 4096 + x].xyz = glm::normalize(glm::cross(vB, vA));
            smap.normals[z * 4096 + x].w = hgt[z * 4096 + x];

            smap.map[z * 4096 + x].offset = offset;
            smap.map[z * 4096 + x].bottom = bottom;
            smap.map[z * 4096 + x].size = size;

            if (size > 5)
            {
                bool cm = true;
            }

            for (int j = 0; j < size; j++)
            {
                float h = (float)(bottom + j);
                float4 sfloat = { 0, 0, 0, 0 };

                // just integrate it for now my maths is failing me for side area rule

                for (float k = 0.05f; k < 1; k += 0.1f)
                {
                    float dX = glm::clamp(glm::lerp(b, e, k) - h, 0.f, 1.f);
                    sfloat.x += dX;
                    float dZ = glm::clamp(glm::lerp(b, c, k) - h, 0.f, 1.f);
                    sfloat.z += dZ;

                    float y0 = glm::lerp(b, c, k);
                    float y1 = glm::lerp(e, d, k);
                    for (float l = 0.05f; l < 1; l += 0.1f)
                    {
                        float y = glm::lerp(y0, y1, l);
                        if (y > h)
                        {
                            sfloat.y += 1;
                        }
                    }
                }

                glm::u8vec4 S;
                S.x = 255 - (uint)(sfloat.x * 25.5f);
                S.y = 255 - (uint)(sfloat.y * 2.55f);  // 100 samples
                S.z = 255 - (uint)(sfloat.z * 25.5f);
                // w zero for now for trees and blcokers in future ?
                smap.S.push_back(S);
                offset++;
            }
        }
    }


    // save smap
    smap.save(filename + "S_4096.bin");
    smap.saveNormals(filename + "S_4096.normals.bin");

    //mip down and save
    _cfd_Smap smap_2048;
    smap_2048.init(2048);
    smap_2048.mipDown(smap);
    smap_2048.save(filename + "S_2048.bin");
    smap_2048.saveNormals(filename + "S_2048.normals.bin");

    _cfd_Smap smap_1024;
    smap_1024.init(1024);
    smap_1024.mipDown(smap_2048);
    smap_1024.save(filename + "S_1024.bin");
    smap_1024.saveNormals(filename + "S_1024.normals.bin");

    _cfd_Smap smap_512;
    smap_512.init(512);
    smap_512.mipDown(smap_1024);
    smap_512.save(filename + "S_512.bin");
    smap_512.saveNormals(filename + "S_512.normals.bin");

    _cfd_Smap smap_256;
    smap_256.init(256);
    smap_256.mipDown(smap_512);
    smap_256.save(filename + "S_256.bin");
    smap_256.saveNormals(filename + "S_256.normals.bin");

    _cfd_Smap smap_128;
    smap_128.init(128);
    smap_128.mipDown(smap_256);
    smap_128.save(filename + "S_128.bin");
    smap_128.saveNormals(filename + "S_128.normals.bin");

    // do so me test images
    std::ofstream ofs;
    ofs.open(filename + "4096_Cut.raw", std::ios::binary);
    if (ofs)
    {
        for (int y = 212; y >= 0; y--)
        {
            for (int x = 0; x < 4096; x++)
            {
                glm::u8vec4 s = smap.getS(uint3(x, y, 109 * 32));
                ofs.write((char*)&s.z, 1);
            }
        }
        ofs.close();
    }

    ofs.open(filename + "2048_Cut.raw", std::ios::binary);
    if (ofs)
    {
        for (int y = 106; y >= 0; y--)
        {
            for (int x = 0; x < 2048; x++)
            {
                glm::u8vec4 s = smap_2048.getS(uint3(x, y, 109 * 16));
                ofs.write((char*)&s.z, 1);
            }
        }
        ofs.close();
    }

    ofs.open(filename + "1024_Cut.raw", std::ios::binary);
    if (ofs)
    {
        for (int y = 127; y >= 0; y--)
        {
            for (int x = 0; x < 1024; x++)
            {
                glm::u8vec4 s = smap_1024.getS(uint3(x, y, 109 * 8));
                ofs.write((char*)&s.z, 1);
            }
        }
        ofs.close();
    }


    ofs.open(filename + "512_Cut.raw", std::ios::binary);
    if (ofs)
    {
        for (int y = 63; y >= 0; y--)
        {
            for (int x = 0; x < 512; x++)
            {
                glm::u8vec4 s = smap_512.getS(uint3(x, y, 109 * 4));
                ofs.write((char*)&s.z, 1);
            }
        }
        ofs.close();
    }

    ofs.open(filename + "256_Cut.raw", std::ios::binary);
    if (ofs)
    {
        for (int y = 31; y >= 0; y--)
        {
            for (int x = 0; x < 256; x++)
            {
                glm::u8vec4 s = smap_256.getS(uint3(x, y, 109 * 2));
                ofs.write((char*)&s.z, 1);
            }
        }
        ofs.close();
    }

    ofs.open(filename + "128_Cut.raw", std::ios::binary);
    if (ofs)
    {
        for (int y = 15; y >= 0; y--)
        {
            for (int x = 0; x < 128; x++)
            {
                glm::u8vec4 s = smap_128.getS(uint3(x, y, 109));
                ofs.write((char*)&s.z, 1);
            }
        }
        ofs.close();
    }

    ofs.open(filename + "256_Normals.raw", std::ios::binary);
    if (ofs)
    {
        for (int y = 0; y < 256; y++)
        {
            for (int x = 0; x < 256; x++)
            {
                unsigned char val = (int)((smap_256.normals[y * 256 + x].x + 1.f) * 127);
                ofs.write((char*)&val, 1);
                val = (int)((smap_256.normals[y * 256 + x].z + 1.f) * 127);
                ofs.write((char*)&val, 1);
                val = (int)((smap_256.normals[y * 256 + x].y + 1.f) * 127);
                ofs.write((char*)&val, 1);
            }
        }
        ofs.close();
    }
}
