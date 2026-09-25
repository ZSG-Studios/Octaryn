#pragma once
#include "BlockTransportGI.h"
namespace octaryn::client::rendering {
bool prepare_block_transport_admission(WorldRenderer&,bool reset);
bool admit_block_transport_world(WorldRenderer&,rhi::ICommandEncoder*);
void commit_block_transport_admission(WorldRenderer&,bool reset);
bool prepare_block_transport_lights(WorldRenderer&,rhi::ICommandEncoder*);
bool update_block_transport_coverage(WorldRenderer&);
bool resolve_block_transport_statistics(WorldRenderer&);
}
