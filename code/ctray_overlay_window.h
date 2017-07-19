#if !defined(CTRAY_OVERLAY_WINDOW_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: $
   ======================================================================== */

struct overlay_window;
overlay_window *CreateOverlayWindow(int32x X, int32x Y, int32x Width, int32x Height);
void DestroyOverlayWindow(overlay_window *Window);

void FadeIn(overlay_window *Window);
void FadeOut(overlay_window *Window);
void FadeInOut(overlay_window *Window);
void UpdateOverlayImage(overlay_window *Window, HDC UpdateDC);

#define CTRAY_OVERLAY_WINDOW_H
#endif
