// configs.h
#pragma once
#include <cstdint>

// ========== Macros & Constants ========== //
#define VAR_CODE_ALIVE             ((uint8_t)0x00)
#define VAR_CODE_COM_EN            ((uint8_t)0x01)
#define VAR_CODE_PAY_EN            ((uint8_t)0x02)
#define VAR_CODE_RF_EN             ((uint8_t)0x03)
#define VAR_CODE_RF_TX             ((uint8_t)0x04)
#define VAR_CODE_RF_RX             ((uint8_t)0x05)
#define VAR_CODE_BLINK_CDH         ((uint8_t)0x06)
#define VAR_CODE_BLINK_COM         ((uint8_t)0x07)
#define VAR_CODE_CORAL_WAKE        ((uint8_t)0x08)
#define VAR_CODE_CORAL_CAM_ON      ((uint8_t)0x09)
#define VAR_CODE_CORAL_INFER_DENBY ((uint8_t)0x0a)
#define VAR_CODE_CORAL_INFER_BLK   ((uint8_t)0x0d)
#define VAR_CODE_CORAL_INFER_PIRATE ((uint8_t)0x11)
#define VAR_CODE_CORAL_INFER_REGENT ((uint8_t)0x12)
#define VAR_CODE_FETCH_RESULT      ((uint8_t)0x0E)
#define VAR_CODE_CLEAR_RESULTS     ((uint8_t)0x0F)
#define VAR_CODE_LIST_RESULTS      ((uint8_t)0x10)
#define VAR_CODE_RUN_DEMO          ((uint8_t)0x0b)
#define VAR_CODE_INFERENCE_RESULT  ((uint8_t)0x0c)
#define VAR_ENABLE                 ((uint8_t)0x01)
#define VAR_DISABLE                ((uint8_t)0x02)

// Removed namespace to ensure compatibility with all build stages
#define TPU_MODEL_PATH "/ssd_mobilenet_v2_face_quant_postprocess_edgetpu.tflite"
#define TPU_IMAGE_DENBY_PATH "/denby.rgb"
#define TPU_IMAGE_BLK_PATH "/blk.rgb"
#define TPU_IMAGE_PIRATE_PATH "/pirate_denby.rgb"
#define TPU_IMAGE_REGENT_PATH "/regent_denby.rgb"
#define TPU_TENSOR_ARENA_SIZE (8 * 1024 * 1024)
