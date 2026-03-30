struct ParticleInput
{
    float4 Position : SV_POSITION;
    [[vk::location(0)]] float4 FragmentColor : COLOR0;
    [[vk::location(1)]] float4 FragmentTexCoord : TEXCOORD0;
};

float4 main(ParticleInput Input) : SV_TARGET0
{
    float Distance = length(Input.FragmentTexCoord);
    clip(1.f - Distance);
    float AlphaFallOff = 1.f - smoothstep(0.8f, 1.f, Distance);
    return float4(Input.FragmentColor.rgb, Input.FragmentColor.a * AlphaFallOff);
}