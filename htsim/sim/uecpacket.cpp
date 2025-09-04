#include "uecpacket.h"

PacketDB<UecDataPacket> UecDataPacket::_packetdb;
PacketDB<UecAckPacket> UecAckPacket::_packetdb;
PacketDB<UecNackPacket> UecNackPacket::_packetdb;
PacketDB<UecPullPacket> UecPullPacket::_packetdb;
PacketDB<UecRtsPacket> UecRtsPacket::_packetdb;

UecBasePacket::pull_quanta
UecBasePacket::quantize_floor(mem_b bytes) {
  return bytes >> PULL_SHIFT;
}

UecBasePacket::pull_quanta
UecBasePacket::quantize_ceil(mem_b bytes) {
  return (bytes + PULL_QUANTUM - 1) / PULL_QUANTUM;
}

mem_b
UecBasePacket::unquantize(UecBasePacket::pull_quanta credit_chunks) {
  return credit_chunks << PULL_SHIFT;
}
