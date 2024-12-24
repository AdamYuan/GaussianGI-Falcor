#include "GMesh.slangh"
__exported import GMesh;
__exported import GLighting;
__exported import GTransform;
__exported import Scene.Camera.Camera;
__exported import Utils.Math.Ray;
import GaussianGI.Util.ShaderUtil;
import Utils.Geometry.GeometryHelpers;
import Utils.Attributes;

public struct GStaticScene
{
    public struct HitInfo
    {
        public bool isHit;
        public bool isFrontFace;
        public uint meshPrimitiveID;
        public uint instanceID;
        public float2 barycentrics;

        public static HitInfo makeNoHit() { return (HitInfo)0; }
    };
    public struct HitPoint
    {
        public MeshInfo meshInfo;
        public InstanceInfo instanceInfo;
        public float3 localPosition;
        public float3 localNormal;
        public float3 localGeomNormal;
        public float3 position;
        public float3 normal;
        public float3 geomNormal;
        public uint textureID;
        public float2 texcoord;

        public float3 getRayOrigin() { return computeRayOrigin(position, geomNormal); }
    };
    public struct MeshInfo
    {
        public uint indexCount, instanceCount, firstIndex, firstInstance;
    };
    public struct InstanceInfo
    {
        public GTransform transform;
        public uint meshID;
    };

    StructuredBuffer<GMesh::Vertex> vertices;
    StructuredBuffer<uint> indices;
    Buffer<uint> textureIDs;
    StructuredBuffer<MeshInfo> meshInfos;
    StructuredBuffer<InstanceInfo> instanceInfos;
    RaytracingAccelerationStructure rtAccel;
    public Texture2D<float4> textures[GMESH_MAX_TEXTURE_COUNT];
    public SamplerState sampler;
    public Camera camera;
    public GLighting lighting;
    public uint meshCount, instanceCount;

    public GMesh::Vertex getVertex(uint index) { return vertices[index]; }
    public MeshInfo getMeshInfo(uint meshID) { return meshInfos[meshID]; }
    public InstanceInfo getInstanceInfo(uint instanceID) { return instanceInfos[instanceID]; }

    public static uint getGlobalPrimitiveID(in const MeshInfo meshInfo, uint meshPrimitiveID)
    {
        return meshInfo.firstIndex / 3 + meshPrimitiveID;
    }
    public uint getGlobalPrimitiveID(uint meshID, uint meshPrimitiveID)
    {
        return getGlobalPrimitiveID(getMeshInfo(meshID), meshPrimitiveID);
    }
    /* public uint3 getPrimitiveIndices(uint globalPrimitiveID)
    {
        return uint3(indices[globalPrimitiveID * 3], indices[globalPrimitiveID * 3 + 1], indices[globalPrimitiveID * 3 + 2]);
    } */
    public void getPrimitiveVertices(uint globalPrimitiveID, out GMesh::Vertex o_primitiveVertices[3])
    {
        var primitiveIndices =
            uint3(indices[globalPrimitiveID * 3], indices[globalPrimitiveID * 3 + 1], indices[globalPrimitiveID * 3 + 2]);
        o_primitiveVertices[0] = getVertex(primitiveIndices[0]);
        o_primitiveVertices[1] = getVertex(primitiveIndices[1]);
        o_primitiveVertices[2] = getVertex(primitiveIndices[2]);
    }
    public uint getPrimitiveTextureID(uint globalPrimitiveID) { return textureIDs[globalPrimitiveID]; }

    public HitPoint getHitPoint(in const HitInfo hitInfo)
    {
        if (!hitInfo.isHit)
            return (HitPoint)0;

        var instanceInfo = getInstanceInfo(hitInfo.instanceID);
        return getHitPoint(hitInfo, instanceInfo);
    }

    public HitPoint getHitPoint(in const HitInfo hitInfo, in const InstanceInfo instanceInfo)
    {
        if (!hitInfo.isHit)
            return (HitPoint)0;

        var meshInfo = getMeshInfo(instanceInfo.meshID);
        return getHitPoint(hitInfo, instanceInfo, meshInfo);
    }

    public HitPoint getHitPoint(in const HitInfo hitInfo, in const InstanceInfo instanceInfo, in const MeshInfo meshInfo)
    {
        if (!hitInfo.isHit)
            return (HitPoint)0;

        uint globalPrimitiveID = getGlobalPrimitiveID(meshInfo, hitInfo.meshPrimitiveID);
        GMesh::Vertex vertices[3];
        getPrimitiveVertices(globalPrimitiveID, vertices);

        float3 baryWeights = float3(1 - hitInfo.barycentrics.x - hitInfo.barycentrics.y, hitInfo.barycentrics);

        float3 localPosition = mul(baryWeights, float3x3(vertices[0].position, vertices[1].position, vertices[2].position));
        float3 localNormal = mul(baryWeights, float3x3(vertices[0].normal, vertices[1].normal, vertices[2].normal));
        localNormal = normalize(localNormal);
        localNormal = select(hitInfo.isFrontFace, localNormal, -localNormal);
        float3 localGeomNormal = normalize(cross(vertices[1].position - vertices[0].position, vertices[2].position - vertices[0].position));
        localGeomNormal = select(hitInfo.isFrontFace, localGeomNormal, -localGeomNormal);

        uint textureID = getPrimitiveTextureID(globalPrimitiveID);

        float2 texcoord = mul(baryWeights, float3x2(vertices[0].texcoord, vertices[1].texcoord, vertices[2].texcoord));

        HitPoint hitPoint;
        hitPoint.meshInfo = meshInfo;
        hitPoint.instanceInfo = instanceInfo;
        hitPoint.localPosition = localPosition;
        hitPoint.localNormal = localNormal;
        hitPoint.localGeomNormal = localGeomNormal;
        hitPoint.position = instanceInfo.transform.apply(localPosition);
        hitPoint.normal = instanceInfo.transform.applyRotation(localNormal);
        hitPoint.geomNormal = instanceInfo.transform.applyRotation(localGeomNormal);
        hitPoint.textureID = textureID;
        hitPoint.texcoord = texcoord;

        return hitPoint;
    }

    public HitInfo traceClosestHit(in const Ray ray, bool useAlphaDiscard)
    {
        float hitT;
        return traceClosestHit(ray, useAlphaDiscard, hitT);
    }
    public HitInfo traceClosestHit(in const Ray ray, bool useAlphaDiscard, out float o_hitT)
    {
        if (useAlphaDiscard)
        {
            RayQuery<RAY_FLAG_NONE> rayQuery;
            return traceClosestHitImpl<true>(rayQuery, ray, o_hitT);
        }
        else
        {
            RayQuery<RAY_FLAG_FORCE_OPAQUE> rayQuery;
            return traceClosestHitImpl<false>(rayQuery, ray, o_hitT);
        }
    }
    public bool traceAnyHit(in const Ray ray, bool useAlphaDiscard)
    {
        if (useAlphaDiscard)
        {
            RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayQuery;
            return traceAnyHitImpl<true>(rayQuery, ray);
        }
        else
        {
            RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayQuery;
            return traceAnyHitImpl<false>(rayQuery, ray);
        }
    }

    HitInfo getNonOpaqueCandidateHitInfo<let Flags : int>(in const RayQuery<Flags> rayQuery)
    {
        HitInfo hitInfo;
        hitInfo.isHit = true;
        hitInfo.instanceID = rayQuery.CandidateInstanceIndex();
        hitInfo.meshPrimitiveID = rayQuery.CandidatePrimitiveIndex();
        hitInfo.barycentrics = rayQuery.CandidateTriangleBarycentrics();
        hitInfo.isFrontFace = rayQuery.CandidateTriangleFrontFace();
        return hitInfo;
    }
    HitInfo getCommittedHitInfo<let Flags : int>(in const RayQuery<Flags> rayQuery)
    {
        HitInfo hitInfo;
        hitInfo.isHit = true;
        hitInfo.instanceID = rayQuery.CommittedInstanceIndex();
        uint firstOpaquePrimitiveID = rayQuery.CommittedInstanceID();
        hitInfo.meshPrimitiveID =
            select(rayQuery.CommittedGeometryIndex() == 0, 0, firstOpaquePrimitiveID) + rayQuery.CommittedPrimitiveIndex();
        hitInfo.barycentrics = rayQuery.CommittedTriangleBarycentrics();
        hitInfo.isFrontFace = rayQuery.CommittedTriangleFrontFace();
        return hitInfo;
    }

    HitInfo traceClosestHitImpl<let UseAlphaDiscard : bool, let Flags : int>(
        inout RayQuery<Flags> io_rayQuery,
        in const Ray ray,
        out float o_hitT
    )
    {
        io_rayQuery.TraceRayInline(rtAccel, RAY_FLAG_NONE, 0xFF, ray.toRayDesc());

        while (io_rayQuery.Proceed())
        {
            if (UseAlphaDiscard && io_rayQuery.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
            {
                HitInfo hitInfo = getNonOpaqueCandidateHitInfo(io_rayQuery);
                HitPoint hitPoint = getHitPoint(hitInfo);
                float alpha = textures[hitPoint.textureID].SampleLevel(sampler, hitPoint.texcoord, 0).a;
                if (alphaShouldDiscard(alpha))
                    continue;

                io_rayQuery.CommitNonOpaqueTriangleHit();
            }
        }

        if (io_rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            o_hitT = io_rayQuery.CommittedRayT();
            return getCommittedHitInfo(io_rayQuery);
        }

        o_hitT = 0.0;
        return HitInfo::makeNoHit();
    }

    bool traceAnyHitImpl<let UseAlphaDiscard : bool, let Flags : int>(inout RayQuery<Flags> io_rayQuery, in const Ray ray)
    {
        io_rayQuery.TraceRayInline(rtAccel, RAY_FLAG_NONE, 0xFF, ray.toRayDesc());

        while (io_rayQuery.Proceed())
        {
            if (UseAlphaDiscard && io_rayQuery.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
            {
                HitInfo hitInfo = getNonOpaqueCandidateHitInfo(io_rayQuery);
                HitPoint hitPoint = getHitPoint(hitInfo);
                float alpha = textures[hitPoint.textureID].SampleLevel(sampler, hitPoint.texcoord, 0).a;
                if (alphaShouldDiscard(alpha))
                    continue;

                io_rayQuery.CommitNonOpaqueTriangleHit();
            }
        }

        return io_rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
    }
}

public ParameterBlock<GStaticScene> gGStaticScene;
