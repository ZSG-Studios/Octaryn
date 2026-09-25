#pragma once
#include "Probe.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"

namespace mesh_probe {
struct BlockTransportSampleQuery {BlockSurfaceKey key;std::array<unsigned,4> info;};
struct BlockTransportSampleResult {std::array<float,4> origin,direction,direct,sequence,direct_sequence;};
std::vector<BlockTransportSampleResult> block_transport_samples(Fixture&,
    const std::vector<BlockTransportSampleQuery>&);
void block_transport_sampling_cases(Fixture&);
}
