#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "shader_phong_lighting.cpp"

layout(binding = 0) uniform ocean_input_buffer
{
    mat4 VP;
    vec3 CameraPos;
    float Time;
} InputBuffer;

#if VERTEX_SHADER

layout(location = 0) in vec3 InPos;

layout(location = 0) out vec3 OutWorldPos;
layout(location = 1) out vec3 OutNormal;
layout(location = 2) out vec2 OutUv;

const float Pi = 3.14159;

vec3 GerstnerWave(float Time, float Steepness, float WaveLength, vec2 Direction, vec3 VertexWorldPos, inout vec3 VertexTangent,
                  inout vec3 VertexBinormal)
{
    vec3 VertexPos = vec3(0);

    // TODO: When steepness gets to 0.9 range, the tangent calc seems to get bad on the top of waves. Is this a mesh resolution bug?
    // NOTE: https://catlikecoding.com/unity/tutorials/flow/waves/
    // NOTE: The idea is that from the model pos of this vertex, the vertex can move in a circle half way to the next vertex. These are
    // Gerstner Waves. The intuition being that when water moves up, some of the nearby water has to come in to actually fill the bottom
    // with water (same total volume). So instead of just moving in z direction, we also move in x towards where the wave passing through
    // the water is.
    float k = 2*Pi / WaveLength;
    float c = sqrt(9.8 / k); // NOTE: This makes wave speed be a function of the waves size
    float f = k*(dot(Direction, VertexWorldPos.xy) - c*Time);
    float a = Steepness / k;

    VertexPos.xy = Direction*a*cos(f);
    VertexPos.z = a*sin(f);

    // NOTE: We calculate the normal by getting the tangent using the derivative and then reversing it
    VertexTangent += vec3(1 - Direction.x*Direction.x*Steepness*cos(f),
                          -Direction.x*Direction.y*Steepness*cos(f),
                          Direction.x*Steepness*cos(f)); // NOTE: dx
    VertexBinormal += vec3(-Direction.y*Direction.x*Steepness*sin(f),
                           1 - Direction.y*Direction.y*Steepness*sin(f),
                           Direction.y*Steepness*cos(f)); // NOTE: dy

    return VertexPos;
}

void main()
{
    // NOTE: These values get accumulated by each wave
    vec3 Tangent = vec3(0);
    vec3 Binormal = vec3(0);
    vec3 WorldPos = InPos;

    // NOTE: Wave 1
    {
        float Steepness = 0.5f; // NOTE: This is between 0 and 1
        float WaveLength = 10.0f;
        vec2 Direction = normalize(vec2(1, 0));
        WorldPos += GerstnerWave(InputBuffer.Time, Steepness, WaveLength, Direction, InPos, Tangent, Binormal);
    }

    // NOTE: Wave 2
    {
        float Steepness = 0.25f; // NOTE: This is between 0 and 1
        float WaveLength = 20.0f;
        vec2 Direction = normalize(vec2(0, 1));
        WorldPos += GerstnerWave(InputBuffer.Time, Steepness, WaveLength, Direction, InPos, Tangent, Binormal);
    }

    // NOTE: Wave 3
    {
        float Steepness = 0.15f; // NOTE: This is between 0 and 1
        float WaveLength = 10.0f;
        vec2 Direction = normalize(vec2(1, 1));
        WorldPos += GerstnerWave(InputBuffer.Time, Steepness, WaveLength, Direction, InPos, Tangent, Binormal);
    }
    
    vec3 Normal = normalize(cross(Binormal, Tangent));
    
    gl_Position = InputBuffer.VP*vec4(WorldPos, 1);
    OutWorldPos = WorldPos;
    OutNormal = Normal;
    OutUv = vec2(0, 0); //InUv;
}

#endif

#if FRAGMENT_SHADER

layout(location = 0) in vec3 InWorldPos;
layout(location = 1) in vec3 InWorldNormal;
layout(location = 2) in vec2 InUv;

layout(location = 0) out vec4 OutColor;

void main()
{
    vec3 Ambient = vec3(0.05, 0.05, 0.2);
    vec3 Diffuse = vec3(0.1, 0.1, 0.9);
    vec3 Specular = vec3(1, 1, 1);
    float SpecularPower = 10;

    vec3 LightIntensity = vec3(0.7, 0.7, 0.7);
    vec3 LightPos = vec3(20, 20, 5);

    vec3 Color = Ambient + PhongLighting(Diffuse, Specular, SpecularPower, InWorldPos, normalize(InWorldNormal), InputBuffer.CameraPos,
                                         LightPos, LightIntensity);

    OutColor = vec4(InWorldPos, 1); //vec4(Color, 1);
}

#endif
