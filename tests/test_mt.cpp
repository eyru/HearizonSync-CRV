#include "mt/Translator.h"
#include <cstdio>
int main() {
    hs::LocalTranslator lt;
    std::string err;
    if (!lt.load(&err)) { printf("[FAIL] load: %s\n", err.c_str()); return 1; }
    printf("[PASS] loaded\n");
    const char* tests[] = {
        "Can you hear the difference between these two tones?",
        "The sound level meter measures sound pressure level in decibels.",
        "Please sit down and listen carefully.",
    };
    for (auto t : tests) {
        std::string r = lt.translate(t, &err);
        printf("  %s\n    -> %s\n", t, r.c_str());
        if (r.empty()) { printf("[FAIL] empty\n"); return 1; }
    }
    printf("\n==== local MT ok ====\n");
    return 0;
}
