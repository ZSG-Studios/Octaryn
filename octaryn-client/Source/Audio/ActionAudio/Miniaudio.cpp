// Only PCM synthesis is needed; OpenAL owns output devices and mixing.
#define MA_NO_DEVICE_IO
#define MA_NO_ENCODING
#define MA_NO_DECODING
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
