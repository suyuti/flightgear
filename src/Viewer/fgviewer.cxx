#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <iostream>
#include <cstdlib>

#include <osg/ArgumentParser>
#include <osg/Fog>
#include <osgDB/ReadFile>
#include <osgDB/Registry>
#include <osgDB/WriteFile>
#include <osgViewer/Renderer>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/TerrainManipulator>
#include <osgGA/StateSetManipulator>

#include <simgear/props/props.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/scene/material/EffectCullVisitor.hxx>
#include <simgear/scene/material/matlib.hxx>
#include <simgear/scene/util/SGReaderWriterOptions.hxx>
#include <simgear/scene/tgdb/userdata.hxx>
#include <simgear/scene/model/ModelRegistry.hxx>
#include <simgear/scene/model/modellib.hxx>

#include <Scenery/scenery.hxx>

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Main/options.hxx>
#include <Main/fg_init.hxx>

class GraphDumpHandler : public  osgGA::GUIEventHandler
{
public:
    GraphDumpHandler() : _keyDump('d') {}
    void setKeyDump(int key) { _keyDump = key; }
    int getKeyDump() const { return _keyDump; }
    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa);

    /** Get the keyboard and mouse usage of this manipulator.*/
    virtual void getUsage(osg::ApplicationUsage& usage) const;
protected:
    int _keyDump;
};

static void dumpOut(osg::Node* node)
{
    char filename[24];
    static int count = 1;

    while (count < 1000) {
        FILE *fp;
        snprintf(filename, 24, "fgviewer-%03d.osg", count++);
        if ( (fp = fopen(filename, "r")) == NULL )
            break;
        fclose(fp);
    }

    if (osgDB::writeNodeFile(*node, filename))
        std::cerr << "Entire scene graph saved to \"" << filename << "\".\n";
    else
        std::cerr << "Failed to save to \"" << filename << "\".\n";
}

bool GraphDumpHandler::handle(const osgGA::GUIEventAdapter& ea,
                              osgGA::GUIActionAdapter& aa)
{
    osgViewer::View* view = dynamic_cast<osgViewer::View*>(&aa);
    if (!view)
        return false;
    if (ea.getHandled())
        return false;
    switch(ea.getEventType()) {
    case osgGA::GUIEventAdapter::KEYUP:
        if (ea.getKey() == _keyDump) {
            dumpOut(view->getScene()->getSceneData());
            return true;
        }
        break;
    default:
        return false;
    }
    return false;
}

void GraphDumpHandler::getUsage(osg::ApplicationUsage& usage) const
{
    std::ostringstream ostr;
    ostr << char(_keyDump);
            usage.addKeyboardMouseBinding(ostr.str(),
                                          "Dump scene graph to file");
}

int
fgviewerMain(int argc, char** argv)
{

    sgUserDataInit(0);

    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc, argv);

    // construct the viewer.
    osgViewer::Viewer viewer(arguments);
    osg::Camera* camera = viewer.getCamera();
    osgViewer::Renderer* renderer
        = static_cast<osgViewer::Renderer*>(camera->getRenderer());
    for (int i = 0; i < 2; ++i) {
        osgUtil::SceneView* sceneView = renderer->getSceneView(i);
        sceneView->setCullVisitor(new simgear::EffectCullVisitor);
    }
    // Shaders expect valid fog
    osg::StateSet* cameraSS = camera->getOrCreateStateSet();
    osg::Fog* fog = new osg::Fog;
    fog->setMode(osg::Fog::EXP2);
    fog->setColor(osg::Vec4(1.0, 1.0, 1.0, 1.0));
    fog->setDensity(.0000001);
    cameraSS->setAttributeAndModes(fog);
    // ... for some reason, get rid of that FIXME!
    viewer.setThreadingModel(osgViewer::Viewer::SingleThreaded);

    // set up the camera manipulators.
    osgGA::KeySwitchMatrixManipulator* keyswitchManipulator;
    keyswitchManipulator = new osgGA::KeySwitchMatrixManipulator;

    keyswitchManipulator->addMatrixManipulator('1', "Trackball",
                                               new osgGA::TrackballManipulator);
    keyswitchManipulator->addMatrixManipulator('2', "Flight",
                                               new osgGA::FlightManipulator);
    keyswitchManipulator->addMatrixManipulator('3', "Drive",
                                               new osgGA::DriveManipulator);
    keyswitchManipulator->addMatrixManipulator('4', "Terrain",
                                               new osgGA::TerrainManipulator);
    viewer.setCameraManipulator(keyswitchManipulator);

    // Usefull stats
    viewer.addEventHandler(new osgViewer::HelpHandler);
    viewer.addEventHandler(new osgViewer::StatsHandler);
    viewer.addEventHandler( new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()) );
    // Same FIXME ...
    // viewer.addEventHandler(new osgViewer::ThreadingHandler);
    viewer.addEventHandler(new osgViewer::LODScaleHandler);
    viewer.addEventHandler(new osgViewer::ScreenCaptureHandler);

    viewer.addEventHandler(new GraphDumpHandler);

    // Extract files to load from arguments now; this way fgInitConfig
    // won't choke on them.
    string_list dataFiles;
    for (int i = arguments.argc() - 1; i >= 0; --i) {
        if (arguments.isOption(i)) {
            break;
        } else {
            dataFiles.insert(dataFiles.begin(), arguments[i]);
            arguments.remove(i);
        }
    }

    // A subset of full flightgear initialization.
    // Allocate global data structures.  This needs to happen before
    // we parse command line options

    globals = new FGGlobals;

    if ( !fgInitConfig(arguments.argc(), arguments.argv()) ) {
        SG_LOG( SG_GENERAL, SG_ALERT, "Config option parsing failed ..." );
        exit(-1);
    }

    osgDB::FilePathList filePathList
        = osgDB::Registry::instance()->getDataFilePathList();
    filePathList.push_back(globals->get_fg_root());

    string_list path_list = globals->get_fg_scenery();
    for (unsigned i = 0; i < path_list.size(); ++i) {
        filePathList.push_back(path_list[i]);
    }

    globals->set_matlib( new SGMaterialLib );
    simgear::SGModelLib::init(globals->get_fg_root(), globals->get_props());

    // Initialize the material property subsystem.

    SGPath mpath( globals->get_fg_root() );
    mpath.append( fgGetString("/sim/rendering/materials-file") );
    if ( ! globals->get_matlib()->load(globals->get_fg_root(), mpath.str(),
            globals->get_props()) ) {
        SG_LOG( SG_GENERAL, SG_ALERT,
                "Error loading materials file " << mpath.str() );
        exit(-1);
    }

    globals->set_scenery( new FGScenery );
    globals->get_scenery()->init();
    globals->get_scenery()->bind();

    // The file path list must be set in the registry.
    osgDB::Registry::instance()->getDataFilePathList() = filePathList;

    simgear::SGReaderWriterOptions* options = new simgear::SGReaderWriterOptions;
    options->getDatabasePathList() = filePathList;
    options->setMaterialLib(globals->get_matlib());
    options->setPropertyNode(globals->get_props());
    options->setPluginStringData("SimGear::PREVIEW", "ON");
    
    // read the scene from the list of file specified command line args.
    osg::ref_ptr<osg::Node> loadedModel;
    loadedModel = osgDB::readNodeFiles(dataFiles, options);

    // if no model has been successfully loaded report failure.
    if (!loadedModel.valid()) {
        std::cerr << arguments.getApplicationName()
                  << ": No data loaded" << std::endl;
        return EXIT_FAILURE;
    }

    // pass the loaded scene graph to the viewer.
    viewer.setSceneData(loadedModel.get());

    return viewer.run();
}
