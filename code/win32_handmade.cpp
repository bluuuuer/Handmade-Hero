#include <windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Xinput.lib")
#pragma comment(lib, "dsound.lib")
// #pragma warning(disable:4311 4005)

#define internal static
#define local_persist static
#define global_variable static

#ifndef XUSE_MAX_COUNT
#define XUSER_MAX_COUNT 4
#endif

#define Pi32 3.1415926f

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef float real32;
typedef double real64;

struct win32_offscreen_buffer {
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    uint32 pitch;
    int bytesPerPixel = 4;
};

struct win32_window_dimension {
    int width;
    int height;
};

struct win32_sound_output {
    int samplesPerSecond;
    int toneHz;
    int toneVolume;
    uint32 runningSampleIndex;
    int wavePeriod;
    int bytesPerSample;
    int secondaryBufferSize;
    real32 tSine;
    int latencySampleCount;
};

// TODO: This is a global for now.
global_variable bool globalRunning;
global_variable win32_offscreen_buffer globalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER globalSecondaryBuffer;

// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// DirectSoundCreated
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void FillSoundBuffer(win32_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite) {
    VOID *region1;
    DWORD region1Size;
    VOID *region2;
    DWORD region2Size;
    if (SUCCEEDED(globalSecondaryBuffer->Lock(byteToLock, bytesToWrite,
                                                &region1, &region1Size,
                                                &region2, &region2Size,
                                                0))) {
        DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
        int16 * sampleOut = (int16 *)region1;
        for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++ sampleIndex) {
            real32 sineValue = sinf(soundOutput->tSine);
            int16 sampleValue = (int16)(sineValue * soundOutput->toneVolume);
            *sampleOut ++ = sampleValue;
            *sampleOut ++ = sampleValue;
            soundOutput->tSine += 2.0f * Pi32 * 1.0f / (real32)soundOutput->wavePeriod;
            ++soundOutput->runningSampleIndex;
        }
        DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
        sampleOut = (int16 *)region2;
        for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++ sampleIndex) {
            real32 sineValue = sinf(soundOutput->tSine);
            int16 sampleValue = (int16)(sineValue * soundOutput->toneVolume);
            *sampleOut ++ = sampleValue;
            *sampleOut ++ = sampleValue;
            soundOutput->tSine += 2.0f * Pi32 * 1.0f / (real32)soundOutput->wavePeriod;
            ++soundOutput->runningSampleIndex;
        }
        globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
    }
}

internal void LoadXInput(void) {
    HMODULE xInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!xInputLibrary) {
        xInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    }
    if (!xInputLibrary) {
        // TODO: Diagnostic
        xInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if (xInputLibrary) {
        XInputGetState = (x_input_get_state *)GetProcAddress(xInputLibrary, "XInputGetState");
        if (!XInputGetState) {
            XInputGetState = XInputGetStateStub;
        }
        XInputSetState = (x_input_set_state *)GetProcAddress(xInputLibrary, "XInputSetState");
        if (!XInputSetState) {
            XInputSetState = XInputSetStateStub;
        }
    } else {
        // TODO: Diagnostic
    }
}

internal void InitDSound(HWND windowHandler, int32 samplesPerSecond, int32 bufferSize) {
    // Load the library
    HMODULE dSoundLibrary = LoadLibraryA("dsound.dll");

    if (dSoundLibrary) {
        // Get a DirectSound object - cooperative
        direct_sound_create *directSoundCreate = (direct_sound_create *)GetProcAddress(dSoundLibrary, "DirectSoundCreate");

        IDirectSound *directSound;
        if (directSoundCreate && SUCCEEDED(DirectSoundCreate(0, &directSound, 0))) {
            WAVEFORMATEX waveFormat = {};
            waveFormat.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat.nChannels = 2;
            waveFormat.nSamplesPerSec = samplesPerSecond;
            waveFormat.wBitsPerSample = 16;                    
            waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;                    
            waveFormat.cbSize = 0;

            // Create a primary buffer
            if (SUCCEEDED(directSound->SetCooperativeLevel(windowHandler, DSSCL_PRIORITY))) {
                DSBUFFERDESC bufferDesc = {};
                bufferDesc.dwSize = sizeof(bufferDesc);
                bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
                // Create a primary buffer
                LPDIRECTSOUNDBUFFER primaryBuffer;
                if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDesc, &primaryBuffer, 0))) {
                    
                    if (SUCCEEDED(primaryBuffer->SetFormat(&waveFormat))) {
                        OutputDebugStringA("Primary buffer format was set.\n");
                    } else {
                        // TODO: Diagnostic
                    }
                } else {
                    // TODO: Diagnostic
                }
            } else {
                // TODO: Diagnostic
            }

            // Create a secondary buffer
            DSBUFFERDESC bufferDesc = {};
            bufferDesc.dwSize = sizeof(bufferDesc);
            bufferDesc.dwFlags = 0;
            bufferDesc.dwBufferBytes = bufferSize;
            bufferDesc.lpwfxFormat = &waveFormat;
            if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDesc, &globalSecondaryBuffer, 0))) {
                OutputDebugStringA("Secondary buffer format was set.\n");
            }
        } else {
            // TODO: Diagnostic
        }
    } else {
        // TODO: Diagnostic
    }
}

internal win32_window_dimension GetWindowDimension(HWND window) {
    win32_window_dimension result;
    RECT clientRect;
    GetClientRect(window, &clientRect);
    result.width = clientRect.right - clientRect.left;
    result.height = clientRect.bottom - clientRect.top;
    return result;
}

internal void RenderGradient(win32_offscreen_buffer buffer, int xOffset, int yOffset) {
    uint8 *row = (uint8 *)buffer.memory;
    for (int i = 0; i < buffer.height; i ++) {
        uint32 *pixel = (uint32 *)row;
        for (int j = 0; j < buffer.width; j ++) {
            // pixel in memory: 00 00 00 00
            uint8 blue = i + xOffset;
            uint8 green = j + yOffset;
            // pixel (32-bits): xx RR GG BB
            *pixel ++ = ((green << 8) | blue);
        }
        row += buffer.pitch;
    }
}

internal void ResizeDIBSection(win32_offscreen_buffer *buffer, int width, int height) {
    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;

    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize = buffer->width * buffer->height * buffer->bytesPerPixel;
    buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    buffer->pitch = (uint32)(width * buffer->bytesPerPixel);
}

internal void DisplayBufferInWindow(HDC deviceContext, 
                            int windowWidth, int windowHeight,
                            win32_window_dimension dimension, 
                            win32_offscreen_buffer *buffer) {
    StretchDIBits(deviceContext, 
                    0, 0, windowWidth, windowHeight,
                    0, 0, buffer->width, buffer->height,                   
                    buffer->memory, 
                    &buffer->info, 
                    DIB_RGB_COLORS, SRCCOPY);
}

internal LRESULT CALLBACK MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT result = 0;
    switch(message) {
        case WM_CREATE: {
            OutputDebugStringA("WM_CREATE\n");
        } break;

        case WM_ACTIVATEAPP: {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_DESTROY: {
            OutputDebugStringA("WM_DESTROY\n");
            // TODO: Handle this as an error.
            globalRunning = false;
        } break;

        case WM_CLOSE: {
            OutputDebugStringA("WM_CLOSE\n");
            // TODO: Handle this with a message to the user?
            globalRunning = false;
            // PostQuitMessage(0);
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            uint32 vkCode = wParam;
            bool wasDown = ((lParam & (1 << 30)) != 0);
            bool isDown = ((lParam & (1 << 31)) == 0);
            if (isDown != wasDown) {
                if (vkCode == 'W') {
                } else if (vkCode == 'A') {
                } else if (vkCode == 'S') {
                } else if (vkCode == 'D') {
                } else if (vkCode == 'Q') {
                } else if (vkCode == 'E') {
                } else if (vkCode == VK_UP) {
                } else if (vkCode == VK_DOWN) {
                } else if (vkCode == VK_LEFT) {
                } else if (vkCode == VK_RIGHT) {
                } else if (vkCode == VK_ESCAPE) {
                    // OutputDebugStringA("ESCAPE: ");
                    // if (isDown) {
                    //     OutputDebugStringA("IsDown");
                    // } 
                    // if (wasDown) {
                    //     OutputDebugStringA("WasDown");
                    // } 
                    // OutputDebugStringA("\n");
                } else if (vkCode == VK_SPACE) {
                }            
            }

            bool altKeyWasDown = ((lParam & (1 << 29)) != 0);
            if ((vkCode == VK_F4) && altKeyWasDown) {
                globalRunning = false;
            }
        }  break;

        case WM_SIZE: {
            OutputDebugStringA("WM_SIZE\n");
            RECT clientRect;
            win32_window_dimension dimension = GetWindowDimension(window);
            ResizeDIBSection(&globalBackbuffer, dimension.width, dimension.height);
            OutputDebugStringA("WM_SIZE\n");
        } break;

        case WM_PAINT: {
            OutputDebugStringA("WM_PAINT\n");
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);
			win32_window_dimension dimension = GetWindowDimension(window);
            DisplayBufferInWindow(deviceContext, dimension.width, dimension.height, dimension, &globalBackbuffer);
            EndPaint(window, &paint);
        } break;

        defalut: {
            // OutputDebugStringA("default\n");
            // result = DefWindowProc(window, message, wParam, lParam);
        } break;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prevInstacne, LPSTR commandLine, int showCode) {
    // MessageBox(0, TEXT("This is handmade hero."), TEXT("Handmade Hero"), MB_OK|MB_ICONINFORMATION);
    // std::cout << "0" << std::endl;
    LoadXInput();

    WNDCLASS windowClass = {};

    windowClass.style = CS_HREDRAW|CS_VREDRAW;
    windowClass.lpfnWndProc = MainWindowCallback;
    windowClass.hInstance = instance;
    // TODO: windowClass.hIcon = ;
    windowClass.lpszClassName = TEXT("HandmadeHeroWindowClass");

    if (RegisterClass(&windowClass)) {
        HWND windowHandle = CreateWindow(windowClass.lpszClassName, 
                                        TEXT("Handmade Hero"), 
                                        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
                                        CW_USEDEFAULT, 
                                        CW_USEDEFAULT, 
                                        CW_USEDEFAULT, 
                                        CW_USEDEFAULT, 
                                        0, 0, instance, 0);
        if (windowHandle) {
            // ShowWindow(windowHandle, SW_SHOWNORMAL);  // Do not need because WS_VISIBLE above
            HDC deviceContext = GetDC(windowHandle);

            int xOffset = 0;
            int yOffset = 0;

            win32_sound_output soundOutput = {};
            soundOutput.samplesPerSecond = 48000;
            soundOutput.toneHz = 512;
            soundOutput.toneVolume = 2000;
            soundOutput.runningSampleIndex = 0;
            soundOutput.wavePeriod = soundOutput.samplesPerSecond / soundOutput.toneHz;
            soundOutput.bytesPerSample = sizeof(int16) * 2;
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 15;
            InitDSound(windowHandle, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
            FillSoundBuffer(&soundOutput, 0, soundOutput.latencySampleCount * soundOutput.bytesPerSample);
            globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            globalRunning = true;
            LARGE_INTEGER frequencyResult;
            bool canCount = false;
            LARGE_INTEGER beginCount;
            LARGE_INTEGER endCount;
            int64 countFrequency;
            if (QueryPerformanceFrequency(&frequencyResult)) {
                countFrequency = frequencyResult.QuadPart;
                QueryPerformanceCounter(&beginCount);
                canCount = true;
                OutputDebugStringA("canCount");
            }
            // char buffer[256];
            // wsprintf(buffer, "canCount: %dms\n", canCount);
            while (globalRunning) {
                MSG message;
                while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        globalRunning = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }

                // Controller
                // TODO: Should we pool this more frequently
                for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++ controllerIndex) {
                    XINPUT_STATE controllerState;
                    if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS) {
                        // controllerState.dwPacketNumber
                        XINPUT_GAMEPAD *pad = &controllerState.Gamepad;
                        bool up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool right = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool start = (pad->wButtons & XINPUT_GAMEPAD_START);
                        bool back = (pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool leftShoulder = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool rightShoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool aButton = (pad->wButtons & XINPUT_GAMEPAD_A);
                        bool bButton = (pad->wButtons & XINPUT_GAMEPAD_B);
                        bool xButton = (pad->wButtons & XINPUT_GAMEPAD_X);
                        bool yButton = (pad->wButtons & XINPUT_GAMEPAD_Y);

                        int16_t stickX = pad->sThumbLX;
                        int16_t stickY = pad->sThumbLY;

                        xOffset += stickY / XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
                        yOffset -= stickX / XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;

                        soundOutput.toneHz = 512 + (int)(256.0f * ((real32)stickY / 30000.0f));
                        soundOutput.wavePeriod = soundOutput.samplesPerSecond / soundOutput.toneHz;
                    } else {
                        // The controller is not available
                    }
                }

                // XINPUT_VIBRATION vibration;
                // vibration.wLeftMotorSpeed = 300;
                // vibration.wRightMotorSpeed = 300;
                // XInputSetState(0, &vibration);

                // Render
                RenderGradient(globalBackbuffer, xOffset, yOffset);
                
                // Sound               
                DWORD playCursor;
                DWORD writeCursor;
                if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {
                    DWORD byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;
                    DWORD targetCursor = (playCursor + soundOutput.latencySampleCount * soundOutput.bytesPerSample) 
                                        % soundOutput.secondaryBufferSize;
                    DWORD bytesToWrite;
                    // TODO: Change this to using a lower latency offset from the playcursor when we actually astart haveing sound
                    if (byteToLock > targetCursor) {
                        bytesToWrite = soundOutput.secondaryBufferSize - byteToLock;
                        bytesToWrite += targetCursor;
                    } else {
                        bytesToWrite = targetCursor - byteToLock;
                    }

                    FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite);
                }

                win32_window_dimension dimension = GetWindowDimension(windowHandle);
                DisplayBufferInWindow(deviceContext, dimension.width, dimension.height, dimension, &globalBackbuffer); //, 0, 0, dimension.width, dimension.height);
                // ReleaseDC(windowHandle, deviceContext);

                // xOffset ++;

                // FPS 
                if (canCount) {
                    QueryPerformanceCounter(&endCount);
                    int64 countElapsed = endCount.QuadPart - beginCount.QuadPart;
                    int32 msPerFrame = (int32)(countElapsed * 1000 / countFrequency); // ms
                    int32 fps = countFrequency / countElapsed;

                    char buffer[256];
                    wsprintf(buffer, "Milliseconds/frame: %d ms / %d FPS\n", msPerFrame, fps);
                    OutputDebugStringA(buffer);

                    beginCount = endCount;
                }
            }
        } else {
            // TODO:
        }
    } else {
        // TODO:
    }

    return(0);
}
