#include "overlay.h"
#include "ImGui\customed.h"
#include "Utils\NotSDK.h"
#include "Utils\OverlayDrawing.h"
#include "Mouse.h"
#include <Windows.h>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
    class OptionalMouseBackend
    {
    public:
        OptionalMouseBackend()
            : available_(mouseGHUB != nullptr && mouseGHUB->mouse_open() != FALSE)
        {
            if (!available_)
            {
                g.aimbot = false;
                g.fovCircle = false;
                std::cout << "[INFO] Optional mouse backend unavailable; visual-only mode.\n";
            }
        }
        ~OptionalMouseBackend()
        {
            if (available_ && mouseGHUB != nullptr) mouseGHUB->mouse_close();
        }
        OptionalMouseBackend(const OptionalMouseBackend&) = delete;
        OptionalMouseBackend& operator=(const OptionalMouseBackend&) = delete;
        bool available() const noexcept { return available_; }
    private:
        bool available_;
    };

    bool HasMouseBackend()
    {
        static OptionalMouseBackend backend;
        return backend.available();
    }

    void DisableUnavailableMouseFeatures()
    {
        if (!HasMouseBackend()) { g.aimbot = false; g.fovCircle = false; }
    }

    struct FrameStatus
    {
        unsigned players = 0, soldiers = 0, alive = 0, opponents = 0;
        unsigned projectionFailures = 0, markers = 0, boxes = 0, lines = 0, bars = 0;
        bool localFound = false;
        Memory::ReadReport reads{};
        const char* message = "Waiting for first data frame";
    };
    FrameStatus status;
    bool drawSelfTest = false;

    std::filesystem::path DiagnosticPath()
    {
        wchar_t buffer[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
            return std::filesystem::path(buffer).parent_path() / "BF1-diagnostics.txt";
        return "BF1-diagnostics.txt";
    }

    void FinishFrame()
    {
        status.reads = m.report;
        if (status.players == 0)
            status.message = status.reads.failures ? "Player lookup failed: see read error below" : "No player records returned";
        else if (!status.localFound)
            status.message = "Your player record was not identified; no team-dependent markers drawn";
        else if (!status.soldiers)
            status.message = "No soldier records returned";
        else if (!status.alive)
            status.message = "No valid living-player data";
        else if (!status.opponents)
            status.message = "No opposing players passed the data checks";
        else if (!status.markers)
            status.message = "No drawable markers: projection, pose, or screen bounds failed";
        else
            status.message = "Markers submitted to the renderer";

        // Current diagnostic snapshot, not an unbounded log. Never wait for console input.
        static double nextLog = 0.0;
        const double now = ImGui::GetTime();
        if (now >= nextLog)
        {
            nextLog = now + 2.0;
            std::ostringstream text;
            text << "BF1 drawing diagnostics v2\n"
                 << "Status: " << status.message << '\n'
                 << "Players=" << status.players << " Soldiers=" << status.soldiers
                 << " Alive=" << status.alive << " LocalFound=" << status.localFound
                 << " Opponents=" << status.opponents << '\n'
                 << "Markers=" << status.markers << " Boxes=" << status.boxes
                 << " Lines=" << status.lines << " HealthBars=" << status.bars
                 << " ProjectionFailures=" << status.projectionFailures << '\n'
                 << "ReadAttempts=" << status.reads.attempts << " ReadFailures=" << status.reads.failures
                 << " FirstWin32Error=" << status.reads.firstError << '\n'
                 << "FirstFailedStage=" << (status.reads.firstStage ? status.reads.firstStage : "none") << '\n'
                 << "Original game addresses are unchanged and unverified for the installed game build.\n"
                 << "Synthetic draw-test markers are not included in these counters.\n";
            std::ofstream output(DiagnosticPath(), std::ios::trunc);
            if (output) output << text.str();
            std::cout << text.str();
        }
    }

    void DrawStatus()
    {
        ImGui::TextUnformatted(g.ESP ? status.message : "ESP disabled");
        ImGui::Text("Players: %u | Soldiers: %u | Alive: %u | Local found: %s",
                    status.players, status.soldiers, status.alive, status.localFound ? "yes" : "no");
        ImGui::Text("Opponents: %u | Markers: %u | Projection failures: %u",
                    status.opponents, status.markers, status.projectionFailures);
        ImGui::Text("Drawn boxes: %u | Lines: %u | Health bars: %u",
                    status.boxes, status.lines, status.bars);
        ImGui::Text("Read failures: %u / %u | First Win32 error: %lu",
                    status.reads.failures, status.reads.attempts, status.reads.firstError);
        if (status.reads.firstStage)
            ImGui::Text("Failed stage: %s", status.reads.firstStage);
    }
}

// This field identifies the local player, not a requested opponent.
char LocalName[32] = "HQuserTR_61";
uint64_t LocalPlayer = 0, LocalSoldier = 0;
int LocalTeam = 0;
float LocalHealth = 0.0f;
Vector3 LocalPosition(0.0f, 0.0f, 0.0f);
int y_offset = 0;

void Overlay::m_Info()
{
    DisableUnavailableMouseFeatures();
    if (drawSelfTest)
    {
        const ImVec2 size = ImGui::GetIO().DisplaySize;
        visual::Marker sample;
        sample.top = ImVec2(size.x * 0.75f, size.y * 0.3f);
        sample.bottom = ImVec2(size.x * 0.75f, size.y * 0.65f);
        sample.health = 65.0f;
        sample.color = IM_COL32(255, 0, 255, 255);
        auto* draw = ImGui::GetBackgroundDrawList();
        visual::DrawMarker(*draw, sample, size, {true, true, true});
        draw->AddText(ImVec2(sample.top.x - 100.0f, sample.top.y - 22.0f), sample.color,
                      "SYNTHETIC DRAW TEST - NOT A PLAYER");
    }
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("##Info", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);
    ImGui::Text("BF1 drawing diagnostics v2 [%.1f FPS]", ImGui::GetIO().Framerate);
    if (!HasMouseBackend()) ImGui::TextUnformatted("Visual-only mode: optional mouse backend unavailable.");
    DrawStatus();
    time_t t = time(nullptr);
    struct tm local{};
    if (localtime_s(&local, &t) == 0)
        ImGui::Text("%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
    ImGui::End();
}

void Overlay::m_Menu()
{
    DisableUnavailableMouseFeatures();
    const bool mouseAvailable = HasMouseBackend();
    ImGui::Begin("PythonP BF 1 [ EXTERNAL ]", &ShowMenu, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Toggle("ESP", &g.ESP, g.ESP);
    ImGui::SeparatorText("Visual options");
    ImGui::Checkbox("Box", &g.vBox);
    ImGui::Checkbox("Line", &g.vLine);
    ImGui::Checkbox("HealthBar", &g.vHealth);
    ImGui::Checkbox("Distance", &g.vDistance);
    ImGui::Checkbox("Name", &g.vName);
    ImGui::InputText("Your player name", LocalName, sizeof(LocalName));
    ImGui::BeginDisabled(!mouseAvailable);
    ImGui::Checkbox("Aimbot", &g.aimbot);
    if (g.aimbot)
    {
        ImGui::SliderFloat("Speed", &g.speed, 0.0f, 1.0f);
        ImGui::SliderInt("FOV", &g.fov, 0, 500);
        ImGui::Checkbox("FOV Circle", &g.fovCircle);
    }
    else g.fovCircle = false;
    ImGui::EndDisabled();
    if (!mouseAvailable) ImGui::TextUnformatted("Mouse automation unavailable; normal mouse/menu input is unaffected.");
    ImGui::SeparatorText("Drawing diagnostics");
    ImGui::Checkbox("Synthetic drawing test (not real players)", &drawSelfTest);
    DrawStatus();
    ImGui::TextUnformatted("BF1-diagnostics.txt is written beside this executable when writable.");
    ImGui::End();
}

void Overlay::m_ESP()
{
    DisableUnavailableMouseFeatures();
    const bool canAutomateMouse = HasMouseBackend() && g.aimbot;
    status = {};
    m.ResetReadReport();
    LocalPlayer = 0; LocalSoldier = 0; LocalTeam = 0; LocalHealth = 0.0f;
    LocalPosition = Vector3(0.0f, 0.0f, 0.0f);

    struct Record
    {
        uint64_t player = 0, soldier = 0;
        char name[33]{};
        int team = 0;
        float health = 0.0f;
        Vector3 position{};
        bool nameValid = false, teamValid = false, healthValid = false, positionValid = false;
    };
    std::array<Record, 64> records{};

    // Collect first; identify the local record before filtering any other record.
    // The original lookup implementation and all game addresses remain unchanged.
    for (int index = 0; index < static_cast<int>(records.size()); ++index)
    {
        auto& record = records[index];
        m.readStage = "player lookup";
        record.player = GetPlayerById(index);
        if (!record.player) continue;
        ++status.players;
        m.readStage = "player name";
        record.nameValid = m.ReadBuffer(record.player + offset::PlayerName,
                                        record.name, sizeof(record.name) - 1);
        m.readStage = "team";
        record.team = m.Read<int>(record.player + offset::TeamID);
        record.teamValid = m.lastReadSucceeded;
        m.readStage = "soldier";
        record.soldier = m.Read<uint64_t>(record.player + offset::clientSoldierEntity);
        if (!m.lastReadSucceeded || !record.soldier) continue;
        ++status.soldiers;
        m.readStage = "health component";
        const auto component = m.Read<uint64_t>(record.soldier + offset::healthcomponent);
        if (m.lastReadSucceeded && component)
        {
            m.readStage = "health";
            record.health = m.Read<float>(component + 0x20);
            record.healthValid = m.lastReadSucceeded && std::isfinite(record.health);
        }
        m.readStage = "position";
        record.position = m.Read<Vector3>(record.soldier + offset::location);
        record.positionValid = m.lastReadSucceeded && std::isfinite(record.position.x) &&
            std::isfinite(record.position.y) && std::isfinite(record.position.z);
        if (record.healthValid && record.health > 0.0f) ++status.alive;
    }

    for (const auto& record : records)
    {
        if (record.player && record.nameValid && record.teamValid &&
            std::strcmp(record.name, LocalName) == 0)
        {
            status.localFound = true;
            LocalPlayer = record.player; LocalSoldier = record.soldier; LocalTeam = record.team;
            if (record.healthValid) LocalHealth = record.health;
            if (record.positionValid) LocalPosition = record.position;
            break;
        }
    }
    if (!status.localFound) { FinishFrame(); return; }

    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    auto* draw = ImGui::GetBackgroundDrawList();
    float shortestDistance = 999999.0f;
    int moveX = 0, moveY = 0;
    for (const auto& record : records)
    {
        if (!record.player || record.player == LocalPlayer || !record.soldier ||
            !record.teamValid || record.team == LocalTeam || !record.healthValid ||
            record.health <= 0.0f || !record.positionValid)
            continue;
        ++status.opponents;
        m.readStage = "bounding box pose";
        const auto bounds = GetAABB(record.soldier);
        if (!m.lastReadSucceeded || !std::isfinite(bounds.Max.y) || bounds.Max.y <= bounds.Min.y)
        { ++status.projectionFailures; continue; }
        Vector3 topWorld = record.position;
        Vector3 bottomWorld = record.position;
        topWorld.y += bounds.Max.y;
        bottomWorld.y += bounds.Min.y;
        Vector2 top, bottom;
        m.readStage = "top projection";
        if (!WorldToScreen(topWorld, top) || !m.lastReadSucceeded)
        { ++status.projectionFailures; continue; }
        m.readStage = "bottom projection";
        if (!WorldToScreen(bottomWorld, bottom) || !m.lastReadSucceeded)
        { ++status.projectionFailures; continue; }
        m.readStage = "visibility";
        const bool occluded = m.Read<bool>(record.soldier + offset::occluded);
        const bool visible = m.lastReadSucceeded && !occluded;
        const ImColor color = visible ? ESP_Visible : ESP_Normal;
        visual::Marker marker;
        marker.top = ImVec2(top.x, top.y);
        marker.bottom = ImVec2(bottom.x, bottom.y);
        marker.health = record.health;
        marker.color = color;
        const auto result = visual::DrawMarker(*draw, marker, viewport, {g.vBox, g.vLine, g.vHealth});
        if (!result.valid) { ++status.projectionFailures; continue; }
        ++status.markers;
        status.boxes += result.boxes; status.lines += result.lines; status.bars += result.healthBars;
        std::string label;
        if (g.vDistance)
        {
            const float distance = GetDistance(LocalPosition, record.position);
            if (std::isfinite(distance) && distance >= 0.0f && distance < 1000000.0f)
                label = std::to_string(static_cast<int>(distance)) + "m";
        }
        if (g.vName && record.nameValid) label += (label.empty() ? "" : " | ") + std::string(record.name);
        if (!label.empty())
        {
            const ImVec2 size = ImGui::CalcTextSize(label.c_str());
            draw->AddText(ImVec2(bottom.x - size.x * 0.5f, bottom.y), IM_COL32_WHITE, label.c_str());
        }
        // Preserve the existing optional output behavior; no new input backend is added.
        if (canAutomateMouse && visible && (GetAsyncKeyState(VK_RBUTTON) & 0x8000))
        {
            const float height = bottom.y - top.y;
            const float targetY = bottom.y - height * 0.75f + static_cast<float>(y_offset);
            const float dx = bottom.x - viewport.x * 0.5f;
            const float dy = targetY - viewport.y * 0.5f;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < shortestDistance && distance < static_cast<float>(g.fov))
            {
                shortestDistance = distance;
                moveX = static_cast<int>(dx * g.speed); moveY = static_cast<int>(dy * g.speed);
            }
        }
    }
    if (canAutomateMouse && g.fovCircle)
        draw->AddCircle(ImVec2(viewport.x * 0.5f, viewport.y * 0.5f),
                        static_cast<float>(g.fov), IM_COL32_WHITE, 64, 1.0f);
    if (canAutomateMouse && (moveX || moveY)) mouseGHUB->mouse_move(0, moveX, moveY, 0);
    FinishFrame();
}
