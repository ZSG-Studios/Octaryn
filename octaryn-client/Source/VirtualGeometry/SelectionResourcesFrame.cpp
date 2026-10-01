#include "SelectionResourcesInternal.h"
#include <algorithm>

namespace octaryn::client::rendering::virtual_geometry {
bool SelectionResources::State::collect(Bank& bank) {
  if(bank.encoder || !bank.fence)return true;
  std::uint64_t value{};
  if(SLANG_FAILED(bank.fence->getCurrentValue(&value)) || value==UINT64_MAX)return fail("shared selection bank fence failed");
  if(value<bank.signal)return true;
  std::uint64_t bytes{};
  for(const auto& weak:bank.tickets)if(auto ticket=weak.lock();ticket && !ticket->consumed && !ticket->ready) {
    if(!ticket->submitted)return fail("shared selection feedback lacks submission ownership");
    bytes+=24+std::uint64_t(ticket->capacity)*8+std::uint64_t(ticket->pages)*4;
  }
  if(!bytes) {bank.tickets.clear();return true;}
  if(bytes>feedback_memory->limit-feedback_memory->used.load())return fail("shared selection CPU feedback arena exhausted");
  void* data{};
  if(SLANG_FAILED(device->mapBuffer(bank.buffers.readback,rhi::CpuAccessMode::Read,&data)) || !data)
    return fail("shared selection feedback mapping failed");
  bool ok=true;
  try {
    for(const auto& weak:bank.tickets)if(auto ticket=weak.lock();ticket && !ticket->consumed && !ticket->ready) {
      const auto count=6+std::uint64_t(ticket->capacity)*2+ticket->pages;
      const auto* words=static_cast<const std::uint32_t*>(data)+ticket->offset/4;
      ticket->feedback.assign(words,words+count);ticket->memory=feedback_memory;
      feedback_memory->used.fetch_add(count*4);ticket->ready=true;
    }
  }catch(...) {ok=false;}
  const auto result=device->unmapBuffer(bank.buffers.readback);
  if(!ok || SLANG_FAILED(result))return fail("shared selection feedback copy failed");
  bank.tickets.clear();return true;
}
bool SelectionResources::record(rhi::ICommandEncoder* commands,std::uint64_t owner,const SelectionTopology& topology,
    std::uint32_t capacity,std::uint32_t instances,SharedSelectionRecording& output) {
  auto& s=*state_;output={};s.error.clear();
  if(!commands || !s.owners.contains(owner) || !supports(topology,capacity) || instances>s.config.instances)
    return s.fail("shared selection recording exceeds registered capacities");
  const auto pages=*std::max_element(topology.pages.begin(),topology.pages.end())+1;
  const auto bytes=feedback_bytes(pages,capacity);
  if(bytes!=s.owners.at(owner))return s.fail("shared selection owner feedback range changed");
  unsigned selected=invalid_id;
  for(unsigned i=0;i<s.banks.size();++i)if(s.banks[i].encoder==commands) {selected=i;break;}
  if(selected==invalid_id)for(unsigned i=0;i<s.banks.size();++i) {
    auto& bank=s.banks[i];if(bank.encoder || bank.generation==UINT32_MAX)continue;
    if(!s.collect(bank))return false;
    if(bank.fence) {
      std::uint64_t completed{};
      if(SLANG_FAILED(bank.fence->getCurrentValue(&completed)) || completed==UINT64_MAX)return s.fail("shared selection bank fence failed");
      if(completed<bank.signal)continue;
    }
    bank.tickets.clear();bank.readback_used=0;bank.encoder=commands;++bank.generation;selected=i;break;
  }
  if(selected==invalid_id)return s.fail("shared selection banks still have GPU or feedback consumers");
  auto& bank=s.banks[selected];
  if(bank.tickets.size()==s.config.tickets_per_frame || bytes>s.config.readback_bytes-bank.readback_used)
    return s.fail("shared selection frame feedback arena exhausted");
  auto ticket=std::make_shared<SharedSelectionTicket>();
  ticket->owner=owner;ticket->offset=bank.readback_used;ticket->slot=selected;ticket->generation=bank.generation;
  ticket->pages=pages;ticket->capacity=capacity;ticket->clusters=unsigned(topology.clusters.size());
  bank.readback_used+=bytes;bank.tickets.push_back(ticket);
  output={&bank.buffers,&s.pipelines,ticket};
  commands->globalBarrier();
  const auto upload=[&](rhi::IBuffer* buffer,const auto& values) {
    return values.empty() || SLANG_SUCCEEDED(commands->uploadBufferData(buffer,0,values.size()*sizeof(values[0]),values.data()));
  };
  if(!upload(bank.buffers.groups,topology.groups) || !upload(bank.buffers.clusters,topology.clusters) ||
      !upload(bank.buffers.group_pages,topology.pages) || !upload(bank.buffers.parents,topology.parents))
    return s.fail("shared selection topology upload failed");
  return true;
}
bool SelectionResources::submitted(const std::shared_ptr<SharedSelectionTicket>& ticket,rhi::IFence* fence,std::uint64_t value) {
  auto& s=*state_;
  if(!ticket || ticket->slot>=s.banks.size() || ticket->submitted || !fence || !value)
    return s.fail("invalid shared selection ticket submission");
  auto& bank=s.banks[ticket->slot];
  if(bank.generation!=ticket->generation)return s.fail("stale shared selection ticket generation");
  if(bank.encoder) {bank.fence=fence;bank.signal=value;bank.encoder=nullptr;}
  else if(bank.fence.get()!=fence || bank.signal!=value)return s.fail("shared selection frame submission differs between assets");
  ticket->submitted=true;return true;
}
bool SelectionResources::completed(const std::shared_ptr<SharedSelectionTicket>& ticket) const {
  auto& s=*state_;
  if(!ticket || !ticket->submitted || ticket->consumed || ticket->slot>=s.banks.size())return false;
  if(ticket->ready)return true;
  auto& bank=s.banks[ticket->slot];
  if(ticket->generation!=bank.generation)return s.fail("shared selection feedback generation reused early");
  return s.collect(bank) && ticket->ready;
}
}
