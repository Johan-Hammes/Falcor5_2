#include "cfd.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <iostream>
#include <fstream>

#include "terrafector.h"

using namespace std::chrono;
//#define GLM_FORCE_SSE2
//#define GLM_FORCE_ALIGNED


//#pragma optimize("", off)

std::vector<cfd_V_type> _cfd_lod::root_v;
std::vector<cfd_data_type> _cfd_lod::root_data;

float                   _cfdClipmap::incompres_relax = 1.9f;
int                     _cfdClipmap::incompres_loop = 4;
float                   _cfdClipmap::vort_confine = 0.f;;
std::vector<cfd_V_type> _cfdClipmap::v_back;
std::vector<cfd_V_type> _cfdClipmap::v_bfecc;
std::vector<cfd_data_type> _cfdClipmap::data_back;
std::vector<cfd_data_type> _cfdClipmap::data_bfecc;

float _cfdClipmap::rootAltitude = 350.f;

std::vector<cfd_V_type> _cfdClipmap::curl;
std::vector<float> _cfdClipmap::mag;
std::vector<cfd_V_type> _cfdClipmap::vorticity;

std::vector<unsigned char> _cfdClipmap::heatMap_4k;

float3 _cfdClipmap::tmp_8_1[8];
float3 _cfdClipmap::tmp_8_2[8][8];
float3 _cfdClipmap::tmp_8_3[8][8][8]; // maybe unnesesary straight to data

float3 _cfdClipmap::skewT_corners_Data[4][100];
float3 _cfdClipmap::skewT_corners_V[4][100];

cfd_V_type _cfdClipmap::skewT_wind[6][1024];
cfd_data_type _cfdClipmap::skewT_data[6][1024];

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
















void _cfd_lod::init(uint _w, uint _h, float _worldSize, float scale)
{
    width = _w;
    w2 = _w * _w;
    height = _h;
    cellSize = _worldSize / _w / scale;
    oneOverSize = 1.f / cellSize;

    v.resize(_h * _w * _w);
    data.resize(_h * _w * _w);

    uint block_h = _h / 8;
    uint block_w = _w / 8;
    blockRating.resize(block_h * block_w * block_w);

    //v2.resize(_h * _w * _w);
    //curl.resize(_h * _w * _w);
    //mag.resize(_h * _w * _w);
    //vorticity.resize(_h * _w * _w);
    //other.resize(_h * _w * _w);
    //other_2.resize(_h * _w * _w);
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




void _cfd_lod::setV(uint3 _p, cfd_V_type _v, cfd_data_type _data)
{
    uint i = idx(_p.x, _p.y, _p.z);
    v[i] = _v;
    data[i] = _data;
}



    // assume request is valid
void _cfd_lod::shiftOrigin(int3 _shift)
{
    if (!root) return;

    offset += _shift;

    float3 to_Root = offset;
    to_Root *= 0.5f;
    to_Root -= root->offset;

    for (uint h = 0; h < height; h++) {
        for (uint z = 0; z < width; z++) {
            for (uint x = 0; x < width; x++)
            {
                bool shift = false;

                if ((_shift.x < 0) && (x < -_shift.x))  shift = true;
                if ((_shift.y < 0) && (h < -_shift.y))  shift = true;
                if ((_shift.z < 0) && (z < -_shift.z))  shift = true;

                if ((_shift.x > 0) && (x >= width - _shift.x))  shift = true;
                if ((_shift.y > 0) && (h >= height - _shift.y))  shift = true;
                if ((_shift.z > 0) && (z >= width - _shift.z))  shift = true;

                //if ((x < -_shift.x) || (x >= width - _shift.x) || (h < -_shift.y) || (h >= height - _shift.y) || (z < -_shift.z) || (z >= width - _shift.z))
                if (shift)
                {
                    uint i = idx(x, h, z);
                    v[i] = root->sample(root->v, float3(x, h, z) * 0.5f + to_Root);
                    data[i] = root->sample(root->data, float3(x, h, z) * 0.5f + to_Root);
                }
            }
        }
    }
}


// lodding
void _cfd_lod::fromRoot()
{
    if (!root) return;


    float3 to_Root = offset;
    to_Root *= 0.5f;
    to_Root -= root->offset;

    for (uint h = 0; h < height; h++) {
        for (uint z = 0; z < width; z++) {
            for (uint x = 0; x < width; x++)
            {
                uint i = idx(x, h, z);
                v[i] = root->sample(root->v, float3(x - 0.5f, h - 0.5f, z - 0.5f) * 0.5f + to_Root);
                data[i] = root->sample(root->data, float3(x - 0.5f, h - 0.5f, z - 0.5f) * 0.5f + to_Root);
            }
        }
    }
}


void _cfd_lod::fromRoot_Alpha()
{
    uint bottom = 0;
    if (offset.y > 0)   bottom = 4;

    if (!root)
    {
        for (uint h = 0; h < height; h++) {
            for (uint z = 0; z < width; z++) {
                for (uint x = 0; x < width; x++)
                {
                    float alpha = 0;
                    if (x < 4) alpha = 1;
                    if (h < bottom) alpha = 1; // dont do bottom
                    if (z < 4) alpha = 1;
                    if (width - x < 5) alpha = 1;
                    if (height - h < 5) alpha = 1;
                    if (width - z < 5) alpha = 1;

                    if (alpha == 1)
                    {
                        uint i = idx(x, h, z);
                        v[i] = _cfd_lod::root_v[i];
                        data[i] = _cfd_lod::root_data[i];
                    }
                }
            }
        }
        
    }

    else
    {

        float3 to_Root = offset;
        to_Root *= 0.5f;
        to_Root -= root->offset;

        for (uint h = 0; h < height; h++) {
            for (uint z = 0; z < width; z++) {
                for (uint x = 0; x < width; x++)
                {
                    // 4 hard pixels
                    float alpha = 0;
                    if (x < 4) alpha = 1;
                    if (h < bottom) alpha = 1;
                    if (z < 4) alpha = 1;
                    if (width - x < 5) alpha = 1;
                    if (height - h < 5) alpha = 1;
                    if (width - z < 5) alpha = 1;

                    if (alpha == 1)
                    {
                        uint i = idx(x, h, z);
                        float3 _p = float3(x - 0.5f, h - 0.5f, z - 0.5f) * 0.5f + to_Root;
                        v[i] = root->sample(root->v, _p);
                        data[i] = root->sample(root->data, _p);
                    }
                }
            }
        }
    }
}

void _cfd_lod::toRoot_Alpha()
{
    if (!root) return;

    uint bottom = 0;
    if (offset.y > 0) bottom = 4;

    for (uint h = bottom; h < height - 4; h+=2) {
        for (uint z = 4; z < width - 4; z+=2) {
            for (uint x = 4; x < width - 4; x+=2)
            {
                uint i = idx(x, h, z);
                cfd_V_type newV = v[i] + v[i + 1] + v[i + width] + v[i + width + 1];
                cfd_data_type newData = data[i] + data[i + 1] + data[i + width] + data[i + width + 1];    // sum bottom 4
                i += width * width;
                newV += v[i] + v[i + 1] + v[i + width] + v[i + width + 1];
                newData += data[i] + data[i + 1] + data[i + width] + data[i + width + 1];   // sum top 4

                int3 iroot = { x, h, z };
                iroot += offset;
                iroot /= 2;
                iroot -= root->offset;
                uint ri = root->idx(iroot.x, iroot.y, iroot.z);
                root->v[ri] = 0.125f * newV;
                root->data[ri] = 0.125f * newData;
            }
        }
    }
}






// not even every time, have some sample thing
void _cfd_lod::clearBelowGround()
{
    for (uint z = 0; z < width; z ++) {
        for (uint x = 0; x < width; x ++)
        {
            // smap
            // 0 .. smin - sample skewt
        }
    }
}



void _cfd_lod::incompressibility_SMAP()      // red-black
{
    glm::u8vec4 smin, smax;
    float3 v0, v1;
    uint3 i0, i1;


    float mP = 0;

    for (uint n = 0; n < _cfdClipmap::incompres_loop * 4; n++)
    {
        if (n % 4 == 0) mP = 0;

        for (uint h = 0; h < height - 1; h++)
        {
            int dX = (n + (h % 2)) % 2;
            int dY = ((n >> 1) + (h % 2)) % 2;
            for (uint z = 0; z < width - 2; z += 2) {
                for (uint x = 0; x < width - 2; x += 2)
                {
                    smin = smap.getS(int3(x + dX, h, z + dY) + offset);
                    smax.x = smap.getS(int3(x + dX + 1, h, z + dY) + offset).x;   // extra reads account for 30ms
                    smax.y = smap.getS(int3(x + dX, h + 1, z + dY) + offset).y;
                    smax.z = smap.getS(int3(x + dX, h, z + dY + 1) + offset).z;
                    uint sum = smin.x + smin.y + smin.z + smax.x + smax.y + smax.z;


                    // this gets better if we split V and move to boundaries I think
                    // really look at it
                    if (sum > 1)
                    {
                        // Thsi block is about 50ms, and the SMAP stuff is only about 6ms so not a huge win to split out

                        uint i = idx(x + dX, h, z + dY);
                        uint ix = idx(x + dX + 1, h, z + dY);
                        uint iy = idx(x + dX, h + 1, z + dY);
                        uint iz = idx(x + dX, h, z + dY + 1);


                        
                        float P = ((v[i].x * smin.x - v[ix].x * smax.x) +
                            (v[i].y * smin.y - v[iy].y * smax.y) +
                            (v[i].z * smin.z - v[iz].z * smax.z)) / (float)sum * _cfdClipmap::incompres_relax / 255.f;

                        mP = __max(mP, abs(P / _cfdClipmap::incompres_relax));

                        // doen black red ...   gauss seidel verwag oorskryf van waardes maar ons propagate in een rigting
                        v[i].x -= (cfd_P_type)(P * smin.x);
                        v[i].y -= (cfd_P_type)(P * smin.y);
                        v[i].z -= (cfd_P_type)(P * smin.z);
                        v[ix].x += (cfd_P_type)(P * smax.x);
                        v[iy].y += (cfd_P_type)(P * smax.y);
                        v[iz].z += (cfd_P_type)(P * smax.z);
                        
                        //if (smax.y == 0) v[i] *= 0.f;
                    }
                    else
                    {
                        // The else accounts for 60-70ms THIS IS BAD
                        // Remove from here
                        
                        uint i = idx(x + dX, h, z + dY);
                        v[i] *= 0.f;
                        data[i] = data[idx(x + dX, h + 1, z + dY)];     // copy above, seems safest
                        //data[i].x += cellSize * 0.0098; //+ dry lapse
                         data[i].x += cellSize * 0.0082; //+ dry lapse HAS TO BE THE SKEWT LOOKUP
                        //data[i].y += cellSize * 0.0098; // moisture
                         data[i].y = 0.45f; // FROM SKEWT once we have it
                        data[i].z = 0; //no smoke underground
                        //data[i].z = 0.2f;

                        float3 v, data_Smp;

                        //float alt = _cfdClipmap::rootAltitude + cellSize * (h + offset.y + 0.5f);
                        //_cfdClipmap::sampleSkewT(float3(0, alt, 0), &v, &data_Smp);
                        //data[i] = data_Smp;
                        data[i] = _cfdClipmap::skewT_data[lod][h + offset.y];
                    }
                }
            }
        }
    }
    maxP = mP;
};



void _cfd_lod::edges()      //?? 1 deep
{
    return;
    if (root == nullptr) return;

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


            data[idx(1, h, i)] = data[idx(2, h, i)]; // left
            data[idx(0, h, i)] = data[idx(2, h, i)];

            data[idx(width - 1, h, i)] = data[idx(width - 3, h, i)];  // rigth
            data[idx(width - 2, h, i)] = data[idx(width - 3, h, i)];

            data[idx(i, h, 0)] = data[idx(i, h, 2)]; // zmin
            data[idx(i, h, 1)] = data[idx(i, h, 2)];

            data[idx(i, h, width - 1)] = data[idx(i, h, width - 3)];  // zmax
            data[idx(i, h, width - 2)] = data[idx(i, h, width - 3)];
        }
    }

    for (int j = 0; j < width; j++)
    {
        for (int i = 0; i < width; i++)
        {
            if (offset.y > 0) {  // bottom
                v[idx(i, 0, j)] = v[idx(i, 2, j)]; // left
                v[idx(i, 1, j)] = v[idx(i, 2, j)];

                data[idx(i, 0, j)] = data[idx(i, 2, j)]; // left
                data[idx(i, 1, j)] = data[idx(i, 2, j)];
            }

            v[idx(i, height - 1, j)] = v[idx(i, height - 3, j)]; // left
            v[idx(i, height - 2, j)] = v[idx(i, height - 3, j)];

            data[idx(i, height - 1, j)] = data[idx(i, height - 3, j)]; // left
            data[idx(i, height - 2, j)] = data[idx(i, height - 3, j)];
        }
    }
};


void _cfd_lod::diffuse(float _dt)
{
    memcpy(_cfdClipmap::v_back.data(), v.data(), width * width * height * sizeof(cfd_V_type));
    memcpy(_cfdClipmap::data_back.data(), data.data(), width * width * height * sizeof(cfd_data_type));


    for (uint h = 1; h < height - 1; h++) {
        for (uint z = 1; z < width - 1; z++) {
            for (uint x = 1; x < width - 1; x++)
            {
                uint i = idx(x, h, z);
                cfd_V_type V = v[i];
                V += v[idx(x - 1, h, z)] * 0.005f;
                V += v[idx(x + 1, h, z)] * 0.005f;
                V += v[idx(x, h - 1, z)] * 0.005f;
                V += v[idx(x, h + 1, z)] * 0.005f;
                V += v[idx(x, h, z - 1)] * 0.005f;
                V += v[idx(x, h, z + 1)] * 0.005f;
                //_cfdClipmap::v_back[i] = V / 1.03f;  lest test without V, I think incompressable fixes this

                

                cfd_data_type d = data[i];
                cfd_data_type d_sides = { 0, 0, 0 };
                d_sides += data[idx(x - 1, h, z)];
                d_sides += data[idx(x + 1, h, z)];
                d_sides += data[idx(x, h - 1, z)];
                d_sides += data[idx(x, h + 1, z)];
                d_sides += data[idx(x, h, z - 1)];
                d_sides += data[idx(x, h, z + 1)];
                _cfdClipmap::data_back[i].x = (d.x + d_sides.x / 6 * 0.05f) / 1.05f;
                _cfdClipmap::data_back[i].y = (d.y + d_sides.y / 6 * 0.05f) / 1.05f;
                _cfdClipmap::data_back[i].z = (d.z + d_sides.z / 6 * 0.01f) / 1.01f;
                
            }
        }
    }

    memcpy(v.data(), _cfdClipmap::v_back.data(), width * width * height * sizeof(cfd_V_type));
    memcpy(data.data(), _cfdClipmap::data_back.data(), width * width * height * sizeof(cfd_data_type));
}




// we can always do more smaller steps for better acuracy, migth work great
void _cfd_lod::advect(float _dt)
{
    float3 P;
    float scale = oneOverSize * _dt;
    float speed = 0;

    // vbach needs edges set, see if its faster to do less here
    // 7ms quite a lot - We only need edges, that is not covered below, so do better
    memcpy(_cfdClipmap::v_back.data(), v.data(), width * width * height * sizeof(cfd_V_type));
    memcpy(_cfdClipmap::data_back.data(), data.data(), width * width * height * sizeof(cfd_data_type));

    // back
    for (uint h = 0; h < height - 2; h++) {
        for (uint z = 2; z < width - 2; z++) {
            for (uint x = 2; x < width - 2; x++)
            {
                uint i = idx(x, h, z);
                P = float3(x, h, z) - (getV(idx(x, h, z)) * scale);
                _cfdClipmap::v_back[i] = sample(v, P);
                _cfdClipmap::data_back[i] = sample(data, P);

                //5 ms need to remove eventually, or run sparse
                speed = __max(speed, glm::length(getV(idx(x, h, z)))); // migth as well, doesn show on profilibg
            }
        }
    }

    //bfecc
    for (uint h = 0; h < height; h++) {
        for (uint z = 0; z < width; z++) {
            for (uint x = 0; x < width; x++)
            {
                uint i = idx(x, h, z);
                float3 V = getV(idx(x, h, z));  //v[i]
                P = float3(x, h, z) + ( V * scale);      // + for forward
                float3 B = sample(_cfdClipmap::v_back, P);
                _cfdClipmap::v_bfecc[i] = V + 0.5f * (V - B);

                _cfdClipmap::data_bfecc[i] = data[i] + 0.5f * (data[i] - sample(_cfdClipmap::data_back, P));
                _cfdClipmap::data_bfecc[i].z = __max(_cfdClipmap::data_bfecc[i].z, 0);

                // start of a limiter cant really stay since negative is allowed, do black and red in incomoperssibility first
                //_cfdClipmap::v_bfecc[i].w = __max(0, _cfdClipmap::v_bfecc[i].w);
            }
        }
    }

    // last step
    uint bottom = 0;
    if (offset.y > 0) bottom = 4;
    for (uint h = bottom; h < height - 4; h++) {
        for (uint z = 4; z < width - 4; z++) {
            for (uint x = 4; x < width - 4; x++)
            {
                uint i = idx(x, h, z);
                float dh = v[i].y * _dt;    // vertical move

                P = float3(x, h, z) - (getV(idx(x, h, z)) * scale);
                v[i] = sample(_cfdClipmap::v_bfecc, P);
                data[i] = sample(_cfdClipmap::data_bfecc, P);

                //now correct the temperture
                float dewPoint = dew_Temp_C(data[i].y);
                float dtemp = -0.0098f * dh;        // dry adiabatic lapse
                if (dewPoint > to_C(data[i].x))     // test wet adiabatic
                {
                    float alt = _cfdClipmap::rootAltitude + cellSize * (h + offset.y + 0.5f);
                    //dtemp = -1.f * moistLapse(data[i].x, alt) * dh;
                }
                data[i].x = glm::lerp(data[i].x, _cfdClipmap::data_back[i].x, 0.5f); // normnla fo r temp
                data[i].x += dtemp;
            }
        }
    }


   // memcpy(v.data(), _cfdClipmap::v_back.data(), width * width * height * sizeof(cfd_V_type));
   // memcpy(data.data(), _cfdClipmap::data_back.data(), width * width * height * sizeof(cfd_data_type));

    maxSpeed = speed;
    maxStep = maxSpeed * scale;
};





/*  Do this 2D with ew smap*/
void _cfd_lod::addTemperature(float _dt)
{
    
    //if (cellSize > 35) return;

    // stokberg
    float4 thermals[24] = {
        {6651, 1250, 5863, 75},
        {8610, 1075, 6229, 72},
        {7700, 1299, 7137, 73},
        {10086, 1394, 8716, 81},
        {9791, 1616, 9503, 76},
        {14455, 1325, 8365, 76},
        {13175, 1386, 7366, 87},
        {13877, 1812, 6764, 69},
        {13130, 1653, 5941, 83},
        {15579, 1473, 7191, 82},
        {16352, 1471, 6842, 77},
        {17155, 1473, 6500, 71},
        {17949, 1336, 6205, 95},
        {12253, 1240, 5493, 62},
        {12715, 1483, 4503, 99},
        {16301, 1601, 4474, 80},
        {17323, 1704, 5239, 76},
        {17261, 1280, 3225, 63},
        {16676, 1148, 2728, 83},
        {17731, 1319, 2413, 83},
        {14975, 1395, 1994, 78},
        {14114, 1315, 1738, 76},
        {13586, 1493, 1329, 83},
        {11906, 1311, 1304, 87} };

    //float3 t1 = float3(-1425 + 400, 425 + 50, 12533);
    //float3 t1 = float3(1184, 422 + 40, 14829);
    float3 t1[10] = { float3(9224, 758 + 40, 4291),
                    float3(9024, 758 + 90, 3891),
                    float3(9424, 758 + 40, 4091),
                    float3(9624, 758 + 80, 4891),
                    float3(9924, 758 + 40, 4491),

                    float3(9324, 758 + 240, 4591),
                    float3(9524, 758 + 280, 4891),
                    float3(9224, 758 + 240, 5091),
                    float3(9324, 758 + 280, 5391),
                    float3(9524, 758 + 240, 4791) };

    float s1 = 20.f + cellSize / 2;
    float s0 = 20.f + 10.f / 2;
    float volume = 4.18879020f * s1 * s1;
    float volumeZero = 4.18879020f * s0 * s0;
    float energy = volumeZero / volume * 1.5f;

    if (energy < 0.05f) energy = 0;

    float smtStep = 1.f + cellSize / s1 * 0.5f;

    // sphere for now with gradient, slow this is temporary
    for (uint h = 0; h < height; h++) {
        for (uint z = 0; z < width; z++) {
            for (uint x = 0; x < width; x++) {
                float3 P = { x, h, z };
                P += offset;
                P *= cellSize;
                P.x -= 20000;
                P.y += _cfdClipmap::rootAltitude;
                P.z -= 20000;

                glm::u8vec4 s = smap.getS(int3(x, h, z) + offset);
                if (s.x < 255 || s.z < 255) // Find groud level
                {
                    
                    // kk7
                    for (int i = 0; i < 24; i++)
                    {
                        float l = glm::length(P - thermals[i].rgb);
                        if (l < 300)
                        {
                            // temp
                            data[idx(x, h, z)].x += 0.05f;  // * _dt but account for volume

                            //data[idx(x, h, z)].z += 0.01f;
                        }
                    }
                    
                    /*
                    int scale = 32;
                    if (cellSize < 160) scale = 16;
                    if (cellSize < 80) scale = 8;
                    if (cellSize < 40) scale = 4;
                    if (cellSize < 20) scale = 2;
                    if (cellSize < 10) scale = 1;

                    if (scale < 16)
                    {
                        uint hIndex = (z + offset.z) * scale * 4096 + (x + offset.x) * scale;
                        float dTmp = ((float)_cfdClipmap::heatMap_4k[hIndex] - 128.f) / 128.f;

                        float heatScale = 0.2f;// / pow(scale, 0.5);
                        data[idx(x, h, z)].x += heatScale * dTmp;  // * _dt but account for volume
                        //data[idx(x, h, z)].y += heatScale * dTmp;  // * _dt but account for volume
                    }
                    */
                }

                
                
                for (int i = 0; i < 0; i++)
                {
                    float l = glm::length(P - t1[i]);
                    if (l < 5 * cellSize)
                    {
                        float alpha = glm::smoothstep(0.5f, 1.5f, s1 / l);
                        data[idx(x, h, z)].z = __max(data[idx(x, h, z)].z, energy * alpha * 0.5f);
                        data[idx(x, h, z)].x = __max(data[idx(x, h, z)].x, to_K(24 + alpha * 4.05));
                    }
                }
            }
        }
    }
}


/*  ??? where to fold this into, unnesesary loop*/
void _cfd_lod::bouyancy(float _dt)
{
    for (uint h = 0; h < height-1; h++)
    {
        float baseTemperature = _cfdClipmap::skewT_data[lod][h + offset.y].x;

        for (uint z = 0; z < width; z++) {
            for (uint x = 0; x < width; x++)
            {
                uint i = idx(x, h, z);
                v[i].y += (1.f - baseTemperature / data[i].x) * _dt * 9.8f;
            }
        }
    }
}





void _cfd_lod::clamp(float3& _p)
{
    _p.x = glm::clamp(_p.x, 0.f, (float)(width - 2));       // wrong reference in palce and inline
    _p.y = glm::clamp(_p.y, 0.f, (float)(height - 2));
    _p.z = glm::clamp(_p.z, 0.f, (float)(width - 2));
}


template <typename T> T _cfd_lod::sample(std::vector<T>& _data, float3 _p)
{
    clamp(_p);
    int3 i = _p;
    float3 f1 = _p - (float3)i;
    float3 f0 = float3(1, 1, 1) - f1;

    T a = glm::mix(_data[idx(i.x, i.y, i.z)], _data[idx(i.x + 1, i.y, i.z)], f1.x);
    T b = glm::mix(_data[idx(i.x, i.y, i.z + 1)], _data[idx(i.x + 1, i.y, i.z + 1)], f1.x);
    T bottom = glm::mix(a, b, f1.z);

    a = glm::mix(_data[idx(i.x, i.y + 1, i.z)], _data[idx(i.x + 1, i.y + 1, i.z)], f1.x);
    b = glm::mix(_data[idx(i.x, i.y + 1, i.z + 1)], _data[idx(i.x + 1, i.y + 1, i.z + 1)], f1.x);
    T top = glm::mix(a, b, f1.z);

    return glm::mix(bottom, top, f1.y);
}

// split to faces




void _cfd_lod::simulate(float _dt)
{
    timer += _dt;

    auto a = high_resolution_clock::now();
    
    addTemperature(_dt);
    bouyancy(_dt);
    //vorticty_confine(_dt, _cfdClipmap::vort_confine); ??? not sure

    auto b = high_resolution_clock::now();
    incompressibility_SMAP();                       //??? I think this should be last, leave the saves state as good as possible

    auto c = high_resolution_clock::now();
    advect(_dt);        // just do smoke temperature etc as well

    auto d = high_resolution_clock::now();
    diffuse(_dt);

    auto e = high_resolution_clock::now();
    tickCount++;
    frameTime += _dt;

    simTimeLod_boyancy_ms = (double)duration_cast<microseconds>(b - a).count() / 1000.;
    simTimeLod_incompress_ms = (double)duration_cast<microseconds>(c - b).count() / 1000.;
    simTimeLod_advect_ms = (double)duration_cast<microseconds>(d - c).count() / 1000.;
    simTimeLod_edges_ms = (double)duration_cast<microseconds>(e - d).count() / 1000.;
    solveTime_ms = (float)duration_cast<microseconds>(e - a).count() / 1000.;
};



void _cfdClipmap::sampleSkewT(float3 _pos, float3* _v, float3* _data)
{
    // LAZY just use heigth
    float h = __max(0, __min(98, _pos.y / 50.f));       // weird inert atmosphere above 10km
    uint ih = (uint)h;
    float dh = h - ih;

    *_v = glm::lerp(skewT_corners_V[0][ih], skewT_corners_V[0][ih+1], dh);
    *_data = glm::lerp(skewT_corners_Data[0][ih], skewT_corners_Data[0][ih + 1], dh);
}


void _cfdClipmap::loadSkewT(std::string _path)
{
    std::ifstream ifs;
    ifs.open(_path, std::ios::binary);
    if (ifs)
    {
        ifs.read((char*)&skewT_corners_Data[0][0], sizeof(float3) * 100);
        ifs.read((char*)&skewT_corners_V[0][0], sizeof(float3) * 100);
        ifs.close();
    }

    for (int j = 0; j < 100; j++)
    {
        skewT_corners_Data[0][j].x = to_K(skewT_corners_Data[0][j].x);
        skewT_corners_Data[0][j].z = 0;

        for (float rh = 0; rh < 1; rh += 0.0001f)
        {
            float dewC = dew_Temp_C(rh);
            if (dewC > skewT_corners_Data[0][j].y)
            {
                skewT_corners_Data[0][j].y = rh;
                break;
            }
        }
    }
    

    // replicate for now
    for (int i = 1; i < 4; i++)
    {
        for (int j = 0; j < 100; j++)
        {
            skewT_corners_Data[i][j] = skewT_corners_Data[0][j];
            skewT_corners_V[i][j] = skewT_corners_V[0][j];
        }
    }

    // Now sample my lod specific skewT - own function so we can do after wind 
    float3 v_samp, data;
    fprintf(terrafectorSystem::_logfile, "loadSkewT(std::string _path)\n");
    for (int i = 0; i < 1024; i++)
    {
        
        sampleSkewT(float3(0, rootAltitude + 5.f + (i * 10.f), 0), &v_samp, &data);
        skewT_wind[5][i] = v_samp;
        skewT_data[5][i] = data;
        fprintf(terrafectorSystem::_logfile, "%d, %f\n", i, skewT_data[5][i].x);
    }
    // sample down
    for (int i = 0; i < 512; i++)
    {
        skewT_wind[4][i] = (skewT_wind[5][i * 2 + 0] + skewT_wind[5][i * 2 + 1]) * 0.5f;
        skewT_data[4][i] = (skewT_data[5][i * 2 + 0] + skewT_data[5][i * 2 + 1]) * 0.5f;
        fprintf(terrafectorSystem::_logfile, "%d, %f\n", i, skewT_data[4][i].x);
    }

    for (int i = 0; i < 256; i++)
    {
        skewT_wind[3][i] = (skewT_wind[4][i * 2 + 0] + skewT_wind[4][i * 2 + 1]) * 0.5f;
        skewT_data[3][i] = (skewT_data[4][i * 2 + 0] + skewT_data[4][i * 2 + 1]) * 0.5f;
        fprintf(terrafectorSystem::_logfile, "%d, %f\n", i, skewT_data[3][i].x);
    }

    for (int i = 0; i < 128; i++)
    {
        skewT_wind[2][i] = (skewT_wind[3][i * 2 + 0] + skewT_wind[3][i * 2 + 1]) * 0.5f;
        skewT_data[2][i] = (skewT_data[3][i * 2 + 0] + skewT_data[3][i * 2 + 1]) * 0.5f;
        fprintf(terrafectorSystem::_logfile, "%d, %f\n", i, skewT_data[2][i].x);
    }

    for (int i = 0; i < 64; i++)
    {
        skewT_wind[1][i] = (skewT_wind[2][i * 2 + 0] + skewT_wind[2][i * 2 + 1]) * 0.5f;
        skewT_data[1][i] = (skewT_data[2][i * 2 + 0] + skewT_data[2][i * 2 + 1]) * 0.5f;
    }

    for (int i = 0; i < 32; i++)
    {
        skewT_wind[0][i] = (skewT_wind[1][i * 2 + 0] + skewT_wind[1][i * 2 + 1]) * 0.5f;
        skewT_data[0][i] = (skewT_data[1][i * 2 + 0] + skewT_data[1][i * 2 + 1]) * 0.5f;
    }
}


void _cfdClipmap::build(std::string _path)
{
    //loadSkewT(_path + "/Shanis_August3_1400.skewT_5000");
    loadSkewT(_path + "/Walenstad_August5_1500.skewT_5000");
    
    //loadSkewT(_path + "/Altenstadt_July18_1600.skewT_5000");
    

    v_back.resize(128 * 128 * 128);     // maximum of any below or bigger
    v_bfecc.resize(128 * 128 * 128);
    data_back.resize(128 * 128 * 128);     // maximum of any below or bigger
    data_bfecc.resize(128 * 128 * 128);

    curl.resize(128 * 128 * 128);
    mag.resize(128 * 128 * 128);
    vorticity.resize(128 * 128 * 128);

    // normals instead of smap
    lods[0].init(128, 32, 40000, 1);
    lods[0].offset = { 0, 0, 0 };
    lods[0].smap.init(128);
    lods[0].smap.load(_path + "/S_128.bin");
    lods[0].lod = 0;

    _cfd_lod::root_v.resize(128 * 128 * 32);
    _cfd_lod::root_data.resize(128 * 128 * 32);

    lods[1].init(128, 64, 40000, 2);  // already clipped
    lods[1].offset = { 70, 0, 115 };
    lods[1].root = &lods[0];
    lods[1].smap.init(256);
    lods[1].smap.load(_path + "/S_256.bin");
    lods[1].lod = 1;

    lods[2].init(128, 128, 40000, 4);
    lods[2].offset = lods[1].offset + int3(32, 0, 50);
    lods[2].offset *= 2;
    lods[2].root = &lods[1];
    lods[2].smap.init(512);
    lods[2].smap.load(_path + "/S_512.bin");
    lods[2].lod = 2;

    lods[3].init(128, 128, 40000, 8);
    lods[3].offset = lods[2].offset + int3(42, 0, 50);
    lods[3].offset *= 2;
    lods[3].root = &lods[2];
    lods[3].smap.init(1024);
    lods[3].smap.load(_path + "/S_1024.bin");
    lods[3].lod = 3;

    lods[4].init(128, 128, 40000, 16);
    lods[4].offset = lods[3].offset + int3(32, 0, 32);
    lods[4].offset *= 2;
    lods[4].root = &lods[3];
    lods[4].smap.init(2048);
    lods[4].smap.load(_path + "/S_2048.bin");
    lods[4].lod = 4;

    lods[5].init(128, 128, 40000, 32);
    lods[5].offset = lods[4].offset + int3(32, 20, 32);
    lods[5].offset *= 2;
    lods[5].root = &lods[4];
    lods[5].smap.init(4096);
    lods[5].smap.load(_path + "/S_4096.bin");
    lods[5].lod = 5;

    
    lods[0].loadNormals(_path + "/S_128.normals.bin");
    lods[1].loadNormals(_path + "/S_256.normals.bin");
    lods[2].loadNormals(_path + "/S_512.normals.bin");
    lods[3].loadNormals(_path + "/S_1024.normals.bin");
    lods[4].loadNormals(_path + "/S_2048.normals.bin");
    lods[5].loadNormals(_path + "/S_4096.normals.bin");

    /*  FIXME needs cleanup bit arb and hardcoded*/
    std::ifstream ifs;
    ifs.open(_path + "/Heat_Index_A.raw", std::ios::binary);
    if (ifs)
    {
        heatMap_4k.resize(4096 * 4096);
        ifs.read((char*)heatMap_4k.data(), 4096 * 4096);
        ifs.close();
    }
    
}


void _cfdClipmap::setWind(float3 _bottom, float3 _top)
{
    cfd_data_type DATA;
    cfd_V_type wind;

    for (uint y = 0; y < 32; y++)
    {
        float altitude = _cfdClipmap::rootAltitude + ((y + 0.5f) * lods[0].cellSize);
        //float3 wind = glm::mix(_bottom, _top, (float)y / 32.f);
        sampleSkewT(float3(0, altitude, 0), &wind, &DATA);

        DATA = _cfdClipmap::skewT_data[0][y];

        for (uint z = 0; z < 128; z++)
        {
            for (uint x = 0; x < 128; x++)
            {
                lods[0].setV(uint3(x, y, z), wind, DATA);
            }
        }
    }

}

void _cfdClipmap::setFromSkewT(uint lod)
{
    float3 v, data;

    for (uint y = 0; y < lods[lod].height; y++)
    {
        float altitude = _cfdClipmap::rootAltitude + ((y + 0.5f + lods[lod].offset.y) * lods[lod].cellSize);
        sampleSkewT(float3(0, altitude, 0), &v, &data);

        data = _cfdClipmap::skewT_data[lod][y + lods[lod].offset.y];

        for (uint z = 0; z < lods[lod].width; z++)
        {
            for (uint x = 0; x < lods[lod].width; x++)
            {
                lods[lod].setV(uint3(x, y, z), v, data);
            }
        }
    }
}



void _cfdClipmap::simulate_start(float _dt)
{
    for (int k = 0; k < 5; k++)
    {
        //lods[0].simulate(_dt);
        lods[0].timer = 0;
    }

    setFromSkewT(0);
    lods[0].lastStart = high_resolution_clock::now();

    // make a backup of the root velocities for edges
    // later on feed this from windguru or yr, so actual source
    // Replace with SkewT
    memcpy(_cfd_lod::root_v.data(), lods[0].v.data(), lods[0].width * lods[0].width * lods[0].height * sizeof(cfd_V_type));
    memcpy(_cfd_lod::root_data.data(), lods[0].data.data(), lods[0].width * lods[0].width * lods[0].height * sizeof(cfd_data_type));
    

    for (int lod = 1; lod < 6; lod++)
    {
        float time = _dt / pow(2, lod);
        //lods[lod].fromRoot();
        setFromSkewT(lod);
        lods[lod].lastStart = high_resolution_clock::now();
        //lods[lod].incompressibilityNormal(10);
        //for (int k = 0; k < 3; k++)       lods[lod].simulate(time);
        lods[lod].timer = 0;
    }
}




void _cfdClipmap::simulate(float _dt)
{
    for (int l = 1; l <= 6; l++)
    {
        uint start = (int)pow(2, l);
        uint step = start * 2;
        uint test = (counter - start) % step;

        //l = 6;
        //test = 0;
        if (test == 0)
        {
            auto start = high_resolution_clock::now();

            uint lod = 6 - l;
            float time = _dt / pow(2, lod);

            
            auto& L = lods[lod];
            
            {
                // WAIT HERE
                auto starTime = high_resolution_clock::now();
                while ((float)duration_cast<microseconds>(starTime - lods[lod].lastStart).count() / 1000000. < time)
                {
                    starTime = high_resolution_clock::now();
                }
                lods[lod].lastStart = starTime;


                if (lod < 5)        // copy the above block down
                {
                    auto a = high_resolution_clock::now();
                    lods[lod + 1].toRoot_Alpha();
                    auto b = high_resolution_clock::now();
                    lods[lod].simTimeLod_toRoot_ms = (float)duration_cast<microseconds>(b - a).count() / 1000.;
                }

                L.simulate(time);

                if (lod < 5)        // now copy this block up
                {
                    auto a = high_resolution_clock::now();
                    lods[lod + 1].fromRoot_Alpha();
                    auto b = high_resolution_clock::now();
                    lods[lod].simTimeLod_fromRoot_ms = (float)duration_cast<microseconds>(b - a).count() / 1000.;
                }
            }

            
            auto stop = high_resolution_clock::now();
            lods[lod].solveTime_ms = (float)duration_cast<microseconds>(stop - start).count() / 1000.;
            

            if (lod == 5)
            {
                simTimeLod_ms = (double)duration_cast<microseconds>(stop - start).count() / 1000.;
            }

            if (showSlice&& lod >=2)
            {
                slicelod = lod;
                lodOffsets[lod] = float4(-20000, 350, -20000, 0) + L.cellSize * float4(L.offset, 0);
                lodScales[lod] = float4(1.f / (L.cellSize * L.width), 1.f / (L.cellSize * L.height), 1.f / (L.cellSize * L.width), 0);

                for (int y = 0; y < L.height; y++)
                {
                    for (int x = 0; x < 128; x++)
                    {
                        uint index = L.idx(x, y, sliceIndex);
                        sliceV[(L.height - 1- y) * 128 + x] = L.v[index];
                        sliceData[(L.height - 1 - y) * 128 + x] = L.data[index];

                        /*
                        uint bW = L.width >> 3;
                        uint bi = ((y >> 3) * bW * bW) + ((sliceIndex >> 3) * bW) + (x >> 3);  // do a safe version
                        if (L.blockRating[bi] < 30)
                        {
                            sliceData[(L.height - 1 - y) * 128 + x].z += 0.01f;
                        }*/
                    }


                    //float alt = _cfdClipmap::rootAltitude + L.cellSize * (y + L.offset.y + 0.5f);
                    //float3 v, data;
                    //sampleSkewT(float3(0, alt, 0), &v, &data);
                    //float baseTemperature = data.x;
                    float baseTemperature = skewT_data[lod][y + L.offset.y].x;

                    for (int z = 0; z < 128; z++)
                    {
                        for (int x = 0; x < 128; x++)
                        {
                            unsigned short R, G, B;
                            uint i = L.idx(x, y, z);
                            R = (unsigned short)(L.data[i].z * 31);

                            float C = to_C(L.data[i].x);
                            float DewC = dew_Temp_C(L.data[i].y);
                            float cloud = saturate((DewC - C) / 2);
                            B = (unsigned short)(cloud * 31);

                            // veticvl velocity
                            // FIM overflow CLAMP
                            G = (unsigned short)(L.v[i].y * 5.f + 32);  // 32 gee 0.5 in shader

                            // bouyncy
                            //float d0 = density(alt, baseTemperature);
                            //float d1 = density(alt, L.data[i].x);
                            float dT = (L.data[i].x - baseTemperature) * 10;    // 3 degrees spread
                            dT = __min(31, __max(-31, dT));
                            G = (unsigned short)(dT  + 32);
                            //G = (unsigned short)(32);
                            

                            volumeData[y][z][x] = (R << 11) + (G << 5) + B;
                        }
                    }
                    
                }

                float S = 128 * L.cellSize;
                sliceCorners[0] = float3(lods[lod].offset + int3(0, 0, sliceIndex)) * L.cellSize + float3(-20000, 350, -20000);
                sliceCorners[1] = sliceCorners[0];
                sliceCorners[1].x += S;
                sliceCorners[2] = sliceCorners[0];
                sliceCorners[2].y += S;
                sliceCorners[3] = sliceCorners[2];
                sliceCorners[3].x += S;


                float3 p0 = {64, 0, sliceIndex };
                float altSample = 0.f;
                for (int j = 0; j < 100; j++)
                {
                    p0.y = (altSample - _cfdClipmap::rootAltitude + lods[3].cellSize * .5f) / lods[3].cellSize;
                    float3 data;
                    data = lods[3].sample(lods[3].data, p0);
                    skewTData[j].x = to_C(data.x);
                    skewTData[j].y = dew_Temp_C(data.y);

                    skewTV[j] = lods[3].sample(lods[3].v, p0);

                    altSample += 5000.f / 100.f;
                    //p0.y += 1.2f;                    
                }
                skewTGround = 0;

                

                sliceNew = true;
            }

            
        }
    }

    counter++;
}

//#pragma optimize("", off)
void _cfdClipmap::streamlines(float3 _p, float4* _data, float3 right)
{
    float3 origin = float3(-20000, _cfdClipmap::rootAltitude, -20000);

    std::mt19937 generator(2);
    std::uniform_real_distribution<> distribution(-0.5, 0.5f);


    for (int h = -3; h < 7; h++)
    {
        for (int y = -10; y < 10; y++)
        {
            float3 P = (_p - origin) * lods[0].oneOverSize;
            /*
            P.y += h * 0.85f;
            P.z += y * 1.85f * right.z;
            P.x += y * 1.85f * right.x;

            P += float3(distribution(generator), distribution(generator) * 0.3f, distribution(generator) * 0.4f);
            */
            P += float3(distribution(generator), distribution(generator), distribution(generator)) * streamlineAreaScale;    //??? 80 meters I hope

            float3 Pstart = P;

            for (int j = 50; j < 100; j++)
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
                scale = stremlineScale;

                pLOD->clamp(Psample);

                float3 V = pLOD->sample(pLOD->v, Psample);

                (_data + j)->xyz = P * lods[0].cellSize + origin;
                (_data + j)->w = glm::length(V);
                float lv = glm::length(V);
                V = glm::normalize(V);
                if (lv < 0.01f) V = { 0, 0, 0 };
                //if (P.z < pLOD->width && P.x < pLOD->width)
                {
                    P += V * 0.86f *scale;// *300.f * oneOverSize;
                }
            }


            P = Pstart;

            for (int j = 49; j >= 0; j--)
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
                scale = stremlineScale;

                pLOD->clamp(Psample);
                float3 V = pLOD->sample(pLOD->v, Psample);

                (_data + j)->xyz = P * lods[0].cellSize + origin;
                (_data + j)->w = glm::length(V);
                float lv = glm::length(V);
                V = glm::normalize(V);
                if (lv < 0.01f) V = { 0, 0, 0 };
                //if (P.z < pLOD->width && P.x < pLOD->width)
                {
                    P -= V * 0.86f *scale;// *300.f * oneOverSize;
                }

            }
            _data += 100;
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


void _cfdClipmap::shiftOrigin(float3 _origin)
{
    float3 world_origin = float3(-20000, _cfdClipmap::rootAltitude, -20000);
    float3 P0 = (_origin - world_origin) * lods[0].oneOverSize;

    for (int lod = 1; lod < 6; lod++)
    {
        int3 plo = P0 * (float)pow(2, lod);
        plo -= int3(lods[lod].width / 2, lods[lod].height  * 0.75f, lods[lod].width / 2 );  // 75% height
        int maxHeight = lods[0].height * (int)pow(2, lod);

        // clapmp to even numbers for up down copy
        plo.x &= 0xfffffffe;
        plo.y &= 0xfffffffe;
        plo.z &= 0xfffffffe;

        // Y NOT IN MIDDEL 66% or 75%

        // clamp
        plo.x = __max(plo.x, 0);
        plo.y = __max(plo.y, 0);
        plo.z = __max(plo.z, 0);

        plo.x = __min(plo.x, lods[lod].smap.width - lods[lod].width);
        plo.y = __min(plo.y, maxHeight - lods[lod].height);
        plo.z = __min(plo.z, lods[lod].smap.width - lods[lod].width);

        if ((plo.x != lods[lod].offset.x) || (plo.y != lods[lod].offset.y) || (plo.z != lods[lod].offset.z))
        {
            int3 shift = plo - lods[lod].offset;
            lods[lod].shiftOrigin(shift);
        }
    }
}







//#pragma optimize("", off)
void _cfdClipmap::heightToSmap(std::string filename)
{

    float scale4K = 40000.f / 4096.f;   // single voxel at 4k
    float oneOver = 4096.f / 40000.f;
    //float rootAltitude = 350.f;   // in voxel space
    // BUT MAYBE SET IT HERE

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
            hgt[z * 4096 + x] -= _cfdClipmap::rootAltitude;
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
                    float dX = glm::clamp(glm::mix(b, e, k) - h, 0.f, 1.f);
                    sfloat.x += dX;
                    float dZ = glm::clamp(glm::mix(b, c, k) - h, 0.f, 1.f);
                    sfloat.z += dZ;

                    float y0 = glm::mix(b, c, k);
                    float y1 = glm::mix(e, d, k);
                    for (float l = 0.05f; l < 1; l += 0.1f)
                    {
                        float y = glm::mix(y0, y1, l);
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
/*
*   https://github.com/kalexmills/jos-stam-vort-confine/blob/master/code/solver.c
    float a=dt*diff*N*N;
	lin_solve ( N, b, x, x0, a, 1+4*a );
    x[IX(i,j)] = (x0[IX(i,j)] + a*(x[IX(i-1,j)]+x[IX(i+1,j)]+x[IX(i,j-1)]+x[IX(i,j+1)]))/c;

    this on in 3d, same idea
    https://github.com/BlainMaguire/3dfluid/blob/master/solver3d.c#L91          
    int max = MAX(MAX(M, N), MAX(N, O));
    float a=dt*diff*max*max*max;
    lin_solve ( M, N, O, b, x, x0, a, 1+6*a );

    this one calculates the pressure field, more important for wing later
    https://people.computing.clemson.edu/~jtessen/cpsc8190/html/lectures/velocity_fields/incompressibility_slides.pdf

    file:///C:/Users/hamme/Downloads/CAM_CDO.pdf

    https://www.cs.ubc.ca/~rbridson/fluidsimulation/fluids_notes.pdf        // Mathias

    https://www.cs.unc.edu/xcms/wpfiles/dissertations/harris.pdf            // HArris 2003

    https://www.ljll.fr/~frey/papers/levelsets/Selle%20A.,%20An%20unconditionally%20stable%20MacCormack%20method.pdf
*/




/*  D only 8 pixels around the actual height - this is really fast, pretty much 2D
*/
/*
void _cfd_lod::Normal()
{
    for (uint z = 0; z < width - 1; z++) {
        for (uint x = 0; x < width - 1; x++)
        {
            uint nIndex = (z + offset.z) * smap.width + (x + offset.x);

            float3 N = normals[nIndex].xyz;
            float Hcell = 0.5f - normals[nIndex].w - offset.y;
            int h0 = (int)(normals[nIndex].w - 0.f - offset.y);
            h0 = __min(height - 10, __max(0, h0));   // far enoufgh down that even fraction wont break it, we have
            Hcell += h0;

            for (int h = 0; h < h0; h++)
            {
                float scale = __max(0, 1.f - __min(1.f, abs(Hcell) / 2.f));
                uint i = idx(x, h, z);
                v[i] *= 0.f;
                //v[i] = float4(0, 0, 0, 0);
            }


            for (uint h = h0; h < h0 + 8; h++)
            {
                float scale = __max(0, 1.f - __min(1.f, abs(Hcell) / 2.f));
                uint i = idx(x, h, z);
                float3 normWind = N * glm::dot(float3(v[i].xyz), N);
                v[i].xyz -= normWind * scale;

                //v[i] = float4(255, 0, 0, 0);


                //v[i] *= 1.f - scale * 0.0001f;
                //if (Hcell < 0.f) v[i] *= 0.f;
                Hcell += 1.f;
            }
        }
    }
}


// Stepping heigth last will simplify normal checking, also means packing height last will simplify memory
void _cfd_lod::incompressibility_Normal(uint _num)
{
    float mP = 0;

    for (uint n = 0; n < _num; n++)
    {
        Normal();

        mP = 0;
        for (uint h = 0; h < height - 1; h++)
        {
            int dX = (n + (h % 2)) % 2;
            int dY = ((n >> 1) + (h % 2)) % 2;

            for (uint z = 0; z < width - 2; z += 2) {
                for (uint x = 0; x < width - 2; x += 2)
                {
                    uint nIndex = (z + offset.z) * smap.width + (x + offset.x);

                    float h_rel = h + 0.5 - normals[nIndex].w;

                    uint i = idx(x + dX, h, z + dY);
                    uint ix = idx(x + dX + 1, h, z + dY);
                    uint iy = idx(x + dX, h + 1, z + dY);
                    uint iz = idx(x + dX, h, z + dY + 1);
                    float P = (v[i].x + v[i].y + v[i].z - v[ix].x - v[iy].y - v[iz].z) * 0.16666666;
                    mP = __max(mP, abs(P));
                    P *= 1.9f;

                    // doen black red ...   gauss seidel verwag oorskryf van waardes maar ons propagate in een rigting
                    v[i].x -= P;
                    v[i].y -= P;
                    v[i].z -= P;
                    v[ix].x += P;
                    v[iy].y += P;
                    v[iz].z += P;
                }
            }
        }
    }

    maxP = mP;
};

*/
/*
void _cfd_lod::vorticty_confine(float _dt, float _scale)
{
    if (_scale == 0) return;

    // curl from velocity
    for (uint h = 0; h < height - 1; h++) {
        for (uint z = 0; z < width - 1; z++) {
            for (uint x = 0; x < width - 1; x++)
            {
                uint i = idx(x, h, z);
                float xx_c = v[idx(x, h + 1, z)].z - v[i].z - v[idx(x, h, z + 1)].y + v[i].y;
                float yy_c = v[idx(x, h, z + 1)].x - v[i].x - v[idx(x + 1, h, z)].z + v[i].z;
                float zz_c = v[idx(x + 1, h, z)].y - v[i].y - v[idx(x, h + 1, z)].x + v[i].x;
                _cfdClipmap::curl[i] = { xx_c, yy_c, zz_c };
                _cfdClipmap::curl[i] *= oneOverSize;
                _cfdClipmap::mag[i] = glm::length(_cfdClipmap::curl[i]);
            }
        }
    }

    // -2 because last mag does not exists, or duplicate mag
    // Calculate Vorticty Direction From Curl Length Gradient, Cross and Apply Vortex Confinement Force -
    for (uint h = 0; h < height - 2; h++) {
        for (uint z = 0; z < width - 2; z++) {
            for (uint x = 0; x < width - 2; x++)
            {
                uint i = idx(x, h, z);
                float3 gradient = { _cfdClipmap::mag[idx(x + 1, h, z)] , _cfdClipmap::mag[idx(x, h + 1, z)] , _cfdClipmap::mag[idx(x, h, z + 1)] };  // gradient
                gradient -= _cfdClipmap::mag[i];
                //gradient *= oneOverSize;
                gradient += 0.000001f;
                _cfdClipmap::vorticity[i] = glm::normalize(gradient);

                float3 f = glm::cross(_cfdClipmap::vorticity[i], _cfdClipmap::curl[i]) * oneOverSize;

                v[i].xyz -= _scale * f * _dt;
            }
        }
    }
}
*/

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

    std::ofstream ofs;
    ofs.open(filename + ".sun.raw", std::ios::binary);
    if (ofs)
    {
        float3 sun = glm::normalize(float3(-0.4f, 0.6f, 0.5f));
        for (int i = 0; i < normals.size(); i++)
        {
            float3 N = normals[i].xyz;
            float V = glm::dot(N, sun);
            unsigned char val = (unsigned char)__max(0, V * 255.f);
            ofs.write((char*) & val, 1);
        }
        ofs.close();
    }
}


