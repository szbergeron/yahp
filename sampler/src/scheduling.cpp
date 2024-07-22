#include "Array.h"
#include "keys.cpp"
#include "sample.cpp"

struct scheduler {
  Array<sample_request_t, SAMPLE_BATCH_TYPICAL_SIZE> get_batch() {
    Array<uint8_t, 256> boards;

    Array<sample_request_t, SAMPLE_BATCH_TYPICAL_SIZE> batch;

    // first, do a pass over keys to check (per board)
    // if any of them are immediately due for sampling
    //
    // this includes ones in the "active" state,
    // or that are outside of their "loose" window
    // for relaxed sampling
    for (auto &key : this->keys) {
      if(key.sample_due()) {
      }
    }

    // then, now that we have boards
    // that we're "already in the business of sampling",
    // check if we have any keys _within those boards_
    // that are within their "loose" window,
    // which saves us aggregated overhead from
    // switching the active board for relaxed ones too often
    // (lets us batch boards more)
    for (auto &key : this->keys) {
      uint8_t bid = key.board_id;
      for (auto bidi : boards) {
        if (bid == bidi) {
          if (key.relaxed_sample_due()) {
            batch.push_back(key.make_sample_request());
          }
        }
      }
    }
  }
};
