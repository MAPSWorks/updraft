#include "mapmanager.h"
#include <osgDB/ReadFile>
#include <osgEarthUtil/ElevationManager>
#include <osgEarthDrivers/arcgis/ArcGISOptions>
#include <osgEarthDrivers/tms/TMSOptions>
#include <QDebug>
#include <string>
#include "maps/updraftarcgistilesource.h"

namespace Updraft {
namespace Core {

MapManager::MapManager() {
  this->mapNode = NULL;
  this->map = NULL;
  this->manipulator = NULL;
}

MapManager::MapManager(QString earthFile) {
  osg::Node* loadedMap = osgDB::readNodeFile(earthFile.toStdString());
  if (loadedMap != NULL) {
    this->mapNode = osgEarth::MapNode::findMapNode(loadedMap);
    this->map = mapNode->getMap();

    // add image layer wih our own driver
    /*
    osgEarth::Drivers::ArcGISOptions opt;
    opt.url() =
      "http://server.arcgisonline.com/ArcGIS/rest/"
      "services/ESRI_Imagery_World_2D/MapServer";
    UpdraftArcGisTileSource* source =
      new UpdraftArcGisTileSource(opt);

    osgEarth::ImageLayerOptions* imOpt =
      new osgEarth::ImageLayerOptions("Satellite map", opt);

    osgEarth::ImageLayer* onlineMaps =
      new osgEarth::ImageLayer(*imOpt, source);

    this->map->insertImageLayer(onlineMaps, 1);
    */
  } else {
    this->map = new osgEarth::Map();
    this->mapNode = new osgEarth::MapNode(this->map);
  }
    // initialize the manipulator
  this->manipulator = new MapManipulator();
  manipulator->setNode(mapNode);
}

QVector<osgEarth::ImageLayer*> MapManager::getImageLayers() {
  QVector<osgEarth::ImageLayer*> imageLayers;
  osgEarth::ImageLayerVector outImageLayers;
  map->getImageLayers(outImageLayers);
  for (uint i = 0; i < outImageLayers.size(); i++) {
    imageLayers.append(outImageLayers[i]);
  }
  return imageLayers;
}

QVector<osgEarth::ElevationLayer*> MapManager::getElevationLayers() {
  QVector<osgEarth::ElevationLayer*> elevationLayers;
  osgEarth::ElevationLayerVector outElevationLayers;
  map->getElevationLayers(outElevationLayers);
  for (uint i = 0; i < outElevationLayers.size(); i++) {
    elevationLayers.append(outElevationLayers[i]);
  }
  return elevationLayers;
}

QVector<osgEarth::ModelLayer*> MapManager::getModelLayers() {
  QVector<osgEarth::ModelLayer*> modelLayers;
  osgEarth::ModelLayerVector outModelLayers;
  map->getModelLayers(outModelLayers);
  for (uint i = 0; i < outModelLayers.size(); i++) {
    modelLayers.append(outModelLayers[i]);
  }
  return modelLayers;
}

QVector<MapLayerInterface*> MapManager::getMapLayers() {
  return mapLayers;
}

osgEarth::MapNode* MapManager::getMapNode() {
  return mapNode;
}

osgEarth::Map* MapManager::getMap() {
  return map;
}

MapManipulator* MapManager::getManipulator() {
  return manipulator;
}

QString MapManager::getName() {
  return QString::fromStdString(map->getName());
}

}  // End namespace Core
}  // End namespace Updraft
