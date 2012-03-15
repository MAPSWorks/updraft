#ifndef UPDRAFT_SRC_CORE_PICKHANDLER_H_
#define UPDRAFT_SRC_CORE_PICKHANDLER_H_

#include <osgGA/GUIEventHandler>
#include <osgViewer/Viewer>
#include <QtGui>
#include "../mapobject.h"

namespace Updraft {
namespace Core {

class PickHandler: public osgGA::GUIEventHandler {
 public:
  PickHandler(): mX(0), mY(0) {}

  bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa);

 protected:
  // Store mouse xy location for button press & move events.
  float mX, mY;

  void raiseClickEvent(QVector<MapObject*> mapObjects);
  MapObject* getMapObject(osg::Node* node);

  // Get all MapObjectes associated with intersected nodes.
  QVector<MapObject*> getIntersectedMapObjects(
    const double x,
    const double y,
    osgViewer::Viewer* viewer);
};

}  // End namespace Core
}  // End namespace Updraft

#endif  // UPDRAFT_SRC_CORE_PICKHANDLER_H_
