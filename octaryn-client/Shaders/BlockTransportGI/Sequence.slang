#ifndef OCTARYN_BLOCK_TRANSPORT_SEQUENCE
#define OCTARYN_BLOCK_TRANSPORT_SEQUENCE
#include "Types.slang"
// Sobol direction recurrence: x+1, x^2+x+1, x^3+x+1; initial m=(1),(1,3),(1,3,1).
// Reference: pbr-book.org/4ed/Sampling_and_Reconstruction/Sobol_Samplers
float4 bt_sequence(uint index,uint scramble) {
    const uint4 directions[32]={
        uint4(0x80000000u,0x80000000u,0x80000000u,0x80000000u),
        uint4(0x40000000u,0xc0000000u,0xc0000000u,0xc0000000u),
        uint4(0x20000000u,0xa0000000u,0x60000000u,0x20000000u),
        uint4(0x10000000u,0xf0000000u,0x90000000u,0x50000000u),
        uint4(0x08000000u,0x88000000u,0xe8000000u,0xf8000000u),
        uint4(0x04000000u,0xcc000000u,0x5c000000u,0x74000000u),
        uint4(0x02000000u,0xaa000000u,0x8e000000u,0xa2000000u),
        uint4(0x01000000u,0xff000000u,0xc5000000u,0x93000000u),
        uint4(0x00800000u,0x80800000u,0x68800000u,0xd8800000u),
        uint4(0x00400000u,0xc0c00000u,0x9cc00000u,0x25400000u),
        uint4(0x00200000u,0xa0a00000u,0xee600000u,0x59e00000u),
        uint4(0x00100000u,0xf0f00000u,0x55900000u,0xe6d00000u),
        uint4(0x00080000u,0x88880000u,0x80680000u,0x78080000u),
        uint4(0x00040000u,0xcccc0000u,0xc09c0000u,0xb40c0000u),
        uint4(0x00020000u,0xaaaa0000u,0x60ee0000u,0x82020000u),
        uint4(0x00010000u,0xffff0000u,0x90550000u,0xc3050000u),
        uint4(0x00008000u,0x80008000u,0xe8808000u,0x208f8000u),
        uint4(0x00004000u,0xc000c000u,0x5cc0c000u,0x51474000u),
        uint4(0x00002000u,0xa000a000u,0x8e606000u,0xfbea2000u),
        uint4(0x00001000u,0xf000f000u,0xc5909000u,0x75d93000u),
        uint4(0x00000800u,0x88008800u,0x6868e800u,0xa0858800u),
        uint4(0x00000400u,0xcc00cc00u,0x9c9c5c00u,0x914e5400u),
        uint4(0x00000200u,0xaa00aa00u,0xeeee8e00u,0xdbe79e00u),
        uint4(0x00000100u,0xff00ff00u,0x5555c500u,0x25db6d00u),
        uint4(0x00000080u,0x80808080u,0x8000e880u,0x58800080u),
        uint4(0x00000040u,0xc0c0c0c0u,0xc0005cc0u,0xe54000c0u),
        uint4(0x00000020u,0xa0a0a0a0u,0x60008e60u,0x79e00020u),
        uint4(0x00000010u,0xf0f0f0f0u,0x9000c590u,0xb6d00050u),
        uint4(0x00000008u,0x88888888u,0xe8006868u,0x800800f8u),
        uint4(0x00000004u,0xccccccccu,0x5c009c9cu,0xc00c0074u),
        uint4(0x00000002u,0xaaaaaaaau,0x8e00eeeeu,0x200200a2u),
        uint4(0x00000001u,0xffffffffu,0xc5005555u,0x50050093u)
    };
    uint4 bits=uint4(bt_hash(scramble^0x63d83595u),bt_hash(scramble^0xa511e9b3u),
        bt_hash(scramble^0x9e3779b9u),bt_hash(scramble^0x85ebca6bu));
    for(uint bit=0;index!=0;++bit,index>>=1)if((index&1)!=0)bits^=directions[bit];
    // Exact 23-bit cell centers keep both endpoints out after float conversion.
    return (float4(bits>>9)+.5)/8388608.0;
}
float4 bt_transport_sequence(int4 key,uint slot,uint batch) {
    uint scramble=bt_key_hash(key)^bt_hash(batch>>28);
    return bt_sequence((batch<<4)|(slot&15u),scramble);
}
#endif
