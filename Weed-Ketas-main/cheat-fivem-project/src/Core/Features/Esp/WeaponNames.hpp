#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

// Tabela hash -> nome, no formato do wiki da RAGE (wiki.rage.mp/wiki/Weapons):
// o id "weapon_pistol_mk2" vira "PISTOL_MK2".
//
// Nao da para tirar isso de CWeaponInfo::GetName(): aquele campo (+0x5F0)
// guarda um nome CURTO interno -- "PUMP" no lugar de "PUMPSHOTGUN", por
// exemplo -- entao o unico jeito de bater com o wiki e mapear pelo hash.

namespace Core::Features::WeaponNames
{
    inline const std::unordered_map<uint32_t, const char*>& Table()
    {
        static const std::unordered_map<uint32_t, const char*> M = {
            // --- Melee ---
            { 0x92A27487, "DAGGER" },            { 0x958A4A8F, "BAT" },
            { 0xF9E6AA4B, "BOTTLE" },            { 0x84BD7BFD, "CROWBAR" },
            { 0xA2719263, "UNARMED" },           { 0x8BB05FD7, "FLASHLIGHT" },
            { 0x440E4788, "GOLFCLUB" },          { 0x4E875F73, "HAMMER" },
            { 0xF9DCBF2D, "HATCHET" },           { 0xD8DF3C3C, "KNUCKLE" },
            { 0x99B507EA, "KNIFE" },             { 0xDD5DF8D9, "MACHETE" },
            { 0xDFE37640, "SWITCHBLADE" },       { 0x678B81B1, "NIGHTSTICK" },
            { 0x19044EE0, "WRENCH" },            { 0xCD274149, "BATTLEAXE" },
            { 0x94117305, "POOLCUE" },           { 0x3813FC08, "STONE_HATCHET" },
            { 0x6589186A, "CANDYCANE" },         { 0xDAC00025, "STUNROD" },

            // --- Handguns ---
            { 0x1B06D571, "PISTOL" },            { 0xBFE256D4, "PISTOL_MK2" },
            { 0x5EF9FEC4, "COMBATPISTOL" },      { 0x22D8FE39, "APPISTOL" },
            { 0x3656C8C1, "STUNGUN" },           { 0x99AEEB3B, "PISTOL50" },
            { 0xBFD21232, "SNSPISTOL" },         { 0x88374054, "SNSPISTOL_MK2" },
            { 0xD205520E, "HEAVYPISTOL" },       { 0x083839C4, "VINTAGEPISTOL" },
            { 0x47757124, "FLAREGUN" },          { 0xDC4DB296, "MARKSMANPISTOL" },
            { 0xC1B3C3D1, "REVOLVER" },          { 0xCB96392F, "REVOLVER_MK2" },
            { 0x97EA20B8, "DOUBLEACTION" },      { 0xAF3696A1, "RAYPISTOL" },
            { 0x2B5EF5EC, "CERAMICPISTOL" },     { 0x917F6C8C, "NAVYREVOLVER" },
            { 0x57A4368C, "GADGETPISTOL" },      { 0x45CD9CF3, "STUNGUN_MP" },
            { 0x1BC4FDB9, "PISTOLXM3" },

            // --- SMGs ---
            { 0x13532244, "MICROSMG" },          { 0x2BE6766B, "SMG" },
            { 0x78A97CD0, "SMG_MK2" },           { 0xEFE7E2DF, "ASSAULTSMG" },
            { 0x0A3D4D34, "COMBATPDW" },         { 0xDB1AA450, "MACHINEPISTOL" },
            { 0xBD248B55, "MINISMG" },           { 0x476BF155, "RAYCARBINE" },
            { 0x14E5AFD5, "TECPISTOL" },

            // --- Shotguns ---
            { 0x1D073A89, "PUMPSHOTGUN" },       { 0x555AF99A, "PUMPSHOTGUN_MK2" },
            { 0x7846A318, "SAWNOFFSHOTGUN" },    { 0xE284C527, "ASSAULTSHOTGUN" },
            { 0x9D61E50F, "BULLPUPSHOTGUN" },    { 0x3AABBBAA, "HEAVYSHOTGUN" },
            { 0xEF951FBB, "DBSHOTGUN" },         { 0x12E82D3D, "AUTOSHOTGUN" },
            { 0x05A96BA4, "COMBATSHOTGUN" },

            // --- Assault Rifles ---
            { 0xBFEFFF6D, "ASSAULTRIFLE" },      { 0x394F415C, "ASSAULTRIFLE_MK2" },
            { 0x83BF0278, "CARBINERIFLE" },      { 0xFAD1F1C9, "CARBINERIFLE_MK2" },
            { 0xAF113F99, "ADVANCEDRIFLE" },     { 0xC0A3098D, "SPECIALCARBINE" },
            { 0x969C3D67, "SPECIALCARBINE_MK2" },{ 0x7F229F94, "BULLPUPRIFLE" },
            { 0x84D6FAFD, "BULLPUPRIFLE_MK2" },  { 0x624FE830, "COMPACTRIFLE" },
            { 0x9D1F17E6, "MILITARYRIFLE" },     { 0xC78D71B4, "HEAVYRIFLE" },
            { 0xD1D5F52B, "TACTICALRIFLE" },

            // --- LMGs ---
            { 0x9D07F764, "MG" },                { 0x7FD62962, "COMBATMG" },
            { 0xDBBD7280, "COMBATMG_MK2" },      { 0x61012683, "GUSENBERG" },

            // --- Sniper Rifles ---
            { 0x05FC3C11, "SNIPERRIFLE" },       { 0x0C472FE2, "HEAVYSNIPER" },
            { 0x0A914799, "HEAVYSNIPER_MK2" },   { 0xC734385A, "MARKSMANRIFLE" },
            { 0x6A6C02E0, "MARKSMANRIFLE_MK2" }, { 0x6E7DDDEC, "PRECISIONRIFLE" },
            { 0xA89CB99E, "MUSKET" },

            // --- Heavy Weapons ---
            { 0xB1CA77B1, "RPG" },               { 0xA284510B, "GRENADELAUNCHER" },
            { 0x4DD2DC56, "GRENADELAUNCHER_SMOKE" }, { 0x42BF8A85, "MINIGUN" },
            { 0x7F7497E5, "FIREWORK" },          { 0x6D544C99, "RAILGUN" },
            { 0x63AB0442, "HOMINGLAUNCHER" },    { 0x0781FE4A, "COMPACTLAUNCHER" },
            { 0xB62D1F67, "RAYMINIGUN" },        { 0xDB26713A, "EMPLAUNCHER" },
            { 0xFEA23564, "RAILGUNXM3" },

            // --- Throwables ---
            { 0x93E220BD, "GRENADE" },           { 0xA0973D5E, "BZGAS" },
            { 0x24B17070, "MOLOTOV" },           { 0x2C3731D9, "STICKYBOMB" },
            { 0xAB564B93, "PROXMINE" },          { 0x0787F0BB, "SNOWBALL" },
            { 0xBA45E8B8, "PIPEBOMB" },          { 0x23C9F95C, "BALL" },
            { 0xFDBC8A50, "SMOKEGRENADE" },      { 0x497FACC3, "FLARE" },
            { 0xF7F1E25E, "ACIDPACKAGE" },

            // --- Miscellaneous ---
            { 0x34A67B97, "PETROLCAN" },         { 0xFBAB5776, "PARACHUTE" },
            { 0x060EC506, "FIREEXTINGUISHER" },  { 0xBA536372, "HAZARDCAN" },
            { 0x184140A1, "FERTILIZERCAN" },
        };
        return M;
    }

    // Devolve "" quando o hash nao esta na tabela, para o chamador poder
    // cair no nome curto que o jogo fornece.
    inline std::string FromHash(uint32_t Hash)
    {
        const auto& M = Table();
        auto It = M.find(Hash);
        return (It != M.end()) ? std::string(It->second) : std::string();
    }
}
