/* ========================================================================
   $RCSfile: $
   $Date: 2007/09/23 05:44:28PM $
   $Revision: 22 $
   $Creator: Casey Muratori $
   $Notice: $
   ======================================================================== */

/* NOTE(casey):

   This program is totally hacky.  Don't expect it to work particularly well
   without a fair bit of care and attention.

   It's also super sloppy, like you pass printf arguments in from the settings
   file right now.  So DON'T EXPECT A WELL WRITTEN PROGRAM.  This is not one
   of those.  This is the opposite of that.

   YOU HAVE BEEN WARNED.

   - The Management
*/

#include <windows.h>
#include <shellapi.h>
#include <assert.h>
#include <stdio.h>

#include "ctray.h"

#define QAMinuteCount 15

static int TargetActive;
static file_time TargetHour;
static int DayNumber;
static display_strings DisplayStrings;

#define Allocate(type) (type *)malloc(sizeof(type))
#define Deallocate(Pointer) free(Pointer)

struct win32_dib_section
{
    HDC DrawDC;
    BITMAPINFOHEADER BitmapHeader;
    HBITMAP DrawBitmap, OldBitmap;

    int32x Width;
    int32x Height;

    int32x TotalBytesPerLine;
    int32x OverHang;
    int32x Stride;
    int32x BufferSize;

    uint8 *PixelBuffer;
    uint8 *TopOfFrame;
};

#include "ctray_overlay_window.h"

void *
Win32GetWindowLocalThis(HWND Window)
{
    void *This;

#if !defined(GWL_USERDATA)
    This = (void *)GetWindowLongPtr(Window, GWLP_USERDATA);
#else
    This = (void *)GetWindowLong(Window, GWL_USERDATA);
#endif

    return(This);
}

void
Win32SetWindowLocalThis(HWND Window, void *This)
{
#if !defined(GWL_USERDATA)
    SetWindowLongPtr(Window, GWLP_USERDATA, (LONG_PTR)This);
#else
    SetWindowLong(Window, GWL_USERDATA, (DWORD)This);
#endif
}

void *
Win32WindowLocalThis(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    void *This;

    if(Message == WM_CREATE)
    {
        CREATESTRUCT *CreateStruct = (CREATESTRUCT *)LParam;
        if(CreateStruct)
        {
            This = (void *)CreateStruct->lpCreateParams;
            Win32SetWindowLocalThis(Window, This);
        }
    }
    else
    {
        This = Win32GetWindowLocalThis(Window);
    }

    return(This);
}

void
Win32FreeDIBSection(win32_dib_section &This)
{
    if(This.DrawBitmap)
    {
        SelectObject(This.DrawDC, This.OldBitmap);
        DeleteObject(This.DrawBitmap);
        This.DrawBitmap = 0;
    }

    if(This.DrawDC)
    {
        DeleteDC(This.DrawDC);
        This.DrawDC = 0;
    }
}

bool32x
Win32IsInitialized(win32_dib_section &This)
{
    return(This.DrawDC != 0);
}

bool
Win32ResizeDIBSection(win32_dib_section &This, int32x Width, int32x Height)
{
    bool Result = false;

    if(!Win32IsInitialized(This))
    {
        // One-time only initialization
        This.BitmapHeader.biSize = sizeof(This.BitmapHeader);
        This.BitmapHeader.biPlanes = 1;
        This.BitmapHeader.biBitCount = 32;
        This.BitmapHeader.biCompression = BI_RGB;
        This.BitmapHeader.biSizeImage = 0;
        This.BitmapHeader.biClrUsed = 0;
        This.BitmapHeader.biClrImportant = 0;

        This.DrawBitmap = 0;
        This.OldBitmap = 0;

        This.DrawDC = CreateCompatibleDC(0);
    }

    if((Width > 0) && (Height > 0))
    {
        if(This.DrawBitmap)
        {
            SelectObject(This.DrawDC, This.OldBitmap);
            DeleteObject(This.DrawBitmap);

            This.OldBitmap = 0;
            This.DrawBitmap = 0;
        }

        This.BitmapHeader.biWidth = Width;
        This.BitmapHeader.biHeight = Height;

        This.PixelBuffer = 0;
        HDC ScreenDC = GetDC(0);
        This.DrawBitmap = CreateDIBSection(ScreenDC,
            (BITMAPINFO *)&This.BitmapHeader,
            DIB_RGB_COLORS,
            (void **)&This.PixelBuffer,
            0, 0);
        if(This.DrawBitmap)
        {
            This.OldBitmap = (HBITMAP)SelectObject(This.DrawDC, This.DrawBitmap);

            This.Width = Width;
            This.Height = Height;

            int32x SizeOfPixel = 4;
            int32x PadTo = 4;

            This.TotalBytesPerLine = (((This.Width * SizeOfPixel) + PadTo - 1) / PadTo) * PadTo;
            This.OverHang = This.TotalBytesPerLine - This.Width;
            This.Stride = -This.TotalBytesPerLine;
            This.BufferSize = This.Height * This.TotalBytesPerLine;
            This.TopOfFrame = ((uint8 *)This.PixelBuffer +
                    This.BufferSize - This.TotalBytesPerLine);

#if 0
            SetUInt32(This.Width * This.Height, 0x00FFFF00, This.PixelBuffer); 

            {for(int32x Y = 0;
                    Y < This.Height;
                    ++Y)
                {
                    {for(int32x X = 0;
                            X < This.Width;
                            ++X)
                        {
                            *(uint32 *)(This.TopOfFrame + Y*This.Stride + (SizeOfPixel*X)) =
                                0x00FF0F00;
                        }}
                }}            
#endif

            Result = true;
        }
        ReleaseDC(0, ScreenDC);

        assert(This.PixelBuffer);
    }

    return(Result);
}

void
Win32BlitWholeDIBToDC(win32_dib_section &This, HDC ToDC)
{
    if(Win32IsInitialized(This))
    {
        HDC FromDC = This.DrawDC;
        BitBlt(ToDC, 0, 0, This.Width, This.Height, FromDC, 0, 0, SRCCOPY);
    }
}

void
Win32BlitWholeDIBToWindow(win32_dib_section &This, HWND Window)
{
    if(Win32IsInitialized(This))
    {
        HDC ToDC = GetDC(Window);
        Win32BlitWholeDIBToDC(This, ToDC);
        ReleaseDC(Window, ToDC);
    }
}

void
Win32BlitWholeDIBToDCAtXY(win32_dib_section &This, HDC ToDC, int32x X, int32x Y)
{
    if(Win32IsInitialized(This))
    {
        HDC FromDC = This.DrawDC;
        BitBlt(ToDC, X, Y, This.Width, This.Height, FromDC, 0, 0, SRCCOPY);
    }
}

void
Win32BlitDIBToDC(win32_dib_section &This, int32x FromX, int32x FromY, int32x Width, int32x Height, HDC ToDC, int32x ToX, int32x ToY)
{
    if(Win32IsInitialized(This))
    {
        if(Width > This.Width)
        {
            Width = This.Width;
        }

        if(Height > This.Height)
        {
            Height = This.Height;
        }

        HDC FromDC = This.DrawDC;
        BitBlt(ToDC, ToX, ToY, Width, Height, FromDC, FromX, FromY, SRCCOPY);
    }
}

#include "ctray_overlay_window.cpp"

#define Win32TrayIconMessage (WM_USER + 1)
#define Win32SocketMessage (WM_USER + 2)

static char *TrayWindowClassName = "CTrayTrayWindowClassName";
static HWND TrayWindow;

static overlay_window *CountdownWindow;
static overlay_window *CornerWindow;
static win32_dib_section CountdownDIBSection;
static win32_dib_section CornerDIBSection;

typedef void hot_key_callback(void);
struct hot_key
{
    UINT Modifiers;
    UINT VKCode;
    hot_key_callback *Callback;
};

inline int32x
GetPixelsPerLine(HDC DC)
{
    TEXTMETRIC FontMetric;
    GetTextMetrics(DC, &FontMetric);
    return(FontMetric.tmHeight);
}

static void
DropShadowDraw(HDC DrawDC, RECT &TextRect, char *Text)
{
    TextRect.top += 1;
    TextRect.left += 1;
    SetTextColor(DrawDC, 0x00000000);
    DrawText(DrawDC, Text, strlen(Text), &TextRect, DT_LEFT | DT_NOPREFIX);

    TextRect.top -= 1;
    TextRect.left -= 1;
    SetTextColor(DrawDC, 0x00FFFFFF);
    DrawText(DrawDC, Text, strlen(Text), &TextRect, DT_LEFT | DT_NOPREFIX);
}

static HFONT TitleFont = 0;
static HFONT ArtistTrackFont = 0;
static HFONT StatusFont = 0;
static HFONT TinyFont = 0;

static void
GetFonts(void)
{
    if(!TitleFont)
    {
        TitleFont = CreateFont(70, 0, 0, 0,
            FW_NORMAL,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH|FF_DONTCARE, "Arial");

        ArtistTrackFont = CreateFont(44, 0, 0, 0,
            FW_NORMAL,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH|FF_DONTCARE, "Arial");

        StatusFont = CreateFont(32, 0, 0, 0,
            FW_NORMAL,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH|FF_DONTCARE, "Arial");

        TinyFont = CreateFont(16, 0, 0, 0,
            FW_NORMAL,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH|FF_DONTCARE, "Arial");
    }
}


uint32 const SpecialPixel = 0xFF000000;

static void
PreClear(win32_dib_section &DIBSection)
{
    uint8 *Line = DIBSection.TopOfFrame;
    {for(int32x Y = 0;
            Y < DIBSection.Height;
            ++Y)
        {
            uint32 *Pixel = (uint32 *)Line;
            {for(int32x X = 0;
                    X < DIBSection.Width;
                    ++X)
                {
                    *Pixel++ = SpecialPixel;
                }}
            Line += DIBSection.Stride;
        }}
}

static void
PostClear(win32_dib_section &DIBSection, bool32 Gradient)
{
    uint8 *Line = DIBSection.TopOfFrame;
    {for(int32x Y = 0;
            Y < DIBSection.Height;
            ++Y)
        {
            uint32 *Pixel = (uint32 *)Line;
            {for(int32x X = 0;
                    X < DIBSection.Width;
                    ++X)
                {
                    if(*Pixel == SpecialPixel)
                    {
                        int32x Alpha = 255;
                        if(Gradient)
                        {
                            Alpha = 255 - (X * 255 / DIBSection.Width);
                        }
                        *Pixel++ = Alpha << 24;
                    }
                    else
                    {
                        *Pixel++ |= 0xFF000000;
                    }
                }}
            Line += DIBSection.Stride;
        }}
}

static void
UpdateCountdownWindowGraphic(char *Line0,
    char *Line1,
    char *Line2)
{
    GetFonts();

    win32_dib_section &DIBSection = CountdownDIBSection;
    HDC DrawDC = DIBSection.DrawDC;

    PreClear(DIBSection);

    SetTextColor(DrawDC, 0x00FFFFFF);
    SetBkMode(DrawDC, TRANSPARENT);

    RECT TextRect;
    TextRect.top = 15;
    TextRect.left = 15;
    TextRect.right = DIBSection.Width - 10;
    TextRect.bottom = DIBSection.Height - 10;

    int Leading0 = 0;
    int Leading1 = 20;

    HGDIOBJ OldFont = SelectObject(DrawDC, TitleFont);
    DropShadowDraw(DrawDC, TextRect, Line0);
    TextRect.top += GetPixelsPerLine(DrawDC) + Leading0;
    SelectObject(DrawDC, StatusFont);
    TextRect.left += 4;
    DropShadowDraw(DrawDC, TextRect, Line1);
    TextRect.left -= 4;
    TextRect.top += GetPixelsPerLine(DrawDC) + Leading1;
    SelectObject(DrawDC, ArtistTrackFont);
    DropShadowDraw(DrawDC, TextRect, Line2);
    SelectObject(DrawDC, OldFont);

    PostClear(DIBSection, true);

    UpdateOverlayImage(CountdownWindow, DrawDC);    
}

static void
UpdateCornerWindowGraphic(char *Line0)
{
    GetFonts();

    win32_dib_section &DIBSection = CornerDIBSection;
    HDC DrawDC = DIBSection.DrawDC;

    PreClear(DIBSection);

    SetTextColor(DrawDC, 0x00888888);
    SetBkMode(DrawDC, TRANSPARENT);

    HGDIOBJ OldFont = SelectObject(DrawDC, TinyFont);

    RECT TextRect;
    TextRect.top = DIBSection.Height - GetPixelsPerLine(DrawDC) - 2;
    TextRect.left = 6;
    TextRect.right = DIBSection.Width;
    TextRect.bottom = DIBSection.Height;

    DropShadowDraw(DrawDC, TextRect, Line0);
    SelectObject(DrawDC, OldFont);

    PostClear(DIBSection, false);

    UpdateOverlayImage(CornerWindow, DrawDC);    
}

static void
MenuExitCTray(void)
{
    PostQuitMessage(0);
}

static void
ShowCountdown(void)
{
    FadeInOut(CountdownWindow);
}

#define FILETIME_SECOND ((__int64)10000000)
#define FILETIME_MINUTE (60 * FILETIME_SECOND)
#define FILETIME_HOUR (60 * FILETIME_MINUTE)
#define FILETIME_DAY (24 * FILETIME_HOUR)
struct hour_options
{
    file_time Hours[4];
};
static hour_options
GetHourOptions(void)
{
    hour_options Options;

    SYSTEMTIME SysTime;
    GetLocalTime(&SysTime);
    SysTime.wMinute = 0;
    SysTime.wSecond = 0;
    SysTime.wMilliseconds = 0;

    file_time Time;
    SystemTimeToFileTime(&SysTime, &Time.Win);
    __int64 dMinutes = FILETIME_MINUTE*15;

    Options.Hours[0] = Time;
    Time.Raw += dMinutes;
    Options.Hours[1] = Time;
    Time.Raw += dMinutes;
    Options.Hours[2] = Time;
    Time.Raw += dMinutes;
    Options.Hours[3] = Time;

    return(Options);
}

inline bool32x
InMinute(file_time Value, __int64 Minute)
{
    bool32x Result = ((Value.Raw >= Minute) && (Value.Raw < (Minute + FILETIME_MINUTE)));

    return(Result);
}

static void
RecomputeGraphic(void)
{
    file_time MainHour = TargetHour;
    file_time QAHour = TargetHour;
    QAHour.Raw += FILETIME_HOUR;

    char Line0[256];
    wsprintf(Line0, DisplayStrings.TitleLine, DayNumber);
    char *Line1 = DisplayStrings.BiLine;
    char Line2[256] = "";

    if(TargetActive)
    {
        file_time Current;
        GetSystemTimeAsFileTime(&Current.Win);
        FileTimeToLocalFileTime(&Current.Win, &Current.Win);

        if(InMinute(Current, MainHour.Raw))
        {
            wsprintf(Line2, "Stream begins now.");
        }
        else if (InMinute(Current, MainHour.Raw + 55*FILETIME_MINUTE))
        {
            wsprintf(Line2, "Five minute warning!");
            FadeInOut(CountdownWindow);
        }
        else if(InMinute(Current, QAHour.Raw))
        {
            wsprintf(Line2, "Q&A begins now.");
            FadeInOut(CountdownWindow);
        }
        else if(InMinute(Current, QAHour.Raw + (QAMinuteCount)*FILETIME_MINUTE))
        {
            wsprintf(Line2, "Stream has ended.");
            FadeInOut(CountdownWindow);
        }
        else if(InMinute(Current, QAHour.Raw + (QAMinuteCount + 1)*FILETIME_MINUTE))
        {
            TargetActive = false;
        }
        else if(Current.Raw < MainHour.Raw)
        {
            int SecondsRemaining = (MainHour.Raw - Current.Raw)/FILETIME_SECOND;
            int MinutesRemaining = SecondsRemaining/60;
            int HoursRemaining = MinutesRemaining/60;
            SecondsRemaining -= MinutesRemaining*60;
            MinutesRemaining -= HoursRemaining*60;

            FadeInOut(CountdownWindow);
            wsprintf(Line2, "Stream begins in %d:%02d:%02d...",
                HoursRemaining, MinutesRemaining, SecondsRemaining);    
        }
    }

    if(CountdownWindow->CurrentOverlayOpacity > 0)
    {
        UpdateCountdownWindowGraphic(Line0, Line1, Line2);
    }

    if(TargetActive)
    {
        FadeIn(CornerWindow);
    }
    else
    {
        FadeOut(CornerWindow);
    }
}

static void
AddToDayNumber(int dValue)
{
    DayNumber += dValue;
    RecomputeGraphic();
    FadeInOut(CountdownWindow);
}

static void
IncrementDayNumber(void)
{
    AddToDayNumber(1);
}

static void
DecrementDayNumber(void)
{
    AddToDayNumber(-1);
}

static void
IncrementDayNumber10(void)
{
    AddToDayNumber(10);
}

static void
DecrementDayNumber10(void)
{
    AddToDayNumber(-10);
}

static void
IncrementDayNumber100(void)
{
    AddToDayNumber(100);
}

static void
DecrementDayNumber100(void)
{
    AddToDayNumber(-100);
}

static void
ActivateTarget(file_time NewTargetHour)
{
    ++DayNumber;
    TargetHour = NewTargetHour;
    TargetActive = true;
}

static void
StartSessionAtHourOption0(void)
{
    ActivateTarget(GetHourOptions().Hours[0]);
}

static void
StartSessionAtHourOption1(void)
{
    ActivateTarget(GetHourOptions().Hours[1]);
}

static void
StartSessionAtHourOption2(void)
{
    ActivateTarget(GetHourOptions().Hours[2]);
}

static void
StartSessionAtHourOption3(void)
{
    ActivateTarget(GetHourOptions().Hours[3]);
}

static void
EndSession(void)
{
    TargetActive = false;
}

hot_key HotKeys[] =
{
    {MOD_ALT | MOD_CONTROL, 'O', ShowCountdown},
    {MOD_ALT | MOD_CONTROL, 'N', IncrementDayNumber},
    {MOD_ALT | MOD_CONTROL | MOD_SHIFT, 'N', IncrementDayNumber10},
    {MOD_ALT | MOD_CONTROL, 'P', DecrementDayNumber},
    {MOD_ALT | MOD_CONTROL | MOD_SHIFT, 'P', DecrementDayNumber10},
};
int32x const HotKeyCount = sizeof(HotKeys)/sizeof(HotKeys[0]);

void  
Win32AddSubMenu(HMENU MenuHandle, char *SubMenuTitle, HMENU SubMenu)
{
    AppendMenu(MenuHandle, MF_POPUP, UINT(SubMenu), SubMenuTitle);
}

int32x  
Win32AddMenuItem(HMENU MenuHandle, char *ItemText,
    bool32x Checked, bool32x Enabled,
    void *ExtraData)
{
    // Figure out whether the menu item should be checked or not
    UINT Flags = MF_STRING;
    if(Checked)
    {
        Flags |= MF_CHECKED;
    }

    MENUITEMINFO MenuItem;
    MenuItem.cbSize = sizeof(MenuItem);
    MenuItem.fMask = MIIM_ID | MIIM_STATE | MIIM_DATA | MIIM_TYPE;
    MenuItem.fType = MFT_STRING;
    MenuItem.fState = ((Checked ? MFS_CHECKED : MFS_UNCHECKED) |
            (Enabled ? MFS_ENABLED : MFS_DISABLED));
    MenuItem.wID = GetMenuItemCount(MenuHandle) + 1;
    MenuItem.dwItemData = (LPARAM)ExtraData;
    MenuItem.dwTypeData = (char *)ItemText;

    InsertMenuItem(MenuHandle, GetMenuItemCount(MenuHandle),
        true, &MenuItem);

    return(MenuItem.wID);
}

void  
Win32AddSeparator(HMENU MenuHandle)
{
    MENUITEMINFO MenuItem;
    MenuItem.cbSize = sizeof(MenuItem);
    MenuItem.fMask = MIIM_ID | MIIM_DATA | MIIM_TYPE;
    MenuItem.fType = MFT_SEPARATOR;
    MenuItem.wID = 0;
    MenuItem.dwItemData = 0;

    InsertMenuItem(MenuHandle, GetMenuItemCount(MenuHandle),
        true, &MenuItem);
}

void * 
Win32GetMenuItemExtraData(HMENU MenuHandle, int Index)
{
    MENUITEMINFO MenuItemInfo;

    MenuItemInfo.cbSize = sizeof(MenuItemInfo);
    MenuItemInfo.fMask = MIIM_DATA;
    MenuItemInfo.dwItemData = 0;

    GetMenuItemInfo(MenuHandle, Index, false, &MenuItemInfo);

    return((void *)MenuItemInfo.dwItemData);
}

typedef void menu_callback(void);
LRESULT CALLBACK
TrayWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case Win32TrayIconMessage:
        {
            switch(LParam)              
            {                
                case WM_LBUTTONDOWN:
                {
                    POINT MousePosition = {0, 0};
                    GetCursorPos(&MousePosition);

                    HMENU WindowMenu = CreatePopupMenu();

                    if(TargetActive)
                    {
                        Win32AddMenuItem(WindowMenu, "End current session", false, true, EndSession);
                    }
                    else
                    {
                        menu_callback *Callbacks[] =
                        {
                            StartSessionAtHourOption0,
                            StartSessionAtHourOption1,
                            StartSessionAtHourOption2,
                            StartSessionAtHourOption3,
                        };
                        hour_options Options = GetHourOptions();
                        for(int HourOptionIndex = 0;
                            HourOptionIndex < (sizeof(Options.Hours)/sizeof(Options.Hours[0]));
                            ++HourOptionIndex)
                        {
                            SYSTEMTIME SysTime;
                            FileTimeToSystemTime(&Options.Hours[HourOptionIndex].Win, &SysTime);

                            int Hour = SysTime.wHour;
                            int Minute = SysTime.wMinute;

                            char *Suffix = ((Hour >= 12) ? "PM" : "AM");
                            if(Hour > 12)
                            {
                                Hour = Hour - 12;
                            }

                            char Buf[256];
                            wsprintf(Buf, "Start session at %d:%02d%s",
                                Hour, Minute, Suffix);
                            Win32AddMenuItem(WindowMenu, Buf, false, true, Callbacks[HourOptionIndex]);
                        }
                    }
                    Win32AddSeparator(WindowMenu);
                    Win32AddMenuItem(WindowMenu, "Close this menu", false, true, 0);
                    Win32AddMenuItem(WindowMenu, "Exit CTray", false, true, MenuExitCTray);

                    int32x PickedIndex = TrackPopupMenu(
                        WindowMenu,
                        TPM_LEFTBUTTON |
                            TPM_NONOTIFY |
                            TPM_RETURNCMD |
                            TPM_CENTERALIGN |
                            TPM_TOPALIGN,
                        MousePosition.x, MousePosition.y,
                        0, Window, 0);

                    menu_callback *Callback = (menu_callback *)Win32GetMenuItemExtraData(
                        WindowMenu, PickedIndex);
                    if(Callback)
                    {
                        Callback();
                    }

                    DestroyMenu(WindowMenu);
                } break;

                default:
                {
                    // An ignored tray message
                } break;
            }
        } break;

        case WM_CREATE:
        {
            SetTimer(Window, 1, 100, 0);
        } break;

        case WM_TIMER:
        {
            RecomputeGraphic();
        } break;

        case WM_HOTKEY:
        {
            int32x HotKeyIndex = WParam;
            if(HotKeyIndex < HotKeyCount)
            {
                HotKeys[HotKeyIndex].Callback();
            }
        } break;

        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;        
    }

    return(Result);
}

int CALLBACK
WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    FILE *Settings = fopen("settings.ctray", "r");
    if(Settings)
    {
        fgets(DisplayStrings.TitleLine, sizeof(DisplayStrings.TitleLine), Settings);
        fgets(DisplayStrings.BiLine, sizeof(DisplayStrings.BiLine), Settings);
        fgets(DisplayStrings.CornerTag, sizeof(DisplayStrings.CornerTag), Settings);
        fclose(Settings);
    }
    else
    {
        strcpy(DisplayStrings.TitleLine, "UNKNOWN STREAM - Day %d");
        strcpy(DisplayStrings.BiLine, "unknown.stream");
        strcpy(DisplayStrings.CornerTag, "UNKNOWN STREAM    An Unknown Project in FORTRAN66   (unknown.stream)");
    }

    Win32RegisterWindowClass(TrayWindowClassName, GetModuleHandle(0), TrayWindowCallback);

    TrayWindow = CreateWindowEx(0, TrayWindowClassName, 0, 0,
        0, 0, 1, 1,
        0, 0, GetModuleHandle(0), 0);

    int32x ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int32x ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    int32x CountdownWindowWidth = 1400;
    int32x CountdownWindowHeight = 200;

    Win32ResizeDIBSection(CountdownDIBSection, CountdownWindowWidth, CountdownWindowHeight);    
    CountdownWindow = CreateOverlayWindow(0, ScreenHeight-CountdownWindowHeight,
        CountdownWindowWidth, CountdownWindowHeight);

    int32x CornerWindowWidth = 432;
    int32x CornerWindowHeight = 266;
    Win32ResizeDIBSection(CornerDIBSection, CornerWindowWidth, CornerWindowHeight);
    CornerWindow = CreateOverlayWindow(ScreenWidth-CornerWindowWidth, ScreenHeight-CornerWindowHeight,
        CornerWindowWidth, CornerWindowHeight);
    UpdateCornerWindowGraphic(DisplayStrings.CornerTag);

    if(TrayWindow)
    {
        for(int32x HotKeyIndex = 0;
             HotKeyIndex < HotKeyCount;
             ++HotKeyIndex)
        {
            hot_key &HotKey = HotKeys[HotKeyIndex];
            RegisterHotKey(TrayWindow, HotKeyIndex, HotKey.Modifiers, HotKey.VKCode);
        }

        // Insert ourselves into the system tray
        static NOTIFYICONDATA TrayIconData;
        TrayIconData.cbSize = sizeof(NOTIFYICONDATA); 
        TrayIconData.hWnd = TrayWindow;
        TrayIconData.uID = 0;
        TrayIconData.uFlags = NIF_MESSAGE | NIF_ICON;
        TrayIconData.uCallbackMessage = Win32TrayIconMessage;
        TrayIconData.hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(101));
        TrayIconData.szTip[0] = '\0';

        Shell_NotifyIcon(NIM_ADD, &TrayIconData);

        MSG Message;
        while(GetMessage(&Message, 0, 0, 0) > 0)
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        Shell_NotifyIcon(NIM_DELETE, &TrayIconData);

        for(int32x HotKeyIndex = 0;
             HotKeyIndex < HotKeyCount;
             ++HotKeyIndex)
        {
            hot_key &HotKey = HotKeys[HotKeyIndex];
            UnregisterHotKey(TrayWindow, HotKeyIndex);
        }
    }

    ExitProcess(0);
}
