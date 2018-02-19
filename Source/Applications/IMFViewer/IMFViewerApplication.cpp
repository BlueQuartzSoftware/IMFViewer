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

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QMimeDatabase>

#include <QtWidgets/QFileDialog>

#include "SVWidgetsLib/QtSupport/QtSStyles.h"
#include "SVWidgetsLib/QtSupport/QtSRecentFileList.h"
#include "SVWidgetsLib/Widgets/SIMPLViewMenuItems.h"

#include "IMFViewer/IMFController.h"
#include "IMFViewer/IMFViewer_UI.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewerApplication::IMFViewerApplication(int& argc, char** argv) :
  QApplication(argc, argv),
  m_Controller(new IMFController())
{
//  loadStyleSheet("light");

  createApplicationMenu();

  // Connection to update the recent files list on all windows when it changes
  QtSRecentFileList* recentsList = QtSRecentFileList::instance();
  connect(recentsList, SIGNAL(fileListChanged(const QString&)), this, SLOT(updateRecentFileList(const QString&)));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewerApplication::~IMFViewerApplication()
{
  delete m_Controller;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::createApplicationMenu()
{
  SIMPLViewMenuItems* menuItems = SIMPLViewMenuItems::Instance();

  m_ApplicationMenuBar = new QMenuBar();
  QMenu* fileMenu = new QMenu("File", m_ApplicationMenuBar);

  QAction* importAction = new QAction("Import");
  importAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_I));
  connect(importAction, &QAction::triggered, this, &IMFViewerApplication::importFile);
  fileMenu->addAction(importAction);

  fileMenu->addSeparator();

  QMenu* recentsMenu = menuItems->getMenuRecentFiles();
  QAction* clearRecentsAction = menuItems->getActionClearRecentFiles();
  fileMenu->addMenu(recentsMenu);


  m_ApplicationMenuBar->addMenu(fileMenu);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::updateRecentFileList(const QString& file)
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
void IMFViewerApplication::openRecentFile()
{
  QAction* action = qobject_cast<QAction*>(sender());

  if(action && m_ActiveInstance)
  {
    QString filePath = action->data().toString();

    bool success = m_Controller->importFile(filePath, m_ActiveInstance);

    if (success)
    {
      m_OpenDialogLastDirectory = filePath;
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::importFile()
{
  if (m_ActiveInstance != nullptr)
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

    // Open a file in the application
    QString filter = tr("Data Files (*.dream3d *.vtk *.vti *.vtp *.vtr *.vts *.vtu *.stl %1 %3 %3);;"
                        "DREAM.3D Files (*.dream3d);;"
                        "Image Files (%1 %2 %3);;"
                        "VTK Files (*.vtk *.vti *.vtp *.vtr *.vts *.vtu);;"
                        "STL Files (*.stl)").arg(pngSuffixStr).arg(tiffSuffixStr).arg(jpegSuffixStr);
    QString filePath = QFileDialog::getOpenFileName(m_ActiveInstance, "Open Input File", m_OpenDialogLastDirectory, filter);
    if (filePath.isEmpty())
    {
      return;
    }

    m_Controller->importFile(filePath, m_ActiveInstance);
  }
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

  #if defined(Q_OS_WIN)
  newInstance->setMenuBar(m_ApplicationMenuBar);
  #endif

  setActiveInstance(newInstance);

  return newInstance;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::loadStyleSheet(const QString &sheetName)
{
  QFile file(":/Resources/StyleSheets/" + sheetName.toLower() + ".qss");
  file.open(QFile::ReadOnly);
  QString styleSheet = QString::fromLatin1(file.readAll());
  qApp->setStyleSheet(styleSheet);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::setActiveInstance(IMFViewer_UI* instance)
{
  if (instance == nullptr)
  {
    return;
  }

  // Disconnections from the old instance
  if (m_ActiveInstance != nullptr)
  {

  }

  // Connections to the new instance

  m_ActiveInstance = instance;
}





