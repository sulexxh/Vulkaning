struct VSInput {
    float3 Pos;
    float3 Normal;
    float2 UV;
};

Sampler2D textures[];

/*
    Same layout as the shader data structure on the CPU-side
*/
struct ShaderData {
    float4x4 projection;
    float4x4 view;
    float4x4 model;
    float4 lightPos;
};

struct VSOutput {
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 UV;
    float3 LightVec;
    float3 ViewVec;
    uint32_t InstanceIndex;
};

/*
    Slang let's us put all shader stages into 1 file, saving us the need to duplicate the shader interface or putting
    in shared includes.
    The Vertex shader is invoked for every vertex drawn. It takes in the vertices from VSInput, matching the graphics
    pipeline vertex layout. As we use the buffer device address, we pass and access the UBO as a pointer. We draw
    multiple instances of the 3D model to use difference matrices for every instance, using the SV_VulkanInstanceID
    system value to index into the model matrices. WE also want to highlight the selected model, so if the current 
    instance matches that selection we pass in a different colour factor to the fragment shader.
*/
[shader("vertex")]
VSOutput main(VSInput input, uniform ShaderData *shaderData, uint instanceIndex: SV_VulkanInstanceID) {
    VSOutput output;
    float4x4 modelMat = shaderData->model;
    output.Normal = mul((float3x3)mul(shaderData->view, modelMat), input.Normal);
    output.UV = input.UV;
    output.Pos = mul(shaderData->projection, mul(shaderData->view, mul(modelMat, float4(input.Pos.xyz, 1.0))));
    output.InstanceIndex = instanceIndex;

    // Calculate view vectors required for lighting
    float4 fragPos = mul(mul(shaderData->view, modelMat), float4(input.Pos.xyz, 1.0));
    output.LightVec = shaderData->lightPos.xyz - fragPos.xyz;
    output.ViewVec = -fragPos.xyz;
    return output;
}

/*
    We do some basic lighting here using the values passed from the vertex shader. Then using descriptor indexing we
    read from the array of textures[] using the instance index, and combine that with the lighting calculation. This is
    written to the current colour attachment.
*/
[shader("fragment")]
float4 main(VSOutput input) {
    // Phong lighting
    float3 N = normalize(input.Normal);
    float3 L = normalize(input.LightVec);
    float3 V = normalize(input.ViewVec);
    float3 R = reflect(-L, N);
    float3 diffuse = max(dot(N, L), 0.0025);
    float3 specular = pow(max(dot(R, V), 0.0), 16.0) * 0.75;

    // Sample from texture
    float3 colour = textures[0].Sample(input.UV).rgb;
    return float4(diffuse * colour.rgb + specular, 1.0);
}
