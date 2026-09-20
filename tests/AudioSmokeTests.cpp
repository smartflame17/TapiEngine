#include "AudioTestAccess.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
void Write(std::ofstream& file, unsigned value, int bytes)
{
    for (int i = 0; i < bytes; ++i) file.put(static_cast<char>((value >> (i * 8)) & 0xff));
}
void CreateSilentWave(const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary);
    file.write("RIFF", 4); Write(file, 36 + 1600, 4); file.write("WAVEfmt ", 8);
    Write(file, 16, 4); Write(file, 1, 2); Write(file, 1, 2);
    Write(file, 8000, 4); Write(file, 16000, 4); Write(file, 2, 2); Write(file, 16, 2);
    file.write("data", 4); Write(file, 1600, 4);
    for (int i = 0; i < 800; ++i) Write(file, 0, 2);
    if (!file) throw std::runtime_error("Could not write smoke-test WAV");
}
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc != 2) throw std::runtime_error("Pass the test output directory");
        const auto path = std::filesystem::path(argv[1]) / L"효과.wav";
        CreateSilentWave(path);
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            auto audio = AudioTestAccess::Create();
            audio->SetMasterVolume(0.0f);
            const auto loop = audio->Play(path.u8string(), true);
            audio->SetVolume(loop, 0.5f);
            audio->SetPitch(loop, -0.5f);
            audio->SetPan(loop, 0.5f);
            audio->Pause(loop);
            audio->Resume(loop);
            audio->PlayOneShot(path.u8string());
            audio->Play(path.u8string());
            audio->Suspend();
            audio->Resume();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            audio->ResetDevice();
            audio->Stop(loop);
            audio->StopAll();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (AudioTestAccess::Failed(*audio)) throw std::runtime_error("Native audio worker failed");
        }
        std::cout << "DirectXTK audio smoke passed (silent WAV; 3 service lifetimes)\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
