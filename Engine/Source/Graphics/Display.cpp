/******************************************************************************/
#include "stdafx.h"
namespace EE{
/******************************************************************************/
#define D3D_DEBUG     0
#define FORCE_D3D9_3  0
#define FORCE_D3D10_0 0
#define FORCE_D3D10_1 0

#define MAC_GL_MT 1 // enable multi threaded rendering, TODO: test on newer hardware if improves performance, all GL Contexts should have the same value, otherwise creating VAO's from VBO's on other threads could fail

#if D3D_DEBUG || FORCE_D3D9_3 || FORCE_D3D10_0 || FORCE_D3D10_1
   #pragma message("!! Warning: Use this only for debugging !!")
#endif

#if 0
   #define LOG(x) LogN(x)
#else
   #define LOG(x)
#endif

#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
/******************************************************************************/
Display D;

#if DX11 // DirectX 10/11
   #define GDI_COMPATIBLE 0 // requires DXGI_FORMAT_B8G8R8A8_UNORM or DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
   static Bool                  AllowTearing;
   static UInt                  PresentFlags;
          ID3D11Device         *D3D;
          ID3D11DeviceContext  *D3DC;
          ID3D11DeviceContext1 *D3DC1;
       #if WINDOWS_OLD
          IDXGIFactory1        *Factory;
          IDXGISwapChain       *SwapChain;
          DXGI_SWAP_CHAIN_DESC  SwapChainDesc;
       #else
          IDXGIFactory2        *Factory;
          IDXGISwapChain1      *SwapChain;
          DXGI_SWAP_CHAIN_DESC1 SwapChainDesc;
       #endif
          IDXGIAdapter         *Adapter;
          IDXGIOutput          *Output;
          ID3D11Query          *Query;
          D3D_FEATURE_LEVEL     FeatureLevel;
#elif GL // OpenGL
   #if WINDOWS
      static HDC hDC;
   #elif MAC
      NSOpenGLContext *OpenGLContext;
   #elif LINUX
      typedef void (*glXSwapIntervalType)(::Display *display, GLXDrawable drawable, int interval);
      static         glXSwapIntervalType   glXSwapInterval;
                   ::Display              *XDisplay;
                     GLXFBConfig           GLConfig;
      static         int                   vid_modes=0;
      static         XF86VidModeModeInfo **vid_mode=null;
   #elif ANDROID
      static EGLConfig  GLConfig;
      static EGLDisplay GLDisplay;
      static Int        GLCtxVer;
   #elif IOS
      UInt FBO1;
   #endif
   #if APPLE
      static CFBundleRef OpenGLBundle;
   #endif
               GLContext       MainContext;
   static Mems<GLContext> SecondaryContexts;
   static SyncLock        ContextLock;
   static SyncCounter     ContextUnlocked; // have to use counter and not event, because if there would be 2 unlocks at the same time while 2 other are waiting, then only 1 would get woken up with event
          UInt            FBO, VAO;
#endif

#if WINDOWS_NEW
Flt ScreenScale; // can't initialize here because crash will occur
#elif IOS
Flt ScreenScale=[UIScreen mainScreen].nativeScale;
#endif
/******************************************************************************/
static Bool ActualSync() {return D.sync() && !VR.active();} // we can synchronize only when not using VR, otherwise, it will handle synchronization based on the VR refresh rate
/******************************************************************************/
static Bool CustomMode;
#if MAC
extern NSOpenGLView *OpenGLView;
static CGDisplayModeRef GetDisplayMode(Int width, Int height)
{
   CFArrayRef       modes=CGDisplayCopyAllDisplayModes(CGMainDisplayID(), null);
   CGDisplayModeRef ret=null;
   Flt              refresh;
   REP(CFArrayGetCount(modes))
   {
      CGDisplayModeRef mode=(CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
      UInt flags=CGDisplayModeGetIOFlags(mode);
      Bool ok   =FlagTest(flags, kDisplayModeSafetyFlags);
      if(  ok)
      {
         Int w=CGDisplayModeGetWidth      (mode),
             h=CGDisplayModeGetHeight     (mode);
         Flt r=CGDisplayModeGetRefreshRate(mode);
         if(w==width && h==height)
            if(!ret || r>refresh) // choose highest refresh rate
         {
            ret    =mode;
            refresh=r;
         }
      }
   }
   CFRelease(modes);
   return ret;
}
#endif
Bool SetDisplayMode(Int mode)
{
   Bool  full=(D.full() && (mode==2 || mode==1 && App.activeOrBackFull()));
   VecI2 size=(full ? D.res() : App.desktop());
#if WINDOWS_OLD
   auto monitor=D.getMonitor();
   auto device =(monitor ? WChar(monitor->device_name) : null); // use null which means default device if no monitor available
   if(full)
   {
      if(monitor && monitor->mode()==D.res())return true;

      DEVMODE mode; Zero(mode);
      mode.dmSize              =SIZE(mode);
      mode.dmPelsWidth         =D.resW();
      mode.dmPelsHeight        =D.resH();
      mode.dmDisplayFixedOutput=DMDFO_STRETCH; // this will stretch to entire screen if aspect ratio is not the same
      mode.dmFields            =DM_PELSWIDTH|DM_PELSHEIGHT|DM_DISPLAYFIXEDOUTPUT;
   again:
      Int result=ChangeDisplaySettingsEx(device, &mode, null, CDS_FULLSCREEN, null); if(result==DISP_CHANGE_SUCCESSFUL)
      {
         CustomMode=true;
         return true;
      }
      if(result==DISP_CHANGE_BADMODE && (mode.dmFields&DM_DISPLAYFIXEDOUTPUT)) // this can fail if trying to set the biggest resolution with stretching
         {FlagDisable(mode.dmFields, DM_DISPLAYFIXEDOUTPUT); mode.dmDisplayFixedOutput=0; goto again;} // try again without scaling
   }else
   {
      if(!CustomMode)return true;
      if(ChangeDisplaySettingsEx(device, null, null, 0, null)==DISP_CHANGE_SUCCESSFUL)
      {
         CustomMode=false;
         return true;
      }
   }
#elif MAC
   if(OpenGLView)
   {
      REPA(Ms._button)Ms.release(i); // Mac will not detect mouse button release if it was pressed during screen changing, so we need to disable it manually

      if([OpenGLView isInFullScreenMode])[OpenGLView exitFullScreenModeWithOptions:nil];

      // set screen mode
      Bool ok=false;
      if(D.screen()==size)ok=true;else
      if(CGDisplayModeRef mode=GetDisplayMode(size.x, size.y))
         ok=(CGDisplaySetDisplayMode(kCGDirectMainDisplay, mode, null)==noErr);

      if(ok && full)
      {
      #if 0
         ok=[OpenGLView enterFullScreenMode:[NSScreen mainScreen] withOptions:nil];
      #else
         NSApplicationPresentationOptions options=NSApplicationPresentationHideDock|NSApplicationPresentationHideMenuBar;
         ok=[OpenGLView enterFullScreenMode:[NSScreen mainScreen] withOptions:@{NSFullScreenModeApplicationPresentationOptions:@(options)}];
      #endif
      }
      return ok;
   }
#elif LINUX
   if(XDisplay)
   {
      // set screen mode
      Bool ok=false;
      if(D.screen()==size)ok=true;else
      FREP(vid_modes)
      {
         XF86VidModeModeInfo &mode=*vid_mode[i];
         if(mode.hdisplay==size.x && mode.vdisplay==size.y)
            if(XF86VidModeSwitchToMode(XDisplay, DefaultScreen(XDisplay), &mode) && XFlush(XDisplay)){ok=true; break;}
      }
      return ok;  
   }
#endif
   return false;
}
#if WINDOWS_NEW
void RequestDisplayMode(Int w, Int h, Int full)
{
   if(full> 0)Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->TryEnterFullScreenMode();else
   if(full==0)Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->ExitFullScreenMode    ();
   if(w>0 || h>0)
   {
      if(w<=0)w=D.resW();
      if(h<=0)h=D.resH();
      Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->TryResizeView(Windows::Foundation::Size(PixelsToDips(w), PixelsToDips(h)));
   }
}
#endif

#if WINDOWS && GL
static void glewSafe()
{
#define V(x, y, z) {if(!x)x=y; if(!x)Exit("OpenGL " z " function not supported.\nGraphics Driver not installed or better video card is required.");}
   V(glGenRenderbuffers          , glGenRenderbuffersEXT          , "glGenRenderbuffers")
   V(glDeleteRenderbuffers       , glDeleteRenderbuffersEXT       , "glDeleteRenderbuffers")
   V(glRenderbufferStorage       , glRenderbufferStorageEXT       , "glRenderbufferStorage")
   V(glGetRenderbufferParameteriv, glGetRenderbufferParameterivEXT, "glGetRenderbufferParameteriv")
   V(glBindRenderbuffer          , glBindRenderbufferEXT          , "glBindRenderbuffer")

   V(glGenFramebuffers           , glGenFramebuffersEXT           , "glGenFramebuffers")
   V(glDeleteFramebuffers        , glDeleteFramebuffersEXT        , "glDeleteFramebuffers")
   V(glBindFramebuffer           , glBindFramebufferEXT           , "glBindFramebuffer")
   V(glBlitFramebuffer           , glBlitFramebufferEXT           , "glBlitFramebuffer")
#if DEBUG
   V(glCheckFramebufferStatus    , glCheckFramebufferStatusEXT    , "glCheckFramebufferStatus")
#endif   

   V(glFramebufferTexture2D      , glFramebufferTexture2DEXT      , "glFramebufferTexture2D")
   V(glFramebufferRenderbuffer   , glFramebufferRenderbufferEXT   , "glFramebufferRenderbuffer")

   V(glBlendColor                , glBlendColorEXT                , "glBlendColor")
   V(glBlendEquation             , glBlendEquationEXT             , "glBlendEquation")
   V(glBlendEquationSeparate     , glBlendEquationSeparateEXT     , "glBlendEquationSeparate")
   V(glBlendFuncSeparate         , glBlendFuncSeparateEXT         , "glBlendFuncSeparate")

   V(glColorMaski, glColorMaskIndexedEXT, "glColorMaski")
#undef V
}
#endif
/******************************************************************************/
// GL CONTEXT
/******************************************************************************/
#if GL
static Ptr GetCurrentContext()
{
#if WINDOWS
   return wglGetCurrentContext();
#elif MAC
   return CGLGetCurrentContext();
#elif LINUX
   return glXGetCurrentContext();
#elif ANDROID
   return eglGetCurrentContext();
#elif IOS
   return [EAGLContext currentContext];
#elif WEB
   return (Ptr)emscripten_webgl_get_current_context();
#endif
}
Bool GLContext::is()C
{
#if WEB
   return context!=NULL;
#else
   return context!=null;
#endif
}
GLContext::GLContext()
{
   locked=false;
#if WEB
   context=NULL;
#else
   context=null;
#endif
#if ANDROID
   surface=null;
#endif
}
void GLContext::del()
{
   if(context)
   {
   #if WINDOWS
      wglMakeCurrent(null, null); wglDeleteContext(context); context=null;
   #elif MAC
      CGLDestroyContext(context); context=null;
   #elif LINUX
      if(XDisplay){glXMakeCurrent(XDisplay, NULL, NULL); glXDestroyContext(XDisplay, context);} context=null;
   #elif ANDROID
      if(GLDisplay){eglMakeCurrent(GLDisplay, null, null, null); eglDestroyContext(GLDisplay, context);} context=null;
   #elif IOS
      [EAGLContext setCurrentContext:null]; [context release]; context=null;
   #elif WEB
      emscripten_webgl_destroy_context(context); context=NULL;
   #endif
   }
#if ANDROID
   if(surface){if(GLDisplay)eglDestroySurface(GLDisplay, surface); surface=null;}
#endif
}
Bool GLContext::createSecondary()
{
#if WINDOWS
   if(context=wglCreateContext(hDC))if(!wglShareLists(MainContext.context, context))return false;
#elif MAC
   CGLCreateContext(CGLGetPixelFormat(MainContext.context), MainContext.context, &context);
#elif LINUX
   context=glXCreateNewContext(XDisplay, GLConfig, GLX_RGBA_TYPE, MainContext.context, true);
#elif ANDROID
   EGLint attribs[]={EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE}; // end of list
   if(surface=eglCreatePbufferSurface(GLDisplay, GLConfig, attribs))
   {
      EGLint ctx_attribs[]={EGL_CONTEXT_CLIENT_VERSION, GLCtxVer, EGL_NONE}; // end of list
      context=eglCreateContext(GLDisplay, GLConfig, MainContext.context, ctx_attribs);
   }
#elif IOS
	context=[[EAGLContext alloc] initWithAPI:[MainContext.context API] sharegroup:[MainContext.context sharegroup]];
#elif WEB
   // currently WEB is not multi-threaded
   #if HAS_THREADS
      add support
   #endif
#endif
   if(context)
   {
      lock();
      // these settings are per-context
   #if MAC && MAC_GL_MT
      Bool mt_ok=(CGLEnable(context, kCGLCEMPEngine)!=kCGLNoError);
   #endif
      glPixelStorei(GL_PACK_ALIGNMENT  , 1);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      unlock(); // to clear 'locked'
      return true;
   }
   return false;
}
void GLContext::lock()
{
#if WINDOWS
   if(wglMakeCurrent(hDC, context))
#elif MAC
   if(CGLSetCurrentContext(context)==kCGLNoError)
#elif LINUX
   if(glXMakeCurrent(XDisplay, App.Hwnd(), context))
#elif ANDROID
   if(eglMakeCurrent(GLDisplay, surface, surface, context)==EGL_TRUE)
#elif IOS
   if([EAGLContext setCurrentContext:context]==YES)
#elif WEB
   if(emscripten_webgl_make_context_current(context)==EMSCRIPTEN_RESULT_SUCCESS)
#endif
   {
      locked=true;
   }else
   {
      Exit("Can't activate OpenGL Context.");
   }
}
void GLContext::unlock()
{
#if WINDOWS
   if(wglMakeCurrent(hDC, null))
#elif MAC
   if(CGLSetCurrentContext(null)==kCGLNoError)
#elif LINUX
   if(glXMakeCurrent(XDisplay, NULL, NULL))
#elif ANDROID
   if(eglMakeCurrent(GLDisplay, null, null, null)==EGL_TRUE)
#elif IOS
   if([EAGLContext setCurrentContext:null]==YES)
#elif WEB
   if(emscripten_webgl_make_context_current(NULL)==EMSCRIPTEN_RESULT_SUCCESS)
#endif
   {
      locked=false;
   }else
   {
      Exit("Can't deactivate OpenGL Context.");
   }
}
#endif
/******************************************************************************/
// DISPLAY
/******************************************************************************/
VecI2 Display::screen()C
{
#if WINDOWS_OLD
   return VecI2(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
#elif WINDOWS_NEW
   VecI2 size=0;
   IDXGIFactory1 *factory=Factory;
   IDXGIAdapter  *adapter=Adapter;
   if(!adapter)
   {
      if(!factory)CreateDXGIFactory1(__uuidof(IDXGIFactory1), (Ptr*)&factory); if(factory)factory->EnumAdapters(0, &adapter); // first adapter only
   }
   if(adapter)
   {
      IDXGIOutput *output=null; adapter->EnumOutputs(0, &output); if(output) // first output is primary display - https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiadapter-enumoutputs
      {
         DXGI_OUTPUT_DESC desc; if(OK(output->GetDesc(&desc)))size.set(desc.DesktopCoordinates.right-desc.DesktopCoordinates.left, desc.DesktopCoordinates.bottom-desc.DesktopCoordinates.top);
         output->Release();
      }
      if(adapter!=Adapter)adapter->Release();
   }
   if(factory && factory!=Factory)factory->Release();
   return size;
#elif MAC
   if(CGDisplayModeRef mode=CGDisplayCopyDisplayMode(kCGDirectMainDisplay))
   {
      VecI2 s(CGDisplayModeGetWidth(mode), CGDisplayModeGetHeight(mode));
      CGDisplayModeRelease(mode);
      return s;
   }
#elif LINUX
   if(XDisplay)
   {
      int clock; XF86VidModeModeLine mode; if(XF86VidModeGetModeLine(XDisplay, DefaultScreen(XDisplay), &clock, &mode))return VecI2(mode.hdisplay, mode.vdisplay);
      Screen *screen=DefaultScreenOfDisplay(XDisplay); return VecI2(WidthOfScreen(screen), HeightOfScreen(screen));
   }
#elif ANDROID
   JNI jni;
   if(jni && ActivityClass && Activity)
      if(JMethodID screen=jni.func(ActivityClass, "screen", "()J"))
   {
      ULong s=jni->CallLongMethod(Activity, screen); // 'screen' is changed when device is rotated
      return VecI2(s&UINT_MAX, s>>32);
   }
#elif IOS
   CGSize size=[[UIScreen mainScreen] nativeBounds].size; // 'nativeBounds' is not changed when device is rotated
   VecI2  screen(RoundPos(size.width), RoundPos(size.height));
   switch(App.orientation())
   {
      case DIR_RIGHT: 
      case DIR_LEFT : screen.swap(); break; // rotate manually
   }
   return screen;
#elif WEB
   return VecI2(JavaScriptRunI("screen.width"), JavaScriptRunI("screen.height")); // it's not possible to get correct results, because on Chrome: this value is adjusted by "System DPI/Scaling", but not 'D.browserZoom', and does not change when zooming. Because "System DPI/Scaling" is unknown, it can't be calculated.
#endif
   return App.desktop(); // this is not changed when device is rotated (obtained at app startup)
}
/******************************************************************************/
void Display::setShader(C Material *material)
{
   if(created())
   {
     _set_shader_material=material;
      {Meshes     .lock(); REPA(Meshes     )Meshes     .lockedData(i).setShader(); Meshes     .unlock();}
      {ClothMeshes.lock(); REPA(ClothMeshes)ClothMeshes.lockedData(i).setShader(); ClothMeshes.unlock();}
                                                       if(set_shader)set_shader();
     _set_shader_material=null;
   }
}
Display& Display::drawNullMaterials(Bool on)
{
   if(_draw_null_mtrl!=on)
   {
     _draw_null_mtrl=on;
      setShader();
   }
   return T;
}
void Display::screenChanged(Flt old_width, Flt old_height)
{
   if(old_width>0 && old_height>0)
   {
                     Gui.screenChanged(old_width, old_height);
      if(screen_changed)screen_changed(old_width, old_height);
   }
}
Str8 Display::shaderModelName()C
{
   switch(shaderModel())
   {
      default          : return "Unknown"; // SM_UNKNOWN
      case SM_GL_ES_3  : return "GL ES 3";
      case SM_GL_ES_3_1: return "GL ES 3.1";
      case SM_GL_ES_3_2: return "GL ES 3.2";
      case SM_GL_3     : return "GL 3";
      case SM_GL_4     : return "GL 4";
      case SM_4        : return "4";
      case SM_4_1      : return "4.1";
      case SM_5        : return "5";
      case SM_6        : return "6";
      case SM_6_2      : return "6.2";
   }
}
Str8 Display::apiName()C
{
#if DX11
   return "DirectX 11";
#elif DX12
   return "DirectX 12";
#elif METAL
   return "Metal";
#elif VULKAN
   return "Vulkan";
#elif WEB // check this first before 'GL' and 'GL_ES'
   return "Web GL";
#elif GL_ES // check this first before 'GL'
   return "OpenGL ES";
#elif GL
   return "OpenGL";
#endif
}
Bool Display::smallSize()C
{
#if WINDOWS_NEW
   Dbl inches; if(OK(GetIntegratedDisplaySize(&inches)))return inches<7;
#elif ANDROID
   Int    size=((AndroidApp && AndroidApp->config) ? AConfiguration_getScreenSize(AndroidApp->config) : ACONFIGURATION_SCREENSIZE_NORMAL);
   return size==ACONFIGURATION_SCREENSIZE_SMALL || size==ACONFIGURATION_SCREENSIZE_NORMAL;
   // HTC EVO 3D             ( 4   inch) has ACONFIGURATION_SCREENSIZE_NORMAL
   // Samsung Galaxy Note 2  ( 5.5 inch) has ACONFIGURATION_SCREENSIZE_NORMAL
   // Asus Transformer Prime (10   inch) has ACONFIGURATION_SCREENSIZE_XLARGE
#elif IOS
   // UI_USER_INTERFACE_IDIOM UIUserInterfaceIdiomPhone UIUserInterfaceIdiomPad 
   return UI_USER_INTERFACE_IDIOM()==UIUserInterfaceIdiomPhone;
#endif
   return false;
}
Flt Display::browserZoom()C
{
#if WEB
   return emscripten_get_device_pixel_ratio();
#else
   return 1;
#endif
}
#if GL
VecI2 Display::glVer()
{
   if(created())
   {
      int major=0, minor=0;
      glGetIntegerv(GL_MAJOR_VERSION, &major);
      glGetIntegerv(GL_MINOR_VERSION, &minor);
      return VecI2(major, minor);
   }
   return 0;
}
Ptr Display::glGetProcAddress(CChar8 *name)
{
#if WINDOWS
   return wglGetProcAddress(name);
#elif APPLE
   if(OpenGLBundle)
      if(CFStringRef cf_name=CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII))
   {
      Ptr    proc=CFBundleGetFunctionPointerForName(OpenGLBundle, cf_name); CFRelease(cf_name);
      return proc;
   }
   return null;
#elif LINUX
   return (Ptr)glXGetProcAddressARB((C GLubyte*)name);
#elif ANDROID || WEB
   return (Ptr)eglGetProcAddress(name);
#else
   #error
#endif
}
#endif
Bool Display::gatherAvailable()C
{
#if DX11
   return shaderModel()>=SM_4_1;
#elif GL_ES
   return shaderModel()>=SM_GL_ES_3_1; // 3.1+ GLES required
#elif GL
   return shaderModel()>=SM_GL_4; // 4.0+ GL required
#endif
}
Bool Display::gatherChannelAvailable()C
{
#if DX11
   return shaderModel()>=SM_5;
#elif GL_ES
   return shaderModel()>=SM_GL_ES_3_1; // 3.1+ GLES required
#elif GL
   return shaderModel()>=SM_GL_4; // 4.0+ GL required
#endif
}
Bool Display::independentBlendAvailable()C
{
#if DX11
   return shaderModel()>=SM_4_1;
#elif GL_ES
   return shaderModel()>=SM_GL_ES_3_2; // 3.2+ GLES required
#elif GL
   return shaderModel()>=SM_GL_4; // 4.0+ GL required
#endif
}
Bool Display::deferredUnavailable  ()C {return created() &&       _max_rt<3     ;} // deferred requires at least 3 MRT's (#0 Color, #1 Nrm, #2 Ext, #3 Vel optional) #RTOutput
Bool Display::deferredMSUnavailable()C {return created() && shaderModel()<SM_4_1;} // only Shader Model 4.1 (DX 10.1) and above support multi-sampled RT's
/******************************************************************************/
// MONITOR
/******************************************************************************/
Display::Monitor::Monitor()
{
   primary=false;
   full=work.zero();
#if WINDOWS_OLD
   device_key[0]=device_name[0]='\0';
#endif
}
VecI2 Display::Monitor::mode()C
{
#if WINDOWS_OLD
   DEVMODE mode; Zero(mode);
   mode.dmSize=SIZE(mode);
   if(EnumDisplaySettings(WChar(device_name), ENUM_CURRENT_SETTINGS, &mode))return VecI2(mode.dmPelsWidth, mode.dmPelsHeight);
   return 0;
#elif WINDOWS_NEW
   VecI2 mode=0;
   if(C Ptr *hmonitor_ptr=D._monitors.dataToKey(this))
   {
      IDXGIFactory1 *factory=Factory;
      IDXGIAdapter  *adapter=Adapter;
      if(!adapter)
      {
         if(!factory)CreateDXGIFactory1(__uuidof(IDXGIFactory1), (Ptr*)&factory); if(factory)factory->EnumAdapters(0, &adapter); // first adapter only
      }
      if(adapter)
      {
         CPtr hmonitor=*hmonitor_ptr;
         for(Int i=0; ; i++) // all outputs
         {
            IDXGIOutput *output=null; adapter->EnumOutputs(i, &output); if(output) // first output is primary display - https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiadapter-enumoutputs
            {
               DXGI_OUTPUT_DESC desc; if(OK(output->GetDesc(&desc)))if(desc.Monitor==hmonitor)
               {
                  mode.set(desc.DesktopCoordinates.right-desc.DesktopCoordinates.left, desc.DesktopCoordinates.bottom-desc.DesktopCoordinates.top);
                  i=INT_MAX; // stop looking
               }
               output->Release();
            }else break;
         }
         if(adapter!=Adapter)adapter->Release();
      }
      if(factory && factory!=Factory)factory->Release();
   }
   return mode;
#else
   return D.screen(); // TODO:
#endif
}
Str Display::Monitor::standardColorProfilePath()C
{
#if WINDOWS_OLD
   wchar_t file_name[MAX_PATH]; file_name[0]='\0'; DWORD size=SIZE(file_name); // must be size in bytes
   if(GetStandardColorSpaceProfile(null, LCS_sRGB, file_name, &size))return file_name; // this is "Color Management \ Advanced \ Device Profile"
#endif
   return S;
}
Str Display::Monitor::colorProfilePath()C
{
#if WINDOWS_OLD
   if(Is(device_key))
   {
   #if SUPPORT_WINDOWS_XP || SUPPORT_WINDOWS_7
      DLL mscms; if(mscms.createFile("Mscms.dll"))
      if(auto WcsGetDefaultColorProfile=(decltype(&::WcsGetDefaultColorProfile))mscms.getFunc("WcsGetDefaultColorProfile")) // available on Vista+
      {
         auto WcsGetUsePerUserProfiles =(decltype(&::WcsGetUsePerUserProfiles ))mscms.getFunc("WcsGetUsePerUserProfiles" ); // available on Vista+, optional
   #else
      {
   #endif
         wchar_t file_name[MAX_PATH]; file_name[0]='\0';
         BOOL    user=true;

      #if SUPPORT_WINDOWS_XP || SUPPORT_WINDOWS_7
         if(WcsGetUsePerUserProfiles)
      #endif
            WcsGetUsePerUserProfiles(WChar(device_key), CLASS_MONITOR, &user); // this is needed to get precise information (without this call results were not always OK)

         if(WcsGetDefaultColorProfile(user ? WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER : WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE, WChar(device_key), CPT_ICC, CPST_RGB_WORKING_SPACE, 0, SIZE(file_name), file_name))return file_name;
      }
   }
#endif
   return S;
}
#if WINDOWS_OLD
Bool Display::Monitor::set(HMONITOR monitor)
{
   MONITORINFOEX monitor_info; Zero(monitor_info); monitor_info.cbSize=SIZE(monitor_info);
   if(GetMonitorInfo(monitor, &monitor_info))
   {
      full.set(monitor_info.rcMonitor.left, monitor_info.rcMonitor.top, monitor_info.rcMonitor.right, monitor_info.rcMonitor.bottom);
      work.set(monitor_info.rcWork   .left, monitor_info.rcWork   .top, monitor_info.rcWork   .right, monitor_info.rcWork   .bottom);
      primary=FlagTest(monitor_info.dwFlags, MONITORINFOF_PRIMARY);
      Set(device_name, WChar(monitor_info.szDevice)); ASSERT(ELMS(device_name)==ELMS(monitor_info.szDevice));

      DISPLAY_DEVICEW display_device; Zero(display_device); display_device.cb=SIZE(display_device);
      for(Int i=0; EnumDisplayDevicesW(WChar(device_name), i, &display_device, 0); i++)
         //if(FlagAll(display_device.StateFlags, DISPLAY_DEVICE_ACTIVE|DISPLAY_DEVICE_ATTACHED))
      {
         name=display_device.DeviceString;
         Set(device_key, WChar(display_device.DeviceKey)); ASSERT(ELMS(device_key)==ELMS(display_device.DeviceKey));
         break;
      }

      MemtN<VecI2, 128> modes;
      DEVMODE mode; Zero(mode); mode.dmSize=SIZE(mode);
      for(Int i=0; EnumDisplaySettings(monitor_info.szDevice, i, &mode); i++)modes.binaryInclude(VecI2(mode.dmPelsWidth, mode.dmPelsHeight));
      T.modes=modes;

      return true;
   }
   return false;
}
#endif
#if DX11
Display::Monitor* Display::getMonitor(IDXGIOutput &output)
{
   DXGI_OUTPUT_DESC desc; if(OK(output.GetDesc(&desc)))
      if(auto monitor=_monitors(desc.Monitor))
   {
      if(!monitor->is()) // if not yet initialized
      {
      #if WINDOWS_OLD
         if(!monitor->set(desc.Monitor)) // get precise 'work'
      #endif
         {
            monitor->full=monitor->work.set(desc.DesktopCoordinates.left, desc.DesktopCoordinates.top, desc.DesktopCoordinates.right, desc.DesktopCoordinates.bottom);
         #if WINDOWS_OLD
            Set(monitor->device_name, WChar(desc.DeviceName)); ASSERT(ELMS(monitor->device_name)==ELMS(desc.DeviceName));
         #endif

            MemtN<VecI2, 128> modes;
            DXGI_FORMAT                                          mode=DXGI_FORMAT_R8G8B8A8_UNORM; // always use this mode in case system doesn't support 10-bit color
            UInt                                           descs_elms=0; output.GetDisplayModeList(mode, 0, &descs_elms, null        ); // get number of mode descs
            MemtN<DXGI_MODE_DESC, 128> descs; descs.setNum(descs_elms ); output.GetDisplayModeList(mode, 0, &descs_elms, descs.data()); // get           mode descs
            FREPA(descs)modes.binaryInclude(VecI2(descs[i].Width, descs[i].Height)); // add from the start to avoid unnecessary memory moves
            monitor->modes=modes;
         }
      }
      return monitor;
   }
   return null;
}
#endif
#if WINDOWS_OLD
Display::Monitor* Display::getMonitor(HMONITOR hmonitor)
{
   if(hmonitor)if(auto monitor=_monitors(hmonitor))
   {
      if(!monitor->is()) // if not yet initialized
      {
         monitor->set(hmonitor);
      }
      return monitor;
   }
   return null;
}
static BOOL CALLBACK EnumMonitors(HMONITOR hmonitor, HDC dc, LPRECT rect, LPARAM dwData) {D.getMonitor(hmonitor); return true;} // continue
#endif
C Display::Monitor* Display::mainMonitor()C
{
   REPA(_monitors){C Monitor &monitor=_monitors[i]; if(monitor.primary)return &monitor;}
   return null;
}
C Display::Monitor* Display::curMonitor()
{
#if DX11
   if(SwapChain)
   {
      IDXGIOutput *output=null;
      {
         SyncLocker locker(_lock);
         if(SwapChain)SwapChain->GetContainingOutput(&output);
      }
      if(output)
      {
         auto monitor=getMonitor(*output);
         output->Release();
         if(monitor)return monitor;
      }
   }
#endif
#if WINDOWS_OLD // try alternative method if above failed
   if(App.hwnd())
   {
   #if 1
      if(HMONITOR hmonitor=MonitorFromWindow(App.Hwnd(), MONITOR_DEFAULTTONEAREST))
   #else
      RectI win_rect=WindowRect(false); // watch out because 'WindowRect' can return weird position when the window is minimized
      POINT p; p.x=win_rect.centerXI(); p.y=win_rect.centerYI();
      if(HMONITOR hmonitor=MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST))
   #endif
         if(auto monitor=getMonitor(hmonitor))return monitor;
   }
#endif
   return null;
}
C Display::Monitor* Display::getMonitor()
{
   if(auto monitor=curMonitor())return monitor;
           return mainMonitor();
}
/******************************************************************************/
void Display::getMonitor(RectI &full, RectI &work, VecI2 &max_normal_win_client_size, VecI2 &maximized_win_client_size)
{
   if(auto monitor=getMonitor())
   {
      full=monitor->full;
      work=monitor->work;
   }else
   {
      full.set(0, 0, App.desktopW(), App.desktopH());
      work=App.desktopArea();
   }
   max_normal_win_client_size.set(full.w()-App._bound.w(), full.h()-App._bound.h());
    maximized_win_client_size.set(work.w()+App._bound_maximized.min.x+App._bound_maximized.max.x, work.h()+App._bound_maximized.min.y+App._bound_maximized.max.y);
}
/******************************************************************************/
// MANAGE
/******************************************************************************/
Display::Display() : _monitors(Compare, null, null, 4)
{
  _full            =MOBILE; // by default request fullscreen for MOBILE, on WINDOWS_PHONE this will hide the navigation bar
  _sync            =true;
  _exclusive       =true;
  _color_space     =COLOR_SPACE_NONE;
  _hp_col_rt       =false;
  _hp_nrm_rt       =false;
  _hp_lum_rt       =false;
  _dither          =true;
  _mtrl_blend      =true;
  _device_mem      =-1;
  _monitor_prec    =IMAGE_PRECISION_8;
  _lit_col_rt_prec =IMAGE_PRECISION_8;
  _aspect_mode     =(MOBILE ? ASPECT_SMALLER : ASPECT_Y);
  _tex_filter      =(MOBILE ? 4 : 16);
  _tex_mip_filter  =true;
  _tex_detail      =(MOBILE ? TEX_USE_DISABLE : TEX_USE_MULTI);
  _density_filter  =(MOBILE ? FILTER_LINEAR : FILTER_CUBIC_FAST);
  _tex_mip_min     =0;
  _tex_mip_bias    =0;
  _tex_macro       =true;
  _tex_detail_lod  =false;
  _font_sharpness  =0.75f;
  _bend_leafs      =true;
  _particles_soft  =!MOBILE;
  _particles_smooth=!MOBILE;
  _taa             =_taa_dual=false;

  _initialized=false;
  _resetting  =false;

  _allow_stereo=true;
  _density=127;
  _samples=1;
  _scale=1;
  _disp_aspect_ratio=_disp_aspect_ratio_want=0;
  _app_aspect_ratio=1;
  _pixel_aspect=1;
  _pixel_size=_pixel_size_2=_pixel_size_inv=0;
  _window_pixel_to_screen_mul=1; // init to 1 to avoid div by 0 at app startup which could cause crash on Web
  _window_pixel_to_screen_add=0;
  _window_pixel_to_screen_scale=1;

  _ao_all      =true;
  _amb_mode    =AMBIENT_FLAT;
  _amb_soft    =1;
  _amb_jitter  =true;
  _amb_normal  =true;
  _amb_res     =FltToByteScale(0.5f);
  _amb_contrast=4.375f;
  _amb_min     =0.2f;
  _amb_range   =0.4f;
  _amb_color_l =SRGBToLinear(0.4f);

  _ns_color_l.zero();

  _env_color=1;

  _vol_light=false;
  _vol_add  =false;
  _vol_max  =1;

  _shd_mode           =(MOBILE ? SHADOW_NONE : SHADOW_MAP);
  _shd_soft           =0;
  _shd_jitter         =false;
  _shd_reduce         =false;
  _shd_frac           =1;
  _shd_fade           =1;
  _shd_map_num        =6;
  _shd_map_size       =1024;
  _shd_map_size_actual=0;
  _shd_map_size_l     =1;
  _shd_map_size_c     =1;
  _shd_map_split      .set(2, 1);
  _cld_map_size       =128;

  _diffuse_mode=(MOBILE ? DIFFUSE_LAMBERT : DIFFUSE_BURLEY);

  _bump_mode=(MOBILE ? BUMP_FLAT : BUMP_RELIEF);

  _mtn_mode  =MOTION_NONE;
  _mtn_dilate=DILATE_ORTHO2;
  _mtn_scale =1;
  _mtn_res   =FltToByteScale(1.0f/3);

  _dof_mode     =DOF_NONE;
  _dof_foc_mode =false;
//_dof_focus    =0;
  _dof_range    =30;
  _dof_intensity=1;

  _eye_adapt           =false;
  _eye_adapt_brightness=0.7f;
  _eye_adapt_exp       =0.5f;
  _eye_adapt_max_dark  =0.5f;
  _eye_adapt_max_bright=2.0f;
  _eye_adapt_speed     =6.5f;
  _eye_adapt_weight.set(0.9f, 1, 0.7f); // use smaller value for blue, to make blue skies have brighter multiplier, because human eye sees blue color as darker than others

  _grass_range  =50;
  _grass_density=(MOBILE ? 0.5f : 1);
  _grass_shadow =false;
  _grass_mirror =false;

  _bloom_original=1.0f;
  _bloom_scale   =0.5f;
  _bloom_cut     =0.3f;
  _bloom_blurs   =1;
  _bloom_max     =false;
  _bloom_half    =!MOBILE;
  _bloom_samples =!MOBILE;
          _bloom_allow=!MOBILE;
           _glow_allow=!MOBILE;
  _color_palette_allow=!MOBILE;

  _lod_factor       =1;
  _lod_factor_mirror=2;

  _tesselation          =false;
  _tesselation_allow    =true;
  _tesselation_heightmap=false;
  _tesselation_density  =60;

  _outline_mode=EDGE_DETECT_THIN;
  _edge_detect =EDGE_DETECT_NONE;
  _edge_soften =EDGE_SOFTEN_NONE;

  _fur_gravity  =-1    ;
  _fur_vel_scale=-0.75f;

  _eye_dist=0.064f; _eye_dist_2=_eye_dist/2;

  _view_square_pixel =false;
  _view_main.fov_mode=FOV_Y;
  _view_fov          =
  _view_main.fov.x   =
  _view_main.fov.y   =DegToRad(70);
  _view_main.from    =_view_from=0.05f;
  _view_main.range   =100;
  _view_main.full    =true; // needed for 'viewReset' which will always set full viewport if last was full too

  _smaa_threshold=0.1f;
}
void Display::init() // make this as a method because if we put this to Display constructor, then 'SecondaryContexts' may not have been initialized yet
{
#if DEBUG
   REP(IMAGE_ALL_TYPES)DYNAMIC_ASSERT(ImageTI[i].high_precision==(ImageTI[i].precision>IMAGE_PRECISION_8 || IsSByte(IMAGE_TYPE(i))), "Invalid 'ImageTI.high_precision'");
#endif

   secondaryOpenGLContexts(1); // default 1 secondary context

   // re-use cached result obtained at app startup, because if the app is currently fullscreen at a custom resolution, then the monitor will return that resolution, however this function is used for getting default resolutions
#if DX11
   IDXGIFactory1 *factory=null; CreateDXGIFactory1(__uuidof(IDXGIFactory1), (Ptr*)&factory); if(factory)
   {
      IDXGIAdapter *adapter=null; factory->EnumAdapters(0, &adapter); if(adapter) // first adapter only
      {
         for(Int i=0; ; i++) // all outputs
         {
            IDXGIOutput *output=null; adapter->EnumOutputs(i, &output); if(output) // first output is primary display - https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiadapter-enumoutputs
            {
               if(auto monitor=getMonitor(*output))
                  if(i==0)monitor->primary=true; // first output is primary display
               output->Release();
            }else break;
         }
         adapter->Release();
      }
      factory->Release();
   }
#elif WINDOWS_OLD
   EnumDisplayMonitors(null, null, EnumMonitors, 0); // list all monitors at app startup so we can know their original sizes
#elif MAC
   CGDirectDisplayID main_display=CGMainDisplayID(), display[256];
   CGDisplayCount    displays=0;

   if(!CGGetActiveDisplayList(Elms(display), display, &displays))FREP(displays)if(auto monitor=_monitors((Ptr)display[i]))
   {
      monitor->primary=(display[i]==main_display);
      if(monitor->primary)
      {
         monitor->full.set(0, App.desktop    ());
         monitor->work=       App.desktopArea() ;
      }//else TODO:

      // get available modes
      MemtN<VecI2, 128> modes;
      if(CFArrayRef display_modes=CGDisplayCopyAllDisplayModes(display[i], null))
      {
         Int  count=CFArrayGetCount(display_modes);
         FREP(count)
         {
            CGDisplayModeRef mode=(CGDisplayModeRef)CFArrayGetValueAtIndex(display_modes, i);
            UInt flags=CGDisplayModeGetIOFlags(mode);
            Bool ok   =FlagTest(flags, kDisplayModeSafetyFlags);
            if(  ok)modes.binaryInclude(VecI2(CGDisplayModeGetWidth(mode), CGDisplayModeGetHeight(mode)));
         }
         CFRelease(display_modes);
      }
      monitor->modes=modes;
   }
#elif LINUX
   if(XDisplay)if(auto monitor=_monitors(null))
   {
      monitor->primary=true;
      monitor->full.set(0, App.desktop    ());
      monitor->work=       App.desktopArea() ;
      // get available modes
      MemtN<VecI2, 128> modes;
      if(XF86VidModeGetAllModeLines(XDisplay, DefaultScreen(XDisplay), &vid_modes, &vid_mode))for(int i=0; i<vid_modes; i++)
      {
         XF86VidModeModeInfo &vm=*vid_mode[i]; modes.binaryInclude(VecI2(vm.hdisplay, vm.vdisplay));
      }
      monitor->modes=modes;
   }
#endif
}
/******************************************************************************/
void Display::del()
{
   Gui.del(); // deleting Gui should be outside of '_lock' lock (because for example it can wait for a thread working in the background which is using '_lock')

   SyncLocker locker(_lock);

  _initialized=false;

             gamma(0); // reset gamma when closing app
       VR.delImages();
           ShutFont();
         ShutVtxInd();
  DisplayState::del();
             Sh.del();
       Renderer.del();
         Images.del();
         _modes.del();

#if DX11
   if(SwapChain)SwapChain->SetFullscreenState(false, null); // full screen state must be first disabled, according to http://msdn.microsoft.com/en-us/library/windows/desktop/bb205075(v=vs.85).aspx#Destroying
   RELEASE(SwapChain);
   RELEASE(Output);
   RELEASE(Adapter);
   RELEASE(Factory);
   RELEASE(Query);
   RELEASE(D3DC1);
   RELEASE(D3DC);
   RELEASE(D3D);
#elif GL
   #if IOS
      if(FBO1){glDeleteFramebuffers(1, &FBO1); FBO1=0;}
   #endif
      if(FBO){glDeleteFramebuffers(1, &FBO); FBO=0;}
      if(VAO){glDeleteVertexArrays(1, &VAO); VAO=0;}
      SecondaryContexts.del();
           MainContext .del();
   #if WINDOWS
      if(hDC){ReleaseDC(App.Hwnd(), hDC); hDC=null;}
      SetDisplayMode(0); // switch back to the desktop
   #elif MAC
      [OpenGLContext release]; OpenGLContext=null;
   #elif LINUX
      SetDisplayMode(0); // switch back to the desktop
      if(vid_mode){XFree(vid_mode); vid_mode=null;} vid_modes=0; // free after 'SetDisplayMode'
   #elif ANDROID
      if(GLDisplay){eglTerminate(GLDisplay); GLDisplay=null;}
   #endif
   #if APPLE
      if(OpenGLBundle){CFRelease(OpenGLBundle); OpenGLBundle=null;}
   #endif
#endif
}
/******************************************************************************/
void Display::createDevice()
{
   if(LogInit)LogN("Display.createDevice");
   SyncLocker locker(_lock);
#if WINDOWS || MAC || LINUX
   MemtN<VecI2, 128> modes; FREPA(_monitors) // store display modes for all outputs, in case user prefers to use another monitor rather than the main display
   {
    C Monitor &monitor=_monitors[i];
      FREPA(monitor.modes)modes.binaryInclude(monitor.modes[i]); // add from the start to avoid unnecessary memory moves
   }
  _modes=modes;
#endif

#if DX11
   UInt flags=(D3D_DEBUG ? D3D11_CREATE_DEVICE_DEBUG : 0); // DO NOT include D3D11_CREATE_DEVICE_SINGLETHREADED to allow multi-threaded resource creation - https://docs.microsoft.com/en-us/windows/desktop/direct3d11/overviews-direct3d-11-render-multi-thread

   // ADAPTER = GPU
   // OUTPUT  = MONITOR

   U64 adapter_id=VR._adapter_id;
   if( adapter_id) // if want a custom adapter
   {
      IDXGIFactory1 *factory=null; CreateDXGIFactory1(__uuidof(IDXGIFactory1), (Ptr*)&factory); if(factory)
      {
         for(Int i=0; OK(factory->EnumAdapters(i, &Adapter)); i++)
         {
            if(!Adapter)break;
         #if DEBUG
            IDXGIOutput *output=null; for(Int i=0; OK(Adapter->EnumOutputs(i, &output)); i++)
            {
               DXGI_OUTPUT_DESC desc; output->GetDesc(&desc);
               RELEASE(output);
            }
         #endif
            DXGI_ADAPTER_DESC desc; if(OK(Adapter->GetDesc(&desc)))
            {
               ASSERT(SIZE(desc.AdapterLuid)==SIZE(adapter_id));
               if(EqualMem(&adapter_id, &desc.AdapterLuid, SIZE(adapter_id)))break; // if this is the adapter, then use it and don't look any more
            }
            RELEASE(Adapter);
         }
         RELEASE(factory); // release 'factory' because we need to obtain it from the D3D Device in case it will be different
      }
   }

   D3D_FEATURE_LEVEL *feature_level_force=null;
#if   FORCE_D3D9_3
   D3D_FEATURE_LEVEL fl=D3D_FEATURE_LEVEL_9_3 ; feature_level_force=&fl;
#elif FORCE_D3D10_0
   D3D_FEATURE_LEVEL fl=D3D_FEATURE_LEVEL_10_0; feature_level_force=&fl;
#elif FORCE_D3D10_1
   D3D_FEATURE_LEVEL fl=D3D_FEATURE_LEVEL_10_1; feature_level_force=&fl;
#endif

   if(OK(D3D11CreateDevice(Adapter, Adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, null, flags, feature_level_force, feature_level_force ? 1 : 0, D3D11_SDK_VERSION, &D3D, &FeatureLevel, &D3DC)))
   {
     _can_draw=true;
     _no_gpu  =false;
      if(FeatureLevel<D3D_FEATURE_LEVEL_10_0)Exit("Minimum D3D Feature Level 10.0 required.\nA better GPU is required.");
   }else
   {
     _can_draw=true; // we can still draw on DX10+ by using D3D_DRIVER_TYPE_WARP
     _no_gpu  =true;
      if((App.flag&APP_ALLOW_NO_GPU) ? !OK(D3D11CreateDevice(null, D3D_DRIVER_TYPE_WARP, null, flags, null, 0, D3D11_SDK_VERSION, &D3D, &FeatureLevel, &D3DC)) : true)Exit(MLTC(u"Can't create Direct3D Device.", PL,u"Nie można utworzyć Direct3D."));
      RELEASE(Adapter); // D3D may have gotten a different adapter
   }
   if(D3D_DEBUG)D3D->SetExceptionMode(D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR);

   D3DC->QueryInterface(__uuidof(ID3D11DeviceContext1), (Ptr*)&D3DC1);

  _shader_model=((FeatureLevel>=D3D_FEATURE_LEVEL_12_0) ? SM_6 : (FeatureLevel>=D3D_FEATURE_LEVEL_11_0) ? SM_5 : (FeatureLevel>=D3D_FEATURE_LEVEL_10_1) ? SM_4_1 : SM_4);

   IDXGIDevice1 *device=null; D3D->QueryInterface(__uuidof(IDXGIDevice1), (Ptr*)&device); if(device)
   {
      device->SetMaximumFrameLatency(1); // set max frame latency, for WINDOWS_OLD this doesn't seem to have any effect, however for WINDOWS_NEW it makes a big difference (it makes it work as WINDOWS_OLD), this may be related to the type of SwapChain, so always call this just in case, as without this, the latency is very slow (for example drawing something at mouse position and moving the mouse in circles)
      if(!Adapter)device->GetAdapter(&Adapter); // if adapter is unknown
      RELEASE(device);
   }
   if(!Adapter) // if adapter is unknown
   {
      IDXGIDevice *device=null; D3D->QueryInterface(__uuidof(IDXGIDevice), (Ptr*)&device); if(device)
      {
         device->GetAdapter(&Adapter);
         RELEASE(device);
      }
   }
   if(!Factory) // if Factory is unknown
   {
      if(Adapter)Adapter->GetParent(WINDOWS_OLD ? __uuidof(IDXGIFactory1) : __uuidof(IDXGIFactory2), (Ptr*)&Factory);
      if(!Factory)Exit("Can't access DXGIFactory.\nPlease install latest DirectX and Graphics Drivers.");
   }

   IDXGIFactory5 *factory5=null; Factory->QueryInterface(__uuidof(IDXGIFactory5), (Ptr*)&factory5); if(factory5)
   {
      int allow_tearing=false; // must be 'int' because 'bool' will fail
		if(OK(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, SIZE(allow_tearing))))AllowTearing=(allow_tearing!=0);
      factory5->Release();
   }

   if(Adapter)
   {
      DXGI_ADAPTER_DESC desc; if(OK(Adapter->GetDesc(&desc)))
      {
         if(!deviceName().is())_device_name=desc.Description;
        _device_mem=desc.DedicatedVideoMemory;
      }
   }

   // init
   if(!findMode())Exit("Valid display mode not found.");
#if WINDOWS_OLD
   if(!exclusive() && full()){if(!SetDisplayMode(2))Exit("Can't set fullscreen mode."); adjustWindow();}
again:
   Factory->CreateSwapChain(D3D, &SwapChainDesc, &SwapChain);
   if(!SwapChain)
   {
      if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R32G32B32A32_FLOAT ){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT ; goto again;} // if failed with 32-bit then try again with 16-bit
      if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT ){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; goto again;} // if failed with 16-bit then try again with  8-bit
      if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R10G10B10A2_UNORM  ){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM     ; goto again;} // if failed with 10-bit then try again with  8-bit
      if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM     ; goto again;} // #SwapFlipSRGB may fail to create sRGB in that case create as linear and 'swapRTV' in 'ImageRT.map'
   }
   Factory->MakeWindowAssociation(App.Hwnd(), DXGI_MWA_NO_ALT_ENTER|DXGI_MWA_NO_WINDOW_CHANGES|DXGI_MWA_NO_PRINT_SCREEN); // this needs to be called after 'CreateSwapChain'
#else
again:
	Factory->CreateSwapChainForCoreWindow(D3D, (IUnknown*)App._hwnd, &SwapChainDesc, null, &SwapChain);
   if(!SwapChain)
   {
      if(SwapChainDesc.Format==DXGI_FORMAT_R32G32B32A32_FLOAT ){SwapChainDesc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT ; goto again;} // if failed with 32-bit then try again with 16-bit
      if(SwapChainDesc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT ){SwapChainDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; goto again;} // if failed with 16-bit then try again with  8-bit
      if(SwapChainDesc.Format==DXGI_FORMAT_R10G10B10A2_UNORM  ){SwapChainDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM     ; goto again;} // if failed with 10-bit then try again with  8-bit
      if(SwapChainDesc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB){SwapChainDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM     ; goto again;} // #SwapFlipSRGB may fail to create sRGB in that case create as linear and 'swapRTV' in 'ImageRT.map'
   }
#endif
   if(!SwapChain)Exit("Can't create Direct3D Swap Chain.");
 //if( SwapChain && Output && !SwapChainDesc.Windowed)SwapChain->SetFullscreenState(true, Output); // if we want a custom output then we need to apply it now, otherwise the fullscreen can occur on the main display

   D3D11_QUERY_DESC query_desc={D3D11_QUERY_EVENT, 0};
   D3D->CreateQuery(&query_desc, &Query);
#elif GL
   if(FlagTest(App.flag, APP_ALLOW_NO_GPU)) // completely disable hardware on OpenGL because there's no way to know if it's available
   {
     _can_draw=false;
     _no_gpu  =true;
   }else
   {
     _can_draw=true;
     _no_gpu  =false;
   }

   const VecB2 ctx_vers[]={{3,2}, {4,0}}; // set highest at the end, 4.0 needed for 'TexGather', 3.2 needed for 'glDrawElementsBaseVertex', 3.1 needed for instancing, 3.0 needed for 'glColorMaski', 'gl_ClipDistance', 'glClearBufferfv', 'glGenVertexArrays', 'glMapBufferRange'

   #if WINDOWS
      PIXELFORMATDESCRIPTOR pfd=
      {
         SIZE(PIXELFORMATDESCRIPTOR), // size of 'pfd'
         1, // version number
         PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
         PFD_TYPE_RGBA,
         32, // color bits
         0, 0, 0, 0, 0, 0, // color bits ignored
         0, // no alpha buffer
         0, // shift bit ignored
         0, // no accumulation buffer
         0, 0, 0, 0, // accumulation bits ignored
         24, // 24-bit depth buffer
         8, // 8-bit stencil buffer
         0, // no auxiliary buffer
         PFD_MAIN_PLANE, // main drawing layer
         0, // reserved
         0, 0, 0 // layer masks ignored
      };
      Int PixelFormat;

      if(!(hDC                =            GetDC(App.Hwnd()            )))Exit("Can't create an OpenGL Device Context.");
      if(!(PixelFormat        =ChoosePixelFormat(hDC,              &pfd)))Exit("Can't find a suitable PixelFormat.");
      if(!(                       SetPixelFormat(hDC, PixelFormat, &pfd)))Exit("Can't set the PixelFormat.");
      if(!(MainContext.context= wglCreateContext(hDC                   )))Exit("Can't create an OpenGL Context.");
           MainContext.lock();

      if(glewInit()!=GLEW_OK)Exit("Can't init OpenGL");
         glewSafe();

   #if LINEAR_GAMMA
      if(wglChoosePixelFormatARB)
      {
         const int pf_attribs[]=
         {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB , GL_TRUE,
            WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, LINEAR_GAMMA,
          //WGL_COLORSPACE_EXT, LINEAR_GAMMA ? WGL_COLORSPACE_SRGB_EXT : WGL_COLORSPACE_LINEAR_EXT, fails
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB  , 32,
            WGL_DEPTH_BITS_ARB  , 24,
            WGL_STENCIL_BITS_ARB,  8,
            NULL // end of list
         };
         int  pixel_formats[1]; // just need the first one
         UINT numFormatsAvailable=0;
         if(wglChoosePixelFormatARB(hDC, pf_attribs, null, Elms(pixel_formats), pixel_formats, &numFormatsAvailable))
         if(numFormatsAvailable){Bool ok=SetPixelFormat(hDC, pixel_formats[0], &pfd); DEBUG_ASSERT(ok, "SetPixelFormat failed");}
      }
   #endif
      {
         // in tests 'wglCreateContext' returned highest possible context, however do these checks just in case
         VecI2 ver=glVer(); if(Compare(ver, (VecI2)ctx_vers[Elms(ctx_vers)-1])<0 && wglCreateContextAttribsARB) // if it's smaller than highest needed
            REPA(ctx_vers) // go from the end, to try highest first
         {
            VecI2 v=ctx_vers[i]; if(Compare(v, ver)<=0)break; // if <= than what we already have then stop
            const int attribs[]={WGL_CONTEXT_MAJOR_VERSION_ARB, v.x,
                                 WGL_CONTEXT_MINOR_VERSION_ARB, v.y,
                                 NULL}; // end of list
            if(HGLRC context=wglCreateContextAttribsARB(hDC, 0, attribs))
            {
               MainContext.del();
               MainContext.context=context;
               MainContext.lock();
               break; // stop on first found
            }
         }
      }
   #elif MAC
      OpenGLBundle=CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));

      const CGLPixelFormatAttribute profile_versions[]=
      {
         (CGLPixelFormatAttribute)kCGLOGLPVersion_Legacy  , // NSOpenGLProfileVersionLegacy
         (CGLPixelFormatAttribute)kCGLOGLPVersion_GL3_Core, // NSOpenGLProfileVersion3_2Core
         (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core, // NSOpenGLProfileVersion4_1Core
      };
      CGLPixelFormatObj pf=null;
      REPD (hw , 2) // HW acceleration, most important !! it's very important to check it first, in case device supports only 3.2 accelerated, and 4.1 perhaps could be done in software (if that's possible, it's very likely as one user with Intel HD 3300 which has 3.3 GL, reported poor performance without this) so first we check all profiles looking for accelerated, and if none are found, then try software !!
      REPAD(ver, profile_versions) // profile version
      REPD (buf, 2)
      {
         const CGLPixelFormatAttribute attribs[]=
         {
            buf ? kCGLPFATripleBuffer : kCGLPFADoubleBuffer, // triple/double buffered
            kCGLPFADepthSize  , (CGLPixelFormatAttribute)24, // depth buffer
            kCGLPFAStencilSize, (CGLPixelFormatAttribute) 8, // stencil
            kCGLPFAOpenGLProfile, profile_versions[ver], // version
            hw ? kCGLPFAAccelerated : kCGLPFAAllowOfflineRenderers, // HW/Soft
            (CGLPixelFormatAttribute)NULL // end of list
         };
         GLint num_pixel_formats; CGLChoosePixelFormat(attribs, &pf, &num_pixel_formats); if(pf)goto found_pf;
      }
      Exit("Can't create an OpenGL Pixel Format.");
   found_pf:
      CGLCreateContext(pf, null, &MainContext.context);
      CGLDestroyPixelFormat(pf);
      if(!MainContext.context)Exit("Can't create an OpenGL Context.");
      if(MAC_GL_MT)Bool mt_ok=(CGLEnable(MainContext.context, kCGLCEMPEngine)!=kCGLNoError);
      MainContext.lock();
      OpenGLContext=[[NSOpenGLContext alloc] initWithCGLContextObj:MainContext.context];
      [OpenGLContext setView:OpenGLView];
      [App.Hwnd() makeKeyAndOrderFront:NSApp]; // show only after everything finished (including GL context to avoid any flickering)
   #elif LINUX
      if(XDisplay)
      {
      #if 0 // 2.0 context
         if(!(MainContext.context=glXCreateNewContext(XDisplay, GLConfig, GLX_RGBA_TYPE, null, true)))Exit("Can't create a OpenGL Context.");
      #else // 3.0+ context (this does not link on some old graphics drivers when compiling, "undefined reference to glXCreateContextAttribsARB", it would need to be accessed using 'glGetProcAddress')
         // access 'glXCreateContextAttribsARB', on Linux we don't need an existing GL context to be able to load extensions via 'glGetProcAddress'
         typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSARBPROC) (::Display* dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list);
         if(PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB=(PFNGLXCREATECONTEXTATTRIBSARBPROC)glGetProcAddress("glXCreateContextAttribsARB"))
         {
            // in tests 'glXCreateContextAttribsARB' returned higher version than what was requested (which is what we want)
            REPA(ctx_vers) // go from the end, to try highest first
            {
               const int attribs[]=
               {
                  GLX_CONTEXT_MAJOR_VERSION_ARB, ctx_vers[i].x,
                  GLX_CONTEXT_MINOR_VERSION_ARB, ctx_vers[i].y,
                  NULL // end of list
               };
               // create context
               if(MainContext.context=glXCreateContextAttribsARB(XDisplay, GLConfig, null, true, attribs))break; // stop on first found
            }
         }
      #endif
         if(!MainContext.context)Exit("Can't create a OpenGL 3.2 Context.");
         XSync(XDisplay, false); // Forcibly wait on any resulting X errors
         MainContext.lock();
         glXSwapInterval=(glXSwapIntervalType)glGetProcAddress("glXSwapIntervalEXT"); // access it via 'glGetProcAddress' because some people have linker errors "undefined reference to 'glXSwapIntervalEXT'
      }else
      {
        _can_draw=false;
        _no_gpu  =true;
         return;
      }
   #elif ANDROID
      if(LogInit)LogN("EGL");
      GLDisplay=eglGetDisplay(EGL_DEFAULT_DISPLAY); if(!GLDisplay)Exit("Can't get EGL Display"); if(eglInitialize(GLDisplay, null, null)!=EGL_TRUE)Exit("Can't initialize EGL Display");
      Byte samples=1; IMAGE_TYPE ds_type=IMAGE_NONE;
      EGLint win_attribs[]=
      {
      #if LINEAR_GAMMA
         EGL_GL_COLORSPACE_KHR, LINEAR_GAMMA ? EGL_GL_COLORSPACE_SRGB_KHR : EGL_GL_COLORSPACE_LINEAR_KHR,
      #endif
         EGL_NONE // end of list
      };
      for(GLCtxVer=3; GLCtxVer>=3; GLCtxVer--) // start from OpenGL ES 3
      {
         EGLint ctx_attribs[]=
         {
            EGL_CONTEXT_CLIENT_VERSION, GLCtxVer,
            EGL_NONE // end of list
         };
         FREPD(d, 3) // depth   - process this with priority #1
         FREPD(s, 2) // stencil - process this with priority #2
         {
            ds_type=((d==0) ? ((s==0) ? IMAGE_D24S8 : IMAGE_D24X8) : (d==1) ? IMAGE_D32 : IMAGE_D16);
            EGLint attribs[]=
            {
               EGL_SURFACE_TYPE   , EGL_WINDOW_BIT,
               EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, // depends on 'GLCtxVer'
               EGL_BLUE_SIZE      , 8,
               EGL_GREEN_SIZE     , 8,
               EGL_RED_SIZE       , 8,
               EGL_ALPHA_SIZE     , 8,
               EGL_DEPTH_SIZE     , (d==0) ? 24 : (d==1) ? 32 : 16,
               EGL_STENCIL_SIZE   , (s==0) ?  8 : 0,
               EGL_NONE // end of list
            };
            if(LogInit)LogN(S+"Trying config GL:"+GLCtxVer+", D:"+d+", S:"+s);
            EGLint num_configs=0;
            if(eglChooseConfig(GLDisplay, attribs, &GLConfig, 1, &num_configs)==EGL_TRUE)
               if(num_configs>=1)
            {
               EGLint format; eglGetConfigAttrib(GLDisplay, GLConfig, EGL_NATIVE_VISUAL_ID, &format);
               ANativeWindow_setBuffersGeometry(AndroidApp->window, 0, 0, format);
               if(MainContext.surface=eglCreateWindowSurface(GLDisplay, GLConfig, AndroidApp->window, win_attribs))
               {
                  if(MainContext.context=eglCreateContext(GLDisplay, GLConfig, null, ctx_attribs))goto context_ok;
                  MainContext.del();
               }
            }
         }
      }
      Exit("Can't create an OpenGL Context.");
   context_ok:
      MainContext.lock();
      if(LogInit)LogN("EGL OK");
      EGLint width, height;
      eglQuerySurface(GLDisplay, MainContext.surface, EGL_WIDTH , &width );
      eglQuerySurface(GLDisplay, MainContext.surface, EGL_HEIGHT, &height);
      Renderer._main   .forceInfo(width, height, 1, LINEAR_GAMMA ? IMAGE_R8G8B8A8_SRGB : IMAGE_R8G8B8A8, IMAGE_GL_RB, samples);
      Renderer._main_ds.forceInfo(width, height, 1, ds_type                                            , IMAGE_GL_RB, samples);
      if(LogInit)LogN(S+"Renderer._main: "+Renderer._main.w()+'x'+Renderer._main.h()+", type: "+Renderer._main.hwTypeInfo().name+", ds_type: "+Renderer._main_ds.hwTypeInfo().name);
   #elif IOS
      OpenGLBundle=CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengles"));

      MainContext.context=[[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3]; if(!MainContext.context)Exit("Can't create an OpenGL Context.");
      MainContext.context.multiThreaded=false; // disable multi-threaded rendering as enabled actually made things slower, TOOD: check again in the future !! if enabling then probably all contexts have to be enabled as well, secondary too, because VAO from VBO's on a secondary thread could fail, as in Dungeon Hero, needs checking !!
      MainContext.lock();
   #elif WEB
      EmscriptenWebGLContextAttributes attrs;
      emscripten_webgl_init_context_attributes(&attrs);
      attrs.minorVersion=0;
      attrs.alpha       =false; // this would enable compositing graphics using transparency onto the web page
      attrs.depth       =true;
      attrs.stencil     =true;
      attrs.antialias   =false;
      attrs.preserveDrawingBuffer=false;
      attrs.enableExtensionsByDefault=true;
      attrs.preferLowPowerToHighPerformance=false;
      for(Int GLCtxVer=2; GLCtxVer>=2; GLCtxVer--) // start from WebGL 2.0 (ES3)
      {
         attrs.majorVersion=GLCtxVer;
         if(MainContext.context=emscripten_webgl_create_context(null, &attrs))goto context_ok;
      }
      Exit("Can't create an OpenGL Context.");
   context_ok:
      MainContext.lock();
      Byte samples=(attrs.antialias ? 4 : 1);
      int  width, height; emscripten_get_canvas_element_size(null, &width, &height);
      Renderer._main   .forceInfo(width, height, 1,/*LINEAR_GAMMA  ? IMAGE_R8G8B8A8_SRGB :*/IMAGE_R8G8B8A8, IMAGE_GL_RB, samples); // #WebSRGB currently web doesn't support sRGB SwapChain
      Renderer._main_ds.forceInfo(width, height, 1,  attrs.stencil ? IMAGE_D24S8         :  IMAGE_D24X8   , IMAGE_GL_RB, samples);
   #endif

      VecI2 ver=glVer();
   #if GL_ES
      if(Compare(ver, VecI2(3, 2))>=0)_shader_model=SM_GL_ES_3_2;else
      if(Compare(ver, VecI2(3, 1))>=0)_shader_model=SM_GL_ES_3_1;else
      if(Compare(ver, VecI2(3, 0))>=0)_shader_model=SM_GL_ES_3  ;else
                                       Exit("OpenGL ES 3.0 support not available.\nGraphics Driver not installed or better video card is required.");
   #else
      if(Compare(ver, VecI2(4, 0))>=0)_shader_model=SM_GL_4;else
      if(Compare(ver, VecI2(3, 2))>=0)_shader_model=SM_GL_3;else
                                       Exit("OpenGL 3.2 support not available.\nGraphics Driver not installed or better video card is required.");
   #endif

   if(!deviceName().is())
   {
     _device_name=(CChar8*)glGetString(GL_RENDERER);
   #if LINUX
     _device_name.removeOuterWhiteChars(); // Linux may have unnecessary spaces at the end
   #endif
   }

   if(LogInit)LogN("Secondary Contexts");
   if(SecondaryContexts.elms())
   {
      REPA(SecondaryContexts)if(!SecondaryContexts[i].createSecondary())
      {
         LogN(S+"Failed to create a Secondary OpenGL Context"
         #if ANDROID
            +", error:"+eglGetError()
         #endif
         );
         SecondaryContexts.remove(i); // remove after error code was displayed
      }
      MainContext.lock(); // lock main context because secondary were locked during creation to set some things
   }
   if(LogInit)LogN("Secondary Contexts OK");

   if(LogInit)
   {
      LogN(S+"Device Name: "      +_device_name);
      LogN(S+"Device Vendor: "    +(CChar8*)glGetString(GL_VENDOR    ));
      LogN(S+"Device Version: "   +(CChar8*)glGetString(GL_VERSION   ));
      LogN(S+"Device Extensions: "+(CChar8*)glGetString(GL_EXTENSIONS));
   }

   if(!findMode())Exit("Valid display mode not found.");
   setSync();

   if(LogInit)LogN("FBO");
#if LINEAR_GAMMA
   #ifdef        GL_FRAMEBUFFER_SRGB
        glEnable(GL_FRAMEBUFFER_SRGB);
   #elif defined GL_FRAMEBUFFER_SRGB_EXT
        glEnable(GL_FRAMEBUFFER_SRGB_EXT);
   #endif
#endif
   glGenFramebuffers(1, &FBO); if(!FBO)Exit("Couldn't create OpenGL Frame Buffer Object (FBO)");
   glGenVertexArrays(1, &VAO); if(!VAO)Exit("Couldn't create OpenGL Vertex Arrays (VAO)");

	#if WINDOWS
      if(full()){if(!SetDisplayMode(2))Exit("Can't set fullscreen mode."); adjustWindow();}
   #elif MAC
      if(!SetDisplayMode(2))Exit("Can't set display mode.");
   #elif LINUX
      if(full()){if(!SetDisplayMode(2))Exit("Can't set display mode."); adjustWindow();} // 'adjustWindow' because we need to set fullscreen state
   #elif IOS
      glGenFramebuffers(1, &FBO1); if(!FBO1)Exit("Couldn't create OpenGL Frame Buffer Object (FBO)");
      fbo(FBO); // set custom frame buffer, on iOS there's only one FBO and one FBO change, and it is here, this is because there's no default(0) FBO on this platform
   #endif

   // call these as soon as possible because they affect all images (including those created in the renderer)
	glPixelStorei(GL_PACK_ALIGNMENT  , 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#endif

#if MOBILE
   T._modes.setNum(2);
   T._modes[1]=T._modes[0]=screen();
   T._modes[1].swap();
#endif
}
void Display::androidClose()
{
#if ANDROID
   SyncLocker locker(_lock);
	if(GLDisplay)
	{
         MainContext.unlock();
      if(MainContext.surface)eglDestroySurface(GLDisplay, MainContext.surface);
	}
   MainContext.surface=null;
#endif
}
void Display::androidOpen()
{
#if ANDROID
   SyncLocker locker(_lock);
   androidClose();
   if(GLDisplay && MainContext.context)
   {
      EGLint win_attribs[]=
      {
      #if LINEAR_GAMMA
         EGL_GL_COLORSPACE_KHR, LINEAR_GAMMA ? EGL_GL_COLORSPACE_SRGB_KHR : EGL_GL_COLORSPACE_LINEAR_KHR,
      #endif
         EGL_NONE // end of list
      };
      EGLint format; eglGetConfigAttrib(GLDisplay, GLConfig, EGL_NATIVE_VISUAL_ID, &format);
      ANativeWindow_setBuffersGeometry(AndroidApp->window, 0, 0, format);
      MainContext.surface=eglCreateWindowSurface(GLDisplay, GLConfig, AndroidApp->window, win_attribs); if(!MainContext.surface)Exit("Can't create EGLSurface.");
      MainContext.lock();
   }else Exit("OpenGL Display and MainContext not available.");
#endif
}
Bool Display::create()
{
if(LogInit)LogN("Display.create");
           createDevice();
               getGamma();
                getCaps();
      Sh.createSamplers();
   DisplayState::create();
              Sh.create();
_linear_gamma^=1; linearGamma(!_linear_gamma); // set after loading shaders
             InitMatrix(); // !! call this after creating main shaders, because it creates the "ObjMatrix, ObjMatrixPrev" shader buffers !!
  if(!Renderer.rtCreate())Exit("Can't create Render Targets."); // !! call this after creating shaders because it modifies shader values !!
           viewRect(null); // reset full viewport in case user made some changes to view rect in 'InitPre' which would be actually invalid since resolutions were not yet known
             InitVtxInd();
        Renderer.create();
                 envMap(ImagePtr().get("Img/Environment.img"));
           colorPalette(ImagePtr().get("Img/Color Palette.img"));
        VR.createImages(); // !! call this before 'after', because VR gui image may affect aspect ratio of display !!
                  after(false);
             Gui.create();

   // set default settings
   {Byte v=texFilter   (); _tex_filter    ^=1               ; texFilter   (v);}
   {Bool v=texMipFilter(); _tex_mip_filter^=1               ; texMipFilter(v);}
   {Bool v=bloomMaximum(); _bloom_max      =false           ; bloomMaximum(v);} // resetting will load shaders
   {auto v=edgeSoften  (); _edge_soften    =EDGE_SOFTEN_NONE; edgeSoften  (v);} // resetting will load shaders
   {auto v=edgeDetect  (); _edge_detect    =EDGE_DETECT_NONE; edgeDetect  (v);} // resetting will load shaders
   {auto v=tAA         (); _taa            =false           ; tAA         (v);} // resetting will load shaders
   {Flt  v=grassRange  (); _grass_range    =-1              ; grassRange  (v);}
   lod            (_lod_factor, _lod_factor_mirror);
   shadowJitterSet();
   shadowRangeSet ();
   SetMatrix      ();

  _initialized=true;

   return true;
}
/******************************************************************************/
Bool Display::created()
{
#if DX11
   return D3DC!=null;
#elif GL
   return MainContext.is();
#endif
}
/******************************************************************************/
void ThreadMayUseGPUData()
{
#if GL && HAS_THREADS
   Ptr context=GetCurrentContext();
   if(!context)
   {
      ContextLock.on();
      for(;;)
      {
         REPA(SecondaryContexts)if(!SecondaryContexts[i].locked)
         {
            SecondaryContexts[i].lock();
            goto context_locked;
         }
         if(!SecondaryContexts.elms())Exit("No secondary OpenGL contexts have been created");
         ContextLock.off(); ContextUnlocked.wait(); // wait until any other context is unlocked
         ContextLock.on ();
      }
   context_locked:
      ContextLock.off();
   }
#endif   
}
void ThreadFinishedUsingGPUData()
{
#if GL && HAS_THREADS
   if(Ptr context=GetCurrentContext())
   {
      ContextLock.on();
      REPA(SecondaryContexts)if(SecondaryContexts[i].context==context)
      {
         SecondaryContexts[i].unlock();
         goto context_unlocked;
      }
   context_unlocked:
      ContextLock.off();
      ContextUnlocked++; // notify of unlocking
   }
#endif   
}
/******************************************************************************/
static Int DisplaySamples(Int samples)
{
   Clamp(samples, 1, 16);
   if(Renderer.anyDeferred() && D.deferredMSUnavailable())samples=1;
#if DX11
   if(samples>1)samples=4; // only 4 samples are supported in DX10+ implementation
#else
   samples=1; // other implementations don't support multi-sampling
#endif
   return samples;
}
#if DX11
static DXGI_FORMAT SwapChainFormat()
{
   if(GDI_COMPATIBLE)return LINEAR_GAMMA ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
#if 0 // possibly this check is no longer needed, it's possible Windows supports windowed HDR natively now on HDR capable monitors
   if(full() && exclusive()) // on Windows we need fullscreen and exclusive to be able to really enable it, without it, it will be only 8-bit
#endif
   {
   #if LINEAR_GAMMA
      if(D.monitorPrecision()>IMAGE_PRECISION_16)return DXGI_FORMAT_R32G32B32A32_FLOAT;
      if(D.monitorPrecision()>IMAGE_PRECISION_8 )return DXGI_FORMAT_R16G16B16A16_FLOAT;
      // can't use DXGI_FORMAT_R10G10B10A2_UNORM because it's non-sRGB
   #else
      // can't use DXGI_FORMAT_R32G32B32A32_FLOAT DXGI_FORMAT_R16G16B16A16_FLOAT because on Windows it means linear gamma
      if(D.monitorPrecision()>IMAGE_PRECISION_8 )return DXGI_FORMAT_R10G10B10A2_UNORM;
   #endif
   }
   return LINEAR_GAMMA ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
}
#endif
Bool Display::findMode()
{
   SyncLocker locker(_lock);

#if WINDOWS_NEW // on WindowsNew we can't change mode here, we need to set according to what we've got, instead 'RequestDisplayMode' can be called
  _res =WindowSize(true);
  _full=App.Fullscreen();
#elif IOS
   // '_res' will be set in 'mapMain'
#elif MOBILE || WEB
   // Renderer._main is already available
  _res=Renderer._main.size();
#else
   RectI full, work; VecI2 max_normal_win_client_size, maximized_win_client_size;
    getMonitor(full, work, max_normal_win_client_size, maximized_win_client_size);
   if(resW()>=full.w() && resH()>=full.h())_full=true; // force fullscreen only if both dimensions are equal-bigger because on Windows it's perfectly fine to have a window as wide as the whole desktop
   if(D.full())
   {
      Int   nearest=-1; Int desired_area=res().mul(), area_error;
      auto  monitor=getMonitor();
      auto &modes  =((monitor && monitor->modes.elms()) ? monitor->modes : _modes); // use monitor modes if available
      REPA( modes)
      {
       C VecI2 &mode=modes[i];
         if(mode==res()){nearest=i; break;} // exact match
         Int ae=Abs(mode.mul()-desired_area);
         if(nearest<0 || ae<area_error){nearest=i; area_error=ae;}
      }
      if(nearest<0)return false;
     _res=modes[nearest];
   }else
   {
      if(resW()>=Min(maximized_win_client_size.x, max_normal_win_client_size.x+1)
      && resH()>=Min(maximized_win_client_size.y, max_normal_win_client_size.y+1))_res=maximized_win_client_size;/*else
      {  don't limit for 2 reasons: 1) having multiple monitors we can make the window cover multiple monitors 2) on Windows initially we can drag the window only until 'max_normal_win_client_size', however after that we can drag again to make it bigger, and this time it will succeed, which means that making windows bigger than 'max_normal_win_client_size' is actually possible, unless it's a bug in the OS
         MIN(_res.x, max_normal_win_client_size.x);
         MIN(_res.y, max_normal_win_client_size.y);
      }*/
   }
#endif

#if DX11
   Zero(SwapChainDesc);
   Bool sync=ActualSync();
   #if WINDOWS_OLD
      SwapChainDesc.OutputWindow      =App.Hwnd();
      SwapChainDesc.Windowed          =(!exclusive() || !T.full());
      SwapChainDesc.BufferCount       =(sync ? 3 : 2); // if we're rendering to VR display, then it has its own swap chain, and it handles the most intense rendering, so we don't need to have more buffers here, so keep it low to reduce memory usage
      SwapChainDesc.BufferDesc.Width  =resW();
      SwapChainDesc.BufferDesc.Height =resH();
      // !! using HDR may require 'DXGI_SWAP_EFFECT_FLIP_DISCARD' for non-exclusive !!
      SwapChainDesc.BufferDesc.Format =SwapChainFormat();
      SwapChainDesc.SampleDesc.Count  =1;
      SwapChainDesc.SampleDesc.Quality=0;
      SwapChainDesc.BufferUsage       =DXGI_USAGE_RENDER_TARGET_OUTPUT|DXGI_USAGE_SHADER_INPUT|DXGI_USAGE_BACK_BUFFER;
      SwapChainDesc.Flags             =DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH|(GDI_COMPATIBLE ? DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE : 0);
      if(_freq_want && !SwapChainDesc.Windowed) // set custom frequency only if desired and in true full-screen
      {
         SwapChainDesc.BufferDesc.RefreshRate.Numerator  =_freq_want;
         SwapChainDesc.BufferDesc.RefreshRate.Denominator=1;
      }

      SwapChainDesc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
   #if !GDI_COMPATIBLE // flip modes are incompatible with GDI
      {
         VecI4 ver=OSVerNumber();
         if(        ver.x>=10                 )SwapChainDesc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD   ;else // DXGI_SWAP_EFFECT_FLIP_DISCARD    is available on Windows 10
         if(Compare(ver, VecI4(6, 2, 0, 0))>=0)SwapChainDesc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;     // DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL is available on Windows 8 - https://msdn.microsoft.com/en-us/library/windows/desktop/bb173077(v=vs.85).aspx
      }
   #endif
      if(AllowTearing && SwapChainDesc.SwapEffect==DXGI_SWAP_EFFECT_FLIP_DISCARD)SwapChainDesc.Flags|=DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
      PresentFlags=((!sync && (SwapChainDesc.Flags&DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) && SwapChainDesc.Windowed) ? DXGI_PRESENT_ALLOW_TEARING : 0); // according to docs, we can use DXGI_PRESENT_ALLOW_TEARING only with DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING and in windowed mode - https://msdn.microsoft.com/en-us/library/windows/desktop/bb509554(v=vs.85).aspx

      SwapChainDesc.BufferDesc.Scaling=DXGI_MODE_SCALING_UNSPECIFIED; // can't always use 'DXGI_MODE_SCALING_STRETCHED' because it will fail to set highest supported resolution by the monitor, because that resolution never supports streching, so we set DXGI_MODE_SCALING_UNSPECIFIED which means it will be set according to the driver settings
      if(1 // find the monitor/output that we're going to use, and iterate all of its modes to check if that mode supports stretched mode
      && !SwapChainDesc.Windowed) // needed for exclusive fullscreen only and only when the driver has scaling set to DXGI_MODE_SCALING_CENTERED
      {
         IDXGIOutput *output;
         Bool         ok=false;
         if(SwapChain) // if we already have a swap chain, then reuse its output
         {
            output=null; SwapChain->GetContainingOutput(&output); if(output){ok=true; goto has_output;} // set 'ok' so break will be called
         }
         if(Adapter)
            if(HMONITOR monitor=MonitorFromWindow(App.Hwnd(), MONITOR_DEFAULTTONEAREST)) // get nearest monitor
               for(Int i=0; ; i++) // iterate all outputs
         {
            output=null; Adapter->EnumOutputs(i, &output); if(output)
            {
               DXGI_OUTPUT_DESC desc; ok=(OK(output->GetDesc(&desc)) && desc.Monitor==monitor); // if found the monitor that we're going to use
               if(ok)
               {
               has_output:
                  DXGI_FORMAT                                          mode=DXGI_FORMAT_R8G8B8A8_UNORM; // always use this mode in case system doesn't support 10-bit color
                  UInt                                           descs_elms=0; output->GetDisplayModeList(mode, 0, &descs_elms, null); // get number of mode descs
                  MemtN<DXGI_MODE_DESC, 128> descs; descs.setNum(descs_elms ); output->GetDisplayModeList(mode, 0, &descs_elms, descs.data()); // get mode descs
                  FREPA(descs)
                  {
                   C DXGI_MODE_DESC &mode=descs[i];
                     if(mode.Width==resW() && mode.Height==resH() && mode.Scaling!=DXGI_MODE_SCALING_UNSPECIFIED) // can't just check for ==DXGI_MODE_SCALING_STRETCHED because it's never listed, however DXGI_MODE_SCALING_CENTERED will be listed for modes that support stretching, so we use !=DXGI_MODE_SCALING_UNSPECIFIED to support both DXGI_MODE_SCALING_STRETCHED and DXGI_MODE_SCALING_CENTERED
                     {
                        SwapChainDesc.BufferDesc.Scaling=DXGI_MODE_SCALING_STRETCHED;
                        break;
                     }
                  }
               }
               output->Release();
               if(ok)break;
            }else break;
         }
      }
   #else // WINDOWS_NEW
      SwapChainDesc.Width =resW();
      SwapChainDesc.Height=resH();
      SwapChainDesc.Format=SwapChainFormat();
      SwapChainDesc.Stereo=false;
      SwapChainDesc.SampleDesc.Count  =1;
      SwapChainDesc.SampleDesc.Quality=0;
      SwapChainDesc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT|DXGI_USAGE_SHADER_INPUT|DXGI_USAGE_BACK_BUFFER;
      SwapChainDesc.BufferCount=(sync ? 3 : 2); // if we're rendering to VR display, then it has its own swap chain, and it handles the most intense rendering, so we don't need to have more buffers here, so keep it low to reduce memory usage
      SwapChainDesc.Scaling    =DXGI_SCALING_STRETCH;
      SwapChainDesc.SwapEffect =DXGI_SWAP_EFFECT_FLIP_DISCARD;
      SwapChainDesc.AlphaMode  =DXGI_ALPHA_MODE_IGNORE;
      SwapChainDesc.Flags      =0;
      if(AllowTearing && SwapChainDesc.SwapEffect==DXGI_SWAP_EFFECT_FLIP_DISCARD)SwapChainDesc.Flags|=DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
      PresentFlags=((!sync && (SwapChainDesc.Flags&DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)/* && SwapChainDesc.Windowed*/) ? DXGI_PRESENT_ALLOW_TEARING : 0); // according to docs, we can use DXGI_PRESENT_ALLOW_TEARING only with DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING and in windowed mode, in UWP there's no windowed check because "UWP apps that enter fullscreen mode by calling Windows::UI::ViewManagement::ApplicationView::TryEnterFullscreen() are fullscreen borderless windows and may use the flag" - https://msdn.microsoft.com/en-us/library/windows/desktop/bb509554(v=vs.85).aspx
   #endif
#endif
   densityUpdate();
   return true;
}
/******************************************************************************/
CChar8* Display::AsText(RESET_RESULT result)
{
   switch(result)
   {
      case RESET_OK                  : return "RESET_OK";
      case RESET_DEVICE_NOT_CREATED  : return "RESET_DEVICE_NOT_CREATED";
      case RESET_DEVICE_RESET_FAILED : return "RESET_DEVICE_RESET_FAILED";
      case RESET_RENDER_TARGET_FAILED: return "RESET_RENDER_TARGET_FAILED";
      default                        : return "RESET_UNKNOWN";
   }
}
void Display::ResetFailed(RESET_RESULT New, RESET_RESULT old)
{
   Exit(
     ((New==old) ? S+"Can't set display mode: "+AsText(New)
                 : S+"Can't set new display mode: "+AsText(New)
                  +"\nCan't set old display mode: "+AsText(old))
       );
}

#if DX11
static Bool ResizeTarget()
{
#if WINDOWS_OLD
again:
   if(OK(SwapChain->ResizeTarget(&SwapChainDesc.BufferDesc)))return true;
   if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R32G32B32A32_FLOAT ){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT ; goto again;} // if failed with 32-bit then try again with 16-bit
   if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT ){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; goto again;} // if failed with 16-bit then try again with  8-bit
   if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R10G10B10A2_UNORM  ){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM     ; goto again;} // if failed with 10-bit then try again with  8-bit
   if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM     ; goto again;} // #SwapFlipSRGB may fail to create sRGB in that case create as linear and 'swapRTV' in 'ImageRT.map'
#endif
   return false;
}
static Bool ResizeBuffers()
{
again:
#if WINDOWS_OLD
   if(OK(SwapChain->ResizeBuffers(SwapChainDesc.BufferCount, SwapChainDesc.BufferDesc.Width, SwapChainDesc.BufferDesc.Height, SwapChainDesc.BufferDesc.Format, SwapChainDesc.Flags)))return true;
   if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R32G32B32A32_FLOAT ){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT ; goto again;} // if failed with 32-bit then try again with 16-bit
   if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT ){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; goto again;} // if failed with 16-bit then try again with  8-bit
   if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R10G10B10A2_UNORM  ){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM     ; goto again;} // if failed with 10-bit then try again with  8-bit
   if(SwapChainDesc.BufferDesc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB){SwapChainDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM     ; goto again;} // #SwapFlipSRGB may fail to create sRGB in that case create as linear and 'swapRTV' in 'ImageRT.map'
#else
   if(OK(SwapChain->ResizeBuffers(SwapChainDesc.BufferCount, SwapChainDesc.Width, SwapChainDesc.Height, SwapChainDesc.Format, SwapChainDesc.Flags)))return true;
   if(SwapChainDesc.Format==DXGI_FORMAT_R32G32B32A32_FLOAT ){SwapChainDesc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT ; goto again;} // if failed with 32-bit then try again with 16-bit
   if(SwapChainDesc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT ){SwapChainDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; goto again;} // if failed with 16-bit then try again with  8-bit
   if(SwapChainDesc.Format==DXGI_FORMAT_R10G10B10A2_UNORM  ){SwapChainDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM     ; goto again;} // if failed with 10-bit then try again with  8-bit
   if(SwapChainDesc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB){SwapChainDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM     ; goto again;} // #SwapFlipSRGB may fail to create sRGB in that case create as linear and 'swapRTV' in 'ImageRT.map'
#endif
   return false;
}
#endif

Display::RESET_RESULT Display::ResetTry(Bool set)
{
   SyncLocker locker(_lock);
  _resetting=true;
   RESET_RESULT result;

   if(!created())result=RESET_DEVICE_NOT_CREATED;else
   {
      Renderer.rtDel();

      Bool ok=true;
   #if WINDOWS
      #if DX11 && !WINDOWS_NEW // on WindowsNew we can't change mode here, we need to set according to what we've got, instead 'RequestDisplayMode' can be called
         if(!exclusive())ok=SetDisplayMode();
      #endif

      #if DX11
         #if WINDOWS_OLD
            // https://docs.microsoft.com/en-us/windows/win32/direct3darticles/dxgi-best-practices
            if(!SwapChainDesc.Windowed){ResizeTarget(); ResizeBuffers();} // if want fullscreen then first set new size, so the following 'SetFullscreenState' will set mode based on desired resolution (only 'ResizeBuffers' was needed in tests, however keep both just in case), don't call this if we want non-fullscreen mode, because if we're already in fullscreen, then this could switch to another fullscreen mode, so first disable fullscreen with 'SetFullscreenState' below

            if(ok&=OK(SwapChain->SetFullscreenState(!SwapChainDesc.Windowed, SwapChainDesc.Windowed ? null : Output)))
            if(ok&=ResizeTarget())
         #elif 0 // both 'SetFullscreenState' and 'ResizeTarget' fail on WINDOWS_NEW, instead, 'TryEnterFullScreenMode', 'ExitFullScreenMode', 'TryResizeView' are used
            DXGI_MODE_DESC mode; Zero(mode);
            mode.Format =SwapChainDesc.Format;
            mode.Width  =SwapChainDesc.Width;
            mode.Height =SwapChainDesc.Height;
            mode.Scaling=DXGI_MODE_SCALING_UNSPECIFIED;
            if(_freq_want && full()) // set custom frequency only if desired and in full-screen
            {
               mode.RefreshRate.Numerator  =_freq_want;
               mode.RefreshRate.Denominator=1;
            }
            if(ok&=OK(SwapChain->SetFullscreenState(full(), null)))
            if(ok&=OK(SwapChain->ResizeTarget(&mode)))
         #endif
         {
            // 'ResizeTarget' may select a different resolution than requested for fullscreen mode, so check what we've got
         #if WINDOWS_OLD
            if(!SwapChainDesc.Windowed)
         #else
            if(0) // if(full()) this shouldn't be performed for WINDOWS_NEW because for this we're not setting a custom display mode, but instead we're setting to what we've got
         #endif
            {
               IDXGIOutput *output=null; SwapChain->GetContainingOutput(&output); if(output)
               {
                  DXGI_OUTPUT_DESC desc; if(OK(output->GetDesc(&desc)))
                  {
                    _res.set(desc.DesktopCoordinates.right-desc.DesktopCoordinates.left, desc.DesktopCoordinates.bottom-desc.DesktopCoordinates.top);
                  #if WINDOWS_OLD
                     SwapChainDesc.BufferDesc.Width =resW();
                     SwapChainDesc.BufferDesc.Height=resH();
                  #else
                     // for WINDOWS_NEW this should adjust '_res' based on relative rotation
                     SwapChainDesc.Width =resW();
                     SwapChainDesc.Height=resH();
                  #endif
                     densityUpdate();
                  }
                  output->Release();
               }
            }
            ok&=ResizeBuffers();
         }
      #elif GL
         if(ok)ok=SetDisplayMode();
      #endif
   #elif MAC || LINUX
      ok=SetDisplayMode();
   #else
      ok=true;
   #endif

      getCaps();
      if(!ok                 )result=RESET_DEVICE_RESET_FAILED ;else
      if(!Renderer.rtCreate())result=RESET_RENDER_TARGET_FAILED;else
      {
         adjustWindow(set); // !! call before 'after' so current monitor can be detected properly based on window position which affects the aspect ratio in 'after' !!
         after(true);
         resetEyeAdaptation(); // this potentially may use drawing

         Time.skipUpdate(2); // when resetting display skip 2 frames, because the slow down can occur for this long
         result=RESET_OK;
      }
   }

  _resetting=false;
   return result;
}
void Display::Reset()
{
   RESET_RESULT result=ResetTry(); if(result!=RESET_OK)ResetFailed(result, result);
}
/******************************************************************************/
void Display::getGamma()
{
   Bool ok=false;
#if WINDOWS_OLD
   if(HDC hdc=GetDC(null)){ok=(GetDeviceGammaRamp(hdc, _gamma_array)!=0); ReleaseDC(null, hdc);}
#elif MAC
   Int capacity =CGDisplayGammaTableCapacity(kCGDirectMainDisplay);
   if( capacity>=1)
   {
      Memc<CGGammaValue> r, g, b; r.setNum(capacity); g.setNum(capacity); b.setNum(capacity);
      UInt samples=0; CGGetDisplayTransferByTable(kCGDirectMainDisplay, capacity, r.data(), g.data(), b.data(), &samples);
      if(  samples>1 && samples<=capacity)
      {
         ok=true;
         REP(256)
         {
            Int src=i*(samples-1)/255;
           _gamma_array[0][i]=RoundU(Sat(r[src])*0xFFFF);
           _gamma_array[1][i]=RoundU(Sat(g[src])*0xFFFF);
           _gamma_array[2][i]=RoundU(Sat(b[src])*0xFFFF);
         }
      }
   }
#elif LINUX
   if(XDisplay)
   {
      Int size=0; if(XF86VidModeGetGammaRampSize(XDisplay, DefaultScreen(XDisplay), &size))
      {
         if(size==256)
         {
            ok=(XF86VidModeGetGammaRamp(XDisplay, DefaultScreen(XDisplay), 256, _gamma_array[0], _gamma_array[1], _gamma_array[2])!=0);
         }else
         if(size>0)
         {
            Memc<UShort> r, g, b; r.setNum(size); g.setNum(size); b.setNum(size);
            if(XF86VidModeGetGammaRamp(XDisplay, DefaultScreen(XDisplay), size, r.data(), g.data(), b.data()))
            {
               ok=true;
               REP(256)
               {
                  Int src=i*(size-1)/255;
                 _gamma_array[0][i]=r[src];
                 _gamma_array[1][i]=g[src];
                 _gamma_array[2][i]=b[src];
               }
            }
         }
      }
   }
#endif
   if(!ok)REP(256)_gamma_array[0][i]=_gamma_array[1][i]=_gamma_array[2][i]=(i*0xFFFF+128)/255;
}
void Display::getCaps()
{
   if(LogInit)LogN("Display.getCaps");
#if DX11
   // values taken from - https://msdn.microsoft.com/en-us/library/windows/desktop/ff476876(v=vs.85).aspx
   DXGI_SWAP_CHAIN_DESC desc;
   SwapChain->GetDesc(&desc); _freq_got=(desc.BufferDesc.RefreshRate.Denominator ? RoundPos(Flt(desc.BufferDesc.RefreshRate.Numerator)/desc.BufferDesc.RefreshRate.Denominator) : 0);
  _max_rt        =((FeatureLevel>=D3D_FEATURE_LEVEL_10_0) ? 8 : (FeatureLevel>=D3D_FEATURE_LEVEL_9_3) ? 4 : 1);
  _max_tex_filter=((FeatureLevel>=D3D_FEATURE_LEVEL_9_2 ) ? 16 : 2);
  _max_tex_size  =((FeatureLevel>=D3D_FEATURE_LEVEL_11_0) ? 16384 : (FeatureLevel>=D3D_FEATURE_LEVEL_10_0) ? 8192 : (FeatureLevel>=D3D_FEATURE_LEVEL_9_3) ? 4096 : 2048);

   REP(IMAGE_ALL_TYPES)
   {
      UInt usage=0; UINT fs; if(OK(D3D->CheckFormatSupport(ImageTI[i].format, &fs)))
      {
         if(fs&D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER        )usage|=ImageTypeInfo::USAGE_VTX;
         if(fs&D3D11_FORMAT_SUPPORT_TEXTURE2D               )usage|=ImageTypeInfo::USAGE_IMAGE_2D;
         if(fs&D3D11_FORMAT_SUPPORT_TEXTURE3D               )usage|=ImageTypeInfo::USAGE_IMAGE_3D;
         if(fs&D3D11_FORMAT_SUPPORT_TEXTURECUBE             )usage|=ImageTypeInfo::USAGE_IMAGE_CUBE;
         if(fs&D3D11_FORMAT_SUPPORT_RENDER_TARGET           )usage|=ImageTypeInfo::USAGE_IMAGE_RT;
         if(fs&D3D11_FORMAT_SUPPORT_DEPTH_STENCIL           )usage|=ImageTypeInfo::USAGE_IMAGE_DS;
         if(fs&D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET)usage|=ImageTypeInfo::USAGE_IMAGE_MS;
      }
      ImageTI[i]._usage=usage;
   }

 /*D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT min_prec;
   if(OK(D3D->CheckFeatureSupport(D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &min_prec, SIZE(min_prec)))) // check for hlsl half support
   {
   }*/

   /*IDXGISwapChain4 *swap_chain4=null; SwapChain->QueryInterface(__uuidof(IDXGISwapChain4), (Ptr*)&swap_chain4); if(swap_chain4)
   {
      swap_chain4->SetColorSpace1();
      swap_chain4->Release();
   }*/
   
   /*IDXGIOutput *output=null; SwapChain->GetContainingOutput(&output); if(output)
   {
      IDXGIOutput6 *output6=null; output->QueryInterface(__uuidof(IDXGIOutput6), (Ptr*)&output6); if(output6)
      {
         DXGI_OUTPUT_DESC1 desc; if(OK(output6->GetDesc1(&desc)))
         {
            // get info about color space
            // TODO: this could replace 'highMonitorPrecision', however on a computer that returns 8-bit, using 10-bit still gives better quality, so perhaps it's not yet ready
         }
         output6->Release();
      }
      output->Release();
   }*/
   /*void SetCS(Int cs)
   {
      Clamp(cs, 0, DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020);
      IDXGISwapChain4 *swap_chain4=null; SwapChain->QueryInterface(__uuidof(IDXGISwapChain4), (Ptr*)&swap_chain4); if(swap_chain4)
      {
         OK(swap_chain4->SetColorSpace1((DXGI_COLOR_SPACE_TYPE)cs));
         swap_chain4->Release();
      }
   }
   void SetMeta()
   {
      IDXGISwapChain4 *swap_chain4=null; SwapChain->QueryInterface(__uuidof(IDXGISwapChain4), (Ptr*)&swap_chain4); if(swap_chain4)
      {
         DXGI_HDR_METADATA_HDR10 meta; Zero(meta);
         meta.RedPrimary[0] = 0.680 * 50000;
         meta.RedPrimary[1] = 0.320 * 50000;
         meta.GreenPrimary[0] = 0.265 * 50000;
         meta.GreenPrimary[1] = 0.690 * 50000;
         meta.BluePrimary[0] = 0.150 * 50000;
         meta.BluePrimary[1] = 0.060 * 50000;
         meta.WhitePoint[0] = 0.3127 * 50000;
         meta.WhitePoint[1] = 0.3290 * 50000;
         meta.MaxMasteringLuminance = 1000 * 10000;
         meta.MinMasteringLuminance = 0.001 * 10000;
         meta.MaxContentLightLevel = 2000;
         meta.MaxFrameAverageLightLevel = 500;
         OK(swap_chain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, SIZE(meta), &meta));
         swap_chain4->Release();
      }
   }*/
#elif GL
 //CChar8 *ext=(CChar8*)glGetString(GL_EXTENSIONS);
      _max_tex_size    =2048; glGetIntegerv(GL_MAX_TEXTURE_SIZE          , &_max_tex_size    );
   int aniso           =  16; glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY, & aniso           ); _max_tex_filter =Mid(aniso         , 1, 255);
 //int max_vtx_attrib  =   0; glGetIntegerv(GL_MAX_VERTEX_ATTRIBS        , & max_vtx_attrib  ); _max_vtx_attribs=Mid(max_vtx_attrib, 0, 255);
   int max_draw_buffers=   1; glGetIntegerv(GL_MAX_DRAW_BUFFERS          , & max_draw_buffers);
   int max_col_attach  =   1; glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS     , & max_col_attach  ); _max_rt=Mid(Min(max_draw_buffers, max_col_attach), 1, 255);

   #if !GL_ES && (defined GL_INTERNALFORMAT_SUPPORTED || defined GL_COLOR_RENDERABLE || defined GL_DEPTH_RENDERABLE) // on GL_ES glGetInternalformativ works only for GL_RENDERBUFFER
   #if WINDOWS
      if(glGetInternalformativ)
   #endif
      {
         glGetError(); // clear any previous errors
         REP(IMAGE_ALL_TYPES)
         {
            GLint params[1]; UInt usage=0; GLenum internalformat=ImageTI[i].format;
            // no ImageTypeInfo::USAGE_VTX ?
            glGetInternalformativ(GL_TEXTURE_2D            , internalformat, GL_INTERNALFORMAT_SUPPORTED, Elms(params), params); if(glGetError()==GL_NO_ERROR && params[0])usage|=ImageTypeInfo::USAGE_IMAGE_2D;
            glGetInternalformativ(GL_TEXTURE_3D            , internalformat, GL_INTERNALFORMAT_SUPPORTED, Elms(params), params); if(glGetError()==GL_NO_ERROR && params[0])usage|=ImageTypeInfo::USAGE_IMAGE_3D;
            glGetInternalformativ(GL_TEXTURE_CUBE_MAP      , internalformat, GL_INTERNALFORMAT_SUPPORTED, Elms(params), params); if(glGetError()==GL_NO_ERROR && params[0])usage|=ImageTypeInfo::USAGE_IMAGE_CUBE;
            glGetInternalformativ(GL_TEXTURE_2D            , internalformat, GL_COLOR_RENDERABLE        , Elms(params), params); if(glGetError()==GL_NO_ERROR && params[0])usage|=ImageTypeInfo::USAGE_IMAGE_RT;
            glGetInternalformativ(GL_TEXTURE_2D            , internalformat, GL_DEPTH_RENDERABLE        , Elms(params), params); if(glGetError()==GL_NO_ERROR && params[0])usage|=ImageTypeInfo::USAGE_IMAGE_DS;
            glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, internalformat, GL_COLOR_RENDERABLE        , Elms(params), params); if(glGetError()==GL_NO_ERROR && params[0])usage|=ImageTypeInfo::USAGE_IMAGE_MS;
            glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, internalformat, GL_DEPTH_RENDERABLE        , Elms(params), params); if(glGetError()==GL_NO_ERROR && params[0])usage|=ImageTypeInfo::USAGE_IMAGE_MS;
            ImageTI[i]._usage=usage;
         }
      }
   #endif
#endif

#if IOS
  _freq_got=[UIScreen mainScreen].maximumFramesPerSecond;
#elif ANDROID
   JNI jni;
   if(jni && ActivityClass && Activity)
      if(JMethodID refreshRate=jni.func(ActivityClass, "refreshRate", "()F"))
         _freq_got=RoundPos(jni->CallFloatMethod(Activity, refreshRate));
#elif LINUX
   if(XDisplay)
   {
      int dotclock; XF86VidModeModeLine mode;
      if(XF86VidModeGetModeLine(XDisplay, DefaultScreen(XDisplay), &dotclock, &mode))
      {
         Int total=mode.htotal*mode.vtotal;
        _freq_got=(total ? DivRound(dotclock*1000, total) : 0);
      }
   }
#endif
  
   if(!Physics.precision())Physics.precision(0); // adjust physics precision when possibility of screen refresh rate change
   densityUpdate(); // max texture size affects max allowed density
  _samples=DisplaySamples(_samples);

   if(Renderer.anyDeferred() && deferredUnavailable())
   {
      if(Renderer.type              ()==RT_DEFERRED)Renderer.type              (RT_FORWARD);
      if(Water   .reflectionRenderer()==RT_DEFERRED)Water   .reflectionRenderer(RT_FORWARD);
   }
}
/******************************************************************************/
void Display::after(Bool resize_callback)
{
   if(LogInit)LogN("Display.after");
   if(!full() // if we're setting window
#if !WEB // for WEB set size even if we're maximized, because for WEB 'App.maximized' means the browser window and not the canvas size
   && !App.maximized() // which is not maximized
#endif
   )App._window_size=res();
   if(_gamma)gammaSet(); // force reset gamma
   aspectRatioEx(true, !resize_callback);
   setColorLUT();
}
/******************************************************************************/
Bool Display::flip()
{
   if(created())
   {
      if(D.colorManaged())
      {
         ImageRT &src=Renderer._main_temp, &dest=Renderer._main;
         ALPHA_MODE alpha=D.alpha(ALPHA_NONE);
         Sh.Vol->set(D._color_lut);
         Int  res      =D._color_lut.w(); Sh.ImgSize->set(Vec2(Flt(res-1)/res, 0.5f/res)); // assumes all 3 dimensions are same size
         Bool hdr      =(src.highPrecision() && dest.highPrecision()), // need HDR only if both are high precision
              dither   =(D.dither() && !dest.highPrecision()),
               in_gamma=LINEAR_GAMMA,  in_swap=( in_gamma && src .canSwapSRV() && !hdr); if( in_swap){ in_gamma=false; src .swapSRV();} // can't swap for 'hdr' because shader assumes that  input is Linear
         Bool out_gamma=LINEAR_GAMMA, out_swap=(out_gamma && dest.canSwapRTV() && !hdr); if(out_swap){out_gamma=false; dest.swapRTV();} // can't swap for 'hdr' because shader assumes that output is Linear
         Renderer.set(&dest, null, false);
         Sh.ColorLUT[hdr][dither][in_gamma][out_gamma]->draw(src);
         if( in_swap)src .swapSRV();
         if(out_swap)dest.swapRTV();
         D.alpha(alpha);
         Renderer.set(Renderer._cur_main, Renderer._cur_main_ds, false);
      }
   #if DX11
      Bool sync=ActualSync(); if(!OK(SwapChain->Present(sync, sync ? 0 : PresentFlags)))
      {
         static Bool showed=false; if(!showed) // check if not yet showed, because this can be called on another thread, while the main thread already started 'DrawState', which would then call this again, and show the message box 2 times
         {
            showed=true;
            WindowMsgBox("Error", "DirectX Device lost, please restart application.", true);
            StateExit.set();
         }
         return false; // we can use 'DXGI_PRESENT_ALLOW_TEARING' only when "sync==false", do this extra check here, because 'ActualSync' depends on VR which may disconnect after 'SwapChain' was created and 'PresentFlags' already set
      }
      if(SwapChainDesc.SwapEffect!=DXGI_SWAP_EFFECT_DISCARD) // when using swap chain flip mode, 'Present' clears the backbuffer from 'OMSetRenderTargets', so reset it
         D3DC->OMSetRenderTargets(Elms(Renderer._cur_id), Renderer._cur_id, Renderer._cur_ds_id);
   #elif GL
      #if WINDOWS
         SwapBuffers(hDC);
      #elif MAC
         CGLFlushDrawable(MainContext.context); // same as "[[OpenGLView openGLContext] flushBuffer];"
      #elif LINUX
         glXSwapBuffers(XDisplay, App.Hwnd());
      #elif ANDROID
         eglSwapBuffers(GLDisplay, MainContext.surface);
      #elif IOS
         glBindRenderbuffer(GL_RENDERBUFFER, Renderer._main._rb);
         [MainContext.context presentRenderbuffer:GL_RENDERBUFFER];
      #elif WEB
         // this is done automatically on Web

         // #WebSRGB
         Renderer.set(&Renderer._main, null, false);
         ALPHA_MODE alpha=D.alpha(ALPHA_NONE);
         Sh.get("WebLToS")->draw(Renderer._main_temp);
         D.alpha(alpha);
         Renderer.set(Renderer._cur_main, Renderer._cur_main_ds, false);
      #endif
   #endif
      D._flip.clearNoDiscard(); // can't discard here, because on Nvidia GeForce artifacts may occur for slow-downs (for example when browsing Esenthel Store and switching tabs/categories)
   }
   return true;
}
void Display::flush()
{
   if(created())
   {
   #if DX11
      D3DC->Flush();
   #elif GL
      glFlush();
   #endif
   }
}
void Display::finish()
{
   if(created())
   {
   #if DX11
      if(Query)
      {
         D3DC->End(Query);
         BOOL done=FALSE; while(OK(D3DC->GetData(Query, &done, SIZE(done), 0)) && !done);
      }
   #elif GL
      glFinish();
   #endif
   }
}
/******************************************************************************/
// SETTINGS
/******************************************************************************/
void Display::adjustWindow(Bool set)
{
   RectI full, work; VecI2 max_normal_win_client_size, maximized_win_client_size;
    getMonitor(full, work, max_normal_win_client_size, maximized_win_client_size);

#if DEBUG && 0
   LogN(S+"full:"+full.asText()+", work:"+work.asText()+", App._window_pos:"+App._window_pos+", D.res:"+D.res());
#endif

#if WINDOWS_OLD
   if(D.full()) // fullscreen
   {
      SetWindowLong(App.Hwnd(), GWL_STYLE, App._style_full);
      SetWindowPos (App.Hwnd(), (App.backgroundFull() && !exclusiveFull()) ? HWND_NOTOPMOST : HWND_TOPMOST, full.min.x, full.min.y, resW(), resH(), 0);
   }else
   if(resW()>=maximized_win_client_size.x && resH()>=maximized_win_client_size.y) // maximized
   {
      SetWindowLong(App.Hwnd(), GWL_STYLE, App._style_window_maximized);
   #if 0 // this doesn't work as expected
      SetWindowPos (App.Hwnd(), HWND_TOP , work.min.x+App._bound_maximized.min.x, work.min.y-App._bound_maximized.max.y, resW()+App._bound_maximized.w(), resH()+App._bound_maximized.h(), SWP_NOACTIVATE); 
   #else
      SetWindowPos (App.Hwnd(), HWND_TOP , work.min.x+App._bound_maximized.min.x, work.min.y-App._bound_maximized.max.y, resW()+App._bound_maximized.max.x, resH()-App._bound_maximized.min.y, SWP_NOACTIVATE);
   #endif
   }else // normal window
   {
      Int w=resW()+App._bound.w(),
          h=resH()+App._bound.h();

      if(App._window_pos.x==INT_MAX){if(App.x<=-1)App._window_pos.x=work.min.x;else if(!App.x)App._window_pos.x=work.centerXI()-w/2;else App._window_pos.x=work.max.x-w;}
      if(App._window_pos.y==INT_MAX){if(App.y>= 1)App._window_pos.y=work.min.y;else if(!App.y)App._window_pos.y=work.centerYI()-h/2;else App._window_pos.y=work.max.y-h;}

      // make sure the window is not completely outside of working area
      const Int b=32; Int r=b;
      if(!(App.flag&APP_NO_TITLE_BAR)) // has bar
      {
         Int size=GetSystemMetrics(SM_CXSIZE); // TODO: this should be OK because we're DPI-Aware, however it doesn't work OK
       /*if(HDC hdc=GetDC(App.Hwnd()))
         {
            size=DivCeil(size*GetDeviceCaps(hdc, LOGPIXELSX), 96);
            ReleaseDC(App.Hwnd(), hdc);
         }*/
         if(!(App.flag& APP_NO_CLOSE                   ))r+=size  ; // has close                button
         if(  App.flag&(APP_MINIMIZABLE|APP_MAXIMIZABLE))r+=size*2; // has minimize or maximize button (if any is enabled, then both will appear)
      }

      if(App._window_pos.x+b>work.max.x)App._window_pos.x=Max(work.min.x, work.max.x-b);else{Int p=App._window_pos.x+w; if(p-r<work.min.x)App._window_pos.x=Min(work.min.x+r, work.max.x)-w;}
      if(App._window_pos.y+b>work.max.y)App._window_pos.y=Max(work.min.y, work.max.y-b);else{Int p=App._window_pos.y+h; if(p-b<work.min.y)App._window_pos.y=Min(work.min.y+b, work.max.y)-h;}

      SetWindowLong(App.Hwnd(), GWL_STYLE     , App._style_window);
      SetWindowPos (App.Hwnd(), HWND_NOTOPMOST, App._window_pos.x, App._window_pos.y, w, h, SWP_NOACTIVATE);
   }
#elif MAC
   if(!D.full()) // on Mac don't adjust the window size/pos when in fullscreen because it's not needed, additionally it will cancel out original window position when later restoring
   {
      WindowSize(resW(), resH(), true);
      RectI r=WindowRect(false); WindowPos(r.min.x, r.min.y); // reset position because if Application was created in fullscreen mode, and then we've toggled to windowed mode, then because system MenuBar was hidden for fullscreen, App's window position could've gone underneath the menu bar, for the windowed mode the bar is unhidden, however just applying new window size doesn't reposition it so it doesn't collide with the MenuBar, that's why we need to trigger repositioning it manually, avoid calling "WindowMove(0, 0)" because that internally does nothing if the delta is zero
   }
#elif LINUX
   // set window fullscreen state
   if(App.hwnd())
   {
      // setting fullscreen mode will fail if window is not resizable, so force it to be just for this operation
      Bool set_resizable=(D.full() && !(App.flag&APP_RESIZABLE));
      if(  set_resizable)App.setWindowFlags(true);

      #define _NET_WM_STATE_REMOVE 0
      #define _NET_WM_STATE_ADD    1
      #define _NET_WM_STATE_TOGGLE 2
      Atom FIND_ATOM(_NET_WM_STATE), FIND_ATOM(_NET_WM_STATE_FULLSCREEN);
      XEvent e; Zero(e);
      e.xclient.type        =ClientMessage;
      e.xclient.window      =App.Hwnd();
      e.xclient.message_type=_NET_WM_STATE;
      e.xclient.format      =32;
      e.xclient.data.l[0]   =(D.full() ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE);
      e.xclient.data.l[1]   =_NET_WM_STATE_FULLSCREEN;
      e.xclient.data.l[2]   =0;
      e.xclient.data.l[3]   =1;
      XSendEvent(XDisplay, DefaultRootWindow(XDisplay), false, SubstructureRedirectMask|SubstructureNotifyMask, &e);
      XSync(XDisplay, false);

      if(set_resizable)App.setWindowFlags();
   }

   // set window size
   if(!D.full() && !set)WindowSize(resW(), resH(), true); // don't resize Window on Linux when changing mode due to 'set' (when window got resized due to OS/User input instead of calling 'D.mode', because there the window is already resized and calling this would cause window jumping)
#endif
}
Display::RESET_RESULT Display::modeTry(Int w, Int h, Int full, Bool set)
{
         if(w   <=0)w= T.resW();
         if(h   <=0)h= T.resH();
   Bool f=((full< 0) ? T.full() : (full!=0));
   if(w==resW() && h==resH() && f==T.full())return RESET_OK;

   if(created())
   {
      SyncLocker locker(_lock);

      Int  cur_x   =T.resW(); T._res.x=w;
      Int  cur_y   =T.resH(); T._res.y=h;
      Bool cur_full=T.full(); T._full =f;

   #if WEB
      Renderer._main.forceInfo(w, h, 1, Renderer._main.type(), Renderer._main.mode(), Renderer._main.samples()); // '_main_ds' will be set in 'rtCreate'
   #endif
      if(!findMode())return RESET_DEVICE_NOT_CREATED;
      if(cur_x==T.resW() && cur_y==T.resH() && cur_full==T.full())return RESET_OK; // new mode matches the current one, need to check again since 'findMode' may have adjusted the T.resW T.resH T.full values
      RESET_RESULT result=ResetTry(set);      if(result!=RESET_OK)return result  ; // reset the device

      Ms.clipUpdateConditional();
   }else
   {
      T._res.x=w;
      T._res.y=h;
      T._full =f;
      densityUpdate();
   }
   return RESET_OK;
}
void Display::modeSet(Int w, Int h, Int full)
{
   RESET_RESULT result=modeTry(w, h, full, true);
   if(result!=RESET_OK)ResetFailed(result, result);
}
Display& Display::mode(Int w, Int h, Int full)
{
#if WINDOWS_NEW // on WindowsNew we can only request a change on the window
   if(created())
   {
      RequestDisplayMode(w, h, full);
      return T;
   }
#elif MOBILE
   return T; // we can't manually change mode for mobile platforms at all because they're always fullscreen
#elif WEB
   if(created())
   {
      Bool f=((full<0) ? T.full() : (full!=0));
      if(T.full()!=f)
      {
         if(f)
         {
            EmscriptenFullscreenStrategy f; Zero(f);
            f.scaleMode                =EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
            f.canvasResolutionScaleMode=EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_HIDEF;
            f.filteringMode            =EMSCRIPTEN_FULLSCREEN_FILTERING_NEAREST;
            emscripten_request_fullscreen_strategy(null, true, &f);
            return T; // after request was made, return and wait until Emscripten handles it
         }else
         {
            emscripten_exit_fullscreen();
            return T; // after request was made, return and wait until Emscripten handles it, we can't process canvas resizes here, because Emscripten still needs to do some stuff
         }
      }
      if(w<=0)w=resW();
      if(h<=0)h=resH();
      if(f // can't change resolution when in full screen mode
      || w==resW() && h==resH() // nothing to change
      )return T;

      // resize canvas
      Flt  zoom=browserZoom();
      Vec2 css_size=VecI2(w, h)/zoom; // calculate css
      emscripten_set_element_css_size   (null, css_size.x, css_size.y); // this accepts floating point sizes
      emscripten_set_canvas_element_size(null, w, h);
      extern Flt ScreenScale; ScreenScale=zoom; // here we maintain 1:1 pixel ratio so we can set scale to zoom
   }
#endif
   Int cur_w   =T.resW(),
       cur_h   =T.resH(),
       cur_full=T.full();
   RESET_RESULT result0=modeTry(w, h, full); // try to set new mode
   if(result0!=RESET_OK)
   {
      RESET_RESULT result1=modeTry(cur_w, cur_h, cur_full); // try to set old mode
      if(result1!=RESET_OK)ResetFailed(result0, result1);
   }
   return T;
}
Display& Display::toggle(Bool window_size)
{
   if(!created())_full^=1;else
   {
      if(full()     )mode(App._window_size.x, App._window_size.y, false);else // if app was in fullscreen then set windowed mode based on last known window size
      if(window_size)mode(App._window_size.x, App._window_size.y, true );else // if set        fullscreen using                           last known window size
      {  // set full screen based on resolution of the monitor
         RectI full, work; VecI2 max_normal_win_client_size, maximized_win_client_size;
          getMonitor(full, work, max_normal_win_client_size, maximized_win_client_size);
         mode(full.w(), full.h(), true);
      }
   }
   return T;
}
Display& Display::full(Bool full, Bool window_size)
{
   if(!created())T._full=full;else
   if(full!=T.full())toggle(window_size);
   return T;
}
Display& Display::monitorPrecision(IMAGE_PRECISION precision)
{
   Clamp(precision, IMAGE_PRECISION_8, IMAGE_PRECISION(IMAGE_PRECISION_NUM-1));
   if(!created())_monitor_prec=precision;else
   if(monitorPrecision()!=precision){_monitor_prec=precision; if(findMode())Reset();}
   return T;
}
Bool Display::exclusiveFull()C
{
#if WINDOWS_OLD && DX11
   return !SwapChainDesc.Windowed;
#else
   return false;
#endif
}
Display& Display::exclusive(Bool exclusive)
{
   if(_exclusive!=exclusive)
   {
     _exclusive=exclusive;
   #if WINDOWS_OLD && (DX11)
      if(created() && full())
      {
      #if DX11 // on DX11 have to disable 'SetFullscreenState' first, otherwise custom resolutions may get reverted to desktop resolution
         SyncLocker locker(_lock);
         if(SwapChain)SwapChain->SetFullscreenState(false, null);
      #endif
         if(findMode())Reset();
      }
   #endif
   }
   return T;
}
static COLOR_SPACE LastSrcColorSpace;
static Str         LastDestColorSpace;
void Display::setColorLUT()
{
   SyncLocker locker(D._lock); // needed by 'LastSrcColorSpace', 'LastDestColorSpace', '_color_lut' and 'rtCreateMain'
   Str dest_color_space; if(_color_space && created())if(auto monitor=getMonitor())dest_color_space=monitor->colorProfilePath();
   if(LastSrcColorSpace!=_color_space || !EqualPath(LastDestColorSpace, dest_color_space)) // set only if different
   {
      LastSrcColorSpace =    _color_space;
      LastDestColorSpace=dest_color_space;
      if(SetColorLUT(_color_space, dest_color_space, _color_lut))
      {
         if(!Sh.ColorLUT[0][0][0][0])
            REPD(hdr      , 2)
            REPD(dither   , 2)
            REPD( in_gamma, 2)
            REPD(out_gamma, 2)
               if(!(Sh.ColorLUT[hdr][dither][in_gamma][out_gamma]=Sh.find(S+"ColorLUT"+hdr+dither+in_gamma+out_gamma)))goto error; // use 'find' to allow fail

         if(Sh.ColorLUT[0][0][0][0])goto ok;
      }
   error:
     _color_lut.del();
   }
ok:
   Renderer.rtCreateMain(); // always needs to be called, if succeeded or failed
}
Display& Display::colorSpace(COLOR_SPACE color_space)
{
   if(InRange(color_space, COLOR_SPACE_NUM) && _color_space!=color_space)
   {
     _color_space=color_space;
      setColorLUT();
   }
   return T;
}
/******************************************************************************/
void Display::validateCoords(Int eye)
{
   Vec4  coords;
   Vec2 &coords_mul=coords.xy,
        &coords_add=coords.zw;

   coords_mul.set(1/w(), 1/h());
   coords_add=0; // or coords_mul*_draw_offset;
   if(InRange(eye, 2)) // stereo
   {
      coords_add.x+=ProjMatrixEyeOffset[eye];
      coords_mul.x*=2;
   }else
   if(!_view_active.full)
   {
      Vec2 ds_vs(Flt(Renderer.resW())/_view_active.recti.w(),
                 Flt(Renderer.resH())/_view_active.recti.h());
      coords_mul  *=ds_vs;
      coords_add  *=ds_vs;
      coords_add.x-=Flt(_view_active.recti.min.x+_view_active.recti.max.x-Renderer.resW())/_view_active.recti.w();
      coords_add.y+=Flt(_view_active.recti.min.y+_view_active.recti.max.y-Renderer.resH())/_view_active.recti.h();
   }

#if GL
   if(!mainFBO()) // in OpenGL when drawing to custom RenderTarget the 'dest.pos.y' must be flipped
   {
      CHS(coords_mul.y);
      CHS(coords_add.y);
   }
#endif

   Sh.Coords->setConditional(coords);
}
/******************************************************************************/
void Display::sizeChanged()
{
   D._size =D._unscaled_size/D._scale;
   D._size2=D._size         *2;

 C VecI2 &res=((VR.active() && D._allow_stereo) ? VR.guiRes() : D.res());
   D._pixel_size    .set( w2()/res.x,  h2()/res.y); D._pixel_size_2=D._pixel_size*0.5f;
   D._pixel_size_inv.set(res.x/ w2(), res.y/ h2());

   // this is used by mouse/touch pointers
   D._window_pixel_to_screen_mul.set( D.w2()/(D.resW()-1),  // div by "resW-1" so we can have full -D.w .. D.w range !! if this is changed for some reason, then adjust 'rect.max' in 'Ms.clipUpdate' !!
                                     -D.h2()/(D.resH()-1)); // div by "resH-1" so we can have full -D.h .. D.h range !! if this is changed for some reason, then adjust 'rect.max' in 'Ms.clipUpdate' !!
   D._window_pixel_to_screen_add.set(-D.w(), D.h());
   if(VR.active()) // because VR Gui Rect may be different than Window Rect, we need to adjust scaling
   {
      // '_window_pixel_to_screen_scale' is set so that 'windowPixelToScreen' will always point to VR 'GuiTexture', no matter if 'D._allow_stereo' is enabled/disabled (drawing to 'GuiTexture' or System Window)
      D._window_pixel_to_screen_mul*=D._window_pixel_to_screen_scale;
      D._window_pixel_to_screen_add*=D._window_pixel_to_screen_scale;
   }

   viewReset();
}
Display& Display::scale(Flt scale)
{
   if(T._scale!=scale)
   {
      T._scale=scale;
      if(created())
      {
         Vec2 old_size=size();
           sizeChanged();
         screenChanged(old_size.x, old_size.y);
      }
   }
   return T;
}
void Display::densityUpdate()
{
again:
  _render_res=ByteScale2Res(res(), densityByte());
   if(_render_res.max()>maxTexSize() && maxTexSize()>0 && densityUpsample()) // if calculated size exceeds possible max texture size, then we need to clamp the density, don't try to go below 1.0 density
   {
      Flt  max_density_f=Flt(maxTexSize())/res().max();
      Byte max_density  =FltToByteScale2(max_density_f);
      if(_density>max_density)_density=max_density;else _density--;
      goto again;
   }
}
Bool Display::densityFast(Byte density)
{
   if(density!=densityByte())
   {
      T._density=density;
      densityUpdate();
      return true;
   }
   return false;
}
Flt      Display::density(           )C {return ByteScale2ToFlt(densityByte());}
Display& Display::density(Flt density)
{
   Byte b=FltToByteScale2(density);
   if(densityFast(b))Renderer.rtClean();
   return T;
}
Display& Display::densityFilter(FILTER_TYPE filter) {_density_filter=filter; return T;}
Display& Display::samples(Byte samples)
{
   samples=DisplaySamples(samples);
   if(T._samples!=samples){T._samples=samples; Renderer.rtClean();}
   return T;
}
/******************************************************************************/
Display& Display::highPrecColRT(Bool on) {if(_hp_col_rt!=on){_hp_col_rt=on; Renderer.rtClean();} return T;}
Display& Display::highPrecNrmRT(Bool on) {if(_hp_nrm_rt!=on){_hp_nrm_rt=on; Renderer.rtClean();} return T;}
Display& Display::highPrecLumRT(Bool on) {if(_hp_lum_rt!=on){_hp_lum_rt=on; Renderer.rtClean();} return T;}
Display& Display::litColRTPrecision(IMAGE_PRECISION precision) // !! Warning: there might be small render result differences between using high precision RT's, because sometimes we can use sRGB/non-sRGB RT views, but sometimes we have to do gamma correction in the shader, and for performance reasons, LinearToSRGBFast/SRGBToLinearFast are chosen, which don't provide the same exact results !!
{
   Clamp(precision, IMAGE_PRECISION_8, IMAGE_PRECISION(IMAGE_PRECISION_NUM-1));
   if(_lit_col_rt_prec!=precision){_lit_col_rt_prec=precision; Renderer.rtClean();}
   return T;
}
/******************************************************************************/
void Display::setSync()
{
   SyncLocker locker(_lock);
   if(created())
   {
   #if DX11
      if(findMode())Reset();
   #elif GL
         Bool sync=(T.sync() && !VR.active()); // if using VR then we have to disable screen sync, because HMD will handle this according to its own refresh rate
      #if WINDOWS
         wglSwapIntervalEXT(sync);
      #elif MAC
         Int value=sync; CGLSetParameter(MainContext.context, kCGLCPSwapInterval, &value);
      #elif LINUX
         if(glXSwapInterval)glXSwapInterval(XDisplay, App.Hwnd(), sync);
      #elif ANDROID
         eglSwapInterval(GLDisplay, sync);
      #elif WEB
         if(sync)emscripten_set_main_loop_timing(EM_TIMING_RAF       , 1);
         else    emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 0); // however be careful as on WEB disabling sync introduces stuttering
      #endif
   #endif
   }
}
Display& Display::sync(Bool sync) {if(T._sync!=sync){T._sync=sync; setSync();} return T;}
/******************************************************************************/
Display& Display::dither             (Bool             dither   ) {                                                                    if(T._dither          !=dither   ){T._dither          =dither   ;             } return T;}
Display& Display::maxLights          (Byte             max      ) {Clamp(max, 0, 255);                                                 if(T._max_lights      !=max      ){T._max_lights      =max      ;             } return T;}
Display& Display::texMacro           (Bool             use      ) {                                                                    if(T._tex_macro       !=use      ){T._tex_macro       =use      ; setShader();} return T;}
Display& Display::texDetail          (TEXTURE_USAGE    usage    ) {Clamp(usage, TEX_USE_DISABLE, TEXTURE_USAGE(TEX_USE_NUM-1));        if(T._tex_detail      !=usage    ){T._tex_detail      =usage    ; setShader();} return T;}
Display& Display::texDetailLOD       (Bool             on       ) {                                                                    if(T._tex_detail_lod  !=on       ){T._tex_detail_lod  =on       ; setShader();} return T;}
Display& Display::materialBlend      (Bool             per_pixel) {                                                                    if(T._mtrl_blend      !=per_pixel){T._mtrl_blend      =per_pixel; setShader();} return T;}
Display& Display::bendLeafs          (Bool             on       ) {                                                                    if(T._bend_leafs      !=on       ){T._bend_leafs      =on       ; setShader();} return T;}
Display& Display::outlineMode        (EDGE_DETECT_MODE mode     ) {Clamp(mode, EDGE_DETECT_NONE, EDGE_DETECT_MODE(EDGE_DETECT_NUM-1)); if(T._outline_mode    !=mode     ){T._outline_mode    =mode     ;             } return T;}
Display& Display::particlesSoft      (Bool             on       ) {                                                                    if(T._particles_soft  !=on       ){T._particles_soft  =on       ;             } return T;}
Display& Display::particlesSmoothAnim(Bool             on       ) {                                                                    if(T._particles_smooth!=on       ){T._particles_smooth=on       ;             } return T;}

Display& Display::eyeDistance(Flt dist)
{
   if(_eye_dist!=dist)
   {
     _eye_dist=dist; _eye_dist_2=_eye_dist/2;
      SetEyeMatrix();
      Frustum.set();
      tAAReset();
   }
   return T;
}

Display& Display::edgeDetect(EDGE_DETECT_MODE mode)
{
   Clamp(mode, EDGE_DETECT_NONE, EDGE_DETECT_MODE(EDGE_DETECT_NUM-1)); if(T._edge_detect!=mode)
   {
      T._edge_detect=mode; if(!Sh.EdgeDetect && mode && created())
      {
         Sh.EdgeDetect     =Sh.get("EdgeDetect");
         Sh.EdgeDetectApply=Sh.get("EdgeDetectApply");
      }
   }
   return T;
}
Display& Display::edgeSoften(EDGE_SOFTEN_MODE mode)
{
   Clamp(mode, EDGE_SOFTEN_NONE, EDGE_SOFTEN_MODE(EDGE_SOFTEN_NUM-1)); if(T._edge_soften!=mode)
   {
      T._edge_soften=mode; if(created())switch(mode) // techniques can be null if failed to load
      {
         case EDGE_SOFTEN_FXAA: if(!Sh.FXAA[0])
         {
            ShaderFile &sf=*ShaderFiles("FXAA");
            if(Sh.FXAA[0]=sf.find("FXAA0"))
            if(Sh.FXAA[1]=sf.find("FXAA1"))
               break; // all OK
         }break;

      #if SUPPORT_MLAA
         case EDGE_SOFTEN_MLAA: if(!Renderer._mlaa_area.is())
         {
            Sh.MLAAEdge =Sh.find("MLAAEdge" );
            Sh.MLAABlend=Sh.find("MLAABlend");
            Sh.MLAA     =Sh.find("MLAA"     );

            Renderer._mlaa_area.load("Img/MLAA Area.img");
         }break;
      #endif

         case EDGE_SOFTEN_SMAA: if(!Sh.SMAAEdge[0])
         {
            Sh.SMAAThreshold=GetShaderParam("SMAAThreshold"); Sh.SMAAThreshold->set(D.smaaThreshold());

            if(Sh.SMAAEdge[0]=Sh.find("SMAAEdge0"))
            if(Sh.SMAAEdge[1]=Sh.find("SMAAEdge1"))
            if(Sh.SMAABlend  =Sh.find("SMAABlend"))
            if(Sh.SMAA       =Sh.find("SMAA"     ))
            if(Renderer._smaa_area  .load("Img/SMAA Area.img"))
            if(Renderer._smaa_search.load("Img/SMAA Search.img"))
               break; // all OK
            // failed
            Renderer._smaa_area  .del();
            Renderer._smaa_search.del();
         }break;
      }
   }
   return T;
}
Display& Display::smaaThreshold(Flt threshold)
{
   SAT(threshold); _smaa_threshold=threshold; Sh.SMAAThreshold->setConditional(_smaa_threshold); return T;
}
Display& Display::tAA(Bool on)
{
   if(tAA()!=on)
   {
     _taa=on;
      if(!Sh.TAA[0][0][0] && on && created())
         REPD(clamp, 2)
         REPD(alpha, 2)
         REPD(dual , 2)
            Sh.TAA[clamp][alpha][dual]=Sh.get(S+"TAA"+clamp+alpha+dual+D.gatherChannelAvailable());
      tAAReset(); // clear RT's and make sure enabling will start from zero 'frame' index
   }
   return T;
}
Display& Display::tAAReset()
{
   DYNAMIC_ASSERT(Renderer._ctx    ==                    null, "'D.tAAReset' called during rendering"); // check in case this is called during rendering
     DEBUG_ASSERT(Renderer._ctx_sub==&Renderer._ctx_sub_dummy, "'D.tAAReset' called during rendering"); // check in case this is called during rendering
   Renderer._ctxs.clear();
   return T;
}
Display& Display::tAADualHistory(Bool dual) {dual=(dual!=false); if(_taa_dual!=dual){_taa_dual=dual; tAAReset();} return T;} // make sure this is bool because this is used as array index

Int      Display::secondaryOpenGLContexts(             )C {return GPU_API(0, SecondaryContexts.elms());}
Display& Display::secondaryOpenGLContexts(Byte contexts)
{
#if GL && HAS_THREADS
   if(!created())SecondaryContexts.setNum(contexts);
#endif
   return T;
}
Bool Display::canUseGPUDataOnSecondaryThread()C
{
#if GL
   return created() && SecondaryContexts.elms(); // was created and there are some secondary GL contexts
#else
   return true;
#endif
}
/******************************************************************************/
Display& Display::aspectMode(ASPECT_MODE mode)
{
   Clamp(mode, ASPECT_MODE(0), ASPECT_MODE(ASPECT_NUM-1));
   if(T._aspect_mode!=mode)
   {
      T._aspect_mode=mode;
      aspectRatioEx();
   }
   return T;
}
void Display::aspectRatioEx(Bool force, Bool quiet)
{
   Flt aspect_ratio=_disp_aspect_ratio_want;
#if DESKTOP || WEB
   RectI full, work; VecI2 max_normal_win_client_size, maximized_win_client_size;
    getMonitor(full, work, max_normal_win_client_size, maximized_win_client_size); // calculate based on current monitor, as connected monitors may have different aspects
   VecI2 size=full.size(); Flt desktop_aspect=(size.y ? Flt(size.x)/size.y : 1);

   if(aspect_ratio<=EPS)aspect_ratio=desktop_aspect; // if not specified then use default
#else
   Flt desktop_aspect=Renderer._main.aspect();
        aspect_ratio =Renderer._main.aspect();
#endif

   if(T._disp_aspect_ratio!=aspect_ratio || force)
   {
      T._disp_aspect_ratio=aspect_ratio;
      if(created())
      {
         Bool vr=(VR.active() && _allow_stereo);
	   #if DESKTOP || WEB
         Flt window_aspect=Flt(resW())/resH(),
      #else
         Flt window_aspect=Renderer._main.aspect(),
      #endif
                 vr_aspect=Flt(VR.guiRes().x)/VR.guiRes().y,
               mono_aspect=(D.full() ? aspect_ratio : aspect_ratio*window_aspect/desktop_aspect);
         Vec2     old_size=D.size();

         T._app_aspect_ratio=(vr ? vr_aspect : mono_aspect);
         switch(aspectMode())
         {
            default            : aspect_y: _unscaled_size.y=1; _unscaled_size.x=_unscaled_size.y*_app_aspect_ratio; break; // ASPECT_Y
            case ASPECT_X      : aspect_x: _unscaled_size.x=1; _unscaled_size.y=_unscaled_size.x/_app_aspect_ratio; break;
            case ASPECT_SMALLER: if(_app_aspect_ratio>=1)goto aspect_y; goto aspect_x;
         }
         T._pixel_aspect=(vr ? 1 : D.full() ? aspect_ratio/window_aspect : aspect_ratio/desktop_aspect);

         // '_window_pixel_to_screen_scale' is set so that 'windowPixelToScreen' will always point to VR 'GuiTexture', no matter if 'D._allow_stereo' is enabled/disabled (drawing to 'GuiTexture' or System Window)
         if(!VR.active())_window_pixel_to_screen_scale=1;else
         if( vr)
         {
            if(mono_aspect>vr_aspect)_window_pixel_to_screen_scale.set(mono_aspect/vr_aspect, 1);
            else                     _window_pixel_to_screen_scale.set(1, vr_aspect/mono_aspect);
         }else
         {
            if(mono_aspect>vr_aspect)_window_pixel_to_screen_scale=1;
            else                     _window_pixel_to_screen_scale=vr_aspect/mono_aspect;
         }

         sizeChanged();

         if(!quiet)screenChanged(old_size.x, old_size.y);
      }
   }
}
Display& Display::aspectRatio(Flt aspect_ratio)
{
   T._disp_aspect_ratio_want=Max(0, aspect_ratio);
   aspectRatioEx(false);
   return T;
}
/******************************************************************************/
Display& Display::texFilter(Byte filter)
{
   if(created())MIN(filter, maxTexFilter());
   if(T._tex_filter!=filter)
   {
      T._tex_filter=filter;
      if(created())
      {
      #if DX11
         CreateAnisotropicSampler();
      #elif GL
         Images.lock  (); REPA(Images)Images.lockedData(i).setGLParams();
         Images.unlock();
      #endif
      }
   }
   return T;
}
Display& Display::texMipFilter(Bool on)
{
   if(T._tex_mip_filter!=on)
   {
      T._tex_mip_filter=on;
      if(created())
      {
      #if DX11
         CreateAnisotropicSampler();
      #elif GL
         Images.lock  (); REPA(Images)Images.lockedData(i).setGLParams();
         Images.unlock();
      #endif
      }
   }
   return T;
}
Display& Display::texMipMin(Byte min)
{
   Clamp(min, 0, 16);
   if(T._tex_mip_min!=min)
   {
      T._tex_mip_min=min;
      if(created())
      {
      #if DX11
         CreateAnisotropicSampler();
      #endif
      }
   }
   return T;
}
Display& Display::texMipBias(Flt bias)
{
   if(T._tex_mip_bias!=bias)
   {
      T._tex_mip_bias=bias;
      if(created())
      {
      #if DX11
         CreateAnisotropicSampler();
      #endif
      }
   }
   return T;
}
/******************************************************************************/
Display& Display::fontSharpness(Flt value)
{
   if(T._font_sharpness!=value)
   {
      T._font_sharpness=value;
      if(created())
      {
      #if DX11
         CreateFontSampler();
      #elif GL
         Fonts.  lock(); REPA(Fonts)Fonts.lockedData(i).setGLFont();
         Fonts.unlock();
      #endif
      }
   }
   return T;
}
/******************************************************************************/
Display& Display::gamma   (Flt gamma) {if(T._gamma!=gamma){T._gamma=gamma; gammaSet();} return T;}
void     Display::gammaSet()
{
   if(created())
   {
      Flt  exp_want=ScaleFactor(gamma()*-0.5f);
      Bool separate=false; // if we can set gamma separately for the system (all monitors) and monitor in use
   #if DX11
      #if WINDOWS_OLD
         separate=exclusiveFull(); // 'SetGammaControl' will succeed only in exclusive full screen
      #else
         separate=true;
      #endif
   #endif

      // !! set system gamma first so it won't override device output gamma !!
      Flt exp=(separate ? 1 : exp_want);
      if(_gamma_all || exp!=1) // if custom gamma is already set, or we want to apply custom gamma
      {
      #if WINDOWS_OLD
         SyncLocker locker(_lock);
         if(HDC hdc=GetDC(null))
	      {
            UShort gr[3][256];
            REP(256)
            {
               gr[0][i]=RoundU(Pow(_gamma_array[0][i]/65535.0f, exp)*0xFFFF);
               gr[1][i]=RoundU(Pow(_gamma_array[1][i]/65535.0f, exp)*0xFFFF);
               gr[2][i]=RoundU(Pow(_gamma_array[2][i]/65535.0f, exp)*0xFFFF);
            }
		      SetDeviceGammaRamp(hdc, gr);
            ReleaseDC(null, hdc);
	      }
      #elif MAC
         CGGammaValue r[256], g[256], b[256];
         REP(256)
         {
            r[i]=Pow(_gamma_array[0][i]/65535.0f, exp);
            g[i]=Pow(_gamma_array[1][i]/65535.0f, exp);
            b[i]=Pow(_gamma_array[2][i]/65535.0f, exp);
         }
         SyncLocker locker(_lock);
         CGSetDisplayTransferByTable(kCGDirectMainDisplay, 256, r, g, b);
      #elif LINUX
         UShort r[256], g[256], b[256];
         REP(256)
         {
            r[i]=RoundU(Pow(_gamma_array[0][i]/65535.0f, exp)*0xFFFF);
            g[i]=RoundU(Pow(_gamma_array[1][i]/65535.0f, exp)*0xFFFF);
            b[i]=RoundU(Pow(_gamma_array[2][i]/65535.0f, exp)*0xFFFF);
         }
         XF86VidModeSetGammaRamp(XDisplay, DefaultScreen(XDisplay), 256, r, g, b);
      #endif
        _gamma_all=(exp!=1); // if gamma is set for all monitors
      }

      if(separate)
      {
         exp=exp_want;
      #if DX11
         SyncLocker locker(_lock);
         IDXGIOutput *output=null; SwapChain->GetContainingOutput(&output); if(output)
         {
            DXGI_GAMMA_CONTROL gc;
            gc.Scale .Red=gc.Scale .Green=gc.Scale .Blue=1;
            gc.Offset.Red=gc.Offset.Green=gc.Offset.Blue=0;
            REP(256)
            {
               gc.GammaCurve[i].Red  =Pow(_gamma_array[0][i]/65535.0f, exp);
               gc.GammaCurve[i].Green=Pow(_gamma_array[1][i]/65535.0f, exp);
               gc.GammaCurve[i].Blue =Pow(_gamma_array[2][i]/65535.0f, exp);
            }
            output->SetGammaControl(&gc);
            output->Release();
         }
      #endif
      }
   }
}
/******************************************************************************/
Display& Display::diffuseMode(DIFFUSE_MODE mode)
{
   Clamp(mode, DIFFUSE_MODE(0), DIFFUSE_MODE(DIFFUSE_NUM-1));
   if(_diffuse_mode!=mode){_diffuse_mode=mode; /*setShader();*/} // RT_FORWARD always uses lambert, so 'setShader' not needed
   return T;
}
/******************************************************************************/
Display& Display::bumpMode(BUMP_MODE mode)
{
   Clamp(mode, BUMP_FLAT, BUMP_MODE(BUMP_NUM-1));
   if(_bump_mode!=mode){_bump_mode=mode; setShader();}
   return T;
}
/******************************************************************************/
Display& Display:: glowAllow   (Bool allow   ) {if(_glow_allow!=allow){_glow_allow   =allow   ; tAAReset();} return T;} // 'glowAllow' affects type of TAA RT's #RTOutput
Display& Display::bloomAllow   (Bool allow   ) {                      _bloom_allow   =allow   ;              return T;}
Display& Display::bloomOriginal(Flt  original) {MAX  (original, 0);   _bloom_original=original;              return T;}
Display& Display::bloomScale   (Flt  scale   ) {MAX  (scale   , 0);   _bloom_scale   =scale   ;              return T;}
Display& Display::bloomCut     (Flt  cut     ) {MAX  (cut     , 0);   _bloom_cut     =cut     ;              return T;}
Display& Display::bloomHalf    (Bool half    ) {                      _bloom_half    =half    ;              return T;}
Display& Display::bloomBlurs   (Byte blurs   ) {Clamp(blurs, 0, 4);   _bloom_blurs   =blurs   ;              return T;}
Display& Display::bloomSamples (Bool high    ) {                      _bloom_samples =high    ;              return T;}
Display& Display::bloomMaximum (Bool on      ) {if(_bloom_max!=on){   _bloom_max     =on      ; if(!Sh.MaxX && on && created()){Sh.MaxX=Sh.get("MaxX"); Sh.MaxY=Sh.get("MaxY");}} return T;}
Bool     Display::bloomUsed    (             )C{return bloomAllow() && (!Equal(bloomOriginal(), 1, EPS_COL) || !Equal(bloomScale(), 0, EPS_COL));}
/******************************************************************************/
Display& Display::volLight(Bool on ) {_vol_light=    on     ; return T;}
Display& Display::volAdd  (Bool add) {_vol_add  =    add    ; return T;}
Display& Display::volMax  (Flt  max) {_vol_max  =Max(max, 0); return T;}
/******************************************************************************/
Display& Display::shadowMode         (SHADOW_MODE mode) {Clamp(mode, SHADOW_NONE, SHADOW_MODE(SHADOW_NUM-1)); _shd_mode=mode;             return T;}
Display& Display::shadowJitter       (Bool     on     ) {           if(_shd_jitter!=on){_shd_jitter   ^=1;   shadowJitterSet();         } return T;}
Display& Display::shadowReduceFlicker(Bool     reduce ) {                               _shd_reduce    =reduce;                           return T;}
Display& Display::shadowFrac         (Flt      frac   ) {SAT(frac); if(_shd_frac!=frac){_shd_frac      =frac; shadowRangeSet();         } return T;}
Display& Display::shadowFade         (Flt      fade   ) {SAT(fade); if(_shd_fade!=fade){_shd_fade      =fade; shadowRangeSet();         } return T;}
Display& Display::shadowSoft         (Byte     soft   ) {                               _shd_soft      =Min(soft   , SHADOW_SOFT_NUM-1);  return T;}
Display& Display::shadowMapNum       (Byte     map_num) {                               _shd_map_num   =Mid(map_num,    1,    6       );  return T;}
Display& Display::shadowMapSizeLocal (Flt      frac   ) {                               _shd_map_size_l=Sat(frac                      );  return T;}
Display& Display::shadowMapSizeCone  (Flt      factor ) {                               _shd_map_size_c=Mid(factor , 0.0f, 2.0f       );  return T;}
Display& Display::shadowMapSplit     (C Vec2  &factor ) {                               _shd_map_split .set(Max(2, factor.x), Max(0, factor.y)); return T;}

Display& Display::shadowMapSize(Int map_size)
{
   MAX(map_size, 0); if(_shd_map_size!=map_size){_shd_map_size=map_size; if(created())Renderer.createShadowMap();}
   return T;
}
Display& Display::cloudsMapSize(Int map_size)
{
   MAX(map_size, 0); if(_cld_map_size!=map_size){_cld_map_size=map_size; if(created())Renderer.createShadowMap();}
   return T;
}
Int Display::shadowMapNumActual()C
{
   return (Renderer._cur_type==RT_FORWARD) ? Ceil2(shadowMapNum()) // align to even numbers on RT_FORWARD
                                           :       shadowMapNum();
}
Bool Display::shadowSupported()C
{
   return Renderer._shd_map.is();
}
void Display::shadowJitterSet()
{
   Vec4 j;
   j.xy=Flt(shadowJitter())/Renderer._shd_map.hwSize(); // mul
   j.zw=j.xy*-0.5f; // add
   Sh.ShdJitter->set(j);
}
void Display::shadowRangeSet()
{
   Sh.ShdRange->setConditional(_shd_range=D.shadowFrac()*D.viewRange());
   {
      Flt from=_shd_range*D.shadowFade(),
          to  =_shd_range;
      if(from>=D.viewRange()-EPSL) // disabled
      {
         Sh.ShdRangeMulAdd->setConditional(Vec2Zero);
      }else
      {
         MAX(to, from+0.01f);
         from*=from; to*=to;
         Flt mul=1/(to-from), add=-from*mul;
         Sh.ShdRangeMulAdd->setConditional(Vec2(mul, add));
      }
   }
}
/******************************************************************************/
Bool Display::aoWant()C
{
   return ambientMode    ()!=AMBIENT_FLAT
      &&  ambientContrast()> EPS_COL8
      && (aoAll() || (ambientColorD()+nightShadeColorD()).max()>EPS_COL8_NATIVE); // no need to calculate AO if it's too small
}
Flt Display::ambientRes   ()C {return ByteScaleToFlt(_amb_res);}
Flt Display::ambientPowerS()C {return LinearToSRGB(ambientPowerL());}
Vec Display::ambientColorS()C {return LinearToSRGB(ambientColorL());}

void Display::ambientSet()C
{
   Sh.AmbientColor_l ->set(   ambientColorD());
   Sh.NightShadeColor->set(nightShadeColorD());
   // in the shader, night shade is applied as "night_shade * Sat(1-lum)" (to be applied only for low lights), so night shade is limited by light, since light is per-pixel and depends on ambient and dynamic lights, we don't know the exact value, however we know ambient, so here limit according to ambient only
   Flt max_lum=ambientColorD().max(), intensity=Sat(1-max_lum);
   Sh.AmbientColorNS_l->set(ambientColorD() + nightShadeColorD()*intensity);
}
void Display::ambientSetRange()C
{
   Sh.AmbientRange_2    ->set(         D.ambientRange()/2);
   Sh.AmbientRangeInvSqr->set(Sqr(1.0f/D.ambientRange()) );
}

Display& Display::ambientRes     (  Flt          scale     ) {Byte res=FltToByteScale( scale  );    if(res!=_amb_res){_amb_res =res ; Renderer.rtClean();} return T;}
Display& Display::ambientMode    (  AMBIENT_MODE mode      ) {Clamp(mode, AMBIENT_FLAT, AMBIENT_MODE(AMBIENT_NUM-1)); _amb_mode=mode;                      return T;}
Display& Display::ambientSoft    (  Byte         soft      ) {MIN  (soft,                       AMBIENT_SOFT_NUM-1 ); _amb_soft=soft;                      return T;}
Display& Display::ambientJitter  (  Bool         jitter    ) {_amb_jitter=jitter;                                                                          return T;}
Display& Display::ambientNormal  (  Bool         normal    ) {_amb_normal=normal;                                                                          return T;}
Display& Display::ambientPowerS  (  Flt          srgb_power) {return ambientPowerL(SRGBToLinear(srgb_power));}
Display& Display::ambientColorS  (C Vec         &srgb_color) {return ambientColorL(SRGBToLinear(srgb_color));}
Display& Display::ambientPowerL  (  Flt           lin_power) {MAX(lin_power, 0);                                                    if(_amb_color_l !=lin_power){_amb_color_l =lin_power; ambientSet();} return T;}
Display& Display::ambientColorL  (C Vec         & lin_color) {Vec c(Max(lin_color.x, 0), Max(lin_color.y, 0), Max(lin_color.z, 0)); if(_amb_color_l !=c        ){_amb_color_l =c        ; ambientSet();} return T;}
Display& Display::ambientContrast(  Flt          contrast  ) {MAX(contrast, 0);                                                     if(_amb_contrast!=contrast ){_amb_contrast=contrast ; Sh.AmbientContrast->set(ambientContrast());} return T;}
Display& Display::ambientMin     (  Flt          min       ) {SAT(min        );                                                     if(_amb_min     !=min      ){_amb_min     =min      ; Sh.AmbientMin     ->set(ambientMin     ());} return T;}
Display& Display::ambientRange   (  Flt          range     ) {MAX(range   , 0);                                                     if(_amb_range   !=range    ){_amb_range   =range    ; ambientSetRange();} return T;}
/******************************************************************************/
Vec      Display::nightShadeColorS(                 )C {return LinearToSRGB(nightShadeColorL());}
Display& Display::nightShadeColorS(C Vec &srgb_color)  {return nightShadeColorL(SRGBToLinear(srgb_color));}
Display& Display::nightShadeColorL(C Vec & lin_color)  {Vec c(Max(lin_color.x, 0), Max(lin_color.y, 0), Max(lin_color.z, 0)); if(_ns_color_l!=c){_ns_color_l=c; ambientSet();} return T;}
/******************************************************************************/
Display& Display::envColor(C Vec      &color) {if(_env_color!=color)                    Sh.EnvColor->set( _env_color=color  );                                                                                          return T;}
Display& Display::envMap  (C ImagePtr &cube ) {if(_env_map  !=cube ){Bool was=_env_map; Sh.Env     ->set((_env_map  =cube)()); if(cube)Sh.EnvMipMaps->setConditional(cube->mipMaps()-1); if(was!=_env_map)setShader();} return T;} // if changed map presence then reset shader
/******************************************************************************/
Flt      Display::motionRes   (                  )C {return   ByteScaleToFlt(_mtn_res);}
Display& Display::motionRes   (Flt         scale )  {Byte res=FltToByteScale(scale); if(res!=_mtn_res){_mtn_res=res; Renderer.rtClean();}               return T;}
Display& Display::motionMode  (MOTION_MODE mode  )  {Clamp(mode , MOTION_NONE   , MOTION_MODE(MOTION_NUM-1));                       _mtn_mode  =mode ;  return T;}
Display& Display::motionDilate(DILATE_MODE mode  )  {Clamp(mode , DILATE_MODE(0), DILATE_MODE(DILATE_NUM-1));                       _mtn_dilate=mode ;  return T;}
Display& Display::motionScale (Flt         scale )  {MAX  (scale, 0                                        ); if(_mtn_scale!=scale){_mtn_scale =scale;} return T;}
/******************************************************************************/
Display& Display::dofMode     (DOF_MODE mode     ) {Clamp(mode, DOF_NONE, DOF_MODE(DOF_NUM-1)); _dof_mode     =mode             ; return T;}
Display& Display::dofFocusMode(Bool     realistic) {                                            _dof_foc_mode =(realistic!=0)   ; return T;}
Display& Display::dofFocus    (Flt      z        ) {                                            _dof_focus    =Max(z        , 0); return T;}
Display& Display::dofRange    (Flt      range    ) {                                            _dof_range    =Max(range    , 0); return T;}
Display& Display::dofIntensity(Flt      intensity) {                                            _dof_intensity=Max(intensity, 0); return T;}
/******************************************************************************/
Display& Display::eyeAdaptation          (  Bool on        ) {                                                            _eye_adapt           =on        ;                                                    return T;}
Display& Display::eyeAdaptationBrightness(  Flt  brightness) {MAX  (brightness, 0); if(_eye_adapt_brightness!=brightness){_eye_adapt_brightness=brightness; Sh.HdrBrightness->set(eyeAdaptationBrightness());} return T;}
Display& Display::eyeAdaptationExp       (  Flt  exp       ) {Clamp(exp, 0.1f , 1); if(_eye_adapt_exp       !=exp       ){_eye_adapt_exp       =exp       ; Sh.HdrExp       ->set(eyeAdaptationExp       ());} return T;}
Display& Display::eyeAdaptationMaxDark   (  Flt  max_dark  ) {MAX  (max_dark  , 0); if(_eye_adapt_max_dark  !=max_dark  ){_eye_adapt_max_dark  =max_dark  ; Sh.HdrMaxDark   ->set(eyeAdaptationMaxDark   ());} return T;}
Display& Display::eyeAdaptationMaxBright (  Flt  max_bright) {MAX  (max_bright, 0); if(_eye_adapt_max_bright!=max_bright){_eye_adapt_max_bright=max_bright; Sh.HdrMaxBright ->set(eyeAdaptationMaxBright ());} return T;}
Display& Display::eyeAdaptationSpeed     (  Flt  speed     ) {MAX  (speed     , 1); if(_eye_adapt_speed     !=speed     ){_eye_adapt_speed     =speed     ;                                                  } return T;}
Display& Display::eyeAdaptationWeight    (C Vec &weight    ) {                      if(_eye_adapt_weight    !=weight    ){_eye_adapt_weight    =weight    ; Sh.HdrWeight    ->set(eyeAdaptationWeight()/4  );} return T;}
Display& Display::resetEyeAdaptation     (  Flt  brightness)
{
   if(Renderer._eye_adapt_scale[0].is())
   {
      MAX(brightness, 0);
      SyncLocker locker(_lock);
      REPAO(Renderer._eye_adapt_scale).clearFull(brightness, true);
   }
   return T;
}
/******************************************************************************/
Display& Display::grassDensity(Flt  density) {_grass_density=Sat(density); return T;}
Display& Display::grassShadow (Bool on     ) {_grass_shadow =    on      ; return T;}
Display& Display::grassMirror (Bool on     ) {_grass_mirror =    on      ; return T;}
Display& Display::grassRange  (Flt  range  )
{
   MAX(range, 0); if(_grass_range!=range)
   {
     _grass_range    =range;
     _grass_range_sqr=Sqr(_grass_range);
      Flt from=Sqr(_grass_range*0.8f), to=_grass_range_sqr, mul, add; if(to>from+EPSL){mul=1/(to-from); add=-from*mul;}else{mul=0; add=1;} // else{no grass} because distance is 0
      Sh.GrassRangeMulAdd->set(Vec2(mul, add));
   }
   return T;
}
static Flt BendFactor;
Display& Display::grassUpdate()
{
   BendFactor+=Time.d();
   Vec4 bf=Vec4(1.6f, 1.2f, 1.4f, 1.1f)*BendFactor+Vec4(0.1f, 0.5f, 0.7f, 1.1f);
#if 1 // increase precision on GPU when using Half's
   bf.x=AngleFull(bf.x);
   bf.y=AngleFull(bf.y);
   bf.z=AngleFull(bf.z);
   bf.w=AngleFull(bf.w);
#endif
   Sh.BendFactorPrev->set(Sh.BendFactor->getVec4());
   Sh.BendFactor    ->set(bf);
   return T;
}
/******************************************************************************/
Display& Display::furStaticGravity (Flt gravity  ) {_fur_gravity  =gravity  ; return T;}
Display& Display::furStaticVelScale(Flt vel_scale) {_fur_vel_scale=vel_scale; return T;}
/******************************************************************************/
void Display::lodSetCurrentFactor()
{
   // '_lod_current_factor' contains information about '_lod_factors_fov' in current rendering mode
  _lod_current_factor=_lod_factors_fov[Renderer.mirror()];
}
void Display::lodUpdateFactors()
{
   // '_lod_fov2' is a value based on Fov (viewFovY()), squared
   if(FovPerspective(viewFovMode()))_lod_fov2=Sqr(Tan(viewFovY()*0.5f));
   else                             _lod_fov2=Sqr(    viewFovY()      );
   REPD(m, 2)_lod_factors_fov[m]=_lod_factors[m]*_lod_fov2;
   lodSetCurrentFactor();
}
Display& Display::lod(Flt general, Flt mirror)
{
   // set values
  _lod_factor       =Max(0, general);
  _lod_factor_mirror=Max(0, mirror );

   // build precomputed helper array
   REPD(m, 2)
   {
      Flt &factor =_lod_factors[m];
           factor =_lod_factor;
      if(m)factor*=_lod_factor_mirror;
           factor*=     factor; // make square
   }
   lodUpdateFactors();
   return T;
}
Display& Display::lodFactor      (Flt factor) {return lod(     factor, _lod_factor_mirror);}
Display& Display::lodFactorMirror(Flt factor) {return lod(_lod_factor,      factor       );}
/******************************************************************************/
Display& Display::tesselationAllow    (Bool on     ) {                       if(_tesselation_allow    !=on     ) _tesselation_allow    =on     ;               return T;}
Display& Display::tesselation         (Bool on     ) {                       if(_tesselation          !=on     ){_tesselation          =on     ; setShader();} return T;}
Display& Display::tesselationHeightmap(Bool on     ) {                       if(_tesselation_heightmap!=on     ){_tesselation_heightmap=on     ; setShader();} return T;}
Display& Display::tesselationDensity  (Flt  density) {MAX(density, EPS_GPU); if(_tesselation_density  !=density){_tesselation_density  =density; Sh.TesselationDensity->set(tesselationDensity());} return T;}
/******************************************************************************/
void Display::setViewFovTan()
{
   Vec2 mul(Renderer.resW()/(_view_active.recti.w()*w()), Renderer.resH()/(_view_active.recti.h()*h()));
  _view_fov_tan_full=_view_active.fov_tan*mul;

   if(VR.active() && _allow_stereo
   && !(Renderer.inside() && !Renderer._stereo) // if we're inside rendering, and stereo is disabled, then we need to process this normally, this is so that we can do mono rendering in Gui, and use correct '_view_fov_tan' (PosToScreen,ScreenToPos) in that space
   )
   {
      Flt scale=VR.guiSize()*0.5f, aspect=VR.guiRes().divF();
     _view_fov_tan_gui.x=scale*aspect;
     _view_fov_tan_gui.y=scale;
      if(Renderer.inside() && Renderer._stereo)_view_fov_tan_gui.x*=0.5f;
     _view_fov_tan_gui*=mul;
   }else
   {
     _view_fov_tan_gui=_view_fov_tan_full;
   }

   Sh.DepthWeightScale->set(_view_active.fov_tan.y*0.00714074011f);
}
void Display::viewUpdate()
{
  _view_active=_view_main;
   if(_lock.owned())_view_active.setViewport(); // set actual viewport only if we own the lock, this is because this method can be called outside of 'Draw' where we don't have the lock, however to avoid locking which could affect performance (for example GPU still owning the lock on other thread, for example for flipping back buffer, we would have to wait until it finished), we can skip setting the viewport because drawing is not allowed in update anyway. To counteract this skip here, instead we always reset the viewport at the start of Draw in 'DrawState'
  _view_active.setShader().setProjMatrix(); Frustum.set();

  _view_from  =(FovPerspective(viewFovMode()) ? viewFrom() : 0         );
  _view_fov   =(FovHorizontal (viewFovMode()) ? viewFovX() : viewFovY());
  _view_rect  =Renderer.pixelToScreen(viewRectI());
  _view_center=_view_rect.center();

   REPAO(_view_eye_rect)=_view_rect;
  _view_eye_rect[0].max.x=_view_eye_rect[1].min.x=_view_rect.centerX();

   setViewFovTan   ();
   validateCoords  ();
   lodUpdateFactors();
   shadowRangeSet  ();
}
void Display::viewReset()
{
   Rect rect=viewRect(); _view_main.recti.set(0, -1); _view_rect.set(0, -1); if(_view_main.full)viewRect(null);else viewRect(rect); // if last was full then set full, otherwise set based on rect
}

Display& Display::view(C Rect  &rect, Flt from, Flt range, C Vec2 &fov, FOV_MODE fov_mode) {_view_main.set(screenToPixelI(rect), from, range, fov, fov_mode); viewUpdate(); return T;}
Display& Display::view(C RectI &rect, Flt from, Flt range, C Vec2 &fov, FOV_MODE fov_mode) {_view_main.set(               rect , from, range, fov, fov_mode); viewUpdate(); return T;}

Display& Display::viewRect (C RectI *rect              ) {RectI recti=(rect ? *rect : RectI(0, 0, Renderer.resW(), Renderer.resH())); if( viewRectI()      !=recti                        ){                       _view_main.setRect (recti      ).setFov(); viewUpdate();} return T;} // need to use 'Renderer.res' in case VR is enabled and we're rendering to its 'GuiTexture'
Display& Display::viewRect (C Rect  &rect              ) {RectI recti=screenToPixelI(rect);                                           if( viewRectI()      !=recti                        ){                       _view_main.setRect (recti      ).setFov(); viewUpdate();} return T;}
Display& Display::viewFrom (  Flt    from              ) {                                                                            if( viewFrom ()      !=from                         ){                       _view_main.setFrom (from       )         ; viewUpdate();} return T;}
Display& Display::viewRange(  Flt    range             ) {                                                                            if( viewRange()      !=range                        ){                       _view_main.setRange(range      )         ; viewUpdate();} return T;}
Display& Display::viewFov  (  Flt    fov, FOV_MODE mode) {                                                                            if( viewFov  ()      !=fov  || viewFovMode()!=mode  ){                       _view_main.setFov  (fov, mode  )         ; viewUpdate();} return T;}
Display& Display::viewFov  (C Vec2  &fov               ) {                                                                            if( viewFovXY()      !=fov  || viewFovMode()!=FOV_XY){                       _view_main.setFov  (fov, FOV_XY)         ; viewUpdate();} return T;}
Display& Display::viewForceSquarePixel(Bool on         ) {                                                                            if(_view_square_pixel!=on                           ){_view_square_pixel=on; _view_main.setFov  (           )         ; viewUpdate();} return T;}

Flt Display::viewQuadDist()C
{
   Vec    view_quad_max(_view_main.fov_tan.x*_view_main.from, _view_main.fov_tan.y*_view_main.from, _view_main.from);
   return view_quad_max.length();
}
/******************************************************************************/
// CLEAR
/******************************************************************************/
#define CLEAR_DEPTH_VALUE (!REVERSE_DEPTH) // Warning: for GL this is set at app startup using 'glClearDepth' and not here
void Display::clear(C Color &srgb_color)
{
   clearCol(srgb_color);
   clearDS (          );
}
void Display::clearCol(C Color &srgb_color) {return clearCol(SRGBToDisplay(srgb_color));}
void Display::clearCol(C Vec4  &     color)
{
   if(Renderer._cur[0])
   {
   #if DX11
      if(D._view_active.full)Renderer._cur[0]->clearHw(color);else
      {
         Bool clip=D._clip_allow; D.clipAllow(false); ALPHA_MODE alpha=D.alpha(ALPHA_NONE); Sh.clear(color);
                                  D.clipAllow(clip );                  D.alpha(alpha     );
      }
   #elif GL
      if(D._clip_real)glDisable(GL_SCISSOR_TEST); // scissor test affects clear on OpenGL so we need to temporarily disable it to have the same functionality as in DirectX
      if(D._view_active.full)
      {
         glClearColor(color.x, color.y, color.z, color.w); // even though doc states that values are clamped to the range [0,1] - https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glClearColor.xhtml tests on Windows show they're not clamped, which is good because we might need it
         glClear(GL_COLOR_BUFFER_BIT);
      }else
      {
         ALPHA_MODE alpha=D.alpha(ALPHA_NONE); Sh.clear(color);
                          D.alpha(alpha     );
      }
      if(D._clip_real)glEnable(GL_SCISSOR_TEST);
   #endif
   }
}

void Display::clearCol(Int i, C Vec4 &color) // !! this ignores the viewport !!
{
   DEBUG_RANGE_ASSERT(i, Renderer._cur);
#if DX11
   if(ImageRT *image=Renderer._cur[i])image->clearHw(color);
#elif GL
   if(Renderer._cur[i]){if(D._clip_real)glDisable(GL_SCISSOR_TEST); glClearBufferfv(GL_COLOR, i, color.c); if(D._clip_real)glEnable(GL_SCISSOR_TEST);} // 'glClearBufferfv' always clears full RT (viewport is ignored)
#endif
}
#if DX11
// DX10+ 'clearDepth' always clears full depth buffer (viewport is ignored)
void Display::clearDepth  (      ) {if(Renderer._cur_ds)D3DC->ClearDepthStencilView(Renderer._cur_ds->_dsv, D3D11_CLEAR_DEPTH                                                             , CLEAR_DEPTH_VALUE, 0);}
void Display::clearDS     (Byte s) {if(Renderer._cur_ds)D3DC->ClearDepthStencilView(Renderer._cur_ds->_dsv, D3D11_CLEAR_DEPTH|(Renderer._cur_ds->hwTypeInfo().s ? D3D11_CLEAR_STENCIL : 0), CLEAR_DEPTH_VALUE, s);}
void Display::clearStencil(Byte s) {if(Renderer._cur_ds)D3DC->ClearDepthStencilView(Renderer._cur_ds->_dsv,                                                       D3D11_CLEAR_STENCIL     , CLEAR_DEPTH_VALUE, s);}
#elif GL
// GL 'clearDepth' always clears full depth buffer (viewport is ignored)
// Don't check for '_cur_ds_id' because this can be 0 for RenderBuffers
void Display::clearDepth  (      ) {if(Renderer._cur_ds){if(D._clip_real)glDisable(GL_SCISSOR_TEST);                    glClear(GL_DEPTH_BUFFER_BIT                                                           ); if(D._clip_real)glEnable(GL_SCISSOR_TEST);}}
void Display::clearDS     (Byte s) {if(Renderer._cur_ds){if(D._clip_real)glDisable(GL_SCISSOR_TEST); glClearStencil(s); glClear(GL_DEPTH_BUFFER_BIT|(Renderer._cur_ds->hwTypeInfo().s?GL_STENCIL_BUFFER_BIT:0)); if(D._clip_real)glEnable(GL_SCISSOR_TEST);}}
void Display::clearStencil(Byte s) {if(Renderer._cur_ds){if(D._clip_real)glDisable(GL_SCISSOR_TEST); glClearStencil(s); glClear(                                                      GL_STENCIL_BUFFER_BIT   ); if(D._clip_real)glEnable(GL_SCISSOR_TEST);}}
#endif
/******************************************************************************/
// CONVERT COORDINATES
/******************************************************************************/
Rect ImgClamp(C Rect &screen, C VecI2 &size)
{
   Rect r((screen.min.x+D.w())*size.x/D.w2(), (D.h()-screen.max.y)*size.y/D.h2(),
          (screen.max.x+D.w())*size.x/D.w2(), (D.h()-screen.min.y)*size.y/D.h2());
   RectI ri=RoundGPU(r);
   r.min=(ri.min+0.5f)/size; // yes +0.5 is needed
   r.max=(ri.max-0.5f)/size; // yes -0.5 is needed
   return r;
}

Vec2 Display::screenToUV(C Vec2 &screen)
{
   return Vec2((screen.x+D.w())/D.w2(),
               (D.h()-screen.y)/D.h2());
}
Rect Display::screenToUV(C Rect &screen)
{
   return Rect((screen.min.x+D.w())/D.w2(), (D.h()-screen.max.y)/D.h2(),
               (screen.max.x+D.w())/D.w2(), (D.h()-screen.min.y)/D.h2());
}
Vec2 Display::UVToScreen(C Vec2 &uv)
{
   return Vec2(uv.x*D.w2()-D.w(),
               D.h()-uv.y*D.h2());
}

Rect ScreenToPixel(C Rect &screen, C VecI2 &res)
{
   return Rect((screen.min.x+D.w())*res.x/D.w2(), (D.h()-screen.max.y)*res.y/D.h2(),
               (screen.max.x+D.w())*res.x/D.w2(), (D.h()-screen.min.y)*res.y/D.h2());
}
Vec2 Display::screenToPixel(C Vec2 &screen)
{
   return Vec2((screen.x+D.w())*D._pixel_size_inv.x,
               (D.h()-screen.y)*D._pixel_size_inv.y);
}
Rect Display::screenToPixel(C Rect &screen)
{
   return Rect((screen.min.x+D.w())*D._pixel_size_inv.x, (D.h()-screen.max.y)*D._pixel_size_inv.y,
               (screen.max.x+D.w())*D._pixel_size_inv.x, (D.h()-screen.min.y)*D._pixel_size_inv.y);
}

// use 'Round' instead of 'Floor' to match GPU vertex rounding, so that all 3 ways of rect drawing will be identical: Rect r; 1) r.draw 2) D.clip(r) full_rect.draw() 3) D.viewRect(r) full_rect.draw()
RectI          ScreenToPixelI(C Rect &screen, C VecI2 &res) {return RoundGPU(ScreenToPixel(screen, res));}
VecI2 Display::screenToPixelI(C Vec2 &screen              ) {return RoundGPU(screenToPixel(screen));}
RectI Display::screenToPixelI(C Rect &screen              ) {return RoundGPU(screenToPixel(screen));}

Vec2 Display::pixelToScreen(C Vec2 &pixel)
{
   return Vec2(pixel.x*D._pixel_size.x-D.w(),
               D.h()-pixel.y*D._pixel_size.y);
}
Vec2 Display::pixelToScreen(C VecI2 &pixel)
{
   return Vec2(pixel.x*D._pixel_size.x-D.w(),
               D.h()-pixel.y*D._pixel_size.y);
}
Rect Display::pixelToScreen(C Rect &pixel)
{
   return Rect(pixel.min.x*D._pixel_size.x-D.w(), D.h()-pixel.max.y*D._pixel_size.y,
               pixel.max.x*D._pixel_size.x-D.w(), D.h()-pixel.min.y*D._pixel_size.y);
}
Rect Display::pixelToScreen(C RectI &pixel)
{
   return Rect(pixel.min.x*D._pixel_size.x-D.w(), D.h()-pixel.max.y*D._pixel_size.y,
               pixel.max.x*D._pixel_size.x-D.w(), D.h()-pixel.min.y*D._pixel_size.y);
}
Vec2 Display::screenToPixelSize(C Vec2  &screen) {return screen*D._pixel_size_inv;}
Vec2 Display::pixelToScreenSize(  Flt    pixel ) {return  pixel*D._pixel_size    ;}
Vec2 Display::pixelToScreenSize(C Vec2  &pixel ) {return  pixel*D._pixel_size    ;}
Vec2 Display::pixelToScreenSize(C VecI2 &pixel ) {return  pixel*D._pixel_size    ;}

Vec2 Display::windowPixelToScreen(C Vec2 &pixel) // this is used by mouse/touch pointers
{
   return Vec2(pixel.x*D._window_pixel_to_screen_mul.x+D._window_pixel_to_screen_add.x,
               pixel.y*D._window_pixel_to_screen_mul.y+D._window_pixel_to_screen_add.y);
}
Vec2 Display::windowPixelToScreen(C VecI2 &pixel) // this is used by mouse/touch pointers
{
   return Vec2(pixel.x*D._window_pixel_to_screen_mul.x+D._window_pixel_to_screen_add.x,
               pixel.y*D._window_pixel_to_screen_mul.y+D._window_pixel_to_screen_add.y);
}
VecI2 Display::screenToWindowPixelI(C Vec2 &screen)
{
   return VecI2(Round((screen.x-D._window_pixel_to_screen_add.x)/D._window_pixel_to_screen_mul.x),
                Round((screen.y-D._window_pixel_to_screen_add.y)/D._window_pixel_to_screen_mul.y));
}
RectI Display::screenToWindowPixelI(C Rect &screen)
{  // since Y gets flipped, we need to swap min.y <-> max.y
   return RectI(Round((screen.min.x-D._window_pixel_to_screen_add.x)/D._window_pixel_to_screen_mul.x),
                Round((screen.max.y-D._window_pixel_to_screen_add.y)/D._window_pixel_to_screen_mul.y),
                Round((screen.max.x-D._window_pixel_to_screen_add.x)/D._window_pixel_to_screen_mul.x),
                Round((screen.min.y-D._window_pixel_to_screen_add.y)/D._window_pixel_to_screen_mul.y));
}

/* following functions were based on these codes:

   static Image img; if(!img.is()){img.create2D(16, 16, IMAGE_R8G8B8A8, 1); img.lock(); REPD(y,img.h())REPD(x,img.w())img.color(x, y, (x^y)&1 ? WHITE : BLACK); img.unlock();}
   for(Int i=0; i<D.resW(); i+=2)
   {
      Vec2 pos=D.pixelToScreen(Vec2(i*1.111f+0.5f, 0)), s=D.pixelToScreenSize(VecI2(1)), p;
      p=pos;
      Rect_LU(p.x, p.y, s.x, 0.1f).draw(RED, true);
      D.lineY(RED, p.x, p.y-0.2f, p.y-0.3f);

      p=pos-Vec2(0, 0.1f); D.screenAlignToPixel (p); Rect_LU(p.x, p.y, s.x, 0.1f).draw(RED, true);
      p=pos-Vec2(0, 0.1f); D.screenAlignToPixelL(p); D.lineY(RED, p.x, p.y-0.2f, p.y-0.3f);

      p=pos-Vec2(0, 0.4f); D.screenAlignToPixel(p);
      if(!(i%16))img.draw(Rect_LU(Vec2(p.x, p.y), D.pixelToScreenSize(img.size())));
   }
   D.text(0, D.h()-0.05f, "Rect");
   D.text(0, D.h()-0.15f, "Rect aligned");
   D.text(0, D.h()-0.25f, "Line");
   D.text(0, D.h()-0.35f, "Line aligned");
*/

Vec2 Display::screenAlignedToPixel      (C Vec2 &screen  ) {return pixelToScreen(screenToPixelI(screen));}
Vec2 Display::  alignScreenToPixelOffset(C Vec2 &screen  ) {return  screenAlignedToPixel(screen)-screen;}
void Display::  alignScreenToPixel      (  Vec2 &screen  ) {screen =screenAlignedToPixel(screen);}
void Display::  alignScreenToPixel      (  Rect &screen  ) {screen+=alignScreenToPixelOffset(screen.lu());}
void Display::  alignScreenXToPixel     (  Flt  &screen_x)
{
   Int pixel=RoundGPU((screen_x+D.w())*D._pixel_size_inv.x); // use 'RoundGPU' to match 'screenToPixelI'
    screen_x=pixel*D._pixel_size.x-D.w();
}
void Display::alignScreenYToPixel(Flt &screen_y)
{
   Int pixel=RoundGPU((D.h()-screen_y)*D._pixel_size_inv.y); // use 'RoundGPU' to match 'screenToPixelI'
    screen_y=D.h()-pixel*D._pixel_size.y;
}
/******************************************************************************/
// FADE
/******************************************************************************/
Bool Display::fading()C {return Renderer._fade || _fade_get;}
void Display::setFade(Flt seconds, Bool previous_frame)
{
   if(!VR.active()) // fading is currently not supported in VR mode, because fading operates on a static image that's not rotating when moving headset
   {
      if(seconds<=0)
      {
         clearFade();
      }else
      if(previous_frame)
      {
      #if 0 // copy 'Renderer._main' from previous frame (not supported on GL ES or when calling 'discard' after 'D.flip')
         if(Renderer._main.is())
         {
            SyncLocker locker(_lock);
            Renderer._fade.get(ImageRTDesc(Renderer._main.w(), Renderer._main.h(), IMAGERT_SRGB)); // doesn't use Alpha
            Renderer._ptr_main->copyHw(*Renderer._fade, true);
           _fade_get =false  ;
           _fade_step=0      ;
           _fade_len =seconds;
         }
      #else // draw now
        _fade_get=false; // disable before calling 'fadeDraw'
         if(created() && StateActive && StateActive->draw)
         {
            ImageRT *cur_main=Renderer._cur_main, *cur_main_ds=Renderer._cur_main_ds, *ui=Renderer._ui, *ui_ds=Renderer._ui_ds;
            SyncLocker locker(_lock);
            {
               ImageRTPtr temp(ImageRTDesc(Renderer._main.w(), Renderer._main.h(), IMAGERT_SRGB)); // doesn't use Alpha, use a temporary instead of 'Renderer._fade' because we might still need it
               ImageRTPtr ds; ds.getDS(temp->w(), temp->h(), temp->samples()); // this will call 'discard', this is needed to hold ref count until DS is no longer needed
               Renderer._ui   =Renderer._cur_main   =temp;
               Renderer._ui_ds=Renderer._cur_main_ds=ds;
               Renderer.set(temp, ds, true); // draw directly to new RT
               StateActive->draw();
               Renderer.cleanup1();
               fadeDraw(); // draw old fade if any
              _fade_step=0; _fade_len=seconds; // set after calling 'fadeDraw'
               Swap(Renderer._fade, temp); // swap RT as new fade
            } // <- 'discard' will be called for 'temp' and 'ds'
            Renderer._cur_main   =cur_main   ; Renderer._ui   =ui   ;
            Renderer._cur_main_ds=cur_main_ds; Renderer._ui_ds=ui_ds;
            Renderer.set(Renderer._cur_main, Renderer._cur_main_ds, true); // set RT's after 'temp' and 'ds' are discarded
         }
      #endif
      }else
      {
        _fade_get=true;
        _fade_len=seconds;
      }
   }
}
void Display::clearFade()
{
   Renderer._fade.clear();
  _fade_get=false;
  _fade_step=_fade_len=0;
}
void Display::fadeUpdate()
{
   if(Renderer._fade && (_fade_step+=Time.ad()/_fade_len)>=1)clearFade();
}
void Display::fadeDraw()
{
   if(Renderer._fade)
   {
      Sh.Step->set(1-_fade_step);
      Sh.DrawA->draw(Renderer._fade);
   }
   if(_fade_get)
   {
     _fade_get =false;
     _fade_step=0    ;
      Renderer._fade.get(ImageRTDesc(Renderer._main.w(), Renderer._main.h(), IMAGERT_SRGB)); // doesn't use Alpha
      Renderer._ptr_main->copyHw(*Renderer._fade, true);
   }
}
/******************************************************************************/
// COLOR PALETTE
/******************************************************************************/
static void SetPalette(Int index, C ImagePtr &palette) // !! Warning: '_color_palette_soft' must be of IMAGE_SOFT IMAGE_R8G8B8A8_SRGB type because 'Particles.draw' codes require that !!
{
   D._color_palette[index]=palette;
   if(palette)palette->copyTry(D._color_palette_soft[index], -1, -1, -1, IMAGE_R8G8B8A8_SRGB, IMAGE_SOFT, 1);
   else                        D._color_palette_soft[index].del();
}
Display& Display::colorPalette     (C ImagePtr &palette) {SetPalette(0, palette);    return T;}
Display& Display::colorPalette1    (C ImagePtr &palette) {SetPalette(1, palette);    return T;}
Display& Display::colorPaletteAllow(  Bool      on     ) {D._color_palette_allow=on; return T;}
/******************************************************************************/
#if WINDOWS_NEW
Int DipsToPixels(Flt dips) {return Round(dips*ScreenScale);}
Flt PixelsToDips(Int pix ) {return       pix /ScreenScale ;}
#endif
/******************************************************************************/
}
/******************************************************************************/
