#include "harness_steam.h"

#include <steam/steam_api.h>

void Harness_Steam_Init()
{
    SteamAPI_Init();
}

void Harness_Steam_Shutdown()
{
    SteamAPI_Shutdown();
}
