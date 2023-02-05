#include "roadNetwork.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "assimp/scene.h"
#include "assimp/Exporter.hpp"
using namespace Assimp;

//#pragma optimize( "", off )



static bool showMaterials = false;
//roadMaterialCache roadNetwork::roadMatCache;
Texture::SharedPtr roadNetwork::displayThumbnail;





/*  bezierLayer
    FIXME this may well flip stuf but mte to double bezier first then fix
    --------------------------------------------------------------------------------------------------------------------*/
bezierLayer::bezierLayer(bezier_edge inner, bezier_edge outer, uint _material, uint bezierIndex, float w0, float w1, bool startSeg, bool endSeg)
{
    if (bezierIndex == 0) {
        bool bCM = true;
    }
    A = 0;
    B = 0;

    A = (inner << 31) + (outer << 30);
    // leaves 2 bits free for now

    // max 4095 materials
    A += (_material & 0x7ff) << 17;
    A += bezierIndex & 0x1ffff;

    //-16 to plus 16m in 2mm jumps
    w0 += 16.0f;
    w1 += 16.0f;
    w0 = __min(32.0f, __max(0.0f, w0));
    w1 = __min(32.0f, __max(0.0f, w1));
    B = (((int)(w0 * 500.0f)) << 14) + ((int)(w1 * 500.0f));

    if (startSeg)	B += 1 << 31;
    if (endSeg)		B += 1 << 30;
}



/*  plane_2d
    --------------------------------------------------------------------------------------------------------------------*/
void plane_2d::frompoints(glm::vec3 A, glm::vec3 B) {

    glm::vec3 perpendicular = B - A;
    norm = glm::normalize(glm::vec2(perpendicular.z, -perpendicular.x));
    dst = glm::dot(glm::vec2(A.x, A.z), norm);
}



float plane_2d::distance(glm::vec2 P) {
    return(glm::dot(P, norm) - dst);
}



/*  physicsBezier
*   FIXME should become vec4 with the distance included
    --------------------------------------------------------------------------------------------------------------------*/
physicsBezier::physicsBezier(splinePoint a, splinePoint b, bool bRight, uint _guid, uint _index, bool _fullWidth)
{
    bRighthand = bRight;
    roadGUID = _guid;
    index = _index;

    layers.clear();

    if (_fullWidth)
    {
        glm::vec3 C = (a.bezier[left].pos + a.bezier[right].pos + b.bezier[left].pos + b.bezier[right].pos) * 0.25f;
        float dA = glm::length(C - a.bezier[left].pos);
        float dB = glm::length(C - a.bezier[right].pos);
        float dC = glm::length(C - b.bezier[left].pos);
        float dD = glm::length(C - b.bezier[right].pos);
        float dst = __max(__max(dA, dB), __max(dC, dD)) * 1.3f;	// max and scale bigger for outer blends  1.3 is thumbsuck

        center = glm::vec2(C.x, C.z);
        radius = dst;
        //		radiusSquare = dst*dst;
        startPlane.frompoints(a.bezier[left].pos, a.bezier[right].pos);
        endPlane.frompoints(b.bezier[left].pos, b.bezier[right].pos);

        // unlike for the GPU we epand it here with dupicates. Cache miss is far more critical than trying to save some memory, unlike on the GPU
        // that has fantastic cache for the vertex pipeline
        bezier[0][0] = glm::vec4(a.bezier[left].pos, 0);
        bezier[0][1] = glm::vec4(a.bezier[left].forward(), 0);
        bezier[0][2] = glm::vec4(b.bezier[left].backward(), 0);
        bezier[0][3] = glm::vec4(b.bezier[left].pos, 0);

        bezier[1][0] = glm::vec4(a.bezier[right].pos, 0);
        bezier[1][1] = glm::vec4(a.bezier[right].forward(), 0);
        bezier[1][2] = glm::vec4(b.bezier[right].backward(), 0);
        bezier[1][3] = glm::vec4(b.bezier[right].pos, 0);

        layers.clear();
    }
    else if (bRight)
    {
        glm::vec3 C = (a.bezier[middle].pos + a.bezier[right].pos + b.bezier[middle].pos + b.bezier[right].pos) * 0.25f;
        float dA = glm::length(C - a.bezier[middle].pos);
        float dB = glm::length(C - a.bezier[right].pos);
        float dC = glm::length(C - b.bezier[middle].pos);
        float dD = glm::length(C - b.bezier[right].pos);
        float dst = __max(__max(dA, dB), __max(dC, dD)) * 1.3f;	// max and scale bigger for outer blends  1.3 is thumbsuck

        center = glm::vec2(C.x, C.z);
        radius = dst;
        //		radiusSquare = dst*dst;
        startPlane.frompoints(a.bezier[middle].pos, a.bezier[right].pos);
        endPlane.frompoints(b.bezier[middle].pos, b.bezier[right].pos);

        // unlike for the GPU we epand it here with dupicates. Cache miss is far more critical than trying to save some memory, unlike on the GPU
        // that has fantastic cache for the vertex pipeline
        bezier[0][0] = glm::vec4(a.bezier[middle].pos, 0);
        bezier[0][1] = glm::vec4(a.bezier[middle].forward(), 0);
        bezier[0][2] = glm::vec4(b.bezier[middle].backward(), 0);
        bezier[0][3] = glm::vec4(b.bezier[middle].pos, 0);

        bezier[1][0] = glm::vec4(a.bezier[right].pos, 0);
        bezier[1][1] = glm::vec4(a.bezier[right].forward(), 0);
        bezier[1][2] = glm::vec4(b.bezier[right].backward(), 0);
        bezier[1][3] = glm::vec4(b.bezier[right].pos, 0);

        layers.clear();
    }
    else
    {
        glm::vec3 C = (a.bezier[middle].pos + a.bezier[left].pos + b.bezier[middle].pos + b.bezier[left].pos) * 0.25f;
        float dA = glm::length(C - a.bezier[middle].pos);
        float dB = glm::length(C - a.bezier[left].pos);
        float dC = glm::length(C - b.bezier[middle].pos);
        float dD = glm::length(C - b.bezier[left].pos);
        float dst = __max(__max(dA, dB), __max(dC, dD)) * 1.3f;	// max and scale bigger for outer blends  1.3 is thumbsuck

        center = glm::vec2(C.x, C.z);
        radius = dst;
        //		radiusSquare = dst*dst;
        startPlane.frompoints(a.bezier[middle].pos, a.bezier[left].pos);
        endPlane.frompoints(b.bezier[middle].pos, b.bezier[left].pos);

        // unlike for the GPU we epand it here with dupicates. Cache miss is far more critical than trying to save some memory
        bezier[0][0] = glm::vec4(a.bezier[middle].pos, 0);
        bezier[0][1] = glm::vec4(a.bezier[middle].backward(), 0);
        bezier[0][2] = glm::vec4(b.bezier[middle].forward(), 0);
        bezier[0][3] = glm::vec4(b.bezier[middle].pos, 0);

        bezier[1][0] = glm::vec4(a.bezier[left].pos, 0);
        bezier[1][1] = glm::vec4(a.bezier[left].backward(), 0);
        bezier[1][2] = glm::vec4(b.bezier[left].forward(), 0);
        bezier[1][3] = glm::vec4(b.bezier[left].pos, 0);

        layers.clear();
    }
}



void physicsBezier::addLayer(bezier_edge inner, bezier_edge outer, uint material, float w0, float w1)
{
    physicsBezierLayer layer;
    layer.material = material;

    // translate w0 and w1 into absolute values
    // FIXME dink nog n keer hieroor, begin eers lookup skryf, kyk dan weer
    //layer.vOffset = 0;
    //layer.vScale = 1;
    layer.edge[0] = inner;
    layer.edge[1] = outer;
    layer.offset[0] = w0;
    layer.offset[1] = w1;

    layers.push_back(layer);
}



void physicsBezier::binary_import(FILE* file)
{
    fread(&center, 1, sizeof(glm::vec2), file);
    fread(&radius, 1, sizeof(float), file);
    fread(&startPlane, 1, sizeof(plane_2d), file);
    fread(&endPlane, 1, sizeof(plane_2d), file);
    fread(&cacheIndex, 1, sizeof(int), file);
    fread(&bezier[0][0], 2 * 4, sizeof(glm::vec4), file);

    uint numL;
    fread(&numL, 1, sizeof(uint), file);
    layers.clear();
    for (uint i = 0; i < numL; i++) {
        physicsBezierLayer layer;
        fread(&layer, 1, sizeof(physicsBezierLayer), file);
        layers.push_back(layer);
    }
}



void physicsBezier::binary_export(FILE* file)
{
    fwrite(&center, 1, sizeof(glm::vec2), file);
    fwrite(&radius, 1, sizeof(float), file);
    fwrite(&startPlane, 1, sizeof(plane_2d), file);
    fwrite(&endPlane, 1, sizeof(plane_2d), file);
    fwrite(&cacheIndex, 1, sizeof(int), file);
    fwrite(&bezier[0][0], 2 * 4, sizeof(glm::vec4), file);

    uint numL = (uint)layers.size();
    fwrite(&numL, 1, sizeof(uint), file);
    for (uint i = 0; i < numL; i++) {
        fwrite(&layers[i], 1, sizeof(physicsBezierLayer), file);
    }
}


std::vector<physicsBezierLayer> layers;




/*  bezierCache
    --------------------------------------------------------------------------------------------------------------------*/
uint bezierCache::findEmpty()
{
    uint maxAge = 0;
    uint slot = 0;
    for (int i = 0; i < cache.size(); i++) {
        if (cache[i].counter > maxAge) {
            slot = i;
            maxAge = cache[i].counter;
        }
    }
    return slot;
}



void bezierCache::solveStats()
{
}







/*	Figure out if its already cached or cache is not
    what the hell does everything point at ??? ;-) */
void bezierCache::cacheSpline(physicsBezier* pBezier, uint cacheSlot)
{
    pBezier->cacheIndex = cacheSlot;

    cache[cacheSlot].counter = 0;

    for (int i = 0; i <= numBezierCache; i++) {

        float t = (float)i / (float)numBezierCache;
        cache[cacheSlot].points[i].A = cubic_Casteljau(t, pBezier->bezier[0][0], pBezier->bezier[0][1], pBezier->bezier[0][2], pBezier->bezier[0][3]);
        cache[cacheSlot].points[i].B = cubic_Casteljau(t, pBezier->bezier[1][0], pBezier->bezier[1][1], pBezier->bezier[1][2], pBezier->bezier[1][3]);
        cache[cacheSlot].points[i].planeTangent.frompoints(cache[cacheSlot].points[i].A, cache[cacheSlot].points[i].B);
    }

    for (int i = 0; i < numBezierCache; i++) {
        cache[cacheSlot].points[i].planeInner.frompoints(cache[cacheSlot].points[i + 1].A, cache[cacheSlot].points[i].A);
        cache[cacheSlot].points[i].planeOuter.frompoints(cache[cacheSlot].points[i + 1].B, cache[cacheSlot].points[i].B);
    }
}



void bezierCache::increment()
{
    for (auto& item : cache) {
        item.counter++;
    }
}



void bezierCache::clear()
{
    for (auto& item : cache) {
        item.bezierIndex = -1;
        item.counter = 10000;
    }
}



/*  bezierOneIntersection
    --------------------------------------------------------------------------------------------------------------------*/
bezierOneIntersection::bezierOneIntersection(glm::vec2 P, uint boundingIndex)
{
    pos = P;
    boundIndex = boundingIndex;
    bHit = false;
    distanceTillNextSearch = 0;
    cacheIndex = -1;
    t_idx = 0;
}



void bezierOneIntersection::updatePosition(glm::vec2 P, float distanceTraveled)
{
    distanceTillNextSearch -= distanceTraveled;
    pos = P;
}



/*  bezierIntersection
    --------------------------------------------------------------------------------------------------------------------*/
void bezierIntersection::updatePosition(glm::vec2 P)
{
    float distanceTraveled = glm::length(pos - P);
    pos = P;
    for (auto& patch : beziers) {
        patch.updatePosition(P, distanceTraveled);
    }
}




/*  bezierFastGrid
    --------------------------------------------------------------------------------------------------------------------*/
void bezierFastGrid::allocate(float sz, uint numX, uint numY)
{
    size = sz;
    nX = numX;
    nY = numY;
    offset = int2(nX >> 1, nY >> 1);
    //lookup.resize(nX * nY);
    //data.reserve(); maybe not, too hard to guess, messes with binary load

    buidData.resize(nX * nY);
    maxHash = (nX * nY) - 1;
}



void bezierFastGrid::populateBezier()
{
}



/*	For performance reasons this does not do bounds checking
    In practive we should always be in bounds in the game, and getBlockLookup does include bounds checks, so a bad value here is
    of no particular consequince*/
inline uint bezierFastGrid::getHash(glm::vec2 pos)
{
    return ((int)floor((pos.y) / size + offset.y) * nX) + (int)floor((pos.x) / size + offset.x);
}



std::vector<uint> bezierFastGrid::getBlockLookup(uint hash)
{
    return buidData[__min(hash, maxHash)];
}



// FIXME this selightly overestimate by doeing AABB instead of sphere but would need to return a list then
void bezierFastGrid::insertBezier(glm::vec2 pos, float radius, uint index)
{
    int2 min, max;
    min.x = __max(0, (int)floor((pos.x - radius) / size + offset.x));
    min.y = __max(0, (int)floor((pos.y - radius) / size + offset.y));

    max.x = __min((int)nX - 1, (int)floor((pos.x + radius) / size + offset.x));
    max.y = __min((int)nY - 1, (int)floor((pos.y + radius) / size + offset.y));

    for (int y = min.y; y <= max.y; y++)
    {
        for (int x = min.x; x <= max.x; x++)
        {
            buidData[y * nX + x].push_back(index);
        }
    }

}



void bezierFastGrid::binary_import(FILE* file)
{
    fread(&size, 1, sizeof(float), file);
    fread(&offset, 1, sizeof(int2), file);
    fread(&nX, 1, sizeof(uint), file);
    fread(&nY, 1, sizeof(uint), file);
    fread(&maxHash, 1, sizeof(uint), file);

    buidData.resize(nX * nY);
    for (uint y = 0; y < (nX * nY); y++) {
        uint numBezier;
        fread(&numBezier, 1, sizeof(uint), file);
        buidData[y].clear();
        buidData[y].resize(numBezier);
        for (uint x = 0; x < numBezier; x++) {
            uint bezierIndex;
            fread(&bezierIndex, 1, sizeof(uint), file);
            buidData[y][x] = bezierIndex;
        }
    }
}



void bezierFastGrid::binary_export(FILE* file)
{
    fwrite(&size, 1, sizeof(float), file);
    fwrite(&offset, 1, sizeof(int2), file);
    fwrite(&nX, 1, sizeof(uint), file);
    fwrite(&nY, 1, sizeof(uint), file);
    fwrite(&maxHash, 1, sizeof(uint), file);

    uint datasize = nX * nY;// (uint)buidData.size();
    for (uint y = 0; y < datasize; y++) {
        uint numBezier = (uint)buidData[y].size();
        fwrite(&numBezier, 1, sizeof(uint), file);
        for (uint x = 0; x < numBezier; x++) {
            fwrite(&buidData[y][x], 1, sizeof(uint), file);
        }
    }
}



/*  ODE_bezier
    --------------------------------------------------------------------------------------------------------------------*/
void ODE_bezier::intersect(bezierIntersection* pI)
{
    cache.increment();

    // First check the cell-hash to see if we crossed boundaries
    // It would be faster if we can see which beziers exists in both and transfer the existing solution over, but for nwo this should be fine
    {
        //		PROFILE(grid);
        if (gridLookup.getHash(pI->pos) != pI->cellHash) {
            pI->cellHash = gridLookup.getHash(pI->pos);
            pI->beziers.clear();

            std::vector<uint> cell = gridLookup.getBlockLookup(pI->cellHash);
            for (uint bezierIndex : cell) {
                pI->beziers.push_back(bezierOneIntersection(pI->pos, bezierIndex));		// I am still not sure this is fast enough, look at array<> again
            }
        }
    }

    {
        //		PROFILE(intersectBezier);
        pI->numHits = 0;
        for (auto& bezier : pI->beziers)
        {
            if (bezier.bHit) {
                //if (needsReCache)
                {
                    // this bloxk re-cache the data
                    physicsBezier* pPhysics = &bezierBounding[bezier.boundIndex];
                    cache.cacheSpline(pPhysics, bezier.cacheIndex);
                }
                pI->numHits += intersectBezier(&bezier);
            }
            else if (bezier.distanceTillNextSearch <= 0) {
                if (testBounds(&bezier)) {
                    pI->numHits += intersectBezier(&bezier);
                }
            }
        }
    }

    needsReCache = false;

    // Now sum and merge
    {
        //		PROFILE(sumandmerge);
        for (auto& bezier : pI->beziers)
        {
            if (bezier.bHit)
            {
                physicsBezier* pPhysics = &bezierBounding[bezier.boundIndex];
                for (auto& layer : pPhysics->layers)
                {
                    float V0 = 0;	// FXIME FO nicely with arrays
                    float V1 = 0;
                    if (layer.edge[0] == bezier_edge::center) {
                        V0 = bezier.d0 - layer.offset[0];
                    }
                    else {
                        V0 = bezier.d1 - layer.offset[0];
                    }

                    if (layer.edge[1] == bezier_edge::center) {
                        V1 = bezier.d0 - layer.offset[1];
                    }
                    else {
                        V1 = bezier.d1 - layer.offset[1];
                    }

                    float V = V0 / (V0 - V1);

                    if (V >= 0 && V <= 1.0f)
                    {
                        // FIXME BLEND WITH ALPHA
                        pI->height = bezier.bezierHeight;

                        switch (layer.material) {
                        case 0:
                            pI->grip = 1.0f;
                            break;
                        case 1:
                            pI->grip = 0.6f;
                            break;
                        case 2:
                            pI->grip = 0.3f;
                            break;
                        }
                    }
                }
            }
        }
    }

}



bool ODE_bezier::intersectBezier(bezierOneIntersection* pI)
{
    if (pI->cacheIndex < 0) return false;

    //	PROFILE(intersectBezier);

    float d0 = 0;
    float d1 = 0;

    cacheItem* C = &cache.cache[pI->cacheIndex];	// fixme more likely an iterator is better
    C->counter = 0;

    d0 = C->points[pI->t_idx].planeTangent.distance(pI->pos);
    while ((pI->t_idx > -1) && (d0 < 0.0f)) {
        pI->t_idx--;
        d0 = C->points[pI->t_idx].planeTangent.distance(pI->pos);
    };

    if (pI->t_idx >= 0)
    {
        // forward
        d1 = C->points[pI->t_idx + 1].planeTangent.distance(pI->pos);
        while ((pI->t_idx <= numBezierCache) && d1 >= 0.0f) {
            pI->t_idx++;
            d0 = d1;
            d1 = C->points[pI->t_idx + 1].planeTangent.distance(pI->pos);
        };
    }

    if ((pI->t_idx >= 0) && (pI->t_idx < numBezierCache))
    {
        // ok we pass the tangent plane test for this trapesium
        pI->d0 = C->points[pI->t_idx].planeInner.distance(pI->pos);
        pI->d1 = C->points[pI->t_idx].planeOuter.distance(pI->pos);

        if ((pI->d0 > 0) && (pI->d1 < 0.0f))
        {
            pI->UV.x = d0 / (d0 - d1); // since d1 is negative
            pI->UV.y = pI->d0 / (pI->d0 - pI->d1);
            pI->bHit = true;

            float h0 = C->points[pI->t_idx].A.y * (1.0f - pI->UV.x) + (C->points[pI->t_idx + 1].A.y) * pI->UV.x;
            float h1 = C->points[pI->t_idx].B.y * (1.0f - pI->UV.x) + (C->points[pI->t_idx + 1].B.y) * pI->UV.x;
            pI->bezierHeight = h0 * (1.0f - pI->UV.y) + h1 * pI->UV.y;

            pI->bRighthand = bezierBounding[C->bezierIndex].bRighthand;
            pI->roadGUID = bezierBounding[C->bezierIndex].roadGUID;
            pI->index = bezierBounding[C->bezierIndex].index;

            pI->UV.x = (pI->UV.x + pI->t_idx) / numBezierCache; // NOW FIX it up for the whole bezier

            return true;
        }
        else
        {
            pI->distanceTillNextSearch = __max(-pI->d0, pI->d1 - 10.0f);
            pI->bHit = false;
            return false;
        }
    }
    else
    {
        pI->bHit = false;
        pI->distanceTillNextSearch = __max(-d0, d1);
        return false;
    }
}



bool ODE_bezier::testBounds(bezierOneIntersection* pI)
{
    physicsBezier* pPhysics = &bezierBounding[pI->boundIndex];

    glm::vec2 diff = pI->pos - pPhysics->center;
    float length = glm::length(diff) - pPhysics->radius;

    if (length < 0)
    {
        float a = pPhysics->startPlane.distance(pI->pos);
        float b = pPhysics->endPlane.distance(pI->pos);
        if ((a >= 0) && (b < 0))
        {

            // ist true so make sure its cached
            {
                if (pPhysics->cacheIndex < 0) {
                    uint cacheSlot = cache.findEmpty();
                    // now break all teh existing links
                    if (cache.cache[cacheSlot].bezierIndex >= 0) {
                        bezierBounding[cache.cache[cacheSlot].bezierIndex].cacheIndex = -1;
                    }

                    // set new link and cache
                    cache.cache[cacheSlot].bezierIndex = pI->boundIndex;
                    cache.cacheSpline(pPhysics, cacheSlot);
                }
                pI->cacheIndex = pPhysics->cacheIndex;
                pI->t_idx = 0;
            }
            return true;
        }
        else
        {
            pI->distanceTillNextSearch = __max(-a, b);
            return false;
        }
    }
    else
    {
        pI->distanceTillNextSearch = length;		// this is for missed the bounding circle
        return false;
    }

}



void ODE_bezier::blendLayers(bezierIntersection* pI)
{
}



void ODE_bezier::setGrid(float size, uint numX, uint numY)
{
    gridLookup.allocate(size, numX, numY);
}



void ODE_bezier::clear()
{
    bezierBounding.clear();
    //cache.clear();
    needsReCache = true;
}



void ODE_bezier::buildGridFromBezier()
{
    for (auto& cell : gridLookup.buidData) {
        cell.clear();
    }
    //gridLookup.buidData.clear();
    uint size = (uint)bezierBounding.size();
    for (uint i = 0; i < size; i++)
    {
        gridLookup.insertBezier(bezierBounding[i].center, bezierBounding[i].radius, i);
    }
}




/*  aiIntersection
    --------------------------------------------------------------------------------------------------------------------*/
void aiIntersection::setPos(glm::vec2 _p)
{
    glm::vec2 delta = _p - pos;

    float l = glm::length(delta);
    if (l > 0) {
        dir = glm::normalize(delta);
        delDistance += l;
        pos = _p;
    }
}



/*  AI_bezier
    --------------------------------------------------------------------------------------------------------------------*/
float cross2(glm::vec2 a, glm::vec2 b) {
    return a.x * b.y - b.x * a.y;
}



void AI_bezier::intersect(aiIntersection* _pAI)
{
    if (segments.size() > 0)
    {
        //_pAI->segment = __min(_pAI->segment, (uint)segments.size());
        float d0 = 0.0f;
        float d1 = 0.0f;

        if (!_pAI->bHit)
        {
            _pAI->segment = (uint)floor(rootsearch(_pAI->pos));
            _pAI->dist_Left = segments[_pAI->segment].planeInner.distance(_pAI->pos);
            _pAI->dist_Right = segments[_pAI->segment].planeOuter.distance(_pAI->pos);
        }

        // move it all one along
        if (_pAI->segment > 0 && _pAI->delDistance > segments[_pAI->segment].length && _pAI->dist_Left > -2.0f && _pAI->dist_Right < 2.0f)
        {
            _pAI->delDistance = 0.0f;
            uint numMoves = 0;

            // acurate position
            d0 = segments[_pAI->segment].planeTangent.distance(_pAI->pos);
            if (d0 < 0.0f && _pAI->segment > 0) {
                _pAI->segment--;
                d1 = d0;
                d0 = segments[_pAI->segment].planeTangent.distance(_pAI->pos);
            }

            d1 = segments[_pAI->segment + 1].planeTangent.distance(_pAI->pos);
            if (d1 >= 0.0f && _pAI->segment < segments.size() - 2) {
                _pAI->segment++;
                d0 = d1;
                d1 = segments[_pAI->segment + 1].planeTangent.distance(_pAI->pos);
            }

        }
        else {
            d0 = segments[_pAI->segment].planeTangent.distance(_pAI->pos);
            d1 = segments[_pAI->segment + 1].planeTangent.distance(_pAI->pos);
        }

        if (_pAI->segment > 0)
        {
            _pAI->dist_Left = segments[_pAI->segment].planeInner.distance(_pAI->pos);
            _pAI->dist_Right = segments[_pAI->segment].planeOuter.distance(_pAI->pos);

            if (_pAI->dist_Left > -2.0f && _pAI->dist_Right < 2.0f)
            {
                _pAI->bHit = true;
                _pAI->distance = segments[_pAI->segment].distance - start;

                _pAI->road_angle = cross2(_pAI->dir, segments[_pAI->segment].planeTangent.norm);

                _pAI->camber[0] = segments[_pAI->segment].camber;

                // NOW for lookahead
                // just pretend car is 2m wide for the moment
                /*
                uint apexCounter;
                uint maxLook = __min(100, segments.size() - _pAI->segment - 1);

                _pAI->lookahead_curvature = 0;
                _pAI->lookahead_camber = 0;
                for (uint i = 0; i < maxLook; i++)
                {
                    _pAI->lookahead_curvature += segments[_pAI->segment + i].camber;
                    _pAI->lookahead_camber += segments[_pAI->segment + i].camber;
                    _pAI->lookahead_apex_distance;
                }
                _pAI->lookahead_apex_left;
                _pAI->lookahead_apex_camber;
                _pAI->lookahead_apex_curvature
                */
            }
            else
            {
                _pAI->bHit = false;
            }

        }

    }
    else
    {
        _pAI->bHit = false;
    }
}



float AI_bezier::rootsearch(glm::vec2 _pos)
{
    float d0 = segments[0].planeTangent.distance(_pos);

    for (uint i = 1; i < segments.size(); i++) {
        float d1 = segments[i].planeTangent.distance(_pos);
        if (d0 >= 0 && d1 < 0)
        {

            if ((segments[i].planeInner.distance(_pos) > -2) && (segments[i].planeOuter.distance(_pos) < 2.0f))
            {
                return (float)i + (d0 / (d0 - d1));
            }
        }
        d0 = d1;
    }

    return 0.0f;
}



void AI_bezier::clear()
{
    segments.clear();

    bezierPatches.clear();
    bezierPatches.reserve(64);

    patchSegments.clear();
}



void AI_bezier::solveStartEnd()
{
    float startIndex = rootsearch(glm::vec2(startpos.x, startpos.z));
    if (startIndex != 0.0f)
    {
        uint idx = (uint)floor(startIndex);
        startIdx = idx;
        float frac_A = glm::fract(startIndex);
        float frac_B = 1.0f - frac_A;
        start = segments[idx].distance * frac_B + segments[idx + 1].distance * frac_A;
    }

    float endIndex = rootsearch(glm::vec2(endpos.x, endpos.z));
    if (endIndex != 0.0f)
    {
        uint idx = (uint)floor(endIndex);
        endIdx = idx;
        float frac_A = glm::fract(endIndex);
        float frac_B = 1.0f - frac_A;
        end = segments[idx].distance * frac_B + segments[idx + 1].distance * frac_A;
    }
}



// fix me split push from cacheAll();
void AI_bezier::pushSegment(glm::vec4 bezier[2][4])
{
    cubicDouble B;
    memcpy(B.data, bezier, sizeof(glm::vec4) * 2 * 4);
    bezierPatches.push_back(B);
}



void AI_bezier::cacheAll()
{
    // first the patches themselves
    numBezier = (uint)bezierPatches.size() - 1;
    segments.reserve(numBezier * 16);
    float stepSize = 1000;	// just big to begin

    for (uint i = 0; i <= numBezier; i++)
    {
        bezierSegment patch;
        patch.A = bezierPatches[i].data[0][0];
        patch.B = bezierPatches[i].data[1][0];
        patch.planeTangent.frompoints(patch.A, patch.B);

        patchSegments.push_back(patch);
    }

    for (uint i = 0; i < numBezier; i++)
    {
        patchSegments[i].planeInner.frompoints(patchSegments[i + 1].A, patchSegments[i].A);
        patchSegments[i].planeOuter.frompoints(patchSegments[i + 1].B, patchSegments[i].B);
        float width = glm::length(patchSegments[i].B - patchSegments[i].A);
        patchSegments[i].camber = atan2(patchSegments[i].A.y - patchSegments[i].B.y, width);	// see if there is faster way
        /*
        float camberFast = (patchSegments[i].A.y - patchSegments[i].B.y) / width;
        float errorPersent = fabs(camberFast - patchSegments[i].camber) / patchSegments[i].camber * 100.0f;
        if (errorPersent > 5.0f) {
            bool bCM = true;
        }
        */
    }

    uint bezSement = 0;
    bezierSegment S;
    glm::vec3 pos, vel, acc;
    float t = 0;
    S.A = bezierPatches[0].data[0][0];
    S.B = bezierPatches[0].data[1][0];
    S.planeTangent.frompoints(S.A, S.B);	// solve func
    S.optimalPos = (S.A + S.B) * 0.5f;
    S.optimalShift = 0.5f;

    segments.emplace_back(S);	// FIRST ONE  - COME BACK AND FIX IF LOOPING

    while (bezSement < numBezier)
    {
        t += 0.1f;
        float dst = 0;
        float3 a, b;
        while (dst < kevSampleMinSpacing)
        {
            a = cubic_Casteljau(t, bezierPatches[bezSement].data[0][0], bezierPatches[bezSement].data[0][1], bezierPatches[bezSement].data[0][2], bezierPatches[bezSement + 1].data[0][0]);
            b = cubic_Casteljau(t, bezierPatches[bezSement].data[1][0], bezierPatches[bezSement].data[1][1], bezierPatches[bezSement].data[1][2], bezierPatches[bezSement + 1].data[1][0]);
            //dst = __min(glm::length(a - S.A), glm::length(b - S.B));
            dst = (glm::length(a - S.A) + glm::length(b - S.B)) / 2;
            t += 0.01f;
            if (t > 1.0f)
            {
                t -= 1.0f;
                bezSement++;
            }
        }

        if (bezSement < numBezier)	// avoid the last one
        {
            S.A = a;
            S.B = b;
            S.planeTangent.frompoints(a, b);	// solve func
            S.optimalPos = (a + b) * 0.5f;
            S.optimalShift = 0.5f;

            segments.emplace_back(S);
        }
    }
    /*
    // place first #####################################################################
    for (uint i = 0; i < numBezier; i++)
    {

        // now cache individual sections and get length and distance from that
        for (int j = 0; j < 16; j++)
        {
            segments.emplace_back(S);
            float t = (float)j / 16.0f;
            uint idx = i * 16 + j;
            segments[idx].A = cubic_Casteljau(t, bezierPatches[i].data[0][0], bezierPatches[i].data[0][1], bezierPatches[i].data[0][2], bezierPatches[i + 1].data[0][0]);
            segments[idx].B = cubic_Casteljau(t, bezierPatches[i].data[1][0], bezierPatches[i].data[1][1], bezierPatches[i].data[1][2], bezierPatches[i + 1].data[1][0]);
            segments[idx].planeTangent.frompoints(segments[idx].A, segments[idx].B);


            //cubic_Casteljau_Full(t, bezierPatches[i].data[0][0], bezierPatches[i].data[0][1], bezierPatches[i].data[0][2], bezierPatches[i + 1].data[0][0], pos, vel, acc);
            //float num = vel.x * acc.z - vel.z * acc.x;	// 2d det
            //float length = glm::length(vel);
            //segments[idx].radius = length * length * length / num;
            segments[idx].optimalPos = (segments[idx].A + segments[idx].B) * 0.5f;
            segments[idx].optimalShift = 0.5f;
        }
    }
    */

    /*
    int WRAP(int _index)
    {
        int size = segments.size();
        while (_index < 0) _index += size();
        while (_index >= size) )index -= size;
        return _index;
    };*/

    // Loop hier en solve racing line ##################################################################
    uint SIZE = (uint)segments.size();
#define WRAP(x) ((x + SIZE)%SIZE)

    for (int L = 0; L < kevRepeats; L++)
    {
        // calc angles
        for (uint i = 0; i < SIZE; i++)
        {
            glm::vec3 T0 = (segments[WRAP(i)].optimalPos - segments[WRAP(i - 1)].optimalPos);
            glm::vec3 T1 = (segments[WRAP(i + 1)].optimalPos - segments[WRAP(i)].optimalPos);
            glm::vec2 P0 = glm::normalize(glm::vec2(T0.z, -T0.x));
            float L = glm::length(segments[WRAP(i + 1)].optimalPos - segments[WRAP(i - 1)].optimalPos);
            segments[i].radius = glm::dot(P0, glm::vec2(T1.x, T1.z)) / (L * L);
            segments[i].length = L * 0.5f;
        }

        // shifts
        for (uint i = 0; i < SIZE; i++)
        {
            float biggestR = 0;	// same sign
            float sgn = segments[i].radius / fabs(segments[i].radius);
            float sgnBiggest = 1.0f;

            float avsR = 0;	// same sign
            for (int j = -kevAvsCnt; j <= kevAvsCnt; j++) {
                avsR += segments[WRAP(i + j)].radius;
            }
            avsR /= (kevAvsCnt * 2 + 1);

            for (int j = -kecMaxCnt; j < kecMaxCnt; j++) {
                if (fabs(segments[WRAP(i + j)].radius) > biggestR)
                {
                    biggestR = fabs(segments[WRAP(i + j)].radius);
                    sgnBiggest = fabs(segments[WRAP(i + j)].radius) / segments[WRAP(i + j)].radius;
                }
            }
            biggestR *= sgnBiggest;

            float shift = (segments[i].radius - biggestR) * kevOutScale;		// Oopskop
            shift += avsR * kevInScale;
            shift = glm::clamp(shift, -0.1f, 0.1f);

            segments[i].optimalShift -= shift;
            if (segments[i].optimalShift > 0.8f) segments[i].optimalShift = 0.8f;	// FIXME on DISTANCE in METERS
            if (segments[i].optimalShift < 0.2f) segments[i].optimalShift = 0.2f;
        }

        // push out
        /*
        for (uint i = 50; i < segments.size() - 52; i++)
        {
            float push = 0;
            if (segments[i].optimalShift > 0.9f) push = 0.9f - segments[i].optimalShift;
            if (segments[i].optimalShift < 0.1f) push = 0.1f - segments[i].optimalShift;

            if (push != 0) {
                for (int j = -40; j < 40; j++) {
                    float scale = glm::smoothstep(0.0f, 1.0f, (40.0f - fabs((float)j)) / 40.0f) * 2.7f;
                    segments[i].optimalShift -= push * scale;
                }
            }
        }
        */

        // smooth out
        for (int k = 0; k < kevSmooth; k++)
        {
            for (uint i = 0; i < SIZE; i++)
            {
                //segments[i].optimalTemp = (segments[i - 2].optimalShift + segments[i - 1].optimalShift + segments[i].optimalShift + segments[i + 1].optimalShift + segments[i + 2].optimalShift) / 5.0f;
                //segments[i].optimalTemp = ( segments[i - 1].optimalShift + segments[i].optimalShift + segments[i + 1].optimalShift ) / 3.0f;
                segments[i].optimalTemp = (segments[WRAP(i - 1)].optimalShift * 0.5f + segments[i].optimalShift + segments[WRAP(i + 1)].optimalShift * 0.5f) / 2.0f;
            }

            for (uint i = 0; i < SIZE; i++)
            {
                segments[i].optimalShift = segments[i].optimalTemp;
            }
        }

        // solve position
        for (uint i = 0; i < segments.size(); i++)
        {
            segments[i].optimalPos = glm::lerp(segments[i].A, segments[i].B, segments[i].optimalShift);
        }


    }

    for (uint i = 0; i < segments.size() - 1; i++)
    {
        segments[i].planeInner.frompoints(segments[i + 1].A, segments[i].A);
        segments[i].planeOuter.frompoints(segments[i + 1].B, segments[i].B);
        segments[i].length = (glm::length(segments[i + 1].A - segments[i].A) + glm::length(segments[i + 1].B - segments[i].B)) * 0.5f;
        segments[i + 1].distance = segments[i].distance + segments[i].length;
        float width = glm::length(segments[i].B - segments[i].A);
        //segments[i].camber = atan2(segments[i].A.y - segments[i].B.y, width);	// see if there is faster way
        segments[i].camber = (segments[i].A.y - segments[i].B.y) / width;
        /*
        segments[i].radius = 100000;	// net baie groot
        if (i > 0 && i < segments.size() - 2) {
            float R_a = 0;
            float R_b = 0;
            vec3 AC = glm::normalize(segments[i + 1].A - segments[i - 1].A);
            vec2 Bperp = vec2(AC.z, -AC.x);
            vec3 AB = segments[i].A - segments[i - 1].A;
            //float l = glm::length(AB);
            float l = glm::length(segments[i + 1].A - segments[i - 1].A) * 0.5f;
            vec2 ABnorm = glm::normalize(vec2(AB.x, AB.z));
            float det = glm::dot(Bperp, ABnorm);
            if (fabs(det) > 0.00001) {
                R_a = l / det;
            }



            // B side
            AC = glm::normalize(segments[i + 1].B - segments[i - 1].B);
            Bperp = vec2(AC.z, -AC.x);
            AB = segments[i].B - segments[i - 1].B;
            l = glm::length(AB);
            ABnorm = glm::normalize(vec2(AB.x, AB.z));
            det = glm::dot(Bperp, ABnorm);
            if (fabs(det) > 0.00001) {
                R_b = l / det;
            }

            float R_max = R_a;
            //if (fabs(R_b) > fabs(R_a)) R_max = R_b;

            if (R_max != 0) {
                segments[i].radius = R_max;
            }

        }
        */
    }

    for (uint i = 0; i < numBezier; i++)
    {
        //patchSegments[i].length = (glm::length(patch.A - segments[size - 1].A) + glm::length(segments[size].B - segments[size - 1].B)) * 0.5f;
        //patchSegments[i].distance
    }
}



void AI_bezier::exportAI(FILE* file)
{
    fwrite(&isClosedPath, sizeof(int), 1, file);
    fwrite(&startpos, sizeof(float3), 1, file);
    fwrite(&endpos, sizeof(float3), 1, file);
    fwrite(&pathLenght, sizeof(float), 1, file);
    fwrite(&numBezier, sizeof(uint), 1, file);
    fwrite(&bezierPatches[0], sizeof(cubicDouble), bezierPatches.size(), file);
}



void AI_bezier::exportAITXTDEBUG(FILE* file)
{
    if (isClosedPath)	fprintf(file, "closed loop\n");
    else				fprintf(file, "open\n");
    fprintf(file, "start %2.3f, %2.3f, %2.3f\n", startpos.x, startpos.y, startpos.z);
    fprintf(file, "end %2.3f, %2.3f, %2.3f\n", endpos.x, endpos.y, endpos.z);
    fprintf(file, "pathLength %2.3fkm\n", pathLenght);
    fprintf(file, "# %d\n\n", numBezier);

    for (uint i = 0; i < (uint)bezierPatches.size(); i++) {
        fprintf(file, "%3d  (%2.3f, %2.3f, %2.3f, %2.3f)  (%2.3f, %2.3f, %2.3f, %2.3f) \n", i, bezierPatches[i].data[0][0].x, bezierPatches[i].data[0][0].y, bezierPatches[i].data[0][0].z, bezierPatches[i].data[0][0].w, bezierPatches[i].data[1][0].x, bezierPatches[i].data[1][0].y, bezierPatches[i].data[1][0].z, bezierPatches[i].data[1][0].w);
    }
}



void AI_bezier::exportGates(FILE* file)
{
    for (uint i = 0; i < numBezier; i++)
    {
        glm::vec3 startL = cubic_Casteljau(0, bezierPatches[i].data[0][0], bezierPatches[i].data[0][1], bezierPatches[i].data[0][2], bezierPatches[i + 1].data[0][0]);
        glm::vec3 midL = cubic_Casteljau(0.5, bezierPatches[i].data[0][0], bezierPatches[i].data[0][1], bezierPatches[i].data[0][2], bezierPatches[i + 1].data[0][0]);

        glm::vec3 startR = cubic_Casteljau(0, bezierPatches[i].data[1][0], bezierPatches[i].data[1][1], bezierPatches[i].data[1][2], bezierPatches[i + 1].data[1][0]);
        glm::vec3 midR = cubic_Casteljau(0.5, bezierPatches[i].data[1][0], bezierPatches[i].data[1][1], bezierPatches[i].data[1][2], bezierPatches[i + 1].data[1][0]);

        fprintf(file, "%f %f %f %f  %f %f\n", startL.x, startL.z, startR.x, startR.z, segments[i * 16].camber, segments[i * 16].radius);
    }
}



void AI_bezier::exportCSV(FILE* file, uint _side)
{
    for (uint i = startIdx; i <= endIdx; i++)
    {
        glm::vec3 pos;

        switch (_side) {
        case 0:
            pos = segments[i].A;
            break;
        case 1:
            //pos = (segments[i].A + segments[i].B) * 0.5f;
            pos = segments[i].optimalPos;
            break;
        case 2:
            pos = segments[i].B;
            break;
        }

        //fprintf(file, "%f %f %f %f %f %f\n", pos.x, pos.y, pos.z, segments[i].camber, segments[i].radius, segments[i].optimalShift);
        fprintf(file, "%f %f %f\n", pos.x, pos.y, pos.z);
    }
}



void AI_bezier::save() {
}



void AI_bezier::load(FILE* file)
{
    fread(&isClosedPath, sizeof(int), 1, file);
    fread(&startpos, sizeof(float3), 1, file);
    fread(&endpos, sizeof(float3), 1, file);
    fread(&pathLenght, sizeof(float), 1, file);
    fread(&numBezier, sizeof(uint), 1, file);
    bezierPatches.resize(numBezier + 1);
    fread(&bezierPatches[0], sizeof(cubicDouble), numBezier + 1, file);

    cacheAll();
    solveStartEnd();	//??? maybe save startend
}









/*  roadNetwork
    --------------------------------------------------------------------------------------------------------------------*/
roadNetwork::roadNetwork()
{
    odeBezier.setGrid(50.0f, 500, 500);		// so 25x25km we do not need the edges
    roadSectionsList.reserve(10000);
    intersectionList.reserve(10000);
}



bool roadNetwork::init()
{
    /*
    sceneBridges = Scene::create();
    if (sceneBridges) {
        rendererSceneBridges = SceneRenderer::create(sceneBridges);
        if (rendererSceneBridges) {
            mpProgramBridges = GraphicsProgram::createFromFile(appendShaderExtension("Flight.vs"), appendShaderExtension("Flight_ground.ps"));
            mpProgramVarsBridges = GraphicsVars::create(mpProgramBridges->getActiveVersion()->getReflector());
            return true;
        }
    } */
    return false;
}



bool roadNetwork::renderpopupGUI(Gui* _gui, intersection* _intersection)
{
    bool bChanged = false;

    static float3 COL;
    ImGui::ColorEdit3("TEST", &COL.x);
    ImGui::PushFont(_gui->getFont("roboto_32"));
    {
        ImGui::Text("Intersection : %d", _intersection->GUID);
        if (_intersection->lidarLOD >= 7) {
            ImGui::SliderInt("I quality", &_intersection->buildQuality, 0, 2);
        }
        else {
            ImGui::Text("lidar warning - too low LOD used - %d", _intersection->lidarLOD);
        }

        if (ImGui::Button("normal up")) { _intersection->anchorNormal = glm::vec3(0, 1, 0); solveIntersection(); _intersection->customNormal = true; }
        if (ImGui::DragFloat3("up", &_intersection->anchorNormal.x, 0.01f, -1, 1)) {
            _intersection->anchorNormal = glm::normalize(_intersection->anchorNormal);
            solveIntersection();
            _intersection->customNormal = true;
            bChanged = true;

        }

        if (ImGui::Selectable("export XML")) {
            std::filesystem::path path;
            FileDialogFilterVec filters = { {"intersection.xml"} };
            if (saveFileDialog(filters, path))
            {
                std::ofstream os(path);
                cereal::XMLOutputArchive archive(os);
                _intersection->serialize(archive, 101);
            }
        }


        ImGui::Checkbox("custom Normal", &_intersection->customNormal);
        ImGui::DragFloat("height", &_intersection->heightOffset, -2.0f, 2.0f);
        ImGui::Checkbox("do not Push", &_intersection->doNotPush);

        for (auto rd : _intersection->roadLinks)
        {
            ImGui::Text("%d", rd.roadGUID);
        }
    }
    ImGui::PopFont();

    return bChanged;
}



std::array<std::string, 9> laneNames{ "shoulder in", "turn in", "lane 1", "lane 2", "lane 3", "turn", "shoulder", "side", "clearing" };
void roadNetwork::renderPopupVertexGUI(Gui* _gui, glm::vec2 _pos, uint _vertex)
{
    /*
    Gui::Window avxWindow(_gui, "Avx", { 200, 200 }, { 100, 100 });
    {
        ImGui::SetWindowPos(ImVec2(_pos.x + 100, _pos.y - 100));

        ImGui::Text("vertex %d", _vertex);
        ImGui::Separator();


        // camber
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImVec2 A = window->Rect().GetTL();
        A.x += 100;
        A.y += 90 + currentRoad->points[_vertex].camber * 50.0f;
        ImVec2 B = window->Rect().GetTL();
        B.x += 0;
        B.y += 90 - currentRoad->points[_vertex].camber * 50.0f;
        ImVec2 C = window->Rect().GetTL();
        C.x += 50;
        C.y += 90;
        window->DrawList->AddLine(A, C, ImColor(0, 0, 255, 255), 5.0f);
        window->DrawList->AddLine(C, B, ImColor(0, 255, 0, 255), 5.0f);

        switch (currentRoad->points[_vertex].constraint) {
        case e_constraint::none:
            ImGui::Text("free =  %3.1f", currentRoad->points[_vertex].camber);
            break;
        case e_constraint::camber:
            ImGui::Text("comstrained =  %3.1f", currentRoad->points[_vertex].camber);
            break;
        case e_constraint::fixed:		//??? is this used
            ImGui::Text("fixed");
            break;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("use left - right arrows to modify while hovering vertex");

    }
    avxWindow.release();

    Gui::Window bvxWindow(_gui, "Bvx", { 200, 200 }, { 100, 100 });
    {
        ImGui::SetWindowPos(ImVec2(_pos.x - 300, _pos.y - 200));
    }
    bvxWindow.release();

    Gui::Window cvxWindow(_gui, "Cvx", { 200, 200 }, { 100, 100 });
    {
        ImGui::SetWindowPos(ImVec2(_pos.x - 100, _pos.y + 200));
    }
    cvxWindow.release();
    */
}



void roadNetwork::renderPopupSegmentGUI(Gui* _gui, glm::vec2  _pos, uint _idx)
{
    /*
    Gui::Window seginfoWindow(_gui, "SEGInfo", { 200, 200 }, { 100, 100 });
    {
        ImGui::SetWindowPos(ImVec2(_pos.x + 100, _pos.y - 100));
    }
    seginfoWindow.release();
    */
}



// FIXME selection
void roadNetwork::loadRoadMaterial(uint _side, uint _slot)
{

    std::filesystem::path path;
    FileDialogFilterVec filters = { {"roadMaterial.jpg"} };
    if (openFileDialog(filters, path))
    {
        std::string filename = path.string();
        if (filename.find("jpg") != std::string::npos)
        {
            filename = filename.substr(0, filename.size() - 4);
        }

        uint idx = roadMatCache.find_insert_material(filename);
        for (int i = selectFrom; i < selectTo; i++)
        {
            currentRoad->points[i].setMaterialIndex(_side, _slot, idx);
        }
    }
}



void roadNetwork::clearRoadMaterial(uint _side, uint _slot)
{
    for (int i = selectFrom; i <= selectTo; i++)
    {
        currentRoad->points[i].setMaterialIndex(_side, _slot, -1);
    }
}



uint roadNetwork::getRoadMaterial(uint _side, uint _slot, uint _index)
{
    return currentRoad->points[_index].getMaterialIndex(_side, _slot);
}



const std::string roadNetwork::getMaterialName(uint _side, uint _slot)
{
    int idx = currentRoad->points[selectFrom].getMaterialIndex(_side, _slot);
    if (idx >= 0 && idx < roadMatCache.materialVector.size()) {
        return std::to_string(idx) + roadMatCache.materialVector.at(idx).displayName;
        //return std::to_string(idx);
    }
    return "";
}



const std::string roadNetwork::getMaterialPath(uint _side, uint _slot)
{
    int idx = currentRoad->points[selectFrom].getMaterialIndex(_side, _slot);
    if (idx >= 0 && idx < roadMatCache.materialVector.size()) {
        return roadMatCache.materialVector.at(idx).relativePath;
    }
    return "";
}



const Texture::SharedPtr roadNetwork::getMaterialTexture(uint _side, uint _slot)
{
    int idx = currentRoad->points[selectFrom].getMaterialIndex(_side, _slot);
    if (idx >= 0 && idx < roadMatCache.materialVector.size()) {
        return roadMatCache.materialVector.at(idx).thumbnail;
    }
    return nullptr;
}



#define TOOLTIP(x)  if (ImGui::IsItemHovered()) {ImGui::SetTooltip(x);}

#define MATERIAL(side, slot, tooltip)	{ \
												ImGui::PushID(9999 + side * 1000 + slot); \
													uint mat = getRoadMaterial(side, slot, selectFrom); \
													if (ImGui::Button(getMaterialName(side, slot).c_str(), ImVec2(190, 0))) { loadRoadMaterial(side, slot); } \
													TOOLTIP( tooltip ); \
													if (ImGui::IsItemHovered()) { roadNetwork::displayThumbnail = getMaterialTexture(side, slot); } \
													ImGui::SameLine(); \
													if (ImGui::Button("X")) { ;}\
												ImGui::PopID(); \
										} \

#define DISPLAY_MATERIAL(side, slot, tooltip)	{ \
										bool same = true; \
										int mat =  getRoadMaterial(side, slot, selectFrom); \
										for (int sel = selectFrom; sel < selectTo; sel++) \
										{ \
											if (getRoadMaterial(side, slot, sel) != mat) same &= false; 	 \
										} \
										\
										std::string mat_name = "... various ..."; \
										if (same) { \
											style.Colors[ImGuiCol_Button] = ImVec4(0.02f, 0.02f, 0.02f, 0.4f);  \
											mat_name = getMaterialName(side, slot); \
										} \
										else { \
											style.Colors[ImGuiCol_Button] = ImVec4(0.72f, 0.42f, 0.12f, 0.95f); 	\
										}  \
										if (selectTo <= selectFrom) { style.Colors[ImGuiCol_Button] = ImVec4(0.3f, 0.3f, 0.3f, 0.5f); }\
										 \
										 ImGui::PushID(9999 + side * 1000 + slot); \
												if (ImGui::Button(mat_name.c_str(), ImVec2(190, 0))) { if (selectTo > selectFrom) loadRoadMaterial(side, slot); } \
												TOOLTIP( tooltip ); \
												if (same && ImGui::IsItemHovered()) { roadNetwork::displayThumbnail = getMaterialTexture(side, slot); } \
												ImGui::SameLine(); \
												if (ImGui::Button("X")) { clearRoadMaterial(side, slot);}\
										ImGui::PopID(); \
										} \


//??? can we do this with a function, this is sillt
#define DISPLAY_WIDTH(_lane, _min, _max, _SIDE) {					\
									bool same = true; 					\
									float val_0 = _road->points[selectFrom]._SIDE[_lane].laneWidth; 					\
									float val = 0; 					\
									float MIN = 99999; 					\
									float MAX = -99999; 					\
									int total = selectTo - selectFrom + 1; 					\
									for (int sel = selectFrom; sel <= selectTo; sel++) 					\
									{ 					\
										val += _road->points[sel]._SIDE[_lane].laneWidth; 					\
										if (_road->points[sel]._SIDE[_lane].laneWidth > MAX )   MAX = _road->points[sel]._SIDE[_lane].laneWidth; 					\
										if (_road->points[sel]._SIDE[_lane].laneWidth < MIN )   MIN = _road->points[sel]._SIDE[_lane].laneWidth; 					\
										if (_road->points[sel]._SIDE[_lane].laneWidth != val_0) same &= false; 					\
									} 					\
									val /= total; 					\
									 										\
									if (same) { 					\
										if (val == 0) { 					\
											style.Colors[ImGuiCol_FrameBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.95f); 					\
										} 					\
										else { 					\
											style.Colors[ImGuiCol_FrameBg] = ImVec4(0.42f, 0.02f, 0.02f, 0.95f); 					\
										} 					\
									} 					\
									else { 					\
										style.Colors[ImGuiCol_FrameBg] = ImVec4(0.72f, 0.42f, 0.12f, 0.95f); 					\
									} 					\
									 					\
									if (ImGui::DragFloat("", &val, 0.01f, _min, _max, "%2.2f m")) 					\
									{ 					\
										for (int sel = selectFrom; sel <= selectTo; sel++) 					\
										{ 					\
											_road->points[sel]._SIDE[_lane].laneWidth = val; 					\
										} 					\
									} 					\
									 					\
									if (same) { 					\
									} 					\
									else { 					\
										if (ImGui::IsItemHovered()) {ImGui::SetTooltip("VARIOUS VALUES %2.2f = %2.2f m", MIN, MAX);} 					\
									} 					\
								} 					\



// FIXME should likely be names and then lookup for future use
void roadNetwork::saveRoadMaterials(roadSection* _road, int _vertex)
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"roadlayout.xml"} };
    if (saveFileDialog(filters, path))
    {
        FILE* file = fopen(path.string().c_str(), "w");
        if (file) {
            for (int i = 0; i < 15; i++) fprintf(file, "%d\n", _road->points[_vertex].matLeft[i]);
            for (int i = 0; i < 2; i++) fprintf(file, "%d\n", _road->points[_vertex].matCenter[i]);
            for (int i = 0; i < 15; i++) fprintf(file, "%d\n", _road->points[_vertex].matRight[i]);
            //fwrite(&_road->points[_vertex].matLeft[0], sizeof(int), 15, file);
            //fwrite(&_road->points[_vertex].matCenter[0], sizeof(int), 2, file);
            //fwrite(&_road->points[_vertex].matRight[0], sizeof(int), 15, file);

            fprintf(file, "testing\n");
        }
        fclose(file);
    }
}

int  load_left[15];
int  load_center[2];
int  load_right[15];



void roadNetwork::loadRoadMaterials(roadSection* _road, int _from, int _to)
{


    std::filesystem::path path;
    FileDialogFilterVec filters = { {"roadlayout"} };
    if (openFileDialog(filters, path))
    {
        FILE* file = fopen(path.string().c_str(), "r");
        if (file) {
            for (int i = 0; i < 15; i++) fscanf(file, "%d\n", &load_left[i]);
            for (int i = 0; i < 2; i++) fscanf(file, "%d\n", &load_center[i]);
            for (int i = 0; i < 15; i++) fscanf(file, "%d\n", &load_right[i]);
        }
        fclose(file);
    }
}



void roadNetwork::loadRoadMaterialsAll(roadSection* _road, int _from, int _to)
{
    loadRoadMaterials(_road, _from, _to);

    for (int idx = _from; idx <= _to; idx++)
    {
        for (int i = 0; i < 15; i++) _road->points[idx].matLeft[i] = load_left[i];
        for (int i = 0; i < 2; i++) _road->points[idx].matCenter[i] = load_center[i];
        for (int i = 0; i < 15; i++) _road->points[idx].matRight[i] = load_right[i];
    }
}



void roadNetwork::loadRoadMaterialsVerge(roadSection* _road, int _from, int _to)
{
    loadRoadMaterials(_road, _from, _to);

    for (int idx = _from; idx <= _to; idx++)
    {
        _road->points[idx].matLeft[0] = load_left[0];
        _road->points[idx].matRight[0] = load_right[0];
    }
}



void roadNetwork::loadRoadMaterialsSidewalk(roadSection* _road, int _from, int _to)
{
    loadRoadMaterials(_road, _from, _to);

    for (int idx = _from; idx <= _to; idx++)
    {
        _road->points[idx].matLeft[1] = load_left[1];
        _road->points[idx].matLeft[2] = load_left[2];
        _road->points[idx].matRight[1] = load_right[1];
        _road->points[idx].matRight[2] = load_right[2];
    }
}



void roadNetwork::loadRoadMaterialsAsphalt(roadSection* _road, int _from, int _to)
{
    loadRoadMaterials(_road, _from, _to);

    for (int idx = _from; idx <= _to; idx++)
    {
        _road->points[idx].matLeft[3] = load_left[3];
        _road->points[idx].matRight[3] = load_right[3];
    }
}



void roadNetwork::loadRoadMaterialsLines(roadSection* _road, int _from, int _to)
{
    loadRoadMaterials(_road, _from, _to);

    for (int idx = _from; idx <= _to; idx++)
    {
        for (int i = 4; i < 10; i++) _road->points[idx].matLeft[i] = load_left[i];
        for (int i = 0; i < 2; i++) _road->points[idx].matCenter[i] = load_center[i];
        for (int i = 4; i < 10; i++) _road->points[idx].matRight[i] = load_right[i];
    }
}



void roadNetwork::loadRoadMaterialsWT(roadSection* _road, int _from, int _to)
{
    loadRoadMaterials(_road, _from, _to);

    for (int idx = _from; idx <= _to; idx++)
    {
        for (int i = 10; i < 15; i++) _road->points[idx].matLeft[i] = load_left[i];
        for (int i = 10; i < 15; i++) _road->points[idx].matRight[i] = load_right[i];
    }
}




bool roadNetwork::renderpopupGUI(Gui* _gui, roadSection* _road, int _vertex) {

    if (!_road) return false;

    auto& style = ImGui::GetStyle();
    ImGuiStyle oldStyle = style;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.02f, 0.01f, 0.80f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.17f, 0.70f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.14f, 0.17f, 0.90f);

    style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.5f);
    //	style.Colors[ImGuiCol_ButtonHovered] = MED(0.86f);

    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

    style.FrameRounding = 6.0f;

    roadNetwork::displayThumbnail = nullptr;

    ImGui::PushFont(_gui->getFont("roboto_32"));
    {
        ImGui::Text("Road %d, {%d}", _road->GUID, _road->points.size());
        ImGui::PushItemWidth(180);
        ImGui::SliderInt("q", &_road->buildQuality, 0, 2);
        ImGui::PopItemWidth();

        ImGui::Checkbox("is closed loop", &_road->isClosedLoop);
        TOOLTIP("set for tracks\nIt will snap the last point to the first if closer than 20m appart, \nand we have more than 5 points to avoid mess at start of a road");

    }
    ImGui::PopFont();

    if (_road->points.size() == 0) return false;

    if (ImGui::Selectable("export XML")) {
        std::filesystem::path path;
        FileDialogFilterVec filters = { {"road.xml"} };
        if (saveFileDialog(filters, path))
        {
            std::ofstream os(path);
            cereal::XMLOutputArchive archive(os);
            _road->serialize(archive, 101);
        }
    }

    // Selection #$###########################################################################
    if (_vertex >= (int)_road->points.size()) _vertex = (int)_road->points.size() - 1;

    if (selectionType == 0) {
        selectFrom = _vertex;
        selectTo = _vertex;
    }
    if (selectionType == 2) {
        selectFrom = 0;
        selectTo = (int)_road->points.size() - 1;
    }
    ImGui::NewLine();
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.7f);
    ImGui::BeginChildFrame(199, ImVec2(220, 100), 0);
    {
        ImGui::Text("Selection");
        ImGui::PushFont(_gui->getFont("roboto_32"));
        {
            ImGui::PushItemWidth(60);
            if (selectionType == 0)	style.Colors[ImGuiCol_Button] = ImVec4(0.6f, 0.4f, 0.0f, 1.0f);
            if (ImGui::Button("None", ImVec2(60, 0))) { selectionType = 0; }
            TOOLTIP("select None\nDisplay and actions apply to the last vertex under the mouse\nctrl-d");
            style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.5f);

            if (selectionType == 1)	style.Colors[ImGuiCol_Button] = ImVec4(0.6f, 0.4f, 0.0f, 1.0f);
            ImGui::SameLine();
            if (ImGui::Button("Range", ImVec2(60, 0))) { selectionType = 1; }
            TOOLTIP("select Range\nClick and shift Click for start and end of range");
            style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.5f);

            if (selectionType == 2)	style.Colors[ImGuiCol_Button] = ImVec4(0.6f, 0.4f, 0.0f, 1.0f);
            ImGui::SameLine();
            if (ImGui::Button("All", ImVec2(60, 0))) { selectionType = 2; }
            TOOLTIP("select All\nctrl-a");
            style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.5f);

            ImGui::PopItemWidth();




            ImGui::PushItemWidth(90);
            if (ImGui::DragInt("##start", &selectFrom, 1, 0, (int)_road->points.size())) { ; }
            ImGui::SameLine();
            ImGui::Text("-");
            ImGui::SameLine();
            if (ImGui::DragInt("##end", &selectTo, 1, selectFrom, (int)_road->points.size())) { ; }
            ImGui::PopItemWidth();

        }
        ImGui::PopFont();
    }
    ImGui::EndChildFrame();




    /*

    {
        // camber
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImVec2 A = window->Rect().GetTL();
        A.x += 90;
        A.y += 130 + _road->points[_vertex].camber * 40.0f;
        ImVec2 B = window->Rect().GetTL();
        B.x += 10;
        B.y += 130 - _road->points[_vertex].camber * 40.0f;
        ImVec2 C = window->Rect().GetTL();
        C.x += 50;
        C.y += 130;
        window->DrawList->AddLine(A, C, ImColor(0, 0, 255, 255), 5.0f);
        window->DrawList->AddLine(C, B, ImColor(0, 155, 0, 255), 5.0f);

        switch (_road->points[_vertex].constraint) {
        case e_constraint::none:
            ImGui::Text("Camber - free =  %3.2f", _road->points[_vertex].camber);
            break;
        case e_constraint::camber:
            ImGui::Text("Camber - constrained =  %3.2f", _road->points[_vertex].camber);
            break;
        case e_constraint::fixed:
            ImGui::Text("Camber - fixed");
            break;
        }
    }
    */






    ImGui::PushFont(_gui->getFont("roboto_32"));
    ImGui::Checkbox("show materials", &showMaterials);
    ImGui::PopFont();







    ImGui::PushFont(_gui->getFont("roboto_32"));
    {
        ImGui::SetCursorPosX(30);
        ImGui::Text("geometry");

        ImGui::SameLine();
        ImGui::SetCursorPosX(270);
        ImGui::Text("base");
        ImGui::SameLine();
        ImGui::SetCursorPosX(500 - 60);
        if (ImGui::Button("X##base")) {
            clearRoadMaterial(0, splinePoint::matName::verge);
            clearRoadMaterial(0, splinePoint::matName::sidewalk);
            clearRoadMaterial(0, splinePoint::matName::gutter);
            clearRoadMaterial(0, splinePoint::matName::tarmac);

            clearRoadMaterial(2, splinePoint::matName::verge);
            clearRoadMaterial(2, splinePoint::matName::sidewalk);
            clearRoadMaterial(2, splinePoint::matName::gutter);
            clearRoadMaterial(2, splinePoint::matName::tarmac);
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(510);
        ImGui::Text("lines");
        ImGui::SameLine();
        ImGui::SetCursorPosX(740 - 60);
        if (ImGui::Button("X##lines")) {
            clearRoadMaterial(0, splinePoint::matName::shoulder);
            clearRoadMaterial(0, splinePoint::matName::outerTurn);
            clearRoadMaterial(0, splinePoint::matName::lane2);
            clearRoadMaterial(0, splinePoint::matName::lane1);
            clearRoadMaterial(0, splinePoint::matName::innerTurn);
            clearRoadMaterial(0, splinePoint::matName::innerShoulder);

            clearRoadMaterial(1, 0);
            clearRoadMaterial(1, 1);

            clearRoadMaterial(2, splinePoint::matName::shoulder);
            clearRoadMaterial(2, splinePoint::matName::outerTurn);
            clearRoadMaterial(2, splinePoint::matName::lane2);
            clearRoadMaterial(2, splinePoint::matName::lane1);
            clearRoadMaterial(2, splinePoint::matName::innerTurn);
            clearRoadMaterial(2, splinePoint::matName::innerShoulder);
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(750);
        ImGui::Text("wear&tear");
        ImGui::SameLine();
        ImGui::SetCursorPosX(980 - 60);
        if (ImGui::Button("X##wt")) {
            clearRoadMaterial(0, splinePoint::matName::rubberOuter);
            clearRoadMaterial(0, splinePoint::matName::rubberLane3);
            clearRoadMaterial(0, splinePoint::matName::rubberLane2);
            clearRoadMaterial(0, splinePoint::matName::rubberLane1);
            clearRoadMaterial(0, splinePoint::matName::rubberInner);

            clearRoadMaterial(2, splinePoint::matName::rubberOuter);
            clearRoadMaterial(2, splinePoint::matName::rubberLane3);
            clearRoadMaterial(2, splinePoint::matName::rubberLane2);
            clearRoadMaterial(2, splinePoint::matName::rubberLane1);
            clearRoadMaterial(2, splinePoint::matName::rubberInner);
        }
    }
    ImGui::PopFont();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);

    float tempY = ImGui::GetCursorPosY();



    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.2f, 0.0f, 1.0f);
    ImGui::BeginChildFrame(100, ImVec2(970, 260), 0);
    {
        for (int i = 8; i >= 0; i--) {
            style.Colors[ImGuiCol_Button] = ImVec4(0.22f, 0.02f, 0.02f, 0.05f);
            if (!editRight && editLaneIndex == i) { style.Colors[ImGuiCol_Button] = ImVec4(0.22f, 0.02f, 0.02f, 0.95f); }
            if (ImGui::Button(laneNames[i].c_str(), ImVec2(110, 0))) { editLaneIndex = i; editRight = false; }

            ImGui::SameLine();
            ImGui::SetCursorPosX(120);
            ImGui::PushID(3333 + i);
            ImGui::PushItemWidth(90);
            {
                if (i == 0) { DISPLAY_WIDTH(i, -10, 50, lanesLeft); }
                else { DISPLAY_WIDTH(i, 0, 50, lanesLeft); }
            }
            ImGui::PopItemWidth();
            ImGui::PopID();
        }
    }
    ImGui::EndChildFrame();



    float lineY = (ImGui::GetCursorPosY() - tempY) / 9.0f;
    style.Colors[ImGuiCol_Button] = ImVec4(0.01f, 0.05f, 0.08f, 0.9f);

    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.0f);
    ImGui::SetCursorPos(ImVec2(240, tempY));
    ImGui::BeginChildFrame(101, ImVec2(220, 260), 0);
    {
        DISPLAY_MATERIAL(0, splinePoint::verge, "VERGE \nLine between the outermost lane and the shoulder.");
        DISPLAY_MATERIAL(0, splinePoint::sidewalk, "SIDEWALK \nLine between the outermost lane and the shoulder.");
        DISPLAY_MATERIAL(0, splinePoint::gutter, "GUTTER \nLine between the outermost lane and the shoulder.");
        ImGui::SetCursorPosY(lineY * 5);
        DISPLAY_MATERIAL(0, splinePoint::tarmac, "ROAD SURFACE \nLine between the outermost lane and the shoulder.");
    }
    ImGui::EndChildFrame();

    ImGui::SetCursorPos(ImVec2(480, tempY));
    ImGui::BeginChildFrame(102, ImVec2(220, 260), 0);
    {
        ImGui::SetCursorPosY(lineY * 2);


        DISPLAY_MATERIAL(0, splinePoint::shoulder, "SHOULDER \nLine between the outermost lane and the shoulder.");
        DISPLAY_MATERIAL(0, splinePoint::outerTurn, "OUTER TURN LANE \nUsually dotted line to get into a turn lane \nApplied to the outside of the lane next to it.");
        ImGui::NewLine();
        DISPLAY_MATERIAL(0, splinePoint::lane2, "LANE 2 \nOutside line to the next lane if there is one. \nOnly applies to lane 3 it it exists.");
        DISPLAY_MATERIAL(0, splinePoint::lane1, "LANE 1 \nOutside line to the next lane if there is one. \nOnly applies to lane 2 or 3 if they exist.");
        DISPLAY_MATERIAL(0, splinePoint::innerTurn, "INNER TURN LANE \nUsually dotted line to get into a turn lane \nApplied to the inside of the lane next to it.");
        DISPLAY_MATERIAL(0, splinePoint::innerShoulder, "INNER SHOULDER \nLine between a triffic island and the first lane.");
    }
    ImGui::EndChildFrame();

    ImGui::SetCursorPos(ImVec2(720, tempY));
    ImGui::BeginChildFrame(103, ImVec2(220, 260), 0);
    {
        ImGui::SetCursorPosY(lineY * 3);
        DISPLAY_MATERIAL(0, splinePoint::rubberOuter, "OUTER TURN 2 \nRubbering, tyre wear, oil and dirt \nApplied to only this lane");
        DISPLAY_MATERIAL(0, splinePoint::rubberLane3, "LANE 3 \nRubbering, tyre wear, oil and dirt \nApplied to only this lane");
        DISPLAY_MATERIAL(0, splinePoint::rubberLane2, "LANE 2 \nRubbering, tyre wear, oil and dirt \nApplied to only this lane");
        DISPLAY_MATERIAL(0, splinePoint::rubberLane1, "LANE 1 \nRubbering, tyre wear, oil and dirt \nApplied to only this lane");
        DISPLAY_MATERIAL(0, splinePoint::rubberInner, "INNER TURN \nRubbering, tyre wear, oil and dirt \nApplied to only this lane");
    }
    ImGui::EndChildFrame();


    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
    {
        ImGui::SetCursorPosX(480);
        DISPLAY_MATERIAL(1, 0, "CENTERLINE");
        ImGui::SameLine();
        ImGui::SetCursorPosX(720);
        DISPLAY_MATERIAL(1, 1, "CENTER CRACKS");
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);


    tempY = ImGui::GetCursorPosY();
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.02f, 0.02f, 0.7f, 0.3f);
    ImGui::BeginChildFrame(200, ImVec2(970, 260), 0);
    for (int i = 0; i < 9; i++) {
        style.Colors[ImGuiCol_Button] = ImVec4(0.22f, 0.02f, 0.02f, 0.05f);
        if (editRight && editLaneIndex == i) { style.Colors[ImGuiCol_Button] = ImVec4(0.22f, 0.02f, 0.02f, 0.95f); }
        if (ImGui::Button(laneNames[i].c_str(), ImVec2(110, 0))) { editLaneIndex = i; editRight = true; }

        ImGui::SameLine();
        ImGui::SetCursorPosX(120);
        ImGui::PushItemWidth(90);
        ImGui::PushID(999 + i);
        {
            {
                if (i == 0) { DISPLAY_WIDTH(i, -10, 50, lanesRight); }
                else { DISPLAY_WIDTH(i, 0, 50, lanesRight); }
            }
        }
        ImGui::PopItemWidth();
        ImGui::PopID();
    }
    ImGui::EndChildFrame();






    style.Colors[ImGuiCol_Button] = ImVec4(0.01f, 0.05f, 0.08f, 0.9f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.0f);
    ImGui::SetCursorPos(ImVec2(240, tempY));
    ImGui::BeginChildFrame(201, ImVec2(220, 260), 0);
    {

        ImGui::SetCursorPosY(lineY * 3);
        DISPLAY_MATERIAL(2, splinePoint::tarmac, "ROAD SURFACE \nLine between the outermost lane and the shoulder.");
        ImGui::SetCursorPosY(lineY * 6);
        DISPLAY_MATERIAL(2, splinePoint::gutter, "GUTTER \nLine between the outermost lane and the shoulder.");
        DISPLAY_MATERIAL(2, splinePoint::sidewalk, "SIDEWALK \nLine between the outermost lane and the shoulder.");
        DISPLAY_MATERIAL(2, splinePoint::verge, "VERGE \nLine between the outermost lane and the shoulder.");
    }
    ImGui::EndChildFrame();

    ImGui::SetCursorPos(ImVec2(480, tempY));
    ImGui::BeginChildFrame(202, ImVec2(220, 260), 0);
    {
        DISPLAY_MATERIAL(2, splinePoint::innerShoulder, "INNER SHOULDER \nLine between a traffic island and the first lane.");
        DISPLAY_MATERIAL(2, splinePoint::innerTurn, "INNER TURN LANE \nUsually dotted line to get into a turn lane \nApplied to the inside of the lane next to it.");
        DISPLAY_MATERIAL(2, splinePoint::lane1, "LANE 1 \nOutside line to the next lane if there is one. \nOnly applies to lane 2 or 3 if they exist.");
        DISPLAY_MATERIAL(2, splinePoint::lane2, "LANE 2 \nOutside line to the next lane if there is one. \nOnly applies to lane 3 it it exists.");
        ImGui::NewLine();
        DISPLAY_MATERIAL(2, splinePoint::outerTurn, "OUTER TURN LANE \nUsually dotted line to get into a turn lane \nApplied to the outside of the lane next to it.");
        DISPLAY_MATERIAL(2, splinePoint::shoulder, "SHOULDER \nLine between the outermost lane and the shoulder.");
    }
    ImGui::EndChildFrame();

    ImGui::SetCursorPos(ImVec2(720, tempY));
    ImGui::BeginChildFrame(203, ImVec2(220, 260), 0);
    {
        ImGui::SetCursorPosY(lineY * 1);
        DISPLAY_MATERIAL(2, splinePoint::rubberInner, "INNER TURN \nRubbering, tyre wear, oil and dirt \nApplied to only this lane");
        DISPLAY_MATERIAL(2, splinePoint::rubberLane1, "LANE 1 \nRubbering, tyre wear, oil and dirt \nApplied to only this lane");
        DISPLAY_MATERIAL(2, splinePoint::rubberLane2, "LANE 2 \nRubbering, tyre wear, oil and dirt \nApplied to only this lane");
        DISPLAY_MATERIAL(2, splinePoint::rubberLane3, "LANE 3 \nRubbering, tyre wear, oil and dirt \nApplied to only this lane");
        DISPLAY_MATERIAL(2, splinePoint::rubberOuter, "OUTER TURN 2 \nRubbering, tyre wear, oil and dirt \nApplied to only this lane");

    }
    ImGui::EndChildFrame();



    // Load and save section
    style.Colors[ImGuiCol_Button] = ImVec4(0.022f, 0.052f, 0.072f, 0.9f);
    ImGui::NewLine();
    tempY = ImGui::GetCursorPosY();
    float cursorBelow = ImGui::GetCursorPosY();
    tempY = 150;




    ImGui::SetCursorPos(ImVec2(340, tempY));
    if (ImGui::Button("Save all", ImVec2(100, 0)))
    {
        saveRoadMaterials(_road, selectFrom);
    }

    ImGui::SetCursorPos(ImVec2(240, tempY));
    if (ImGui::Button("Load all", ImVec2(100, 0)))
    {
        loadRoadMaterialsAll(_road, selectFrom, selectTo);
    }

    ImGui::SetCursorPos(ImVec2(240, tempY + 50));
    if (ImGui::Button("Load tarmac", ImVec2(200, 0))) {
        loadRoadMaterialsAsphalt(_road, selectFrom, selectTo);
    }

    ImGui::SetCursorPos(ImVec2(240, tempY + 80));
    if (ImGui::Button("Load verges", ImVec2(200, 0))) {
        loadRoadMaterialsVerge(_road, selectFrom, selectTo);
    }




    ImGui::SetCursorPos(ImVec2(480, tempY + 80));
    if (ImGui::Button("Load lines", ImVec2(200, 0))) {
        loadRoadMaterialsLines(_road, selectFrom, selectTo);
    }
    //	ImGui::SetCursorPos(ImVec2(580, tempY));
    //	if (ImGui::Button("Save lines", ImVec2(100, 0))) { ; }

    ImGui::SetCursorPos(ImVec2(720, tempY + 80));
    if (ImGui::Button("Load w&t", ImVec2(200, 0))) {
        loadRoadMaterialsWT(_road, selectFrom, selectTo);
    }
    //	ImGui::SetCursorPos(ImVec2(820, tempY));
    //	if (ImGui::Button("Save w&t", ImVec2(100, 0))) { ; }


    ImGui::SetCursorPos(ImVec2(20, cursorBelow));
    if (ImGui::Button("Load geometry", ImVec2(100, 0))) { ; }
    ImGui::SetCursorPos(ImVec2(120, cursorBelow));
    if (ImGui::Button("Save geometry", ImVec2(100, 0))) { ; }


    ImGui::Text("Sampler offsets");
    if (ImGui::DragFloat("Height", &_road->points[_vertex].height_AboveAnchor, 0.1f, -10.0f, 10.0f, "%2.2f m")) {
        _road->points[_vertex].solveMiddlePos();
    }
    if (ImGui::DragFloat("Tangent", &_road->points[_vertex].tangent_Offset, 0.1f, -50.0f, 50.0f, "%2.2f m")) {
        _road->points[_vertex].solveMiddlePos();
    }


    ImGui::Checkbox("is bridge", &_road->points[_vertex].isBridge);
    if (_road->points[_vertex].isBridge)
    {
        char txt[256];
        strncpy(txt, _road->points[_vertex].bridgeName.c_str(), _road->points[_vertex].bridgeName.length());
        if (ImGui::InputText("name", &txt[0], 256))
        {
            _road->points[_vertex].bridgeName = txt;
        }

    }

    ImGui::Checkbox("isAIonly", &_road->points[_vertex].isAIonly);
    ImGui::NewLine();
    ImGui::NewLine();


    style = oldStyle;

    return showMaterials;
}



void roadNetwork::renderGUI(Gui* _gui)
{

    ImGui::PushFont(_gui->getFont("roboto_32"));
    {
        ImGui::Text("%d rd, %d int", (int)roadSectionsList.size(), (int)intersectionList.size());
        ImGui::Text("%4.2f km   (%d%%)", getDistance() / 1000.0f, getDone());
    }
    ImGui::PopFont();

    char txt[256];
    ImGui::Separator();
    sprintf(txt, "#bezier - %d (%d Kb)", (int)debugNumBezier, ((int)debugNumBezier * (int)sizeof(cubicDouble)) / 1024);
    ImGui::Text(txt);
    sprintf(txt, "#layer - %d (%d Kb)", (int)debugNumIndex, ((int)debugNumIndex * (int)sizeof(bezierLayer) / 1024));
    ImGui::Text(txt);
    sprintf(txt, "#ODE - %d (%d Kb)", (int)odeBezier.bezierBounding.size(), ((int)odeBezier.bezierBounding.size() * (int)sizeof(physicsBezier)) / 1024);
    ImGui::Text(txt);

}



void roadNetwork::renderGUI_3d(Gui* mpGui)
{
    if (ImGui::BeginPopupContextWindow(false))
    {
        if (ImGui::Selectable("New Ecotope 3D")) { ; }
        if (ImGui::Selectable("Load")) { ; }
        if (ImGui::Selectable("Save")) { ; }

        ImGui::EndPopup();
    }
}



void roadNetwork::newRoadSpline()
{
    roadSectionsList.emplace_back();
    std::vector<roadSection>::iterator it = roadSectionsList.end();
    it--;
    currentRoad = &(*it);
    currentRoad->GUID = (int)roadSectionsList.size() - 1;
    currentIntersection = nullptr;
}



void roadNetwork::newIntersection()
{
    intersectionList.emplace_back();
    currentIntersection = &intersectionList.back();
    currentIntersection->GUID = (int)intersectionList.size() - 1;	// cant we automate?
    currentRoad = nullptr;
}



float roadNetwork::getDistance() {
    float total = 0;
    uint numRoads = (uint)roadSectionsList.size();
    for (uint i = 0; i < numRoads; i++) {
        roadSection* pRoad = &roadSectionsList[i];
        total += pRoad->getDistance();
    }

    return total;
}



int roadNetwork::getDone()
{
    float total = 0;
    float q = 0;
    uint numRoads = (uint)roadSectionsList.size();
    for (uint i = 0; i < numRoads; i++) {
        roadSection* pRoad = &roadSectionsList[i];
        total += pRoad->getDistance();

        if (pRoad->buildQuality == 2) {
            q += pRoad->getDistance();
        }
    }

    return (int)(q / total * 100.0f);
}



void roadNetwork::load(uint _version)
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"roadnetwork"} };
    if (openFileDialog(filters, lastUsedFilename))
    {
        load(lastUsedFilename, _version);
    }
}



void roadNetwork::load(std::filesystem::path _path, uint _version)
{
    lastUsedFilename = _path;

    std::ifstream is(_path, std::ios::binary);
    if (!is.fail()) {
        cereal::BinaryInputArchive archive(is);
        serialize(archive, _version);

        for (auto& roadSection : roadSectionsList) {
            roadSection.solveRoad();
            roadSection.solveRoad();
        }

        roadMatCache.reloadMaterials();
        terrafectorEditorMaterial::static_materials.rebuildAll();
    }

    currentRoad = nullptr;
}



void roadNetwork::save()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"roadnetwork"} };
    if (saveFileDialog(filters, lastUsedFilename))
    {
        std::ofstream os(lastUsedFilename, std::ios::binary);
        cereal::BinaryOutputArchive archive(os);
        serialize(archive, 101);
    }
}



bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}



void roadNetwork::quickSave()
{
    time_t now = time(0);
    tm* ltm = localtime(&now);

    char _time[256];
    sprintf(_time, "_%d_%d_%d_%d_%d.roadnetwork", ltm->tm_mon, ltm->tm_mday, ltm->tm_hour, ltm->tm_min, ltm->tm_sec);

    std::string newname = lastUsedFilename.string();
    newname.replace(newname.find(".roadnetwork"), sizeof(".roadnetwork") - 1, _time);

    std::ofstream os(newname, std::ios::binary);
    cereal::BinaryOutputArchive archive(os);
    serialize(archive, 101);
}

/*
bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}
*/
void roadNetwork::exportBinary() {
    
    char name[256];
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"gpu"} };
    if (saveFileDialog(filters, path))
    {
        sprintf(name, "attrib -r \"%s\"", path.string().c_str());
        system(name);

        updateAllRoads(true);

        FILE* file = fopen(path.string().c_str(), "wb");
        if (file) {
            uint matSize = (uint)terrafectorEditorMaterial::static_materials.materialVector.size();
            uint textureSize = (uint)terrafectorEditorMaterial::static_materials.textureVector.size();

            fwrite(&textureSize, 1, sizeof(uint), file);// textures
            fwrite(&matSize, 1, sizeof(uint), file);// materials
            fwrite(&debugNumBezier, 1, sizeof(uint), file);
            fwrite(&debugNumIndex, 1, sizeof(uint), file);


            char name[512];
            for (uint i = 0; i < textureSize; i++) {
                std::string path = terrafectorEditorMaterial::static_materials.textureVector[i]->getSourcePath().string();
                path = path.substr(13, path.size());
                path = path.substr(0, path.rfind("."));
                path += ".texture";
                memset(name, 0, 512);
                sprintf(name, "%s", path.c_str());
                fwrite(name, 1, 512, file);
            }

            // materials
            for (uint i = 0; i < matSize; i++) {
                fwrite(&terrafectorEditorMaterial::static_materials.materialVector[i]._constData, 1, sizeof(TF_material), file);
            }




            for (uint i = 0; i < debugNumBezier; i++) {
                //fwrite(staticBezierData.data(), debugNumBezier, sizeof(cubicDouble), file);
                fwrite(&staticBezierData[i], 1, sizeof(cubicDouble), file);
            }

            for (uint i = 0; i < debugNumIndex; i++) {
                //fwrite(staticIndexData.data(), debugNumIndex, sizeof(bezierLayer), file);
                fwrite(&staticIndexData[i], 1, sizeof(bezierLayer), file);
            }

            fclose(file);
        }
        std::string filename = path.string();
        replace(filename, "gpu", "ode");
        sprintf(name, "attrib -r \"%s\"", filename.c_str());
        system(name);

        file = fopen(filename.c_str(), "wb");
        if (file) {
            uint size = (uint)odeBezier.bezierBounding.size();
            fwrite(&size, 1, sizeof(uint), file);
            for (uint i = 0; i < size; i++) {
                //fwrite(odeBezier.bezierBounding.data(), size, sizeof(physicsBezier), file);
                //fwrite(&odeBezier.bezierBounding[i], 1, sizeof(physicsBezier), file);
                odeBezier.bezierBounding[i].binary_export(file);
            }

            odeBezier.buildGridFromBezier();
            odeBezier.gridLookup.binary_export(file);

            fclose(file);
        }


        replace(filename, "ode", "ai");
        sprintf(name, "attrib -r \"%s\"", filename.c_str());
        system(name);

        file = fopen(filename.c_str(), "wb");
        if (file) {
            uint size = (uint)odeBezier.bezierAI.size();
            fwrite(&size, 1, sizeof(uint), file);
            for (uint i = 0; i < size; i++) {
                odeBezier.bezierAI[i].binary_export(file);
            }

            //odeBezier.buildGridFromBezier();
            //odeBezier.gridLookup.binary_export(file);

            fclose(file);
        }
    }
}



void roadNetwork::exportBridges() {

    aiScene* scene = new aiScene;
    scene->mRootNode = new aiNode();

    scene->mMaterials = new aiMaterial * [1];
    scene->mMaterials[0] = nullptr;
    scene->mNumMaterials = 1;

    scene->mMaterials[0] = new aiMaterial();

    scene->mMeshes = new aiMesh * [1];
    scene->mMeshes[0] = new aiMesh();
    scene->mMeshes[0]->mMaterialIndex = 0;
    scene->mNumMeshes = 1;

    scene->mRootNode->mMeshes = new unsigned int[1];
    scene->mRootNode->mMeshes[0] = 0;
    scene->mRootNode->mNumMeshes = 1;

    //scene->mMetaData = new aiMetadata(); // workaround, issue #3781

    auto pMesh = scene->mMeshes[0];

    pMesh->mVertices = new aiVector3D[128];
    pMesh->mNumVertices = 128;

    pMesh->mFaces = new aiFace[63];
    pMesh->mNumFaces = 63;
    pMesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;

    for (uint i = 0; i < 63; i++) {
        aiFace& face = pMesh->mFaces[i];

        face.mIndices = new unsigned int[4];
        face.mNumIndices = 3;

        face.mIndices[0] = i * 2 + 0;
        face.mIndices[1] = i * 2 + 1;
        face.mIndices[2] = i * 2 + 3;
        //face.mIndices[3] = i * 2 + 2;
    }


    char listfilename[256];
    sprintf(listfilename, "%s//bridges//bridgelist.txt", rootPath.c_str());
    FILE* listfile = fopen(listfilename, "w");
    if (listfile)
    {
        for (auto& road : roadSectionsList) {
            int cnt = 0;
            for (auto& pnt : road.points) {
                if (pnt.isBridge) {
                    char filename[256];
                    sprintf(filename, "%s//bridges//bridge_%s.x", rootPath.c_str(), pnt.bridgeName.c_str());
                    if (pnt.bridgeName.length() == 0) {
                        sprintf(filename, "%s//bridges//please name me %d.x", rootPath.c_str(), cnt);
                    }
                    FILE* file = fopen(filename, "w");
                    if (file) {
                        fprintf(file, "xof 0303txt 0032\n");
                        fprintf(file, "\n");
                        fprintf(file, "Frame Root {\n");
                        fprintf(file, "FrameTransformMatrix { 100.0, 0.0, 0.0, 0.0,    0.0, 100.0, 0.0, 0.0,   0.0, 0.0, 100.0, 0.0,   0.0, 0.0, 0.0, 1.0;;  }\n");
                        fprintf(file, "\n");


                        glm::vec4 bezier[2][4];
                        bezier[0][0] = glm::vec4(pnt.bezier[left].pos, 0);
                        bezier[0][1] = glm::vec4(pnt.bezier[left].forward(), 0);
                        bezier[0][2] = glm::vec4(road.points[cnt + 1].bezier[left].backward(), 0);
                        bezier[0][3] = glm::vec4(road.points[cnt + 1].bezier[left].pos, 0);

                        bezier[1][0] = glm::vec4(pnt.bezier[right].pos, 0);
                        bezier[1][1] = glm::vec4(pnt.bezier[right].forward(), 0);
                        bezier[1][2] = glm::vec4(road.points[cnt + 1].bezier[right].backward(), 0);
                        bezier[1][3] = glm::vec4(road.points[cnt + 1].bezier[right].pos, 0);

                        glm::vec3 origin = (bezier[0][0] + bezier[0][3] + bezier[1][0] + bezier[1][3]) * 0.25f;
                        fprintf(file, "# origin %f, %f, %f\n", origin.x, origin.y, origin.z);
                        fprintf(file, "# origin- MAX %f, %f, %f\n", origin.x, -origin.z, origin.y);

                        fprintf(listfile, "%5.4f, %5.4f, %5.4f, %s\n", origin.x, origin.y, origin.z, pnt.bridgeName.c_str());

                        // NOW lod6 tile to use with
                        float tileSize = 40000.0f / pow(2.0f, 6.0f);
                        uint x = (uint)floor((origin.x + 20000) / tileSize);
                        uint y = (uint)floor((origin.z + 20000) / tileSize);
                        fprintf(file, "# tile lod 6, y - %d, x - %d, size - %f, origin(x, z) %f %f\n", y, x, tileSize / 248.0f * 256.0f, x * tileSize - (tileSize / 248.0f * 4.0f) - 20000, y * tileSize - (tileSize / 248.0f * 4.0f) - 20000);
                        fprintf(file, "# tile MAX, size middle - %f, origin(x, y) %f, %f, 0.0\n", tileSize / 248.0f * 256.0f, x * tileSize + (tileSize / 2.0f) - 20000, (-1) * (y * tileSize + (tileSize / 2.0f) - 20000));



                        fprintf(file, "Mesh{\n");
                        fprintf(file, "128;\n");

                        for (int y = 0; y < 64; y++) {
                            float t = (float)y / 63.0f;
                            bezierPoint* pntThis = &pnt.bezier[left];
                            bezierPoint* pntNext = &road.points[cnt + 1].bezier[left];
                            glm::vec3 A = cubic_Casteljau(t, pntThis, pntNext);// -origin;

                            pntThis = &pnt.bezier[right];
                            pntNext = &road.points[cnt + 1].bezier[right];
                            glm::vec3 B = cubic_Casteljau(t, pntThis, pntNext);// -origin; absolute coordinates

                            fprintf(file, "%f; %f; %f;,\n", A.x, A.y, A.z);
                            fprintf(file, "%f; %f; %f;,\n", B.x, B.y, B.z);

                            pMesh->mVertices[y * 2 + 0] = aiVector3D(A.x, A.y, A.z);
                            pMesh->mVertices[y * 2 + 1] = aiVector3D(B.x, B.y, B.z);
                        }

                        fprintf(file, "\n");
                        fprintf(file, "63;\n");
                        for (int i = 0; i < 63; i++) {
                            uint start = i * 2;
                            fprintf(file, "4;%d, %d, %d, %d;,\n", start, start + 1, start + 3, start + 2);
                        }
                        fprintf(file, "}\n");
                        fprintf(file, "\n");

                        fprintf(file, "}\n");
                        fprintf(file, "\n");

                        fclose(file);
                    }
                }
                cnt++;
            }
        }

        fclose(listfile);
    }


    Exporter exp;
    exp.Export(scene, "collada", "f:/test_bridges.dae");
    exp.Export(scene, "fbx", "f://test_bridges.fbx");
    exp.Export(scene, "obj", "f://test_bridges.obj");
}



void roadNetwork::exportRoads()
{
    int numRoad = (int)roadSectionsList.size();

    aiScene* scene = new aiScene;
    scene->mRootNode = new aiNode();

    scene->mMaterials = new aiMaterial * [1];
    scene->mMaterials[0] = nullptr;
    scene->mNumMaterials = 1;

    scene->mMaterials[0] = new aiMaterial();

    scene->mMeshes = new aiMesh * [numRoad];
    scene->mRootNode->mMeshes = new unsigned int[numRoad];
    for (int i = 0; i < numRoad; i++)
    {
        scene->mMeshes[i] = nullptr;
        //scene->mMeshes[i] = new aiMesh();
        //scene->mMeshes[i]->mMaterialIndex = 0;
        scene->mRootNode->mMeshes[i] = i;
    }
    scene->mNumMeshes = numRoad;
    scene->mRootNode->mNumMeshes = numRoad;


    int cnt = 0;
    for (auto& road : roadSectionsList)
    {
        scene->mMeshes[cnt] = new aiMesh();
        scene->mMeshes[cnt]->mMaterialIndex = 0;
        auto pMesh = scene->mMeshes[cnt];
        uint numBez = (uint)road.points.size() - 1;

        if (road.points.size() > 2)
        {

            {
                pMesh->mFaces = new aiFace[63 * numBez];
                pMesh->mNumFaces = 63 * numBez;
                pMesh->mPrimitiveTypes = aiPrimitiveType_POLYGON;

                for (uint j = 0; j < numBez; j++)
                {
                    for (uint i = 0; i < 63; i++)
                    {
                        aiFace& face = pMesh->mFaces[j * 63 + i];

                        face.mIndices = new unsigned int[4];
                        face.mNumIndices = 4;

                        face.mIndices[0] = (j * 64 + i) * 2 + 0;
                        face.mIndices[1] = (j * 64 + i) * 2 + 1;
                        face.mIndices[2] = (j * 64 + i) * 2 + 3;
                        face.mIndices[3] = (j * 64 + i) * 2 + 2;
                    }
                }
            }

            {
                pMesh->mVertices = new aiVector3D[128 * numBez];
                pMesh->mNumVertices = 128 * numBez;


                //for (auto &pnt : road.points)
                for (uint ZZ = 0; ZZ < numBez; ZZ++)
                {
                    auto& pnt = road.points[ZZ];

                    for (int y = 0; y < 64; y++)
                    {
                        float t = (float)y / 63.0f;
                        bezierPoint* pntThis = &pnt.bezier[left];
                        bezierPoint* pntNext = &road.points[ZZ + 1].bezier[left];
                        glm::vec3 A = cubic_Casteljau(t, pntThis, pntNext);// -origin;

                        pntThis = &pnt.bezier[right];
                        pntNext = &road.points[ZZ + 1].bezier[right];
                        glm::vec3 B = cubic_Casteljau(t, pntThis, pntNext);// -origin; absolute coordinates

                        pMesh->mVertices[ZZ * 128 + y * 2 + 0] = aiVector3D(A.x, A.y, A.z);
                        pMesh->mVertices[ZZ * 128 + y * 2 + 1] = aiVector3D(B.x, B.y, B.z);
                    }
                }
            }
        }
        else
        {
            pMesh->mNumFaces = 0;
            pMesh->mNumVertices = 0;
        }

        cnt++;
    }


    Exporter exp;
    exp.Export(scene, "collada", "f:/test_roads.dae");
    exp.Export(scene, "fbx", "f://test_roads.fbx");
    exp.Export(scene, "obj", "f://test_roads.obj");
}




void roadNetwork::exportAI()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"track_layout"} };
    if (saveFileDialog(filters, path))
    {
        FILE* file = fopen(path.string().c_str(), "wb");
        pathBezier.exportAI(file);
        fclose(file);

        FILE* fileTxt = fopen((path.string() + ".txt").c_str(), "w");
        pathBezier.exportAITXTDEBUG(file);
        fclose(fileTxt);
    }
}



void roadNetwork::loadAI()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"track_layout"} };
    if (openFileDialog(filters, path))
    {
        FILE* file = fopen(path.string().c_str(), "rb");
        pathBezier.load(file);
        fclose(file);
    }
}



void roadNetwork::addRoad() {
    if (bHIT) {
        AI_bezier::road R;
        R.roadIndex = hitRoadGuid;
        R.bForward = true;
        pathBezier.roads.push_back(R);
    }
}



void roadNetwork::exportAI_CSV()
{
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"csv"} };
    if (saveFileDialog(filters, path))
    {
        FILE* file = fopen((path.string() + ".left.csv").c_str(), "w");
        pathBezier.exportCSV(file, 0);
        fclose(file);

        file = fopen((path.string() + ".center.csv").c_str(), "w");
        pathBezier.exportCSV(file, 1);
        fclose(file);

        file = fopen((path.string() + ".right.csv").c_str(), "w");
        pathBezier.exportCSV(file, 2);
        fclose(file);

        file = fopen((path.string() + ".GATES.csv").c_str(), "w");
        pathBezier.exportGates(file);
        fclose(file);
    }
}



/*	temp code to push some material data top all troad to populate Eifel*/
void roadNetwork::TEMP_pushAllMaterial()
{
    if (this->currentRoad)
    {
        for (auto& roadSection : this->roadSectionsList)
        {
            for (auto& point : roadSection.points)
            {
                point.matRight = currentRoad->points[0].matRight;
                point.matLeft = currentRoad->points[0].matLeft;
                point.matCenter = currentRoad->points[0].matCenter;
            }
        }
    }
}



void roadNetwork::currentRoadtoAI()
{
    /*	This is a hacky version
        just push the whole current road
    */
    if (currentRoad)
    {
        pathBezier.clear();

        // Nordsliefe hillclimb
        //pathBezier.startpos = vec3(5713.95f, 561.05f, 1859.83f);
        //pathBezier.endpos = vec3(6623.0f, 550.0f, 2360.0f);

        // Full Nordsliefe not looped yet
        //pathBezier.startpos = vec3(3916.0f, 575.0f, 4942.0f);
        //pathBezier.endpos = vec3(4009.0f, 570.0f, 4883.0f);

        // OVAL
        pathBezier.startpos = glm::vec3(0.0f, 0.5f, 61.0f);
        pathBezier.endpos = glm::vec3(0.0f, 0.5f, 61.0f);

        // IMOLA
        //pathBezier.startpos = vec3(50.3f, 17.08f, -407.9f);
        //pathBezier.endpos = vec3(50.3f, 17.08f, -407.9f);

        // Nordsliefe test best curve
        //pathBezier.startpos = vec3(232.0f, 386.0f, 1431.0f);
        //pathBezier.endpos = vec3(6491.0f, 558.0f, 2412.0f);

        // Hairpin hillclimb backwards
        //pathBezier.startpos = vec3(3383.0f, 409.0f, 2369.0f);
        //pathBezier.endpos = vec3(3220.0f, 369.0f, 2443.0f);

        // CLSOE LOOP GETS FIERST POINT
        pathBezier.isClosedPath = currentRoad->isClosedLoop;
        if (currentRoad->isClosedLoop)
        {
            pathBezier.startpos = currentRoad->points[0].anchor;
            pathBezier.endpos = currentRoad->points[0].anchor;
        }

        uint size = (uint)currentRoad->points.size() - 1;
        glm::vec4 bez[2][4];
        for (uint i = 0; i < size; i++)
        {

            bez[0][0] = currentRoad->points[i].bezier[0].pos_uv();
            bez[0][1] = currentRoad->points[i].bezier[0].forward_uv();
            bez[0][2] = currentRoad->points[i + 1].bezier[0].backward_uv();
            bez[0][3] = currentRoad->points[i + 1].bezier[0].pos_uv();

            bez[1][0] = currentRoad->points[i].bezier[2].pos_uv();
            bez[1][1] = currentRoad->points[i].bezier[2].forward_uv();
            bez[1][2] = currentRoad->points[i + 1].bezier[2].backward_uv();
            bez[1][3] = currentRoad->points[i + 1].bezier[2].pos_uv();

            pathBezier.pushSegment(bez);
        }

        // and the last one
        //bez[0][0] = currentRoad->points[size].bezier[0].pos_uv();
        //bez[1][0] = currentRoad->points[size].bezier[2].pos_uv();
        //pathBezier.pushSegment(bez);

        pathBezier.cacheAll();
        pathBezier.solveStartEnd();
    }
}



void roadNetwork::incrementLane(int index, float _amount, roadSection* _road)
{
    if (index < 0)		// do all
    {
        if (editRight) {
            for (auto& pnt : _road->points) {
                pnt.lanesRight[editLaneIndex].laneWidth += _amount;
                if ((editLaneIndex > 0) && pnt.lanesRight[editLaneIndex].laneWidth < 0) pnt.lanesRight[editLaneIndex].laneWidth = 0;
            }
        }
        else {
            for (auto& pnt : _road->points) {
                pnt.lanesLeft[editLaneIndex].laneWidth += _amount;
                if ((editLaneIndex > 0) && pnt.lanesLeft[editLaneIndex].laneWidth < 0) pnt.lanesLeft[editLaneIndex].laneWidth = 0;
            }
        }
    }
    else				// selective
    {
        if (editRight) {
            _road->points[index].lanesRight[editLaneIndex].laneWidth += _amount;
            if ((editLaneIndex > 0) && _road->points[index].lanesRight[editLaneIndex].laneWidth < 0) _road->points[index].lanesRight[editLaneIndex].laneWidth = 0;
            _road->lastEditedPoint = _road->points[index];
        }
        else {
            _road->points[index].lanesLeft[editLaneIndex].laneWidth += _amount;
            if ((editLaneIndex > 0) && _road->points[index].lanesLeft[editLaneIndex].laneWidth < 0) _road->points[index].lanesLeft[editLaneIndex].laneWidth = 0;
            _road->lastEditedPoint = _road->points[index];
        }
    }
}



void roadNetwork::updateDynamicRoad()
{
    uint bezierCount = 0;
    odeBezier.clear();
    roadNetwork::staticBezierData.clear();
    roadNetwork::staticIndexData.clear();

    if (currentRoad) {
        //currentRoad->convertToGPU_Stylized(&bezierCount, false, selectFrom, selectTo);
        currentRoad->convertToGPU_Realistic(staticBezierData, staticIndexData, selectFrom, selectTo, true, showMaterials);

    }

    if (currentIntersection) {
        for (int i = 0; i < currentIntersection->roadLinks.size(); i++) {
            currentIntersection->roadLinks[i].roadPtr = &roadSectionsList.at(currentIntersection->roadLinks[i].roadGUID);
            //currentIntersection->roadLinks[i].roadPtr->convertToGPU_Stylized(&bezierCount);
            currentIntersection->roadLinks[i].roadPtr->convertToGPU_Realistic(staticBezierData, staticIndexData, 0, 0, true, false);
        }
        currentIntersection->convertToGPU(staticBezierData, staticIndexData, &bezierCount);
    }

    debugNumBezier = (uint)staticBezierData.size();
    debugNumIndex = (uint)staticIndexData.size();

}



void roadNetwork::updateAllRoads(bool _forExport)
{
    uint bezierCount = 0;
    odeBezier.clear();
    roadNetwork::staticBezierData.clear();
    roadNetwork::staticIndexData.clear();
    //roadNetwork::staticIndexDataSolid.clear();

    for (auto& roadSection : roadSectionsList) {
        roadSection.convertToGPU_Realistic(staticBezierData, staticIndexData);
    }

    //	for (auto &intersection : intersectionList) {
    //		intersection.convertToGPU(&bezierCount, _forExport);
    //	}

    debugNumBezier = (uint)staticBezierData.size();
    debugNumIndex = (uint)staticIndexData.size();

    odeBezier.buildGridFromBezier();
}

void roadNetwork::physicsTest(glm::vec3 pos)
{
    physicsMouseIntersection.updatePosition(glm::vec2(pos.x, pos.z));
    odeBezier.intersect(&physicsMouseIntersection);
}



void roadNetwork::lanesFromHit()
{
    bHIT = false;
    hitRandomFeedbackValue = 0;

    for (int i = 0; i < physicsMouseIntersection.beziers.size(); i++)
    {
        bezierOneIntersection* pI = &physicsMouseIntersection.beziers[i];
        hitRandomFeedbackValue++;

        if (pI->bHit) {
            bHIT = true;
            hitRoadGuid = pI->roadGUID;
            hitRoadIndex = pI->index;
            hitRoadLane;
            hitRoadRight = pI->bRighthand;
            dA = pI->a;
            dW = pI->W;

            roadSection* road = &roadSectionsList[hitRoadGuid];

            float dA = 1.0f - pI->UV.x;
            float dB = pI->UV.x;
            float sum = 0;
            float widthRight = (road->points[hitRoadIndex].widthRight * dA) + (road->points[hitRoadIndex + 1].widthRight * dB);
            float widthLeft = (road->points[hitRoadIndex].widthLeft * dA) + (road->points[hitRoadIndex + 1].widthLeft * dB);

            pI->a = dB;
            pI->W = widthRight;

            if (pI->bRighthand) {
                for (int i = 0; i < 7; i++) {
                    sum += (road->points[hitRoadIndex].lanesRight[i].laneWidth * dA) + (road->points[hitRoadIndex + 1].lanesRight[i].laneWidth * dB);
                    if (sum / widthRight > pI->UV.y) {
                        hitRoadLane = i;
                        break;
                    }
                }
            }
            else {
                for (int i = 0; i < 7; i++) {
                    sum += (road->points[hitRoadIndex].lanesLeft[i].laneWidth * dA) + (road->points[hitRoadIndex + 1].lanesLeft[i].laneWidth * dB);
                    if (sum / widthLeft > pI->UV.y) {
                        hitRoadLane = i;
                        break;
                    }
                }
            }
        }
    }
}



void roadNetwork::testHit(glm::vec3 pos)
{
    physicsMouseIntersection.updatePosition(glm::vec2(pos.x, pos.z));
    odeBezier.intersect(&physicsMouseIntersection);

    lanesFromHit();
}



void roadNetwork::testHitAI(glm::vec3 pos)
{
    AIpathIntersect.setPos(glm::vec2(pos.x, pos.z));

    for (uint i = 0; i < 1000; i++)
    {
        pathBezier.intersect(&AIpathIntersect);
    }
}



void roadNetwork::doSelect(glm::vec3 pos)
{
    // first subselection on curren road
    if (currentRoad)
    {
        for (int j = 0; j < currentRoad->points.size(); j++) {
            if (glm::length(currentRoad->points[j].anchor - pos) < 3.0f)
            {
                currentRoad->addSelection(j);
                return;
            }
        }
    }

    float distance = 10000;
    // then intersections
    for (int i = 0; i < intersectionList.size(); i++) {
        float L = glm::length(intersectionList[i].anchor - pos);
        if (L < 10.0f && L < distance) {
            currentIntersection = &intersectionList[i];
            distance = L;
            currentRoad = nullptr;
        }
    }

    //then new selectiosn on roads in general
    for (int i = 0; i < roadSectionsList.size(); i++) {
        for (int j = 0; j < roadSectionsList[i].points.size(); j++) {
            float L = glm::length(roadSectionsList[i].points[j].anchor - pos);
            if (L < 15.0f && L < distance)
            {
                distance = L;
                currentIntersection = nullptr;
                currentRoad = &roadSectionsList[i];				// new selection
                currentRoad->newSelection(j);
                currentRoad->solveRoad();
            }
        }
    }

    /*
    if (!currentIntersection)
    {
        currentRoad = &roadSectionsList[871];
    }*/
}



void roadNetwork::currentIntersection_findRoads()
{
    intersectionRoadLink link;
    currentIntersection->roadLinks.clear();

    float linkDistance = 10.0f;		// if closer than this, consider it a link

    for (auto& road : roadSectionsList)
    {
        if (road.points.size() >= 3)
        {
            if (glm::length(road.points[0].anchor - currentIntersection->anchor) < linkDistance)
            {
                road.int_GUID_start = currentIntersection->GUID;

                link.roadPtr = &road;
                link.roadGUID = road.GUID;
                link.broadStart = true;
                float3 dirStart = road.points[1].anchor - currentIntersection->anchor;
                link.angle = atan2(dirStart.x, dirStart.z);
                currentIntersection->roadLinks.push_back(link);
            }
            else if (glm::length(road.points[road.points.size() - 1].anchor - currentIntersection->anchor) < linkDistance)
            {
                road.int_GUID_end = currentIntersection->GUID;

                link.roadPtr = &road;
                link.roadGUID = road.GUID;
                link.broadStart = false;
                float3 dirEnd = road.points[road.points.size() - 2].anchor - currentIntersection->anchor;
                link.angle = atan2(dirEnd.x, dirEnd.z);
                currentIntersection->roadLinks.push_back(link);
            }
        }
    }

    std::sort(currentIntersection->roadLinks.begin(), currentIntersection->roadLinks.end());
    solveIntersection(true);
}



void roadNetwork::solve2RoadIntersection()
{
    for (auto& link : currentIntersection->roadLinks)								// move the road vertex to the intersection position
    {
        if (link.broadStart) {
            link.roadPtr->points.front().forceAnchor(currentIntersection->getAnchor(), currentIntersection->anchorNormal, currentIntersection->lidarLOD);
        }
        else {
            link.roadPtr->points.back().forceAnchor(currentIntersection->getAnchor(), currentIntersection->anchorNormal, currentIntersection->lidarLOD);
        }
    }
}



void roadNetwork::solveIntersection(bool _solveTangents)
{
    int numRoads = (int)currentIntersection->roadLinks.size();
    if (numRoads < 2) return;														// stop breaking the single road intersections

    //if (numRoads == 2) return solve2RoadIntersection();	// Do later but important


    for (auto& link : currentIntersection->roadLinks)								// move the road vertex to the intersection position
    {
        if (link.broadStart)
        {
            if (numRoads > 2) link.roadPtr->points.front().bAutoGenerate = false;
            link.roadPtr->points.front().forceAnchor(currentIntersection->getAnchor(), currentIntersection->anchorNormal, currentIntersection->lidarLOD);
        }
        else
        {
            if (numRoads > 2) link.roadPtr->points.back().bAutoGenerate = false;
            link.roadPtr->points.back().forceAnchor(currentIntersection->getAnchor(), currentIntersection->anchorNormal, currentIntersection->lidarLOD);
        }
    }


    // solve forward
    for (int i = 0; i < numRoads; i++)
    {
        intersectionRoadLink* link_A = &currentIntersection->roadLinks[i];
        intersectionRoadLink* link_B = &currentIntersection->roadLinks[(i + 1) % numRoads];
        splinePoint* pnt_A, * pnt_B;
        float width_A, width_B;

        if (link_A->broadStart) {
            pnt_A = &link_A->roadPtr->points[1];
            width_A = pnt_A->widthLeft;
        }
        else {
            pnt_A = &link_A->roadPtr->points[link_A->roadPtr->points.size() - 2];
            width_A = pnt_A->widthRight;
        }

        if (link_B->broadStart) {
            pnt_B = &link_B->roadPtr->points[1];
            width_B = pnt_B->widthRight;
        }
        else {
            pnt_B = &link_B->roadPtr->points[link_B->roadPtr->points.size() - 2];
            width_B = pnt_B->widthLeft;
        }

        float3 dir_A = glm::normalize(pnt_A->anchor - currentIntersection->getAnchor());
        float3 dir_B = glm::normalize(pnt_B->anchor - currentIntersection->getAnchor());
        link_A->theta = acos(glm::dot(dir_A, dir_B));
        float sintheta = sin(link_A->theta);
        float costheta = cos(link_A->theta);


        float3 right_A = glm::normalize(glm::cross(-dir_A, currentIntersection->anchorNormal));
        float3 right_B = glm::normalize(glm::cross(dir_B, currentIntersection->anchorNormal));

        dir_A = glm::normalize(glm::cross(currentIntersection->anchorNormal, -right_A));
        dir_B = glm::normalize(glm::cross(currentIntersection->anchorNormal, right_B));

        float3 vec_C, pos_C, tangent_C;

        if (link_A->cornerType == typeOfCorner::automatic) {
            link_A->cornerRadius = __max(2.0f, pow(width_A + width_B, 0.5f) * 1.4f * link_A->theta);		/// FIXME this can do with a project wide scale adjustment but seems good for now
        }

        if ((glm::dot(right_A, dir_B) < 0.5) || (numRoads == 2))			// this is an open corner, flat side of a T junction
        {
            float distance = (width_A + width_B) / 2.0f;
            //vec_C = glm::normalize(glm::normalize(right_A) + glm::normalize(right_B)) * distance;
            vec_C = (right_A * width_A + right_B * width_B) * 0.5f;
            tangent_C = glm::normalize(glm::cross(vec_C, currentIntersection->anchorNormal));
            pos_C = currentIntersection->getAnchor() + vec_C;
            link_A->bOpenCorner = true;

            link_A->pushBack_A = distance * 2;
            link_B->pushBack_B = distance * 2;
        }
        else {
            vec_C = dir_B * ((width_A + link_A->cornerRadius) / sintheta) + dir_A * ((width_B + link_A->cornerRadius) / sintheta);			//(wo + r0)/sintheta 5, 3
            //float3 C0 = dir_B * (width_A / sintheta);
            //float3 C1 = dir_A * (width_B / sintheta);
            //tangent_C = glm::normalize(C1 - C0);
            tangent_C = glm::normalize(glm::cross(vec_C, currentIntersection->anchorNormal));
            //float3 Cdir = glm::normalize(glm::normalize(C0) + glm::normalize(C1));
            //pos_C = currentIntersection->anchor + vec_C - (Cdir * link_A->cornerRadius);									// (r0 + r1)/2
            pos_C = currentIntersection->getAnchor() + vec_C - (glm::normalize(vec_C) * link_A->cornerRadius * 0.8f);									// (r0 + r1)/2
            link_A->bOpenCorner = false;

            link_A->pushBack_A = glm::dot(vec_C, dir_A) + link_A->cornerRadius * 0.8f;	// FIXME make these interactive so I can tweak it
            link_B->pushBack_B = glm::dot(vec_C, dir_B) + link_A->cornerRadius * 0.8f;
        }



        link_A->cornerUp_A = currentIntersection->anchorNormal;
        link_B->cornerUp_B = currentIntersection->anchorNormal;

        if (link_A->cornerType != typeOfCorner::artistic) {
            link_A->corner_A = pos_C;
            link_B->corner_B = pos_C;



            if (link_A->bOpenCorner) {
                link_A->cornerTangent_A = tangent_C * link_A->pushBack_A * 0.2f;
                link_B->cornerTangent_B = -tangent_C * link_B->pushBack_B * 0.2f;
            }
            else {
                float phi = 1.57079632679489f - (link_A->theta / 2.0f);
                float D = phi * link_A->cornerRadius / 3.0f;
                link_A->cornerTangent_A = tangent_C * D;
                link_B->cornerTangent_B = -tangent_C * D;
            }

        }
    }


    int splineSegment = 0;
    int indexSegment = 0;
    // do the pushback and set roads
    for (int i = 0; i < numRoads; i++) {

        intersectionRoadLink* link_A = &currentIntersection->roadLinks[i];
        intersectionRoadLink* link_PREV = &currentIntersection->roadLinks[(i - 1 + numRoads) % numRoads];

        splinePoint* pnt_A1, * pnt_A0;
        if (link_A->broadStart) {
            pnt_A0 = &link_A->roadPtr->points[0];
            pnt_A1 = &link_A->roadPtr->points[1];
        }
        else {
            pnt_A0 = &link_A->roadPtr->points[link_A->roadPtr->points.size() - 1];
            pnt_A1 = &link_A->roadPtr->points[link_A->roadPtr->points.size() - 2];
        }

        link_A->pushBack = __max(__max(link_A->pushBack_A, link_A->pushBack_B), pnt_A1->widthLeft + pnt_A1->widthRight);
        float3 dir = glm::normalize(pnt_A1->anchor - currentIntersection->getAnchor());

        // only push beack if we have enought points
        // thsi looks problematic - dir comes from A1
        if (!currentIntersection->doNotPush)
        {
            if (link_A->roadPtr->points.size() > 3) {
                pnt_A1->setAnchor(currentIntersection->getAnchor() + dir * link_A->pushBack, currentIntersection->anchorNormal, currentIntersection->lidarLOD);
            }
        }

        // OK this is the sorce of our issue - 
        // FIXME
        // CALC BETTER IF WE DOTN PUSH

        glm::vec3 left = glm::cross(currentIntersection->anchorNormal, dir);
        if (_solveTangents)
        {
            link_A->tangentVector = glm::normalize(glm::cross(left, currentIntersection->anchorNormal)) * link_A->pushBack * 0.33f;

            if (link_A->bOpenCorner) {
                link_A->tangentVector = glm::normalize(link_A->cornerTangent_A) * link_A->pushBack * 0.13f;
            }
            if (link_PREV->bOpenCorner) {
                link_A->tangentVector = glm::normalize(-link_PREV->cornerTangent_A) * link_A->pushBack * 0.13f;
            }

            if (link_A->tangentVector.x == 0)
            {
                bool bCM = true;
            }
        }

        //link_A->road->solveRoad();	// This is overkill but fix spline rendering first
    }

    // do the tangents from correct pushbacks
    // do pushback and set roads
    for (int i = 0; i < numRoads; i++)
    {
        intersectionRoadLink* link_A = &currentIntersection->roadLinks[i];
        intersectionRoadLink* link_B = &currentIntersection->roadLinks[(i - 1 + numRoads) % numRoads];

        if (link_A->cornerType != typeOfCorner::artistic) {
            if (!link_A->bOpenCorner) {
                float blend = glm::clamp((link_A->theta - 1.7f) * 3.0f, 0.0f, 1.0f);
                float r1 = tan(link_A->theta / 4.0f) * 1.333f * link_A->cornerRadius;
                float r2 = link_A->pushBack * 0.33f;
                float r = r2 * blend + r1 * (1.0f - blend);
                link_A->cornerTangent_A *= r;
            }
            else {
                link_A->cornerTangent_A *= link_A->pushBack * 0.33;
            }
        }

        if (link_B->cornerType != typeOfCorner::artistic) {
            if (!link_B->bOpenCorner) {
                float blend = glm::clamp((link_B->theta - 1.7f) * 3.0f, 0.0f, 1.0f);
                float r1 = tan(link_B->theta / 4.0f) * 1.333f * link_B->cornerRadius;
                float r2 = link_B->pushBack * 0.33f;
                float r = r2 * blend + r1 * (1.0f - blend);
                link_A->cornerTangent_B *= r;
            }
            else
            {
                link_A->cornerTangent_B *= link_B->pushBack * 0.33;
            }
        }
    }


    for (int i = 0; i < numRoads; i++)
    {
        currentIntersection->roadLinks[i].roadPtr->solveRoad();
    }
}



// basic editing
void  roadNetwork::breakCurrentRoad(uint _index)
{
    if (!currentRoad || _index >= currentRoad->points.size()) {
        return;
    }

    uint currentGUID = currentRoad->GUID;

    //new road
    roadSectionsList.emplace_back();
    roadSection& newRoad = roadSectionsList.back();
    newRoad.GUID = (int)roadSectionsList.size() - 1;

    currentRoad = &roadSectionsList[currentGUID];
    uint currentSize = (uint)currentRoad->points.size();

    for (uint i = _index; i < currentSize; i++)
    {
        newRoad.points.push_back(currentRoad->points[i]);
    }

    if (currentRoad->int_GUID_end >= 0)
    {
        intersectionRoadLink* link = intersectionList[currentRoad->int_GUID_end].findLink(currentGUID);
        if (link)
        {
            newRoad.int_GUID_end = currentRoad->int_GUID_end;
            link->roadGUID = newRoad.GUID;
            link->roadPtr = &roadSectionsList[newRoad.GUID];
            currentRoad->int_GUID_end = -1;
        }
    }

    for (uint i = _index + 1; i < currentSize; i++)
    {
        currentRoad->points.pop_back();
    }
}



void  roadNetwork::deleteCurrentRoad()
{
    currentRoad->points.clear();

    if (currentRoad->int_GUID_start >= 0) {
        currentIntersection = &intersectionList[currentRoad->int_GUID_start];
        currentIntersection_findRoads();
        currentRoad->int_GUID_start = -1;
        currentIntersection = nullptr;
    }

    if (currentRoad->int_GUID_end >= 0) {
        currentIntersection = &intersectionList[currentRoad->int_GUID_end];
        currentIntersection_findRoads();
        currentRoad->int_GUID_end = -1;
        currentIntersection = nullptr;
    }

    currentRoad->solveRoad();
    currentRoad = nullptr;
}



/*	Tricky function since we dont actually delete, it will break the GUID's
    we could in theory moce to std::map rather than std::array, would likely solve this
    for now boolean, dont do anything with it isValid dont display anything*/
void  roadNetwork::deleteCurrentIntersection()
{
    for (auto& link : currentIntersection->roadLinks)
    {
        roadSectionsList[link.roadGUID].breakLink(link.broadStart);
    }

    currentIntersection->roadLinks.clear();
    currentIntersection->anchor = float3(0, 0, 0);
    currentIntersection = nullptr;
}



void  roadNetwork::copyVertex(uint _index)
{
    clipboardPoint = currentRoad->points[_index];
    hasClipboardPoint = true;
}



void  roadNetwork::pasteVertexGeometry(uint _index)
{
    if (hasClipboardPoint) {
        for (int i = 0; i < 9; i++) {
            currentRoad->points[_index].lanesLeft[i] = clipboardPoint.lanesLeft[i];
            currentRoad->points[_index].lanesRight[i] = clipboardPoint.lanesRight[i];
        }
        currentRoad->points[_index].centerlineType = clipboardPoint.centerlineType;
        currentRoad->solveRoad();
    }
}



void roadNetwork::pasteVertexMaterial(uint _index)
{
    if (hasClipboardPoint) {
        for (int v = selectFrom; v < selectTo; v++) {
            for (int i = 0; i < 15; i++) {
                currentRoad->points[v].matLeft[i] = clipboardPoint.matLeft[i];
                currentRoad->points[v].matRight[i] = clipboardPoint.matRight[i];
            }
            for (int i = 0; i < 2; i++) {
                currentRoad->points[v].matCenter[i] = clipboardPoint.matCenter[i];
            }
        }
    }
}



void roadNetwork::pasteVertexAll(uint _index)
{
    if (hasClipboardPoint) {
        for (int i = 0; i < 9; i++) {
            currentRoad->points[_index].lanesLeft[i] = clipboardPoint.lanesLeft[i];
            currentRoad->points[_index].lanesRight[i] = clipboardPoint.lanesRight[i];
        }
        currentRoad->points[_index].centerlineType = clipboardPoint.centerlineType;
        currentRoad->solveRoad();


        for (int i = 0; i < 15; i++) {
            currentRoad->points[_index].matLeft[i] = clipboardPoint.matLeft[i];
            currentRoad->points[_index].matRight[i] = clipboardPoint.matRight[i];
        }
        for (int i = 0; i < 2; i++) {
            currentRoad->points[_index].matCenter[i] = clipboardPoint.matCenter[i];
        }
    }
}




