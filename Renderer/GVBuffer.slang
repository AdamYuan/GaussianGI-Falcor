public struct GVBuffer
{
    public static uint encodeMeshInstanceID(uint meshID, uint instanceID) { return meshID << 16 | instanceID; }
}
