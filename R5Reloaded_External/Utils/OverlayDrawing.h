#pragma once
#include "../ImGui/imgui.h"
#include <cmath>

// Pure drawing code. No process access or input-device operations occur here.
namespace visual
{
    struct Marker
    {
        ImVec2 top{};
        ImVec2 bottom{};
        float health = 0.0f;
        float maxHealth = 100.0f;
        ImU32 color = IM_COL32(255, 255, 255, 255);
    };

    struct DrawOptions
    {
        bool box = true;
        bool line = false;
        bool health = true;
    };

    struct DrawResult
    {
        unsigned boxes = 0;
        unsigned lines = 0;
        unsigned healthBars = 0;
        bool valid = false;
    };

    inline bool Finite(const ImVec2& value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    inline DrawResult DrawMarker(ImDrawList& draw, const Marker& marker,
                                 const ImVec2& viewport, const DrawOptions& options)
    {
        DrawResult result{};
        if (!Finite(marker.top) || !Finite(marker.bottom) || !Finite(viewport) ||
            viewport.x <= 0.0f || viewport.y <= 0.0f)
            return result;

        const float height = marker.bottom.y - marker.top.y;
        if (!std::isfinite(height) || height <= 0.5f || height > viewport.y * 8.0f)
            return result;
        const float halfWidth = height * 0.25f;
        const float center = marker.bottom.x;
        const ImVec2 minimum(center - halfWidth, marker.top.y);
        const ImVec2 maximum(center + halfWidth, marker.bottom.y);
        if (!Finite(minimum) || !Finite(maximum) || maximum.x < 0.0f ||
            minimum.x > viewport.x || maximum.y < 0.0f || minimum.y > viewport.y)
            return result;

        result.valid = true;
        if (options.box)
        {
            draw.AddRect(minimum, maximum, marker.color, 0.0f, 0, 1.0f);
            result.boxes = 1;
        }
        if (options.line)
        {
            draw.AddLine(ImVec2(viewport.x * 0.5f, viewport.y), marker.bottom,
                         marker.color, 1.0f);
            result.lines = 1;
        }
        if (options.health && std::isfinite(marker.health) &&
            std::isfinite(marker.maxHealth) && marker.maxHealth > 0.0f)
        {
            const float ratio = marker.health <= 0.0f ? 0.0f :
                (marker.health >= marker.maxHealth ? 1.0f : marker.health / marker.maxHealth);
            const float left = minimum.x - 6.0f;
            draw.AddRectFilled(ImVec2(left - 1.0f, minimum.y - 1.0f),
                               ImVec2(left + 4.0f, maximum.y + 1.0f),
                               IM_COL32(0, 0, 0, 200));
            if (ratio > 0.0f)
            {
                const int red = static_cast<int>((1.0f - ratio) * 255.0f);
                const int green = static_cast<int>(ratio * 255.0f);
                draw.AddRectFilled(ImVec2(left, maximum.y - height * ratio),
                                   ImVec2(left + 3.0f, maximum.y),
                                   IM_COL32(red, green, 25, 255));
            }
            result.healthBars = 1;
        }
        return result;
    }
}
