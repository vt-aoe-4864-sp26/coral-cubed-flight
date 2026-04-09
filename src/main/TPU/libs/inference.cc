// inference.cc
#include "inference.h"

#include <cstdio>
#include "libs/base/filesystem.h"
#include "libs/base/led.h"
#include "libs/tensorflow/utils.h"
#include "libs/tpu/edgetpu_manager.h"
#include "libs/tpu/edgetpu_op.h"

namespace coral_cubed
{

    std::shared_ptr<coralmicro::EdgeTpuContext> InitEdgeTpu()
    {
        printf("Powering up and Initializing Edge TPU...\r\n");
        auto tpu_context = coralmicro::EdgeTpuManager::GetSingleton()->OpenDevice();
        if (!tpu_context)
        {
            printf("ERROR: Failed to get EdgeTpu context!\r\n");
            return nullptr;
        }
        printf("Edge TPU Initialized successfully.\r\n");
        return tpu_context;
    }

    ModelRunner::ModelRunner(const char *model_path, uint8_t *tensor_arena, size_t arena_size)
    {
        // 1. Read the model from LittleFS into our class-scoped vector
        if (!coralmicro::LfsReadFile(model_path, &model_data_))
        {
            printf("ERROR: Failed to load model from %s\r\n", model_path);
            return;
        }

        // 2. Validate the model schema
        model_ = tflite::GetModel(model_data_.data());
        if (model_->version() != TFLITE_SCHEMA_VERSION)
        {
            printf("ERROR: Model schema version not supported!\r\n");
            return;
        }

        // 3. Set up the Op Resolver (matching your object detection pipeline)
        resolver_.AddDequantize();
        resolver_.AddDetectionPostprocess();
        resolver_.AddCustom(coralmicro::kCustomOp, coralmicro::RegisterCustomOp());

        // 4. Build the interpreter using the arena passed in
        interpreter_ = new tflite::MicroInterpreter(
            model_, resolver_, tensor_arena, arena_size, &error_reporter_);

        if (interpreter_->AllocateTensors() != kTfLiteOk)
        {
            printf("ERROR: AllocateTensors() failed\r\n");
            return;
        }

        if (interpreter_->inputs().size() != 1)
        {
            printf("ERROR: Model must have only one input tensor\r\n");
            return;
        }

        is_valid_ = true;
        printf("Model loaded and interpreter initialized.\r\n");
    }

    ModelRunner::~ModelRunner()
    {
        if (interpreter_ != nullptr)
        {
            delete interpreter_;
        }
        // Note: We do NOT delete the tensor_arena here because we didn't allocate it.
        // It is statically allocated in main and passed to us.
    }

    bool ModelRunner::RunInferenceFromLfs(const char *image_path)
    {
        if (!is_valid_)
            return false;

        auto *input_tensor = interpreter_->input_tensor(0);

        // Load the image directly into the TF Lite input tensor's memory space
        if (!coralmicro::LfsReadFile(image_path, tflite::GetTensorData<uint8_t>(input_tensor), input_tensor->bytes))
        {
            printf("ERROR: Failed to load image from %s\r\n", image_path);
            return false;
        }

        if (interpreter_->Invoke() != kTfLiteOk)
        {
            printf("ERROR: Invoke() failed\r\n");
            return false;
        }

        return true;
    }

    bool ModelRunner::RunInferenceFromBuffer(const uint8_t *image_data, size_t image_size)
    {
        if (!is_valid_)
            return false;

        auto *input_tensor = interpreter_->input_tensor(0);

        if (image_size != input_tensor->bytes)
        {
            printf("ERROR: Buffer size (%zu) does not match expected tensor size (%zu)\r\n", image_size, input_tensor->bytes);
            return false;
        }

        // Copy the incoming image data into the input tensor
        memcpy(tflite::GetTensorData<uint8_t>(input_tensor), image_data, image_size);

        if (interpreter_->Invoke() != kTfLiteOk)
        {
            printf("ERROR: Invoke() failed\r\n");
            return false;
        }

        return true;
    }

    std::vector<coralmicro::tensorflow::Object> ModelRunner::GetDetectionResults(float threshold, int top_k)
    {
        if (!is_valid_)
            return {};
        return coralmicro::tensorflow::GetDetectionResults(interpreter_, threshold, top_k);
    }

} // namespace coral_cubed