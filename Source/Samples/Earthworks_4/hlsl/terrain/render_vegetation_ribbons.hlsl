
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"		
#include "vegetation_defines.hlsli"
#include "../render_Common.hlsli"




SamplerState gSampler;
SamplerState gSamplerClamp; // for blending with hald-buffer
Texture2D gAlbedo : register(t0);
TextureCube gEnv : register(t1);
Texture2D gHalfBuffer : register(t2);

Texture2D gDappledLight : register(t3);

struct ribbonTextures
{
    Texture2D<float4> T[4096];
};
ParameterBlock<ribbonTextures>
textures;



StructuredBuffer<sprite_material> materials;
//StructuredBuffer<ribbonVertex8> instanceBuffer;     // THIS HAS TO GO
//StructuredBuffer<xformed_PLANT> instances;

StructuredBuffer<plant> plant_buffer;
StructuredBuffer<_plant_anim_pivot> plant_pivot_buffer;
StructuredBuffer<plant_instance> instance_buffer;
StructuredBuffer<block_data> block_buffer;
StructuredBuffer<ribbonVertex8> vertex_buffer;

cbuffer gConstantBuffer
{
    float4x4 viewproj;
    float3 eyePos;
    
    float time;
    float bake_radius_alpha;
    float bake_height_alpha;
    int bake_AoToAlbedo;

    float3 windDir;
    float windStrength;
};


cbuffer PerFrameCB
{
    bool gConstColor;
    float3 gAmbient;
	
	// volume fog parameters
    float2 screenSize;
    float fog_far_Start;
    float fog_far_log_F; //(k-1 / k) / log(far)		// FIXME might be k-2 to make up for half pixel offsets
	
    float fog_far_one_over_k; // 1.0 / k
    float fog_near_Start;
    float fog_near_log_F; //(k-1 / k) / log(far)		// FIXME might be k-2 to make up for half pixel offsets
    float fog_near_one_over_k; // 1.0 / k
	
    float gisOverlayStrength;
    int showGIS;
    float redStrength;
    float redScale;
    float4 gisBox;
	
    float redOffset;
    float3 padding;
};

// PDIN flags
#define AmbietOcclusion lighting.w
#define AlbedoScale colour.x
#define TranslucencyScale colour.y
#define Shadow sunUV.z
#define PlantIdx flags.w
#define isCameraFacing (v.a >> 31)




float4 sunLight(float3 posKm)
{
    float2 sunUV;
    float dX = dot(posKm.xz, normalize(sunDirection.xz));
    sunUV.x = saturate(0.5 - (dX / 1600.0f));
    sunUV.y = 1 - saturate(posKm.y / 100.0f);
    return SunInAtmosphere.SampleLevel(gSmpLinearClamp, sunUV, 0) * 0.07957747154594766788444188168626;
}


struct PSIn
{
    float4 pos : SV_POSITION;
    
    
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    
    float3 diffuseLight : COLOR;
    float4 vertexLight : COLOR1;
    
    float2 uv : TEXCOORD; // texture uv
    float4 lighting : TEXCOORD1; // uv, sunlight, ao
    nointerpolation uint4 flags : TEXCOORD2; // material        BLENDINDICES[n]
    float3 eye : TEXCOORD3; // can this become SVPR  or per plant somehow, feels overkill here per vertex
    float4 colour : TEXCOORD4;
    float3 lineScale : TEXCOORD5; // its w and h for boillboard
    float3 sunUV : TEXCOORD6;
    float4 viewTangent : TEXCOORD7;
    float4 viewBinormal : TEXCOORD8;
    float3 debugColour : TEXCOORD9;
};



inline float3 rot_xz(const float3 v, const float yaw)
{
    float s, c;
    sincos(yaw, s, c);
    return float3((v.x * c) + (v.z * s), v.y, (-v.x * s) + (v.z * c));
}


inline float3 yawPitch_9_8bit(int yaw, int pitch, const float r) // 9, 8 bits 8 b
{
    float plane, x, y, z;
    sincos((yaw - 256) * 0.01227 - r, z, x);
    sincos((pitch - 128) * 0.01227, y, plane);
    return float3(plane * x, y, plane * z);
}



// These two can optimize, its only tangent that differs
inline void extractTangent(inout PSIn o, const ribbonVertex8 v, const float rotation)
{
    o.binormal = yawPitch_9_8bit(v.c >> 23, (v.c >> 15) & 0xff, 0); // remember to add rotation to yaw
    o.tangent = yawPitch_9_8bit(v.d >> 23, (v.d >> 15) & 0xff, 0); // remember to add rotation to yaw
}

inline void extractUVRibbon(inout PSIn o, const ribbonVertex8 v)
{
    o.uv = float2(((v.d >> 8) & 0x7f) * 0.00390625, ((v.c) & 0x7fff) * 0.00390625); // allows 16 V repeats scales are wrong decide
    //o.uv.y = 1 - o.uv.y;
}

inline void extractFlags(inout PSIn o, const ribbonVertex8 v)
{
    o.flags.x = (v.b >> 8) & 0x2ff; //  material
    o.flags.y = ((v.a >> 30) & 0x1) + ((v.b & 0x1) << 1); //  start bool   we can double pack
    o.flags.z = (v.d & 0xff); //  int radius
}


float rand_1_05(in float2 uv)
{
    float2 noise = (frac(sin(dot(uv, float2(12.9898, 78.233) * 2.0)) * 43758.5453));
    return frac(noise.x + noise.y);
}

float3x3 AngleAxis3x3(float angle, float3 axis)
{
    float c, s;
    sincos(angle, s, c);

    float t = 1 - c;
    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    return float3x3(
        t * x * x + c, t * x * y - s * z, t * x * z + s * y,
        t * x * y + s * z, t * y * y + c, t * y * z - s * x,
        t * x * z - s * y, t * y * z + s * x, t * z * z + c
    );
}










void bezierLeaf(_plant_anim_pivot PVT, inout float3 pos, inout float3 binorm, inout float3 tangent, float3 wind, float freq_scale, float tIn)
{
    const float3 rel = pos - PVT.root;
    const float S = length(rel);
    const float3 right = normalize(cross(rel, wind));

    wind /= PVT.stiffness; //??? pow() to scale effect better, sane for wind as well
    
    //  now ossilate
    float swayStrength = 0.15 * sin(time / PVT.frequency * 6.283 * freq_scale + PVT.offset);
    float sideStrength = 0.20 * sin(time / PVT.frequency * 4.283 * freq_scale + PVT.offset + 1);
    float3 c = normalize(rel) + tIn * (wind * (1 + swayStrength)) + tIn * (right * length(wind) * sideStrength);
    binorm += c - normalize(rel);
    binorm = normalize(binorm);

    pos = PVT.root + normalize(c) * S;

    float flutterStrength = length(wind) * 0.5 * PVT.shift * sin(time / PVT.frequency * 4.283 * freq_scale);
    float3 N = cross(binorm, tangent);
    tangent = normalize(tangent + N * flutterStrength);
}


void bezierPivotSum(_plant_anim_pivot PVT, inout float3 pos, inout float3 binorm, float3 wind, float freq_scale)
{
    const float S = 1.f / length(PVT.extent);
    const float3 rel = pos - PVT.root;
    const float3 b = normalize(rel) * 0.5; // feel like I should combine these two
    const float t = length(rel) / S;
    const float3 right = normalize(cross(rel, wind));

    wind /= PVT.stiffness;  //??? pow() to scale effect better, sane for wind as well
    
    //  now ossilate
    float swayStrength = 0.15 * sin(time / PVT.frequency * 6.283 * freq_scale + PVT.offset);
    float sideStrength = 0.20 * sin(time / PVT.frequency * 4.283 * freq_scale + PVT.offset + 1);
    float3 c = b * 2 + (wind * (1 + swayStrength)) + (right * length(wind) * sideStrength);
    
    float3 bc = normalize(c - b) * 0.5;
    c = b + bc;
    float scale = (1 / length(c) + 2.5) / 3.5 * S;

    float3 f = b * t;
    float3 g = lerp(b, c, t);
    binorm += normalize(g - f) - (b * 2);
    binorm = normalize(binorm);

    pos = PVT.root + lerp(f, g, t) * scale;
}


#define A ((_v.g >> 24) & 0xff)
#define B ((_v.g >> 16) & 0xff)
#define C ((_v.g >> 8) & 0xff)
#define D (_v.g & 0xff)
#define F (_v.h & 0xff)

float3 allPivotsSum(inout float3 _position, inout float3 _binormal, inout float3 _tangent, ribbonVertex8 _v, uint _vId,
                    const plant _plant, const plant_instance _instance)
{
    
    // WIND ################################################################################################################
    float dx = dot(_instance.position.xz, windDir.xz) * 0.4; // so repeat roughly every 100m
    float newWindStrenth = (1 + 0.6 * pow(sin(dx - time * windStrength * 0.025), 1)); //so 0.3 - 1.7 of set speed   pow(0.2 and 0.6 bot steepends it)
    newWindStrenth = windStrength * (0.4 + smoothstep(0.4, 1.3, newWindStrenth));
    float ss = 0; //    sin(_instance.position.x * 0.3 - time * 0.4) + sin(_instance.position.z * 0.3 - time * 0.3); // swirl strenth -2 to 2
    float3x3 rot = AngleAxis3x3(ss * 0.3, float3(0, 1, 0));
    float3 NewWindDir = normalize(mul(windDir, rot));

    //newWindStrenth += 0.00000001;
    //newWindStrenth = windStrength * 10; // * sin(time * 0.5);
    NewWindDir = rot_xz(NewWindDir, -_instance.rotation) * newWindStrenth * 0.01;
    NewWindDir.y = 0;
    

    if (newWindStrenth < 0.00000001)
        return 0;

    const uint pivotOffset = _instance.plant_idx * 256;


    if (F > 0)
    {
        _plant_anim_pivot PVT;
        ribbonVertex8 vRoot = vertex_buffer[_vId - F];
        //pivots[numPivots].root = rot_xz(float3((vRoot.a >> 16) & 0x3fff, vRoot.a & 0xffff, (vRoot.b >> 18) & 0x3fff) * _plant.scale - _plant.offset, _instance.rotation);
        PVT.root = float3((vRoot.a >> 16) & 0x3fff, vRoot.a & 0xffff, (vRoot.b >> 18) & 0x3fff) * _plant.scale - _plant.offset;
        //PVT.extent = yawPitch_9_8bit(vRoot.c >> 23, (vRoot.c >> 15) & 0xff, 0);
        //PVT.extent = normalize(PVT.extent) * 20.01;
        //PVT.extent = PVT.root + float3(0, 0.1, 0);
        PVT.stiffness = 1 / (((_v.h >> 16) & 0xff) * 0.004);
        PVT.frequency = ((_v.h >> 8) & 0xff) * 0.004;
        PVT.shift = ((_v.h >> 24) & 0xff) * 0.004;
        PVT.offset = _vId - F;
        //bezierPivotSum(PVT, _position, _binormal, NewWindDir, _instance.scale);
        bezierLeaf(PVT, _position, _binormal, _tangent, NewWindDir, _instance.scale, PVT.shift);

        //float flutterStrength = newWindStrenth * 0.005 * PVT.shift * sin(time / PVT.frequency * 4.283 * _instance.scale) / PVT.stiffness;

        //float3 N = cross(_binormal, _tangent);

        //_tangent = normalize(_tangent + N * flutterStrength);

    }

    if (B < 255)
    {
        //bezierPivotSum(plant_pivot_buffer[B + pivotOffset], _position, _binormal, NewWindDir, _instance.scale);
    }

    if (A < 255)
    {
        bezierPivotSum(plant_pivot_buffer[A + pivotOffset], _position, _binormal, NewWindDir, _instance.scale);
    }


    // FIXME add debug colours again, both _t and pivotpoitns
    return 0;
}


// packing flags
#define packDiamond (v.b & 0x1)
#define unpackPosition() float3((v.a >> 16) & 0x3fff, v.a & 0xffff, (v.b >> 18) & 0x3fff)

PSIn vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    PSIn output = (PSIn) 0;

#if defined(_BILLBOARD)
    const plant_instance INSTANCE = instance_buffer[vId];
    const plant PLANT = plant_buffer[INSTANCE.plant_idx];

    output.pos.xyz = INSTANCE.position;
    output.pos.w = 1;
    output.eye = normalize(output.pos.xyz - eyePos);

    output.binormal = float3(0, 1, 0);
    output.tangent = normalize(cross(output.binormal, -output.eye));
    output.normal = cross(output.binormal, output.tangent);

    output.AlbedoScale = 1;
    output.TranslucencyScale = 1;

    output.Shadow = 0;
    output.AmbietOcclusion = 1;

    if ((dot(output.pos.xyz, sunRightVector)) > 5 && (dot(output.pos.xyz, sunRightVector)) < 8)
    {
        output.Shadow = 1;
    }

    output.flags.x = PLANT.billboardMaterialIndex;
    output.PlantIdx = INSTANCE.plant_idx;

    output.lineScale.xy = PLANT.size;// * INSTANCE.scale;

    output.lineScale.z = 0.6;

    output.viewTangent = mul( float4( output.tangent * output.lineScale.x, 0), viewproj);
    output.viewBinormal = mul( float4( output.binormal * output.lineScale.y, 0), viewproj);
    output.pos = mul(output.pos, viewproj);

    output.diffuseLight = sunLight(output.pos.xyz * 0.001).rgb;
    output.diffuseLight = sunLight(INSTANCE.position * 0.001).rgb;
    
    return output;


#else
    const block_data BLOCK = block_buffer[iId];
    const plant_instance INSTANCE = instance_buffer[BLOCK.instance_idx];
    const plant PLANT = plant_buffer[INSTANCE.plant_idx];
    const ribbonVertex8 v = vertex_buffer[BLOCK.vertex_offset + vId];

    // position
    //float3 p = unpackPosition() * PLANT.scale - PLANT.offset;
    output.pos = float4(unpackPosition() * PLANT.scale - PLANT.offset, 1);
    //output.pos = float4(rot_xz(p, INSTANCE.rotation), 1);

#if defined(_BAKE)
    float3 rootPos = float3(0, 0, 0);
    float3 p = unpackPosition() * PLANT.scale - PLANT.offset;
    output.pos =  float4(rot_xz(p, 1.57079632679), 1);        // because of clumps we want to rotate to catch their side, for symmetrical it doesnt matter
        
    p.y = 0;
    float R = length(p);
    if (R > 0.3f)      output.colour.a = 0;
    output.colour.a = 1.f - smoothstep(bake_radius_alpha * 0.85f, bake_radius_alpha, R);
    output.colour.a *= (1.f - smoothstep(bake_height_alpha * 0.9f, bake_height_alpha, output.pos.y)); //last 10% f16tof32 tip asdouble well

    extractTangent(output, v, 1.57079632679);
#endif


    extractTangent(output, v, INSTANCE.rotation); // Likely only after animate, do only once
    extractUVRibbon(output, v);
    extractFlags(output, v);

    output.AlbedoScale = 0.1 + ((v.f >> 8) & 0xff) * 0.008;
    output.TranslucencyScale = ((v.f >> 0) & 0xff) * 0.008;
    
    {
        float3 lightCone = yawPitch_9_8bit(v.e >> 23, (v.e >> 15) & 0xff, INSTANCE.rotation);
        float sunCone = (((v.e >> 8) & 0x7f) * 0.01575) - 1; //cone7
        float sunDepth = (v.e & 0xff) * 0.00784; //depth8   // sulight % how deep inside this plant 0..1 sun..shadow
        float a = saturate(dot(normalize(lightCone - sunDirection * PLANT.sunTilt), sunDirection)); // - sunCone * 0 sunCosTheta sunCone biasess this bigger or smaller 0 is 180 degrees
        output.Shadow = saturate(a * (sunDepth) + sunDepth); // darker to middle
        output.AmbietOcclusion = pow(((v.f >> 24) / 255.f), 3);

        if ((dot(output.pos.xyz, sunRightVector)) > 5 && (dot(output.pos.xyz, sunRightVector)) < 8)
        {
           // output.Shadow = 1;
        }
    }

    

#if defined(_BAKE)
    output.pos.xyz *= INSTANCE.scale;
    float3 root = INSTANCE.position;
    root.y = 0;
    output.pos.xyz += root;

    output.eye = float3(0, 0, -1);
#else
    output.debugColour = allPivotsSum(output.pos.xyz, output.binormal, output.tangent, v, BLOCK.vertex_offset + vId, PLANT, INSTANCE);

    //SCALE and root here.
    output.pos.xyz = rot_xz(output.pos.xyz, INSTANCE.rotation);
    output.pos.xyz *= INSTANCE.scale;
    output.pos.xyz += INSTANCE.position;

    // Rotate teh binormal and tenegmt here
    output.binormal = rot_xz(output.binormal, INSTANCE.rotation);
    output.tangent = rot_xz(output.tangent, INSTANCE.rotation); // write sinlge rot_xz that can do all 4

    output.eye = normalize(output.pos.xyz - eyePos);
#endif
    
    
    if (!isCameraFacing)
    {
        output.tangent = normalize(cross(output.binormal, -output.eye));
    }
    output.normal = cross(output.binormal, output.tangent);
    output.diffuseLight = sunLight(output.pos.xyz * 0.001).rgb; // should really happen lower but i guess its fast
    output.lineScale.x = pow(output.flags.z / 255.f, 2) * PLANT.radiusScale;
    output.PlantIdx = INSTANCE.plant_idx;

#if defined(_GOURAUD_SHADING)    
    // now light it
    {
        const sprite_material MAT = materials[output.flags.x];
        float3 albedo = textures.T[MAT.albedoTexture].SampleLevel(gSamplerClamp, float2(0.5, 0.5), 8).rgb;

        float dappled = pow(1 - output.Shadow, 2);
        float3 N = output.normal;
        if (dot(N, output.eye) < 0)
            N *= -1;
        float ndoth = saturate(dot(N, normalize(sunDirection + output.eye)));
        float ndots = dot(N, sunDirection);

        output.vertexLight.rgb = output.diffuseLight * 3.14 * (saturate(ndots)) * albedo * dappled;
        
        // environment cube light
        output.vertexLight.rgb += 1.16 * gEnv.SampleLevel(gSampler, N * float3(1, 1, -1), 5).rgb * albedo.rgb * pow(output.AmbietOcclusion, 0.3);
    
    // specular sunlight
        float RGH = MAT.roughness[0] + 0.001; //?? frontback
        float pw = 1.f / RGH;
        output.vertexLight.rgb += pow(ndoth, pw) * 0.1 * dappled * output.diffuseLight;

        output.vertexLight.a = output.lineScale.x / length(output.pos.xyz - eyePos) * 10;
    }
#endif  //_GOURAUD_SHADING

    return output;
#endif
}




#if defined(_BILLBOARD)
[maxvertexcount(4)]
void gsMain(point PSIn pt[1], inout TriangleStream<PSIn> OutputStream)
{
    PSIn v = pt[0];
    float scale = 1 - pt[0].lineScale.z;

    v.uv = float2(0.5, 1.1);
    v.pos = pt[0].pos - pt[0].viewBinormal * 0.1;
    v.AmbietOcclusion = 0.3;
    OutputStream.Append(v);

    v.uv = float2(1.0 -  scale/2 , 0.5);
    v.pos = pt[0].pos + pt[0].viewTangent * pt[0].lineScale.z + pt[0].viewBinormal * 0.5;
    v.AmbietOcclusion = 0.6;
    OutputStream.Append(v);
        
    v.uv = float2(0.0 + scale/2, 0.5);
    v.pos = pt[0].pos - pt[0].viewTangent * pt[0].lineScale.z + pt[0].viewBinormal * 0.5;
    OutputStream.Append(v);

    v.uv = float2(0.5, -0.1);
    v.pos = pt[0].pos + pt[0].viewBinormal * 1.1;
    v.AmbietOcclusion = 1.0;
    OutputStream.Append(v);

/*
    v.uv = float2(0.0, 1);
    //v.AmbietOcclusion = 1.03;
    v.pos = pt[0].pos - pt[0].viewTangent;
    OutputStream.Append(v);

    v.uv = float2(1.0, 1);
    v.pos = pt[0].pos + pt[0].viewTangent;
    OutputStream.Append(v);
        
    v.uv = float2(0.0, 0);
    //v.AmbietOcclusion = 1.0;
    v.pos = pt[0].pos - pt[0].viewTangent + pt[0].viewBinormal;
    OutputStream.Append(v);

    v.uv = float2(1.0, 0);
    v.pos = pt[0].pos + pt[0].viewTangent + pt[0].viewBinormal;
    OutputStream.Append(v);
*/
  
}
#else
// HOLY fukcing shit, this is bad anythign above 6
// at 6 I can still get gain out of this
[maxvertexcount(4)]
void gsMain(line PSIn L[2], inout TriangleStream<PSIn> OutputStream)
{
    PSIn v;

    if ((L[1].flags.y & 0x1) == 1)
    {
        const bool diamond = (L[0].flags.y >> 1);
        if (diamond)
        {
            PSIn v = L[0];
            float scale = 0; //            1 - L[0].lineScale.z;

            v.uv = float2(0.5, 1.1);
            // first one is correct v.pos = pt[0].pos - pt[0].viewBinormal * 0.1;
            v.sunUV.x = dot(v.pos.xyz, sunRightVector);
            v.sunUV.y = dot(v.pos.xyz, sunUpVector);
            v.pos = mul(v.pos, viewproj);
            OutputStream.Append(v);

            // we should really interpolate here, but use start fo now
            v.uv = float2(1.0 + scale / 2, 0.5);
            v.pos = (L[0].pos + L[1].pos) * 0.5 + float4(v.tangent * v.lineScale.x, 0);
            v.sunUV.x = dot(v.pos.xyz, sunRightVector);
            v.sunUV.y = dot(v.pos.xyz, sunUpVector);
            v.pos = mul(v.pos, viewproj);
            OutputStream.Append(v);
        
            v.uv = float2(0.0 - scale / 2, 0.5);
            v.pos = (L[0].pos + L[1].pos) * 0.5 - float4(v.tangent * v.lineScale.x, 0);
            v.sunUV.x = dot(v.pos.xyz, sunRightVector);
            v.sunUV.y = dot(v.pos.xyz, sunUpVector);
            v.pos = mul(v.pos, viewproj);
            OutputStream.Append(v);

            v = L[1];
            v.uv = float2(0.5, -0.1);
            v.pos = L[1].pos + 0.1 * (L[1].pos - L[0].pos);
            // last one is correct v.pos = pt[0].pos + pt[0].viewBinormal * 1.1;
            v.sunUV.x = dot(v.pos.xyz, sunRightVector);
            v.sunUV.y = dot(v.pos.xyz, sunUpVector);
            v.pos = mul(v.pos, viewproj);
            OutputStream.Append(v);
            
        }
        else
        {
            v = L[0];
            v.uv.x = 0.5 + L[0].uv.x;
            v.pos = L[0].pos - float4(v.tangent * v.lineScale.x, 0);
            v.sunUV.x = dot(v.pos.xyz, sunRightVector);
            v.sunUV.y = dot(v.pos.xyz, sunUpVector);
        //v.pos = mul(L[0].pos + float4(v.tangent * v.lineScale, 0), viewproj);
            v.eye = normalize(v.pos.xyz - eyePos);
            v.pos = mul(v.pos, viewproj);
            OutputStream.Append(v);

            v.uv.x = 0.5 - L[0].uv.x;
            v.pos = L[0].pos + float4(v.tangent * v.lineScale.x, 0);
            v.sunUV.x = dot(v.pos.xyz, sunRightVector);
            v.sunUV.y = dot(v.pos.xyz, sunUpVector);
        //v.pos = mul(L[0].pos - float4(v.tangent * v.lineScale, 0), viewproj);
            v.eye = normalize(v.pos.xyz - eyePos);
            v.pos = mul(v.pos, viewproj);
            OutputStream.Append(v);

            v = L[1];
            v.uv.x = 0.5 + L[1].uv.x;
            v.pos = L[1].pos - float4(v.tangent * v.lineScale.x, 0);
            v.sunUV.x = dot(v.pos.xyz, sunRightVector);
            v.sunUV.y = dot(v.pos.xyz, sunUpVector);
            v.eye = normalize(v.pos.xyz - eyePos);
            v.pos = mul(v.pos, viewproj);
            OutputStream.Append(v);

            v.uv.x = 0.5 - L[1].uv.x;
            v.pos = L[1].pos + float4(v.tangent * v.lineScale.x, 0);
            v.sunUV.x = dot(v.pos.xyz, sunRightVector);
            v.sunUV.y = dot(v.pos.xyz, sunUpVector);
            v.eye = normalize(v.pos.xyz - eyePos);
            v.pos = mul(v.pos, viewproj);
            OutputStream.Append(v);
        }
    }
}
#endif






#if defined(_BAKE)

struct PS_OUTPUT_Bake
{
    float4 albedo: SV_Target0;
    float4 normal: SV_Target1;
    float4 normal_8: SV_Target2;
    float4 pbr: SV_Target3;
    float4 extra: SV_Target4;
};

PS_OUTPUT_Bake psMain(PSIn vOut, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    PS_OUTPUT_Bake output = (PS_OUTPUT_Bake)0;

    sprite_material MAT = materials[vOut.flags.x];


    float alpha = textures.T[MAT.albedoTexture].Sample(gSampler, vOut.uv.xy).a;
    if (MAT.alphaTexture >= 0)
    {
        //alpha = textures.T[MAT.alphaTexture].Sample(gSampler, vOut.uv.xy).r;
        //
    }
    alpha *= vOut.colour.a;

    float rnd = 0.25 + 0.5 * rand_1_05(vOut.pos.xy);
    clip(alpha - rnd);

    float3 color = textures.T[MAT.albedoTexture].Sample(gSampler, vOut.uv.xy).rgb;// * vOut.AlbedoScale * pow(vOut.AmbietOcclusion, 2);


    int frontback = (int) !isFrontFace;
    color *= vOut.AlbedoScale  * MAT.albedoScale[frontback] * 2.f;
    if(bake_AoToAlbedo)
    {
        color = color * (0.9 + 0.1 * vOut.AmbietOcclusion);
    }
    output.albedo = float4(pow(color, 1.0 / 2.2), 1);
    

    float3 N = vOut.normal;
    if (MAT.normalTexture >= 0)
    {
        float3 n = (textures.T[MAT.normalTexture].Sample(gSampler, vOut.uv.xy).rgb * 2) - 1;
        N = (n.r * vOut.tangent) + (n.g * vOut.binormal) + (n.b * vOut.normal);
    }
    N *= (isFrontFace * 2 - 1);


    
//lighting.rgb
    float3 NAdjusted = N;
    NAdjusted = normalize(vOut.lighting.rgb);

    float3 NCone =vOut.lighting.rbg;
    NCone.r *= -1;
    NCone.b *= -1;

   // N = normalize(NCone + N * 0.3);
    

    NAdjusted.r = -N.r * 0.5 + 0.5;
    NAdjusted.g = N.g * 0.5 + 0.5;
    NAdjusted.b = -N.b * 0.5 + 0.5;  //fixme -

//NAdjusted = vOut.lighting.xyz * 0.5 + 0.5;
//NAdjusted.b = 1 - NAdjusted.b;


//float instance_PLANT = saturate(dot(vOut.lighting.xyz, normalize(float3(0.8, 0.4, 0.4))));
//output.albedo *= instance_PLANT * 0.3 + 0.7;

    output.normal = float4(NAdjusted, 1);       // FIXME has to convert to camera space, cant eb too hard , uprigthmaybe, and 0-1 space
    output.normal_8 = float4(NAdjusted, 1);

    

    float trans = vOut.TranslucencyScale * MAT.translucency;
    {
        if (MAT.translucencyTexture >= 0)
        {
            trans *= dot(float3(0.3, 0.4, 0.3), textures.T[MAT.translucencyTexture].Sample(gSampler, vOut.uv.xy).rgb);
        }
    }
    //trans = saturate(pow(trans, 1.0 / 2.2));
    output.extra = float4(0, 0, 0, 1-trans);

    return output;
}

#else

float4 psMain(PSIn vOut, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
/*
    if (isFrontFace)
        return float4(frac(vOut.uv.x), 1, frac(vOut.uv.y), 1);
    else
        return float4(1, 0, vOut.uv.y, 1);
*/
    
    
#if defined(_DEBUG_PIXELS)
    if (vOut.uv.y < 0)
    {
        return float4(0, 0, 1, 1);  // blue for top tip of diamond
    }
    if (!isFrontFace) return float4(1, 0, 1, 1);
#endif

    //  clip(vOut.uv.y);    // clip the extra tip off diamonds

#if defined(_GOURAUD_SHADING)    
    {
        float2 uv = vOut.pos.xy / screenSize; //        float2(2560, 1440);
        float3 prev = gHalfBuffer.Sample(gSamplerClamp, uv).rgb;
        float alphaB = smoothstep(0, 0.3, vOut.uv.x) * smoothstep(1, 0.7, vOut.uv.x);
        float3 colorE = lerp(prev, vOut.vertexLight.rgb, alphaB);
        return float4(colorE, 1);
    }
#endif
    
    
    const sprite_material MAT = materials[vOut.flags.x];
    const plant PLANT = plant_buffer[vOut.PlantIdx];
    const int frontback = (int) !isFrontFace;
    const float flipNormal = (isFrontFace * 2 - 1);
    
    float4 albedo = textures.T[MAT.albedoTexture].SampleBias(gSamplerClamp, vOut.uv.xy, -0.0);
    albedo.rgb *= vOut.AlbedoScale * MAT.albedoScale[frontback] * 2.f;
    float alpha = pow(albedo.a, MAT.alphaPow);

    float rnd = 0.5; //    +0.15 * rand_1_05(vOut.pos.xy);
#if defined(_DEBUG_PIXELS)
    {
        if ((alpha - rnd) < 0)
            return float4(1, 0, 0, 0.3);
    }
#endif
    clip(alpha - rnd);
    alpha = smoothstep(rnd, 0.8, alpha);
    
    

#if defined(_DEBUG_PIVOTS)
    {
        albedo.rgb = albedo.rgb * vOut.debugColour;
        albedo.a = 1;
        return albedo;
    }
#endif

    
    float3 N = vOut.normal;
    if (MAT.normalTexture >= 0)
    {
        float3 normalTex = ((textures.T[MAT.normalTexture].Sample(gSamplerClamp, vOut.uv.xy).rgb) * 2.0) - 1.0;
        N = (normalTex.r * vOut.tangent) + (normalTex.g * vOut.binormal) + (normalTex.b * vOut.normal);
        //albedo.rgb = normalTex;

    }
    N *= flipNormal;
    float ndoth = saturate(dot(N, normalize(sunDirection + vOut.eye)));
    float ndots = dot(N, sunDirection);

    //albedo.a = 1;
    //return albedo;

    // sunlight dapple
    float dappled;
#if defined(_BILLBOARD)
    dappled = 1 - vOut.Shadow;
#else
    dappled = gDappledLight.Sample(gSampler, frac(vOut.sunUV.xy * PLANT.shadowUVScale)).r;
    dappled = smoothstep(vOut.Shadow - PLANT.shadowSoftness, vOut.Shadow + PLANT.shadowSoftness, dappled);
#endif

    
    // sunlight
    float3 color = vOut.diffuseLight * 3.14 * (saturate(ndots)) * albedo.rgb * dappled;
    
    // environment cube light
    color += 0.39 * gEnv.SampleLevel(gSampler, N * float3(1, 1, -1), 0).rgb * albedo.rgb * pow(vOut.AmbietOcclusion, 0.3);
    
    // specular sunlight
    float RGH = MAT.roughness[frontback] + 0.001;
    float pw = 15.f / RGH;
    color += pow(ndoth, pw) * 1.6 * dappled * vOut.diffuseLight * (1 - RGH);

    // translucent light    
    float3 TN = vOut.normal * flipNormal;
    float3 trans = (saturate(-ndots)) * saturate(dot(-sunDirection, vOut.eye)) * vOut.TranslucencyScale * MAT.translucency * dappled;
    {
        if (MAT.translucencyTexture >= 0)
        {
            float t = textures.T[MAT.translucencyTexture].Sample(gSampler, vOut.uv.xy).r;
            trans *= pow(t, 2);
        }
    }
    color += trans * pow(albedo.rgb, 1.5) * 150 * vOut.diffuseLight;

    
    // apply JHFAA to edges    
    if (alpha < 0.9)
    {
        float2 uv = vOut.pos.xy / screenSize; //        float2(2560, 1440);
        //float2 uv = vOut.pos.xy / float2(2560, 1440);
        float3 prev = gHalfBuffer.Sample(gSamplerClamp, uv).rgb;
        color = lerp(prev, color, alpha);
        
#if defined(_DEBUG_PIXELS)
        {
            return 1;
            float V = vOut.diffuseLight.r * 0.5 * (1 - alpha);
            return float4(V, V, 0, 1); // yellow pixels
        }
#endif
    }

    return float4(color, 1);
    
}

#endif

