// 本地 opus-mt 翻译（CTranslate2 + sentencepiece）
#include <cstring>
#include <string>
#include <vector>

#include "ctranslate2/translator.h"
#include "sentencepiece_processor.h"

#include "core/Util.h"
#include "mt/Translator.h"

namespace hs {

namespace {

struct LocalImpl {
    ctranslate2::Translator* translator = nullptr;
    sentencepiece::SentencePieceProcessor spSrc;
    sentencepiece::SentencePieceProcessor spTgt;
};

}  // namespace

LocalTranslator::LocalTranslator(StatusCallback onStatus)
    : onStatus_(std::move(onStatus)) {}

LocalTranslator::~LocalTranslator() { unload(); }

bool LocalTranslator::available() const {
    return fileExists(joinPath(opusModelDir(), "model.bin"));
}

void LocalTranslator::setStatusCallback(StatusCallback cb) { onStatus_ = std::move(cb); }

bool LocalTranslator::load(std::string* error) {
    if (loaded_ && impl_) return true;
    std::string dir = opusModelDir();
    if (!fileExists(joinPath(dir, "model.bin"))) {
        if (error) *error = "找不到本地翻译模型：" + dir;
        return false;
    }

    if (onStatus_) onStatus_("正在加载本地翻译模型...");
    try {
        auto* impl = new LocalImpl();
        impl->translator = new ctranslate2::Translator(dir, ctranslate2::Device::CPU,
                                                       ctranslate2::ComputeType::INT8);
        std::string srcSpm = joinPath(dir, "source.spm");
        std::string tgtSpm = joinPath(dir, "target.spm");
        auto st1 = impl->spSrc.Load(srcSpm);
        if (!st1.ok()) {
            if (error) *error = "加载 source.spm 失败: " + st1.ToString();
            delete impl->translator;
            delete impl;
            return false;
        }
        auto st2 = impl->spTgt.Load(tgtSpm);
        if (!st2.ok()) {
            if (error) *error = "加载 target.spm 失败: " + st2.ToString();
            delete impl->translator;
            delete impl;
            return false;
        }
        impl_ = impl;
        loaded_ = true;
        if (onStatus_) onStatus_("本地翻译已就绪");
        return true;
    } catch (const std::exception& e) {
        if (error) *error = std::string("本地翻译加载失败: ") + e.what();
        return false;
    }
}

void LocalTranslator::unload() {
    if (impl_) {
        auto* impl = static_cast<LocalImpl*>(impl_);
        delete impl->translator;
        delete impl;
        impl_ = nullptr;
    }
    loaded_ = false;
}

std::string LocalTranslator::translate(const std::string& text, std::string* error) {
    if (trim(text).empty()) return std::string();
    if (!load(error)) return std::string();

    auto* impl = static_cast<LocalImpl*>(impl_);
    try {
        std::vector<std::string> tokens;
        impl->spSrc.Encode(text, &tokens);
        tokens.push_back("</s>");   // Marian 必须补 EOS

        std::vector<std::vector<std::string>> batch{tokens};
        auto results = impl->translator->translate_batch(batch);

        if (results.empty() || results[0].output().empty()) return std::string();
        std::vector<std::string> out = results[0].output();
        if (!out.empty() && out[0] == "</s>") out.erase(out.begin());

        std::string decoded;
        impl->spTgt.Decode(out, &decoded);
        return trim(decoded);
    } catch (const std::exception& e) {
        if (error) *error = std::string("本地翻译失败: ") + e.what();
        return std::string();
    }
}

}  // namespace hs
