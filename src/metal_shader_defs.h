#pragma once
#include <simd/simd.h>
enum class VertexBufferIndex {
    ScreenVertices=0,
    ViewportSize
};
struct VertexData {
    vector_float2 vertexCoord;
    vector_float2 textureCoord;
};
