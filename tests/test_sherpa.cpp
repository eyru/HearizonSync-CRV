// 验证：MSVC 能否链接 sherpa-onnx C API 并跑通 SenseVoice 识别
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "sherpa-onnx/c-api/c-api.h"

int main(int argc, char** argv) {
    const char* model = "models/sv/model.int8.onnx";
    const char* tokens = "models/sv/tokens.txt";

    FILE* f = fopen(model, "rb");
    if (!f) {
        printf("[FAIL] model not found: %s\n", model);
        return 1;
    }
    fclose(f);
    f = fopen(tokens, "rb");
    if (!f) {
        printf("[FAIL] tokens not found: %s\n", tokens);
        return 1;
    }
    fclose(f);
    printf("[PASS] model files present\n");

    SherpaOnnxOfflineSenseVoiceModelConfig sv;
    memset(&sv, 0, sizeof(sv));
    sv.model = model;
    sv.language = "auto";
    sv.use_itn = 1;

    SherpaOnnxOfflineModelConfig mc;
    memset(&mc, 0, sizeof(mc));
    mc.sense_voice = sv;
    mc.tokens = tokens;
    mc.num_threads = 4;
    mc.debug = 0;
    mc.provider = "cpu";

    SherpaOnnxOfflineRecognizerConfig rc;
    memset(&rc, 0, sizeof(rc));
    rc.model_config = mc;

    const SherpaOnnxOfflineRecognizer* rec =
        SherpaOnnxCreateOfflineRecognizer(&rc);
    if (!rec) {
        printf("[FAIL] create recognizer\n");
        return 1;
    }
    printf("[PASS] recognizer created\n");

    // 1 秒 440Hz 正弦（不是语音，只验证推理链路不崩）
    std::vector<float> samples(16000);
    for (int i = 0; i < 16000; ++i) {
        samples[i] = 0.2f * sinf(2.0f * 3.14159265f * 440.0f * i / 16000.0f);
    }

    const SherpaOnnxOfflineStream* stream =
        SherpaOnnxCreateOfflineStream(rec);
    if (!stream) {
        printf("[FAIL] create stream\n");
        SherpaOnnxDestroyOfflineRecognizer(rec);
        return 1;
    }
    SherpaOnnxAcceptWaveformOffline(stream, 16000, samples.data(), 16000);
    SherpaOnnxDecodeOfflineStream(rec, stream);
    const SherpaOnnxOfflineRecognizerResult* r =
        SherpaOnnxGetOfflineStreamResult(stream);
    if (!r) {
        printf("[FAIL] get result\n");
        SherpaOnnxDestroyOfflineStream(stream);
        SherpaOnnxDestroyOfflineRecognizer(rec);
        return 1;
    }
    printf("[PASS] decode ok: text=%s lang=%s\n", r->text ? r->text : "(null)",
           r->lang ? r->lang : "(null)");

    SherpaOnnxDestroyOfflineRecognizerResult(r);
    SherpaOnnxDestroyOfflineStream(stream);
    SherpaOnnxDestroyOfflineRecognizer(rec);
    printf("\n==== sherpa-onnx C API 链接与推理验证通过 ====\n");
    return 0;
}
