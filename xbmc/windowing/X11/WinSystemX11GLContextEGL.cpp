#include "system.h"

#if defined(HAVE_X11) && defined(HAS_EGL)

#include "WinSystemX11GLContextEGL.h"
#include "utils/log.h"
#include "guilib/GraphicContext.h"
#include "guilib/DispResource.h"
#include "threads/SingleLock.h"
#include "Application.h"

CWinSystemX11GLContextEGL::CWinSystemX11GLContextEGL()
{
  m_pGLContext = NULL;
}

CWinSystemX11GLContextEGL::~CWinSystemX11GLContextEGL()
{
  delete m_pGLContext;
}

bool CWinSystemX11GLContextEGL::PresentRenderImpl(const CDirtyRegionList& dirty)
{
  m_pGLContext->SwapBuffers(dirty, m_iVSyncMode);
  if (m_delayDispReset && m_dispResetTimer.IsTimePast())
  {
    m_delayDispReset = false;
    CSingleLock lock(m_resourceSection);
    // tell any shared resources
    for (std::vector<IDispResource *>::iterator i = m_resources.begin(); i != m_resources.end(); ++i)
      (*i)->OnResetDisplay();
  }
}

void CWinSystemX11GLContextEGL::SetVSyncImpl(bool enable)
{
  m_pGLContext->SetVSync(enable, m_iVSyncMode);
}

bool CWinSystemX11GLContextEGL::IsExtSupported(const char* extension)
{
  if(strncmp(extension, m_pGLContext->ExtPrefix().c_str(), 4) != 0)
    return CRenderSystemGLES::IsExtSupported(extension);

  return m_pGLContext->IsExtSupported(extension);
}

bool CWinSystemX11GLContextEGL::SetWindow(int width, int height, bool fullscreen, const std::string &output, int *winstate)
{
  int newwin = 0;
  CWinSystemX11::SetWindow(width, height, fullscreen, output, &newwin);
  if (newwin)
  {
    CDirtyRegionList dr;
    RefreshGLContext(m_currentOutput.compare(output) != 0);
    XSync(m_dpy, FALSE);
    g_graphicsContext.Clear(0);
    g_graphicsContext.Flip(dr);
    ResetVSync();

    m_windowDirty = false;
    m_bIsInternalXrr = false;

    if (!m_delayDispReset)
    {
      CSingleLock lock(m_resourceSection);
      // tell any shared resources
      for (std::vector<IDispResource *>::iterator i = m_resources.begin(); i != m_resources.end(); ++i)
        (*i)->OnResetDisplay();
    }
  }
  return true;
}

bool CWinSystemX11GLContextEGL::CreateNewWindow(const std::string& name, bool fullScreen, RESOLUTION_INFO& res, PHANDLE_EVENT_FUNC userFunction)
{
  CLog::Log(LOGNOTICE, "CWinSystemX11GLContextEGL::CreateNewWindow");
  if(!CWinSystemX11::CreateNewWindow(name, fullScreen, res, userFunction))
    return false;

  m_pGLContext->QueryExtensions();
  return true;
}

bool CWinSystemX11GLContextEGL::ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop)
{
  m_newGlContext = false;
  CWinSystemX11::ResizeWindow(newWidth, newHeight, newLeft, newTop);
  CRenderSystemGLES::ResetRenderSystem(newWidth, newHeight, false, 0);

  if (m_newGlContext)
    g_application.ReloadSkin();

  return true;
}

bool CWinSystemX11GLContextEGL::SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays)
{
  m_newGlContext = false;
  CWinSystemX11::SetFullScreen(fullScreen, res, blankOtherDisplays);
  CRenderSystemGLES::ResetRenderSystem(res.iWidth, res.iHeight, fullScreen, res.fRefreshRate);

  if (m_newGlContext)
    g_application.ReloadSkin();

  return true;
}

bool CWinSystemX11GLContextEGL::DestroyWindowSystem()
{
  bool ret;
  m_pGLContext->Destroy();
  ret = CWinSystemX11::DestroyWindowSystem();
  return ret;
}

bool CWinSystemX11GLContextEGL::DestroyWindow()
{
  bool ret;
  m_pGLContext->Detach();
  ret = CWinSystemX11::DestroyWindow();
  return ret;
}

XVisualInfo* CWinSystemX11GLContextEGL::GetVisual()
{
  CLog::Log(LOGNOTICE, "CWinSystemX11GLContextEGL::GetVisual() m_pGLContext:%p GetVisual", m_pGLContext);
  if(!m_pGLContext)
  {
    CLog::Log(LOGNOTICE, "Create new CGLContextEGL at CWinSystemX11GLContextEGL::CreateNewWindow, m_dpy=%p", m_dpy);
    m_pGLContext = new CGLContextEGL(m_dpy);
  }
  return m_pGLContext->GetVisual();
}

bool CWinSystemX11GLContextEGL::RefreshGLContext(bool force)
{
  bool firstrun = false;
  if (!m_pGLContext)
  {
    m_pGLContext = new CGLContextEGL(m_dpy);
    firstrun = true;
  }
  bool ret = m_pGLContext->Refresh(force, m_nScreen, m_glWindow, m_newGlContext);

  if (ret && !firstrun)
    return ret;

  std::string gpuvendor;
  if (ret)
  {
    const char* vend = (const char*) glGetString(GL_VENDOR);
    if (vend)
      gpuvendor = vend;
  }
  std::transform(gpuvendor.begin(), gpuvendor.end(), gpuvendor.begin(), ::tolower);
  if (firstrun && (!ret || gpuvendor.compare(0, 5, "intel") != 0))
  {
    delete m_pGLContext;
    m_pGLContext = new CGLContextEGL(m_dpy);
    ret = m_pGLContext->Refresh(force, m_nScreen, m_glWindow, m_newGlContext);
  }
  return ret;
}

bool CWinSystemX11GLContextEGL::DestroyGLContext()
{
  m_pGLContext->Destroy();
}

#endif
