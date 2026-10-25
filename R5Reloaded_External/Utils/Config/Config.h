#pragma once

class cfg
{
public:
    // Visual
    bool ESP = true;

    bool vBox = true;
    bool vLine = false;
    bool vHealth = true;
    bool vDistance = true;
    bool vName = false;

    // The optional automation backend is never enabled automatically.
    bool aimbot = false;

    int fov = 200;
    bool fovCircle = false;
    float speed = 0.1f;
};

extern cfg g;
