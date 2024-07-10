#include "cfd.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <iostream>
#include <fstream>

using namespace std::chrono;
//#define GLM_FORCE_SSE2
//#define GLM_FORCE_ALIGNED


//#pragma optimize("", off)

std::vector<cfd_V_type> _cfd_lod::root_v;

float                   _cfdClipmap::incompres_relax = 1.6f;
int                     _cfdClipmap::incompres_loop = 5;
std::vector<cfd_V_type> _cfdClipmap::v_back;
std::vector<cfd_V_type> _cfdClipmap::v_bfecc;
std::vector<cfd_data_type> _cfdClipmap::data_back;
std::vector<cfd_data_type> _cfdClipmap::data_bfecc;


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

    // clear other here
    //memset(&other[0], 0, sizeof(float3) * width * width * height);
    float3 to_Root = offset;
    to_Root *= 0.5f;
    to_Root -= root->offset;

    for (uint h = 0; h < height; h++) {
        for (uint z = 0; z < width; z++) {
            for (uint x = 0; x < width; x++)
            {
                uint i = idx(x, h, z);
                v[i] = root->sample(root->v, float3(x, h, z) * 0.5f + to_Root);
                data[i] = root->sample(root->data, float3(x, h, z) * 0.5f + to_Root);
            }
        }
    }
}


void _cfd_lod::fromRoot_Alpha()
{
    if (!root)
    {
        for (uint h = 0; h < height; h++) {
            for (uint z = 0; z < width; z++) {
                for (uint x = 0; x < width; x++)
                {
                    float alpha = 0;
                    if (x < 4) alpha = 1;
                    //if (h < 2) alpha = 1; // dont do bottom
                    if (z < 4) alpha = 1;
                    if (width - x < 5) alpha = 1;
                    if (height - h < 5) alpha = 1;
                    if (width - z < 5) alpha = 1;

                    if (alpha == 1)
                    {
                        uint i = idx(x, h, z);
                        v[i] = _cfd_lod::root_v[i];
                        //data[i] = root->sample(root->data, _p);
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
                    /*
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
                    alpha = 1.f - alpha;


                    uint i = idx(x, h, z);
                    float3 _p = float3(x, h, z) * 0.5f + to_Root;
                    v[i] = glm::mix(v[i], root->sample(root->v, _p), alpha);
                    data[i] = glm::mix(data[i], root->sample(root->data, _p), alpha);
                    */

                    // 4 hard pixels
                    float alpha = 0;
                    if (x < 4) alpha = 1;
                    if (h < 4) alpha = 1;
                    if (z < 4) alpha = 1;
                    if (width - x < 5) alpha = 1;
                    if (height - h < 5) alpha = 1;
                    if (width - z < 5) alpha = 1;

                    if (alpha == 1)
                    {
                        uint i = idx(x, h, z);
                        float3 _p = float3(x, h, z) * 0.5f + to_Root;
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

    for (uint h = 4; h < height - 4; h+=2) {
        for (uint z = 4; z < width - 4; z+=2) {
            for (uint x = 4; x < width - 4; x+=2) {
                int3 iroot = { x, h, z };
                iroot += offset;
                iroot /= 2;
                iroot -= root->offset;

                uint i = idx(x, h, z);
                uint ri = root->idx(iroot.x, iroot.y, iroot.z);
                root->v[ri] = v[i];
                root->data[ri] = data[i];
            }
        }
    }
    /*
    for (uint h = 0; h < height; h += 2) {
        for (uint z = 0; z < width; z += 2) {
            for (uint x = 0; x < 4; x += 2) {
                int3 iroot = { x, h, z };
                iroot += offset;
                iroot /= 2;
                iroot -= root->offset;

                uint i = idx(x, h, z);
                uint ri = root->idx(iroot.x, iroot.y, iroot.z);
                root->v[ri] = float3(0, 0, 0);
                root->data[ri].z = 0.3f;
            }

            for (uint x = width-4; x < width; x += 2) {
                int3 iroot = { x, h, z };
                iroot += offset;
                iroot /= 2;
                iroot -= root->offset;

                uint i = idx(x, h, z);
                uint ri = root->idx(iroot.x, iroot.y, iroot.z);
                root->v[ri] = float3(0, 0, 0);
                root->data[ri].z = 0.3f;
            }
        }
    }
    */
    /*
    float3 to_Root = offset;
    to_Root *= 0.5f;
    to_Root -= root->offset;

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

                //float3 p = { x, h, z };
                //p += 0.5f;
                /*
                uint i = root->idx(x + to.x, h + to.y, z + to.z);
                float3 _p = float3(x, h, z) * 0.5f + to_Root;
                root->v[i] = glm::mix(root->v[i], topV, alpha);
                v[i] = glm::mix(v[i], root->sampleNew(root->v, _p), alpha);
                data[i] = glm::mix(data[i], root->sampleNew(root->data, _p), alpha);
                //* /
            }
        }
    }
    */
    
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
                    smax.x = smap.getS(int3(x + dX + 1, h, z + dY) + offset).x;
                    smax.y = smap.getS(int3(x + dX, h + 1, z + dY) + offset).y;
                    smax.z = smap.getS(int3(x + dX, h, z + dY + 1) + offset).z;
                    uint sum = smin.x + smin.y + smin.z + smax.x + smax.y + smax.z;



                    // this gets better if we split V and move to boundaries I think
                    // really look at it
                    if (sum > 1)
                    {

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

                        if (smax.y == 0) v[i] *= 0.f;
                    }
                    else
                    {
                        uint i = idx(x + dX, h, z + dY);
                        v[i] *= 0.f;
                        data[i] = data[idx(x + dX, h + 1, z + dY)];     // copy above, seems safest
                        data[i].x += cellSize * 0.0098; //+ dry lapse
                        //data[i].z = 0.2f;
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
                _cfdClipmap::v_back[i] = V / 1.03f;

                

                cfd_data_type d = data[i];
                d += data[idx(x - 1, h, z)] * 0.004f;
                d += data[idx(x + 1, h, z)] * 0.004f;
                d += data[idx(x, h - 1, z)] * 0.004f;
                d += data[idx(x, h + 1, z)] * 0.004f;
                d += data[idx(x, h, z - 1)] * 0.004f;
                d += data[idx(x, h, z + 1)] * 0.004f;
                _cfdClipmap::data_back[i] = d / 1.024f;
                
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
                    float rootAltitude = 350.f;   // in voxel space NIE AL WEER NIE
                    float alt = rootAltitude + cellSize * (h + offset.y);
                    //dtemp = -1.f * moistLapse(data[i].x, alt) * dh;
                }
                data[i].x = glm::lerp(data[i].x, _cfdClipmap::data_back[i].x, 0.5f); // normnla fo r temp
                data[i].x += dtemp;
            }
        }
    }
    

    maxSpeed = speed;
    maxStep = maxSpeed * scale;
};



void _cfd_lod::addTemperature()
{
    if (cellSize > 35) return;

    float rootAltitude = 350.f;   // in voxel space
    float3 t1 = float3(-1425, 425 + 140, 14533 - 2000);
    float s1 = 30.f + cellSize / 2;
    float s0 = 30.f + 10.f / 2;
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
                P.y += rootAltitude;
                P.z -= 20000;

                float l = glm::length(P - t1);
                if (l <5 * cellSize)
                {
                    float alpha = glm::smoothstep(0.5f, 1.5f, s1 / l);
                    data[idx(x, h, z)].z = __max(data[idx(x, h, z)].z, energy * alpha * 0.2f);

                    //data[idx(x, h, z)].x = __max(data[idx(x, h, z)].x, to_K(20 + energy * alpha * 15));
                    data[idx(x, h, z)].x = __max(data[idx(x, h, z)].x, to_K(26 + alpha * 2.05));

                    //data[idx(x, h, z)].y = 0.25 + alpha * 0.6f;// more humid
                }
            }
        }
    }
}


/*
//now correct the data
                float dewPoint = dew_Temp_C(data[i].y);
                // test wet adiabatic
                float dtemp = -0.0098f * dh;    // dry adiabatic lapse
                float dT_1Cell = -0.0098f * cellSize;
                if (dewPoint > to_C(data[i].x))
                {
                    float rootAltitude = 350.f;   // in voxel space NIE AL WEER NIE
                    float alt = rootAltitude + cellSize * (h + offset.y);
                    //dtemp = -1.f * moistLapse(data[i].x, alt) * dh;
                    //dT_1Cell = -1.f * moistLapse(data[i].x, alt) * cellSize;
                }




                //Now for chnage in vertical velocity
                // ----------------------------------------------------------------------------
                float deltaTemp = data[idx(x, h + 1, z)].x - (data[i].x + dT_1Cell);
                if (deltaTemp < 0)
                {
                    v[i].y -= _dt * deltaTemp / data[i].x * 9.8f;
                }

                if (h > 0)
                {
                    float deltaDown = data[idx(x, h - 1, z)].x - (data[i].x - dT_1Cell);
                    if (deltaDown > 0)
                    {
                       v[i].y -= _dt * deltaDown / data[i].x * 9.8f;
                    }
                }
*/
void _cfd_lod::bouyancy(float _dt)
{
    float rootAltitude = 350.f;   // in voxel space NIE AL WEER NIE

    // Pressure
    for (uint h = 0; h < height - 1; h++)
    {
        float alt = rootAltitude + cellSize * (h + offset.y);
        for (uint z = 0; z < width; z++)
        {
            for (uint x = 0; x < width; x++)
            {
                uint i = idx(x, h, z);
                //data[i].z = density(alt, data[i].x);
            }
        }
    }

    /*
    float rootAltitude = 350.f;   // in voxel space
    float rootTemperature = to_K(30.f); // lekker somersdag, dis op seevlak, dus omtrent 26 by waalenstad

    for (uint y = 0; y < 32; y++)
    {
        float altitude = rootAltitude + (y * lods[0].cellSize);
        float temperature = rootTemperature - (altitude * 8.2f / 1000.f);
    */

    float rootTemperature = to_K(30.f);

    for (uint h = 1; h < height-1; h++)
    {
        float alt = rootAltitude + cellSize * (h + offset.y);
        float baseTemperature = rootTemperature - (alt * 8.2f / 1000.f);

        for (uint z = 0; z < width; z++) {
            for (uint x = 0; x < width; x++)
            {
                uint i = idx(x, h, z);
                uint it = idx(x, h + 1, z);
                uint ib = idx(x, h - 1, z);

                float d0 = density(alt, baseTemperature);
                float d1 = density(alt, data[i].x);
                //float dW = (data[i].x - baseTemperature) / baseTemperature;
                v[i].y += (d0 - d1) / d0 * _dt * 9.8f;

                //baseTemperature / data[i].x;
                // BOUYANCY is the change om pressre w r t temperature, relatibe to the local environsment at the same height
                /*
                // Sort of but noqt qute
                // Better to use ave temp at an altitude, and just run this acordingly, its sort fo what the maths0 says
                // Maybe justm smaple temperature at a lower lod
                float alt = rootAltitude + cellSize * (h + offset.y);
                
                float pb = pressure_Pa(alt - cellSize / 2);
                float pt = pressure_Pa(alt - cellSize / 2);
                float d = density(alt, data[i].x);
                //float mass = d * cellSize * cellSize * cellSize;
                //float aP = (pb - pt) * cellSize * cellSize / mass;
                float aP = (pb - pt) / (d * cellSize) - 9.8f;

                v[i].y += aP * _dt;
                */

                /*
                float dewPoint = dew_Temp_C(data[i].y);
                // test wet adiabatic
                float dT_1Cell = -0.0098f * cellSize;
                if (dewPoint > to_C(data[i].x))
                {
                    
                    float alt = rootAltitude + cellSize * (h + offset.y);
                    dT_1Cell = -1.f * moistLapse(data[i].x, alt) * cellSize;
                }

                float diff = (data[it].x - (data[i].x + dT_1Cell)) / data[i].x;
                if (diff < 0)
                {
                    v[i].y -= diff * 9.8f * _dt;
                }

                diff = (data[ib].x - (data[i].x - dT_1Cell)) / data[i].x;
                if (diff > 0)
                {
                    //v[i].y -= diff * 9.8f * _dt;
                }
                */
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
    addTemperature();
    bouyancy(_dt);

    incompressibility_SMAP();                       //??? I think this should be last, leave the saves state as good as possible

    auto b = high_resolution_clock::now();
    //incompressibility_SMAP();                       //??? I think this should be last, leave the saves state as good as possible
    advect(_dt);        // just do smoke temperature etc as well

    auto c = high_resolution_clock::now();
    //edges();
    diffuse(_dt);

    // Plus MAyeb add vortocoty confine back in, I can see teh sue xacswe for it even with BFECC

    auto d = high_resolution_clock::now();
    

    auto e = high_resolution_clock::now();
    tickCount++;
    frameTime += _dt;

    simTimeLod_boyancy_ms = (double)duration_cast<microseconds>(b - a).count() / 1000.;
    simTimeLod_incompress_ms = (double)duration_cast<microseconds>(c - b).count() / 1000.;
    simTimeLod_edges_ms = (double)duration_cast<microseconds>(d - c).count() / 1000.;
    simTimeLod_advect_ms = (double)duration_cast<microseconds>(e - d).count() / 1000.;

    solveTime_ms = (float)duration_cast<microseconds>(e - a).count() / 1000.;

};






void _cfdClipmap::build(std::string _path)
{
    v_back.resize(128 * 128 * 128);     // maximum of any below or bigger
    v_bfecc.resize(128 * 128 * 128);
    data_back.resize(128 * 128 * 128);     // maximum of any below or bigger
    data_bfecc.resize(128 * 128 * 128);

    // normals instead of smap
    lods[0].init(128, 32, 40000, 1);
    lods[0].offset = { 0, 0, 0 };
    lods[0].smap.init(128);
    lods[0].smap.load(_path + "/S_128.bin");

    _cfd_lod::root_v.resize(128 * 128 * 32);

    lods[1].init(128, 64, 40000, 2);  // already clipped
    lods[1].offset = { 70, 0, 115 };
    lods[1].root = &lods[0];
    lods[1].smap.init(256);
    lods[1].smap.load(_path + "/S_256.bin");

    lods[2].init(128, 128, 40000, 4);
    lods[2].offset = lods[1].offset + int3(32, 0, 50);
    lods[2].offset *= 2;
    lods[2].root = &lods[1];
    lods[2].smap.init(512);
    lods[2].smap.load(_path + "/S_512.bin");

    lods[3].init(128, 128, 40000, 8);
    lods[3].offset = lods[2].offset + int3(42, 0, 50);
    lods[3].offset *= 2;
    lods[3].root = &lods[2];
    lods[3].smap.init(1024);
    lods[3].smap.load(_path + "/S_1024.bin");

    lods[4].init(128, 128, 40000, 16);
    lods[4].offset = lods[3].offset + int3(32, 0, 32);
    lods[4].offset *= 2;
    lods[4].root = &lods[3];
    lods[4].smap.init(2048);
    lods[4].smap.load(_path + "/S_2048.bin");

    lods[5].init(128, 128, 40000, 32);
    lods[5].offset = lods[4].offset + int3(32, 20, 32);
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
    cfd_data_type DATA;
    float rootAltitude = 350.f;   // in voxel space
    float rootTemperature = to_K(30.f); // lekker somersdag, dis op seevlak, dus omtrent 26 by waalenstad

    for (uint y = 0; y < 32; y++)
    {
        float altitude = rootAltitude + (y * lods[0].cellSize);
        float temperature = rootTemperature - (altitude * 8.2f / 1000.f);
        float relHumidity = 0.4f;
        if (altitude > 1100) relHumidity = 0.15f;
        if (altitude > 5200) relHumidity = 0.001f;

        DATA.x = temperature;
        DATA.y = relHumidity;
        DATA.z = 0;

        float3 wind = glm::mix(_bottom, _top, (float)y / 32.f);
        for (uint z = 0; z < 128; z++)
        {
            for (uint x = 0; x < 128; x++)
            {
                lods[0].setV(uint3(x, y, z), wind, DATA);
            }
        }
    }
}


void _cfdClipmap::simulate_start(float _dt)
{
    for (int k = 0; k < 2; k++)
    {
        lods[0].simulate(_dt);
        lods[0].timer = 0;
    }

    // make a backup of the root velocities for edges
    // later on feed this from windguru or yr, so actual source
    memcpy(_cfd_lod::root_v.data(), lods[0].v.data(), lods[0].width * lods[0].width * lods[0].height * sizeof(cfd_V_type));
    

    for (int lod = 1; lod < 6; lod++)
    {
        float time = _dt / pow(2, lod);
        lods[lod].fromRoot();
        //lods[lod].incompressibilityNormal(10);
        for (int k = 0; k < 0; k++)       lods[lod].simulate(time);
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

            //if (lod > 0)
            {
                
                lods[lod].simulate(time);
                lods[lod].toRoot_Alpha();
                if (lod < 5) {
                    lods[lod + 1].fromRoot_Alpha();
                }
            }

            auto stop = high_resolution_clock::now();

            if (lod == 5)
            {
                simTimeLod_ms = (double)duration_cast<microseconds>(stop - start).count() / 1000.;
            }

            // now copy the visualization data
            if (showSlice && lod == slicelod)
            {
                for (int y = 0; y < lods[lod].height; y++)
                {
                    for (int x = 0; x < 128; x++)
                    {
                        unsigned int r = 0, g = 0, b = 0, a = 255;

                        uint index = lods[lod].idx(x, y, sliceIndex);
                        uint i_t = index;
                        uint i_b = index;
                        if (y < lods[lod].height - 1)   i_t = lods[lod].idx(x, y + 1, sliceIndex);
                        if (y > 0)                      i_b = lods[lod].idx(x, y - 1, sliceIndex);

                        // Speed
                        float3 V = lods[lod].getV(index);
                        float speed = glm::length(V);
                        if (V.x > 0)    r = __min(255, (int)(V.x * 20.f));
                        else            b = __min(255, (int)(-V.x * 20.f));

                        // temperature
                        float3 data = lods[lod].data[index];
                        float dewPoint_C = dew_Temp_C(data.y);
                        float temp_C = to_C(data.x);
                        temp_C = temp_C - dewPoint_C;

                        r = (int)__max( 0, temp_C * 10.f);
                        g = ((int)(temp_C * 10)) % 20;
                        b = __min(255, (int)__max(0, -temp_C * 10.f));

                        if (temp_C < 0)
                        {
                            int cloudVal = 
                            r = 175;
                            g = 175;
                            b = 175;

                            if (to_C(data.x) < -2)
                            {
                                r = 205;
                                g = 205;
                                b = 205;
                            }
                        }


                        


                        //Relative temeperature and bouyancy
                        {
                            float rootAltitude = 350.f;   // in voxel space NIE AL WEER NIE
                            float alt = rootAltitude + lods[lod].cellSize * (y + lods[lod].offset.y);

                            float dewPoint = dew_Temp_C(data.y);
                            float dT_1Cell = -0.0098f * lods[lod].cellSize;
                            if (dewPoint > to_C(data.x))
                            {
                                dT_1Cell = -1.f * moistLapse(data.x, alt) * lods[lod].cellSize;
                            }
                            //float deltaTemp = lods[lod].data[lods[lod].idx(x, y + 1, sliceIndex)].x - (data.x + dT_1Cell);
                            float3 dataAbove = lods[lod].data[i_t];
                            float3 dataBelow = lods[lod].data[i_b];
                            float deltaTemp =  data.x + dT_1Cell - dataAbove.x;

                            float K_above = dataAbove.x;
                            float K_lapse = data.x + dT_1Cell;

                            r = 0;
                            g = 0;
                            b = 0;

                            // density
                            float _x = lods[lod].cellSize;
                            float pressurePa = pressure_Pa(alt);                // error dry only
                            float p_B = pressure_Pa(alt - _x * 0.5f);
                            float p_T = pressure_Pa(alt + _x * 0.5f);
                            float fP = (p_B - p_T) * _x * _x;

                            float mass = data.z * _x * _x * _x;
                            float fG =  mass * 9.8;

                            float fTotal = fP - fG;

                            float aPres = fP / mass;
                            float aGrav = 9.8f;

                            // assume base temp is perfectly in balance
                            float baseTemperature = to_K(30.f) - (alt * 8.2f / 1000.f);
                            float dW = (data.x - baseTemperature) / baseTemperature;


                            float a = dW;//aPres - aGrav;
                            //if (a > 0)  r = (int)__max(0, a * 100 * 255) % 255;
                            //else        b = (int)__max(0, -a * 100 * 255) % 255;

                            //if (a < 10000) b = 100;
                            //if (a > 14000) g = 100;

                            float dense = data.z;// density(alt, data.x);

                            //g = (int)__max(0, dense / 1.4f * 100 * 255) % 255;
                            //if (pressurePa > 96000) r = 100;
                            //if (alt > 457) b = 100;

                            float top = (data.x + dT_1Cell) - dataAbove.x;
                            float bottom = (data.x - dT_1Cell) - dataBelow.x;
                             
                            float lift = top - bottom;// +bottom;

                            //if (lift > 0)    r = (int)__max(0, lift * 1 * 255) % 255;
                            //else             b = (int)__max(0, -lift * 1 * 255) % 255;
                            if (K_lapse > K_above)
                            {
                                //r = (int)__max(0, deltaTemp * 1 * 255) % 255;
                            }

                            if (dataAbove.x > data.x)
                            {
                                //b = (int)__max(0, -deltaTemp * 100 * 255);
                            }

                            {
                                float temp_C = to_C(data.x);
                                //temp_C = temp_C - dewPoint_C;

                                g = (((int)(temp_C * 0.5 * 256)) % 255) / 20;
                                //b = 100;
                            }

                            deltaTemp = -deltaTemp / data.x;

                            //r = (int)__max(0, deltaTemp * 100 * 255);
                            //g = 0;
                            //b = (int)__max(0, -deltaTemp * 100 * 255);

                            // NEW boyancy

                            /*
                                

                                diff = (data[ib].x - (data[i].x - dT_1Cell)) / data[i].x;
                                if (diff > 0)
                                {
                                    //v[i].y -= diff * 9.8f * _dt;
                                }
                            */
                        }

                        //Smoke
                        
                        if (data.z > 0.001f)
                        {
                            
                            float alpha = 1.f - pow(data.z, 0.9);
                            alpha = __max(alpha, 0.1);
                            alpha = __min(alpha, 1);
                            r = (int)(r * alpha);
                            g = (int)(g * alpha);
                            b = (int)(b * alpha);

                            int a = (int)(60 * (1.f - alpha));
                            r += a;
                            g += a;
                            b += a;
                            
                        }
                        

                        
                        // clouds
                        float temp_C2 = to_C(data.x);
                        float dewPoint = dew_Temp_C(data.y);
                        float diff = temp_C2 - dewPoint;

                        if (dewPoint > temp_C2)
                        {
                            int max = 180;
                            if (temp_C < -5) max = 220;
                            float scale = __min(1, (dewPoint - temp_C2) / 2.f);
                            max = (int)(max * scale);

                            r = __max(r, max);
                            g = __max(g, max);
                            b = __max(b, max);
                        }
                        
                        /*
                        g = (int)(V.y * 255.f);
                        b = (int)(V.z * 255.f);
                        a = 255;// (int)(__min(1.f, __max(0, speed - 14.f)) * 255.f);
                        r = (int)V.x;
                        g = 0;
                        b = 0;
                        */
                        uint idx = (y * 128 + x);
                        arrayVisualize[idx] = (a << 24) + (b << 16) + (g << 8) + r;
                    }
                }
            }
        }
    }

    counter++;
}

//#pragma optimize("", off)
void _cfdClipmap::streamlines(float3 _p, float4* _data, float3 right)
{
    float rootAltitude = 350.f;   // in voxel space
    float3 origin = float3(-20000, rootAltitude, -20000);

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
    float rootAltitude = 350.f;   // in voxel space
    float3 world_origin = float3(-20000, rootAltitude, -20000);
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
                curl[i] = { xx_c, yy_c, zz_c };
                curl[i] *= oneOverSize;
                mag[i] = glm::length(curl[i]);
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
                float3 gradient = { mag[idx(x + 1, h, z)] , mag[idx(x, h + 1, z)] , mag[idx(x, h, z + 1)] };  // gradient
                gradient -= mag[i];
                //gradient *= oneOverSize;
                gradient += 0.000001f;
                vorticity[i] = glm::normalize(gradient);

                float3 f = glm::cross(vorticity[i], curl[i]) * oneOverSize;

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
}


