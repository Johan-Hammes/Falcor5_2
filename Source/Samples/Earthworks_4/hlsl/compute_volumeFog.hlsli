

#include "compute_fogCloudAtmosphereCommon.hlsli"

// more compact light structure for Volumetric fog only. Only support spotlights, and can mimic point lights by making the spot angle very large
// iliminates the if(), and allows more optimization, can discard a lot of lights earlier
struct volumeFogLight {
	float3 position;
	float radius; //??? seems unused, done purely on power

	float3 direction;
	float slope;

	float3 colour;
	float power; // for pow function - also test if just cators fo 2 is faster and multiply out instead of pow(), then save int log(pow) - might well be good enough
};

struct fogVolume {
	float4x4 world;
};

SamplerState linearSampler : register(s0);
SamplerState clampSampler : register(s1);


Texture2D CloudMap : register(t0);
Texture2D FogMap : register(t1);
Texture2D gNoiseA : register(t2);
Texture2D gNoiseB : register(t3);
Texture3D gLightVolume : register(t4);
Texture2D hazePhaseFunction : register(t5);
TextureCube envMap : register(t6);
Texture2D SunInAtmosphere : register(t7);
Texture2D fogDensities[32] : register(t8);

// I rthink remove FogCloudCommonParams
// and make this(b0)

/*	This is slight overkill here, but the loss is tiny and it helps to keep all 3 volume fog shaders in sync, so just accept the unsused parameters
	At some pint I want to play with basic exponential fog crossover between near and far. Need to know at which slice that occurs
	*/
cbuffer FogAtmosphericParams : register(b1) {
	float3 eye_direction;
	float uv_offset;

	float3 eye_position;
	float uv_scale;

	float3 dx;
	float uv_pixSize;

	float3 dy;
	uint numSlices;

	float sliceZero;
	float sliceMultiplier; // distance to the next slice (multiply with current slice)
	float sliceStep;	   // size of this slice (multiply with current slice)
	float slicePADD2;

	float timer;
	int numFogLights;
	float2 paddTime;

	// possible temporary near density
	float2 fogAltitude; //(base, noise)	??? Not sure if base will be sued in the end, get from Fog texture
	float2 fogDensity;	//(base, noise)
	float2 fogUVoffset; // animate to simulate wind
	float fogMip0Size;	// Size of a single pixel on MIP 0, since compute shaders have to explicitely compute mip level
	float numFogVolumes;

	// Block for the new atmosphere
	// ############################################################################################################################
	float3 sunColourBeforeOzone;
	float haze_Turbidity;

	float3 haze_Colour;
	float haze_AltitudeKm;

	float3 fog_Colour;
	float haze_BaseAltitudeKm;

	float fog_AltitudeKm;
	float fog_BaseAltitudeKm;
	float fog_Turbidity;
	float globalExposure; // I dont like this to much - its a duplicate of the value in ksCommon but duplicating makes for cleaner headers

	float3 rain_Colour;
	float rainFade;

	float ozone_Density;
	float3 ozone_Colour;

	float4 refraction;
	// ############################################################################################################################

	// info for parabolic projection
	float parabolicProjection;
	float parabolicMapHalfSize;
	float parabolicUpDown;
	float parabolicPADD;

	// Temporary block - debug sliders for atmosphere
	// ############################################################################################################################
	float tmp_IBL_scale;
	float tmp_B;
	float tmp_C;
	float tmp_D;
	// ############################################################################################################################
};

cbuffer FogLights : register(b2) {
	volumeFogLight volumeFogLights[64]; // smaller because a large number is a VERY bad idea In fact 20 is probably the upper limit without reworking this significantly
};

cbuffer FogVolumes : register(b3) {
	fogVolume volumes[8];
};

static const float EarthR = 6371.0;												// km

static const float3 Rayleigh_scattering = float3(0.003802, 0.013558, 0.033100); // for km as distance unit
static const float3 Rayleigh_extinction = float3(0.003802, 0.013558, 0.033100);

static const float3 Haze_scattering = float3(0.0021627, 0.0033058, 0.005165); // keep the same for now, just edit via colour
static const float3 Haze_extinction = float3(0.0021627, 0.0033058, 0.005165);

static const float3 Fog_scattering = float3(0.0033058, 0.0033058, 0.0033058); // keep the same for now, just edit via colour
static const float3 Fog_extinction = float3(0.0033058, 0.0033058, 0.0033058);

static float3 sun_Luminance = float3(113000, 121000, 120000); // in lux brightest sunlight

// calculates the optical Density at a specific altitude
// x - Rayleigh
// y - Mie haze
// z - Mie fog
// w - Ozone  // 25km either side of 24km altitude
float4 opticalDepth(float altKm) {
	float4 O = 0;
    O.x = saturate(exp(-altKm / 8.4));
    O.y = saturate(exp(-max(0, (altKm - haze_BaseAltitudeKm)) / haze_AltitudeKm)) * haze_Turbidity * haze_Turbidity;
    O.z = saturate(exp(-max(0, (altKm - fog_BaseAltitudeKm)) / fog_AltitudeKm)) * fog_Turbidity * fog_Turbidity;
    O.z = saturate(exp(-max(0, (altKm - 0.75)) / 0.06)) * fog_Turbidity * fog_Turbidity;
	O.w = (1 - smoothstep(0, 1, abs(altKm - 24.0) * 0.04)) * ozone_Density;
	return O;
}

float4 sunLight(float3 posKm) {
	float2 sunUV;
	float dX = dot(posKm.xz, normalize(sun_direction.xz));
	sunUV.x = saturate(0.5 - (dX / 1600.0f));
	sunUV.y = 1 - saturate(posKm.y / 100.0f);
	return SunInAtmosphere.SampleLevel(clampSampler, sunUV, 0) * 0.07957747154594766788444188168626;
}

// FIXME duplicate from ksCommon.hlsli maybe unavoidable
inline float3 projectToCloudbase(inout float3 pos) {
	return pos - sun_direction * (cloudBase - pos.y) / max(0.01, -sun_direction.y);
}

// previsouly this was in ksCommon.hlsli
// clamped upwards projection to the cloud base for sampling cloud shadow maps
//inline void projectToCloudbase(inout float3 pos) {
//	pos -= sunDirection * (cloudBase - pos.y) / max(0.01, -sunDirection.y);
//}

float2 cloud_uv(float3 pos) {

	//pos.xz -= eye_position.xz;
	float distance = log2(length(pos.xz) / 1000 + 1) * 0.16666666666666666666666; // 1/6
	float2 dir = normalize(pos.xz);
	return (dir * distance) * 0.5 + 0.5;
}

// Previously this was in common.hlsli
//float2 cloud_uv(float3 pos) {
//	projectToCloudbase(pos);
//	pos.xz -= cameraPosition.xz;
//	float distance = log2(length(pos.xz) / 1000 + 1) * 0.16666666666666666666666; // 1/6
//	float2 dir = normalize(pos.xz);
//	return (dir * distance) * 0.5 + 0.5;
//}

void calculateStep(float3 eye_dir, float phaseR, float2 phaseUV, float3 IBL, inout float depth, inout float3 newIn, inout float3 newOut, out float planetRadius) {

	// eye_position.xz = 0.0f;	// from camera
	float step = depth * sliceStep;
	float3 pos = eye_dir * (depth + (step / 2));
    pos.y += eye_position.y;
	depth *= sliceMultiplier;

	float altKm = max(0, pos.y * 0.001);
	float4 Optical = opticalDepth(altKm);
	float stepKm = step * 0.001;

	float pRayleigh = Optical.x * stepKm;
	float pMie_haze = Optical.y * stepKm;
	float pMie_fog = Optical.z * stepKm;

    float cloudShadow = 1;//    CloudMap.SampleLevel(clampSampler, cloud_uv(projectToCloudbase(pos)), 0).r;
	float clouds_start = cloudBase + 300; // to avoid moire error
	
	// rain
    float rain = 0;//    CloudMap.SampleLevel(clampSampler, cloud_uv(pos), 0).b;
	float pMie_rain = rain * stepKm;
    /*
	if (pos.y > clouds_start) {
		cloudShadow = 1.0;
		pMie_rain = 0;
	}
*/
    float4 sunlight = sunLight(pos * 0.001); // * cloudShadow;

	float opticalDepthToSun = sunlight.a;
	phaseUV.x = opticalDepthToSun / 10.0f;
	float phaseFog = hazePhaseFunction.SampleLevel(clampSampler, phaseUV, 0).r;
	phaseUV.x += 0.46875;
	float phaseHaze = hazePhaseFunction.SampleLevel(clampSampler, phaseUV, 0).r;

	

	// FIXME
	// get rid og haze_Colour, fog_Colour  and replace with exp values for 1000 scatter events ? OPT for 1000 about 10 I guess
	// add 0.46875; as a vartiable fog  partivcle size

    float3 inscatterRayleigh =  Rayleigh_scattering * phaseR * pRayleigh;
    float3 inscatterMie =     Haze_scattering * pMie_haze * phaseHaze * haze_Colour + Fog_scattering * pMie_fog * phaseFog * fog_Colour;
    float3 inscatterIBL =  (Haze_scattering * pMie_haze * haze_Colour) + (Fog_scattering * pMie_fog * fog_Colour) + (pMie_rain * rain_Colour);


    newIn = inscatterRayleigh * (sunlight.rgb + IBL) + inscatterMie * sunlight.rgb + inscatterIBL * IBL;
	newOut += (Rayleigh_extinction * pRayleigh) + (Haze_extinction * pMie_haze) + (Fog_extinction * pMie_fog) + (1 * pMie_rain);

	planetRadius = length(pos * 0.001 + float3(0, EarthR, 0));
}

void acumulateFog(inout float3 inscatter, inout float3 outscatter, float3 new_in, float3 new_out) {
	inscatter += outscatter * new_in;
	outscatter = exp(-new_out);
}
