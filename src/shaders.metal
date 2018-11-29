#include <metal_stdlib>
#include "metal_shader_defs.h"
using namespace metal;
struct RasterizerData {
    float4 clipSpaceVertex[[position]];
    float2 textureCoord;
};
vertex RasterizerData vertexShader(uint vertexID[[vertex_id]],
                                   constant VertexData *vertexData[[buffer(VertexBufferIndex::ScreenVertices)]],
                                   constant vector_float2 *viewportPtr[[buffer(VertexBufferIndex::ViewportSize)]]) {
    float2 viewport = *viewportPtr;
    float2 clipspaceCoord = vertexData[vertexID].vertexCoord.xy / viewport ;
    RasterizerData ret = {{clipspaceCoord.x, clipspaceCoord.y, 0.0, 1.0}, 
        {vertexData[vertexID].textureCoord.x, vertexData[vertexID].textureCoord.y}};
    return ret;
} 
fragment float4 fragmentShader(RasterizerData in [[stage_in]], texture2d<half> screenTexture [[texture(0)]]) {
        constexpr sampler textureSampler (min_filter::nearest, mag_filter::nearest);
        return float4(screenTexture.sample(textureSampler, in.textureCoord));
} 
