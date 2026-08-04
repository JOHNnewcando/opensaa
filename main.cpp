#include <windows.h>
#include <psapi.h>
#include <winver.h>
#include <cstdint>
#include <cstring>

namespace
{
uint8_t* g_patchAddresses[10]{};
int g_patchCount = 0;

bool PatchByte(uint8_t* addr, uint8_t from, uint8_t to)
{
    if (!addr || *addr != from)
        return false;
    DWORD oldProtect;
    if(!VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;
    *addr = to;
    VirtualProtect(addr, 1, oldProtect, &oldProtect);
    g_patchAddresses[g_patchCount++] = addr;
    return true;
}

bool Nop(uint8_t* addr, size_t size)
{
    DWORD oldProtect;
    if(!VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;
    memset(addr, 0x90, size);
    VirtualProtect(addr, size, oldProtect, &oldProtect);
    return true;
}

uint8_t* FindPattern(uint8_t* base, size_t size, const uint8_t* pattern, const char* mask)
{
    size_t len = strlen(mask);
    for(size_t i = 0; i < size - len; i++)
    {
        bool found = true;
        for(size_t j = 0; j < len; j++)
        {
            if(mask[j] != '?' && base[i + j] != pattern[j])
            {
                found = false;
                break;
            }
        }
        if(found)
            return base + i;
    }
    return nullptr;
}

bool PatchVehicleLimit(HMODULE samp)
{
    MODULEINFO info{};
    if(!GetModuleInformation(GetCurrentProcess(), samp, &info, sizeof(info)))
        return false;
    uint8_t pattern[] =
    {
        0x3D, 0x90, 0x01, 0x00, 0x00,
        0x0F, 0x8C, 0x32, 0x01, 0x00, 0x00,
        0x3D, 0x63, 0x02, 0x00, 0x00,
        0x0F, 0x8F, 0x27, 0x01, 0x00, 0x00
    };
    const char mask[] = "xxxxxxxxxxxxxxxxxxxxxx";
    uint8_t* address = FindPattern((uint8_t*)samp, info.SizeOfImage, pattern, mask);
    if(!address)
        return false;
    Nop(address + 5, 6);
    Nop(address + 16, 6);
    return true;
}

bool IsSampDL(HMODULE module)
{
    char path[MAX_PATH]{};
    if(!GetModuleFileNameA(module, path, MAX_PATH))
        return false;
    DWORD handle;
    DWORD size = GetFileVersionInfoSizeA(path, &handle);
    if(!size)
        return false;
    BYTE* data = new BYTE[size];
    bool result = false;
    if(GetFileVersionInfoA(path, 0, size, data))
    {
        char* version = nullptr;
        UINT len = 0;
        if(VerQueryValueA(data, "\\StringFileInfo\\040904b0\\ProductVersion", (LPVOID*)&version, &len))
        {
            if(version)
            {
                if(strstr(version, "DL"))
                    result = true;
            }
        }
    }
    delete[] data;
    return result;
}

bool PatchLegacy(HMODULE samp)
{
    uint8_t* base = (uint8_t*)samp;
    bool r1 = *(uint8_t*)(base + 0x129) == 0xF4;
    uint8_t* addr = base + (r1 ? 0x5F06C : 0x6240C);
    return PatchByte(addr, 0x74, 0xEB);
}

bool PatchDL(HMODULE samp)
{
    MODULEINFO info{};
    if(!GetModuleInformation(GetCurrentProcess(), samp, &info, sizeof(info)))
        return false;
    uint8_t pattern[] =
    {
        0x83, 0xC4, 0x04,
        0x50, 0xFF, 0x57, 0x0C,
        0x83, 0xF8, 0xFF,
        0x74
    };
    const char mask[] = "xxxxxxxxxx?";
    uint8_t* addr = FindPattern((uint8_t*)samp, info.SizeOfImage, pattern, mask);
    if(!addr)
        return false;
    return PatchByte(addr + 10, 0x74, 0xEB);
}

void ApplyPatch()
{
    HMODULE samp = nullptr;
    while(!(samp = GetModuleHandleA("samp.dll")))
    {
        Sleep(100);
    }
    Sleep(1500);
    PatchVehicleLimit(samp);
    if(IsSampDL(samp))
    {
        if(!PatchDL(samp))
            PatchLegacy(samp);
    }
    else
    {
        if(!PatchLegacy(samp))
            PatchDL(samp);
    }
}

void Restore()
{
    for(int i = 0; i < g_patchCount; i++)
    {
        DWORD old;
        VirtualProtect(g_patchAddresses[i], 1, PAGE_EXECUTE_READWRITE, &old);
        *g_patchAddresses[i] = 0x74;
        VirtualProtect(g_patchAddresses[i], 1, old, &old);
    }
}
}

DWORD WINAPI Thread(LPVOID)
{
    ApplyPatch();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if(reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, Thread, nullptr, 0, nullptr);
    }
    if(reason == DLL_PROCESS_DETACH)
    {
        Restore();
    }
    return TRUE;
}
