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

#include <QtCore/QTime>
#include <QtCore/QPluginLoader>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QDir>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

#include <QtGui/QDesktopServices>
#include <QtGui/QBitmap>
#include <QtGui/QFileOpenEvent>
#include <iostream>

#include "IMFViewer.h"
#include "NewFileDialog.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewerApplication::IMFViewerApplication(int& argc, char** argv) :
QApplication(argc, argv),
m_ActiveWindow(NULL),
#if defined(Q_OS_MAC)
m_GlobalMenu(NULL),
#endif
m_OpenDialogLastDirectory("")
{
  // If Mac, initialize global menu
#if defined (Q_OS_MAC)
  m_GlobalMenu = new IMFViewerMenu();
#endif
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewerApplication::~IMFViewerApplication()
{

}

#if 0
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::updateRecentFileList(const QString& file)
{
#if defined (Q_OS_MAC)
  QMenu* recentFilesMenu = m_GlobalMenu->getRecentFilesMenu();
  QAction* clearRecentFilesAction = m_GlobalMenu->getClearRecentFiles();

  // Clear the Recent Items Menu
  recentFilesMenu->clear();

  // Get the list from the static object
  QStringList files = QRecentFileList::instance()->fileList();
  foreach(QString file, files)
  {
    QAction* action = new QAction(recentFilesMenu);
    action->setText(QRecentFileList::instance()->parentAndFileName(file));
    action->setData(file);
    action->setVisible(true);
    recentFilesMenu->addAction(action);
    connect(action, SIGNAL(triggered()), this, SLOT(openRecentFile()));
  }

  recentFilesMenu->addSeparator();
  recentFilesMenu->addAction(clearRecentFilesAction);
#else
  QList<IMFViewer*> windows = csdfApp->getIMFViewerInstances();

  for (int i = 0; i < windows.size(); i++)
  {
    IMFViewer* window = windows[i];

    if (NULL != window)
    {
      QMenu* recentFilesMenu = window->getIMFViewerMenu()->getRecentFilesMenu();
      QAction* clearRecentFilesAction = window->getIMFViewerMenu()->getClearRecentFiles();

      // Clear the Recent Items Menu
      recentFilesMenu->clear();

      // Get the list from the static object
      QStringList files = QRecentFileList::instance()->fileList();
      foreach(QString file, files)
      {
        QAction* action = new QAction(recentFilesMenu);
        action->setText(QRecentFileList::instance()->parentAndFileName(file));
        action->setData(file);
        action->setVisible(true);
        recentFilesMenu->addAction(action);
        connect(action, SIGNAL(triggered()), this, SLOT(openRecentFile()));
      }

      recentFilesMenu->addSeparator();
      recentFilesMenu->addAction(clearRecentFilesAction);
    }
  }
#endif
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::on_actionClearRecentFiles_triggered()
{
#if defined (Q_OS_MAC)
  QMenu* recentFilesMenu = m_GlobalMenu->getRecentFilesMenu();
  QAction* clearRecentFilesAction = m_GlobalMenu->getClearRecentFiles();

  // Clear the Recent Items Menu
  recentFilesMenu->clear();
  recentFilesMenu->addSeparator();
  recentFilesMenu->addAction(clearRecentFilesAction);

  // Clear the actual list
  QRecentFileList* recents = QRecentFileList::instance();
  recents->clear();

  // Write out the empty list
  DREAM3DSettings prefs;
  recents->writeList(prefs);
#else
  QMap<IMFViewer*, QMenu*> windows = csdfApp->getDREAM3DInstanceMap();

  QMapIterator<IMFViewer*, QMenu*> iter(windows);
  while (iter.hasNext())
  {
    iter.next();

    IMFViewer* window = iter.key();

    if (NULL != window)
    {
      QMenu* recentFilesMenu = window->getIMFViewerMenu()->getRecentFilesMenu();
      QAction* clearRecentFilesAction = window->getIMFViewerMenu()->getClearRecentFiles();

      // Clear the Recent Items Menu
      recentFilesMenu->clear();
      recentFilesMenu->addSeparator();
      recentFilesMenu->addAction(clearRecentFilesAction);

      // Clear the actual list
      QRecentFileList* recents = QRecentFileList::instance();
      recents->clear();

      // Write out the empty list
      DREAM3DSettings prefs;
      recents->writeList(prefs);
    }
  }
#endif
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::openRecentFile()
{
  QAction* action = qobject_cast<QAction*>(sender());

  if (action)
  {
    QString filePath = action->data().toString();

    newInstanceFromFile(filePath, true, true);

    // Add file path to the recent files list for both instances
    QRecentFileList* list = QRecentFileList::instance();
    list->addFile(filePath);
  }
}
#endif // 0


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QList<IMFViewer*> IMFViewerApplication::getIMFViewerInstances()
{
  return m_IMFViewerInstances;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::registerIMFViewerWindow(IMFViewer* window)
{
  m_IMFViewerInstances.push_back(window);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::unregisterIMFViewerWindow(IMFViewer* window)
{
  m_IMFViewerInstances.removeAll(window);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::on_actionNew_triggered()
{
  NewFileDialog* dialog = new NewFileDialog(m_ActiveWindow);
  dialog->setWindowFlags(Qt::Window);
  dialog->setWindowModality(Qt::ApplicationModal);
  dialog->exec();

  int result = dialog->result();
  if (result == QDialog::Accepted)
  {
    QString fileName = dialog->getFileName();
    int partId = dialog->getPartId();

    if (m_ActiveWindow->hasRootItem() == false)
    {
      m_ActiveWindow->createRootItem(fileName, partId);
    }
    else
    {
      IMFViewer* newInstance = getNewIMFViewerInstance();
      newInstance->createRootItem(fileName, partId);
      newInstance->show();
    }
  }

  delete dialog;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::on_actionOpen_triggered()
{
  IMFViewer* window = dynamic_cast<IMFViewer*>(sender());
  if (NULL != window)
  {
    QString filePath = QFileDialog::getOpenFileName(window, tr("Select CSDF File"), m_OpenDialogLastDirectory, tr("CSDF Files (*.csdf)"));
    if (filePath.isEmpty()) { return; }

    m_OpenDialogLastDirectory = filePath;
    QFileInfo fi(filePath);

    if (window->hasRootItem() == true)
    {
      IMFViewer* newInstance = getNewIMFViewerInstance();
      newInstance->openCSDFFile(filePath);
      newInstance->show();
    }
    else
    {
      window->openCSDFFile(filePath);
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::on_actionCloseWindow_triggered()
{
  m_ActiveWindow->close();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::on_actionExit_triggered()
{
  bool shouldReallyClose = true;
  for (int i = m_IMFViewerInstances.size() - 1; i >= 0; i--)
  {
    IMFViewer* window = m_IMFViewerInstances[i];
    if (NULL != window)
    {
      if (window->close() == false)
      {
        shouldReallyClose = false;
      }
    }
  }

  if (shouldReallyClose == true)
  {
    quit();
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::on_treeWidgetContextMenuRequested(const QPoint& pos)
{  
  if (NULL != m_ActiveWindow)
  {
    m_ActiveWindow->treeWidgetCustomContextMenuRequested(pos);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewer* IMFViewerApplication::getNewIMFViewerInstance()
{
  // Create new DREAM3D instance
  IMFViewer* newInstance = new IMFViewer(NULL);
  newInstance->setAttribute(Qt::WA_DeleteOnClose);
  newInstance->setWindowTitle("CSDF Importer");

  if (NULL != m_ActiveWindow)
  {
    newInstance->move(m_ActiveWindow->x() + 45, m_ActiveWindow->y() + 45);
  }

  m_ActiveWindow = newInstance;

  connect(newInstance, SIGNAL(IMFViewerChangedState(IMFViewer*)), this, SLOT(activeWindowChanged(IMFViewer*)));

  registerIMFViewerWindow(newInstance);
  return newInstance;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::activeWindowChanged(IMFViewer* instance)
{
  if (instance->isActiveWindow())
  {
    m_ActiveWindow = instance;
  }
  else
  {
    /* If the inactive signal got fired and there are no more windows,
    * this means that the last window has been closed.
    * Disable menu items. */
    if (m_IMFViewerInstances.size() <= 0)
    {
      m_ActiveWindow = NULL;
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::toRunningState()
{
#if defined (Q_OS_MAC)
  //m_GlobalMenu->getClearPipeline()->setDisabled(true);
#else
  //m_ActiveWindow->getIMFViewerMenu()->getClearPipeline()->setDisabled(true);
#endif

  IMFViewer* runningInstance = qobject_cast<IMFViewer*>(sender());
  if (NULL != runningInstance)
  {
    m_CurrentlyRunningInstances.insert(runningInstance);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerApplication::toIdleState()
{
#if defined (Q_OS_MAC)
  //m_GlobalMenu->getClearPipeline()->setEnabled(true);
#else
  //m_ActiveWindow->getIMFViewerMenu()->getClearPipeline()->setEnabled(true);
#endif

  IMFViewer* runningInstance = qobject_cast<IMFViewer*>(sender());
  if (NULL != runningInstance)
  {
    m_CurrentlyRunningInstances.remove(runningInstance);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewerApplication::isCurrentlyRunning(IMFViewer* instance)
{
  return m_CurrentlyRunningInstances.contains(instance);
}






