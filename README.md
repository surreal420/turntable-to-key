# Turntable to Key

A utility that converts beatmania controller turntable input to keyboard key presses.

Currently works on DAO FPS, FP7, etc.

## Features

- Detects and lists all connected game controllers
- Converts turntable rotation to keyboard input:
  - **Clockwise rotation** → A key
  - **Counter-clockwise rotation** → S key

## Requirements

- Windows 11 (or Windows 10)
- MSVC Build Tools for x64/x86
- Windows 11 SDK

## Building

### Prerequisites

Install the following tools:
1. **MSVC Build Tools** - Available from Visual Studio Installer or standalone Build Tools
2. **Windows 11 SDK** - Included with Visual Studio or Build Tools

### Compilation

Open **Developer Command Prompt for VS** and run:

```batch
cl /std:c++20 /DUNICODE /D_UNICODE main.cpp user32.lib
```

This will generate `main.exe` in the current directory.

## Usage

1. Connect your beatmania controller to your PC
2. Run `main.exe` and smash enter multiple times
3. Profit

## Algorithm

This tool implements a scratch detection algorithm based on **beatoraja's AnalogScratchAlgorithmVersion2**, with the following key features:

### 1. Rotation Detection

- Polls the joystick X-axis with 1ms interval
- Calculates delta between consecutive readings
- Handles wraparound at axis boundaries (0-65535 range)

### 2. Direction Change Handling

When the turntable direction changes (e.g., clockwise → counter-clockwise):
1. **Immediately release** the currently pressed key
2. Reset `scratchActive` flag to `false`
3. Reset tick counter to prevent rapid A-S-A-S toggling
4. Require minimum tick threshold before activating new direction

### 3. Key Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `MIN_TICKS` | 1 | Minimum movement ticks to activate scratch |
| `STOP_THRESHOLD` | 5 | Loop cycles without movement before releasing key |

### 4. Wraparound Handling

The joystick X-axis reports values in range [0, 65535]. When the turntable crosses the boundary:

```cpp
if (delta > 32768) {
    delta -= 65536;  // Wrapped from 65535 → 0
} else if (delta < -32768) {
    delta += 65536;  // Wrapped from 0 → 65535
}
```

## Customization

### Changing Key Mappings

Edit `main.cpp` to change the keys:

```cpp
if (nowRight) {
    pressKey('A');  // Change 'A' to your desired key
    printf("[>] Clockwise - A pressed\n");
} else {
    pressKey('S');  // Change 'S' to your desired key
    printf("[<] Counter-clockwise - S pressed\n");
}
```

### Adjusting Sensitivity

The program prompts for sensitivity settings at startup:

**Minimum ticks to activate:**
- **Default: 1** - Immediate response
- **Range: 1-10**
- **Lower values** (1-2): More responsive, may trigger on small movements
- **Higher values** (3-5): Less sensitive, requires deliberate rotation

**Stop threshold (ticks):**
- **Default: 5 ticks** - Quick release after stopping
- **Range: 1-20 ticks**
- **Lower values** (1-3 ticks): Keys release almost immediately when rotation stops
- **Higher values** (10-20 ticks): Keys stay pressed longer
- **Note:** Duration of each tick depends on system load (i.e. not exact 1ms)

## Credits

Algorithm based on **beatoraja's** AnalogScratchAlgorithmVersion2:
- Repository: [beatoraja](https://github.com/exch-bms2/beatoraja)
- File: `src/bms/player/beatoraja/input/BMControllerInputProcessor.java`
