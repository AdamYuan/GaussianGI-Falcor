public bool cullQuadClip(Array<float4, 4> vertexClips)
{
    float4 wClips = float4(vertexClips[0].w, vertexClips[1].w, vertexClips[2].w, vertexClips[3].w);
    if (all(wClips <= 0.0))
        return true;

    // Test Against Clip AABB
    float4 xClips = float4(vertexClips[0].x, vertexClips[1].x, vertexClips[2].x, vertexClips[3].x);
    float4 yClips = float4(vertexClips[0].y, vertexClips[1].y, vertexClips[2].y, vertexClips[3].y);
    float4 zClips = float4(vertexClips[0].z, vertexClips[1].z, vertexClips[2].z, vertexClips[3].z);
    if (all(zClips <= 0) || all(zClips >= wClips) || all(xClips <= -wClips) || all(xClips >= wClips) || all(yClips <= -wClips) ||
        all(yClips >= wClips))
        return true;

    // Test 2D Quad Edges
    float2 clips[4] = { vertexClips[0].xy, vertexClips[1].xy, vertexClips[2].xy, vertexClips[3].xy };

    // 3 -- 2
    // |    |
    // 0 -- 1

    // Ensure CCW order in 2D
    if (determinant(float2x2(clips[1] - clips[0], clips[2] - clips[0])) < 0)
        clips = { clips[3], clips[2], clips[1], clips[0] };

    [ForceUnroll]
    for (uint i = 0; i < 4; ++i)
    {
        float2 edge = clips[(i + 1) & 3] - clips[i];
        float edgeY = dot(float2(-edge.y, edge.x), clips[i]);
        // if (abs(edge.x) + abs(edge.y) < edgeY)
        //     return true;
    }

    return false;
}
