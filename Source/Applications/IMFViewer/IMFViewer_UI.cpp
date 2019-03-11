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

#include <QtConcurrent>

#include <QtCore/QFileInfo>
#include <QtCore/QMimeDatabase>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

#include "SIMPLib/FilterParameters/IntVec3FilterParameter.h"
#include "SIMPLib/FilterParameters/FloatVec3.h"
#include "SIMPLib/Filtering/FilterPipeline.h"
#include "SIMPLib/Filtering/QMetaObjectUtilities.h"
#include "SIMPLib/Plugin/PluginManager.h"
#include "SIMPLib/Utilities/SIMPLH5DataReader.h"
#include "SIMPLib/Utilities/SIMPLH5DataReaderRequirements.h"

#include "SVWidgetsLib/QtSupport/QtSRecentFileList.h"
#include "SVWidgetsLib/QtSupport/QtSSettings.h"
#include "SVWidgetsLib/Widgets/SVStyle.h"

#include "SIMPLVtkLib/Wizards/ExecutePipeline/ExecutePipelineWizard.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/FijiListWidget.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/ImportMontageWizard.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/ImporterWorker.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/RobometListWidget.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/TileConfigFileGenerator.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/ZeissListWidget.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/ImportMontageConstants.h"

#include "BrandedStrings.h"

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
IMFViewer_UI::IMFViewer_UI(QWidget* parent)
: QMainWindow(parent)
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
  delete m_RecentFilesMenu;
  delete m_ClearRecentsAction;

  writeSettings();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::setupGui()
{
  connect(m_Internals->importDataBtn, &QPushButton::clicked, this, &IMFViewer_UI::importData);
  connect(m_Internals->importMontageBtn, &QPushButton::clicked, this, &IMFViewer_UI::importMontage);

  m_QueueWidget = VSQueueWidget::New();
  connect(m_Internals->queueBtn, &QPushButton::clicked, [=] { m_QueueWidget->show(); });

  // Connection to update the recent files list on all windows when it changes
  QtSRecentFileList* recentsList = QtSRecentFileList::Instance(5, this);
  connect(recentsList, SIGNAL(fileListChanged(const QString&)), this, SLOT(updateRecentFileList(const QString&)));

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);
  connect(baseWidget, &VSMainWidgetBase::importerAddedToQueue, m_QueueWidget.get(), &VSQueueWidget::addImporterToQueue);
  connect(baseWidget, &VSMainWidgetBase::notifyErrorMessage, this, &IMFViewer_UI::processErrorMessage);
  connect(baseWidget, &VSMainWidgetBase::notifyStatusMessage, this, &IMFViewer_UI::processStatusMessage);

  createMenu();

  readSettings();

  m_Internals->vsWidget->setFilterView(m_Internals->treeView);
  m_Internals->vsWidget->setInfoWidget(m_Internals->infoWidget);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importData()
{
  QString filter = tr("DREAM3D File (*.dream3d);;VTK File (*.vtk *.vti *.vtp *.vtr *.vts *.vtu);;"
                      "STL File (*.stl);;Image File (*.png *.tiff *.jpeg *.bmp);;All Files (*.*)");
  QString filePath = QFileDialog::getOpenFileName(this, tr("Select a data file"), m_OpenDialogLastDirectory, filter);

  if(filePath.isEmpty())
  {
    return;
  }

  m_OpenDialogLastDirectory = filePath;

  QFileInfo fi(filePath);
  QString ext = fi.completeSuffix();
  QMimeDatabase db;
  QMimeType mimeType = db.mimeTypeForFile(filePath, QMimeDatabase::MatchContent);

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);

  if(ext == "dream3d")
  {
    baseWidget->launchHDF5SelectionDialog(filePath);
  }
  else if(ext == "vtk" || ext == "vti" || ext == "vtp" || ext == "vtr" || ext == "vts" || ext == "vtu")
  {
    if(baseWidget->importVTKData(filePath) == false)
    {
      QMessageBox::critical(this, "Unable to Load VTK File",
                            tr("IMF Viewer could not load VTK input file '%1'.  Only files "
                               "with 'vtk', 'vti', 'vtp', 'vtr', 'vts', and 'vtu' extensions are permitted.")
                                .arg(filePath),
                            QMessageBox::StandardButton::Ok);
      return;
    }
  }
  else if(ext == "stl")
  {
    if(baseWidget->importSTLData(filePath) == false)
    {
      QMessageBox::critical(this, "Unable to Load STL File",
                            tr("IMF Viewer could not load STL input file '%1'.  Only files "
                               "with 'stl' extension are permitted.")
                                .arg(filePath),
                            QMessageBox::StandardButton::Ok);
      return;
    }
  }
  else if(mimeType.inherits("image/png") || mimeType.inherits("image/tiff") || mimeType.inherits("image/jpeg") || mimeType.inherits("image/bmp"))
  {
    if(baseWidget->importImageData(filePath) == false)
    {
      QMessageBox::critical(this, "Unable to Load Image File",
                            tr("IMF Viewer could not load image file '%1'.  The file is not "
                               "a png, tiff, jpeg, or bmp image file.")
                                .arg(filePath),
                            QMessageBox::StandardButton::Ok);
      return;
    }
  }
  else
  {
    QMessageBox::critical(this, "Invalid File Type",
                          tr("IMF Viewer failed to detect the proper data file type from the given input file '%1'.  "
                             "Supported file types are DREAM3D, VTK, STL and Image files.")
                              .arg(filePath),
                          QMessageBox::StandardButton::Ok);
    return;
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importMontage()
{
  ImportMontageWizard* montageWizard = new ImportMontageWizard(this);
  int result = montageWizard->exec();

  if (result == QDialog::Accepted)
  {
    VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);
    baseWidget->importMontage(montageWizard);
  }

  delete montageWizard;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::executePipeline()
{
	ExecutePipelineWizard* executePipelineWizard = new ExecutePipelineWizard(m_Internals->vsWidget);
	int result = executePipelineWizard->exec();

	if(result == QDialog::Accepted)
	{
	  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);
	  baseWidget->importPipeline(executePipelineWizard);
	}

	delete executePipelineWizard;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::processErrorMessage(const QString &errorMessage, int code)
{
//  statusBar()->showMessage(errorMessage);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::processStatusMessage(const QString &statusMessage)
{
  statusBar()->showMessage(statusMessage);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::updateRecentFileList(const QString& file)
{
  // Clear the Recent Items Menu
  m_RecentFilesMenu->clear();

  // Get the list from the static object
  QStringList files = QtSRecentFileList::Instance()->fileList();
  foreach(QString file, files)
  {
    QAction* action = m_RecentFilesMenu->addAction(QtSRecentFileList::Instance()->parentAndFileName(file));
    action->setData(file);
    action->setVisible(true);
    connect(action, SIGNAL(triggered()), this, SLOT(openRecentFile()));
  }

  m_RecentFilesMenu->addSeparator();

  m_RecentFilesMenu->addAction(m_ClearRecentsAction);
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
  if(filePath.isEmpty())
  {
    return;
  }

  m_OpenDialogLastDirectory = filePath;

  VSController* controller = m_Internals->vsWidget->getController();
  bool success = controller->saveSession(filePath);

  if(success)
  {
    // Add file to the recent files list
    QtSRecentFileList* list = QtSRecentFileList::Instance();
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
  if(filePath.isEmpty())
  {
    return;
  }

  m_OpenDialogLastDirectory = filePath;

  loadSessionFromFile(filePath);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::loadSessionFromFile(const QString& filePath)
{
  VSController* controller = m_Internals->vsWidget->getController();
  bool success = controller->loadSession(filePath);

  if(success)
  {
    // Add file to the recent files list
    QtSRecentFileList* list = QtSRecentFileList::Instance();
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

  QtSRecentFileList::Instance()->readList(prefs.data());
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

  QByteArray splitterGeometry = prefs->value("Splitter_Geometry", QByteArray());
  m_Internals->splitter->restoreGeometry(splitterGeometry);
  QByteArray splitterSizes = prefs->value("Splitter_Sizes", QByteArray());
  m_Internals->splitter->restoreState(splitterSizes);

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

  QtSRecentFileList::Instance()->writeList(prefs.data());
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

  QByteArray splitterGeometry = m_Internals->splitter->saveGeometry();
  QByteArray splitterSizes = m_Internals->splitter->saveState();
  prefs->setValue(QString("Splitter_Geometry"), splitterGeometry);
  prefs->setValue(QString("Splitter_Sizes"), splitterSizes);

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
  m_MenuBar = new QMenuBar();

  // File Menu
  QMenu* fileMenu = new QMenu("File", m_MenuBar);

  QAction* importDataAction = new QAction("Import Data");
  importDataAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
  connect(importDataAction, &QAction::triggered, this, &IMFViewer_UI::importData);
  fileMenu->addAction(importDataAction);

  QAction* importMontageAction = new QAction("Import Montage");
  importMontageAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_M));
  connect(importMontageAction, &QAction::triggered, this, &IMFViewer_UI::importMontage);
  fileMenu->addAction(importMontageAction);

  QAction* executePipelineAction = new QAction("Execute Pipeline");
  executePipelineAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
  connect(executePipelineAction, &QAction::triggered, this, static_cast<void (IMFViewer_UI::*)(void)>(&IMFViewer_UI::executePipeline));
  fileMenu->addAction(executePipelineAction);

  fileMenu->addSeparator();

  QAction* openAction = new QAction("Open Session");
  openAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_O));
  connect(openAction, &QAction::triggered, this, &IMFViewer_UI::loadSession);
  fileMenu->addAction(openAction);

  QAction* saveAction = new QAction("Save Session");
  openAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
  connect(saveAction, &QAction::triggered, this, &IMFViewer_UI::saveSession);
  fileMenu->addAction(saveAction);

  fileMenu->addSeparator();

  m_RecentFilesMenu = new QMenu("Recent Sessions", this);
  fileMenu->addMenu(m_RecentFilesMenu);

  m_ClearRecentsAction = new QAction("Clear Recent Sessions", this);
  connect(m_ClearRecentsAction, &QAction::triggered, [=] {
    // Clear the Recent Items Menu
    m_RecentFilesMenu->clear();
    m_RecentFilesMenu->addSeparator();
    m_RecentFilesMenu->addAction(m_ClearRecentsAction);

    // Clear the actual list
    QtSRecentFileList* recents = QtSRecentFileList::Instance();
    recents->clear();

    // Write out the empty list
    QSharedPointer<QtSSettings> prefs = QSharedPointer<QtSSettings>(new QtSSettings());
    recents->writeList(prefs.data());
  });

  m_MenuBar->addMenu(fileMenu);

  // Add View Menu
  QMenu* viewMenu = new QMenu("View", m_MenuBar);

  QStringList themeNames = BrandedStrings::LoadedThemeNames;
  if(!themeNames.empty()) // We are not counting the Default theme when deciding whether or not to add the theme menu
  {
    m_ThemeActionGroup = new QActionGroup(this);
    m_MenuThemes = createThemeMenu(m_ThemeActionGroup, viewMenu);

    viewMenu->addMenu(m_MenuThemes);

    viewMenu->addSeparator();
  }

  m_MenuBar->addMenu(viewMenu);

  // Add Filter Menu
  QMenu* filterMenu = m_Internals->vsWidget->getFilterMenu();
  m_MenuBar->addMenu(filterMenu);

  // Apply Menu Bar
  setMenuBar(m_MenuBar);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QMenu* IMFViewer_UI::createThemeMenu(QActionGroup* actionGroup, QWidget* parent)
{
  SVStyle* style = SVStyle::Instance();

  QMenu* menuThemes = new QMenu("Themes", parent);

  QString themePath = ":/SIMPL/StyleSheets/Default.json";
  QAction* action = menuThemes->addAction("Default", [=] { style->loadStyleSheet(themePath); });
  action->setCheckable(true);
  if(themePath == style->getCurrentThemeFilePath())
  {
    action->setChecked(true);
  }
  actionGroup->addAction(action);

  QStringList themeNames = BrandedStrings::LoadedThemeNames;
  for(int i = 0; i < themeNames.size(); i++)
  {
    QString themePath = BrandedStrings::DefaultStyleDirectory + QDir::separator() + themeNames[i] + ".json";
    QAction* action = menuThemes->addAction(themeNames[i], [=] { style->loadStyleSheet(themePath); });
    action->setCheckable(true);
    if(themePath == style->getCurrentThemeFilePath())
    {
      action->setChecked(true);
    }
    actionGroup->addAction(action);
  }

  return menuThemes;
}
