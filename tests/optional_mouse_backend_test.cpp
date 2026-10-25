// Headless tests of the production UI and the exact drawing helper used by it.
// Synthetic screen-space fixtures are not game-process or GPU integration tests.
#include "overlay.h"
#include "Mouse.h"
#include "Utils/Memory/Memory.h"
#include "Utils/OverlayDrawing.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{
    bool backendPresent = false;
    int openCalls = 0, closeCalls = 0, moveCalls = 0;
    MouseGHUB fakeMouse{};

    void Require(bool condition, const char* description)
    {
        if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
    }

    void VerifyCleanup()
    {
        if (closeCalls != (backendPresent ? 1 : 0))
        { std::cerr << "FAIL: optional backend cleanup count\n"; std::_Exit(2); }
        std::cout << "PASS: optional backend cleanup\n";
    }

    void ForceUnavailableSettings()
    {
        if (!backendPresent) { g.aimbot = true; g.fovCircle = true; }
    }

    void VerifySettings()
    {
        if (!backendPresent)
            Require(!g.aimbot && !g.fovCircle, "unavailable mouse features remain disabled");
        Require(g.ESP && g.vBox && g.vHealth && g.vDistance, "visual settings preserved");
    }

    visual::Marker ExampleMarker()
    {
        visual::Marker marker;
        marker.top = ImVec2(640.0f, 200.0f);
        marker.bottom = ImVec2(640.0f, 500.0f);
        marker.health = 65.0f;
        marker.color = IM_COL32(255, 0, 0, 255);
        return marker;
    }

    void VerifyFiniteVertices(const ImDrawList& draw)
    {
        for (int index = 0; index < draw.VtxBuffer.Size; ++index)
        {
            const auto& position = draw.VtxBuffer[index].pos;
            Require(std::isfinite(position.x) && std::isfinite(position.y),
                    "every generated vertex must be finite");
        }
    }

    void VerifyMarkerDrawing()
    {
        const ImVec2 viewport(1280.0f, 720.0f);
        unsigned cases = 0;
        // Prove each checkbox controls actual geometry, including all-off.
        for (unsigned mask = 0; mask < 8; ++mask)
        {
            ImGui::NewFrame();
            auto* draw = ImGui::GetBackgroundDrawList();
            const auto marker = ExampleMarker();
            const visual::DrawOptions options{(mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0};
            const int before = draw->VtxBuffer.Size;
            const auto result = visual::DrawMarker(*draw, marker, viewport, options);
            Require(result.valid, "valid sample geometry must be accepted");
            Require(result.boxes == (options.box ? 1u : 0u), "box checkbox controls box output");
            Require(result.lines == (options.line ? 1u : 0u), "line checkbox controls line output");
            Require(result.healthBars == (options.health ? 1u : 0u), "health checkbox controls bar output");
            Require((draw->VtxBuffer.Size > before) == (mask != 0), "enabled features generate vertices");
            VerifyFiniteVertices(*draw);
            ImGui::Render();
            if (mask != 0)
                Require(ImGui::GetDrawData()->TotalIdxCount > 0, "markers survive ImGui render finalization");
            ++cases;
        }
        // Invalid position, reversed box, outside viewport, huge box, bad viewport.
        for (unsigned test = 0; test < 6; ++test)
        {
            auto marker = ExampleMarker();
            auto size = viewport;
            switch (test)
            {
            case 0: marker.top.x = std::numeric_limits<float>::quiet_NaN(); break;
            case 1: marker.bottom.y = std::numeric_limits<float>::infinity(); break;
            case 2: marker.top.y = 600.0f; break;
            case 3: marker.top.x = marker.bottom.x = -1000.0f; break;
            case 4: marker.bottom.y = 10000.0f; break;
            case 5: size.x = 0.0f; break;
            }
            ImGui::NewFrame();
            auto* draw = ImGui::GetBackgroundDrawList();
            const int before = draw->VtxBuffer.Size;
            const auto result = visual::DrawMarker(*draw, marker, size, {true, true, true});
            Require(!result.valid && draw->VtxBuffer.Size == before,
                    "invalid geometry must not generate draw commands");
            ImGui::Render();
            ++cases;
        }
        // Health clamping and invalid-health handling must not emit invalid rectangles.
        for (unsigned test = 0; test < 4; ++test)
        {
            auto marker = ExampleMarker();
            if (test == 0) marker.health = -20.0f;
            if (test == 1) marker.health = 1000.0f;
            if (test == 2) marker.maxHealth = 0.0f;
            if (test == 3) marker.health = std::numeric_limits<float>::quiet_NaN();
            ImGui::NewFrame();
            auto* draw = ImGui::GetBackgroundDrawList();
            const auto result = visual::DrawMarker(*draw, marker, viewport, {false, false, true});
            Require(result.valid, "health errors must not invalidate screen geometry");
            Require(result.healthBars == (test < 2 ? 1u : 0u), "invalid health is omitted and valid health clamped");
            VerifyFiniteVertices(*draw);
            for (int index = 0; index < draw->VtxBuffer.Size; ++index)
            {
                const auto y = draw->VtxBuffer[index].pos.y;
                Require(y >= marker.top.y - 2.0f && y <= marker.bottom.y + 2.0f,
                        "health bar must remain within its intended vertical bounds");
            }
            ImGui::Render();
            ++cases;
        }
        Require(cases == 18, "all marker cases executed");
        std::cout << "PASS: 18 production marker tests: 8 option combinations, 6 invalid geometries, 4 health cases\n";
    }
}

// The test never links the real input device backend or opens another process.
MouseGHUB* mouseGHUB = &fakeMouse;
Memory m;
Overlay* v = nullptr;
BOOL MouseGHUB::mouse_open(void) { ++openCalls; return backendPresent ? TRUE : FALSE; }
void MouseGHUB::mouse_close(void) { ++closeCalls; }
void MouseGHUB::mouse_move(char, char, char, char) { ++moveCalls; }

int main(int argc, char** argv)
{
    Require(argc == 2, "expected missing or available argument");
    Require(std::strcmp(argv[1], "missing") == 0 || std::strcmp(argv[1], "available") == 0,
            "invalid test argument");
    backendPresent = std::strcmp(argv[1], "available") == 0;
    Require(!g.aimbot && !g.fovCircle, "automation defaults off");
    std::atexit(VerifyCleanup);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    VerifyMarkerDrawing();
    Overlay overlay;
    overlay.GameRect = {0, 0, 1280, 720};
    overlay.ShowMenu = true;
    v = &overlay;
    for (int frame = 0; frame < 180; ++frame)
    {
        ImGui::NewFrame();
        ForceUnavailableSettings(); overlay.m_Info(); VerifySettings();
        ForceUnavailableSettings(); overlay.m_Menu(); VerifySettings();
        ForceUnavailableSettings(); overlay.m_ESP(); VerifySettings();
        ImGui::Render();
        Require(openCalls == 1, "backend probed once");
        Require(moveCalls == 0, "no mouse commands in visual-only tests");
        Require(m.report.failures != 0, "missing process read failure recorded");
        Require(m.report.firstStage && std::strcmp(m.report.firstStage, "player lookup") == 0,
                "failure stage available instead of silent blank output");
        if (frame > 0)
            Require(ImGui::GetDrawData()->TotalVtxCount > 0, "UI still produces draw data");
    }
    ImGui::DestroyContext();
    v = nullptr;
    std::cout << "PASS: " << argv[1] << " backend; 180 UI frames; one probe; zero mouse commands; read errors reported\n";
    return 0;
}
