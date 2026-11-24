#pragma once
#include <string>

enum class Accent {
    None,
    Razer, Logitech, SteelSeries, Corsair, HyperX, Rog, Zowie,
    PlayStation, Xbox, Galaxy, Apple, Jabra, Jbl, CoolerMaster, Steam, Redragon,
};

struct BrandColor { unsigned char r, g, b; };

struct BrandStyle {
    Accent accent;
    const wchar_t* name;
    BrandColor dark;
    BrandColor light;
};

const BrandStyle* AllBrands(int& count);
const BrandStyle& StyleOf(Accent a);
BrandColor AccentColor(Accent a, bool lightTaskbar);
