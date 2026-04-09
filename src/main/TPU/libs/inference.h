// inference.h
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>

#include "libs/tensorflow/detection.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_error_reporter.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_interpreter.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_mutable_op_resolver.h"
namespace coralmicro
{
    class EdgeTpuContext;
}
namespace coral_cubed
{

    // Initialize the Edge TPU Hardware. Call once on boot.
    std::shared_ptr<coralmicro::EdgeTpuContext> InitEdgeTpu();

    class ModelRunner
    {
    public:
        // Constructor: Loads the model from LittleFS and sets up the interpreter
        ModelRunner(const char *model_path, uint8_t *tensor_arena, size_t arena_size);

        // Destructor: Cleans up the interpreter
        ~ModelRunner();

        bool IsValid() const { return is_valid_; }

        // Phase 1: Demo using a static image loaded from LittleFS
        bool RunInferenceFromLfs(const char *image_path);

        // Phase 2: Future-proofed for TAB. Pass in an image buffer directly.
        bool RunInferenceFromBuffer(const uint8_t *image_data, size_t image_size);

        // Formats and returns the detection results
        std::vector<coralmicro::tensorflow::Object> GetDetectionResults(float threshold = 0.6f, int top_k = 3);

    private:
        bool is_valid_ = false;

        // The vector holding the model data MUST stay alive as long as the interpreter does
        std::vector<uint8_t> model_data_;

        const tflite::Model *model_ = nullptr;
        tflite::MicroInterpreter *interpreter_ = nullptr;
        tflite::MicroErrorReporter error_reporter_;

        // Resolver sized to 3 to accommodate Dequantize, DetectionPostprocess, and the TPU Custom Op
        tflite::MicroMutableOpResolver<3> resolver_;
    };

} // namespace coral_cubed