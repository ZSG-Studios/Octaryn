namespace Octaryn.Shared.Content;

// Bounded little-endian source values, shared by importers and game libraries.
public static class BinaryValue
{
    public static uint ReadUInt32(System.ReadOnlySpan<byte> bytes,int offset)
    {
        if(offset<0 || offset>bytes.Length-4)throw new System.InvalidOperationException("Truncated uint32 source value.");
        return (uint)bytes[offset]|((uint)bytes[offset+1]<<8)|((uint)bytes[offset+2]<<16)|((uint)bytes[offset+3]<<24);
    }
    public static float ReadSingle(System.ReadOnlySpan<byte> bytes,int offset)
        => System.BitConverter.Int32BitsToSingle(unchecked((int)ReadUInt32(bytes,offset)));
}
