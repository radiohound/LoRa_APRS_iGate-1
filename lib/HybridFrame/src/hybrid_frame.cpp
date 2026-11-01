#include "hybrid_frame.h"

#include <cstring>

extern "C" {
#include "hb.h"
}
#include "set1.h"

#if defined(ARDUINO) && !defined(UNIT_TEST)
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#define HYBRID_FRAME_WORKER 1
#endif

namespace HYBRID_FRAME {

    namespace {

        // The frame code keeps its working buffers on the stack (about 10 KB at the deepest, measured on an ESP32), more than the 8 KB of the Arduino
        // loop task. On the board the calls therefore run in a task of their own with a 16 KB stack, one call at a time.
        enum Op { OP_ENCODE, OP_DECODE };
        struct Job {
            Op op;
            const char* tnc2; size_t tnc2Len;
            const uint8_t* frame; size_t frameLen;
            uint8_t* out; size_t outCap;
            char* text; size_t textCap;
            hb_info_t info;
            long result;
        };

        void run(Job& j) {
            static bool listSet = false;
            if (!listSet) { hb_set_list(SET1, SET1_N); listSet = true; }
            if (j.op == OP_ENCODE) j.result = (long)hb_encode(j.tnc2, j.tnc2Len, nullptr, j.out, j.outCap, &j.info);
            else j.result = (long)hb_decode(j.frame, j.frameLen, j.text, j.textCap, &j.info);
        }

#ifdef HYBRID_FRAME_WORKER
        SemaphoreHandle_t lockMutex = nullptr, startSem = nullptr, doneSem = nullptr;
        TaskHandle_t workerHandle = nullptr;
        Job shared;

        void workerTask(void*) {
            for (;;) {
                xSemaphoreTake(startSem, portMAX_DELAY);
                run(shared);
                xSemaphoreGive(doneSem);
            }
        }

        bool call(Job& j) {
            if (lockMutex == nullptr) {                       // first use
                lockMutex = xSemaphoreCreateMutex();
                startSem = xSemaphoreCreateBinary();
                doneSem = xSemaphoreCreateBinary();
                if (lockMutex == nullptr || startSem == nullptr || doneSem == nullptr) {
                    lockMutex = nullptr;
                    return false;
                }
            }
            xSemaphoreTake(lockMutex, portMAX_DELAY);
            if (workerHandle == nullptr && xTaskCreate(workerTask, "lacf", 16384, nullptr, 1, &workerHandle) != pdPASS) {
                workerHandle = nullptr;
                xSemaphoreGive(lockMutex);
                return false;
            }
            shared = j;
            xSemaphoreGive(startSem);
            xSemaphoreTake(doneSem, portMAX_DELAY);
            j = shared;
            xSemaphoreGive(lockMutex);
            return true;
        }
#else
        bool call(Job& j) { run(j); return true; }
#endif

    }

    void shutdown() {
#ifdef HYBRID_FRAME_WORKER
        if (lockMutex == nullptr) return;
        xSemaphoreTake(lockMutex, portMAX_DELAY);
        if (workerHandle != nullptr) { vTaskDelete(workerHandle); workerHandle = nullptr; }
        xSemaphoreGive(lockMutex);
#endif
    }

    bool isHybrid(const uint8_t* data, size_t size) {
        return size >= 5 && data[0] == 0x3C && data[1] == 0xFF && data[2] == 0x02 && (data[3] & 3) >= 2;
    }

    bool pathOk(const std::string& tnc2) {
        return hb_path_ok(tnc2.data(), tnc2.size()) != 0;
    }

    bool encode(const std::string& tnc2, std::vector<uint8_t>& out) {
        Job j;
        std::memset(&j, 0, sizeof j);
        uint8_t frame[256];
        j.op = OP_ENCODE; j.tnc2 = tnc2.data(); j.tnc2Len = tnc2.size(); j.out = frame; j.outCap = sizeof frame;
        if (!call(j) || j.result <= 0 || j.result > 255) return false;
        out.assign(frame, frame + j.result);
        return true;
    }

    bool decode(const uint8_t* data, size_t size, std::string& tnc2, int* rxtTuples) {
        Job j;
        std::memset(&j, 0, sizeof j);
        std::vector<char> text(700);
        j.op = OP_DECODE; j.frame = data; j.frameLen = size; j.text = text.data(); j.textCap = text.size();
        if (!call(j) || j.result <= 0) return false;
        tnc2.assign(text.data(), (size_t)j.result);
        if (rxtTuples) *rxtTuples = j.info.has_rxt ? j.info.rxt.n : 0;
        return true;
    }

}
