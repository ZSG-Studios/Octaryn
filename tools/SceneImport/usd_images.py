"""Bound encoded images and reject malformed PNG/JPEG containers before copying."""
import struct
import zlib
from usd_support import require


def validate(path, budget):
    require(0 < path.stat().st_size <= 64*1024*1024, "USD image encoded size exceeds 64 MiB")
    budget.check()
    data = path.read_bytes()
    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        png(data,budget)
    else:
        jpeg(data)


def png(data,budget):
    offset, compressed, header = 8, bytearray(), None
    ended, palette = False, False
    while offset < len(data):
        budget.check()
        require(offset+12 <= len(data), "Truncated USD PNG chunk")
        size = struct.unpack_from(">I",data,offset)[0]
        require(size <= len(data)-offset-12, "Truncated USD PNG chunk payload")
        name, payload = data[offset+4:offset+8], data[offset+8:offset+8+size]
        crc = struct.unpack_from(">I",data,offset+8+size)[0]
        require(zlib.crc32(name+payload) == crc, "Invalid USD PNG chunk CRC")
        if header is None: require(name == b"IHDR", "USD PNG lacks first IHDR chunk")
        if name == b"IHDR":
            require(header is None and size == 13, "Invalid USD PNG header")
            header = struct.unpack(">IIBBBBB",payload)
        elif name == b"PLTE": palette = True
        elif name == b"IDAT": compressed.extend(payload)
        elif name == b"IEND":
            require(size == 0, "Invalid USD PNG IEND")
            ended = True; offset += 12; break
        elif name[0] < 97: require(False, "Unknown critical USD PNG chunk")
        offset += size+12
    require(ended and offset == len(data) and compressed, "Incomplete USD PNG image")
    width,height,depth,color,compression,filter_method,interlace = header
    require(width > 0 and height > 0 and width*height <= 16*1024*1024, "USD image pixel budget exceeded")
    channels = {0:1,2:3,3:1,4:2,6:4}.get(color)
    require(channels and depth in ({1,2,4,8,16} if color == 0 else {1,2,4,8} if color == 3 else {8,16}),
            "Unsupported USD PNG color/depth")
    require(compression == 0 and filter_method == 0 and interlace in (0,1), "Unsupported USD PNG encoding")
    require(color != 3 or palette, "Indexed USD PNG lacks a palette")
    passes = [(0,0,1,1)] if not interlace else [(0,0,8,8),(4,0,8,8),(0,4,4,8),(2,0,4,4),(0,2,2,4),(1,0,2,2),(0,1,1,2)]
    rows = []
    for x,y,dx,dy in passes:
        w,h = max(0,(width-x+dx-1)//dx), max(0,(height-y+dy-1)//dy)
        if w and h: rows.extend([1+(w*channels*depth+7)//8]*h)
    expected = sum(rows)
    require(expected <= 64*1024*1024, "USD image decoded size exceeds 64 MiB")
    decoder = zlib.decompressobj()
    decoded = decoder.decompress(compressed,expected+1)
    require(len(decoded) == expected and decoder.eof and not decoder.unconsumed_tail and not decoder.unused_data,
            "Invalid USD PNG compressed pixels")
    offset = 0
    for stride in rows:
        require(decoded[offset] <= 4, "Invalid USD PNG scanline filter")
        offset += stride


def jpeg(data):
    require(data[:2] == b"\xff\xd8" and data[-2:] == b"\xff\xd9", "Invalid/truncated USD JPEG image")
    offset, dimensions = 2, False
    while offset < len(data)-2:
        require(data[offset] == 255, "Invalid USD JPEG marker")
        while offset < len(data) and data[offset] == 255: offset += 1
        require(offset < len(data), "Truncated USD JPEG marker")
        marker = data[offset]; offset += 1
        require(offset+2 <= len(data), "Truncated USD JPEG segment")
        size = struct.unpack_from(">H",data,offset)[0]
        require(size >= 2 and offset+size <= len(data), "Invalid USD JPEG segment size")
        if marker in (0xc0,0xc2):
            require(size >= 8 and data[offset+2] == 8, "USD JPEG requires eight-bit baseline/progressive encoding")
            height,width = struct.unpack_from(">HH",data,offset+3)
            require(width > 0 and height > 0 and width*height <= 16*1024*1024, "USD JPEG pixel budget exceeded")
            dimensions = True
        if marker == 0xda:
            require(dimensions, "USD JPEG lacks supported dimensions")
            return
        offset += size
    require(False, "USD JPEG lacks scan data")
