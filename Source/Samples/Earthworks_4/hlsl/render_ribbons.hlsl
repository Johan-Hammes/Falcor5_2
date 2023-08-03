
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"		




/*
struct ribbonVertex
{
    (1, 1, 14, 16)bit position        // start EMPTY x y
    (14, 10, 8)                     // z, material, v
    (9, 8, 8, 7) yaw pitch, radius mm up to 1m dws 2m width, 7 bits free (32 ? u)            // up
    (9, 8, 7, 8) yaw pitch, cone, depth         // lighting    0.7 degree 7 bit cone 8 bit depth
        lerp(0, 1, (dot + cone)*.5 + .5  )
};

struct ribbonVertex_6
{
    (1, 1, 14, 16)  type, start, x, y        // type (ribbon, flat ribbon)
    (14, 10, 8)     z, material, animation-blend
    (9, 8, 15)      bitangent yaw pitch, v
    (9, 8, 7, 8)    tangent yaw pitch, u, radius
    (9, 8, 7, 8)    yaw pitch, cone, depth         // lighting    0.7 degree 7 bit cone 8 bit depth
    (8, 8, 8, 8)    ao, shadow, albedoScale, trandslucencyScale,
// FUCK bend down middle of the leaf data V, amount, how many triangles to spit out etc
};

struct triVertex_6
{
    (2, 14, 16)     position        // type (ribbon, flat ribbon, trimesh) x y
    (14, 10, 8)     z, material, anmation-blend
    (9, 8, 15)   bitangent yaw pitch, v
    (9, 8, 15)    tangent yaw pitch, u
    (9, 8, 7, 8)    yaw pitch, cone, depth         // lighting    0.7 degree 7 bit cone 8 bit depth  ??????? NOT THIS maybe fancy AO
    (8, 8, 8, 8)        ao, shadow, color, trandslucency, can I pack this
};

{
    int type;
    bool ribbonstartBit;
    float3 position;
    int material;
    float anim_blend;
    float2 uv;
    float3 bitangent;
    float3 tangent;
    float radius;
    float3 lightDir;
    float lightCone;
    float lightDepth;
    uchar ao, sahdow, albedoeascale, translucencysacle;
}


//??? mix ribbons and orientated ribbons in one, or split -
mixing them might be best for animation
{
    2           type            {ribbon, aligned ribbon, triangle list}
    1           start bit
    14,16,14    position        {maybe larger to work for buildings as well 16, 16, 16 - 2mm 128m big ?}
    10          material        {more if we have space}
    12, 12      u, v            {8 bits to 1, plus 4 bits over, ot smaller with UV scale value}
    9, 8        bitangent/up (yaw, pitch)
    8           radius
    9, 8, 7, 8  lighting (yaw, pitch, cone, depth)      {can be used }

    9, 8        tangent (yaw, pitch)
    8, 8, 8, 8  colour with translucency

    8           ao
    8           shadow  ?? per vertex or per object maybe
    8           anim_blend
}

To world space - needed for atmosphere lookup
World to Sun -> for lighting, so can we push cone directly to sun space, i.e. do all lighting in VS
Tangent space -> texture normal mapping ??? can we do somethign to simplify this part, we do not need acurate normals here
*/



SamplerState gSampler;
Texture2D gAlbedo : register(t0);
TextureCube gEnv : register(t1);

struct ribbonTextures
{
    Texture2D<float4> T[4096];
};
ParameterBlock<ribbonTextures>
textures;


struct RV
{
    int4 v;
};
struct RV6
{
    int a;
    int b;
    int c;
    int d;
    int e;
    int f;
};
StructuredBuffer<sprite_material>   materials;
StructuredBuffer<RV6> instanceBuffer;
StructuredBuffer<xformed_PLANT>     instances;


cbuffer gConstantBuffer
{
    float4x4 viewproj;
    float3 eyePos;

    // Meshlet related ----------------------------------------------------------
    uint fakeShadow;
    float3 objectScale;

    float treeDensity;
    float treeScale;        // unused
    float radiusScale;
};




struct PSIn
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float3 diffuseLight : COLOR;
    float3 specularLight : COLOR1;
    float2 uv : TEXCOORD;                           // texture uv
    float4 lighting : TEXCOORD1;                    // uv, sunlight, ao
    nointerpolation uint4 flags : TEXCOORD2;        // material
    float3 eye : TEXCOORD3;
    float3 colour : TEXCOORD4;
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
    sincos((yaw - 256) * 0.01227 + r, z, x);
    sincos((pitch - 128) * 0.01227, y, plane);
    return float3(plane * x, y, plane * z);
}


inline void extractPosition(inout PSIn o, const RV6 v, const float scale, const float rotation)
{
    o.pos.xyz = rot_xz(float3((v.a >> 16) & 0x3fff, v.a & 0xffff, (v.b >> 18) & 0x3fff) * objectScale * scale - 1 * float3(0.8198, 0.04096, 0.8192), rotation);
    o.pos.w = 1;
    o.eye = normalize(o.pos.xyz - eyePos);
}

inline void extractTangent(inout PSIn o, const RV6 v, const float rotation)
{
    o.binormal = yawPitch_9_8bit(v.c >> 23, (v.c >> 15) & 0xff, rotation); // remember to add rotation to yaw
    o.tangent = normalize(cross(o.binormal, o.eye));
    o.normal = cross(o.binormal, o.tangent);
}

inline void extractTangentFlat(inout PSIn o, const RV6 v, const float rotation)
{
    o.binormal = yawPitch_9_8bit(v.c >> 23, (v.c >> 15) & 0xff, rotation); // remember to add rotation to yaw
    o.tangent = yawPitch_9_8bit(v.d >> 23, (v.d >> 15) & 0xff, rotation); // remember to add rotation to yaw
    o.normal = cross(o.binormal, o.tangent);
}

inline void extractUVRibbon(inout PSIn o, const RV6 v)
{
    o.uv = float2(((v.d >> 8) & 0x7f) * 0.00390625, ((v.c) & 0x7fff) * 0.00390625); // allows 16 V repeats scales are wrong decide
}

inline void extractFlags(inout PSIn o, const RV6 v)
{
    o.flags.x = (v.b >> 8) & 0x2ff;     // material
    o.flags.y = (v.a >> 30) & 0x1; //start bool
    o.flags.z = (v.d & 0xff);           // int radius
}

inline void extractColor(inout PSIn o, const RV6 v)
{
    o.colour.x = 0.1 + ((v.f >> 8) & 0xff) * 0.008; // albedo
    o.colour.y = ((v.f >> 0) & 0xff) * 0.008; //tanslucency
    o.colour.z = 0.5;
}

inline void light(inout PSIn o, const RV6 v, const float rotation)
{
    const float3 sunDir = -normalize(float3(1.0, 0.7, -0.8));
    float3 lightCone = yawPitch_9_8bit(v.e >> 23, (v.e >> 15) & 0xff, 0);
    float cone = (((v.e >> 8) & 0x7f) * 0.01575) - 1;
    float d = (v.e & 0xff) * 0.00784;
    float a = saturate(dot(normalize(lightCone + sunDir * cone * 0), sunDir));
    float b = a * (20 - d) + d;
    float ao = 1 - (d * 0.3f);
    o.lighting = float4(0, 0, saturate(1 - (b * 0.5)), ao);
}
/*
void extract(inout PSIn o, const uint4 v)
{
    o.pos.xyz += rot_xz(float3((v.x >> 16) & 0x3fff, v.x & 0xffff, (v.y >> 18) & 0x3fff) * objectScale - 2 * float3(8.198, 4.096, 8.192), 0);
    o.pos.w = 1;
    float3 eye = normalize(o.pos.xyz - eyePos);
    o.binormal = yawPitch_9_8bit(v.z >> 23, (v.z >> 15) & 0xff);        // remember to add rotation to yaw
    o.tangent = normalize(cross(o.binormal, eye));
    o.normal = cross(o.binormal, o.tangent);
    //o.diffuseLight
    //o.specularLight
    o.uv = float2((v.z & 0x7f) * 0.00390625, (v.y & 0xff) * 0.0625);        // allows 16 V repeats
    o.flags.x = (v.y >> 8) & 0x2ff;         // material
    o.flags.y = v.x >> 31;                  //start bool
    o.flags.z = (v.z >> 7) & 0xff; // int radius

    // now light it
    const float3 sunDir = -normalize(float3(1.0, 0.7, -0.8));
    float3 lightCone = yawPitch_9_8bit(v.w >> 23, (v.w >> 15) & 0xff);
    float cone = (((v.w >> 8) & 0x7f) * 0.01575) - 1;
    float d = (v.w & 0xff) * 0.00784;
    float a = saturate(dot(normalize(lightCone + sunDir * cone * 0), sunDir));
    float b = a * (20 - d) + d;
    float ao = 1 - (d * 0.3f);
    o.lighting = float4(0, 0, saturate(1 - (b * 0.5)), ao);
}
*/

float rand_1_05(in float2 uv)
{
    float2 noise = (frac(sin(dot(uv, float2(12.9898, 78.233) * 2.0)) * 43758.5453));
    return abs(noise.x + noise.y) * 0.5;
}

PSIn vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    PSIn output = (PSIn) 0;
    const uint P = 0; // plant.index * 128; // rename meshlet
    float3 rootPosition = float3((iId % 100) * 0.15f, 0, ((int) (iId / 100)) * 0.15f) - float3(5, 0, 5);
    rootPosition += 0.7f * float3(  rand_1_05(float2(iId * 0.92356, iId * 0.003568)), 0, rand_1_05(float2(iId * 0.2356, iId * 1.003568)));
    float scale = 0.8f + 0.6f * rand_1_05(float2(iId * 1.2356, iId * 2.303568));
    float rotation =     3.14f * rand_1_05(float2(iId * 2.2356, iId * 1.703568));
    const RV6 v = instanceBuffer[vId];

    extractPosition(output, v, scale, rotation);
    if (v.a >> 31)
    {
        extractTangentFlat(output, v, rotation);
    }
    else
    {
        extractTangent(output, v, rotation);
    }        
    extractUVRibbon(output, v);
    extractFlags(output, v);
    extractColor(output, v);
    light(output, v, rotation);


    output.pos.xyz += rootPosition;
    
    return output;
}


[maxvertexcount(4)]
void gsMain(line PSIn L[2], inout TriangleStream<PSIn> OutputStream)
{
    PSIn v;

    if (L[1].flags.y == 1)
    {


        

        v = L[0];
        v.uv.x = 0.5 + L[0].uv.x;
        v.pos = mul(L[0].pos - float4(L[0].tangent * L[0].flags.z * radiusScale, 0), viewproj);
        OutputStream.Append(v);


        v = L[1];
        v.uv.x = 0.5 + L[1].uv.x;
        v.pos = mul(L[1].pos - float4(L[1].tangent * L[1].flags.z * radiusScale, 0), viewproj);
        OutputStream.Append(v);

        


        v = L[0];
        v.uv.x = 0.5 - L[0].uv.x;
        v.pos = mul(L[0].pos + float4(L[0].tangent * L[0].flags.z * radiusScale, 0), viewproj);
        OutputStream.Append(v);

        v = L[1];
        v.uv.x = 0.5 - L[1].uv.x;
        v.pos = mul(L[1].pos + float4(L[1].tangent * L[1].flags.z * radiusScale, 0), viewproj);
        OutputStream.Append(v);

    }
}


float4 psMain(PSIn vOut, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    //return 1; this douvle speed even oif no pixels aredraw
    int material = vOut.flags.x;
 //   if (material == 2)
 //   {
 //       material = 1;
 //   }

    // works but saves almost nothing
 //   if ((abs(ddx(vOut.uv.x)) + abs(ddy(vOut.uv.x))) < 0.1)
 //   {
 //       return float4(0, 0, 0, 1);
 //   }

    float alpha = textures.T[material * 2 + 0].Sample(gSampler, vOut.uv.xy).a;  // replace later
    //if (alpha < 0.7)
    //    return float4(1.0,0, 0, 0.4);
    //clip(alpha - 0.5);
    
    float3 albedo = textures.T[material * 2 + 0].Sample(gSampler, vOut.uv.xy).rgb * vOut.colour.x;
    

    //float3 normalTex = ((textures.T[material * 2 + 1].Sample(gSampler, vOut.uv.xy).rgb) * 2.0) - 1.0;   //? from rg
    float3 normalTex = ((textures.T[material * 2 + 1].Sample(gSampler, vOut.uv.xy).rgb) * 2.0) - 1.0;
    float3 N = (normalTex.r * vOut.tangent) + (normalTex.g * vOut.binormal) + (normalTex.b * vOut.normal);
    N *= (isFrontFace * 2 - 1);

    

    const float3 sunDir = normalize(float3(1.0, 0.7, -0.8));
    const float3 sunColor = float3(1.2, 1.0, 0.7) * 1.5;
    float ndotv = saturate(dot(N, vOut.eye));
    float ndoth = saturate(dot(N, normalize(sunDir - vOut.eye)));
    float sun = saturate(dot(N, sunDir)) * vOut.lighting.z;
    float back = saturate(dot(N, -sunDir)) * vOut.lighting.z * vOut.colour.y;
    float spec = pow(ndoth, 20);

    
    

    float3 color = albedo * sunColor * sun ;
    

    if (material > 0)
    {    
        color += albedo * sunColor * float3(1, 1, 0.3) * back;
    }
    
    // specular sun
    color += pow(ndoth, 25) * 0.4;
    
    // specular env???, or just single
    
    //diffuse env
    color += gEnv.SampleLevel(gSampler, N * float3(1, 1, -1), 0).rgb * albedo * vOut.lighting.w;
    
    
    
    
    return float4(color, 1);

}
