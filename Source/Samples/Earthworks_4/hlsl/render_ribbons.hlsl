
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

    // POSSIBLE EXPANSION 2 more uint
    colour with translucency in alpha
    (9, 8, 15)  yaw pitch free  tangent BUT BETTER UV
    
};

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

ribbon - in 128 bits (4 uints)
orientated ribbon - 
    2           type            {ribbon, aligned ribbon, triangle list}
    1           start bit
    14,15,14    position        {maybe larger to work for buildings as well 16, 16, 16 - 2mm 128m big ?}
    10          material        {more if we have space}
    8, 7        u, v            {8 bits to 1, plus 4 bits over, ot smaller with UV scale value}
    9, 8, 8     tangent frame (yaw, pitch, roll)
    8           radius
    9, 8, 7, 8  lighting (yaw, pitch, cone, depth)

    8, 8, 8, 8  colour with translucency

    8           anim_blend


6 uints seems inevitable, even for ribbons once we add shadows and ao
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
StructuredBuffer<sprite_material>   materials;
//StructuredBuffer<ribbonVertex>      instanceBuffer;
StructuredBuffer<RV> instanceBuffer;
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
};




inline float3 rot_xz(const float3 v, const float yaw)
{
    float s, c; 
    sincos(yaw, s, c);
    return float3((v.x * c) + (v.z * s), v.y, (-v.x * s) + (v.z * c));
}


inline float3 yawPitch_9_8bit(int yaw, int pitch) // 9, 8 bits 8 b
{
    float plane, x, y, z;
    sincos((yaw - 256) * 0.01232, z, x);
    sincos((pitch - 128) * 0.01232, y, plane);
    return float3(plane * x, y, plane * z);
}

void extract(inout PSIn o, const uint4 v)
{
    o.pos.xyz += rot_xz(float3((v.x >> 16) & 0x3fff, v.x & 0xffff, (v.y >> 18) & 0x3fff) * objectScale - 2 * float3(8.198, 4.196, 8.192), 0);
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


PSIn vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    PSIn output = (PSIn) 0;
    const uint P = 0; // plant.index * 128; // rename meshlet
    float3 rootPosition = float3((iId % 10) * 50, 0, ((int) (iId / 10)) * 50) - float3(100, 0, 100);
    const int4 v = instanceBuffer[vId].v;

    output.pos.xyz =     rootPosition;
    extract(output, v);
    
    return output;
}


[maxvertexcount(4)]
void gsMain(line PSIn L[2], inout TriangleStream<PSIn> OutputStream)
{
    PSIn v;

    if (L[1].flags.y == 1)
    {
        v = L[0];
        v.uv.x = 0.5 - L[0].uv.x;
        v.pos = mul(L[0].pos - float4(L[0].tangent * L[0].flags.z * radiusScale, 0), viewproj);
        OutputStream.Append(v);

        v.uv.x = 0.5 + L[0].uv.x;
        v.pos = mul(L[0].pos + float4(L[0].tangent * L[0].flags.z * radiusScale, 0), viewproj);
        OutputStream.Append(v);


        v = L[1];
        v.uv.x = 0.5 - L[1].uv.x;
        v.pos = mul(L[1].pos - float4(L[1].tangent * L[1].flags.z * radiusScale, 0), viewproj);
        OutputStream.Append(v);

        v.uv.x = 0.5 + L[1].uv.x;
        v.pos = mul(L[1].pos + float4(L[1].tangent * L[1].flags.z * radiusScale, 0), viewproj);
        OutputStream.Append(v);
    }
}


float4 psMain(PSIn vOut, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
        int material = vOut.flags.x;
    if (material == 2)
    {
        material = 1;
    }

    float4 albedoAlpha = textures.T[material * 2 + 0].Sample(gSampler, vOut.uv.xy);
    //clip(albedoAlpha.a - 0.7);

    float3 normalTex = ((textures.T[material * 2 + 1].Sample(gSampler, vOut.uv.xy).rgb) * 2.0) - 1.0;   //? from rg
    float3 N = (normalTex.r * vOut.tangent) + (normalTex.g * vOut.binormal) + (normalTex.b * vOut.normal);


    
    const float3 sunDir = normalize(float3(1.0, 0.7, -0.8));
    const float3 sunColor = float3(1.2, 1.0, 0.7) * 1.5;

    float3 color = albedoAlpha.rgb * sunColor * saturate(dot(N, sunDir)) * vOut.lighting.z;

    if (material > 0)
    {    
        color += albedoAlpha.rgb * sunColor * float3(1, 1, 0.3) * saturate(dot(N, -sunDir)) * vOut.lighting.z;
    }
    
    //env
    float3 dir = N * float3(1, 1, -1);
    color += gEnv.SampleLevel(gSampler, dir, 0).rgb * albedoAlpha.rgb * 1.0 * vOut.lighting.w;
    
    return float4(color, 1);

}


/*
VSOut vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    VSOut output = (VSOut)0;

    //const xformed_PLANT plant = instances[iId];
    const uint P = 0;// plant.index * 128;
    const ribbonVertex R = instanceBuffer[vId + P];

    xformed_PLANT plant;
    plant.position = float3((iId % 10) * 20, 0, ((int) (iId / 10)) * 20) - float3(100, 0, 100);
    plant.rotation.x = 0;
    plant.rotation.y = 1;
    //sincos(iId, plant.rotation.x, plant.rotation.y);
    plant.scale = 1;

    output.rootPos = float4(plant.position + (rot_xz(R.pos, plant.rotation) * plant.scale), 1);
    
    bool absolute = (R.B >> 16) & 0x1;
    float3 up;
    float3 dir = normalize(eyePos - output.rootPos.xyz);
    if (absolute)
    {
        output.right = float4(rot_xz(R.right, plant.rotation), 0);
        up = normalize(cross(dir, output.right.xyz));

        output.N = normalize(cross(output.right.xyz, up));
        output.U = normalize(up);
        output.R = normalize(output.right.xyz);
        output.flags.z = 0;

    }
    else
    {
        float3 up = rot_xz(R.right, plant.rotation);
        float3 right = normalize(cross(up, dir)) ;
        output.right = float4(right, 0) * plant.scale * length(R.right);  // rather pack that diameter somewhere

        output.U = normalize(up);
        output.R = right;
        output.N = cross(right, output.U);
        output.flags.z = 1;

    }
    
    output.flags.x = R.A;
    output.flags.y = (R.B & 0xff);
    
    output.other.x = R.pos.y * 4;

    

    output.world = -dir;//    float3(dir.x, dir.z, -dir.y);

    output.lighting =  R.lighting; // FIXME rotate this still
    output.extra = R.extra;

    return output;
}



[maxvertexcount(4)]
void gsMain(line VSOut L[2], inout TriangleStream<GSOut> OutputStream)
{
    GSOut v;

    //OutputStream.RestartStrip();
    if (L[1].flags.x == 1)
    {
        v.texCoords = float3(0, 1.0, L[0].other.x);
        v.N = L[0].N;
        v.U = L[0].U;
        v.R = L[0].R;
        v.lighting = L[0].lighting;
        v.extra = L[0].extra;
        v.world = L[0].world;
        v.flags = L[0].flags;
//        if (L[0].flags.z == 0)
 //       {
  //          v.texCoords = float3(0.5, 1.0, L[0].other.x);
    //    }
        v.pos = mul(L[0].rootPos - L[0].right, viewproj);
        OutputStream.Append(v);

        v.pos = mul(L[0].rootPos + L[0].right, viewproj);
        v.texCoords.x = 1;
//        if (L[0].flags.z == 0)
  //      {
    //        v.texCoords = float3(1.0, 0.5, L[0].other.x);
      //  }
        OutputStream.Append(v);


        v.N = L[1].N;
        v.U = L[1].U;
        v.R = L[1].R;
        v.lighting = L[1].lighting;
        v.extra = L[1].extra;
        v.pos = mul(L[1].rootPos - L[1].right, viewproj);
        v.texCoords = float3(0, 0.0, L[1].other.x);
        v.world = L[1].world;
//        if (L[0].flags.z == 0)
  //      {
    //        v.texCoords = float3(0.0, 0.5, L[0].other.x);
      //  }
        OutputStream.Append(v);

        v.pos = mul(L[1].rootPos + L[1].right, viewproj);
        v.texCoords.x = 1;
//        if (L[0].flags.z == 0)
  //      {
    //        v.texCoords = float3(0.5, 0.0, L[0].other.x);
      //  }
        OutputStream.Append(v);
    }
    
}
*/

/*
float4 psMain(GSOut vOut, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    return 0.5;
    
    
        int material = vOut.flags.y;
    if (material == 2)
    {
        material = 1;
    }

    float4 albedoAlpha = textures.T[material * 2 + 0].Sample(gSampler, vOut.texCoords.xy);
    clip(albedoAlpha.a - 0.7);

    float3 N = vOut.N;
    if (material == 0)
    {
        float3 normalTex = ((textures.T[material * 2 + 1].Sample(gSampler, vOut.texCoords.xy).rgb) * 2.0) - 1.0;
        N = (normalTex.r * vOut.R) + (normalTex.g * vOut.U) + (normalTex.b * vOut.N);
        //N = normalTex;
       //return float4(N, 1);
    }
    // consider testing backface and even changing material for that
    
    
    
    float3 R = reflect(vOut.world, N);


    float3 sunDir = normalize(float3(1.0, 0.7, -0.8));
    //float3 sunDir = normalize(float3(0.0, 0.0, 1.0));

    float A = dot(N, sunDir);
    float A_backface = saturate(-A);
    A = saturate(A*1);
    float S = pow(saturate(dot(sunDir, R)), 15);
    float AO = (saturate(1 - vOut.extra.x * 0.3) + 0.8) / 1.8;

    //float Z = dot(normalize(vOut.lighting.xyz) + 0.0 * sunDir * vOut.lighting.w, sunDir);
    //float shadow =     saturate(saturate(3 * Z) - saturate(1 * (1. - vOut.lighting.w)));

    //return float4(saturate(vOut.lighting.w), 0, saturate(-vOut.lighting.w), 1);
    float Z = dot(vOut.lighting.xyz, sunDir) + vOut.lighting.w * 2;
    float shadow = saturate(saturate(Z * 5) - saturate(vOut.extra.x * 0.07 / Z));
    //shadow = shadow * shadow * shadow;
    
    float3 sunColor = float3(1.2, 1.0, 0.7);
    float B = 0.97;
    float3 D = float3(0.5, 0.5, 0.6);
    if (material == 0)
    {
        S = 0;
        B = 0;
        D = 1.0;
    }
    // diffuse
    //shadow = 1;
    float3 final = (albedoAlpha.rgb * A) * shadow * D * sunColor * saturate(1-S);

    // back light
    //final += B * shadow * saturate(-dot(vOut.world, sunDir)) * albedoAlpha.rgb * float3(0.9, 0.7, 0.3);
    final += B * shadow * pow(saturate(dot(vOut.world, sunDir)), 2) * albedoAlpha.rgb * float3(0.9, 0.7, 0.3);

    // specular
    final += shadow * S * 0.2;

    //env
    float3 dir = N;
    dir.z *= -1;
    final += gEnv.SampleLevel(gSampler, dir, 0).rgb * albedoAlpha.rgb * saturate(AO) * 0.7;

    
    //final = albedoAlpha.rgb * D;


    
    albedoAlpha.a = 1;
    return float4(final, albedoAlpha.a);
}

*/
/*  PIXEL SHADER
    --------------------------------------------------------------------------------------------
    DIFFUSE
    SPECULAR
    UV
    LIGHTING (u, v, shadowDepth, ao)
    MATERIAL

    albedoTexture + clip
    normalTexture + N
    calc diffuse, including translucency
    calc specular


    TEXTURES
    albedoAlpha
    normal rg
    translucency bc4 grayscale
*/







/*

    // packing normal vectors
    cubemap face 3 , x 6, y 6   16 bits roughly 1.4 degree acuracy

    sincos hemisphere 8 bits, 7 bits        so extremely similar numbes, look at unpack
    yaw, pitch, cone (8, 7, 9)  1.4 degrees, 0.4 degree cone
    yaw, pitch, cone (9, 8, 7)  0.7 degrees, 0.8 degree cone rougly
        sincos(yaw, z, x);
        sincos(pitch, y, pln);
        float3(pln * x, y, pln * z);
        // 0.02454369260617

    yaw pitch roll? -> Matrix



*/
