#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <cstdio>
#include <cstdlib>

#pragma comment(lib, "winmm.lib")

// Key state enumeration
enum KeyState {
    KEY_RELEASED,
    KEY_PRESSED
};

// Send key press event
inline void pressKey(WORD vkCode) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vkCode;
    input.ki.dwFlags = 0;
    SendInput(1, &input, sizeof(INPUT));
}

// Send key release event
inline void releaseKey(WORD vkCode) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vkCode;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // Allocate console for printf output
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONIN$", "r", stdin);

    printf("=== Turntable to Key ===\n\n");

    // Scan for available game controllers
    printf("Scanning for game controllers...\n");
    UINT availableDevices[16];
    UINT deviceCount = 0;

    for (UINT i = 0; i < 16; i++) {
        JOYINFOEX testInfo = {};
        testInfo.dwSize = sizeof(JOYINFOEX);
        testInfo.dwFlags = JOY_RETURNALL;

        if (joyGetPosEx(i, &testInfo) == JOYERR_NOERROR) {
            JOYCAPS caps = {};
            if (joyGetDevCaps(i, &caps, sizeof(JOYCAPS)) == JOYERR_NOERROR) {
                printf("  [%u] %ls\n", deviceCount, caps.szPname);
                availableDevices[deviceCount] = i;
                deviceCount++;
            }
        }
    }

    if (deviceCount == 0) {
        printf("\nNo game controllers found!\n");
        printf("Please check:\n");
        printf("1. Device is connected\n");
        printf("2. Device shows up in Game Controllers\n");
        printf("3. Device drivers are installed\n");
        system("pause");
        return 1;
    }

    // Let user select device
    UINT selectedIndex = 0;
    if (deviceCount > 1) {
        printf("\nSelect a device (0-%u, default=0): ", deviceCount - 1);
        char input[32];
        fgets(input, sizeof(input), stdin);
        if (input[0] != '\n') {
            sscanf(input, "%u", &selectedIndex);
        }

        if (selectedIndex >= deviceCount) {
            printf("Invalid selection!\n");
            system("pause");
            return 1;
        }
    } else {
        printf("\nAuto-selecting the only available device.\n");
    }

    UINT joystickID = availableDevices[selectedIndex];
    printf("\n=== Device %u selected ===\n", joystickID);

    // Let user select axis
    printf("\nSelect axis for turntable:\n");
    printf("  [0] X-axis (default)\n");
    printf("  [1] Y-axis\n");
    printf("  [2] Z-axis\n");
    printf("  [3] R-axis (Rudder)\n");
    printf("  [4] U-axis\n");
    printf("  [5] V-axis\n");
    printf("Select axis (0-5, press Enter for X-axis): ");

    UINT axisSelection = 0;
    char axisInput[32];
    fgets(axisInput, sizeof(axisInput), stdin);
    if (axisInput[0] != '\n') {
        sscanf(axisInput, "%u", &axisSelection);
    }

    if (axisSelection > 5) {
        printf("Invalid axis selection! Using X-axis.\n");
        axisSelection = 0;
    }

    const char* axisNames[] = {"X", "Y", "Z", "R", "U", "V"};
    printf("\n=== %s-axis selected ===\n", axisNames[axisSelection]);

    // Let user configure sensitivity parameters
    printf("\nSensitivity settings (press Enter to use defaults):\n");

    printf("  Minimum ticks to activate (1-10, default=1): ");
    UINT minTicks = 1;
    char minTicksInput[32];
    fgets(minTicksInput, sizeof(minTicksInput), stdin);
    if (minTicksInput[0] != '\n') {
        sscanf(minTicksInput, "%u", &minTicks);
        if (minTicks < 1) minTicks = 1;
        if (minTicks > 10) minTicks = 10;
    }

    printf("  Stop threshold in ms (1-1000, default=10): ");
    UINT stopThreshold = 10;
    char stopInput[32];
    fgets(stopInput, sizeof(stopInput), stdin);
    if (stopInput[0] != '\n') {
        sscanf(stopInput, "%u", &stopThreshold);
        if (stopThreshold < 1) stopThreshold = 1;
        if (stopThreshold > 1000) stopThreshold = 1000;
    }

    printf("\nConfiguration:\n");
    printf("  Minimum ticks: %u\n", minTicks);
    printf("  Stop threshold: %u ms\n", stopThreshold);
    printf("\nKey mapping:\n");
    printf("  Clockwise (>):         A key\n");
    printf("  Counter-clockwise (<): S key\n");
    printf("\nMonitoring turntable...\n");
    printf("Press Ctrl+C to exit\n\n");

    // Create function pointer to appropriate axis getter
    DWORD (*getAxisValue)(const JOYINFOEX*) = nullptr;
    switch (axisSelection) {
        case 0: getAxisValue = [](const JOYINFOEX* j) { return j->dwXpos; }; break;
        case 1: getAxisValue = [](const JOYINFOEX* j) { return j->dwYpos; }; break;
        case 2: getAxisValue = [](const JOYINFOEX* j) { return j->dwZpos; }; break;
        case 3: getAxisValue = [](const JOYINFOEX* j) { return j->dwRpos; }; break;
        case 4: getAxisValue = [](const JOYINFOEX* j) { return j->dwUpos; }; break;
        case 5: getAxisValue = [](const JOYINFOEX* j) { return j->dwVpos; }; break;
    }

    // State variables (based on beatoraja's AnalogScratchAlgorithmVersion2)
    DWORD lastX = 0;
    bool initialized = false;
    bool scratchActive = false;        // Whether turntable is actively rotating
    bool rightMoveScratching = false;  // Current rotation direction (true=clockwise, false=counter-clockwise)
    int tickCounter = 0;               // Movement tick counter
    DWORD stopCounter = 0;             // Stop detection counter

    while (true) {
        JOYINFOEX joyInfo = {};
        joyInfo.dwSize = sizeof(JOYINFOEX);
        joyInfo.dwFlags = JOY_RETURNALL;

        MMRESULT result = joyGetPosEx(joystickID, &joyInfo);

        if (result == JOYERR_NOERROR) {
            // Get current axis value using pre-selected function
            DWORD currentX = getAxisValue(&joyInfo);

            // Initialize on first run
            if (!initialized) {
                lastX = currentX;
                initialized = true;
                continue;
            }

            // Calculate X-axis delta
            int delta = (int)currentX - (int)lastX;

            // Handle wraparound (0 to 65535)
            if (delta > 32768) {
                delta -= 65536;
            } else if (delta < -32768) {
                delta += 65536;
            }

            if (delta != 0) {
                // Movement detected
                bool nowRight = (delta > 0);

                if (scratchActive && (rightMoveScratching != nowRight)) {
                    // Direction change - beatoraja's key logic: immediately stop current direction
                    // Release current key
                    if (rightMoveScratching) {
                        releaseKey('A');
                        printf("[ ] Direction change - A released\n");
                    } else {
                        releaseKey('S');
                        printf("[ ] Direction change - S released\n");
                    }

                    scratchActive = false;
                    tickCounter = 0;
                    rightMoveScratching = nowRight;
                }
                else if (!scratchActive) {
                    // Starting from rest
                    tickCounter++;

                    if (tickCounter >= (int)minTicks) {
                        // Reached minimum ticks, activate turntable
                        scratchActive = true;
                        rightMoveScratching = nowRight;

                        if (nowRight) {
                            pressKey('A');
                            printf("[>] Clockwise - A pressed\n");
                        } else {
                            pressKey('S');
                            printf("[<] Counter-clockwise - S pressed\n");
                        }
                    }
                }

                stopCounter = 0;
                lastX = currentX;
            } else {
                // No movement
                stopCounter++;

                // Exceeded threshold, stop turntable
                if (stopCounter > stopThreshold && scratchActive) {
                    scratchActive = false;
                    tickCounter = 0;

                    if (rightMoveScratching) {
                        releaseKey('A');
                        printf("[ ] Stopped - A released\n");
                    } else {
                        releaseKey('S');
                        printf("[ ] Stopped - S released\n");
                    }
                }
            }
        }

        Sleep(1);
    }

    return 0;
}
