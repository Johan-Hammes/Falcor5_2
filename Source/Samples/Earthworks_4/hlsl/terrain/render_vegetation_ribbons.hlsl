
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"		
#include "vegetation_defines.hlsli"




SamplerState gSampler;
SamplerState gSamplerClamp;             // for blending with hald-buffer
Texture2D gAlbedo : register(t0);
TextureCube gEnv : register(t1);
Texture2D gHalfBuffer : register(t2);

struct ribbonTextures
{
    Texture2D<float4> T[4096];
};
ParameterBlock<ribbonTextures>
textures;



StructuredBuffer<sprite_material> materials;
//StructuredBuffer<ribbonVertex8> instanceBuffer;     // THIS HAS TO GO
//StructuredBuffer<xformed_PLANT> instances;

StructuredBuffer<plant>             plant_buffer;
StructuredBuffer<plant_instance> instance_buffer;
StructuredBuffer<block_data> block_buffer;
StructuredBuffer<ribbonVertex8> vertex_buffer;

cbuffer gConstantBuffer
{
    float4x4 viewproj;
    float3 eyePos;
    
    float time;
    float bake_radius_alpha;
};




struct PSIn
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    //float3 diffuseLight : COLOR;
    //float3 specularLight : COLOR1;
    float2 uv : TEXCOORD; // texture uv
    float4 lighting : TEXCOORD1; // uv, sunlight, ao
    nointerpolation uint4 flags : TEXCOORD2; // material
    float3 eye : TEXCOORD3;                 // can this become SVPR  or per plant somehow, feels overkill here per vertex
    float4 colour : TEXCOORD4;
    float lineScale : TEXCOORD5; 
};


float3 lighting(const float3 N, const float3 eye, const float3 colour, const float4 lighting, const int matIndex)
{
    const float3 sunDir = -normalize(float3(-0.6, -0.5, -0.8));
    const float3 sunColor = float3(1.2, 1.0, 0.7) * 5.0;
    float ndoth = saturate(dot(N, normalize(sunDir - eye)));
    float ndots = dot(N, sunDir);
    //if (MAT.translucency > 0.98)
    //{
    //    ndots = 0.5;
    //}

    sprite_material MAT = materials[matIndex];

    float3 color = sunColor * (saturate(ndots)); // forward scattered diffuse light

    float3 tempAlbedo = float3(3.03, 0.1, 0.02);
    
    //diffuse env
    color *= tempAlbedo * colour.x * lighting.z;

    color += gEnv.SampleLevel(gSampler, N * float3(1, 1, -1), 0).rgb * 2 * tempAlbedo * colour.x * lighting.w;
    
    // specular sun
    color += pow(ndoth, 15) * 0.3 * lighting.z;

    float3 trans = 2 * saturate(-ndots * 1) * colour.y * float3(1, 2, 0.4) * MAT.translucency * lighting.z;
    color += trans * tempAlbedo * 2;


    return color;
}


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

/*
inline void extractPosition(inout PSIn o, const ribbonVertex8 v, const float scale, const float rotation, const float3 root)
{
    float3 p = float3((v.a >> 16) & 0x3fff, v.a & 0xffff, (v.b >> 18) & 0x3fff) * objectScale - objectOffset;
    o.pos.xyz = root + rot_xz(p, rotation) * scale;
    o.pos.w = 1;
    o.eye = normalize(o.pos.xyz - eyePos);
}*/

// These two can optimize, its only tangent that differs
inline void extractTangent(inout PSIn o, const ribbonVertex8 v, const float rotation)
{
    o.binormal = yawPitch_9_8bit(v.c >> 23, (v.c >> 15) & 0xff, rotation); // remember to add rotation to yaw
    o.tangent = normalize(cross(o.binormal, -o.eye));
    o.normal = cross(o.binormal, o.tangent);
}

inline void extractTangentFlat(inout PSIn o, const ribbonVertex8 v, const float rotation)
{
    o.binormal = yawPitch_9_8bit(v.c >> 23, (v.c >> 15) & 0xff, rotation); // remember to add rotation to yaw
    o.tangent = yawPitch_9_8bit(v.d >> 23, (v.d >> 15) & 0xff, rotation); // remember to add rotation to yaw
    o.normal = cross(o.binormal, o.tangent);
}

inline void extractUVRibbon(inout PSIn o, const ribbonVertex8 v)
{
    o.uv = float2(((v.d >> 8) & 0x7f) * 0.00390625, ((v.c) & 0x7fff) * 0.00390625); // allows 16 V repeats scales are wrong decide
    o.uv.y = 1 - o.uv.y;
}

inline void extractFlags(inout PSIn o, const ribbonVertex8 v)
{
    o.flags.x = (v.b >> 8) & 0x2ff; //  material
    o.flags.y = (v.a >> 30) & 0x1; //  start bool
    o.flags.z = (v.d & 0xff); //  int radius
}
/*
inline void extractColor(inout PSIn o, const ribbonVertex8 v)
{
    o.colour.r = 0.1 + ((v.f >> 8) & 0xff) * 0.008; // albedo
    o.colour.g = ((v.f >> 0) & 0xff) * 0.008; //tanslucency
    //o.colour.b = 0.5;

    o.colour.b = saturate((v.d & 0xff) * radiusScale / length(o.pos.xyz - eyePos) * 200);
    o.colour.b = saturate((o.colour.b - 0.5) * 50);
    
}*/
/*
inline void light(inout PSIn o, const ribbonVertex8 v, const float rotation)
{
    const float3 sunDir = normalize(float3(-0.6, -0.5, -0.8));
    float3 lightCone = yawPitch_9_8bit(v.e >> 23, (v.e >> 15) & 0xff, rotation);
    float cone = (((v.e >> 8) & 0x7f) * 0.01575) - 1; //cone7
    float d = (v.e & 0xff) * 0.00784; //depth8
    float a = saturate(dot(normalize(lightCone + sunDir * sunTilt), sunDir)); //cone * 0
    float b = a * (bDepth - d) + d;
    float ao = 1 - saturate(d * Ao_depthScale);
    o.lighting = float4(0, 0, saturate(1 - (b * bScale)), ao);

#if defined(_BAKE)
    o.lighting.rgb = lightCone;
#endif
}*/

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

float windstrength(float D, float R, float strength, float variance)
{
    float value = sin(D * 0.6 - time * 0.85);
    return strength + (value * variance);
}

float sway(float D, float strength, float variance)
{
    float value = sin(D * 2.1 - time * 1.5);
    return strength + (value * variance);
}

float twist(float D, float strength, float variance)
{
    float value = sin(D * 3.1 - time * 3.5);
    return strength + (value * variance);
}

void windAnimate(inout PSIn vertex, float3 plantRoot, int _rotate)
{
    float3 root = float3(0, 0, 0);
    float3 dir = float3(0, 1, 0);
    float dirL = 0.8f;

    float3 relative = vertex.pos.xyz - plantRoot - root;
    float dL = saturate(dot(relative, dir) / dirL);
    float dL2 = pow(dL, 1.5);

    float3 windDir = float3(1, 0, 0);
    float3 windRight = normalize(cross(windDir, dir));
    float windStrength = windstrength(dot(windDir, vertex.pos.xyz), dot(windRight, vertex.pos.xyz), 0.2, 0.1); // already converted to radians - figure out how

    float W = windStrength * dL2 * 1 * sway(rand_1_05(plantRoot.xz), 0.6, 0.093);
    float3x3 rot = mul(AngleAxis3x3(-W, windRight), AngleAxis3x3(-W * 2, dir));

    float Wside = windStrength * dL2 * sway(rand_1_05(plantRoot.xz), 0.0, 0.043);
    rot = mul(rot, AngleAxis3x3(Wside, windDir));

    
    float3 newV = mul(rot, relative);

    float scale = pow(rcp(abs(1 + W)), 0.2);
    vertex.pos.xyz = plantRoot + root + newV * scale;

    vertex.binormal = mul(rot, vertex.binormal);
    vertex.normal = mul(rot, vertex.normal );
    /*
    if (_rotate)
    {
        vertex.tangent = mul(rot, vertex.tangent);
    }
    else
    {
        vertex.tangent = normalize(cross(vertex.binormal, vertex.eye));
    }
    */
    //vertex.tangent = normalize(cross(vertex.binormal, vertex.eye));
    

}


PSIn vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    PSIn output = (PSIn) 0;

    const block_data BLOCK = block_buffer[iId];
    const plant_instance INSTANCE = instance_buffer[BLOCK.instance_idx];
    const plant PLANT = plant_buffer[INSTANCE.plant_idx];
    const ribbonVertex8 v = vertex_buffer[vId + BLOCK.vertex_offset];

#if defined(_BAKE)
    //rootPosition = float3(0, 0, 0);
    //scale = 1;
    //rotation = 0;
    //INSTANCE.position = float3(0, 0, 0);
    //INSTANCE.rotation = 0;
    //INSTANCE.scale = 1;
#endif

    // position
    {
        float3 p = float3((v.a >> 16) & 0x3fff, v.a & 0xffff, (v.b >> 18) & 0x3fff) * PLANT.scale - PLANT.offset;
        output.pos.xyz = INSTANCE.position + rot_xz(p, INSTANCE.rotation) * INSTANCE.scale;
        output.pos.w = 1;
        output.eye = normalize(output.pos.xyz - eyePos);

        output.colour.a = 1; // new alpha component but just for bake
#if defined(_BAKE)
        p.y = 0;
        float R = length(p);
        if (R > 0.3f)      output.colour.a = 0;
        output.colour.a = 1.f - smoothstep(bake_radius_alpha * 0.5f, bake_radius_alpha, R);
        
#endif
    }
    
    
    if (v.a >> 31)
    {
        extractTangentFlat(output, v, INSTANCE.rotation);
    }
    else
    {
        extractTangent(output, v, INSTANCE.rotation); // Likely only after abnimate, do only once
    }
    /*
    output.binormal = float3(0, 1, 0);
    output.tangent = float3(1, 0, 0);
    output.normal = float3(0, 0, 1);
    output.tangent = normalize(cross(output.binormal, -output.eye));
    output.normal = cross(output.binormal, output.tangent);
    */
    extractUVRibbon(output, v);
    extractFlags(output, v);
    //extractColor(output, v);
    {
        output.colour.r = 0.1 + ((v.f >> 8) & 0xff) * 0.008; // albedo
        output.colour.g = ((v.f >> 0) & 0xff) * 0.008; //tanslucency
        output.colour.b = saturate((v.d & 0xff) * PLANT.radiusScale / length(output.pos.xyz - eyePos) * 200);
        output.colour.b = saturate((output.colour.b - 0.5) * 50);
    }


    
    
    //light(output, v, INSTANCE.rotation);
    {
        const float3 sunDir = normalize(float3(-0.6, -0.5, -0.8));
        float3 lightCone = yawPitch_9_8bit(v.e >> 23, (v.e >> 15) & 0xff, INSTANCE.rotation);
        float cone = (((v.e >> 8) & 0x7f) * 0.01575) - 1; //cone7
        float d = (v.e & 0xff) * 0.00784; //depth8
        float a = saturate(dot(normalize(lightCone + sunDir * PLANT.sunTilt), sunDir)); //cone * 0
        float b = a * (PLANT.bDepth - d) + d;
        float ao = 1 - saturate(d * PLANT.Ao_depthScale);
        output.lighting = float4(0, 0, saturate(1 - (b * PLANT.bScale)), ao);

        
#if defined(_BAKE)
    output.lighting.rgb = lightCone;
#endif
    }

    /*
    float3 Rpos = output.pos.xyz;
    Rpos.y = 0;
    if (Rpos.z > 0)
        Rpos.z = 0;
        if (length(Rpos) > 1.0f)
        {
            output.flags.y = 0;
        }
    */
    //output.lighting.w = 0.5f;

    // Now do wind animation
    // maybe before lighting, although for now it doesnt seem to matter
    // --------------------------------------------------------------------------------------------------------
    //windAnimate(output, rootPosition, v.a >> 31);

    //output.diffuseLight = lighting(output.normal, output.eye, output.colour, output.lighting, output.flags.x);

    // thsi value determines if it splits into 2 or 4 during the geometry shader
    float d = length(output.pos.xyz - eyePos);
    output.flags.w = output.flags.z * PLANT.radiusScale / d * 10000;

    output.lineScale = pow(output.flags.z / 255.f, 2) * PLANT.radiusScale;
            
    return output;
}



// HOLY fukcing shit, this is bad anythign above 6
// at 6 I can still get gain out of this
[maxvertexcount(4)]
void gsMain(line PSIn L[2], inout TriangleStream<PSIn> OutputStream)
{
    float lineScale = 1.f;
    PSIn v;

    
    //if ((L[1].flags.y == 1) && (L[0].flags.w > 1))
    if ((L[1].flags.y == 1))
    {
        v = L[0];
        v.uv.x = 0.5 + L[0].uv.x;
        v.pos = mul(L[0].pos - float4(v.tangent * v.lineScale, 0), viewproj);
        OutputStream.Append(v);

        v.uv.x = 0.5 - L[0].uv.x;
        v.pos = mul(L[0].pos + float4(v.tangent * v.lineScale, 0), viewproj);
        OutputStream.Append(v);

      /*  
        if (L[0].flags.w > 10)      // but we can do the test rigth here from radius
        {
            float l1 = length(L[1].pos.xyz - L[0].pos.xyz);
            float4 posBezier = float4(((L[0].pos.xyz + L[1].pos.xyz) * 0.5) + ((L[0].binormal - L[1].binormal) * l1 * 0.125), 1);

            v.uv.y = (L[0].uv.y + L[1].uv.y) * 0.5;
            v.uv.x = 0.5 + L[0].uv.x;
            v.pos = mul(posBezier - float4(v.tangent * pow(v.flags.z / 255.f, 2) * radiusScale, 0), viewproj);
            OutputStream.Append(v);

            v.uv.x = 0.5 - L[0].uv.x;
            v.pos = mul(posBezier + float4(v.tangent * pow(v.flags.z / 255.f, 2) * radiusScale, 0), viewproj);
            OutputStream.Append(v);
        }
        */
        
        v = L[1];
        v.uv.x = 0.5 + L[1].uv.x;
        v.pos = mul(L[1].pos - float4(v.tangent * v.lineScale, 0), viewproj);
        OutputStream.Append(v);

        v.uv.x = 0.5 - L[1].uv.x;
        v.pos = mul(L[1].pos + float4(v.tangent * v.lineScale, 0), viewproj);
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


    float alpha = textures.T[MAT.albedoTexture].Sample(gSampler, vOut.uv.xy).a;
    if (MAT.alphaTexture >= 0)
    {
        //alpha = textures.T[MAT.alphaTexture].Sample(gSampler, vOut.uv.xy).r;
        //
    }
    alpha *= vOut.colour.a;

    float rnd = 0.25 + 0.5 * rand_1_05(vOut.pos.xy);
    clip(alpha - rnd);

    float3 color = textures.T[MAT.albedoTexture].Sample(gSampler, vOut.uv.xy).rgb;// * vOut.colour.x * pow(vOut.lighting.w, 2);

    output.albedo = float4(pow(color, 1.0 / 2.2), 1);   // previous alpha, doesnt work form bake
    
    float3 N = vOut.normal;
    if (MAT.normalTexture >= 0)
    {
        float3 n = (textures.T[MAT.normalTexture].Sample(gSampler, vOut.uv.xy).rgb * 2) - 1;
        N = (-n.r * vOut.tangent) + (n.g * vOut.binormal) + (n.b * vOut.normal);
    }
    N *= (isFrontFace * 2 - 1);


    

    float3 NAdjusted = N;
    
    NAdjusted.r = N.r * 0.5 + 0.5;
    NAdjusted.g = N.g * 0.5 + 0.5;
    NAdjusted.b = -N.b * 0.5 + 0.5;

//NAdjusted = vOut.lighting.xyz * 0.5 + 0.5;
//NAdjusted.b = 1 - NAdjusted.b;


//float instance_PLANT = saturate(dot(vOut.lighting.xyz, normalize(float3(0.8, 0.4, 0.4))));
//output.albedo *= instance_PLANT * 0.3 + 0.7;

    output.normal = float4(NAdjusted, 1);       // FIXME has to convert to camera space, cant eb too hard , uprigthmaybe, and 0-1 space
    output.normal_8 = float4(NAdjusted, 1);

    

    float trans = vOut.colour.y * MAT.translucency;
    {
        if (MAT.translucencyTexture >= 0)
        {
            trans *= dot(float3(0.3, 0.4, 0.3), textures.T[MAT.translucencyTexture].Sample(gSampler, vOut.uv.xy).rgb);
        }
    }
    trans = saturate(pow(trans, 1.0 / 2.2));
    output.extra = float4(0, 0, 0, 1-trans);

    return output;
}

#else

float4 psMain(PSIn vOut, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    
    /*
    if (isFrontFace)
        return float4(0, 1, frac(vOut.uv.y), 1);
    else
        return float4(1, 0, vOut.uv.y, 1);
    */
    sprite_material MAT = materials[vOut.flags.x];
    float4 albedo = textures.T[MAT.albedoTexture].Sample(gSampler, vOut.uv.xy);
    float alpha = albedo.a;
    if (MAT.alphaTexture >= 0)
    {
        //alpha = textures.T[MAT.alphaTexture].Sample(gSampler, vOut.uv.xy).r;
    }
    alpha =     pow(alpha, MAT.alphaPow);
    clip(alpha - 0.2);

    
    
    float3 N = vOut.normal;
    if (MAT.normalTexture >= 0)
    {
        float3 normalTex = ((textures.T[MAT.normalTexture].Sample(gSampler, vOut.uv.xy).rgb) * 2.0) - 1.0;
        N = (-normalTex.r * vOut.tangent) + (normalTex.g * vOut.binormal) + (normalTex.b * vOut.normal);
    }
    N *= (isFrontFace * 2 - 1);

    
    const float3 sunDir = -normalize(float3(-0.6, -0.5, -0.8));
    const float3 sunColor = float3(1.2, 1.0, 0.7) * 0.3;
    //float ndotv = saturate(dot(N, vOut.eye));
    float ndoth = saturate(dot(N, normalize(-sunDir + vOut.eye)));
    float ndots = dot(N, -sunDir); // * vOut.lighting.z;
    if (MAT.translucency > 0.98)
    {
        //ndots = 0.5;
    }
    

    float3 color = sunColor * (saturate(ndots)) * albedo.rgb * vOut.colour.x * vOut.lighting.z;
    

    color += gEnv.SampleLevel(gSampler, N * float3(1, 1, -1), 0).rgb * 10 * albedo.rgb * vOut.colour.x * vOut.lighting.w;
    // temporary ENV till we seyt gEnv again
    color += float3(0.01, 0.03, 0.05) * albedo.rgb * 2;// * vOut.colour.x * vOut.lighting.w;

    
    
    // specular sun
    color += pow(ndoth, 39) * 0.31 * vOut.lighting.z;
   

    float3 TN = vOut.normal;
    if (!isFrontFace)        TN *= -1;
    //float3 trans = saturate(dot(sunDir, vOut.eye)) * vOut.colour.y * float3(2, 3, 1) * MAT.translucency * vOut.lighting.z;
    float3 trans = (saturate(-ndots)) *     saturate(dot(sunDir, vOut.eye)) * vOut.colour.y * 0.5 * MAT.translucency * vOut.lighting.z;
    //float3 trans = saturate(dot(sunDir, vOut.eye)) *     saturate(dot(sunDir, TN)) * vOut.colour.y * float3(1, 1, 1) * MAT.translucency * vOut.lighting.z; 
    {
        if (MAT.translucencyTexture >= 0)
        {
            trans *= pow(textures.T[MAT.translucencyTexture].Sample(gSampler, vOut.uv.xy).r, 1);
        }
    }
    color += trans * albedo.rgb * albedo.rgb * 5 * vOut.colour.x;
    

    float alphaV = pow(saturate(vOut.flags.w * 0.1), 0.3);
    alpha = smoothstep(0.2, 1, alpha);
    alpha = min(alpha, alphaV);
    
    if (alpha < 0.99)
    {
        float2 uv = vOut.pos.xy / float2(2560, 1440);
        //uv.y = 1 - uv.y;
        float3 prev = gHalfBuffer.Sample(gSamplerClamp, uv).rgb;
        //int3 sample = vOut.pos.xyz / 2;
        //sample.z = 0;
        //float3 prev = gHalfBuffer.Load(sample).rgb;
        color = lerp(prev, color, alpha);
    }

    return float4(color, 1);
}

#endif
