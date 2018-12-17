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

#include <SIMPLib/Utilities/SIMPLH5DataReader.h>

#include "SIMPLib/FilterParameters/IntVec3FilterParameter.h"
#include "SIMPLib/Filtering/FilterFactory.hpp"
#include "SIMPLib/Filtering/FilterManager.h"
#include "SIMPLib/Filtering/FilterPipeline.h"
#include "SIMPLib/Filtering/QMetaObjectUtilities.h"

#include "SVWidgetsLib/QtSupport/QtSRecentFileList.h"
#include "SVWidgetsLib/QtSupport/QtSSettings.h"

#include "SIMPLVtkLib/Wizards/ImportData/ImportDataWizard.h"
#include "SIMPLVtkLib/Wizards/ImportData/TileConfigFileGenerator.h"

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
  // Connection to update the recent files list on all windows when it changes
  QtSRecentFileList* recentsList = QtSRecentFileList::Instance(5, this);
  connect(recentsList, SIGNAL(fileListChanged(const QString&)), this, SLOT(updateRecentFileList(const QString&)));

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
  QString filePath = QFileDialog::getOpenFileName(this, tr("Select a data file"),
    m_OpenDialogLastDirectory, filter);

  if (filePath.isEmpty())
  {
    return;
  }

  m_OpenDialogLastDirectory = filePath;

  QFileInfo fi(filePath);
  QString ext = fi.completeSuffix();
  QMimeDatabase db;
  QMimeType mimeType = db.mimeTypeForFile(filePath, QMimeDatabase::MatchContent);

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);

  if (ext == "dream3d")
  {
    if (baseWidget->openDREAM3DFile(filePath) == false)
    {
      return;
    }
  }
  else if (ext == "vtk" || ext == "vti" || ext == "vtp" || ext == "vtr" || ext == "vts" || ext == "vtu")
  {
    if (baseWidget->importVTKData(filePath) == false)
    {
      QMessageBox::critical(this, "Unable to Load VTK File",
                            tr("IMF Viewer could not load VTK input file '%1'.  Only files "
                               "with 'vtk', 'vti', 'vtp', 'vtr', 'vts', and 'vtu' extensions are permitted.")
                                .arg(filePath),
                            QMessageBox::StandardButton::Ok);
      return;
    }
  }
  else if (ext == "stl")
  {
    if (baseWidget->importSTLData(filePath) == false)
    {
      QMessageBox::critical(this, "Unable to Load STL File",
                            tr("IMF Viewer could not load STL input file '%1'.  Only files "
                               "with 'stl' extension are permitted.")
                                .arg(filePath),
                            QMessageBox::StandardButton::Ok);
      return;
    }
  }
  else if (mimeType.inherits("image/png") || mimeType.inherits("image/tiff") || mimeType.inherits("image/jpeg") || mimeType.inherits("image/bmp"))
  {
    if (baseWidget->importImageData(filePath) == false)
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
  ImportDataWizard* importDataWizard = new ImportDataWizard(this);
  int result = importDataWizard->exec();

  if (result == QDialog::Accepted)
  {
    ImportDataWizard::InputType inputType = importDataWizard->field("InputType").value<ImportDataWizard::InputType>();
    VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);

    // Based on the type of file imported, perform next action
    if(inputType == ImportDataWizard::InputType::GenericMontage)
    {
      // Instantiate ITK Montage filter
      QString filterName = "ITKMontageFromFilesystem";
      FilterManager* fm = FilterManager::Instance();
      IFilterFactory::Pointer factory = fm->getFactoryFromClassName(filterName);
      DataContainerArray::Pointer dca = DataContainerArray::New();
      AbstractFilter::Pointer itkMontageFilter;
      AbstractFilter::Pointer writer;

      // Run the ITK Montage from Filesystem filter
      if(factory.get() != nullptr)
      {

        itkMontageFilter = factory->create();
        if(itkMontageFilter.get() != nullptr)
        {
          itkMontageFilter->setDataContainerArray(dca);
          // DataContainer::Pointer dc = DataContainer::New("DataContainer");
          // AttributeMatrix::Pointer am = AttributeMatrix::New(QVector<size_t>(1, 53), "CellAttributeMatrix", AttributeMatrix::Type::Cell);
          // dc->addAttributeMatrix("CellAttributeMatrix", am);
          // filter->getDataContainerArray()->addDataContainer(dc);

          QVariant var;
          bool propWasSet = false;

          int numOfRows = importDataWizard->field("numOfRows").toInt();
          int numOfCols = importDataWizard->field("numOfCols").toInt();

          // Set montage size
          IntVec3_t montageSize = {numOfCols, numOfRows, 1};
          var.setValue(montageSize);
          propWasSet = itkMontageFilter->setProperty("MontageSize", var);

          // Get input file names
          FileListInfo_t inputFileInfo = importDataWizard->field("GenericFileListInfo").value<FileListInfo_t>();
          var.setValue(inputFileInfo);
          propWasSet = itkMontageFilter->setProperty("InputFileListInfo", var);

          // Set DataContainerName
          QString dcName = "DataContainer";
          var.setValue(dcName);
          propWasSet = itkMontageFilter->setProperty("DataContainerName", var);

          // Set Cell Attribute Matrix Name
          QString cellAttrMatrixName = "CellAttributeMatrix";
          var.setValue(cellAttrMatrixName);
          propWasSet = itkMontageFilter->setProperty("CellAttributeMatrixName", var);

          // Set Metadata Attribute Matrix Name
          QString metaDataAttributeMatrixName = "MetaDataAttributeMatrix";
          var.setValue(metaDataAttributeMatrixName);
          propWasSet = itkMontageFilter->setProperty("MetaDataAttributeMatrixName", var);

          // itkMontageFilter->execute();

          // qInfo() << "ITK Montage error cond: " << itkMontageFilter->getErrorCondition();

          // Generate tile configuration file.
          TileConfigFileGenerator tileConfigFileGenerator(inputFileInfo,
            importDataWizard->field("montageType").value<MontageSettings::MontageType>(),
            importDataWizard->field("montageOrder").value<MontageSettings::MontageOrder>(),
            numOfCols, numOfRows, "TileConfiguration.txt");
          tileConfigFileGenerator.buildTileConfigFile();
        }
      }

      // For testing purposes only
      QString dream3dWriterName = "DataContainerWriter";
      factory = fm->getFactoryFromClassName(dream3dWriterName);

      if(factory.get() != nullptr)
      {
        writer = factory->create();
        if(writer.get() != nullptr)
        {
          writer->setDataContainerArray(dca);
          QVariant var;
          bool propWasSet = false;

          // Set Output File
          QString outputFile = importDataWizard->field("outputFileName").toString();
          var.setValue(outputFile);
          propWasSet = writer->setProperty("OutputFile", var);

          // Set whether to write Xdmf file
          bool writeXdmf = true;
          var.setValue(writeXdmf);
          propWasSet = writer->setProperty("WriteXdmfFile", var);

          // Set whether to write time series
          bool writeTimeSeries = false;
          var.setValue(writeTimeSeries);
          propWasSet = writer->setProperty("WriteTimeSeries", var);

          // writer->execute();

          // qInfo() << "Data Container Writer error cond: " << writer->getErrorCondition();
        }
      }

      if(itkMontageFilter.get() != nullptr && writer.get() != nullptr)
      {
        FilterPipeline::Pointer pipeline = FilterPipeline::New();
        pipeline->pushBack(itkMontageFilter);
        pipeline->pushBack(writer);
        pipeline->execute();

        int err = pipeline->getErrorCondition();
        qInfo() << "Pipeline err condition: " << err;

        if(err >= 0)
        {
          QStringList filePaths;
          QString dream3dFile = importDataWizard->field("outputFileName").toString();
          if(dream3dFile.isEmpty())
          {
            return;
          }

          SIMPLH5DataReader reader;
          bool success = reader.openFile(dream3dFile);
          if(success)
          {
            connect(&reader, &SIMPLH5DataReader::errorGenerated,
                    [=](const QString& title, const QString& msg, const int& code) { QMessageBox::critical(this, title, msg, QMessageBox::StandardButton::Ok); });

            DataContainerArrayProxy dream3dProxy = importDataWizard->field("DREAM3DProxy").value<DataContainerArrayProxy>();

            DataContainerArray::Pointer dca = reader.readSIMPLDataUsingProxy(dream3dProxy, false);
            if(dca.get() == nullptr)
            {
              return;
            }

            VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);
            baseWidget->importDataContainerArray(dream3dFile, dca);
          }
        }
      }
    }
    else if(inputType == ImportDataWizard::InputType::DREAM3D)
    {
      QString dataFilePath = importDataWizard->field("DataFilePath").toString();

      SIMPLH5DataReader reader;
      bool success = reader.openFile(dataFilePath);
      if(success)
      {
        connect(&reader, &SIMPLH5DataReader::errorGenerated,
                [=](const QString& title, const QString& msg, const int& code) { QMessageBox::critical(this, title, msg, QMessageBox::StandardButton::Ok); });

        DataContainerArrayProxy dream3dProxy = importDataWizard->field("DREAM3DProxy").value<DataContainerArrayProxy>();

        DataContainerArray::Pointer dca = reader.readSIMPLDataUsingProxy(dream3dProxy, false);
        if(dca.get() == nullptr)
        {
          return;
        }

        baseWidget->importDataContainerArray(dataFilePath, dca);
      }
    }
    else if(inputType == ImportDataWizard::InputType::Fiji)
    {

    }
    else if(inputType == ImportDataWizard::InputType::Robomet)
    {

    }
    else
    {
      QString dataFilePath = importDataWizard->field("DataFilePath").toString();
      QFileInfo fi(dataFilePath);
      QString ext = fi.completeSuffix();
      QMessageBox::critical(this, "Invalid File Type",
                            tr("IMF Viewer failed to detect the proper data file type from the given input file with extension '%1'.  "
                               "Supported file types are DREAM3D, VTK, STL and Image files, as well as Fiji and Robomet configuration files.")
                                .arg(ext),
                            QMessageBox::StandardButton::Ok);
    }
  }

  delete importDataWizard;
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
  connect(importDataAction, &QAction::triggered, this, static_cast<void (IMFViewer_UI::*)(void)>(&IMFViewer_UI::importData));
  fileMenu->addAction(importDataAction);

  QAction* importMontageAction = new QAction("Import Montage");
  importMontageAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_M));
  connect(importMontageAction, &QAction::triggered, this, static_cast<void (IMFViewer_UI::*)(void)>(&IMFViewer_UI::importMontage));
  fileMenu->addAction(importMontageAction);

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

  // Add Filter Menu
  QMenu* filterMenu = m_Internals->vsWidget->getFilterMenu();
  m_MenuBar->addMenu(filterMenu);

  // Apply Menu Bar
  setMenuBar(m_MenuBar);
}
