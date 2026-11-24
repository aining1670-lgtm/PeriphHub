#include "brands.h"

namespace {
const BrandStyle kBrands[] = {
    { Accent::None,         L"",             { 255, 255, 255 }, {   0,   0,   0 } },
    { Accent::Razer,        L"Razer",        {  68, 214,  44 }, {  22, 140,  20 } },
    { Accent::Logitech,     L"Logitech",     {   0, 184, 252 }, {   0, 120, 180 } },
    { Accent::SteelSeries,  L"SteelSeries",  { 255,  82,   0 }, { 204,  65,   0 } },
    { Accent::Corsair,      L"Corsair",      { 245, 228,  26 }, { 150, 122,   0 } },
    { Accent::HyperX,       L"HyperX",       { 232,  35,  63 }, { 200,  16,  46 } },
    { Accent::Rog,          L"ROG",          { 255,  46,  77 }, { 208,   0,  42 } },
    { Accent::Zowie,        L"ZOWIE",        { 240,  60,  50 }, { 192,  32,  26 } },
    { Accent::PlayStation,  L"PlayStation",  {  31, 139, 255 }, {   0,  98, 196 } },
    { Accent::Xbox,         L"Xbox",         {  34, 168,  34 }, {  16, 124,  16 } },
    { Accent::Galaxy,       L"Samsung",      {  91, 123, 255 }, {  20,  40, 160 } },
    { Accent::Apple,        L"Apple",        { 199, 199, 204 }, { 110, 110, 115 } },
    { Accent::Jabra,        L"Jabra",        { 255, 200,   0 }, { 140, 106,   0 } },
    { Accent::Jbl,          L"JBL",          { 255,  61,   0 }, { 204,  52,   0 } },
    { Accent::CoolerMaster, L"Cooler Master",{ 157, 107, 255 }, { 106,  43, 217 } },
    { Accent::Steam,        L"Steam",        {  26, 159, 255 }, {  11, 111, 191 } },
    { Accent::Redragon,     L"Redragon",     { 230,   0,  18 }, { 192,   0,  15 } },
};
}  // namespace

const BrandStyle* AllBrands(int& count) {
    count = sizeof(kBrands) / sizeof(kBrands[0]);
    return kBrands;
}

const BrandStyle& StyleOf(Accent a) {
    for (const auto& b : kBrands)
        if (b.accent == a) return b;
    return kBrands[0];
}

BrandColor AccentColor(Accent a, bool lightTaskbar) {
    const BrandStyle& b = StyleOf(a);
    return lightTaskbar ? b.light : b.dark;
}
