/* ============================================================================
 * Copyright (c) 2012 Michael A. Jackson (BlueQuartz Software)
 * Copyright (c) 2012 Dr. Michael A. Groeber (US Air Force Research Laboratories)
 * Copyright (c) 2012 Joseph B. Kleingers (Student Research Assistant)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * Neither the name of Michael A. Groeber, Michael A. Jackson, Joseph B. Kleingers,
 * the US Air Force, BlueQuartz Software nor the names of its contributors may be
 * used to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  This code was written under United States Air Force Contract number
 *                           FA8650-07-D-5800
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#include "IMFViewerApplication.h"

#if !defined(_MSC_VER)
#include <unistd.h>
#endif

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QPluginLoader>

#include <QtWidgets/QSplashScreen>
#include <QtWidgets/QMessageBox>

#include "SIMPLib/Filtering/FilterManager.h"
#include "SIMPLib/Plugin/ISIMPLibPlugin.h"
#include "SIMPLib/Plugin/PluginManager.h"

#include "SVWidgetsLib/Core/FilterWidgetManager.h"
#include "SVWidgetsLib/Dialogs/AboutPlugins.h"
#include "SVWidgetsLib/QtSupport/QtSStyles.h"
#include "SVWidgetsLib/QtSupport/QtSSettings.h"

#include "IMFViewer/IMFViewer_UI.h"

#include "BrandedStrings.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewerApplication::IMFViewerApplication(int& argc, char** argv)
: QApplication(argc, argv)
{

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewerApplication::~IMFViewerApplication()
{

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewerApplication::initialize(int argc, char* argv[])
{
  // Load application plugins.
  QVector<ISIMPLibPlugin*> plugins = loadPlugins();

  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewer_UI* IMFViewerApplication::getNewIMFViewerInstance()
{
  // Create new DREAM3D instance
  IMFViewer_UI* newInstance = new IMFViewer_UI(NULL);
  newInstance->setAttribute(Qt::WA_DeleteOnClose);
  newInstance->setWindowTitle("IMF Viewer");

  setActiveInstance(newInstance);

  return newInstance;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::loadStyleSheet(const QString& sheetName)
{
  QFile file(":/Resources/StyleSheets/" + sheetName.toLower() + ".qss");
  file.open(QFile::ReadOnly);
  QString styleSheet = QString::fromLatin1(file.readAll());
  qApp->setStyleSheet(styleSheet);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QVector<ISIMPLibPlugin*> IMFViewerApplication::loadPlugins()
{
  QStringList pluginDirs;
  pluginDirs << applicationDirPath();

  QDir aPluginDir = QDir(applicationDirPath());
  qDebug() << "Loading " << BrandedStrings::ApplicationName << " Plugins....";
  QString thePath;

#if defined(Q_OS_WIN)
  if(aPluginDir.cd("Plugins"))
  {
    thePath = aPluginDir.absolutePath();
    pluginDirs << thePath;
  }
#elif defined(Q_OS_MAC)
  // Look to see if we are inside an .app package or inside the 'tools' directory
  if(aPluginDir.dirName() == "MacOS")
  {
    aPluginDir.cdUp();
    thePath = aPluginDir.absolutePath() + "/Plugins";
    qDebug() << "  Adding Path " << thePath;
    pluginDirs << thePath;
    aPluginDir.cdUp();
    aPluginDir.cdUp();
    // We need this because Apple (in their infinite wisdom) changed how the current working directory is set in OS X 10.9 and above. Thanks Apple.
    chdir(aPluginDir.absolutePath().toLatin1().constData());
  }
  if(aPluginDir.dirName() == "bin")
  {
    aPluginDir.cdUp();
    // We need this because Apple (in their infinite wisdom) changed how the current working directory is set in OS X 10.9 and above. Thanks Apple.
    chdir(aPluginDir.absolutePath().toLatin1().constData());
  }
  // aPluginDir.cd("Plugins");
  thePath = aPluginDir.absolutePath() + "/Plugins";
  qDebug() << "  Adding Path " << thePath;
  pluginDirs << thePath;

// This is here for Xcode compatibility
#ifdef CMAKE_INTDIR
  aPluginDir.cdUp();
  thePath = aPluginDir.absolutePath() + "/Plugins/" + CMAKE_INTDIR;
  pluginDirs << thePath;
#endif
#else
  // We are on Linux - I think
  // Try the current location of where the application was launched from which is
  // typically the case when debugging from a build tree
  if(aPluginDir.cd("Plugins"))
  {
    thePath = aPluginDir.absolutePath();
    pluginDirs << thePath;
    aPluginDir.cdUp(); // Move back up a directory level
  }

  if(thePath.isEmpty())
  {
    // Now try moving up a directory which is what should happen when running from a
    // proper distribution of SIMPLView
    aPluginDir.cdUp();
    if(aPluginDir.cd("Plugins"))
    {
      thePath = aPluginDir.absolutePath();
      pluginDirs << thePath;
      aPluginDir.cdUp(); // Move back up a directory level
      int no_error = chdir(aPluginDir.absolutePath().toLatin1().constData());
      if(no_error < 0)
      {
        qDebug() << "Could not set the working directory.";
      }
    }
  }
#endif

  QByteArray pluginEnvPath = qgetenv("SIMPL_PLUGIN_PATH");
  qDebug() << "SIMPL_PLUGIN_PATH:" << pluginEnvPath;

  char sep = ';';
#if defined(Q_OS_WIN)
  sep = ':';
#endif
  QList<QByteArray> envPaths = pluginEnvPath.split(sep);
  foreach(QByteArray envPath, envPaths)
  {
    if(envPath.size() > 0)
    {
      pluginDirs << QString::fromLatin1(envPath);
    }
  }

  int dupes = pluginDirs.removeDuplicates();
  qDebug() << "Removed " << dupes << " duplicate Plugin Paths";
  QStringList pluginFilePaths;

  foreach(QString pluginDirString, pluginDirs)
  {
    qDebug() << "Plugin Directory being Searched: " << pluginDirString;
    aPluginDir = QDir(pluginDirString);
    foreach(QString fileName, aPluginDir.entryList(QDir::Files))
    {
//   qDebug() << "File: " << fileName() << "\n";
#ifdef QT_DEBUG
      if(fileName.endsWith("_debug.guiplugin", Qt::CaseSensitive))
#else
      if(fileName.endsWith(".guiplugin", Qt::CaseSensitive)            // We want ONLY Release plugins
         && !fileName.endsWith("_debug.guiplugin", Qt::CaseSensitive)) // so ignore these plugins
#endif
      {
        pluginFilePaths << aPluginDir.absoluteFilePath(fileName);
        // qWarning(aPluginDir.absoluteFilePath(fileName).toLatin1(), "%s");
        // qDebug() << "Adding " << aPluginDir.absoluteFilePath(fileName)() << "\n";
      }
    }
  }

  FilterManager* filterManager = FilterManager::Instance();
  FilterWidgetManager* fwm = FilterWidgetManager::Instance();

  // THIS IS A VERY IMPORTANT LINE: It will register all the known filters in the dream3d library. This
  // will NOT however get filters from plugins. We are going to have to figure out how to compile filters
  // into their own plugin and load the plugins from a command line.
  FilterManager::RegisterKnownFilters(filterManager);

  PluginManager* pluginManager = PluginManager::Instance();
  QList<PluginProxy::Pointer> proxies = AboutPlugins::ReadPluginCache();
  QMap<QString, bool> loadingMap;
  for(QList<PluginProxy::Pointer>::iterator nameIter = proxies.begin(); nameIter != proxies.end(); nameIter++)
  {
    PluginProxy::Pointer proxy = *nameIter;
    loadingMap.insert(proxy->getPluginName(), proxy->getEnabled());
  }

  // Now that we have a sorted list of plugins, go ahead and load them all from the
  // file system and add each to the toolbar and menu
  foreach(QString path, pluginFilePaths)
  {
    qDebug() << "Plugin Being Loaded:" << path;
    QApplication::instance()->processEvents();
    QPluginLoader* loader = new QPluginLoader(path);
    QFileInfo fi(path);
    QString fileName = fi.fileName();
    QObject* plugin = loader->instance();
    qDebug() << "    Pointer: " << plugin << "\n";
    if(plugin != nullptr)
    {
      ISIMPLibPlugin* ipPlugin = qobject_cast<ISIMPLibPlugin*>(plugin);
      if(ipPlugin != nullptr)
      {
        QString pluginName = ipPlugin->getPluginFileName();
        if(loadingMap.value(pluginName, true))
        {
          QString msg = QObject::tr("Loading Plugin %1  ").arg(fileName);
          this->m_SplashScreen->showMessage(msg, Qt::AlignVCenter | Qt::AlignRight, Qt::white);
          // ISIMPLibPlugin::Pointer ipPluginPtr(ipPlugin);
          ipPlugin->registerFilterWidgets(fwm);
          ipPlugin->registerFilters(filterManager);
          ipPlugin->setDidLoad(true);
        }
        else
        {
          ipPlugin->setDidLoad(false);
        }

        ipPlugin->setLocation(path);
        pluginManager->addPlugin(ipPlugin);
      }
      m_PluginLoaders.push_back(loader);
    }
    else
    {
      m_SplashScreen->hide();
      QString message("The plugin did not load with the following error\n\n");
      message.append(loader->errorString());
      message.append("\n\n");
      message.append("Possible causes include missing libraries that plugin depends on.");
      QMessageBox box(QMessageBox::Critical, tr("Plugin Load Error"), tr(message.toStdString().c_str()));
      box.setStandardButtons(QMessageBox::Ok | QMessageBox::Default);
      box.setDefaultButton(QMessageBox::Ok);
      box.setWindowFlags(box.windowFlags() | Qt::WindowStaysOnTopHint);
      box.exec();
      m_SplashScreen->show();
      delete loader;
    }
  }

  return pluginManager->getPluginsVector();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::setActiveInstance(IMFViewer_UI* instance)
{
  if(nullptr == instance)
  {
    return;
  }

  m_ActiveInstance = instance;
}
