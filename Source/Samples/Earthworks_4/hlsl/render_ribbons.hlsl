
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
    sincos((yaw - 256) * 0.01227 - r, z, x);
    sincos((pitch - 128) * 0.01227, y, plane);
    return float3(plane * x, y, plane * z);
}


inline void extractPosition(inout PSIn o, const RV6 v, const float scale, const float rotation, const float3 root)
{
    o.pos.xyz = root + rot_xz(float3((v.a >> 16) & 0x3fff, v.a & 0xffff, (v.b >> 18) & 0x3fff) * objectScale * scale - 1 * float3(0.1638, 0.0818, 0.1638), rotation);
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
    o.flags.x = (v.b >> 8) & 0x2ff;     //  material
    o.flags.y =     (v.a >> 30) & 0x1; //  start bool
    o.flags.z = (v.d & 0xff);           //  int radius
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
    float3 lightCone = yawPitch_9_8bit(v.e >> 23, (v.e >> 15) & 0xff, rotation);
    float cone = (((v.e >> 8) & 0x7f) * 0.01575) - 1;
    float d = (v.e & 0xff) * 0.00784;
    float a = saturate(dot(normalize(lightCone + sunDir * (-0.2)), sunDir)); //cone * 0
    float b = a * (3 - d) + d;
    float ao = 1 - (d * 0.3f);
    o.lighting = float4(0, 0, saturate(1 - (b * 0.5)), ao);
}

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
    float scale = 0.5f + 0.5f * rand_1_05(float2(iId * 1.2356, iId * 2.303568));
    float rotation =  3.14f * rand_1_05(float2(iId * 2.2356, iId * 1.703568));
    const RV6 v = instanceBuffer[vId];

#if defined(_BAKE)
    rootPosition = float3(0, 0, 0);
    scale = 1;
    rotation = 0;
#endif
    
    extractPosition(output, v, scale, rotation, rootPosition);
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
        
    return output;
}



[maxvertexcount(4)]
void gsMain(line PSIn L[2], inout TriangleStream<PSIn> OutputStream)
{
    PSIn v;

//    float4 Pmid = 0.5 * (L[0].pos + L[1].pos);
//    Pmid.xyz += normalize(L[0].normal + L[1].normal) * 0.15;    // just 5 cm shift JUST curve
     
    if (L[1].flags.y == 1)
    {
  /*      
        float t = 0;
        for (int i = 0; i <= 10; i++)
        {
            t += 0.11f;
            // bezier interpolate
            float4 Pa = lerp(L[0].pos, Pmid, t);
            float4 Pb = lerp(Pmid, L[1].pos, t);
            float4 P = lerp(Pa, Pb, t);
            //float4 P = lerp(L[0].pos, Pmid, t);
            v = L[0];
            v.tangent = lerp(L[0].tangent, L[1].tangent, t);
            v.binormal = normalize(Pb - Pa).xyz;
            v.uv.y = t;

            v.uv.x = 0;//            0.5 + v.uv.x;
            v.pos = mul(P - float4(v.tangent * L[0].flags.z * radiusScale, 0), viewproj);
            OutputStream.Append(v);

            v.uv.x = 1;//            0.5 + v.uv.x;
            v.pos = mul(P + float4(v.tangent * L[0].flags.z * radiusScale, 0), viewproj);
            OutputStream.Append(v);
        }
        */

        
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


    float alpha = 1;
    if (MAT.alphaTexture >= 0)
    {
        alpha = textures.T[MAT.alphaTexture].Sample(gSampler, vOut.uv.xy).r;
        clip(alpha - 0.5);
    }
    float3 color = textures.T[MAT.albedoTexture].Sample(gSampler, vOut.uv.xy).rgb * vOut.colour.x;
    output.albedo = float4(pow(color, 1.0 / 2.2), 1);
    
    float3 N = vOut.normal;
    if (MAT.normalTexture >= 0)
    {
        float3 n = (textures.T[MAT.normalTexture].Sample(gSampler, vOut.uv.xy).rgb * 2) - 1;
        N = (-n.r * vOut.tangent) + (n.g * vOut.binormal) + (n.b * vOut.normal);
    }
    N *= (isFrontFace * 2 - 1);


    

    float3 NAdjusted = N;
    NAdjusted.r = -N.r * 0.5 + 0.5;
    NAdjusted.g = -N.g * 0.5 + 0.5;
    NAdjusted.b = N.b * 0.5 + 0.5;
    output.normal = float4(NAdjusted, 1);       // FIXME has to convert to camera space, cant eb too hard , uprigthmaybe, and 0-1 space
    output.normal_8 = float4(NAdjusted, 1);

    

    float trans = vOut.colour.y * MAT.translucency;
    {
        if (MAT.translucencyTexture >= 0)
        {
            trans *= dot(float3(0.3, 0.4, 0.3), textures.T[MAT.translucencyTexture].Sample(gSampler, vOut.uv.xy).rgb);
        }
    }
    trans = saturate(trans);
    output.extra = float4(0, 0, 0, 1-trans);

    return output;
}

#else

float4 psMain(PSIn vOut, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    //return 1;
    //return float4(vOut.flags.y, frac(vOut.uv.x), frac(vOut.uv.y), 1);
    //return float4(frac(vOut.uv.y), frac(vOut.uv.y), isFrontFace, 1);
    //return 1; //this douvle speed even oif no pixels aredraw
    
    sprite_material MAT = materials[vOut.flags.x];


    float alpha = 1;
    if (MAT.alphaTexture >= 0)
    {
        alpha = textures.T[MAT.alphaTexture].Sample(gSampler, vOut.uv.xy).r;
        //if (alpha < 0.5)            return float4(1, 0, 0, 1);
        clip(alpha - 0.5);
    }
    //return 1;
    
    float3 N = vOut.normal;
    if (MAT.normalTexture >= 0)
    {
        float3 normalTex = ((textures.T[MAT.normalTexture].Sample(gSampler, vOut.uv.xy).rgb) * 2.0) - 1.0;
        N = (normalTex.g * vOut.tangent) + (-normalTex.r * vOut.binormal) + (normalTex.b * vOut.normal);

        //if ((normalTex.r > -0.01) && (normalTex.r < 0.01))
        //if ((normalTex.r > 0.5))
          //  return float4(1, 0, 0, 1);
    }
    N *= (isFrontFace * 2 - 1);

    
    
        const float3 sunDir = normalize(float3(1.0, 0.9, -0.8));
    const float3 sunColor = float3(1.2, 1.0, 0.7) * 1.5;
    //float ndotv = saturate(dot(N, vOut.eye));
    float ndoth = saturate(dot(N, normalize(sunDir - vOut.eye)));
    float ndots = dot(N, sunDir) * vOut.lighting.z;
    

    float3 color = sunColor * (saturate(ndots)); // forward scattered diffuse light

    float3 albedo = textures.T[MAT.albedoTexture].Sample(gSampler, vOut.uv.xy).rgb;
    //diffuse env
    color *=  albedo * vOut.colour.x;

    color += gEnv.SampleLevel(gSampler, N * float3(1, 1, -1), 0).rgb * 2 * albedo * vOut.lighting.w;
    
    // specular sun
    color += pow(ndoth, 12) * 0.3 *  vOut.lighting.z;
    // specular env???, or just single

    float3 trans = saturate(-ndots * 2) * vOut.colour.y * float3(2, 2, 0.5) * MAT.translucency;
    {
        //if (MAT.translucencyTexture >= 0)
        {
            trans *= textures.T[MAT.albedoTexture].Sample(gSampler, vOut.uv.xy).rgb;
        }
    }
    color += trans;

    
    
    return float4(color, saturate(alpha + 0.2)) ;
}

#endif
