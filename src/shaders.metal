#include <metal_stdlib>

using namespace metal;
struct RasterizerData {
    float4 clipSpaceVertex[[position]];
    float2 textureCoord;
};
vertex RasterizerData vertexShader(uint vertexID[[vertex_id]],
                                   constant vector_float2 *vertexArray[[buffer(0)]]) {
    RasterizerData ret = {{vertexArray[vertexID].x, vertexArray[vertexID].y, 0.0, 1.0}, 
        {(1 + vertexArray[vertexID].x)/2,(1 - vertexArray[vertexID].y)/2}};
    return ret;
} 
fragment float4 fragmentShader(RasterizerData in [[stage_in]], texture2d<half> screenTexture [[texture(0)]]) {
        constexpr sampler textureSampler (mag_filter::nearest);
        return float4(screenTexture.sample(textureSampler, in.textureCoord));
} 
