#include <osgQt/GraphicsWindowQt>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgEarthUtil/Viewpoint>
#include <osgEarthUtil/ObjectPlacer>
#include <osgEarth/Map>
#include <osgEarth/MapNode>
#include <string>

#include "scenemanager.h"
#include "mapmanipulator.h"
#include "pickhandler.h"

#include "updraft.h"
#include "ui/mainwindow.h"
#include "ui/menu.h"
#include "../menuinterface.h"
#include "../settinginterface.h"

namespace Updraft {
namespace Core {

class GraphicsWindow;

/// Wrapper around osgQt::GLWidget to support on demand rendering.
class GraphicsWidget: public osgQt::GLWidget {
 public:
  GraphicsWidget(
    SceneManager* sceneManager,
    QWidget* parent = NULL,
    const QGLWidget* shareWidget = NULL,
    Qt::WindowFlags f = 0,
    bool forwardKeyEvents = false)
    : osgQt::GLWidget(parent, shareWidget, f, forwardKeyEvents),
    sceneManager(sceneManager) {}

  SceneManager* sceneManager;

 protected:
  /// Process event imediately after it is registered.
  bool event(QEvent* ev);
};

/// Wrapper around osgQt::GraphicsWindowQt to support on demand rendering.
class GraphicsWindow: public osgQt::GraphicsWindowQt {
 public:
  explicit GraphicsWindow(SceneManager* sceneManager, osgQt::GLWidget* widget)
    : osgQt::GraphicsWindowQt(widget), sceneManager(sceneManager) {}

  void requestRedraw() {
    qDebug() << "graphics window request redraw";
    sceneManager->requestRedraw();
  }

  void requestContinuousUpdate(bool needed) {
    qDebug() << "graphics window request continuous update " << needed;
    sceneManager->requestContinuousUpdate(needed);
  }

 private:
  SceneManager* sceneManager;
};

class Viewer: public osgViewer::Viewer {
 public:
  explicit Viewer(SceneManager* sceneManager)
  : sceneManager(sceneManager) {}

  void requestRedraw() {
    qDebug() << "viewer request redraw";
    sceneManager->requestRedraw();
  }

  void requestContinuousUpdate(bool needed) {
    qDebug() << "viewer request continuous update " << needed;
    sceneManager->requestContinuousUpdate(needed);
  }

 private:
  SceneManager* sceneManager;
};

bool GraphicsWidget::event(QEvent* ev) {
  qDebug() << "event detected " << ev->type();

  // A little hack to make map manipulator work correctly.
  // TODO(Kuba): This isn't how it should be done.
  switch (ev->type()) {
  case QEvent::MouseButtonDblClick:
  case QEvent::MouseMove:
  case QEvent::Wheel:
    sceneManager->requestRedraw();
    break;
  default:
    // do nothing
    break;
  }

  sceneManager->eventDetected = true;
  return osgQt::GLWidget::event(ev);
}

SceneManager::SceneManager()
  : requestedRedraw(false), requestedContinuousUpdate(false),
  eventDetected(false) {
  osg::DisplaySettings::instance()->setMinimumNumStencilBits(8);

  manipulator = new MapManipulator();

  viewer = new Viewer(this);
  viewer->setThreadingModel(osgViewer::ViewerBase::ThreadPerContext);

  GraphicsWidget *widget = new GraphicsWidget(this);
  graphicsWindow = new GraphicsWindow(this, widget);
  widget->setMouseTracking(false);

  // create camera
  camera = createCamera();
  camera->setGraphicsContext(graphicsWindow);
  camera->setClearStencil(128);
  camera->setClearMask
    (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  viewer->setCamera(camera);

  createMapManagers();

  // create and set scene
  sceneRoot = new osg::Group();

  // create and add background
  background = new osg::ClearNode();
  background->setClearColor(osg::Vec4(0.8f, 0.8f, 0.8f, 1.0f));
  sceneRoot->addChild(background);

  // add basic group for nodes
  simpleGroup = new osg::Group();
  sceneRoot->addChild(simpleGroup);

  // add active map
  mapNode = mapManagers[activeMapIndex]->getMapNode();
  sceneRoot->addChild(mapNode);

  viewer->setSceneData(sceneRoot);

  manipulator->setNode(mapNode);
  manipulator->setHomeViewpoint(getInitialPosition());

  viewer->setCameraManipulator(manipulator);

  // Create a picking handler
  viewer->addEventHandler(new PickHandler());
  viewer->addEventHandler(new osgViewer::StatsHandler);

  menuItems();
  mapLayerGroup();

  // start timer and conenct it appropriately
  timer = new QTimer(this);

  onDemandSetting = updraft->settingsManager->addSetting(
    "core:onDemand",
    tr("Render frames only when necessary"),
    QVariant(true),
    true);

  onDemandSetting->callOnValueChanged(this, SLOT(reconnectTimerSignal()));
  reconnectTimerSignal();  // We need to call this manually once.
  connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
  timer->start(20);
}

osgEarth::Util::Viewpoint SceneManager::getHomePosition() {
  return osgEarth::Util::Viewpoint(14.42046, 50.087811,
    0, 0.0, -90.0, 15e5);
}

osgEarth::Util::Viewpoint SceneManager::getInitialPosition() {
  osgEarth::Util::Viewpoint start = getHomePosition();
  start.setRange(start.getRange() * 1.3);
  return start;
}

void SceneManager::menuItems() {
  Menu* viewMenu = updraft->mainWindow->getSystemMenu(MENU_VIEW);

  QAction* resetNorthAction = new QAction(tr("Rotate to north"), this);
  connect(resetNorthAction, SIGNAL(triggered()), this, SLOT(resetNorth()));
  viewMenu->insertAction(200, resetNorthAction);

  QAction* untiltAction = new QAction(tr("Restore 2D View"), this);
  connect(untiltAction, SIGNAL(triggered()), this, SLOT(untilt()));
  viewMenu->insertAction(300, untiltAction);
}

void SceneManager::mapLayerGroup() {
  osgEarth::MapNode* map = getMapNode();
  osg::Group* group = newGroup();
  // TODO(Maria): Get name from the .earth file.
  // QString title = QString::fromStdString(map->getMap()->getName());
  MapLayerGroupInterface* mapLayerGroup =
    updraft->mainWindow->createMapLayerGroup(tr("Maps"), group, map);

  for (int i = 0; i < mapManagers.size(); i++) {
    MapManager *manager = mapManagers[i];

    MapLayerInterface* layer = mapLayerGroup->insertExistingMapLayer(
      manager->getMapNode(), manager->getName());

    layer->connectSignalChecked(
      this, SLOT(checkedMap(bool, MapLayerInterface*)));

    if (i == activeMapIndex) {
      layer->emitDisplayed(true);
    } else {
      layer->emitDisplayed(false);
    }

    layers.append(layer);
  }
}

SceneManager::~SceneManager() {
  // We should unregister all the registered osg nodes
  QList<osg::Node*> registeredNodes = pickingMap.keys();
  foreach(osg::Node* node, registeredNodes) {
    unregisterOsgNode(node);
  }

  foreach(MapManager *m, mapManagers) {
    delete m;
  }
}

QWidget* SceneManager::getWidget() {
  if (graphicsWindow != NULL) {
    return graphicsWindow->getGLWidget();
  } else {
    return NULL;
  }
}

void SceneManager::startInitialAnimation() {
  osgEarth::Util::Viewpoint home = getHomePosition();
  // set home position for ACTION_HOME
  manipulator->setHomeViewpoint(home, 1.0);
  // go to home position now
  manipulator->setViewpoint(home, 2.0);
}

void SceneManager::tick() {
  bool needRedraw = requestedRedraw || requestedContinuousUpdate ||
    viewer->getDatabasePager()->requiresUpdateSceneGraph() ||
    viewer->getDatabasePager()->getRequestsInProgress() ||
    viewer->getCamera()->getUpdateCallback() ||
    viewer->getSceneData()->getNumChildrenRequiringUpdateTraversal() > 0;

  if (eventDetected && !needRedraw) {
    qDebug() << "event processing";
    viewer->eventTraversal();
    needRedraw = requestedRedraw || requestedContinuousUpdate;
  }

  if (needRedraw) {
    qDebug() << "frame";
    redrawScene();
  }

  requestedRedraw = false;
  eventDetected = false;
}

void SceneManager::redrawScene() {
  // start initial animation in second frame to prevent jerks
  static int i = 0;
  if (i == 1) startInitialAnimation();
  if (i < 2) ++i;

  bool isTilted = (manipulator->getViewpoint().getPitch() > -88);
  bool isFar = (manipulator->getDistance() > 7e6);
  if (isTilted || isFar) {
    updateCameraPerspective(camera);
  } else {
    updateCameraOrtho(camera);
  }
  viewer->frame();
}

osg::Group* SceneManager::newGroup() {
  osg::Group* group = new osg::Group();
  sceneRoot->addChild(group);
  return group;
}

bool SceneManager::removeGroup(osg::Group* group) {
  return sceneRoot->removeChild(group);
}

osgEarth::MapNode* SceneManager::getMapNode() {
  return mapNode;
}

osg::Group* SceneManager::getSimpleGroup() {
  return simpleGroup;
}

void SceneManager::registerOsgNode(osg::Node* node, MapObject* mapObject) {
  pickingMap.insert(node, mapObject);
}

void SceneManager::unregisterOsgNode(osg::Node* node) {
  pickingMap.remove(node);
}

MapObject* SceneManager::getNodeMapObject(osg::Node* node) {
  QHash<osg::Node*, MapObject*>::iterator it = pickingMap.find(node);
  if (it != pickingMap.end()) {
    return it.value();
  } else {
    return NULL;
  }
}

void SceneManager::checkedMap(bool value, MapLayerInterface* object) {
  // find the map layer:
  int index = 0;
  for (index = 0; index < layers.size(); index++) {
    if (layers[index] == object) break;
  }
  if (index == activeMapIndex) {
      // cannot uncheck the active map
    if (value == false) {
      // force the chcekbox to be visible
      layers[index]->emitDisplayed(true);
    }
  } else {
    // checked non active map
    // replace the map in the scene:
    if (value == true) {
      int oldIndex = activeMapIndex;
      activeMapIndex = index;
      sceneRoot->removeChild(mapManagers[oldIndex]->getMapNode());
      sceneRoot->addChild(mapManagers[activeMapIndex]->getMapNode());
      layers[oldIndex]->emitDisplayed(false);
      layers[activeMapIndex]->emitDisplayed(true);
    }
  }
}

MapManager* SceneManager::getMapManager() {
  return mapManagers[activeMapIndex];
}

osg::Camera* SceneManager::createCamera() {
  const osg::GraphicsContext::Traits* traits = graphicsWindow->getTraits();
  osg::Camera* camera = new osg::Camera();

  camera->setClearColor(osg::Vec4(0.2, 0.2, 0.6, 1.0));
  camera->setViewport(new osg::Viewport(0, 0, traits->width, traits->height));
  isCameraPerspective = FALSE;
  updateCameraPerspective(camera);
  return camera;
}

double SceneManager::getAspectRatio() {
  const osg::GraphicsContext::Traits *traits = graphicsWindow->getTraits();
  double aspectRatio = static_cast<double>(traits->width)/
    static_cast<double>(traits->height);
  return aspectRatio;
}

void SceneManager::updateCameraOrtho(osg::Camera* camera) {
  static double lastDistance = 0;
  double distance = manipulator->getDistance();
  bool hasDistanceChanged = (distance != lastDistance);
  // no need to update projection matrix
  if (!hasDistanceChanged && !isCameraPerspective) {
    return;
  }
  lastDistance = distance;

  double hy = distance * 0.2679;  // tan(fovy/2)
  double hx = hy * getAspectRatio();
  camera->setProjectionMatrixAsOrtho2D(-hx, hx, -hy, hy);
  isCameraPerspective = FALSE;
}

void SceneManager::updateCameraPerspective(osg::Camera* camera) {
  // no need to update projection matrix
  if (isCameraPerspective) {
    return;
  }
  camera->setProjectionMatrixAsPerspective(
    30.0f, getAspectRatio(),
    1.0f, 10000.0f);
  isCameraPerspective = TRUE;
}

void SceneManager::resetNorth() {
  manipulator->resetNorth(1);
}

void SceneManager::untilt() {
  manipulator->untilt(1);
}

void SceneManager::requestRedraw() {
  requestedRedraw = true;
}

void SceneManager::requestContinuousUpdate(bool needed) {
  requestedContinuousUpdate = needed;
}

void SceneManager::reconnectTimerSignal() {
  disconnect(timer, SIGNAL(timeout()), this, SLOT(tick()));
  disconnect(timer, SIGNAL(timeout()), this, SLOT(redrawScene()));

  if (onDemandSetting->get().toBool()) {
    connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
    qDebug() << "On demand rendering is ON";
  } else {
    connect(timer, SIGNAL(timeout()), this, SLOT(redrawScene()));
    qDebug() << "On demand rendering is OFF";
  }
}

void SceneManager::createMapManagers() {
  mapManagers.append(
    new MapManager(updraft->getDataDirectory() + "/initial1.earth"));
  mapManagers.append(
    new MapManager(updraft->getDataDirectory() + "/initial2.earth"));

  activeMapIndex = 0;
}

}  // end namespace Core
}  // end namespace Updraft
