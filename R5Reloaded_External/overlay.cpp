#include "overlay.h"
#include "ImGui\customed.h"
#include "Utils\NotSDK.h"

#include "Mouse.h"
#include <Windows.h>
#include <iostream>

namespace
{
    // Probe the optional backend once. Ordinary mouse/menu input does not use it.
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
                std::cout << "[INFO] Optional G HUB input backend unavailable. "
                             "Visual overlay and menu remain enabled; automated mouse movement is disabled.\n";
            }
        }

        ~OptionalMouseBackend()
        {
            if (available_ && mouseGHUB != nullptr)
                mouseGHUB->mouse_close();
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
        if (!HasMouseBackend())
        {
            g.aimbot = false;
            g.fovCircle = false;
        }
    }
}

// Set your ID
char LocalName[32] = "BoranCanOzel";

// LocalPlayer Data
uint64_t LocalPlayer = 0;
uint64_t LocalSoldier = 0;
int LocalTeam = 0;
float LocalHealth = 0;
Vector3 LocalPosition = Vector3(0.f, 0.f, 0.f);

void Overlay::m_Info()
{
    DisableUnavailableMouseFeatures();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)GameRect.right, (float)GameRect.bottom));
    ImGui::Begin("##Info", (bool*)NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);

    ImGui::Text("PythonP Software [%.1f FPS]", ImGui::GetIO().Framerate);
    if (!HasMouseBackend())
        ImGui::TextUnformatted("Visual-only mode: optional mouse automation backend unavailable.");

    // Time
    time_t t = time(nullptr);
    struct tm nw;
    errno_t nTime = localtime_s(&nw, &t);
    ImGui::Text("%d:%d:%d", nw.tm_hour, nw.tm_min, nw.tm_sec);

    ImGui::End();
}

void Overlay::m_Menu()
{
    DisableUnavailableMouseFeatures();
    const bool mouseAvailable = HasMouseBackend();
    ImGui::Begin("PythonP BF 1 [ EXTERNAL ]", &ShowMenu, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Toggle("ESP", &g.ESP, g.ESP);

    ImGui::SeparatorText("ESP Options");

    ImGui::Checkbox("Box", &g.vBox);
    ImGui::Checkbox("Line", &g.vLine);
    ImGui::Checkbox("HealthBar", &g.vHealth);
    ImGui::Checkbox("Distance", &g.vDistance);
    ImGui::BeginDisabled(!mouseAvailable);
    ImGui::Checkbox("Aimbot", &g.aimbot);
    if (g.aimbot) {
        ImGui::SliderFloat("Speed", &g.speed, 0.0f, 1.0f);
        ImGui::SliderInt("FOV", &g.fov, 0, 500);
        ImGui::Checkbox("FOV Circle", &g.fovCircle);
    }
    else if (g.fovCircle) {
        g.fovCircle = false;
    }
    ImGui::EndDisabled();
    if (!mouseAvailable)
    {
        ImGui::TextUnformatted("Mouse automation unavailable (optional G HUB backend).");
        ImGui::TextUnformatted("Your normal mouse still controls this menu. No driver is required for visual-only mode.");
    }
    ImGui::Checkbox("Name", &g.vName);

    ImGui::NewLine();

    ImGui::InputText("SoldierName", LocalName, sizeof(LocalName));
    ImGui::End();
}

int y_offset = 0;
// Player Only
void Overlay::m_ESP()
{
    DisableUnavailableMouseFeatures();
    const bool canAutomateMouse = HasMouseBackend() && g.aimbot;

    // ImGui Window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)GameRect.right, (float)GameRect.bottom));
    ImGui::Begin("##ESP", (bool*)NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);

    float distance = 999999;
    float shortest_distance = 999999;
    int move_x = 0;
    int move_y = 0;

    float startAngle = 0.0f;
    float endAngle = 2 * 3.14159265f;
    float fovLineWidth = 1.0f;

    for (int i = 0; i < 64; i++)
    {
        // Get Player
        uint64_t Player = GetPlayerById(i);

        if (Player == NULL)
            continue;

        // Get Soldier
        uint64_t clientSoldierEntity = m.Read<uint64_t>(Player + 0x1D48);

        if (!clientSoldierEntity)
            continue;

        // Get PlayerName & Get LocalPlayer by name.
        char pName[32];
        ReadProcessMemory(m.pHandle, (void*)(Player + offset::PlayerName), &pName, sizeof(pName), NULL);

        // if PlayerName == Input Name
        if (strcmp(pName, LocalName) == 0)
        {
            LocalPlayer = Player;
            LocalSoldier = clientSoldierEntity;
            LocalTeam = m.Read<int>(Player + offset::TeamID);
            uint64_t HealthComponent = m.Read<uint64_t>(clientSoldierEntity + 0x1D0);
            LocalHealth = m.Read<float>(HealthComponent + 0x20);
            LocalPosition = m.Read<Vector3>(clientSoldierEntity + offset::location);
        }

        if (Player == LocalPlayer)
            continue;

        // Team
        int Team = m.Read<int>(Player + offset::TeamID);

        // Health
        uint64_t HealthComponent = m.Read<uint64_t>(clientSoldierEntity + 0x1D0);
        float Health = m.Read<float>(HealthComponent + 0x20);

        // Position
        Vector3 Position = m.Read<Vector3>(clientSoldierEntity + offset::location);

        // SomeChecks
        if (Team == LocalTeam)
            continue;
        else if (Health <= 0)
            continue;
        else if (Position == Vector3(0.f, 0.f, 0.f))
            continue;

        // WorldToScreen
        Vector2 ScreenPosition = Vector2(0.f, 0.f);
        WorldToScreen(Position, ScreenPosition);

        // Invalid Player
        if (ScreenPosition == Vector2(0.f, 0.f))
            continue;

        // VisCheck
        bool occluded = m.Read<bool>(clientSoldierEntity + offset::occluded);

        // Set ESPColor
        ImColor color = occluded ? ESP_Normal : ESP_Visible;

        // LINE
        if (g.vLine)
            DrawLine(ImVec2(GameRect.right / 2.f, GameRect.bottom), ImVec2(ScreenPosition.x, ScreenPosition.y), color, 1.f);

        // Box bounds
        Vector3 BoxTop = Position + GetAABB(clientSoldierEntity).Max;
        Vector3 BoxBottom = Position + GetAABB(clientSoldierEntity).Min;
        Vector2 vTop;
        Vector2 vBom;
        WorldToScreen(BoxTop, vTop);
        WorldToScreen(BoxBottom, vBom);

        float BoxMiddle = ScreenPosition.x;
        float Height = vBom.y - vTop.y;
        float Width = Height / 2.f;

        // Box
        if (g.vBox)
        {
            DrawLine(ImVec2(BoxMiddle + (Width / 2.f), vTop.y), ImVec2(BoxMiddle - (Width / 2.f), vTop.y), color, 1.f);
            DrawLine(ImVec2(BoxMiddle + (Width / 2.f), ScreenPosition.y), ImVec2(BoxMiddle - (Width / 2.f), ScreenPosition.y), color, 1.f);
            DrawLine(ImVec2(BoxMiddle + (Width / 2.f), vTop.y), ImVec2(BoxMiddle + (Width / 2.f), ScreenPosition.y), color, 1.f);
            DrawLine(ImVec2(BoxMiddle - (Width / 2.f), vTop.y), ImVec2(BoxMiddle - (Width / 2.f), ScreenPosition.y), color, 1.f);
        }
        // AIMBOT: unavailable backends must never receive movement commands.
        if (canAutomateMouse) {
            ImVec2 point2(ScreenPosition.x, ScreenPosition.y - Width - (Width / 2.f) + y_offset);

            ImGui::GetWindowDrawList()->AddCircle(point2, 1, color, 64, 1.0f);

            if (color == ESP_Visible && GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
                ImVec2 point1(GameRect.right / 2.f, GameRect.bottom / 2.f);

                distance = sqrt((point2.x - point1.x) * (point2.x - point1.x) + (point2.y - point1.y) * (point2.y - point1.y));
                if (distance < shortest_distance && g.fov > distance) {
                    shortest_distance = distance;
                    move_x = int(((ScreenPosition.x) - (GameRect.right / 2.f)) * g.speed);
                    move_y = int(((ScreenPosition.y - Width - (Width / 2.f) + y_offset) - (GameRect.bottom / 2.f)) * g.speed);
                }
            }

        }
        if (canAutomateMouse && g.fovCircle) {
            float time = ImGui::GetTime();

            // Calculate RGB values with smooth transitions
            int red = static_cast<int>((sinf(time) * 0.5f + 0.5f) * 255);
            int green = static_cast<int>((cosf(time) * 0.5f + 0.5f) * 255);
            int blue = static_cast<int>((sinf(time * 0.5f) * 0.5f + 0.5f) * 255);

            ImU32 rgb_color = IM_COL32(red, green, blue, 255);
            ImVec2 center(GameRect.right / 2.f, GameRect.bottom / 2.f);
            ImGui::GetWindowDrawList()->AddCircle(center, g.fov, rgb_color, 64, 1.0f);

        }
        if (g.vHealth)
            HealthBar(BoxMiddle - (Width / 2.f) - 4, ScreenPosition.y, 2.f, -Height, Health, 100.f);

        std::string vContext;

        if (g.vDistance)
        {
            float distance = GetDistance(LocalPosition, Position);
            vContext = std::to_string((int)distance) + "m";
        }

        if (g.vName)
            vContext = vContext + "|" + pName;

        ImVec2 textSize = ImGui::CalcTextSize(vContext.c_str());
        float TextCentor = textSize.x / 2.f;
        String(ImVec2(ScreenPosition.x - TextCentor, ScreenPosition.y), ImColor(1.f, 1.f, 1.f, 1.f), vContext.c_str());
    }
    if (canAutomateMouse && (move_x != 0 || move_y != 0)) {
        mouseGHUB->mouse_move(0, move_x, move_y, 0);
    }
    ImGui::End();
}
