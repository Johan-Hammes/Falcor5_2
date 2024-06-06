#include "cfd.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <iostream>
#include <fstream>

using namespace std::chrono;
//#define GLM_FORCE_SSE2
//#define GLM_FORCE_ALIGNED


//#pragma optimize("", off)



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
                float alpha = __min(x / 8.f, z / 8.f);
                alpha = __min(alpha, (width - x - 1) / 8.f);
                alpha = __min(alpha, (width - z - 1) / 8.f);
                //bottom and top
                if (offset.y > 0) {
                    alpha = __min(alpha, h / 8.f);
                }
                alpha = __min(alpha, (height - 1 - h) / 8.f);

                alpha = __min(1.f, alpha);
                alpha = __max(0.f, alpha);

                float3 rootV = root->sample(float3(x, h, z) * 0.5f + to_Root);
                v[idx(x, h, z)] = glm::lerp(v[idx(x, h, z)], rootV, 1.f - alpha);
            }
        }
    }
}

void _cfd_lod::toRoot_Alpha()
{
/*
    if (!root) return;
    int3 to = offset;
    to /= 2;
    to -= root->offset;

    uint wA2 = (width >> 1) - 1;

    for (uint h = 0; h < root->height - 1; h++) {
        for (uint z = 0; z <= wA2; z++) {
            for (uint x = 0; x <= wA2; x++)
            {
                float alpha = __min(x / 8.f, z / 8.f);
                alpha = __min(alpha, (wA2 - x - 1) / 8.f);
                alpha = __min(alpha, (wA2 - z - 1) / 8.f);
                //bottom and top
                if (offset.y > 0) {
                    alpha = __min(alpha, h / 8.f);
                }
                alpha = __min(alpha, (root->height - 1 - h) / 8.f);

                alpha = __min(1.f, alpha);
                alpha = __max(0.f, alpha);

                float3 p = { x, h, z };
                //p += 0.5f;
                float3 topV = sample(float3(x, h, z) * 2.f + 0.5f);
                uint i = root->idx(int3(x, h, z) + to);
                root->v[i] = glm::lerp(root->v[i], topV, alpha);
            }
        }
    }
    */
}


/*  D only 8 pixels around the actual height - this is really fast, pretty much 2D
*/
void _cfd_lod::Normal()
{
    for (uint z = 0; z < width - 1; z++) {
        for (uint x = 0; x < width - 1; x++)
        {
            uint nIndex = (z + offset.z) * smap.width + (x + offset.x);

            float3 N = normals[nIndex].xyz;
            float Hcell = 0.5f - normals[nIndex].w - offset.y;
            int h0 = (int)(normals[nIndex].w - 4.f - offset.y);
            h0 = __min(height - 10, __max(0, h0));   // far enoufgh down that even fraction wont break it, we have
            Hcell += h0;

            for (uint h = h0; h < h0 + 8; h++)
            {
                float scale = __max(0, 1.f - __min(1.f, abs(Hcell) / 2.f));
                uint i = idx(x, h, z);
                float3 normWind = N * glm::dot(v[i], N);
                v[i] -= normWind * scale;

                v[i] *= 1.f - scale * 0.0001f;
                if (Hcell < -1) v[i] *= 0;
                Hcell += 1.f;
            }
        }
    }
}


// Stepping heigth last will simplify normal checking, also means packing height last will simplify memory
void _cfd_lod::incompressibilityNormal(uint _num)
{
    for (uint n = 0; n < _num; n++)
    {
        Normal();

        for (uint h = 0; h < height - 1; h++) {
            for (uint z = 0; z < width - 1; z++) {
                for (uint x = 0; x < width - 1; x++)
                {
                    uint i = idx(x, h, z);
                    float P = (v[i].x + v[i].y + v[i].z - v[i + 1].x - v[i + w2].y - v[i + width].z) * 0.316;

                    // doen black red ...   gauss seidel verwag oorskryf van waardes maar ons propagate in een rigting
                    v[i] -= P;
                    v[i + 1].x += P;
                    v[i + w2].y += P;
                    v[i + width].z += P;
                }
            }
        }
    }
};


void _cfd_lod::edges()      //?? 1 deep
{
    
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

            v[idx(i, h, width - 1)] = v[idx(i, h, width - 2)];  // zmax
            v[idx(i, h, width - 2)] = v[idx(i, h, width - 3)];
        }
    }

    for (int j = 0; j < width; j++)
    {
        for (int i = 0; i < width; i++)
        {
            if (offset.y > 0) {  // bottom
                v[idx(i, 0, j)] = v[idx(i, 2, j)]; // left
                v[idx(i, 1, j)] = v[idx(i, 2, j)];
            }

            v[idx(i, height - 1, j)] = v[idx(i, height - 3, j)]; // left
            v[idx(i, height - 2, j)] = v[idx(i, height - 3, j)];

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
                P = float3(x, h, z) - (v[idx(x, h, z)] * scale);
                v2[i] = sample(P);
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
                v[i] -= 10.f * f * _dt;// *(smin.y / 255.f);
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


void _cfd_lod::simulate(float _dt)
{
    auto a = high_resolution_clock::now();
    vorticty_confine(_dt);

    auto b = high_resolution_clock::now();
    incompressibilityNormal(5); // Mathias uses 100, excessive for this slow air, as LOW as possible, look at feedback

    auto c = high_resolution_clock::now();
    edges();

    auto d = high_resolution_clock::now();
    advect(_dt);        // just do smkoe temperature etc as well

    auto e = high_resolution_clock::now();
    tickCount++;
    frameTime += _dt;

    simTimeLod_vorticity_ms = (double)duration_cast<microseconds>(b - a).count() / 1000.;
    simTimeLod_incompress_ms = (double)duration_cast<microseconds>(c - b).count() / 1000.;
    simTimeLod_edges_ms = (double)duration_cast<microseconds>(d - c).count() / 1000.;
    simTimeLod_advect_ms = (double)duration_cast<microseconds>(e - d).count() / 1000.;

};






void _cfdClipmap::build(std::string _path)
{
    // normals instead f smap
    lods[0].init(128, 32, 40000);
    lods[0].offset = { 0, 0, 0 };
    lods[0].smap.init(128);
    lods[0].smap.load(_path + "/S_128.bin");

    lods[1].init(128, 64, 40000);  // already clipped
    lods[1].offset = { 70, 0, 115 };
    lods[1].root = &lods[0];
    lods[1].smap.init(256);
    lods[1].smap.load(_path + "/S_256.bin");

    lods[2].init(128, 128, 40000);
    lods[2].offset = lods[1].offset + uint3(32, 0, 50);
    lods[2].offset *= 2;
    lods[2].root = &lods[1];
    lods[2].smap.init(512);
    lods[2].smap.load(_path + "/S_512.bin");

    lods[3].init(128, 128, 40000);
    lods[3].offset = lods[2].offset + uint3(42, 0, 50);
    lods[3].offset *= 2;
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

    for (int lod = 1; lod < 6; lod++)
    {
        float time = _dt / pow(2, lod);
        lods[lod].fromRoot();
        lods[lod].incompressibilityNormal(10);
        for (int k = 0; k < 10; k++)       lods[lod].simulate(time);
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
            auto start = high_resolution_clock::now();

            uint lod = 6 - l;
            float time = _dt / pow(2, lod);

            lods[lod].fromRoot_Alpha();
            lods[lod].simulate(time);
            lods[lod].toRoot_Alpha();

            auto stop = high_resolution_clock::now();

            if (lod == 5)
            {
                simTimeLod_ms = (double)duration_cast<microseconds>(stop - start).count() / 1000.;
            }
        }
    }

    counter++;
}

//#pragma optimize("", off)
void _cfdClipmap::streamlines(float3 _p, float4* _data)
{
    float rootAltitude = 350.f;   // in voxel space
    float3 origin = float3(-20000, rootAltitude, -20000);

    std::mt19937 generator(2);
    std::uniform_real_distribution<> distribution(0, 1.f);


    for (int h = 0; h < 10; h++)
    {
        for (int y = 0; y < 20; y++)
        {
            float3 P = (_p - origin) * lods[0].oneOverSize;
            P.y += h * 0.15f;
            P.z += y * 0.2f;

            P += float3(distribution(generator), distribution(generator) * 0.3f, distribution(generator) * 0.4f);

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
                /**/
                if ((p5.x > 2 && p5.y > 0 && p5.z > 2) &&
                    (p5.x < 125 && p5.y < 125 && p5.z < 125))
                {
                    pLOD = &lods[5];
                    Psample = p5;
                    L = 0;
                    scale = 0.3f;
                }

                else if ((p4.x > 2 && p4.y > 0 && p4.z > 2) &&
                    (p4.x < 125 && p4.y < 125 && p4.z < 125))
                {
                    pLOD = &lods[4];
                    Psample = p4;
                    L = 3;
                    scale = 0.3f;
                }

                else  if ((p3.x > 2 && p3.y > 0 && p3.z > 2) &&
                    (p3.x < 125 && p3.y < 125 && p3.z < 125))
                {
                    pLOD = &lods[3];
                    Psample = p3;
                    L = 0;
                    scale = 0.5f;
                }

                else if ((p2.x > 2 && p2.y > 0 && p2.z > 2) &&
                    (p2.x < 125 && p2.y < 125 && p2.z < 125))
                {
                    pLOD = &lods[2];
                    Psample = p2;
                    L = 5;
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
                V = glm::normalize(V);
                if (lv < 0.01f) V = { 0, 0, 0 };
                //if (P.z < pLOD->width && P.x < pLOD->width)
                {
                    P += V * 0.1f;// *scale;// *300.f * oneOverSize;
                }

                float3 c = pLOD->sampleCurl(Psample);
                //_data->w = L;// glm::length(c);

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








/// DEPRECATED

void _cfd_lod::incompressibility_SMAP(uint _num)
{
    glm::u8vec4 smin, smax;
    float3 v0, v1;
    uint3 i0, i1;

    for (uint n = 0; n < _num; n++)
    {
        for (uint h = 0; h < height - 1; h++) {
            for (uint z = 0; z < width - 1; z++) {
                for (uint x = 0; x < width - 1; x++)
                {
                    smin = smap.getS(uint3(x, h, z) + offset);
                    smax.x = smap.getS(uint3(x + 1, h, z) + offset).x;
                    smax.y = smap.getS(uint3(x, h + 1, z) + offset).y;
                    smax.z = smap.getS(uint3(x, h, z + 1) + offset).z;
                    uint sum = smin.x + smin.y + smin.z + smax.x + smax.y + smax.z;


                    // this gets better if we split V and move to boundaries I think
                    // really look at it
                    if (sum > 1)
                    {

                        uint i = idx(x, h, z);
                        float P = ((v[i].x * smin.x - v[i + 1].x * smax.x) +
                            (v[i].y * smin.y - v[i + w2].y * smax.y) +
                            (v[i].z * smin.z - v[i + width].z * smax.z)) / (float)sum * 1.9f / 255.f;

                        // doen black red ...   gauss seidel verwag oorskryf van waardes maar ons propagate in een rigting
                        v[i].x -= P * smin.x;
                        v[i].y -= P * smin.y;
                        v[i].z -= P * smin.z;
                        v[i + 1].x += P * smax.x;
                        v[i + w2].y += P * smax.y;
                        v[i + width].z += P * smax.z;

                        if (smax.y == 0) v[i] *= 0.f;
                    }
                }
            }
        }
    }
};

/*
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
*/
