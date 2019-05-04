#include <emscripten/bind.h>
#include "aubio.h"
#include "fvec.h"

using namespace emscripten;

/**
 * input: Float32Array
 * output: Array
 * config: {
 *   method: string,
 *   buf_size: number,
 *   hop_size: number,
 *   sample_rate: number,
 * }
 */
void Do(val input, val output, val options) {
  // 0. Parse options
  std::string method = "default";
  uint_t buf_size = 2048;
  uint_t hop_size = 128;
  uint_t sample_rate = 44100;

  //val console = val::global("console");

  if (options["method"].as<bool>()) {
    method = options["method"].as<std::string>();
  }
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
  aubio_onset_t *aubio_onset = new_aubio_onset(method.c_str(), buf_size, hop_size, sample_rate);
  fvec_t *aubio_output = new_fvec(1);

  uint_t input_length = input["length"].as<uint_t>();
  fvec_t *full_input = new_fvec(input_length);
  for (uint_t i = 0; i < input_length; ++i) {
    fvec_set_sample(full_input, input[i].as<float>(), i);
  }


  // 2. Apply streaming onset detection algorithm
  uint_t current_head = 0;
  fvec_t chunk;
  while (current_head + hop_size <= input_length) {
    // Begin: gnarly hack of fvec internals to get the right chunk of buffer 
    chunk.length = buf_size;
    chunk.data = full_input->data + current_head;
    aubio_onset_do(aubio_onset, &chunk, aubio_output);
    // End: gnarly hack of fvec internals to get the right chunk of buffer 

    if (aubio_output->data[0]) {
      output.call<void>("push", val(aubio_onset_get_last_s(aubio_onset)));
    }

    current_head += hop_size;
  } 


  // 3. cleanup
  del_fvec(full_input);
  del_aubio_onset(aubio_onset);
  del_fvec(aubio_output);
}

EMSCRIPTEN_BINDINGS(Onset) {
  function("onset_detect", &Do);
}