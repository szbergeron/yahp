// #include "Stream.h"
#include "../../sampler_generated.h"
#include "PacketSerial.h"
#include "core_pins.h"
#include "fballoc.cpp"
#include "flatbuffers/buffer.h"
#include "flatbuffers/flatbuffer_builder.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"
#include "flatbuffers/vector.h"
#include "flatbuffers/verifier.h"
#include "types.cpp"
#include "utils.cpp"
#include "vector.cpp"

#include <cstddef>
#include <cstdint>

const uint_fast16_t MAX_COMMANDS_PER_POLL = 16;
const uint_fast16_t MAX_SAMPLES_PER_BATCH = 128;

struct wirerx {};

namespace tx {
void send_sample(sample_t sample) {}
} // namespace tx

namespace rx {
enum command_discr {
  AddSensor,
};
}

struct commands_t {
  //
};

void configure(const YAHP::Configuration *c) {}

void reprioritize(
    const flatbuffers::Vector<const YAHP::Reprioritize *> *priorities) {}

PacketSerial mps;

void on_packet(const uint8_t *buffer, size_t size) {
  flatbuffers::Verifier v(buffer, size);
  auto verified = v.VerifyBuffer<YAHP::ToTsy>();
  if (!verified) {
    eloop("failed to verify a ToTsy!");
  }

  auto pkt = flatbuffers::GetRoot<YAHP::ToTsy>(buffer);

  if (auto c = pkt->configure()) [[unlikely]] {
    configure(c);
  }

  reprioritize(pkt->priority());
}

const uint32_t FBBUF_SIZE = 4096;
static uint8_t FBBUF[FBBUF_SIZE];

uint64_t micros64()
{
    static uint32_t old = micros();
    static uint64_t highValue = 0;

    uint32_t now = micros();
    if (now < old)
    {
        highValue += 0x0000'0001'0000'0000;
    }
    old = now;
    return (highValue | now);
}

void send_packet(vector_t<sample_t, MAX_SAMPLES_PER_BATCH> samples) {
  // TODO: implement acks
  // auto acks = flatbuffers::Vector<uint64_t, size_t>();
  FlatbufferStaticAllocator_c a(FBBUF, FBBUF_SIZE);
  flatbuffers::FlatBufferBuilder builder(4096, &a);

  auto acks = builder.CreateVector<uint64_t>(nullptr, 0);

  auto now_long = micros64();
  uint32_t now = (now_long & UINT32_MAX);

  // the format sent over the wire is "cur_ts 

  auto tsysamples = builder.CreateVectorOfStructs<YAHP::TsySample>(
      samples.data(), samples.size());
}

void init_handlers() { mps.setPacketHandler(on_packet); }

void apply_pending_commands() {
  auto s = Serial;
  auto limit = 50;

  // flatbuffers::GetRoot<YAHP::ToTsy>(const void *buf);
}
