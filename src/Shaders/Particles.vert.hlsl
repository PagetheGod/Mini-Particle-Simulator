struct Particle
{
    float2 position;
    float2 velocity;
    float4 color;
    float  size;
    float  age;
    float  lifetime;
    float  _padding;  // Align to 16 bytes
};
