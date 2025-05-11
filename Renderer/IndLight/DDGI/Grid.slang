import GaussianGI.Algorithm.SphericalHarmonics;

public namespace DDGI
{
    public struct Grid
    {
        float3 base;
        float unit;
        uint dim;

        public float3 gridPos2WorldPos(int3 gridPos)
        {
            gridPos = clamp(gridPos, 0, dim - 1);
            return base + float3(gridPos) * unit;
        }
        public uint gridPos2GridIndex(int3 gridPos)
        {
            gridPos = clamp(gridPos, 0, dim - 1);
            return (gridPos.x * dim + gridPos.y) * dim + gridPos.z;
        }
        public int3 worldPos2GridPos(float3 worldPos, out float3 o_weights)
        {
            int3 gridPos = int3(floor((worldPos - base) / unit));
            gridPos = clamp(gridPos, 0, dim - 1);
            o_weights = saturate((worldPos - gridPos2WorldPos(gridPos)) / unit);
            return gridPos;
        }
        // idx = 0, ..., 7
        public int3 getNeighbourGridPos(int3 gridPos, uint idx)
        {
            gridPos += int3((idx >> 2) & 1, (idx >> 1) & 1, idx & 1);
            gridPos = clamp(gridPos, 0, dim - 1);
            return gridPos;
        }
        public uint getNeighbourGridIndex(int3 gridPos, uint idx) { return gridPos2GridIndex(getNeighbourGridPos(gridPos, idx)); }
        public float3 getNeighbourWeight(float3 weights, uint idx)
        {
            bool3 nei = bool3(bool((idx >> 2) & 1), bool((idx >> 1) & 1), bool(idx & 1));
            weights = select(nei, weights, 1.0 - weights);
            return weights.x * weights.y * weights.z;
        }

        public float3 convolveProbe(StructuredBuffer<SH9<float16_t3>> probes, float3 worldPos, float3 dir)
        {
            float3 weights;
            int3 gridPos = worldPos2GridPos(worldPos, weights);

            float3 result = float3(0);

            SH9<float> dirSh = shBasis<2>(dir);

            [ForceUnroll]
            for (uint i = 0; i < 8; ++i)
            {
                SH9<float16_t3> probe = probes[getNeighbourGridIndex(gridPos, i)];
                result += max(shDiffuse(probe, dirSh), 0.0f) * getNeighbourWeight(weights, i);
            }
            return result;
        }
    }
}
