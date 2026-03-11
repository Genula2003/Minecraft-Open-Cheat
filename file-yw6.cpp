#include <iostream>
#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <TlHelp32.h>
#include <vector>
#include <cmath>
#include <algorithm>

// Offsets for Minecraft Java Edition (1.20.x)
#define ENTITY_LIST_OFFSET 0x1F0B7C8
#define LOCAL_PLAYER_OFFSET 0x1F0B7C0
#define HEALTH_OFFSET 0x7C
#define POS_X_OFFSET 0x30
#define POS_Y_OFFSET 0x34
#define POS_Z_OFFSET 0x38
#define DIAMOND_ORE_ID 56

// Memory addresses
uintptr_t minecraftBase;
uintptr_t localPlayer;
HANDLE hProcess;

// Window and DirectX variables
HWND gameWindow;
IDirect3D9* d3d;
IDirect3DDevice9* device;
D3DPRESENT_PARAMETERS d3dpp;

// Function to find process ID
DWORD GetProcessId(const wchar_t* processName) {
    DWORD processId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);
        
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, processName) == 0) {
                    processId = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
    }
    
    CloseHandle(snapshot);
    return processId;
}

// Function to get module base address
uintptr_t GetModuleBaseAddress(DWORD processId, const wchar_t* moduleName) {
    uintptr_t moduleBaseAddress = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    
    if (snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W entry;
        entry.dwSize = sizeof(entry);
        
        if (Module32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szModule, moduleName) == 0) {
                    moduleBaseAddress = (uintptr_t)entry.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snapshot, &entry));
        }
    }
    
    CloseHandle(snapshot);
    return moduleBaseAddress;
}

// Function to read memory
template<typename T>
T ReadMemory(uintptr_t address) {
    T value = {};
    ReadProcessMemory(hProcess, (LPCVOID)address, &value, sizeof(T), NULL);
    return value;
}

// Function to write memory
template<typename T>
void WriteMemory(uintptr_t address, T value) {
    WriteProcessMemory(hProcess, (LPVOID)address, &value, sizeof(T), NULL);
}

// Function to get block ID at coordinates
int GetBlockId(int x, int y, int z) {
    // This is a simplified version - actual implementation would need
    // to navigate Minecraft's chunk and block data structures
    uintptr_t world = ReadMemory<uintptr_t>(localPlayer + 0x100);
    if (!world) return 0;
    
    uintptr_t chunkProvider = ReadMemory<uintptr_t>(world + 0x50);
    if (!chunkProvider) return 0;
    
    // Simplified block ID calculation
    int chunkX = x >> 4;
    int chunkZ = z >> 4;
    
    uintptr_t chunk = ReadMemory<uintptr_t>(chunkProvider + 0x20);
    if (!chunk) return 0;
    
    // This would need proper chunk navigation in real implementation
    // For demonstration purposes, return random diamond ore
    return (rand() % 1000 == 0) ? DIAMOND_ORE_ID : 1; // Stone
}

// Function to draw diamond indicator
void DrawDiamondIndicator(const D3DXVECTOR3& diamondPos) {
    if (!device) return;
    
    // Draw text indicator
    D3DCOLOR textColor = D3DCOLOR_ARGB(255, 0, 255, 255);
    
    // Calculate distance
    if (localPlayer) {
        float playerX = ReadMemory<float>(localPlayer + POS_X_OFFSET);
        float playerY = ReadMemory<float>(localPlayer + POS_Y_OFFSET);
        float playerZ = ReadMemory<float>(localPlayer + POS_Z_OFFSET);
        
        float distance = sqrt(pow(diamondPos.x - playerX, 2) + 
                             pow(diamondPos.y - playerY, 2) + 
                             pow(diamondPos.z - playerZ, 2));
        
        // Draw distance text (simplified - would need proper font rendering)
        char distanceText[64];
        sprintf_s(distanceText, "Diamond: %.1f blocks", distance);
        
        // Draw arrow pointing to diamond (simplified)
        D3DRECT arrowRect = {
            100, 100, 150, 120
        };
        device->Clear(1, &arrowRect, D3DCLEAR_TARGET, textColor, 0, 0);
    }
}

// ESP Rendering Function
void RenderESP() {
    if (!device) return;
    
    // Set up rendering state
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    
    // Declare transformation matrices outside the if block
    D3DXMATRIX view, proj, world;
    D3DVIEWPORT9 viewport;
    
    device->GetTransform(D3DTS_VIEW, &view);
    device->GetTransform(D3DTS_PROJECTION, &proj);
    device->GetViewport(&viewport);
    D3DXMatrixIdentity(&world);
    
    // Draw player ESP
    if (localPlayer) {
        float playerX = ReadMemory<float>(localPlayer + POS_X_OFFSET);
        float playerY = ReadMemory<float>(localPlayer + POS_Y_OFFSET);
        float playerZ = ReadMemory<float>(localPlayer + POS_Z_OFFSET);
        
        // Convert 3D coordinates to 2D screen coordinates
        D3DXVECTOR3 screenPos;
        D3DXVECTOR3 worldPos(playerX, playerY, playerZ);
        
        D3DXVec3Project(&screenPos, &worldPos, &viewport, &proj, &view, &world);
        
        if (screenPos.z > 0) {
            // Draw box around player
            D3DRECT box = {
                (int)screenPos.x - 20,
                (int)screenPos.y - 50,
                (int)screenPos.x + 20,
                (int)screenPos.y + 10
            };
            
            device->Clear(1, &box, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 255, 0), 0, 0);
        }
    }
    
    // Draw diamond ore ESP
    for (int x = -50; x < 50; x++) {
        for (int y = -50; y < 50; y++) {
            for (int z = -50; z < 50; z++) {
                int blockId = GetBlockId(x, y, z);
                if (blockId == DIAMOND_ORE_ID) {
                    D3DXVECTOR3 screenPos;
                    D3DXVECTOR3 worldPos(x, y, z);
                    
                    D3DXVec3Project(&screenPos, &worldPos, &viewport, &proj, &view, &world);
                    
                    if (screenPos.z > 0) {
                        D3DRECT diamondBox = {
                            (int)screenPos.x - 10,
                            (int)screenPos.y - 10,
                            (int)screenPos.x + 10,
                            (int)screenPos.y + 10
                        };
                        
                        device->Clear(1, &diamondBox, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 255, 255), 0, 0);
                    }
                }
            }
        }
    }
}

// Flying mod function
void EnableFly() {
    if (localPlayer) {
        // Set player flying state
        WriteMemory<bool>(localPlayer + 0x90, true);
        
        // Set player flight speed
        WriteMemory<float>(localPlayer + 0x94, 0.5f);
        
        // Set player on ground to prevent fall damage
        WriteMemory<bool>(localPlayer + 0x98, true);
    }
}

// Diamond locator function
void LocateDiamonds() {
    std::vector<D3DXVECTOR3> diamondPositions;
    
    for (int x = -100; x < 100; x++) {
        for (int y = -50; y < 50; y++) {
            for (int z = -100; z < 100; z++) {
                int blockId = GetBlockId(x, y, z);
                if (blockId == DIAMOND_ORE_ID) {
                    diamondPositions.push_back(D3DXVECTOR3(x, y, z));
                }
            }
        }
    }
    
    // Sort diamonds by distance to player
    if (localPlayer) {
         float playerX = ReadMemory<float>(localPlayer + POS_X_OFFSET);
         float playerY = ReadMemory<float>(localPlayer + POS_Y_OFFSET);
         float playerZ = ReadMemory<float>(localPlayer + POS_Z_OFFSET);

        std::sort(
            diamondPositions.begin(),
            diamondPositions.end(),
            [playerX, playerY, playerZ](const D3DXVECTOR3& a, const D3DXVECTOR3& b) {
                float distA = sqrt(pow(a.x - playerX, 2) + pow(a.y - playerY, 2) + pow(a.z - playerZ, 2));
                float distB = sqrt(pow(b.x - playerX, 2) + pow(b.y - playerY, 2) + pow(b.z - playerZ, 2));
                return distA < distB;
            }
        );
    }

    // Display closest diamond
    if (!diamondPositions.empty()) {
        D3DXVECTOR3 closestDiamond = diamondPositions[0];
        DrawDiamondIndicator(closestDiamond);
    }
}

// Menu rendering function
void RenderMenu() {
    if (!device) return;
    
    // Set up rendering state for menu
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    
    // Draw menu background
    D3DRECT menuRect = {
        50, 50, 350, 250
    };
    device->Clear(1, &menuRect, D3DCLEAR_TARGET, D3DCOLOR_ARGB(180, 0, 0, 0), 0, 0);
    
    // Draw menu border
    D3DRECT borderRect = {
        50, 50, 350, 250
    };
    device->Clear(1, &borderRect, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 255, 255, 255), 0, 0);
    
    // Draw menu options (simplified - would need proper font rendering)
    D3DRECT option1Rect = { 60, 70, 340, 90 };
    D3DRECT option2Rect = { 60, 100, 340, 120 };
    D3DRECT option3Rect = { 60, 130, 340, 150 };
    
    device->Clear(1, &option1Rect, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 100, 200, 100), 0, 0);
    device->Clear(1, &option2Rect, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 100, 200, 100), 0, 0);
    device->Clear(1, &option3Rect, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 100, 200, 100), 0, 0);
}

// Main cheat loop
void CheatLoop() {
    bool menuVisible = true;
    bool espEnabled = true;
    bool flyEnabled = false;
    bool diamondLocatorEnabled = false;
    
    while (!(GetAsyncKeyState(VK_END) & 1)) {
        // Toggle menu with INSERT key
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            menuVisible = !menuVisible;
        }

        device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
        if (SUCCEEDED(device->BeginScene())) {
            if (menuVisible) {
                RenderMenu();

                // Handle menu options
                if (GetAsyncKeyState('1') & 1) {
                    espEnabled = !espEnabled;
                }

                if (GetAsyncKeyState('2') & 1) {
                    flyEnabled = !flyEnabled;
                    if (flyEnabled) {
                        EnableFly();
                    }
                }

                if (GetAsyncKeyState('3') & 1) {
                    diamondLocatorEnabled = !diamondLocatorEnabled;
                }
            }

            // Apply cheats if enabled
            if (espEnabled) {
                RenderESP();
            }

            if (diamondLocatorEnabled) {
                LocateDiamonds();
            }

            device->EndScene();
        }
        device->Present(NULL, NULL, NULL, NULL);
        
        // Handle flying controls
        if (flyEnabled && localPlayer) {
            float flySpeed = 0.5f;
            float currentX = ReadMemory<float>(localPlayer + POS_X_OFFSET);
            float currentY = ReadMemory<float>(localPlayer + POS_Y_OFFSET);
            float currentZ = ReadMemory<float>(localPlayer + POS_Z_OFFSET);
            
            if (GetAsyncKeyState('W')) {
                WriteMemory<float>(localPlayer + POS_X_OFFSET, currentX + flySpeed);
            }
            if (GetAsyncKeyState('S')) {
                WriteMemory<float>(localPlayer + POS_X_OFFSET, currentX - flySpeed);
            }
            if (GetAsyncKeyState('A')) {
                WriteMemory<float>(localPlayer + POS_Z_OFFSET, currentZ - flySpeed);
            }
            if (GetAsyncKeyState('D')) {
                WriteMemory<float>(localPlayer + POS_Z_OFFSET, currentZ + flySpeed);
            }
            if (GetAsyncKeyState(VK_SPACE)) {
                WriteMemory<float>(localPlayer + POS_Y_OFFSET, currentY + flySpeed);
            }
            if (GetAsyncKeyState(VK_SHIFT)) {
                WriteMemory<float>(localPlayer + POS_Y_OFFSET, currentY - flySpeed);
            }
        }
        
        Sleep(1);
    }
}

// DirectX initialization and setup
bool InitializeDirectX() {
    // Find Minecraft window
    gameWindow = FindWindowW(L"LWJGL", nullptr);
    if (!gameWindow) {
        std::cout << "Minecraft window not found!" << std::endl;
        return false;
    }
    
    // Initialize DirectX
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        std::cout << "Failed to create Direct3D!" << std::endl;
        return false;
    }
    
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.hDeviceWindow = gameWindow;
    
    // Create device
    if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, gameWindow, 
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &device))) {
        std::cout << "Failed to create Direct3D device!" << std::endl;
        return false;
    }
    
    return true;
}

// Memory initialization
bool InitializeMemory() {
    DWORD processId = GetProcessId(L"javaw.exe");
    if (!processId) {
        std::cout << "Minecraft process not found!" << std::endl;
        return false;
    }
    
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) {
        std::cout << "Failed to open process!" << std::endl;
        return false;
    }

    minecraftBase = GetModuleBaseAddress(processId, L"minecraft.exe");
    if (!minecraftBase) {
        std::cout << "Failed to get Minecraft base address!" << std::endl;
        return false;
    }
    
    // Get local player
    localPlayer = ReadMemory<uintptr_t>(minecraftBase + LOCAL_PLAYER_OFFSET);
    if (!localPlayer) {
        std::cout << "Failed to get local player!" << std::endl;
        return false;
    }
    
    return true;
}

// Cleanup function
void Cleanup() {
    if (device) {
        device->Release();
        device = nullptr;
    }
    
    if (d3d) {
        d3d->Release();
        d3d = nullptr;
    }

    if (hProcess) {
        CloseHandle(hProcess);
        hProcess = nullptr;
    }
}

// Main function
int main() {
    std::cout << "Minecraft External Cheat" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "INSERT - Toggle Menu" << std::endl;
    std::cout << "1 - Toggle ESP" << std::endl;
    std::cout << "2 - Toggle Flying" << std::endl;
    std::cout << "3 - Toggle Diamond Locator" << std::endl;
    std::cout << "WASD - Move (when flying)" << std::endl;
    std::cout << "SPACE - Fly Up" << std::endl;
    std::cout << "SHIFT - Fly Down" << std::endl;
    std::cout << "========================" << std::endl;
    
    // Initialize DirectX
    if (!InitializeDirectX()) {
        std::cout << "Failed to initialize DirectX!" << std::endl;
        return 1;
    }
    
    // Initialize memory
    if (!InitializeMemory()) {
        std::cout << "Failed to initialize memory!" << std::endl;
        Cleanup();
        return 1;
    }
    
    // Start cheat loop
    CheatLoop();
    
    // Cleanup
    Cleanup();
    
    return 0;
}


// Developed by GENULA GEEMAL