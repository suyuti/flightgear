// fg_os_osgviewer.cxx -- common functions for fg_os interface
// implemented as an osgViewer
//
// Copyright (C) 2007  Tim Moore timoore@redhat.com
// Copyright (C) 2007 Mathias Froehlich 
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

#include <stdlib.h>

#include <boost/foreach.hpp>

#include <simgear/compiler.h>
#include <simgear/structure/exception.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/props/props_io.hxx>

#include <osg/Camera>
#include <osg/GraphicsContext>
#include <osg/Group>
#include <osg/Matrixd>
#include <osg/Viewport>
#include <osg/Version>
#include <osg/Notify>
#include <osg/View>
#include <osgViewer/ViewerEventHandlers>
#include <osgViewer/Viewer>
#include <osgViewer/GraphicsWindow>

#include <Scenery/scenery.hxx>
#include <Main/fg_os.hxx>
#include <Main/fg_props.hxx>
#include <Main/util.hxx>
#include <Main/globals.hxx>
#include "renderer.hxx"
#include "CameraGroup.hxx"
#include "FGEventHandler.hxx"
#include "WindowBuilder.hxx"
#include "WindowSystemAdapter.hxx"

// Static linking of OSG needs special macros
#ifdef OSG_LIBRARY_STATIC
#include <osgDB/Registry>
USE_GRAPHICSWINDOW();
// Image formats
USE_OSGPLUGIN(bmp);
USE_OSGPLUGIN(dds);
USE_OSGPLUGIN(hdr);
USE_OSGPLUGIN(pic);
USE_OSGPLUGIN(pnm);
USE_OSGPLUGIN(rgb);
USE_OSGPLUGIN(tga);
#ifdef OSG_JPEG_ENABLED
  USE_OSGPLUGIN(jpeg);
#endif
#ifdef OSG_PNG_ENABLED
  USE_OSGPLUGIN(png);
#endif
#ifdef OSG_TIFF_ENABLED
  USE_OSGPLUGIN(tiff);
#endif
// Model formats
USE_OSGPLUGIN(3ds);
USE_OSGPLUGIN(ac);
USE_OSGPLUGIN(ive);
USE_OSGPLUGIN(osg);
USE_OSGPLUGIN(txf);
#endif

// fg_os implementation using OpenSceneGraph's osgViewer::Viewer class
// to create the graphics window and run the event/update/render loop.

//
// fg_os implementation
//

using namespace std;    
using namespace flightgear;
using namespace osg;

static osg::ref_ptr<osgViewer::Viewer> viewer;
static osg::ref_ptr<osg::Camera> mainCamera;

static void setStereoMode( const char * mode )
{
    DisplaySettings::StereoMode stereoMode = DisplaySettings::QUAD_BUFFER;
    bool stereoOn = true;

    if (strcmp(mode,"QUAD_BUFFER")==0)
    {
        stereoMode = DisplaySettings::QUAD_BUFFER;
    }
    else if (strcmp(mode,"ANAGLYPHIC")==0)
    {
        stereoMode = DisplaySettings::ANAGLYPHIC;
    }
    else if (strcmp(mode,"HORIZONTAL_SPLIT")==0)
    {
        stereoMode = DisplaySettings::HORIZONTAL_SPLIT;
    }
    else if (strcmp(mode,"VERTICAL_SPLIT")==0)
    {
        stereoMode = DisplaySettings::VERTICAL_SPLIT;
    }
    else if (strcmp(mode,"LEFT_EYE")==0)
    {
        stereoMode = DisplaySettings::LEFT_EYE;
    }
    else if (strcmp(mode,"RIGHT_EYE")==0)
    {
        stereoMode = DisplaySettings::RIGHT_EYE;
    }
    else if (strcmp(mode,"HORIZONTAL_INTERLACE")==0)
    {
        stereoMode = DisplaySettings::HORIZONTAL_INTERLACE;
    }
    else if (strcmp(mode,"VERTICAL_INTERLACE")==0)
    {
        stereoMode = DisplaySettings::VERTICAL_INTERLACE;
    }
    else if (strcmp(mode,"CHECKERBOARD")==0)
    {
        stereoMode = DisplaySettings::CHECKERBOARD;
    } else {
        stereoOn = false; 
    }
    DisplaySettings::instance()->setStereo( stereoOn );
    DisplaySettings::instance()->setStereoMode( stereoMode );
}

static const char * getStereoMode()
{
    DisplaySettings::StereoMode stereoMode = DisplaySettings::instance()->getStereoMode();
    bool stereoOn = DisplaySettings::instance()->getStereo();
    if( !stereoOn ) return "OFF";
    if( stereoMode == DisplaySettings::QUAD_BUFFER ) {
        return "QUAD_BUFFER";
    } else if( stereoMode == DisplaySettings::ANAGLYPHIC ) {
        return "ANAGLYPHIC";
    } else if( stereoMode == DisplaySettings::HORIZONTAL_SPLIT ) {
        return "HORIZONTAL_SPLIT";
    } else if( stereoMode == DisplaySettings::VERTICAL_SPLIT ) {
        return "VERTICAL_SPLIT";
    } else if( stereoMode == DisplaySettings::LEFT_EYE ) {
        return "LEFT_EYE";
    } else if( stereoMode == DisplaySettings::RIGHT_EYE ) {
        return "RIGHT_EYE";
    } else if( stereoMode == DisplaySettings::HORIZONTAL_INTERLACE ) {
        return "HORIZONTAL_INTERLACE";
    } else if( stereoMode == DisplaySettings::VERTICAL_INTERLACE ) {
        return "VERTICAL_INTERLACE";
    } else if( stereoMode == DisplaySettings::CHECKERBOARD ) {
        return "CHECKERBOARD";
    } 
    return "OFF";
}

/**
 * merge OSG output into our logging system, so it gets recorded to file,
 * and so we can display a GUI console with renderer issues, especially
 * shader compilation warnings and errors.
 */
class NotifyLogger : public osg::NotifyHandler
{
public:
  // note this callback will be invoked by OSG from multiple threads.
  // fortunately our Simgear logging implementation already handles
  // that internally, so we simply pass the message on.
  virtual void notify(osg::NotifySeverity severity, const char *message)
  {
    SG_LOG(SG_GL, translateSeverity(severity), message);
  }
  
private:
  sgDebugPriority translateSeverity(osg::NotifySeverity severity)
  {
    switch (severity) {
      case osg::ALWAYS:
      case osg::FATAL:  return SG_ALERT;
      case osg::WARN:   return SG_WARN;
      case osg::NOTICE:
      case osg::INFO:   return SG_INFO;
      case osg::DEBUG_FP:
      case osg::DEBUG_INFO: return SG_DEBUG;
    }
  }
};

void fgOSOpenWindow(bool stencil)
{
    osg::setNotifyHandler(new NotifyLogger);
    //osg::setNotifyLevel(osg::DEBUG_INFO);
  
    viewer = new osgViewer::Viewer;
    viewer->setDatabasePager(FGScenery::getPagerSingleton());
    CameraGroup* cameraGroup = 0;
    std::string mode;
    mode = fgGetString("/sim/rendering/multithreading-mode", "SingleThreaded");
    if (mode == "AutomaticSelection")
      viewer->setThreadingModel(osgViewer::Viewer::AutomaticSelection);
    else if (mode == "CullDrawThreadPerContext")
      viewer->setThreadingModel(osgViewer::Viewer::CullDrawThreadPerContext);
    else if (mode == "DrawThreadPerContext")
      viewer->setThreadingModel(osgViewer::Viewer::DrawThreadPerContext);
    else if (mode == "CullThreadPerCameraDrawThreadPerContext")
      viewer->setThreadingModel(osgViewer::Viewer::CullThreadPerCameraDrawThreadPerContext);
    else
      viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);
    WindowBuilder::initWindowBuilder(stencil);
    WindowBuilder *windowBuilder = WindowBuilder::getWindowBuilder();

    // Look for windows, camera groups, and the old syntax of
    // top-level cameras
    SGPropertyNode* renderingNode = fgGetNode("/sim/rendering");
    SGPropertyNode* cgroupNode = renderingNode->getNode("camera-group", true);
    bool oldSyntax = !cgroupNode->hasChild("camera");
    if (oldSyntax) {
        for (int i = 0; i < renderingNode->nChildren(); ++i) {
            SGPropertyNode* propNode = renderingNode->getChild(i);
            const char* propName = propNode->getName();
            if (!strcmp(propName, "window") || !strcmp(propName, "camera")) {
                SGPropertyNode* copiedNode
                    = cgroupNode->getNode(propName, propNode->getIndex(), true);
                copyProperties(propNode, copiedNode);
            }
        }
        vector<SGPropertyNode_ptr> cameras = cgroupNode->getChildren("camera");
        SGPropertyNode* masterCamera = 0;
        BOOST_FOREACH(SGPropertyNode_ptr& camera, cameras) {
            if (camera->getDoubleValue("shear-x", 0.0) == 0.0
                && camera->getDoubleValue("shear-y", 0.0) == 0.0) {
                masterCamera = camera.ptr();
                break;
            }
        }
        if (!masterCamera) {
            masterCamera = cgroupNode->getChild("camera", cameras.size(), true);
            setValue(masterCamera->getNode("window/name", true),
                     windowBuilder->getDefaultWindowName());
        }
        SGPropertyNode* nameNode = masterCamera->getNode("window/name");
        if (nameNode)
            setValue(cgroupNode->getNode("gui/window/name", true),
                     nameNode->getStringValue());
    }
    cameraGroup = CameraGroup::buildCameraGroup(viewer.get(), cgroupNode);
    Camera* guiCamera = getGUICamera(cameraGroup);
    if (guiCamera) {
        Viewport* guiViewport = guiCamera->getViewport();
        fgSetInt("/sim/startup/xsize", guiViewport->width());
        fgSetInt("/sim/startup/ysize", guiViewport->height());
    }
    FGEventHandler* manipulator = globals->get_renderer()->getEventHandler();
    WindowSystemAdapter* wsa = WindowSystemAdapter::getWSA();
    if (wsa->windows.size() != 1) {
        manipulator->setResizable(false);
    }
    viewer->getCamera()->setProjectionResizePolicy(osg::Camera::FIXED);
    viewer->addEventHandler(manipulator);
    // Let FG handle the escape key with a confirmation
    viewer->setKeyEventSetsDone(0);
    // The viewer won't start without some root.
    viewer->setSceneData(new osg::Group);
    globals->get_renderer()->setViewer(viewer.get());
    CameraGroup::setDefault(cameraGroup);

    DisplaySettings * displaySettings = DisplaySettings::instance();
    fgTie("/sim/rendering/osg-displaysettings/eye-separation", displaySettings, &DisplaySettings::getEyeSeparation, &DisplaySettings::setEyeSeparation );
    fgTie("/sim/rendering/osg-displaysettings/screen-distance", displaySettings, &DisplaySettings::getScreenDistance, &DisplaySettings::setScreenDistance );
    fgTie("/sim/rendering/osg-displaysettings/screen-width", displaySettings, &DisplaySettings::getScreenWidth, &DisplaySettings::setScreenWidth );
    fgTie("/sim/rendering/osg-displaysettings/screen-height", displaySettings, &DisplaySettings::getScreenHeight, &DisplaySettings::setScreenHeight );
    fgTie("/sim/rendering/osg-displaysettings/stereo-mode", getStereoMode, setStereoMode );
    fgTie("/sim/rendering/osg-displaysettings/double-buffer", displaySettings, &DisplaySettings::getDoubleBuffer, &DisplaySettings::setDoubleBuffer );
    fgTie("/sim/rendering/osg-displaysettings/depth-buffer", displaySettings, &DisplaySettings::getDepthBuffer, &DisplaySettings::setDepthBuffer );
    fgTie("/sim/rendering/osg-displaysettings/rgb", displaySettings, &DisplaySettings::getRGB, &DisplaySettings::setRGB );
}


static int status = 0;

void fgOSExit(int code)
{
    viewer->setDone(true);
    viewer->getDatabasePager()->cancel();
    status = code;
}

int fgOSMainLoop()
{
    ref_ptr<FGEventHandler> manipulator
        = globals->get_renderer()->getEventHandler();
    viewer->setReleaseContextAtEndOfFrameHint(false);
    if (!viewer->isRealized())
        viewer->realize();
    while (!viewer->done()) {
        fgIdleHandler idleFunc = manipulator->getIdleHandler();
        if (idleFunc)
            (*idleFunc)();
        globals->get_renderer()->update();
        viewer->frame();
    }
    
    return status;
}

int fgGetKeyModifiers()
{
    if (!globals->get_renderer()) { // happens during shutdown
      return 0;
    }
    
    return globals->get_renderer()->getEventHandler()->getCurrentModifiers();
}

void fgWarpMouse(int x, int y)
{
    warpGUIPointer(CameraGroup::getDefault(), x, y);
}

void fgOSInit(int* argc, char** argv)
{
    globals->get_renderer()->init();
    WindowSystemAdapter::setWSA(new WindowSystemAdapter);
}

void fgOSFullScreen()
{
    std::vector<osgViewer::GraphicsWindow*> windows;
    viewer->getWindows(windows);

    if (windows.size() == 0)
        return; // Huh?!?

    /* Toggling window fullscreen is only supported for the main GUI window.
     * The other windows should use fixed setup from the camera.xml file anyway. */
    osgViewer::GraphicsWindow* window = windows[0];

    {
        osg::GraphicsContext::WindowingSystemInterface    *wsi = osg::GraphicsContext::getWindowingSystemInterface();

        if (wsi == NULL)
        {
            SG_LOG(SG_VIEW, SG_ALERT, "ERROR: No WindowSystemInterface available. Cannot toggle window fullscreen.");
            return;
        }

        static int previous_x = 0;
        static int previous_y = 0;
        static int previous_width = 800;
        static int previous_height = 600;

        unsigned int screenWidth;
        unsigned int screenHeight;
        wsi->getScreenResolution(*(window->getTraits()), screenWidth, screenHeight);

        int x;
        int y;
        int width;
        int height;
        window->getWindowRectangle(x, y, width, height);

        /* Note: the simple "is window size == screen size" check to detect full screen state doesn't work with
         * X screen servers in Xinerama mode, since the reported screen width (or height) exceeds the maximum width
         * (or height) usable by a single window (Xserver automatically shrinks/moves the full screen window to fit a
         * single display) - so we detect full screen mode using "WindowDecoration" state instead.
         * "false" - even when a single window is display in fullscreen */
        //bool isFullScreen = x == 0 && y == 0 && width == (int)screenWidth && height == (int)screenHeight;
        bool isFullScreen = !window->getWindowDecoration();

        SG_LOG(SG_VIEW, SG_DEBUG, "Toggling fullscreen. Previous window rectangle ("
               << x << ", " << y << ") x (" << width << ", " << height << "), fullscreen: " << isFullScreen
               << ", number of screens: " << wsi->getNumScreens());
        if (isFullScreen)
        {
            // limit x,y coordinates and window size to screen area
            if (previous_x + previous_width > (int)screenWidth)
                previous_x = 0;
            if (previous_y + previous_height > (int)screenHeight)
                previous_y = 0;

            // disable fullscreen mode, restore previous window size/coordinates
            x = previous_x;
            y = previous_y;
            width = previous_width;
            height = previous_height;
        }
        else
        {
            // remember previous setting
            previous_x = x;
            previous_y = y;
            previous_width = width;
            previous_height = height;

            // enable fullscreen mode, set new width/height
            x = 0;
            y = 0;
            width = screenWidth;
            height = screenHeight;
        }

        // set xsize/ysize properties to adapt GUI planes
        fgSetInt("/sim/startup/xsize", width);
        fgSetInt("/sim/startup/ysize", height);

        // reconfigure window
        window->setWindowDecoration(isFullScreen);
        window->setWindowRectangle(x, y, width, height);
        window->grabFocusIfPointerInWindow();
    }
}

static void setMouseCursor(osgViewer::GraphicsWindow* gw, int cursor)
{
    if (!gw) {
        return;
    }
  
    osgViewer::GraphicsWindow::MouseCursor mouseCursor;
    mouseCursor = osgViewer::GraphicsWindow::InheritCursor;
    if (cursor == MOUSE_CURSOR_NONE)
        mouseCursor = osgViewer::GraphicsWindow::NoCursor;
    else if(cursor == MOUSE_CURSOR_POINTER)
#ifdef SG_MAC
        // osgViewer-Cocoa lacks RightArrowCursor, use Left
        mouseCursor = osgViewer::GraphicsWindow::LeftArrowCursor;
#else
        mouseCursor = osgViewer::GraphicsWindow::RightArrowCursor;
#endif
    else if(cursor == MOUSE_CURSOR_WAIT)
        mouseCursor = osgViewer::GraphicsWindow::WaitCursor;
    else if(cursor == MOUSE_CURSOR_CROSSHAIR)
        mouseCursor = osgViewer::GraphicsWindow::CrosshairCursor;
    else if(cursor == MOUSE_CURSOR_LEFTRIGHT)
        mouseCursor = osgViewer::GraphicsWindow::LeftRightCursor;
    else if(cursor == MOUSE_CURSOR_TOPSIDE)
        mouseCursor = osgViewer::GraphicsWindow::TopSideCursor;
    else if(cursor == MOUSE_CURSOR_BOTTOMSIDE)
        mouseCursor = osgViewer::GraphicsWindow::BottomSideCursor;
    else if(cursor == MOUSE_CURSOR_LEFTSIDE)
        mouseCursor = osgViewer::GraphicsWindow::LeftSideCursor;
    else if(cursor == MOUSE_CURSOR_RIGHTSIDE)
        mouseCursor = osgViewer::GraphicsWindow::RightSideCursor;
    else if(cursor == MOUSE_CURSOR_TOPLEFT)
        mouseCursor = osgViewer::GraphicsWindow::TopLeftCorner;
    else if(cursor == MOUSE_CURSOR_TOPRIGHT)
        mouseCursor = osgViewer::GraphicsWindow::TopRightCorner;
    else if(cursor == MOUSE_CURSOR_BOTTOMLEFT)
        mouseCursor = osgViewer::GraphicsWindow::BottomLeftCorner;
    else if(cursor == MOUSE_CURSOR_BOTTOMRIGHT)
        mouseCursor = osgViewer::GraphicsWindow::BottomRightCorner;

    gw->setCursor(mouseCursor);
}

static int _cursor = -1;

void fgSetMouseCursor(int cursor)
{
    _cursor = cursor;
  
    std::vector<osgViewer::GraphicsWindow*> windows;
    viewer->getWindows(windows);
    BOOST_FOREACH(osgViewer::GraphicsWindow* gw, windows) {
        setMouseCursor(gw, cursor);
    }
}

int fgGetMouseCursor()
{
    return _cursor;
}
