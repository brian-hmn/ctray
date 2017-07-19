/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: $
   ======================================================================== */

#define WS_EX_LAYERED           0x00080000
#define ULW_COLORKEY            0x00000001
#define ULW_ALPHA               0x00000002
#define ULW_OPAQUE              0x00000004
#define AC_SRC_OVER                 0x00
#define AC_SRC_ALPHA                0x01

static char *OverlayWindowClassName = "CTrayOverlayWindowClassName";

struct overlay_window
{
    HWND Handle;

    int32x CurrentOverlayOpacity;
    int32x dCurrentOverlayOpacity;
    int32x AutoFadeOutTime;
    int32x FadingSpeed;
};

void
FadeIn(overlay_window *Window)
{
    if(Window)
    {
        Window->dCurrentOverlayOpacity = Window->FadingSpeed;
        Window->AutoFadeOutTime = 0;
        KillTimer(Window->Handle, 1);
        SetTimer(Window->Handle, 1, 1, 0);
    }
}

void
FadeOut(overlay_window *Window)
{
    if(Window)
    {
        Window->dCurrentOverlayOpacity = -Window->FadingSpeed;
        Window->AutoFadeOutTime = 0;
        KillTimer(Window->Handle, 1);
        SetTimer(Window->Handle, 1, 1, 0);
    }
}

void
FadeInOut(overlay_window *Window)
{
    if(Window)
    {
        Window->dCurrentOverlayOpacity = Window->FadingSpeed;
        Window->AutoFadeOutTime = 250;
        KillTimer(Window->Handle, 1);
        SetTimer(Window->Handle, 1, 1, 0);
    }
}

static void
SetOpacity(HWND Handle, int32x Opacity)
{
    BLENDFUNCTION Blend;
    Blend.BlendOp = AC_SRC_OVER;
    Blend.BlendFlags = 0;
    Blend.AlphaFormat = AC_SRC_ALPHA;
    Blend.SourceConstantAlpha = Opacity;

    UpdateLayeredWindow(Handle, 0, 0, 0, 0, 0, 0, &Blend, ULW_ALPHA);
}

void
UpdateOverlayImage(overlay_window *Window, HDC UpdateDC)
{
    if(Window)
    {
        RECT ClientRect;
        GetClientRect(Window->Handle, &ClientRect);
        
        BLENDFUNCTION Blend;
        Blend.BlendOp = AC_SRC_OVER;
        Blend.BlendFlags = 0;
        Blend.AlphaFormat = AC_SRC_ALPHA;
        Blend.SourceConstantAlpha = Window->CurrentOverlayOpacity;

        POINT Origin = {0};
        SIZE Size;
        Size.cx = ClientRect.right;
        Size.cy = ClientRect.bottom;
        HDC ScreenDC = GetDC(0);
        UpdateLayeredWindow(Window->Handle, ScreenDC, 0, &Size,
                            UpdateDC, &Origin, 0, &Blend, ULW_ALPHA);
        ReleaseDC(0, ScreenDC);
    }
}

LRESULT CALLBACK
OverlayWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    overlay_window *This =
        (overlay_window *)Win32WindowLocalThis(Window, Message, WParam, LParam);

    if(Message == WM_TIMER)
    {
        if(This->dCurrentOverlayOpacity > 0)
        {
            if(This->CurrentOverlayOpacity == 0)
            {
                ShowWindow(This->Handle, SW_SHOW);                
            }

            This->CurrentOverlayOpacity += This->dCurrentOverlayOpacity;
            if(This->CurrentOverlayOpacity > 255)
            {
                This->CurrentOverlayOpacity = 255;
                This->dCurrentOverlayOpacity = 0;
            }

            SetOpacity(This->Handle, This->CurrentOverlayOpacity);
        }
        else if(This->dCurrentOverlayOpacity < 0)
        {
            This->CurrentOverlayOpacity += This->dCurrentOverlayOpacity;
            if(This->CurrentOverlayOpacity < 0)
            {
                This->CurrentOverlayOpacity = 0;
                This->dCurrentOverlayOpacity = 0;
            }

            SetOpacity(This->Handle, This->CurrentOverlayOpacity);
        }
        else
        {
            if(This->CurrentOverlayOpacity)
            {
                if(This->AutoFadeOutTime > 0)
                {
                    --This->AutoFadeOutTime;
                    if(This->AutoFadeOutTime == 0)
                    {
                        This->dCurrentOverlayOpacity = -This->FadingSpeed;
                    }
                }
            }
            else
            {
                ShowWindow(Window, SW_HIDE);
                KillTimer(Window, 1);
            }
        }
    }

    if(Message == WM_DESTROY)
    {
        Deallocate(This);
    }
    
    return(DefWindowProc(Window, Message, WParam, LParam));
}

static bool32x
Win32RegisterWindowClass(char *Name, HINSTANCE HInstance, WNDPROC Callback, DWORD Style)
{
    WNDCLASSEX WindowClass = {sizeof(WindowClass)};
    WindowClass.style = Style;
    WindowClass.lpfnWndProc = Callback;
    WindowClass.hInstance = HInstance;
    WindowClass.hIcon = LoadIcon(WindowClass.hInstance,
                                 MAKEINTRESOURCE(101));
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    WindowClass.lpszClassName = Name;

    return(RegisterClassEx(&WindowClass) != 0);
}

static bool32x
Win32RegisterWindowClass(char *Name, HINSTANCE HInstance, WNDPROC Callback)
{
    return(Win32RegisterWindowClass(Name, HInstance, Callback, CS_HREDRAW | CS_VREDRAW));
}

overlay_window *
CreateOverlayWindow(int32x X, int32x Y, int32x Width, int32x Height)
{
    static bool32 Initialized = false;
    if(!Initialized)
    {
        Win32RegisterWindowClass(OverlayWindowClassName, GetModuleHandle(0), OverlayWindowCallback);
        Initialized = true;
    }

    overlay_window *This = Allocate(overlay_window);
    This->CurrentOverlayOpacity = 0;
    This->dCurrentOverlayOpacity = 0;
    This->AutoFadeOutTime = 0;
    This->FadingSpeed = 2;
    
    This->Handle = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT |
                                  WS_EX_TOPMOST | WS_EX_TOOLWINDOW, 
                                  OverlayWindowClassName, 0,
                                  WS_POPUP,
                                  X, Y, Width, Height,
                                  0, 0, GetModuleHandle(0), (LPVOID)This);

    return(This);
}

void
DestroyOverlayWindow(overlay_window *Window)
{
    if(Window)
    {
        DestroyWindow(Window->Handle);
    }
}

