
// Basically constant buffer in DX11, so we have a strict 16-byte alignment requirement
// Both for the size of the struct and for each individual member
struct Constants
{
    float4x4 ViewProjection; // View Matrix x ProjectionMatrix
    float3 CameraRight; // Camera's right vector(x-axis) in world space, normalized
    float Padding1;
    float3 CameraUp; // Camera's up vector(y-axis) in world space, normalized
    float Padding2;
};

[[vk::push_constant]] Constants PushConstants;


// Inputs to the vertex shader, both per vertex and per instance data
// Per instance because we are using instance rendering for the particles
struct VertexInput
{
    // The n in position[n] is there so we can have multiple members with the same semantic
    // It helps the shader compiler to distinguish between members with the same semantic but different types or purposes
    [[vk::location(0)]] float2 Position : POSITION0; // Particel billboard vertex position [-1, 1]
    [[vk::location(1)]] float3 InstancePos : POSITION1; // Particle center (world XY)
    [[vk::location(2)]] float4 InstanceColor : COLOR0; // Particle color (RGBA)
    [[vk::location(3)]] float InstanceSize : PSIZE; // Particle radius
};

// Outputs from the vertex shader. This is what gets passed to the fragment/pixel shader
struct VertexOutput
{
    float4 Position : SV_POSITION; // Final position of the vertex in clip space
    [[vk::location(0)]] float4 FragmentColor : COLOR0; 
    [[vk::location(1)]] float2 FragmentTexCoord : TEXCOORD0; // UV coordinates for texture sampling
};


VertexOutput main(VertexInput Input)
{
    VertexOutput Output;
    
    // Calculate the world position of all particle vertices using camera right and up vectors
    // So we get a billboard that always faces the camera
    float4 WorldPos = float4(Input.InstancePos, 1.f);
    // Multiply the camera right by position x has the effect of moving the vertex left or right
    /*
     * Basically,
     * A - - - - - B  A = (-1, 1) B = (1, 1) in the vertex positions, note this DIFFERENT from the instance position which is the center of the particle
     * | - - - - - |
     * | - - - - - |
     * | - - - - - |
     * C - - - - - D  C = (-1, -1) D = (1, -1)
     * Note how x is either -1 or 1, so by multiplying it with camera right, we can get both the left and right vertices' x positions
     * Same thing for the camera up and the y position, we can get both the top and bottom vertices' y positions
     */
    WorldPos += (PushConstants.CameraRight * Input.Position.x * Input.InstanceSize);
    WorldPos += (PushConstants.CameraUp * Input.Position.y * Input.InstanceSize);

    // Transform the vertex coordinate to clip space using the view projection matrix
    // HLSL uses row-major order, so multiplication goes from left to right
    WorldPos = mul(WorldPos, PushConstants.ViewProjection);

    Output.Position = WorldPos;
    Output.FragmentColor = Input.InstanceColor;
    /*
     * This looks a little bit weird because we are not actually using the UV in the fragment shader to sample any texture
     * We are using the UV as a way to generate color directly, based on the fragment's distance away from the center of the particle
     * Therefore we pass the vertex's original positions([-1, 1]) directly
     */
    Output.FragmentTexCoord = Input.Position;
   
    return Output;
}   