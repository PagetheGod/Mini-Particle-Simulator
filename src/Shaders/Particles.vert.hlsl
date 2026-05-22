
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
/*
 * We changed to using a per particle SoA storage buffers to improve cache locality
 * 8 storage buffers bound to a single set, one StructuredBuffer<float> per particle attribute
 * The order must match the descriptor set layout binding numbers
 * 0 = Px, 1 = Py, 2 = Pz, 3 = R, 4 = G, 5 = B, 6 = Size, 7 = LifeTime, 8 = MaxLifeTime
 * All 9 buffers are read only in the shader, so no need for RWStructuredBuffer
 */

// Syntax means binding(slot, set)
[[vk::binding(0, 0)]] StructuredBuffer<float> Px;
[[vk::binding(1, 0)]] StructuredBuffer<float> Py;
[[vk::binding(2, 0)]] StructuredBuffer<float> Pz;
[[vk::binding(3, 0)]] StructuredBuffer<float> R;
[[vk::binding(4, 0)]] StructuredBuffer<float> G;
[[vk::binding(5, 0)]] StructuredBuffer<float> B;
[[vk::binding(6, 0)]] StructuredBuffer<float> Size;
[[vk::binding(7, 0)]] StructuredBuffer<float> LifeTime;
[[vk::binding(8, 0)]] StructuredBuffer<float> MaxLifeTime;

struct VertexInput
{
    // The n in position[n] is there so we can have multiple members with the same semantic
    // It helps the shader compiler to distinguish between members with the same semantic but different types or purposes
    // Right now we only have a vertex positions, since we moved the per-instance data to the structured buffers above
    [[vk::location(0)]] float2 Position : POSITION0; // Particle billboard vertex position [-1, 1]
};

// Outputs from the vertex shader. This is what gets passed to the fragment/pixel shader
struct VertexOutput
{
    float4 Position : SV_POSITION; // Final position of the vertex in clip space
    [[vk::location(0)]] float4 FragmentColor : COLOR0; 
    [[vk::location(1)]] float2 FragmentTexCoord : TEXCOORD0; // UV coordinates for texture sampling
};

/*
 * Instance ID refers to, well the id for the particular instance of the particle, it's supplied by Vulkan when we call
 * vkCmdDraw(CommandBuffer, VertexCount(6), InstanceCount(num particles)). We then use the instance ID to fetch the
 * instance-specific data from the structured buffers
 */

VertexOutput main(VertexInput Input, uint InstanceID : SV_InstanceID)
{
    VertexOutput Output;
    /*
     * We now have to first fetch the instance-specific data from the structured buffers, including positions, colors, and size
     */
    float3 InstancePos = float3(Px[InstanceID], Py[InstanceID], Pz[InstanceID]);
    float4 InstanceColor = float4(R[InstanceID], G[InstanceID], B[InstanceID], 1.f);
    float InstanceSize = Size[InstanceID];
    float InstanceLifeTime = LifeTime[InstanceID];
    float InstanceMaxLifeTime = MaxLifeTime[InstanceID];
    //float InstanceAlpha = InstanceLifeTime / InstanceMaxLifeTime; // Lifetime based linear fades, can be removed
    
    // Calculate the world position of all particle vertices using camera right and up vectors
    // So we get a billboard that always faces the camera
    float4 WorldPos = float4(InstancePos, 1.f);
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
    WorldPos.xyz += PushConstants.CameraRight * Input.Position.x * InstanceSize;
    WorldPos.xyz += PushConstants.CameraUp * Input.Position.y * InstanceSize;

    // Transform the vertex coordinate to clip space using the view projection matrix
    // HLSL uses row-major order, so multiplication goes from left to right
    WorldPos = mul(WorldPos, PushConstants.ViewProjection);

    Output.Position = WorldPos;
    Output.FragmentColor = InstanceColor;
    /*
     * This looks a little bit weird because we are not actually using the UV in the fragment shader to sample any texture
     * We are using the UV as a way to generate color directly, based on the fragment's distance away from the center of the particle
     * Therefore we pass the vertex's original positions([-1, 1]) directly
     */
    Output.FragmentTexCoord = Input.Position;
   
    return Output;
}   