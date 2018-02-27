/* ============================================================================
* Copyright (c) 2009-2015 BlueQuartz Software, LLC
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
* Neither the name of BlueQuartz Software, the US Air Force, nor the names of its
* contributors may be used to endorse or promote products derived from this software
* without specific prior written permission.
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
* The code contained herein was partially funded by the followig contracts:
*    United States Air Force Prime Contract FA8650-07-D-5800
*    United States Air Force Prime Contract FA8650-10-D-5210
*    United States Prime Contract Navy N00173-07-C-2068
*
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#include "IMFViewer_UI.h"

#include <QtCore/QFileInfo>
#include <QtCore/QMimeDatabase>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

#include "IMFViewer/Dialogs/LoadHDF5FileDialog.h"

#include "SIMPLib/Utilities/SIMPLH5DataReader.h"
#include "SIMPLib/Utilities/SIMPLH5DataReaderRequirements.h"

#include "SVWidgetsLib/QtSupport/QtSSettings.h"
#include "SVWidgetsLib/QtSupport/QtSRecentFileList.h"
#include "SVWidgetsLib/Widgets/SIMPLViewMenuItems.h"

#include "ui_IMFViewer_UI.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
class IMFViewer_UI::vsInternals : public Ui::IMFViewer_UI
{
public:
  vsInternals()
  {
  }
};

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewer_UI::IMFViewer_UI(QWidget* parent) :
  QMainWindow(parent)
, m_Internals(new vsInternals())
{
  m_Internals->setupUi(this);

  setupGui();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewer_UI::~IMFViewer_UI()
{
  writeSettings();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::setupGui()
{
  // Connection to update the recent files list on all windows when it changes
  QtSRecentFileList* recentsList = QtSRecentFileList::instance(5, this);
  connect(recentsList, SIGNAL(fileListChanged(const QString&)), this, SLOT(updateRecentFileList(const QString&)));

  readSettings();

  m_Internals->vsWidget->setFilterView(m_Internals->treeView);
  m_Internals->vsWidget->setInfoWidget(m_Internals->infoWidget);

  createMenu();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importDataContainerArray(QString filePath, DataContainerArray::Pointer dca)
{
  VSController* controller = m_Internals->vsWidget->getController();
  controller->importDataContainerArray(filePath, dca);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importData(const QString &filePath)
{
  VSController* controller = m_Internals->vsWidget->getController();
  controller->importData(filePath);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importFile()
{
  QMimeDatabase db;

  QMimeType pngType = db.mimeTypeForName("image/png");
  QStringList pngSuffixes = pngType.suffixes();
  QString pngSuffixStr = pngSuffixes.join(" *.");
  pngSuffixStr.prepend("*.");

  QMimeType tiffType = db.mimeTypeForName("image/tiff");
  QStringList tiffSuffixes = tiffType.suffixes();
  QString tiffSuffixStr = tiffSuffixes.join(" *.");
  tiffSuffixStr.prepend("*.");

  QMimeType jpegType = db.mimeTypeForName("image/jpeg");
  QStringList jpegSuffixes = jpegType.suffixes();
  QString jpegSuffixStr = jpegSuffixes.join(" *.");
  jpegSuffixStr.prepend("*.");

  QMimeType bmpType = db.mimeTypeForName("image/bmp");
  QStringList bmpSuffixes = bmpType.suffixes();
  QString bmpSuffixStr = bmpSuffixes.join(" *.");
  bmpSuffixStr.prepend("*.");

  // Open a file in the application
  QString filter = tr("Data Files (*.dream3d *.vtk *.vti *.vtp *.vtr *.vts *.vtu *.stl %1 %3 %3 %4);;"
                      "DREAM.3D Files (*.dream3d);;"
                      "Image Files (%1 %2 %3 %4);;"
                      "VTK Files (*.vtk *.vti *.vtp *.vtr *.vts *.vtu);;"
                      "STL Files (*.stl)").arg(pngSuffixStr).arg(tiffSuffixStr).arg(jpegSuffixStr).arg(bmpSuffixStr);
  QString filePath = QFileDialog::getOpenFileName(this, "Open Input File", m_OpenDialogLastDirectory, filter);
  if (filePath.isEmpty())
  {
    return;
  }

  importFile(filePath);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer_UI::importFile(const QString &filePath)
{
  QMimeDatabase db;

  QMimeType mimeType = db.mimeTypeForFile(filePath, QMimeDatabase::MatchContent);
  QString mimeName = mimeType.name();

  QFileInfo fi(filePath);
  QString ext = fi.completeSuffix().toLower();
  bool success = false;
  if (ext == "dream3d")
  {
    success = openDREAM3DFile(filePath);
  }
  else if (mimeType.inherits("image/png") || mimeType.inherits("image/tiff") || mimeType.inherits("image/jpeg") || mimeType.inherits("image/bmp"))
  {
    importData(filePath);
    success = true;
  }
  else if (ext == "vtk" || ext == "vti" || ext == "vtp" || ext == "vtr"
           || ext == "vts" || ext == "vtu")
  {
    importData(filePath);
    success = true;
  }
  else if (ext == "stl")
  {
    importData(filePath);
    success = true;
  }
  else
  {
    QMessageBox::critical(this, "Invalid File Type",
                          tr("IMF Viewer failed to open the file because the file extension, '.%1', is not supported by the "
                             "application.").arg(ext), QMessageBox::StandardButton::Ok);
    return false;
  }

  return success;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer_UI::openDREAM3DFile(const QString &filePath)
{
  QFileInfo fi(filePath);

  SIMPLH5DataReader reader;
  bool success = reader.openFile(filePath);
  if (success)
  {
    int err = 0;
    SIMPLH5DataReaderRequirements req(SIMPL::Defaults::AnyPrimitive, SIMPL::Defaults::AnyComponentSize, AttributeMatrix::Type::Any, IGeometry::Type::Any);
    DataContainerArrayProxy proxy = reader.readDataContainerArrayStructure(&req, err);
    if (proxy.dataContainers.isEmpty())
    {
      QMessageBox::critical(this, "Empty File",
                            tr("IMF Viewer opened the file '%1', but it was empty.").arg(fi.fileName()), QMessageBox::StandardButton::Ok);
      return false;
    }

    bool containsCellAttributeMatrices = false;
    bool containsValidArrays = false;

    QStringList dcNames = proxy.dataContainers.keys();
    for (int i = 0; i < dcNames.size(); i++)
    {
      QString dcName = dcNames[i];
      DataContainerProxy dcProxy = proxy.dataContainers[dcName];

      // We want only data containers with geometries displayed
      if (dcProxy.dcType == static_cast<unsigned int>(DataContainer::Type::Unknown))
      {
        proxy.dataContainers.remove(dcName);
      }
      else
      {
        QStringList amNames = dcProxy.attributeMatricies.keys();
        for (int j = 0; j < amNames.size(); j++)
        {
          QString amName = amNames[j];
          AttributeMatrixProxy amProxy = dcProxy.attributeMatricies[amName];

          // We want only cell attribute matrices displayed
          if (amProxy.amType != AttributeMatrix::Type::Cell)
          {
            dcProxy.attributeMatricies.remove(amName);
            proxy.dataContainers[dcName] = dcProxy;
          }
          else
          {
            containsCellAttributeMatrices = true;

            if (amProxy.dataArrays.size() > 0)
            {
              containsValidArrays = true;
            }
          }
        }
      }
    }

    if (proxy.dataContainers.size() <= 0)
    {
      QMessageBox::critical(this, "Invalid Data",
                            tr("IMF Viewer failed to open file '%1' because the file does not "
                               "contain any data containers with a supported geometry.").arg(fi.fileName()), QMessageBox::StandardButton::Ok);
      return false;
    }

    QSharedPointer<LoadHDF5FileDialog> dialog = QSharedPointer<LoadHDF5FileDialog>(new LoadHDF5FileDialog());
    dialog->setProxy(proxy);
    int ret = dialog->exec();

    if (ret == QDialog::Accepted)
    {
      DataContainerArrayProxy dcaProxy = dialog->getDataStructureProxy();
      DataContainerArray::Pointer dca = reader.readSIMPLDataUsingProxy(dcaProxy, false);
      importDataContainerArray(filePath, dca);
      return true;
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::updateRecentFileList(const QString& file)
{
  SIMPLViewMenuItems* menuItems = SIMPLViewMenuItems::Instance();
  QMenu* recentFilesMenu = menuItems->getMenuRecentFiles();
  QAction* clearRecentFilesAction = menuItems->getActionClearRecentFiles();

  // Clear the Recent Items Menu
  recentFilesMenu->clear();

  // Get the list from the static object
  QStringList files = QtSRecentFileList::instance()->fileList();
  foreach(QString file, files)
  {
    QAction* action = recentFilesMenu->addAction(QtSRecentFileList::instance()->parentAndFileName(file));
    action->setData(file);
    action->setVisible(true);
    connect(action, SIGNAL(triggered()), this, SLOT(openRecentFile()));
  }

  recentFilesMenu->addSeparator();
  recentFilesMenu->addAction(clearRecentFilesAction);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::openRecentFile()
{
  QAction* action = qobject_cast<QAction*>(sender());

  if(action)
  {
    QString filePath = action->data().toString();

    loadSessionFromFile(filePath);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::saveSession()
{
    QMimeDatabase db;

    QMimeType jsonType = db.mimeTypeForName("application/json");
    QStringList jsonSuffixes = jsonType.suffixes();
    QString jsonSuffixStr = jsonSuffixes.join(" *.");
    jsonSuffixStr.prepend("*.");

    // Open a file in the application
    QString filter = tr("JSON Files (%1)").arg(jsonSuffixStr);
    QString filePath = QFileDialog::getSaveFileName(this, "Open Input File", m_OpenDialogLastDirectory, filter);
    if (filePath.isEmpty())
    {
      return;
    }

    m_OpenDialogLastDirectory = filePath;

    VSController* controller = m_Internals->vsWidget->getController();
    bool success = controller->saveSession(filePath);

    if (success)
    {
      // Add file to the recent files list
      QtSRecentFileList* list = QtSRecentFileList::instance();
      list->addFile(filePath);
    }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::loadSession()
{
    QMimeDatabase db;

    QMimeType jsonType = db.mimeTypeForName("application/json");
    QStringList jsonSuffixes = jsonType.suffixes();
    QString jsonSuffixStr = jsonSuffixes.join(" *.");
    jsonSuffixStr.prepend("*.");

    // Open a file in the application
    QString filter = tr("JSON Files (%1)").arg(jsonSuffixStr);
    QString filePath = QFileDialog::getOpenFileName(this, "Open Session File", m_OpenDialogLastDirectory, filter);
    if (filePath.isEmpty())
    {
      return;
    }

    m_OpenDialogLastDirectory = filePath;

    loadSessionFromFile(filePath);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::loadSessionFromFile(const QString &filePath)
{
  VSController* controller = m_Internals->vsWidget->getController();
  bool success = controller->loadSession(filePath);

  if (success)
  {
    // Add file to the recent files list
    QtSRecentFileList* list = QtSRecentFileList::instance();
    list->addFile(filePath);
  }
}

// -----------------------------------------------------------------------------
//  Read our settings from a file
// -----------------------------------------------------------------------------
void IMFViewer_UI::readSettings()
{
  QSharedPointer<QtSSettings> prefs = QSharedPointer<QtSSettings>(new QtSSettings());

  // Read the window settings from the prefs file
  readWindowSettings(prefs.data());

  QtSRecentFileList::instance()->readList(prefs.data());
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::readWindowSettings(QtSSettings* prefs)
{
  bool ok = false;
  prefs->beginGroup("WindowSettings");
  if(prefs->contains(QString("MainWindowGeometry")))
  {
    QByteArray geo_data = prefs->value("MainWindowGeometry", QByteArray());
    ok = restoreGeometry(geo_data);
    if(!ok)
    {
      qDebug() << "Error Restoring the Window Geometry"
               << "\n";
    }
  }

  if(prefs->contains(QString("MainWindowState")))
  {
    QByteArray layout_data = prefs->value("MainWindowState", QByteArray());
    restoreState(layout_data);
  }

  prefs->endGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::writeSettings()
{
  QSharedPointer<QtSSettings> prefs = QSharedPointer<QtSSettings>(new QtSSettings());

  // Write the window settings to the prefs file
  writeWindowSettings(prefs.data());

  QtSRecentFileList::instance()->writeList(prefs.data());
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::writeWindowSettings(QtSSettings* prefs)
{
  prefs->beginGroup("WindowSettings");
  QByteArray geo_data = saveGeometry();
  QByteArray layout_data = saveState();
  prefs->setValue(QString("MainWindowGeometry"), geo_data);
  prefs->setValue(QString("MainWindowState"), layout_data);

  prefs->endGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QMenuBar* IMFViewer_UI::getMenuBar()
{
  return m_MenuBar;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::createMenu()
{
  SIMPLViewMenuItems* menuItems = SIMPLViewMenuItems::Instance();

  m_MenuBar = new QMenuBar();

  // File Menu
  QMenu* fileMenu = new QMenu("File", m_MenuBar);

  QAction* importAction = new QAction("Import");
  importAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_I));
  connect(importAction, &QAction::triggered, this, static_cast<void (IMFViewer_UI::*)(void)>(&IMFViewer_UI::importFile));
  fileMenu->addAction(importAction);

  fileMenu->addSeparator();
    
  QAction* openAction = menuItems->getActionOpen();
  connect(openAction, &QAction::triggered, this, &IMFViewer_UI::loadSession);
  fileMenu->addAction(openAction);
    
  QAction* saveAction = menuItems->getActionSave();
  connect(saveAction, &QAction::triggered, this, &IMFViewer_UI::saveSession);
  fileMenu->addAction(saveAction);
    
  fileMenu->addSeparator();

  QMenu* recentsMenu = menuItems->getMenuRecentFiles();
  fileMenu->addMenu(recentsMenu);

  QAction* clearRecentsAction = menuItems->getActionClearRecentFiles();
  connect(clearRecentsAction, &QAction::triggered, [=] {
    // Clear the Recent Items Menu
    recentsMenu->clear();
    recentsMenu->addSeparator();
    recentsMenu->addAction(clearRecentsAction);

    // Clear the actual list
    QtSRecentFileList* recents = QtSRecentFileList::instance();
    recents->clear();

    // Write out the empty list
    QSharedPointer<QtSSettings> prefs = QSharedPointer<QtSSettings>(new QtSSettings());
    recents->writeList(prefs.data());
  });

  m_MenuBar->addMenu(fileMenu);

  // Add Filter Menu
  QMenu* filterMenu = m_Internals->vsWidget->getFilterMenu();
  m_MenuBar->addMenu(filterMenu);

  // Apply Menu Bar
  setMenuBar(m_MenuBar);
}
