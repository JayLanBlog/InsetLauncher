#include "hook/hooks.h"
#include "gl_driver.h"
#include "table/win_gl_dispatch_table.h"

#include "driver/gl/table/gl_dispatch_table.h"


WINGLDispatchTable WGL = {};

class WINGLHook : LibraryHook
{
public:
	WINGLHook() : driver() {}
	void RegisterHooks();
    bool eglDisabled = false;


    bool swapRecurse = false;
    bool createRecurse = false;
	void PopulateFromContext(HDC dc, HGLRC rc);

	WrappedOpenGL driver;
} WINGLHook;


void WINGLHook::PopulateFromContext(HDC dc, HGLRC rc)
{
	//SetDriverForHooks(&driver);
	//EnableGLHooks();
	{
        // get the current DC/context
        HDC prevDC = WGL.wglGetCurrentDC();
        HGLRC prevContext = WGL.wglGetCurrentContext();

        // activate the given context
        WGL.wglMakeCurrent(dc, rc);

        // fill out all functions that we need to get from wglGetProcAddress
        if (!WGL.wglCreateContextAttribsARB)
            WGL.wglCreateContextAttribsARB =
            (PFN_wglCreateContextAttribsARB)WGL.wglGetProcAddress("wglCreateContextAttribsARB");

        if (!WGL.wglMakeContextCurrentARB)
            WGL.wglMakeContextCurrentARB =
            (PFN_wglMakeContextCurrentARB)WGL.wglGetProcAddress("wglMakeContextCurrentARB");

        if (!WGL.wglGetPixelFormatAttribivARB)
            WGL.wglGetPixelFormatAttribivARB =
            (PFN_wglGetPixelFormatAttribivARB)WGL.wglGetProcAddress("wglGetPixelFormatAttribivARB");

        if (!WGL.wglGetExtensionsStringEXT)
            WGL.wglGetExtensionsStringEXT =
            (PFN_wglGetExtensionsStringEXT)WGL.wglGetProcAddress("wglGetExtensionsStringEXT");

        if (!WGL.wglGetExtensionsStringARB)
            WGL.wglGetExtensionsStringARB =
            (PFN_wglGetExtensionsStringARB)WGL.wglGetProcAddress("wglGetExtensionsStringARB");

        /*
            GL.PopulateWithCallback([](const char* funcName) {
            ScopedSuppressHooking suppress;
            return (void*)WGL.wglGetProcAddress(funcName);
            });
        
        */

        // restore DC/context
        if (!WGL.wglMakeCurrent(prevDC, prevContext))
        {
  
            std::wcout << "Couldn't restore prev context "<< prevContext <<" with prev DC "<< prevDC <<" - possibly stale. Using new DC "<<dc<<" to " <<
                "ensure context is rebound properly" << std::endl;
            WGL.wglMakeCurrent(dc, prevContext);
        }
	
	}
}





static HGLRC WINAPI wglCreateContext_hooked(HDC dc)
{
    SCOPED_LOCK(glLock);

    if (WINGLHook.createRecurse || WINGLHook.eglDisabled)
        return WGL.wglCreateContext(dc);

    WINGLHook.createRecurse = true;

    HGLRC ret = WGL.wglCreateContext(dc);

    if (ret)
    {
        DWORD err = GetLastError();

        WINGLHook.PopulateFromContext(dc, ret);

        /*
         GLWindowingData data;
        data.DC = dc;
        data.wnd = WindowFromDC(dc);
        data.ctx = ret;

        WINGLHook.driver.CreateContext(data, NULL, WINGLHook.GetInitParamsForDC(dc), false, false);

        WINGLHook.haveContextCreation = true;
        */

        SetLastError(err);
    }

    WINGLHook.createRecurse = false;

    return ret;
}

static BOOL WINAPI wglDeleteContext_hooked(HGLRC rc)
{
    SCOPED_LOCK(glLock);

    /*
     if (WINGLHook.haveContextCreation && !WINGLHook.eglDisabled)
    {
        SCOPED_LOCK(glLock);
        WINGLHook.driver.DeleteContext(rc);
        WINGLHook.contexts.erase(rc);
    }
    */

    SetLastError(0);

    return WGL.wglDeleteContext(rc);
}

static HGLRC WINAPI wglCreateLayerContext_hooked(HDC dc, int iLayerPlane)
{
    SCOPED_LOCK(glLock);

    if (WINGLHook.createRecurse || WINGLHook.eglDisabled)
        return WGL.wglCreateLayerContext(dc, iLayerPlane);

    WINGLHook.createRecurse = true;

    HGLRC ret = WGL.wglCreateLayerContext(dc, iLayerPlane);

    if (ret)
    {
        DWORD err = GetLastError();

        WINGLHook.PopulateFromContext(dc, ret);

        WINGLHook.createRecurse = true;

       /*
        GLWindowingData data;
        data.DC = dc;
        data.wnd = WindowFromDC(dc);
        data.ctx = ret;

        WINGLHook.driver.CreateContext(data, NULL, WINGLHook.GetInitParamsForDC(dc), false, false);

        WINGLHook.haveContextCreation = true;
       */

        SetLastError(err);
    }

    WINGLHook.createRecurse = false;

    return ret;
}

static HGLRC WINAPI wglCreateContextAttribsARB_hooked(HDC dc, HGLRC hShareContext,
    const int* attribList)
{
    SCOPED_LOCK(glLock);

    // don't recurse
    if (WINGLHook.createRecurse || WINGLHook.eglDisabled)
        return WGL.wglCreateContextAttribsARB(dc, hShareContext, attribList);

    WINGLHook.createRecurse = true;

    int defaultAttribList[] = { 0 };

    const int* attribs = attribList ? attribList : defaultAttribList;
    std::vector<int> attribVec;

    // modify attribs to our liking
    {
        bool flagsFound = false;
        const int* a = attribs;
        while (*a)
        {
            int name = *a++;
            int val = *a++;

            if (name == WGL_CONTEXT_FLAGS_ARB)
            {
                /*
                if (RenderDoc::Inst().GetCaptureOptions().apiValidation)
                    val |= WGL_CONTEXT_DEBUG_BIT_ARB;
                else
                */
                val &= ~WGL_CONTEXT_DEBUG_BIT_ARB;

                // remove NO_ERROR bit
                val &= ~GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR;

                flagsFound = true;
            }

            attribVec.push_back(name);
            attribVec.push_back(val);
        }

        if (!flagsFound)// && RenderDoc::Inst().GetCaptureOptions().apiValidation
        {
            attribVec.push_back(WGL_CONTEXT_FLAGS_ARB);
            attribVec.push_back(WGL_CONTEXT_DEBUG_BIT_ARB);
        }

        attribVec.push_back(0);

        attribs = &attribVec[0];
    }

    //RDCDEBUG("wglCreateContextAttribsARB:");

    bool core = false, es = false;

    int* a = (int*)attribs;
    while (*a)
    {
        //RDCDEBUG("%x: %d", a[0], a[1]);

        if (a[0] == WGL_CONTEXT_PROFILE_MASK_ARB)
        {
            core = (a[1] & WGL_CONTEXT_CORE_PROFILE_BIT_ARB) != 0;
            es = (a[1] & (WGL_CONTEXT_ES_PROFILE_BIT_EXT | WGL_CONTEXT_ES2_PROFILE_BIT_EXT)) != 0;
        }

        a += 2;
    }

    if (es)
    {
       // WINGLHook.driver.SetDriverType(RDCDriver::OpenGLES);
        core = true;
    }

    SetLastError(0);

    HGLRC ret = WGL.wglCreateContextAttribsARB(dc, hShareContext, attribs);

    DWORD err = GetLastError();

    // don't continue if creation failed
    if (ret == NULL)
    {
        WINGLHook.createRecurse = false;
        return ret;
    }

    WINGLHook.PopulateFromContext(dc, ret);

   /*
    GLWindowingData data;
    data.DC = dc;
    data.wnd = WindowFromDC(dc);
    data.ctx = ret;

    WINGLHook.driver.CreateContext(data, hShareContext, WINGLHook.GetInitParamsForDC(dc), core, true);

    WINGLHook.haveContextCreation = true;
   */

    SetLastError(err);

    WINGLHook.createRecurse = false;

    return ret;
}

static BOOL WINAPI wglShareLists_hooked(HGLRC oldContext, HGLRC newContext)
{
    SCOPED_LOCK(glLock);

    bool ret = WGL.wglShareLists(oldContext, newContext) == TRUE;

    DWORD err = GetLastError();

    /*
       if (ret && !WINGLHook.eglDisabled)
    {
        SCOPED_LOCK(glLock);

        ret &= WINGLHook.driver.ForceSharedObjects(oldContext, newContext);
    }
    */

    SetLastError(err);

    return ret ? TRUE : FALSE;
}

static BOOL WINAPI wglMakeCurrent_hooked(HDC dc, HGLRC rc)
{
    SCOPED_LOCK(glLock);

    BOOL ret = WGL.wglMakeCurrent(dc, rc);

    DWORD err = GetLastError();

   /*
    if (ret && !WINGLHook.eglDisabled)
    {
        WINGLHook.ProcessContextActivate(rc, dc);
    }
   */

    SetLastError(err);

    return ret;
}

static BOOL WINAPI wglMakeContextCurrentARB_hooked(HDC drawDC, HDC readDC, HGLRC rc)
{
    SCOPED_LOCK(glLock);

    BOOL ret = WGL.wglMakeContextCurrentARB(drawDC, readDC, rc);

    DWORD err = GetLastError();
    /*
    
    if (ret && !WINGLHook.eglDisabled)
    {
        WINGLHook.ProcessContextActivate(rc, drawDC);
    }
    */

    SetLastError(err);

    return ret;
}

static BOOL WINAPI SwapBuffers_hooked(HDC dc)
{
    SCOPED_LOCK(glLock);

    //WINGLHook.ProcessSwapBuffers(GLChunk::SwapBuffers, dc);
#ifdef ENABLED_LOG
    std::wcout << "SwapBuffers hooked" << std::endl;
#endif // 

    WINGLHook.swapRecurse = true;
    BOOL ret = WGL.SwapBuffers(dc);
    WINGLHook.swapRecurse = false;

    return ret;
}

static BOOL WINAPI wglSwapBuffers_hooked(HDC dc)
{
    SCOPED_LOCK(glLock);

   // WINGLHook.ProcessSwapBuffers(GLChunk::wglSwapBuffers, dc);

    WINGLHook.swapRecurse = true;
    BOOL ret = WGL.wglSwapBuffers(dc);
    WINGLHook.swapRecurse = false;

    return ret;
}

static BOOL WINAPI wglSwapLayerBuffers_hooked(HDC dc, UINT planes)
{
    SCOPED_LOCK(glLock);

    //WINGLHook.ProcessSwapBuffers(GLChunk::wglSwapBuffers, dc);

    WINGLHook.swapRecurse = true;
    BOOL ret = WGL.wglSwapLayerBuffers(dc, planes);
    WINGLHook.swapRecurse = false;

    return ret;
}

static BOOL WINAPI wglSwapMultipleBuffers_hooked(UINT numSwaps, CONST WGLSWAP* pSwaps)
{
    SCOPED_LOCK(glLock);

    /*
       for (UINT i = 0; pSwaps && i < numSwaps; i++)
        WINGLHook.ProcessSwapBuffers(GLChunk::wglSwapBuffers, pSwaps[i].hdc);

    */
    WINGLHook.swapRecurse = true;
    BOOL ret = WGL.wglSwapMultipleBuffers(numSwaps, pSwaps);
    WINGLHook.swapRecurse = false;

    return ret;
}

static LONG WINAPI ChangeDisplaySettingsA_hooked(DEVMODEA* mode, DWORD flags)
{
    if ((flags & CDS_FULLSCREEN) == 0 || WINGLHook.eglDisabled )
        return WGL.ChangeDisplaySettingsA(mode, flags);

    return DISP_CHANGE_SUCCESSFUL;
}

static LONG WINAPI ChangeDisplaySettingsW_hooked(DEVMODEW* mode, DWORD flags)
{
    if ((flags & CDS_FULLSCREEN) == 0 || WINGLHook.eglDisabled )
        return WGL.ChangeDisplaySettingsW(mode, flags);

    return DISP_CHANGE_SUCCESSFUL;
}

static LONG WINAPI ChangeDisplaySettingsExA_hooked(LPCSTR devname, DEVMODEA* mode, HWND wnd,
    DWORD flags, LPVOID param)
{
    if ((flags & CDS_FULLSCREEN) == 0 || WINGLHook.eglDisabled)
        return WGL.ChangeDisplaySettingsExA(devname, mode, wnd, flags, param);

    return DISP_CHANGE_SUCCESSFUL;
}

static LONG WINAPI ChangeDisplaySettingsExW_hooked(LPCWSTR devname, DEVMODEW* mode, HWND wnd,
    DWORD flags, LPVOID param)
{
    if ((flags & CDS_FULLSCREEN) == 0 || WINGLHook.eglDisabled)
        return WGL.ChangeDisplaySettingsExW(devname, mode, wnd, flags, param);

    return DISP_CHANGE_SUCCESSFUL;
}

static PROC WINAPI wglGetProcAddress_hooked(const char* func)
{

    SCOPED_LOCK(glLock);

    PROC realFunc = NULL;
    {
        realFunc = WGL.wglGetProcAddress(func);
    }

    if (WINGLHook.eglDisabled)
        return realFunc;

    // if the real context doesn't support this function, and we don't provide an implementation fully
    // ourselves, return NULL
    if (realFunc == NULL && !FullyImplementedFunction(func))
        return realFunc;

    // otherwise if we plan to return a hook anyway, ensure we don't leak the implementation's
    // LastError code
    SetLastError(0);

    if (!strcmp(func, "wglCreateContext"))
        return (PROC)&wglCreateContext_hooked;
    if (!strcmp(func, "wglDeleteContext"))
        return (PROC)&wglDeleteContext_hooked;
    if (!strcmp(func, "wglCreateLayerContext"))
        return (PROC)&wglCreateLayerContext_hooked;
    if (!strcmp(func, "wglCreateContextAttribsARB"))
        return (PROC)&wglCreateContextAttribsARB_hooked;
    if (!strcmp(func, "wglMakeContextCurrentARB"))
        return (PROC)&wglMakeContextCurrentARB_hooked;
    if (!strcmp(func, "wglMakeCurrent"))
        return (PROC)&wglMakeCurrent_hooked;
    if (!strcmp(func, "wglSwapBuffers"))
        return (PROC)&wglSwapBuffers_hooked;
    if (!strcmp(func, "wglSwapLayerBuffers"))
        return (PROC)&wglSwapLayerBuffers_hooked;
    if (!strcmp(func, "wglSwapMultipleBuffers"))
        return (PROC)&wglSwapMultipleBuffers_hooked;
    if (!strcmp(func, "wglGetProcAddress"))
        return (PROC)&wglGetProcAddress_hooked;

    // assume wgl functions are safe to just pass straight through, but don't pass through the wgl DX
    // interop functions
    if (strncmp(func, "wgl", 3) == 0 && strncmp(func, "wglDX", 5) != 0)
        return realFunc;

    // otherwise, consult our database of hooks
    return realFunc;
   // return (PROC)HookedGetProcAddress(func, (void*)realFunc);
}

static void WINGLHooked(void* handle)
{
  //  RDCDEBUG("WGL library hooked");
#define WINGL_FETCH(library, func) \
  WGL.func = (CONCAT(PFN_, func))Proc::GetFunctionAddress(handle, STRINGIZE(func));
    WINGL_NONHOOKED_SYMBOLS(WINGL_FETCH)
#undef WGL_FETCH

        // maybe in future we could create a dummy context here and populate the GL hooks already?
}

void WINGLHook::RegisterHooks()
{
  //  RDCLOG("Registering WGL hooks");
#ifdef ENABLED_LOG
    std::wcout << "Registering WINGL hooks" << std::endl;
#endif // ENABLE_LOG
    LoadLibraryA("opengl32.dll");

    LibraryHooks::RegisterHookedLibrary("opengl32.dll", &WINGLHooked);
    LibraryHooks::RegisterHookedLibrary("gdi32.dll", NULL);
    LibraryHooks::RegisterHookedLibrary("user32.dll", NULL);

    // register EGL hooks
#define WINGL_REGISTER(library, func)                                                                  \
  if(CheckConstParam(sizeof(library) > 2))                                                           \
  {                                                                                                  \
    LibraryHooks::RegisterHookedLibraryFunction(                                                              \
        library, FuncitonEntry(STRINGIZE(func), (void **)&WGL.func, (void *)&CONCAT(func, _hooked))); \
  }
    WINGL_HOOKED_SYMBOLS(WINGL_REGISTER)
#undef WGL_REGISTER
}