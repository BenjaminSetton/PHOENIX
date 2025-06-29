#version 450

// Resources:
// PBR (Victor Gordan) - https://www.youtube.com/watch?v=RRE-F57fbXw&t=10s
// PBR explanation (OGL Dev) - https://www.youtube.com/watch?v=XK_p2MxGBQs&t=126s
// Diffuse IBL (LearnOpenGL) - https://learnopengl.com/PBR/IBL/Diffuse-irradiance

// Simplified rendering equation:
// vec3 finalColor = ( DiffuseBRDF + SpecularBRDF ) * LightIntensity * NdotL

// Explanation of the components of the equation above, assuming the Cook-Torrance BRDF function:
// [ DiffuseBRDF ]
// kD * Lambert / PI

// [ SpecularBRDF ]
// kS * CookTorrance()

// [ LightIntensity ]
// N

// [NdotL]
// dot( surfaceNormal, lightVector )

// Explanation of [ DiffuseBRDF ]:
// - kD and kS represent the ratio between the refraction and reflection components. Their sum must always equal 1, otherwise 
//   the law of conservation of energy is violated and this isn't a PBR shader anymore.
// - Lambert represents the material color (retrieved from diffuse texture). This means that the diffuse BRDF value is only applied
//   to dielectrics, and NOT metals!

// Explanation of [ SpecularBRDF ]:
// - The CookTorrance() function is, itself, made up of a separate equation, namely: (D * G * F) / 4 * dot(normal, light) * dot(normal, view).
//   D (normal distribution function), G (geometry function), and F (fresnel function) are all functions as well
// - D represents the number of microfacets that reflect light back at the viewer. More specifically, it tells us how many microfacets have their
//   half-vector aligned with the surface normal. If the half-vector is completely aligned with the normal, this means that ALL the light is being
//   reflected towards the viewer.
// - The function we use for D is GGX (Trowbridge & Reitz), given below:
//      roughness^2 / (PI * ( dot( normal, halfVector )^2 * ( roughness^2 - 1 ) + 1 )^2
//   When this function is 0 it implies an ideal smooth surface, while a value of 1 implies maximum roughness
// - G approximates the self-shadowing factor of microfacets. It tells us the probability that any given microfacet with it's normal is
//   visible to both the light source and the viewer
// - The function we use for G is GGX (Schlick & Beckmann), given below:
//      dot( normal, view ) / ( dot( normal, view ) * ( 1 - K ) + K )
//      where K = ( roughness + 1 )^2 / 8
// - F approximates the fresnel effect, where light bouncing off a lower degree of incidence will be reflected more often than light reflecting with
//   a higher degree of incidence (aka closer to the surface's normal).
// - The function we use for F is the Schlick approximation, given below:
//      baseReflectivity + ( 1 - baseReflectivity ) * ( 1 - dot( view, half ) )^5
//   NOTE: The base reflectivity changes over the visible spectrum, which is why it's represented as an RGB value instead of a 
//         float like most other values in the PBR equation. Common practice is to use a value of 0.04 (across all RGB values)
//         for all dielectric materials, but to use the real base reflectivity for metals (which can simply be looked up)

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in mat3 inTBN;

layout(set = 1, binding = 0) uniform sampler2D diffuseSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;
layout(set = 1, binding = 2) uniform sampler2D metallicSampler;
layout(set = 1, binding = 3) uniform sampler2D roughnessSampler;
layout(set = 1, binding = 4) uniform sampler2D lightmapSampler;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const float EPSILON = 0.000001;

// Van der Corput sequence, used for Hammersley sequence. Mirrors a decimal binary representation around it's decimal point
float RadicalInverse_VDC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Returns a low-discrepancy sample 'i' over the total sample size 'N'
vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VDC(i));
}

// Fresnel (Schlick)
vec3 F_Schlick(float cosTheta, vec3 F0)
{
    // F0 + ( 1 - F0 ) * ( 1 - dot( view, half ) )^5
    // where F0 represents the base reflectivity

    return F0 + ( 1.0 - F0 ) * pow( 1.0 - cosTheta, 16.0 );
}

// Fresnel (Schlick + roughness)
// NOTE - The roughness term accounts for the roughness around the edges of the surface
//        making the reflection weaker. This is needed since we started using diffuse IBL
//        (refer to diffuse IBL link at the top of the shader)
vec3 F_Roughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + ( max( vec3( 1.0 - roughness ), F0 ) - F0 ) * pow( ( 1.0 - cosTheta ), 5.0 );
}

// Geometry (GGX - Schlick & Beckmann)
float G_GGX(float NdotV, float roughness)
{
    // Using kIBL
    float a = roughness;
    float K = ( a * a ) / 2.0;

    float funcNominator = NdotV;
    float funcDenominator = NdotV * ( 1.0 - K ) + K;

    return funcNominator / funcDenominator;
}

// Geometry (Smith)
float G_Smith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float ggx2 = G_GGX(NdotV, roughness);
    float ggx1 = G_GGX(NdotL, roughness);

    return ggx1 * ggx2;
}  

// Normal Distribution (GGX - Trowbridge & Reitz)
float D_GGX(float HdotN, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;

    float funcNominator = a2;
    float funcDenominator = ( pow( HdotN, 2.0 ) * ( a2 - 1.0 ) + 1.0 );
    funcDenominator = PI * pow( funcDenominator, 2.0 );

    return funcNominator / ( funcDenominator + EPSILON ); // Prevent division by 0
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness*roughness;
	
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	
    // Spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
	
    // Tangent-space vector to world-space sample vector
    vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
	
    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}  

void main()
{
    outColor = vec4(inUV, 0.0, 1.0);
    return;

    // NORMAL MAP
    vec3 normal = texture(normalSampler, inUV).rgb;
    normal = normal * 2.0 - 1.0;
    normal = normalize( inTBN * normal );
    ////

    // BASE VECTORS
    vec3 cameraPos = vec3(0.0f, 1.0f, -7.0f); // TODO - Replace hard-coded camera position
    vec3 light = -normalize(vec3(0.4, -0.75, 1.0));
    vec3 view = normalize(cameraPos - inWorldPosition);
    vec3 halfVector = normalize(light + view);
    vec3 albedo = texture(diffuseSampler, inUV).rgb;
    vec3 reflection = reflect(-view, normal);
    ////

    // BASE DOT PRODUCTS + TEXTURE SAMPLES
    float NdotV = max(dot(normal, view), 0.0);
    float NdotL = max(dot(normal, light), 0.0);
    float HdotV = max(dot(halfVector, view), 0.0);
    float HdotN = max(dot(halfVector, normal), 0.0);
    float lightIntensity = 1.0; // NOTE - This is only for point / spotlights, which we do not support right now
    float metalness = texture(metallicSampler, inUV).b;
    float roughness = texture(roughnessSampler, inUV).g;
    ////

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metalness);

    vec3 pbrColor = vec3(0.0);
    // BRDF CALCULATION
    {
        float D = D_GGX(HdotN, roughness);
        float G = G_Smith(normal, view, light, roughness);
        vec3 F = F_Schlick(HdotV, F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metalness; // Kill diffuse component if we're dealing with a metal

        vec3 numerator = D * G * F;
        float denominator = ( 4.0 * NdotL * NdotV ) + EPSILON;
        vec3 specularBRDF = numerator / denominator;

        vec3 diffuseBRDF = kD * albedo / PI;

        pbrColor = ( diffuseBRDF + specularBRDF ) * lightIntensity * NdotL;
    }
    ////

    vec3 ambient = vec3(0.1);

    pbrColor += ambient;

    outColor = vec4( pbrColor, 1.0 );
}