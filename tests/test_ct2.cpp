// 验证：MSVC 能否链接 ctranslate2 的 C++ 符号（本地 opus-mt 翻译）
#include <cstdio>
#include <string>
#include <vector>

#include "ctranslate2/translator.h"

int main() {
    printf("[INFO] ctranslate2 headers compiled\n");
    try {
        ctranslate2::Translator tr("models/opus_en_zh", ctranslate2::Device::CPU,
                                   ctranslate2::ComputeType::INT8);
        printf("[PASS] translator created\n");

        std::vector<std::vector<std::string>> batch;
        batch.push_back({"Hello", "world", "</s>"});
        auto results = tr.translate_batch(batch);
        printf("[PASS] translate ok: ");
        for (auto& t : results[0].output()) printf("%s ", t.c_str());
        printf("\n");
    } catch (const std::exception& e) {
        printf("[FAIL] exception: %s\n", e.what());
        return 1;
    }
    return 0;
}
