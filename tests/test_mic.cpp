#include <cstdio>
#include <windows.h>
#include <mmdeviceapi.h>
#include "audio/AudioCapture.h"
using namespace hs;
int main() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    printf("CoInitialize hr=0x%08lX\n", (unsigned long)hr);
    auto devs = enumerateInputDevices();
    int usable = 0;
    for (const auto& d : devs) if (d.active) ++usable;
    printf("count=%d usable=%d\n", (int)devs.size(), usable);
    for (const auto& d : devs) {
        printf("[%d] active=%d loop=%d state=%s\n     name=%s\n", d.index,
               d.active ? 1 : 0, d.isLoopbackLike ? 1 : 0,
               d.stateText.c_str(), d.name.c_str());
    }
    CoUninitialize();
    return 0;
}
