#include <emscripten/bind.h>
#include "aubio.h"

using namespace emscripten;

struct NoteInfo {
  float midi;
  float velocity;
  float on;
  float off;
};

/**
 * input: Float32Array
 * output: {
 *   onset: number[],
 *   pitch: PitchFrame[],
 * }
 * config: {
 *   method: string,
 *   buf_size: number,
 *   hop_size: number,
 *   sample_rate: number,
 * }
 */
void Do(val input, val output, val options) {
  // 0. Parse options
  uint_t buf_size = 512;
  uint_t hop_size = 256;
  uint_t sample_rate = 44100;

  //val console = val::global("console");

  if (options["buf_size"].as<bool>()) {
    buf_size = options["buf_size"].as<uint_t>();
  }
  if (options["hop_size"].as<bool>()) {
    hop_size = options["hop_size"].as<uint_t>();
  }
  if (options["sample_rate"].as<bool>()) {
    sample_rate = options["sample_rate"].as<uint_t>();
  }


  // 1. Allocate buffer on WASM side, copy data in
  aubio_notes_t *aubio_notes = new_aubio_notes("default", buf_size, hop_size, sample_rate);
  fvec_t *aubio_output = new_fvec(3);

  uint_t input_length = input["length"].as<uint_t>();
  fvec_t *full_input = new_fvec(input_length);
  for (uint_t i = 0; i < input_length; ++i) {
    fvec_set_sample(full_input, input[i].as<float>(), i);
  }

  // 2. Apply streaming onset detection algorithm
  uint_t current_head = 0;
  fvec_t chunk;
  NoteInfo info {-1, -1, -1, -1}; 
  while (current_head + buf_size <= input_length) {
    // Begin: gnarly hack of fvec internals to get the right chunk of buffer 
    chunk.length = buf_size;
    chunk.data = full_input->data + current_head;
    // End: gnarly hack of fvec internals to get the right chunk of buffer 

    aubio_notes_do(aubio_notes, &chunk, aubio_output);

    if (aubio_output->data[2] != 0) {
      info.off = float(current_head) / sample_rate;
      output.call<void>("push", val(info));
      info = {-1, -1, -1, -1};
    }

    if (aubio_output->data[0] != 0) {
      info.midi = aubio_output->data[0];
      info.velocity = aubio_output->data[1];
      info.on = float(current_head) / sample_rate;
    }

    current_head += hop_size;
  } 

  // 3. cleanup
  del_fvec(full_input);
  del_aubio_notes(aubio_notes);
  del_fvec(aubio_output);
}

EMSCRIPTEN_BINDINGS(Onset) {
  function("notes_detect", &Do);

  value_object<NoteInfo>("NoteInfo")
    .field("midi", &NoteInfo::midi)
    .field("velocity", &NoteInfo::velocity)
    .field("on", &NoteInfo::on)
    .field("off", &NoteInfo::off)
    ;
}