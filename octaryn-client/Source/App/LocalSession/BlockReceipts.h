#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace octaryn::client::app {
struct BlockReceiptCell { int32_t x{},y{},z{}; uint16_t block{}; };
struct BlockReceipt {
 uint64_t sequence{}, commandID{};
 bool accepted{};
 uint64_t revision{};
 std::vector<BlockReceiptCell> blocks;
};
struct BlockReceiptAck { int version{1}; std::string session; uint64_t sequence{}; };
struct BlockReceipts {
 int version{1};
 std::string session;
 std::vector<BlockReceipt> receipts;
};
}
