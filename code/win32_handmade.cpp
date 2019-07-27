#include <windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>

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

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int32_t int32;
typedef int16_t int16;

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

internal void LoadXInput(void) {
    HMODULE xInputLibrary = LoadLibraryA("xinput1_4.dll");
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

    // Note: Thank you to Chris Hecker of Spy Party fame for clrifying the deal with 
    // StretchDIBits and BitBlt!
    // No more DC for us.

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
                    // 0, 0, width, height,                   
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
            // GetClientRect(window, &clientRect);
            // int width = clientRect.right - clientRect.left;
            // int height = clientRect.bottom - clientRect.top;
            win32_window_dimension dimension = GetWindowDimension(window);
            ResizeDIBSection(&globalBackbuffer, dimension.width, dimension.height);
            OutputDebugStringA("WM_SIZE\n");
        } break;

        case WM_PAINT: {
            OutputDebugStringA("WM_PAINT\n");
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);
            // int x = paint.rcPaint.left;
            // int y = paint.rcPaint.top;
            // int height = paint.rcPaint.bottom - paint.rcPaint.top;
            // int width = paint.rcPaint.right - paint.rcPaint.left;
            // RECT clientRect;
            // GetClientRect(window, &clientRect);
			win32_window_dimension dimension = GetWindowDimension(window);
            DisplayBufferInWindow(deviceContext, dimension.width, dimension.height, dimension, &globalBackbuffer);
            EndPaint(window, &paint);
        } break;

        defalut: {
            // OutputDebugStringA("default\n");
            // result = DefWindowProc(window, message, wParam, lParam);
        } break;
    }
    return DefWindowProc(window, message, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prevInstacne, LPSTR commandLine, int showCode) {
    // MessageBox(0, TEXT("This is handmade hero."), TEXT("Handmade Hero"), MB_OK|MB_ICONINFORMATION);
    // std::cout << "0" << std::endl;
    LoadXInput();

    WNDCLASS windowClass = {};

    // windowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
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
            // ShowWindow(windowHandle, SW_SHOWNORMAL);
            HDC deviceContext = GetDC(windowHandle);

            int xOffset = 0;
            int yOffset = 0;
            int samplesPerSecond = 48000;
            int Hz = 256;
            uint32 runningSampleIndex = 0;
            int squareWaveCounter = 0;
            int squareWavePeriod = samplesPerSecond / Hz;
            int halfSquareWavePeriod = squareWavePeriod / 2;
            int bytesPerSample = sizeof(int16) * 2;
            int secondaryBufferSize = samplesPerSecond * bytesPerSample;

            InitDSound(windowHandle, samplesPerSecond, secondaryBufferSize);

            globalRunning = true;
            while (globalRunning) {
                MSG message;
                while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        globalRunning = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
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

                        int16_t strickX = pad->sThumbLX;
                        int16_t strickY = pad->sThumbLY;
                        
                        if (aButton) {
                            yOffset ++;
                        }
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
                
                // DirectSound output test
                DWORD playCursor;
                DWORD writeCursor;
                if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {
                    DWORD byteToLock = runningSampleIndex * bytesPerSample % secondaryBufferSize;
                    DWORD bytesToWrite;
                    if (byteToLock > playCursor) {
                        bytesToWrite = secondaryBufferSize - byteToLock;
                        bytesToWrite += playCursor;
                    } else {
                        bytesToWrite = playCursor - byteToLock;
                    }

                    VOID *region1;
                    DWORD region1Size;
                    VOID *region2;
                    DWORD region2Size;
                    if (SUCCEEDED(globalSecondaryBuffer->Lock(byteToLock, bytesToWrite,
                                                &region1, &region1Size,
                                                &region2, &region2Size,
                                                0))) {
                        int16 * sampleOut = (int16 *)region1;
                        DWORD region1SampleCount = region1Size / bytesPerSample;
                        for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++ sampleIndex) {
                            int16 sampleValue = (runningSampleIndex / halfSquareWavePeriod % 2) ? 16000 : -16000;
                            *sampleOut ++ = sampleValue;
                            *sampleOut ++ = sampleValue;
                        }
                        sampleOut = (int16 *)region2;
                        DWORD region2SampleCount = region2Size / bytesPerSample;
                        for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++ sampleIndex) {
                            int16 sampleValue = (runningSampleIndex / halfSquareWavePeriod % 2) ? 16000 : -16000;
                            *sampleOut ++ = sampleValue;
                            *sampleOut ++ = sampleValue;
                        }
                    }
                }

                win32_window_dimension dimension = GetWindowDimension(windowHandle);
                DisplayBufferInWindow(deviceContext, dimension.width, dimension.height, dimension, &globalBackbuffer); //, 0, 0, dimension.width, dimension.height);
                // ReleaseDC(windowHandle, deviceContext);

                xOffset ++;
            }
        } else {
            // TODO:
        }
    } else {
        // TODO:
    }

    return(0);
}
