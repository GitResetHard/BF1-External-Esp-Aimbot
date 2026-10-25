// Headless regression test for the production overlay implementation.
// No game process is opened; the input backend below is a test double.
#include "overlay.h"
#include "Mouse.h"
#include "Utils/Memory/Memory.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace
{
    bool backendPresent = false;
    int openCalls = 0;
    int closeCalls = 0;
    int moveCalls = 0;
    MouseGHUB fakeMouse{};

    void Require(bool condition, const char* description)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << description << '\n';
            std::exit(1);
        }
    }

    void VerifyCleanup()
    {
        const int expected = backendPresent ? 1 : 0;
        if (closeCalls != expected)
        {
            std::cerr << "FAIL: optional backend cleanup count\n";
            std::_Exit(2);
        }
        std::cout << "PASS: optional backend cleanup\n";
    }

    void ForceUnavailableSettings()
    {
        if (!backendPresent)
        {
            // Each render entry point must defend against stale/forced settings.
            g.aimbot = true;
            g.fovCircle = true;
        }
    }

    void VerifySettings()
    {
        if (!backendPresent)
            Require(!g.aimbot && !g.fovCircle,
                    "unavailable mouse features must remain disabled");
        Require(g.ESP && g.vBox && g.vHealth && g.vDistance,
                "visual feature settings must be preserved");
    }
}

// Deliberately do not link the real Mouse.cpp or Memory.cpp in this test.
MouseGHUB* mouseGHUB = &fakeMouse;
Memory m; // Zero-initialized null handle: no external process is accessed.
Overlay* v = nullptr;

BOOL MouseGHUB::mouse_open(void)
{
    ++openCalls;
    return backendPresent ? TRUE : FALSE;
}

void MouseGHUB::mouse_close(void)
{
    ++closeCalls;
}

void MouseGHUB::mouse_move(char, char, char, char)
{
    ++moveCalls;
}

int main(int argc, char** argv)
{
    Require(argc == 2, "expected missing or available argument");
    Require(std::strcmp(argv[1], "missing") == 0 ||
            std::strcmp(argv[1], "available") == 0, "invalid test argument");
    backendPresent = std::strcmp(argv[1], "available") == 0;
    Require(!g.aimbot && !g.fovCircle, "automation must default to off");
    std::atexit(VerifyCleanup); // Runs after the lazy session's destructor.

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    Overlay overlay;
    overlay.GameRect = { 0, 0, 1280, 720 };
    overlay.ShowMenu = true;
    v = &overlay;

    for (int frame = 0; frame < 180; ++frame)
    {
        ImGui::NewFrame();
        ForceUnavailableSettings();
        overlay.m_Info();
        VerifySettings();
        ForceUnavailableSettings();
        overlay.m_Menu();
        VerifySettings();
        ForceUnavailableSettings();
        overlay.m_ESP();
        VerifySettings();
        ImGui::Render();
        Require(openCalls == 1, "backend must be probed only once");
        Require(moveCalls == 0, "visual-only test must send no mouse output");
        if (frame > 0)
            Require(ImGui::GetDrawData()->TotalVtxCount > 0,
                    "UI must continue producing draw data");
    }

    ImGui::DestroyContext();
    v = nullptr;
    std::cout << "PASS: " << argv[1]
              << " backend; 180 production UI frames; one probe; zero mouse commands\n";
    return 0;
}
