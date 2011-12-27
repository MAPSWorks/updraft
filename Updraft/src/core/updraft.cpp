#include "updraft.h"

namespace Updraft {
namespace Core {

Updraft::Updraft(int argc, char** argv)
  : QApplication(argc, argv) {
  QTranslator trans;
  trans.load("translations/czech");

  installTranslator(&trans);

  mainWindow = new MainWindow(NULL);
  fileTypeManager = new FileTypeManager();
  pluginManager = new PluginManager();
  sceneManager = new SceneManager("data/initial.earth");

  mainWindow->setMapWidget(sceneManager->getWidget());
}

Updraft::~Updraft() {
  delete sceneManager;
  delete pluginManager;
  delete fileTypeManager;
  delete mainWindow;
}

/// Pull the lever.
/// Shows main window, and enters event loop.
int Updraft::exec() {
  mainWindow->show();
  return QApplication::exec();
}

}  // End namespace Core
}  // End namespace Updraft
