#include "hook/EmittedDecoder.h"

#include <Windows.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <string>

int wmain(int argc, wchar_t** argv)
{
    if (argc != 4) {
        return 2;
    }
    HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, argv[1]);
    HANDLE run = OpenEventW(SYNCHRONIZE, FALSE, argv[2]);
    if (ready == nullptr || run == nullptr) {
        if (ready != nullptr) {
            CloseHandle(ready);
        }
        if (run != nullptr) {
            CloseHandle(run);
        }
        return 3;
    }

    ra3::test::EmittedDecoder decoder;
    if (!decoder.valid()) {
        CloseHandle(ready);
        CloseHandle(run);
        return 4;
    }
    {
        std::ofstream address(argv[3]);
        address << reinterpret_cast<std::uintptr_t>(decoder.address());
    }
    SetEvent(ready);
    if (WaitForSingleObject(run, 15000) != WAIT_OBJECT_0) {
        CloseHandle(ready);
        CloseHandle(run);
        return 5;
    }

    const std::array<std::uint8_t, 11> source = {
        0x10, 0xFB, 0x00, 0x00, 0x04, 0xE0, 'A', 'B', 'C', 'D', 0xFC};
    for (unsigned call = 0; call < 20; ++call) {
        std::array<std::uint8_t, 4> destination{};
        if (decoder.function()(destination.data(), source.data()) != 4
            || destination != std::array<std::uint8_t, 4>{'A', 'B', 'C', 'D'}) {
            CloseHandle(ready);
            CloseHandle(run);
            return 6;
        }
    }
    Sleep(250);
    CloseHandle(ready);
    CloseHandle(run);
    return 0;
}
