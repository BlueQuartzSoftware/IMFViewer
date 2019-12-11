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

#include <array>

#include <QDesktopServices>
#include <QtConcurrent>
#include <vtkImageData.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QMimeDatabase>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

#include "SIMPLib/FilterParameters/IntVec3FilterParameter.h"
#include "SIMPLib/Filtering/FilterPipeline.h"
#include "SIMPLib/Filtering/QMetaObjectUtilities.h"
#include "SIMPLib/Plugin/PluginManager.h"
#include "SIMPLib/Utilities/SIMPLH5DataReader.h"
#include "SIMPLib/Utilities/SIMPLH5DataReaderRequirements.h"

#include "SVWidgetsLib/QtSupport/QtSRecentFileList.h"
#include "SVWidgetsLib/QtSupport/QtSSettings.h"
#include "SVWidgetsLib/Widgets/SVStyle.h"

#include "SIMPLVtkLib/Common/MontageUtilities.h"
#include "SIMPLVtkLib/Common/SIMPLVtkLibConstants.h"
#include "SIMPLVtkLib/Dialogs/ImportDREAM3DMontageDialog.h"
#include "SIMPLVtkLib/Dialogs/ImportEbsdMontageDialog.h"
#include "SIMPLVtkLib/Dialogs/ImportFijiMontageDialog.h"
#include "SIMPLVtkLib/Dialogs/ImportGenericMontageDialog.h"
#include "SIMPLVtkLib/Dialogs/ImportRobometMontageDialog.h"
#include "SIMPLVtkLib/Dialogs/ImportZeissMontageDialog.h"
#include "SIMPLVtkLib/Dialogs/ImportZeissZenMontageDialog.h"
#include "SIMPLVtkLib/Dialogs/PerformMontageDialog.h"
#include "SIMPLVtkLib/Dialogs/Utilities/TileConfigFileGenerator.h"
#include "SIMPLVtkLib/QtWidgets/VSDatasetImporter.h"
#include "SIMPLVtkLib/QtWidgets/VSFilterFactory.h"
#include "SIMPLVtkLib/QtWidgets/VSMontageImporter.h"
#include "SIMPLVtkLib/QtWidgets/VSQueueModel.h"
#include "SIMPLVtkLib/Visualization/VisualFilters/VSDataSetFilter.h"

#include "SIMPLVtkLib/Dialogs/DatasetListWidget.h"
#include "SIMPLVtkLib/Dialogs/RobometListWidget.h"
#include "SIMPLVtkLib/Dialogs/ZeissListWidget.h"
#include "SIMPLVtkLib/Dialogs/ZeissZenListWidget.h"
#include "SIMPLVtkLib/Visualization/VisualFilters/VSRootFilter.h"
#include "SIMPLVtkLib/Visualization/VisualFilters/VSTransform.h"
#include "SIMPLVtkLib/Wizards/ExecutePipeline/ExecutePipelineConstants.h"
#include "SIMPLVtkLib/Wizards/ExecutePipeline/ExecutePipelineWizard.h"
#include "SIMPLVtkLib/Wizards/ExecutePipeline/PipelineWorker.h"

#include "BrandedStrings.h"

#include "ui_IMFViewer_UI.h"

using Array3Type = std::array<double, 3>;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
class IMFViewer_UI::vsInternals : public Ui::IMFViewer_UI
{
public:
  vsInternals() = default;
};

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewer_UI::IMFViewer_UI(QWidget* parent)
: QMainWindow(parent)
, m_Ui(new vsInternals())
{
  m_Ui->setupUi(this);

  setupGui();

  qRegisterMetaType<FilterPipeline::Pointer>();
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
  connect(recentsList, SIGNAL(fileListChanged(QString)), this, SLOT(updateRecentFileList(QString)));

  VSQueueModel* queueModel = new VSQueueModel(m_Ui->queueWidget);
  m_Ui->queueWidget->setQueueModel(queueModel);
  connect(m_Ui->queueWidget, &VSQueueWidget::notifyStatusMessage, this, &IMFViewer_UI::processStatusMessage);

  createMenu();

  m_Ui->queueDockWidget->hide();
  m_Ui->vsWidget->setFilterView(m_Ui->treeView);
  m_Ui->vsWidget->setInfoWidget(m_Ui->infoWidget);

  readSettings();

  // Set up the save image action for the filter view
  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  VSFilterView* filterView = baseWidget->getFilterView();
  connect(filterView, &VSFilterView::saveFilterRequested, [=] { saveImage(); });
  connect(baseWidget, &VSMainWidgetBase::selectedFiltersChanged, this, &IMFViewer_UI::listenSelectionChanged);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importData()
{
  QString filter = tr("DREAM3D File (*.dream3d);;VTK File (*.vtk *.vti *.vtp *.vtr *.vts *.vtu);;"
                      "STL File (*.stl);;Image File (*.png *.tif *.jpg *.bmp);;All Files (*.*)");
  QStringList filePaths = QFileDialog::getOpenFileNames(this, tr("Select a data file"), m_OpenDialogLastDirectory, filter);

  if(filePaths.isEmpty())
  {
    return;
  }

  m_OpenDialogLastDirectory = filePaths[0];

  QStringList imagePaths;
  for(const QString& filePath : filePaths)
  {
    QFileInfo fi(filePath);
    QString ext = fi.completeSuffix();
    QMimeDatabase db;
    QMimeType mimeType = db.mimeTypeForFile(filePath, QMimeDatabase::MatchContent);

    VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);

    if(ext == "dream3d")
    {
      baseWidget->launchHDF5SelectionDialog(filePath);
    }
    else if(ext == "vtk" || ext == "vti" || ext == "vtp" || ext == "vtr" || ext == "vts" || ext == "vtu")
    {
      importData(filePath);
    }
    else if(ext == "stl")
    {
      importData(filePath);
    }
    else if(mimeType.inherits("image/png") || mimeType.inherits("image/tiff") || mimeType.inherits("image/jpeg") || mimeType.inherits("image/bmp"))
    {
      imagePaths.push_back(filePath);
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

  importImages(imagePaths);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importGenericMontage()
{
  ImportGenericMontageDialog::Pointer dialog = ImportGenericMontageDialog::New(this);
  int ret = dialog->exec();
  if(ret == QDialog::Rejected)
  {
    return;
  }

  GenericMontageMetadata metadata = dialog->getMetadata();

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();
  MontageMetadata::DisplayType displayType = metadata.getDataDisplayType();
  filterViewModel->setDisplayType(displayType);

  QString tileConfigFile = "TileConfiguration.txt";

  IntVec2Type rowLimits = metadata.getRowLimits();
  IntVec2Type colLimits = metadata.getColumnLimits();
  int numOfRows = rowLimits[1] - rowLimits[0] + 1;
  int numOfCols = colLimits[1] - colLimits[0] + 1;

  // Get input file names
  StackFileListInfo inputFileInfo = dialog->getFileListInfo();

  // Generate tile configuration file.
  TileConfigFileGenerator tileConfigFileGenerator(inputFileInfo, metadata.getMontageType(), metadata.getMontageOrder(), numOfCols, numOfRows, metadata.getTileOverlap(), tileConfigFile);
  tileConfigFileGenerator.generateTileConfigFile();

  QString fijiFilePath(inputFileInfo.InputPath);
  fijiFilePath.append(QDir::separator());
  fijiFilePath.append(tileConfigFile);

  // Change wizard data for Fiji use case
  FijiMontageMetadata fijiMetadata;
  fijiMetadata.setMontageName(metadata.getMontageName());
  fijiMetadata.setRowLimits(metadata.getRowLimits());
  fijiMetadata.setColumnLimits(metadata.getColumnLimits());
  fijiMetadata.setDataDisplayType(displayType);
  fijiMetadata.setConfigFilePath(fijiFilePath);
  fijiMetadata.setChangeOrigin(true);
  fijiMetadata.setOrigin(metadata.getOrigin());
  fijiMetadata.setLengthUnitsIdx(metadata.getLengthUnitsIdx());

  importFijiMontage(fijiMetadata);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importDREAM3DMontage()
{
  ImportDREAM3DMontageDialog::Pointer dialog = ImportDREAM3DMontageDialog::New(this);
  int ret = dialog->exec();
  if(ret == QDialog::Rejected)
  {
    return;
  }

  DREAM3DMontageMetadata metadata = dialog->getMetadata();

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();
  MontageMetadata::DisplayType displayType = metadata.getDataDisplayType();
  filterViewModel->setDisplayType(displayType);

  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();

  FilterPipeline::Pointer pipeline = FilterPipeline::New();
  QString montageName = metadata.getMontageName();
  pipeline->setName(montageName);

  IntVec2Type rowLimits = metadata.getRowLimits();
  IntVec2Type colLimits = metadata.getColumnLimits();
  QString dcPrefix = metadata.getDataContainerPrefix();

  QStringList dcNames;
  for(int32_t row = rowLimits[1]; row <= rowLimits[1]; row++)
  {
    for(int32_t col = colLimits[0]; col <= colLimits[0]; col++)
    {
      dcNames.push_back(MontageUtilities::GenerateDataContainerName(dcPrefix, {colLimits[1], rowLimits[1]}, row, col));
    }
  }

  SIMPLH5DataReader reader;
  connect(&reader, &SIMPLH5DataReader::errorGenerated,
          [=](const QString& title, const QString& msg) { QMessageBox::critical(this, title, msg, QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok); });

  QString dataFilePath = metadata.getFilePath();
  DataContainerArrayProxy dream3dProxy = MontageUtilities::CreateMontageProxy(reader, dataFilePath, dcNames);
  if(dream3dProxy == DataContainerArrayProxy())
  {
    return;
  }

  AbstractFilter::Pointer dataContainerReader = filterFactory->createDataContainerReaderFilter(dataFilePath, dream3dProxy);
  if(dataContainerReader == AbstractFilter::NullPointer())
  {
    // Error!
    return;
  }

  pipeline->pushBack(dataContainerReader);

  QString amName = metadata.getAttributeMatrixName();
  QString daName = metadata.getDataArrayName();

  if(displayType != MontageMetadata::DisplayType::SideBySide && displayType != MontageMetadata::DisplayType::Outline)
  {
    AbstractFilter::Pointer itkRegistrationFilter = filterFactory->createPCMTileRegistrationFilter(rowLimits, colLimits, dcPrefix, amName, daName);
    if(itkRegistrationFilter == AbstractFilter::NullPointer())
    {
      // Error!
      return;
    }
    pipeline->pushBack(itkRegistrationFilter);

    DataArrayPath montagePath("MontageDC", "MontageAM", "MontageData");
    AbstractFilter::Pointer itkStitchingFilter = filterFactory->createTileStitchingFilter(rowLimits, colLimits, dcPrefix, amName, daName, montagePath);
    if(itkStitchingFilter == AbstractFilter::NullPointer())
    {
      // Error!
      return;
    }
    pipeline->pushBack(itkStitchingFilter);
  }

  VSMontageImporter::Pointer importer = VSMontageImporter::New(pipeline, displayType);
  addMontageImporterToQueue(importer);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importFijiMontage()
{
  ImportFijiMontageDialog::Pointer dialog = ImportFijiMontageDialog::New(this);
  int ret = dialog->exec();
  if(ret == QDialog::Rejected)
  {
    return;
  }

  FijiMontageMetadata metadata = dialog->getMetadata();

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();
  MontageMetadata::DisplayType displayType = metadata.getDataDisplayType();
  filterViewModel->setDisplayType(displayType);

  importFijiMontage(metadata);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importFijiMontage(const FijiMontageMetadata& metadata)
{
  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();

  FilterPipeline::Pointer pipeline = FilterPipeline::New();
  pipeline->setName(metadata.getMontageName());

  DataArrayPath dcPath("UntitledMontage_", "", "");
  QString amName = "Cell Attribute Matrix";
  QString daName = "Image Data";
  AbstractFilter::Pointer importFijiMontageFilter = filterFactory->createImportFijiMontageFilter(metadata, dcPath, amName, daName);
  if(importFijiMontageFilter == AbstractFilter::NullPointer())
  {
    // Error!
    return;
  }

  pipeline->pushBack(importFijiMontageFilter);

  // Set Image Data Containers
  importFijiMontageFilter->preflight();
  QStringList dcNames;
  QString dcPrefix = dcPath.getDataContainerName() + "_";

  int rowCount;
  int colCount;
  IntVec2Type rowLimits = metadata.getRowLimits();
  IntVec2Type colLimits = metadata.getColumnLimits();

  if(colLimits.getY() == 0 && rowLimits.getY() == 0)
  {
    rowCount = importFijiMontageFilter->property("RowCount").toInt();
    colCount = importFijiMontageFilter->property("ColumnCount").toInt();
    DataContainerArray::Pointer dca = importFijiMontageFilter->getDataContainerArray();
    dcNames = dca->getDataContainerNames();
  }
  else
  {
    rowCount = rowLimits[1] - rowLimits[0] + 1;
    colCount = colLimits[1] - colLimits[0] + 1;
    for(int32_t row = rowLimits[0]; row <= rowLimits[1]; row++)
    {
      for(int32_t col = colLimits[0]; col <= colLimits[1]; col++)
      {
        dcNames.push_back(MontageUtilities::GenerateDataContainerName(dcPrefix, {colLimits[1], rowLimits[1]}, row, col));
      }
    }
  }

  IntVec3Type montageSize = {colCount, rowCount, 1};
  MontageMetadata::DisplayType displayType = metadata.getDataDisplayType();

  if(displayType != MontageMetadata::DisplayType::SideBySide && displayType != MontageMetadata::DisplayType::Outline)
  {
    AbstractFilter::Pointer itkRegistrationFilter = filterFactory->createPCMTileRegistrationFilter(rowLimits, colLimits, dcPrefix, amName, daName);
    if(itkRegistrationFilter == AbstractFilter::NullPointer())
    {
      // Error!
      return;
    }
    pipeline->pushBack(itkRegistrationFilter);

    DataArrayPath montagePath("MontageDC", "MontageAM", "MontageData");
    AbstractFilter::Pointer itkStitchingFilter = filterFactory->createTileStitchingFilter(rowLimits, colLimits, dcPrefix, amName, daName, montagePath);
    if(itkStitchingFilter == AbstractFilter::NullPointer())
    {
      // Error!
      return;
    }
    pipeline->pushBack(itkStitchingFilter);
  }

  // Run the pipeline
  VSMontageImporter::Pointer importer = VSMontageImporter::New(pipeline, displayType);
  addMontageImporterToQueue(importer);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importRobometMontage()
{
  ImportRobometMontageDialog::Pointer dialog = ImportRobometMontageDialog::New(this);
  int ret = dialog->exec();
  if(ret == QDialog::Rejected)
  {
    return;
  }

  RobometMontageMetadata metadata = dialog->getMetadata();

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();
  MontageMetadata::DisplayType displayType = metadata.getDataDisplayType();
  filterViewModel->setDisplayType(displayType);

  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();

  IntVec2Type sliceLimits = metadata.getSliceLimits();

  for(int slice = sliceLimits[0]; slice <= sliceLimits[1]; slice++)
  {
    FilterPipeline::Pointer pipeline = FilterPipeline::New();
    QString montageName = metadata.getMontageName();
    QString pipelineName = montageName;
    pipelineName.append(tr("_%1").arg(slice));
    pipeline->setName(pipelineName);

    DataArrayPath dcPath("UntitledMontage_", "", "");
    QString amName = "Cell Attribute Matrix";
    QString daName = "Image Data";

    AbstractFilter::Pointer importRoboMetMontageFilter = filterFactory->createImportRobometMontageFilter(metadata, dcPath, amName, daName, slice);
    if(importRoboMetMontageFilter == AbstractFilter::NullPointer())
    {
      // Error!
      return;
    }

    pipeline->pushBack(importRoboMetMontageFilter);

    importRoboMetMontageFilter->preflight();
    DataContainerArray::Pointer dca = importRoboMetMontageFilter->getDataContainerArray();
    QStringList dcNames = dca->getDataContainerNames();

    if(displayType != MontageMetadata::DisplayType::SideBySide && displayType != MontageMetadata::DisplayType::Outline)
    {
      IntVec2Type rowLimits = metadata.getRowLimits();
      IntVec2Type colLimits = metadata.getColumnLimits();

      QString dcPrefix = dcPath.getDataContainerName() + "_";
      AbstractFilter::Pointer itkRegistrationFilter = filterFactory->createPCMTileRegistrationFilter(rowLimits, colLimits, dcPrefix, amName, daName);
      if(itkRegistrationFilter == AbstractFilter::NullPointer())
      {
        // Error!
        return;
      }
      pipeline->pushBack(itkRegistrationFilter);

      DataArrayPath montagePath("MontageDC", "MontageAM", "MontageData");
      AbstractFilter::Pointer itkStitchingFilter = filterFactory->createTileStitchingFilter(rowLimits, colLimits, dcPrefix, amName, daName, montagePath);
      if(itkStitchingFilter == AbstractFilter::NullPointer())
      {
        // Error!
        return;
      }
      pipeline->pushBack(itkStitchingFilter);
    }

    VSMontageImporter::Pointer importer = VSMontageImporter::New(pipeline, displayType);
    addMontageImporterToQueue(importer);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importZeissMontage()
{
  ImportZeissMontageDialog::Pointer dialog = ImportZeissMontageDialog::New(this);
  int ret = dialog->exec();
  if(ret == QDialog::Rejected)
  {
    return;
  }

  ZeissMontageMetadata metadata = dialog->getMetadata();

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();
  MontageMetadata::DisplayType displayType = metadata.getDataDisplayType();
  filterViewModel->setDisplayType(displayType);

  FilterPipeline::Pointer pipeline = FilterPipeline::New();
  AbstractFilter::Pointer importZeissMontage;
  QString montageName = metadata.getMontageName();
  pipeline->setName(montageName);

  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();

  ZeissListInfo_t zeissListInfo = dialog->getZeissListInfo();

  QString configFilePath = zeissListInfo.ZeissFilePath;
  QString dcPrefix = "UntitledMontage_";
  DataArrayPath dcPath("UntitledMontage", "", "");
  QString amName = "Cell Attribute Matrix";
  QString daName = "Image Data";
  QString metadataAMName = "Metadata Attribute Matrix";
  bool importAllMetadata = true;

  importZeissMontage = filterFactory->createImportZeissMontageFilter(metadata, dcPath, amName, daName, metadataAMName, importAllMetadata);
  if(importZeissMontage == AbstractFilter::NullPointer())
  {
    // Error!
    return;
  }

  pipeline->pushBack(importZeissMontage);

  // Set Image Data Containers
  importZeissMontage->preflight();
  QStringList dcNames;

  int rowCount;
  int colCount;
  IntVec2Type rowLimits = metadata.getRowLimits();
  IntVec2Type colLimits = metadata.getColumnLimits();

  if(colLimits.getY() == 0 && rowLimits.getY() == 0)
  {
    rowCount = importZeissMontage->property("RowCount").toInt();
    colCount = importZeissMontage->property("ColumnCount").toInt();
    DataContainerArray::Pointer dca = importZeissMontage->getDataContainerArray();
    dcNames = dca->getDataContainerNames();
  }
  else
  {
    rowCount = rowLimits[1] - rowLimits[0] + 1;
    colCount = colLimits[1] - colLimits[0] + 1;
    for(int32_t row = rowLimits[0]; row <= rowLimits[1]; row++)
    {
      for(int32_t col = colLimits[0]; col <= colLimits[1]; col++)
      {
        dcNames.push_back(MontageUtilities::GenerateDataContainerName(dcPrefix, {colLimits[1], rowLimits[1]}, row, col));
      }
    }
  }

  IntVec3Type montageSize = {colCount, rowCount, 1};

  if(displayType != MontageMetadata::DisplayType::SideBySide && displayType != MontageMetadata::DisplayType::Outline)
  {
    AbstractFilter::Pointer itkRegistrationFilter = filterFactory->createPCMTileRegistrationFilter(rowLimits, colLimits, dcPrefix, amName, daName);
    if(itkRegistrationFilter == AbstractFilter::NullPointer())
    {
      // Error!
      return;
    }
    pipeline->pushBack(itkRegistrationFilter);

    DataArrayPath montagePath("MontageDC", "MontageAM", "MontageData");
    AbstractFilter::Pointer itkStitchingFilter = filterFactory->createTileStitchingFilter(rowLimits, colLimits, dcPrefix, amName, daName, montagePath);
    if(itkStitchingFilter == AbstractFilter::NullPointer())
    {
      // Error!
      return;
    }
    pipeline->pushBack(itkStitchingFilter);
  }

  VSMontageImporter::Pointer importer = VSMontageImporter::New(pipeline, displayType);
  addMontageImporterToQueue(importer);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importZeissZenMontage()
{
  ImportZeissZenMontageDialog::Pointer dialog = ImportZeissZenMontageDialog::New(this);
  int ret = dialog->exec();
  if(ret == QDialog::Rejected)
  {
    return;
  }

  ZeissZenMontageMetadata metadata = dialog->getMetadata();

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();
  MontageMetadata::DisplayType displayType = metadata.getDataDisplayType();
  filterViewModel->setDisplayType(displayType);

  FilterPipeline::Pointer pipeline = FilterPipeline::New();
  AbstractFilter::Pointer importZeissMontage;
  QString montageName = metadata.getMontageName();
  pipeline->setName(montageName);

  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();

  ZeissZenListInfo_t zeissListInfo = dialog->getZeissZenListInfo();

  QString configFilePath = zeissListInfo.ZeissFilePath;
  QString dcPrefix = "UntitledMontage_";
  DataArrayPath dcPath("UntitledMontage", "", "");
  QString amName = "Cell Attribute Matrix";
  QString daName = "Image Data";

  importZeissMontage = filterFactory->createImportZeissZenMontageFilter(metadata, dcPath, amName, daName);
  if(importZeissMontage == AbstractFilter::NullPointer())
  {
    // Error!
    return;
  }

  pipeline->pushBack(importZeissMontage);

  // Set Image Data Containers
  importZeissMontage->preflight();
  QStringList dcNames;

  int rowCount;
  int colCount;
  IntVec2Type rowLimits = metadata.getRowLimits();
  IntVec2Type colLimits = metadata.getColumnLimits();

  if(colLimits.getY() == 0 && rowLimits.getY() == 0)
  {
    rowCount = importZeissMontage->property("RowCount").toInt();
    colCount = importZeissMontage->property("ColumnCount").toInt();
    DataContainerArray::Pointer dca = importZeissMontage->getDataContainerArray();
    dcNames = dca->getDataContainerNames();
  }
  else
  {
    rowCount = rowLimits[1] - rowLimits[0] + 1;
    colCount = colLimits[1] - colLimits[0] + 1;
    for(int32_t row = rowLimits[0]; row <= rowLimits[1]; row++)
    {
      for(int32_t col = colLimits[0]; col <= colLimits[1]; col++)
      {
        dcNames.push_back(MontageUtilities::GenerateDataContainerName(dcPrefix, {colLimits[1], rowLimits[1]}, row, col));
      }
    }
  }

  IntVec3Type montageSize = {colCount, rowCount, 1};

  if(displayType != MontageMetadata::DisplayType::SideBySide && displayType != MontageMetadata::DisplayType::Outline)
  {
    AbstractFilter::Pointer itkRegistrationFilter = filterFactory->createPCMTileRegistrationFilter(rowLimits, colLimits, dcPrefix, amName, daName);
    if(itkRegistrationFilter == AbstractFilter::NullPointer())
    {
      // Error!
      return;
    }
    pipeline->pushBack(itkRegistrationFilter);

    DataArrayPath montagePath("MontageDC", "MontageAM", "MontageData");
    AbstractFilter::Pointer itkStitchingFilter = filterFactory->createTileStitchingFilter(rowLimits, colLimits, dcPrefix, amName, daName, montagePath);
    if(itkStitchingFilter == AbstractFilter::NullPointer())
    {
      // Error!
      return;
    }
    pipeline->pushBack(itkStitchingFilter);
  }

  VSMontageImporter::Pointer importer = VSMontageImporter::New(pipeline, displayType);
  addMontageImporterToQueue(importer);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importEbsdMontage()
{
  ImportEbsdMontageDialog::Pointer dialog = ImportEbsdMontageDialog::New(this);
  int ret = dialog->exec();
  if(ret == QDialog::Rejected)
  {
    return;
  }

  EbsdMontageMetadata metadata = dialog->getMetadata();

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();
  MontageMetadata::DisplayType displayType = metadata.getDataDisplayType();
  filterViewModel->setDisplayType(displayType);

  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();

  FilterPipeline::Pointer pipeline = FilterPipeline::New();
  pipeline->setName(metadata.getMontageName());

  DataArrayPath dcPath("UntitledMontage_", "", "");
  QString amName = "Cell Attribute Matrix";
  QString ensembleAMName = "Cell Ensemble Attribute Matrix";
  QString daName = "Image Data";
  QString ipfColorsName = "IPF Colors";
  AbstractFilter::Pointer importEbsdMontageFilter = filterFactory->createImportEbsdMontageFilter(metadata, dcPath, amName, ensembleAMName, daName, ipfColorsName);
  if(importEbsdMontageFilter == AbstractFilter::NullPointer())
  {
    // Error!
    return;
  }

  pipeline->pushBack(importEbsdMontageFilter);

  //  // Set Image Data Containers
  //  importEbsdMontageFilter->preflight();
  //  QStringList dcNames;
  //  QString dcPrefix = dcPath.getDataContainerName() + "_";

  //  int rowCount;
  //  int colCount;
  //  IntVec2Type rowLimits = metadata.getRowLimits();
  //  IntVec2Type colLimits = metadata.getColumnLimits();

  //  if(colLimits.getY() == 0 && rowLimits.getY() == 0)
  //  {
  //    rowCount = importEbsdMontageFilter->property("RowCount").toInt();
  //    colCount = importEbsdMontageFilter->property("ColumnCount").toInt();
  //    DataContainerArray::Pointer dca = importEbsdMontageFilter->getDataContainerArray();
  //    dcNames = dca->getDataContainerNames();
  //  }
  //  else
  //  {
  //    rowCount = rowLimits[1] - rowLimits[0] + 1;
  //    colCount = colLimits[1] - colLimits[0] + 1;
  //    for(int32_t row = rowLimits[0]; row <= rowLimits[1]; row++)
  //    {
  //      for(int32_t col = colLimits[0]; col <= colLimits[1]; col++)
  //      {
  //        dcNames.push_back(MontageUtilities::GenerateDataContainerName(dcPrefix, {colLimits[1], rowLimits[1]}, row, col));
  //      }
  //    }
  //  }

  //  IntVec3Type montageSize = {colCount, rowCount, 1};

  //  if(displayType != MontageMetadata::DisplayType::SideBySide && displayType != MontageMetadata::DisplayType::Outline)
  //  {
  //    AbstractFilter::Pointer itkRegistrationFilter = filterFactory->createPCMTileRegistrationFilter(rowLimits, colLimits, dcPrefix, amName, daName);
  //    if(itkRegistrationFilter == AbstractFilter::NullPointer())
  //    {
  //      // Error!
  //      return;
  //    }
  //    pipeline->pushBack(itkRegistrationFilter);

  //    DataArrayPath montagePath("MontageDC", "MontageAM", "MontageData");
  //    AbstractFilter::Pointer itkStitchingFilter = filterFactory->createTileStitchingFilter(rowLimits, colLimits, dcPrefix, amName, daName, montagePath);
  //    if(itkStitchingFilter == AbstractFilter::NullPointer())
  //    {
  //      // Error!
  //      return;
  //    }
  //    pipeline->pushBack(itkStitchingFilter);
  //  }

  // Run the pipeline
  VSMontageImporter::Pointer importer = VSMontageImporter::New(pipeline, metadata.getDataDisplayType());
  addMontageImporterToQueue(importer);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::addMontageImporterToQueue(VSMontageImporter::Pointer importer)
{
  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  connect(importer.get(), &VSMontageImporter::montageResultsReady, baseWidget, &VSMainWidgetBase::importPipelineOutput);

  m_Ui->queueWidget->addDataImporter(importer->getName(), importer);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::handleDatasetResults(VSFileNameFilter* textFilter, VSDataSetFilter* filter)
{
  // Check if any data was imported
  if(filter->getOutput() != nullptr)
  {
    VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
    VSController* controller = baseWidget->getController();
    VSFilterModel* filterModel = controller->getFilterModel();
    filterModel->addFilter(textFilter, false);
    filterModel->addFilter(filter);

    emit controller->dataImported();
  }
  else
  {
    textFilter->deleteLater();
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importData(const QString& filePath)
{
  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  VSFilterModel* filterModel = baseWidget->getController()->getFilterModel();
  VSRootFilter* rootFilter = filterModel->getRootFilter();
  VSFileNameFilter* textFilter = new VSFileNameFilter(filePath);
  textFilter->setParentFilter(dynamic_cast<VSAbstractFilter*>(rootFilter));
  VSDataSetFilter* filter = new VSDataSetFilter(filePath, textFilter);
  VSDatasetImporter::Pointer importer = VSDatasetImporter::New(textFilter, filter);
  connect(importer.get(), &VSDatasetImporter::resultReady, this, &IMFViewer_UI::handleDatasetResults);

  QFileInfo fi(filePath);
  m_Ui->queueWidget->addDataImporter(fi.fileName(), importer);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importImages(const QStringList& filePaths)
{
  if(filePaths.empty())
  {
    return;
  }

  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();

  FilterPipeline::Pointer pipeline = FilterPipeline::New();
  QFileInfo fi(filePaths[0]);
  pipeline->setName(fi.dir().dirName());

  for(int i = 0; i < filePaths.size(); i++)
  {
    QString filePath = filePaths[i];
    fi.setFile(filePath);
    DataArrayPath dcPath(fi.fileName(), "", "");
    AbstractFilter::Pointer imageReaderFilter = filterFactory->createImageFileReaderFilter(filePath, dcPath);
    if(!imageReaderFilter)
    {
      // Error!
      continue;
    }

    pipeline->pushBack(imageReaderFilter);
  }

  // Run the pipeline
  VSMontageImporter::Pointer importer = VSMontageImporter::New(pipeline, MontageMetadata::DisplayType::SideBySide);
  addMontageImporterToQueue(importer);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::executePipeline()
{
  ExecutePipelineWizard* executePipelineWizard = new ExecutePipelineWizard(m_Ui->vsWidget);
  int result = executePipelineWizard->exec();

  if(result == QDialog::Accepted)
  {
    importPipeline(executePipelineWizard);
  }

  delete executePipelineWizard;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importPipeline(ExecutePipelineWizard* executePipelineWizard)
{
  bool displayMontage = executePipelineWizard->field(ExecutePipeline::FieldNames::DisplayMontage).toBool();
  bool displayOutline = executePipelineWizard->field(ExecutePipeline::FieldNames::DisplayOutlineOnly).toBool();

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();
  MontageMetadata::DisplayType displayType;
  if(displayMontage)
  {
    displayType = MontageMetadata::DisplayType::Montage;
  }
  else if(displayOutline)
  {
    displayType = MontageMetadata::DisplayType::Outline;
  }
  else
  {
    displayType = MontageMetadata::DisplayType::SideBySide;
  }

  filterViewModel->setDisplayType(displayType);

  QString filePath = executePipelineWizard->field(ExecutePipeline::FieldNames::PipelineFile).toString();
  ExecutePipelineWizard::ExecutionType executionType = executePipelineWizard->field(ExecutePipeline::FieldNames::ExecutionType).value<ExecutePipelineWizard::ExecutionType>();

  if(filePath.isEmpty())
  {
    return;
  }

  QFileInfo fi(filePath);

  QString jsonContent;
  QFile jsonFile;
  jsonFile.setFileName(fi.absoluteFilePath());
  jsonFile.open(QIODevice::ReadOnly | QIODevice::Text);
  jsonContent = jsonFile.readAll();
  jsonFile.close();
  QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonContent.toUtf8());
  QJsonObject jsonObj = jsonDoc.object();
  FilterPipeline::Pointer pipelineFromJson = FilterPipeline::FromJson(jsonObj);

  if(pipelineFromJson != FilterPipeline::NullPointer())
  {
    if(executionType == ExecutePipelineWizard::ExecutionType::FromFilesystem)
    {
      VSMontageImporter::Pointer importer = VSMontageImporter::New(pipelineFromJson, displayType);
      addMontageImporterToQueue(importer);
    }
    else if(executionType == ExecutePipelineWizard::ExecutionType::OnLoadedData)
    {
      int startFilter = executePipelineWizard->field(ExecutePipeline::FieldNames::StartFilter).toInt();
      int selectedDataset = executePipelineWizard->field(ExecutePipeline::FieldNames::SelectedDataset).toInt();

      // Construct Data Container Array with selected Dataset
      DataContainerArray::Pointer dca = DataContainerArray::New();
      VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
      VSController* controller = baseWidget->getController();
      VSAbstractFilter::FilterListType datasets = controller->getBaseFilters();
      int i = 0;
      for(VSAbstractFilter* dataset : datasets)
      {
        if(i == selectedDataset)
        {
          // Add contents to data container array
          VSAbstractFilter::FilterListType children = dataset->getChildren();
          for(VSAbstractFilter* childFilter : children)
          {
            bool isSIMPL = (dynamic_cast<VSSIMPLDataContainerFilter*>(childFilter) != nullptr);
            if(isSIMPL)
            {
              VSSIMPLDataContainerFilter* dcFilter = dynamic_cast<VSSIMPLDataContainerFilter*>(childFilter);
              if(dcFilter != nullptr)
              {
                DataContainer::Pointer dataContainer = dcFilter->getWrappedDataContainer()->m_DataContainer;
                dca->addOrReplaceDataContainer(dataContainer);
              }
            }
          }
          break;
        }
        i++;
      }
      // Change origin and/or spacing if specified
      FilterPipeline::Pointer pipeline = FilterPipeline::New();
      VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();
      QStringList dcNames = dca->getDataContainerNames();

      bool changeSpacing = executePipelineWizard->field(ExecutePipeline::FieldNames::ChangeSpacing).toBool();
      bool changeOrigin = executePipelineWizard->field(ExecutePipeline::FieldNames::ChangeOrigin).toBool();
      if(changeSpacing || changeOrigin)
      {
        float spacingX = executePipelineWizard->field(ExecutePipeline::FieldNames::SpacingX).toFloat();
        float spacingY = executePipelineWizard->field(ExecutePipeline::FieldNames::SpacingY).toFloat();
        float spacingZ = executePipelineWizard->field(ExecutePipeline::FieldNames::SpacingZ).toFloat();
        FloatVec3Type newSpacing = {spacingX, spacingY, spacingZ};
        float originX = executePipelineWizard->field(ExecutePipeline::FieldNames::OriginX).toFloat();
        float originY = executePipelineWizard->field(ExecutePipeline::FieldNames::OriginY).toFloat();
        float originZ = executePipelineWizard->field(ExecutePipeline::FieldNames::OriginZ).toFloat();
        FloatVec3Type newOrigin = {originX, originY, originZ};

        // For each data container, add a new filter
        for(const QString& dcName : dcNames)
        {
          DataArrayPath dcPath(dcName, "", "");
          AbstractFilter::Pointer setOriginResolutionFilter = filterFactory->createSetOriginResolutionFilter(dcPath, changeSpacing, changeOrigin, newSpacing, newOrigin);

          if(!setOriginResolutionFilter)
          {
            // Error!
          }

          pipeline->pushBack(setOriginResolutionFilter);
        }
      }

      // Reconstruct pipeline starting with new start filter
      for(int i = startFilter; i < pipelineFromJson->getFilterContainer().size(); i++)
      {
        AbstractFilter::Pointer filter = pipelineFromJson->getFilterContainer().at(i);
        filter->setDataContainerArray(dca);
        pipeline->pushBack(filter);
      }

      VSMontageImporter::Pointer importer = VSMontageImporter::New(pipeline, displayType);
      addMontageImporterToQueue(importer);
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::performMontage()
{
  PerformMontageDialog::Pointer performMontageDialog = PerformMontageDialog::New(m_Ui->vsWidget);
  int result = performMontageDialog->exec();

  if(result == QDialog::Rejected)
  {
    return;
  }

  FilterPipeline::Pointer pipeline = FilterPipeline::New();
  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();
  DataContainerArray::Pointer dca = DataContainerArray::New();
  VSAbstractFilter::FilterListType montageDatasets;
  VSAbstractFilter::FilterListType filenameFilters;
  std::pair<int, int> rowColPair;
  bool validSIMPL = false;

  MontageMetadata::DisplayType displayType = MontageMetadata::DisplayType::Montage;
  bool stitchingOnly = performMontageDialog->getStitchingOnly();

  QString montageName = performMontageDialog->getMontageName();
  pipeline->setName(montageName);

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);

  VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();

  filterViewModel->setDisplayType(displayType);

  QStringList selectedFilterNames;
  VSAbstractFilter::FilterListType selectedFilters = baseWidget->getActiveViewWidget()->getSelectedFilters();
  if(selectedFilters.size() < 2)
  {
    return;
  }
  QString amName;
  QString daName;

  for(VSAbstractFilter* selectedFilter : selectedFilters)
  {
    // Add contents to data container array
    bool isSIMPL = (dynamic_cast<VSSIMPLDataContainerFilter*>(selectedFilter) != nullptr);
    if(isSIMPL)
    {
      VSSIMPLDataContainerFilter* dcFilter = dynamic_cast<VSSIMPLDataContainerFilter*>(selectedFilter);
      if(dcFilter != nullptr)
      {
        validSIMPL = true;
        DataContainer::Pointer dataContainer = dcFilter->getWrappedDataContainer()->m_DataContainer;
        if(dataContainer != DataContainer::NullPointer())
        {
          for(const AttributeMatrix::Pointer& am : dataContainer->getAttributeMatrices())
          {
            std::vector<size_t> tupleDims = am->getTupleDimensions();
            if(tupleDims.size() >= 2)
            {
              amName = am->getName();
              daName = am->getAttributeArrayNames().first();
              break;
            }
          }
        }
        montageDatasets.push_back(selectedFilter);
      }
    }
  }

  if(validSIMPL)
  {
    // Build the data container array
    rowColPair = buildCustomDCA(dca, montageDatasets);

    QStringList dcNames = dca->getDataContainerNames();

    IntVec2Type montageSize = {rowColPair.second, rowColPair.first};
    IntVec2Type montageStart = {0, 0};
    IntVec2Type montageEnd = {montageSize[0] - 1, montageSize[1] - 1};

    QString dcPrefix = MontageUtilities::FindDataContainerPrefix(dcNames);

    if(!stitchingOnly)
    {
      AbstractFilter::Pointer itkRegistrationFilter = filterFactory->createPCMTileRegistrationFilter(montageStart, montageEnd, dcPrefix, amName, daName);
      pipeline->pushBack(itkRegistrationFilter);
    }

    DataArrayPath montagePath("MontageDC", "MontageAM", "MontageData");
    AbstractFilter::Pointer itkStitchingFilter = filterFactory->createTileStitchingFilter(montageStart, montageEnd, dcPrefix, amName, daName, montagePath);
    pipeline->pushBack(itkStitchingFilter);

    // Check if output to file was requested
    bool saveToFile = performMontageDialog->getSaveToFile();
    if(saveToFile)
    {
      QString outputFilePath = performMontageDialog->getOutputPath();
      AbstractFilter::Pointer itkImageWriterFilter = filterFactory->createImageFileWriterFilter(outputFilePath, montagePath);
      pipeline->pushBack(itkImageWriterFilter);
    }

    VSMontageImporter::Pointer importer = VSMontageImporter::New(pipeline, displayType);
    addMontageImporterToQueue(importer);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::processStatusMessage(const QString& statusMessage)
{
  statusBar()->showMessage(statusMessage);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::updateRecentFileList(const QString& file)
{
  //  // Clear the Recent Items Menu
  //  m_RecentFilesMenu->clear();

  //  // Get the list from the static object
  //  QStringList files = QtSRecentFileList::Instance()->fileList();
  //  foreach(QString file, files)
  //  {
  //    QAction* action = m_RecentFilesMenu->addAction(QtSRecentFileList::Instance()->parentAndFileName(file));
  //    action->setData(file);
  //    action->setVisible(true);
  //    connect(action, SIGNAL(triggered()), this, SLOT(openRecentFile()));
  //  }

  //  m_RecentFilesMenu->addSeparator();

  //  m_RecentFilesMenu->addAction(m_ClearRecentsAction);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::openRecentFile()
{
  QAction* action = qobject_cast<QAction*>(sender());

  if(action != nullptr)
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

  VSController* controller = m_Ui->vsWidget->getController();
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
void IMFViewer_UI::saveImage()
{
  QString filter = tr("Image File (*.png *.tiff *.jpeg *.bmp)");
  QString filePath = QFileDialog::getSaveFileName(this, "Save As Image File", m_OpenDialogLastDirectory, filter);
  if(filePath.isEmpty())
  {
    return;
  }

  m_OpenDialogLastDirectory = filePath;
  VSController* controller = m_Ui->vsWidget->getController();
  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  //  VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();
  VSAbstractFilter::FilterListType selectedFilters = baseWidget->getActiveViewWidget()->getSelectedFilters();

  bool success = controller->saveAsImage(filePath, selectedFilters.front());

  if(success)
  {
    // Add file to the recent files list
    QtSRecentFileList* list = QtSRecentFileList::Instance();
    list->addFile(filePath);

    // Open the image in the default application for the system
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
  }
  else
  {
    QMessageBox::critical(this, "Invalid Filter Type", tr("The filter must be a data container filter."), QMessageBox::StandardButton::Ok);
    return;
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::saveDream3d()
{
  VSController* controller = m_Ui->vsWidget->getController();
  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Ui->vsWidget);
  VSAbstractFilter::FilterListType selectedFilters = baseWidget->getActiveViewWidget()->getSelectedFilters();
  if(selectedFilters.empty())
  {
    QMessageBox::critical(this, "No Filters Selected", tr("A filter must be selected."), QMessageBox::StandardButton::Ok);
    return;
  }

  QString filter = tr("DREAM3D File (*.dream3d)");
  QString filePath = QFileDialog::getSaveFileName(this, "Save As DREAM3D File", m_OpenDialogLastDirectory, filter);
  if(filePath.isEmpty())
  {
    return;
  }

  m_OpenDialogLastDirectory = filePath;

  bool success = controller->saveAsDREAM3D(filePath, selectedFilters.front());

  if(success)
  {
    // Add file to the recent files list
    QtSRecentFileList* list = QtSRecentFileList::Instance();
    list->addFile(filePath);
  }
  else
  {
    QMessageBox::critical(this, "Invalid Filter Type", tr("The filter must be a data container or pipeline filter."), QMessageBox::StandardButton::Ok);
    return;
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
  VSController* controller = m_Ui->vsWidget->getController();
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

  prefs->beginGroup(SIMPLVtkLib::DockWidgetSettings::GroupName);

  prefs->beginGroup(SIMPLVtkLib::DockWidgetSettings::ImportQueueGroupName);
  readDockWidgetSettings(prefs.data(), m_Ui->queueDockWidget);
  prefs->endGroup();

  prefs->endGroup();

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
  m_Ui->splitter->restoreGeometry(splitterGeometry);
  QByteArray splitterSizes = prefs->value("Splitter_Sizes", QByteArray());
  m_Ui->splitter->restoreState(splitterSizes);

  prefs->endGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::readDockWidgetSettings(QtSSettings* prefs, QDockWidget* dw)
{
  restoreDockWidget(dw);

  bool b = prefs->value(dw->objectName(), QVariant(false)).toBool();
  dw->setHidden(b);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::writeSettings()
{
  QSharedPointer<QtSSettings> prefs = QSharedPointer<QtSSettings>(new QtSSettings());

  // Write the window settings to the prefs file
  writeWindowSettings(prefs.data());

  prefs->beginGroup(SIMPLVtkLib::DockWidgetSettings::GroupName);

  prefs->beginGroup(SIMPLVtkLib::DockWidgetSettings::ImportQueueGroupName);
  writeDockWidgetSettings(prefs.get(), m_Ui->queueDockWidget);
  prefs->endGroup();

  prefs->endGroup();

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

  QByteArray splitterGeometry = m_Ui->splitter->saveGeometry();
  QByteArray splitterSizes = m_Ui->splitter->saveState();
  prefs->setValue(QString("Splitter_Geometry"), splitterGeometry);
  prefs->setValue(QString("Splitter_Sizes"), splitterSizes);

  prefs->endGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::writeDockWidgetSettings(QtSSettings* prefs, QDockWidget* dw)
{
  prefs->setValue(dw->objectName(), dw->isHidden());
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
  connect(importDataAction, &QAction::triggered, [=] { IMFViewer_UI::importData(); });
  fileMenu->addAction(importDataAction);

  QMenu* importMontageMenu = new QMenu("Import Montage");
  fileMenu->addMenu(importMontageMenu);

  QAction* d3dMontageAction = new QAction("DREAM3D");
  connect(d3dMontageAction, &QAction::triggered, this, &IMFViewer_UI::importDREAM3DMontage);
  importMontageMenu->addAction(d3dMontageAction);

  QAction* ebsdMontageAction = new QAction("EBSD");
  connect(ebsdMontageAction, &QAction::triggered, this, &IMFViewer_UI::importEbsdMontage);
  importMontageMenu->addAction(ebsdMontageAction);

  QAction* fijiMontageAction = new QAction("Fiji");
  connect(fijiMontageAction, &QAction::triggered, this, qOverload<>(&IMFViewer_UI::importFijiMontage));
  importMontageMenu->addAction(fijiMontageAction);

  QAction* genericMontageAction = new QAction("Generic");
  connect(genericMontageAction, &QAction::triggered, this, &IMFViewer_UI::importGenericMontage);
  importMontageMenu->addAction(genericMontageAction);

  QAction* robometMontageAction = new QAction("Robomet");
  connect(robometMontageAction, &QAction::triggered, this, &IMFViewer_UI::importRobometMontage);
  importMontageMenu->addAction(robometMontageAction);

  QAction* zeissMontageAction = new QAction("Zeiss AxioVision");
  connect(zeissMontageAction, &QAction::triggered, this, &IMFViewer_UI::importZeissMontage);
  importMontageMenu->addAction(zeissMontageAction);

  QAction* zeissZenMontageAction = new QAction("Zeiss Zen");
  connect(zeissZenMontageAction, &QAction::triggered, this, &IMFViewer_UI::importZeissZenMontage);
  importMontageMenu->addAction(zeissZenMontageAction);

  QAction* executePipelineAction = new QAction("Execute Pipeline");
  executePipelineAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
  connect(executePipelineAction, &QAction::triggered, this, static_cast<void (IMFViewer_UI::*)(void)>(&IMFViewer_UI::executePipeline));
  fileMenu->addAction(executePipelineAction);

  QAction* performMontageAction = new QAction("Perform Montage");
  performMontageAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_P));
  performMontageAction->setEnabled(false);
  connect(performMontageAction, &QAction::triggered, this, static_cast<void (IMFViewer_UI::*)(void)>(&IMFViewer_UI::performMontage));
  fileMenu->addAction(performMontageAction);

  fileMenu->addSeparator();

  QAction* saveImageAction = new QAction("Save Image");
  saveImageAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
  saveImageAction->setEnabled(false);
  connect(saveImageAction, &QAction::triggered, this, &IMFViewer_UI::saveImage);
  fileMenu->addAction(saveImageAction);

  QAction* saveDream3dAction = new QAction("Save As DREAM3D File");
  saveDream3dAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D));
  saveDream3dAction->setEnabled(false);
  connect(saveDream3dAction, &QAction::triggered, this, &IMFViewer_UI::saveDream3d);
  fileMenu->addAction(saveDream3dAction);

  fileMenu->addSeparator();

  //  m_RecentFilesMenu = new QMenu("Recent Sessions", this);
  //  fileMenu->addMenu(m_RecentFilesMenu);

  //  m_ClearRecentsAction = new QAction("Clear Recent Sessions", this);
  //  connect(m_ClearRecentsAction, &QAction::triggered, [=] {
  //    // Clear the Recent Items Menu
  //    m_RecentFilesMenu->clear();
  //    m_RecentFilesMenu->addSeparator();
  //    m_RecentFilesMenu->addAction(m_ClearRecentsAction);

  //    // Clear the actual list
  //    QtSRecentFileList* recents = QtSRecentFileList::Instance();
  //    recents->clear();

  //    // Write out the empty list
  //    QSharedPointer<QtSSettings> prefs = QSharedPointer<QtSSettings>(new QtSSettings());
  //    recents->writeList(prefs.data());
  //  });

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

  viewMenu->addAction(m_Ui->queueDockWidget->toggleViewAction());

  m_MenuBar->addMenu(viewMenu);

  // Add Filter Menu
  QMenu* filterMenu = m_Ui->vsWidget->getFilterMenu();
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

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::listenSelectionChanged(VSAbstractFilter::FilterListType filters)
{
  bool isSIMPL = !filters.empty() && dynamic_cast<VSSIMPLDataContainerFilter*>(filters.front()) != nullptr;
  bool isPipeline = !filters.empty() && dynamic_cast<VSPipelineFilter*>(filters.front()) != nullptr;
  bool isDream3dFile = !filters.empty() && dynamic_cast<VSFileNameFilter*>(filters.front()) != nullptr && filters.front()->getChildCount() > 0 &&
                       dynamic_cast<VSSIMPLDataContainerFilter*>(filters.front()->getChildren().front()) != nullptr;
  QList<QAction*> actions = m_MenuBar->actions();
  for(QAction* action : actions)
  {
    if(action->text() == "File")
    {
      QList<QAction*> fileActions = action->menu()->actions();
      for(QAction* fileAction : fileActions)
      {
        if(fileAction->text() == "Save Image")
        {
          fileAction->setEnabled(isSIMPL);
        }
        else if(fileAction->text() == "Save As DREAM3D File")
        {
          fileAction->setEnabled(isSIMPL || isPipeline || isDream3dFile);
        }
        else if(fileAction->text() == "Perform Montage")
        {
          int validImageCount = 0;
          for(VSAbstractFilter* filter : filters)
          {
            if(dynamic_cast<VSSIMPLDataContainerFilter*>(filter) != nullptr)
            {
              validImageCount++;
            }
          }
          fileAction->setEnabled(validImageCount >= 2);
        }
      }
      break;
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
std::pair<int, int> IMFViewer_UI::buildCustomDCA(const DataContainerArray::Pointer& dca, VSAbstractFilter::FilterListType montageDatasets)
{
  // Get the minimum X and minimum Y coordinates to use as a 'base'
  // This is useful to handle edge cases where the position values are large
  double minX = 0.0;
  double minY = 0.0;
  bool firstDataset = true;
  std::list<VSAbstractFilter*>::const_iterator iterator;
  for(iterator = montageDatasets.begin(); iterator != montageDatasets.end(); ++iterator)
  {
    VSAbstractFilter* montageDataset = *iterator;
    std::array<double, 3> pos = montageDataset->getTransform()->getLocalPosition();
    if(dynamic_cast<VSFileNameFilter*>(montageDataset) != nullptr)
    {
      pos = montageDataset->getChildren().front()->getTransform()->getLocalPosition();
    }
    if(firstDataset)
    {
      minX = pos[0];
      minY = pos[1];
      firstDataset = false;
    }
    else
    {
      if(pos[0] < minX)
      {
        minX = pos[0];
      }
      if(pos[1] < minY)
      {
        minY = pos[1];
      }
    }
  }

  montageDatasets.sort([minX](VSAbstractFilter* first, VSAbstractFilter* second) {
    VSTransform* firstTransform = first->getTransform();
    VSTransform* secondTransform = second->getTransform();
    if(dynamic_cast<VSFileNameFilter*>(first) != nullptr && dynamic_cast<VSFileNameFilter*>(second) != nullptr)
    {
      firstTransform = first->getChildren().front()->getTransform();
      secondTransform = second->getChildren().front()->getTransform();
    }
    int firstX = firstTransform->getLocalPosition()[0] - minX;
    int secondX = secondTransform->getLocalPosition()[0] - minX;
    bool secondXGreaterThanFirstX = floor(1.05 * firstX) < secondX;
    return secondXGreaterThanFirstX;
  });
  montageDatasets.sort([minY](VSAbstractFilter* first, VSAbstractFilter* second) {
    VSTransform* firstTransform = first->getTransform();
    VSTransform* secondTransform = second->getTransform();
    if(dynamic_cast<VSFileNameFilter*>(first) != nullptr && dynamic_cast<VSFileNameFilter*>(second) != nullptr)
    {
      firstTransform = first->getChildren().front()->getTransform();
      secondTransform = second->getChildren().front()->getTransform();
    }
    int firstY = firstTransform->getLocalPosition()[1] - minY;
    int secondY = secondTransform->getLocalPosition()[1] - minY;
    bool secondYGreaterThanFirstY = floor(1.05 * firstY) < secondY;
    return secondYGreaterThanFirstY;
  });

  int row = 0;
  int col = 0;
  int numRows = 0;
  int numCols = 0;
  firstDataset = true;
  double prevX;
  double prevY;

  for(iterator = montageDatasets.begin(); iterator != montageDatasets.end(); ++iterator)
  {
    VSAbstractFilter* montageDataset = *iterator;
    std::array<double, 3> pos = montageDataset->getTransform()->getLocalPosition();
    if(dynamic_cast<VSFileNameFilter*>(montageDataset) != nullptr)
    {
      pos = montageDataset->getChildren().front()->getTransform()->getLocalPosition();
    }
    if(firstDataset)
    {
      prevX = pos[0];
      prevY = pos[1];
      firstDataset = false;
    }
    else
    {
      if(pos[0] > prevX)
      {
        col++;
      }
      else
      {
        row++;
        col = 0;
      }
      if((row + 1) > numRows)
      {
        numRows = row + 1;
      }
      if((col + 1) > numCols)
      {
        numCols = col + 1;
      }
      prevX = pos[0];
      prevY = pos[1];
    }
    DataContainer::Pointer dataContainer;
    VSSIMPLDataContainerFilter* dcFilter = dynamic_cast<VSSIMPLDataContainerFilter*>(montageDataset);
    VSFileNameFilter* filenameFilter = dynamic_cast<VSFileNameFilter*>(montageDataset);
    QString dataContainerPrefix;
    QString rowColIdString = tr("r%1c%2").arg(row).arg(col);
    if(dcFilter != nullptr)
    {
      dataContainer = dcFilter->getWrappedDataContainer()->m_DataContainer;
      QString dataContainerName = dataContainer->getName();
      int indexOfUnderscore = dataContainerName.lastIndexOf("_");
      dataContainerPrefix = dataContainerName.left(indexOfUnderscore);
    }
    else if(filenameFilter != nullptr)
    {
      dataContainerPrefix = filenameFilter->getFilterName();
      dataContainer = DataContainer::New(dataContainerPrefix);
    }
    QString dcName = tr("%1_%2").arg(dataContainerPrefix, rowColIdString);
    dataContainer->setName(dcName);

    ImageGeom::Pointer geom = dataContainer->getGeometryAs<ImageGeom>();
    if(geom == ImageGeom::NullPointer())
    {
      geom = ImageGeom::New();
      dataContainer->setGeometry(geom);
    }
    geom->setOrigin(pos[0], pos[1], 1.0f);
    dca->addOrReplaceDataContainer(dataContainer);
  }
  return {numRows, numCols};
}
