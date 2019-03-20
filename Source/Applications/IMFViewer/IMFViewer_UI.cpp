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

#include "SIMPLib/FilterParameters/FloatVec3.h"
#include "SIMPLib/FilterParameters/IntVec3FilterParameter.h"
#include "SIMPLib/Filtering/FilterPipeline.h"
#include "SIMPLib/Filtering/QMetaObjectUtilities.h"
#include "SIMPLib/Plugin/PluginManager.h"
#include "SIMPLib/Utilities/SIMPLH5DataReader.h"
#include "SIMPLib/Utilities/SIMPLH5DataReaderRequirements.h"

#include "SVWidgetsLib/QtSupport/QtSRecentFileList.h"
#include "SVWidgetsLib/QtSupport/QtSSettings.h"
#include "SVWidgetsLib/Widgets/SVStyle.h"

#include "SIMPLVtkLib/QtWidgets/VSDatasetImporter.h"
#include "SIMPLVtkLib/QtWidgets/VSFilterFactory.h"
#include "SIMPLVtkLib/QtWidgets/VSMontageImporter.h"
#include "SIMPLVtkLib/QtWidgets/VSQueueModel.h"
#include "SIMPLVtkLib/Visualization/VisualFilters/VSDataSetFilter.h"
#include "SIMPLVtkLib/Wizards/ExecutePipeline/ExecutePipelineConstants.h"
#include "SIMPLVtkLib/Wizards/ExecutePipeline/ExecutePipelineWizard.h"
#include "SIMPLVtkLib/Wizards/ExecutePipeline/PipelineWorker.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/FijiListWidget.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/ImportMontageConstants.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/ImportMontageWizard.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/RobometListWidget.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/TileConfigFileGenerator.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/ZeissListWidget.h"

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
  connect(m_Internals->importDataBtn, &QPushButton::clicked, [=] { importData(); });
  connect(m_Internals->importMontageBtn, &QPushButton::clicked, this, &IMFViewer_UI::importMontage);

  // Connection to update the recent files list on all windows when it changes
  QtSRecentFileList* recentsList = QtSRecentFileList::Instance(5, this);
  connect(recentsList, SIGNAL(fileListChanged(const QString&)), this, SLOT(updateRecentFileList(const QString&)));

  VSQueueModel* queueModel = new VSQueueModel(m_Internals->queueWidget);
  m_Internals->queueWidget->setQueueModel(queueModel);
  connect(m_Internals->queueWidget, &VSQueueWidget::notifyErrorMessage, this, &IMFViewer_UI::processErrorMessage);
  connect(m_Internals->queueWidget, &VSQueueWidget::notifyStatusMessage, this, &IMFViewer_UI::processStatusMessage);

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
    importData(filePath);
  }
  else if(ext == "stl")
  {
    importData(filePath);
  }
  else if(mimeType.inherits("image/png") || mimeType.inherits("image/tiff") || mimeType.inherits("image/jpeg") || mimeType.inherits("image/bmp"))
  {
    importData(filePath);
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

  if(result == QDialog::Accepted)
  {
    m_DisplayMontage = montageWizard->field(ImportMontage::FieldNames::DisplayMontage).toBool();
    m_DisplayOutline = montageWizard->field(ImportMontage::FieldNames::DisplayOutlineOnly).toBool();

    VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);
    VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();
    if(m_DisplayMontage)
    {
      filterViewModel->setDisplayType(ImportMontageWizard::DisplayType::Montage);
    }
    else if(m_DisplayOutline)
    {
      filterViewModel->setDisplayType(ImportMontageWizard::DisplayType::Outline);
    }
    else
    {
      filterViewModel->setDisplayType(ImportMontageWizard::DisplayType::SideBySide);
    }

    ImportMontageWizard::InputType inputType = montageWizard->field(ImportMontage::FieldNames::InputType).value<ImportMontageWizard::InputType>();

    // Based on the type of file imported, perform next action
    if(inputType == ImportMontageWizard::InputType::Generic)
    {
      importGenericMontage(montageWizard);
    }
    else if(inputType == ImportMontageWizard::InputType::DREAM3D)
    {
      importDREAM3DMontage(montageWizard);
    }
    else if(inputType == ImportMontageWizard::InputType::Fiji)
    {
      importFijiMontage(montageWizard);
    }
    else if(inputType == ImportMontageWizard::InputType::Robomet)
    {
      importRobometMontage(montageWizard);
    }
    else if(inputType == ImportMontageWizard::InputType::Zeiss)
    {
      importZeissMontage(montageWizard);
    }
    else
    {
      QString msg = tr("IMF Viewer failed to detect the proper data file type from the given input file.  "
                       "Supported file types are DREAM3D files, as well as Fiji, Robomet, and Zeiss configuration files.");
      QMessageBox::critical(this, "Import Montage", msg, QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
    }
  }

  delete montageWizard;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importGenericMontage(ImportMontageWizard* montageWizard)
{
  QString tileConfigFile = "TileConfiguration.txt";
  int numOfRows = montageWizard->field(ImportMontage::Generic::FieldNames::NumberOfRows).toInt();
  int numOfCols = montageWizard->field(ImportMontage::Generic::FieldNames::NumberOfColumns).toInt();

  // Get input file names
  FileListInfo_t inputFileInfo = montageWizard->field(ImportMontage::Generic::FieldNames::FileListInfo).value<FileListInfo_t>();

  // Generate tile configuration file.
  TileConfigFileGenerator tileConfigFileGenerator(inputFileInfo, montageWizard->field(ImportMontage::Generic::FieldNames::MontageType).value<MontageSettings::MontageType>(),
                                                  montageWizard->field(ImportMontage::Generic::FieldNames::MontageOrder).value<MontageSettings::MontageOrder>(), numOfCols, numOfRows,
                                                  montageWizard->field(ImportMontage::Generic::FieldNames::TileOverlap).toDouble(), tileConfigFile);
  tileConfigFileGenerator.buildTileConfigFile();

  QString fijiFilePath(inputFileInfo.InputPath);
  fijiFilePath.append(QDir::separator());
  fijiFilePath.append(tileConfigFile);

  // Change wizard data for Fiji use case
  QString montageName = montageWizard->field(ImportMontage::Generic::FieldNames::MontageName).toString();
  montageWizard->setField(ImportMontage::Fiji::FieldNames::MontageName, montageName);
  FijiListInfo_t fijiListInfo;
  fijiListInfo.FijiFilePath = fijiFilePath;
  QVariant var;
  var.setValue(fijiListInfo);
  montageWizard->setField(ImportMontage::Fiji::FieldNames::FijiListInfo, var);
  bool changeSpacing = montageWizard->field(ImportMontage::Generic::FieldNames::ChangeSpacing).toBool();
  montageWizard->setField(ImportMontage::Fiji::FieldNames::ChangeSpacing, changeSpacing);
  if(changeSpacing)
  {
    float spacingX = montageWizard->field(ImportMontage::Generic::FieldNames::SpacingX).toFloat();
    float spacingY = montageWizard->field(ImportMontage::Generic::FieldNames::SpacingY).toFloat();
    float spacingZ = montageWizard->field(ImportMontage::Generic::FieldNames::SpacingZ).toFloat();
    montageWizard->setField(ImportMontage::Fiji::FieldNames::SpacingX, spacingX);
    montageWizard->setField(ImportMontage::Fiji::FieldNames::SpacingY, spacingY);
    montageWizard->setField(ImportMontage::Fiji::FieldNames::SpacingZ, spacingZ);
  }
  bool changeOrigin = montageWizard->field(ImportMontage::Generic::FieldNames::ChangeOrigin).toBool();
  montageWizard->setField(ImportMontage::Fiji::FieldNames::ChangeOrigin, changeOrigin);
  if(changeOrigin)
  {
    float originX = montageWizard->field(ImportMontage::Generic::FieldNames::OriginX).toFloat();
    float originY = montageWizard->field(ImportMontage::Generic::FieldNames::OriginY).toFloat();
    float originZ = montageWizard->field(ImportMontage::Generic::FieldNames::OriginZ).toFloat();
    montageWizard->setField(ImportMontage::Fiji::FieldNames::OriginX, originX);
    montageWizard->setField(ImportMontage::Fiji::FieldNames::OriginY, originY);
    montageWizard->setField(ImportMontage::Fiji::FieldNames::OriginZ, originZ);
  }

  importFijiMontage(montageWizard);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importDREAM3DMontage(ImportMontageWizard* montageWizard)
{
  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();
  connect(filterFactory.get(), &VSFilterFactory::notifyErrorMessage,
          [=](const QString& msg, int code) { QMessageBox::critical(this, "Import DREAM3D Montage", msg, QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok); });

  FilterPipeline::Pointer pipeline = FilterPipeline::New();
  QString montageName = montageWizard->field(ImportMontage::DREAM3D::FieldNames::MontageName).toString();
  pipeline->setName(montageName);

  QString dataFilePath = montageWizard->field(ImportMontage::DREAM3D::FieldNames::DataFilePath).toString();

  SIMPLH5DataReader reader;
  bool success = reader.openFile(dataFilePath);
  if(success)
  {
    connect(&reader, &SIMPLH5DataReader::errorGenerated,
            [=](const QString& title, const QString& msg, const int& code) { QMessageBox::critical(this, title, msg, QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok); });

    DataContainerArrayProxy dream3dProxy = montageWizard->field(ImportMontage::DREAM3D::FieldNames::Proxy).value<DataContainerArrayProxy>();

    m_dataContainerArray = reader.readSIMPLDataUsingProxy(dream3dProxy, false);
    if(m_dataContainerArray.get() == nullptr)
    {
      reader.closeFile();
      return;
    }

    reader.closeFile();

    AbstractFilter::Pointer dataContainerReader = filterFactory->createDataContainerReaderFilter(dataFilePath, dream3dProxy);
    if(!dataContainerReader)
    {
      // Error!
    }

    pipeline->pushBack(dataContainerReader);

    if(m_DisplayMontage || m_DisplayOutline)
    {
      QStringList dcNames = m_dataContainerArray->getDataContainerNames();

      // Change spacing and/or origin (if selected)
      bool changeSpacing = montageWizard->field(ImportMontage::DREAM3D::FieldNames::ChangeSpacing).toBool();
      bool changeOrigin = montageWizard->field(ImportMontage::DREAM3D::FieldNames::ChangeOrigin).toBool();
      if(changeSpacing || changeOrigin)
      {
        float spacingX = montageWizard->field(ImportMontage::DREAM3D::FieldNames::SpacingX).toFloat();
        float spacingY = montageWizard->field(ImportMontage::DREAM3D::FieldNames::SpacingY).toFloat();
        float spacingZ = montageWizard->field(ImportMontage::DREAM3D::FieldNames::SpacingZ).toFloat();
        FloatVec3_t newSpacing = {spacingX, spacingY, spacingZ};
        float originX = montageWizard->field(ImportMontage::DREAM3D::FieldNames::OriginX).toFloat();
        float originY = montageWizard->field(ImportMontage::DREAM3D::FieldNames::OriginY).toFloat();
        float originZ = montageWizard->field(ImportMontage::DREAM3D::FieldNames::OriginZ).toFloat();
        FloatVec3_t newOrigin = {originX, originY, originZ};
        QVariant var;

        // For each data container, add a new filter
        for(QString dcName : dcNames)
        {
          AbstractFilter::Pointer setOriginResolutionFilter = filterFactory->createSetOriginResolutionFilter(dcName, changeSpacing, changeOrigin, newSpacing, newOrigin);

          if(!setOriginResolutionFilter)
          {
            // Error!
          }

          pipeline->pushBack(setOriginResolutionFilter);
        }
      }

      int rowCount = montageWizard->field(ImportMontage::DREAM3D::FieldNames::NumberOfRows).toInt();
      int colCount = montageWizard->field(ImportMontage::DREAM3D::FieldNames::NumberOfColumns).toInt();

      IntVec3_t montageSize = {colCount, rowCount, 1};

      QString amName = montageWizard->field(ImportMontage::DREAM3D::FieldNames::CellAttributeMatrixName).toString();
      QString daName = montageWizard->field(ImportMontage::DREAM3D::FieldNames::ImageArrayName).toString();
      double tileOverlap = montageWizard->field(ImportMontage::DREAM3D::FieldNames::TileOverlap).toDouble();

      AbstractFilter::Pointer itkRegistrationFilter = filterFactory->createPCMTileRegistrationFilter(montageSize, dcNames, amName, daName);
      pipeline->pushBack(itkRegistrationFilter);

      DataArrayPath montagePath("MontageDC", "MontageAM", "MontageData");
      AbstractFilter::Pointer itkStitchingFilter = filterFactory->createTileStitchingFilter(montageSize, dcNames, amName, daName, montagePath, tileOverlap);
      pipeline->pushBack(itkStitchingFilter);
    }

    addPipelineToQueue(pipeline);
  }
  else
  {
    QMessageBox::critical(this, "Import DREAM3D Montage", tr("Unable to open file '%1'. Aborting.").arg(dataFilePath), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importFijiMontage(ImportMontageWizard* montageWizard)
{
  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();

  FilterPipeline::Pointer pipeline = FilterPipeline::New();
  QString montageName = montageWizard->field(ImportMontage::DREAM3D::FieldNames::MontageName).toString();
  pipeline->setName(montageName);

  FijiListInfo_t fijiListInfo = montageWizard->field(ImportMontage::Fiji::FieldNames::FijiListInfo).value<FijiListInfo_t>();

  QString fijiConfigFilePath = fijiListInfo.FijiFilePath;
  QString dcPrefix = montageWizard->field(ImportMontage::Fiji::FieldNames::DataContainerPrefix).toString();
  QString amName = montageWizard->field(ImportMontage::Fiji::FieldNames::CellAttributeMatrixName).toString();
  QString daName = montageWizard->field(ImportMontage::Fiji::FieldNames::ImageArrayName).toString();

  AbstractFilter::Pointer importFijiMontageFilter = filterFactory->createImportFijiMontageFilter(fijiConfigFilePath, dcPrefix, amName, daName);
  if(!importFijiMontageFilter)
  {
    // Error!
  }

  pipeline->pushBack(importFijiMontageFilter);

  if(m_DisplayMontage || m_DisplayOutline)
  {
    // Set Image Data Containers
    importFijiMontageFilter->preflight();
    DataContainerArray::Pointer dca = importFijiMontageFilter->getDataContainerArray();
    QStringList dcNames = dca->getDataContainerNames();

    // Change spacing and/or origin (if selected)
    bool changeSpacing = montageWizard->field(ImportMontage::Fiji::FieldNames::ChangeSpacing).toBool();
    bool changeOrigin = montageWizard->field(ImportMontage::Fiji::FieldNames::ChangeOrigin).toBool();
    if(changeSpacing || changeOrigin)
    {
      float spacingX = montageWizard->field(ImportMontage::Fiji::FieldNames::SpacingX).toFloat();
      float spacingY = montageWizard->field(ImportMontage::Fiji::FieldNames::SpacingY).toFloat();
      float spacingZ = montageWizard->field(ImportMontage::Fiji::FieldNames::SpacingZ).toFloat();
      FloatVec3_t newSpacing = {spacingX, spacingY, spacingZ};
      float originX = montageWizard->field(ImportMontage::Fiji::FieldNames::OriginX).toFloat();
      float originY = montageWizard->field(ImportMontage::Fiji::FieldNames::OriginY).toFloat();
      float originZ = montageWizard->field(ImportMontage::Fiji::FieldNames::OriginZ).toFloat();
      FloatVec3_t newOrigin = {originX, originY, originZ};
      QVariant var;

      // For each data container, add a new filter
      for(QString dcName : dcNames)
      {
        AbstractFilter::Pointer setOriginResolutionFilter = filterFactory->createSetOriginResolutionFilter(dcName, changeSpacing, changeOrigin, newSpacing, newOrigin);
        if(!setOriginResolutionFilter)
        {
          // Error!
        }

        pipeline->pushBack(setOriginResolutionFilter);
      }
    }

    int rowCount = importFijiMontageFilter->property("RowCount").toInt();
    int colCount = importFijiMontageFilter->property("ColumnCount").toInt();

    IntVec3_t montageSize = {colCount, rowCount, 1};
    double tileOverlap = 0.0;

    bool changeOverlap = montageWizard->field(ImportMontage::Fiji::FieldNames::ChangeTileOverlap).toBool();
    if(changeOverlap)
    {
      tileOverlap = montageWizard->field(ImportMontage::Fiji::FieldNames::TileOverlap).toDouble();
    }

    AbstractFilter::Pointer itkRegistrationFilter = filterFactory->createPCMTileRegistrationFilter(montageSize, dcNames, amName, daName);
    pipeline->pushBack(itkRegistrationFilter);

    DataArrayPath montagePath("MontageDC", "MontageAM", "MontageData");
    AbstractFilter::Pointer itkStitchingFilter = filterFactory->createTileStitchingFilter(montageSize, dcNames, amName, daName, montagePath, tileOverlap);
    pipeline->pushBack(itkStitchingFilter);
  }

  // Run the pipeline
  addPipelineToQueue(pipeline);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importRobometMontage(ImportMontageWizard* montageWizard)
{
  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();

  RobometListInfo_t rbmListInfo = montageWizard->field(ImportMontage::Robomet::FieldNames::RobometListInfo).value<RobometListInfo_t>();
  int sliceMin = rbmListInfo.SliceMin;
  int sliceMax = rbmListInfo.SliceMax;

  for(int slice = sliceMin; slice <= sliceMax; slice++)
  {
    FilterPipeline::Pointer pipeline = FilterPipeline::New();
    QString montageName = montageWizard->field(ImportMontage::DREAM3D::FieldNames::MontageName).toString();
    QString pipelineName = montageName;
    pipelineName.append(tr("_%1").arg(slice));
    pipeline->setName(pipelineName);

    QString robometFilePath = rbmListInfo.RobometFilePath;
    QString dcPrefix = montageWizard->field(ImportMontage::Robomet::FieldNames::DataContainerPrefix).toString();
    QString amName = montageWizard->field(ImportMontage::Robomet::FieldNames::CellAttributeMatrixName).toString();
    QString daName = montageWizard->field(ImportMontage::Robomet::FieldNames::ImageArrayName).toString();
    QString imagePrefix = rbmListInfo.ImagePrefix;
    QString imageFileExtension = rbmListInfo.ImageExtension;

    AbstractFilter::Pointer importRoboMetMontageFilter = filterFactory->createImportRobometMontageFilter(robometFilePath, dcPrefix, amName, daName, slice, imagePrefix, imageFileExtension);
    if(!importRoboMetMontageFilter)
    {
      // Error!
    }

    pipeline->pushBack(importRoboMetMontageFilter);

    if(m_DisplayMontage || m_DisplayOutline)
    {
      importRoboMetMontageFilter->preflight();
      DataContainerArray::Pointer dca = importRoboMetMontageFilter->getDataContainerArray();
      QStringList dcNames = dca->getDataContainerNames();

      int rowCount = rbmListInfo.NumberOfRows;
      int colCount = rbmListInfo.NumberOfColumns;
      IntVec3_t montageSize = {colCount, rowCount, 1};
      double tileOverlap = 0.0;

      bool changeOverlap = montageWizard->field(ImportMontage::Robomet::FieldNames::ChangeTileOverlap).toBool();
      if(changeOverlap)
      {
        tileOverlap = montageWizard->field(ImportMontage::Robomet::FieldNames::TileOverlap).toDouble();
      }

      AbstractFilter::Pointer itkRegistrationFilter = filterFactory->createPCMTileRegistrationFilter(montageSize, dcNames, amName, daName);
      pipeline->pushBack(itkRegistrationFilter);

      DataArrayPath montagePath("MontageDC", "MontageAM", "MontageData");
      AbstractFilter::Pointer itkStitchingFilter = filterFactory->createTileStitchingFilter(montageSize, dcNames, amName, daName, montagePath, tileOverlap);
      pipeline->pushBack(itkStitchingFilter);
    }

    addPipelineToQueue(pipeline);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importZeissMontage(ImportMontageWizard* montageWizard)
{
  FilterPipeline::Pointer pipeline = FilterPipeline::New();
  AbstractFilter::Pointer importZeissMontage;
  QString montageName = montageWizard->field(ImportMontage::DREAM3D::FieldNames::MontageName).toString();
  pipeline->setName(montageName);

  VSFilterFactory::Pointer filterFactory = VSFilterFactory::New();

  ZeissListInfo_t zeissListInfo = montageWizard->field(ImportMontage::Zeiss::FieldNames::ZeissListInfo).value<ZeissListInfo_t>();

  QString configFilePath = zeissListInfo.ZeissFilePath;
  QString dataContainerPrefix = montageWizard->field(ImportMontage::Zeiss::FieldNames::DataContainerPrefix).toString();
  QString cellAttrMatrixName = montageWizard->field(ImportMontage::Zeiss::FieldNames::CellAttributeMatrixName).toString();
  QString attributeArrayName = montageWizard->field(ImportMontage::Zeiss::FieldNames::ImageDataArrayName).toString();
  QString metadataAttrMatrixName = montageWizard->field(ImportMontage::Zeiss::FieldNames::MetadataAttrMatrixName).toString();
  bool importAllMetadata = true;
  bool convertToGrayscale = montageWizard->field(ImportMontage::Zeiss::FieldNames::ConvertToGrayscale).toBool();
  bool changeSpacing = montageWizard->field(ImportMontage::Zeiss::FieldNames::ChangeSpacing).toBool();
  bool changeOrigin = montageWizard->field(ImportMontage::Zeiss::FieldNames::ChangeOrigin).toBool();
  float colorWeights[3];
  float newSpacing[3];
  float newOrigin[3];

  if(changeSpacing || changeOrigin)
  {
    newSpacing[0] = montageWizard->field(ImportMontage::Zeiss::FieldNames::SpacingX).toFloat();
    newSpacing[1] = montageWizard->field(ImportMontage::Zeiss::FieldNames::SpacingY).toFloat();
    newSpacing[2] = montageWizard->field(ImportMontage::Zeiss::FieldNames::SpacingZ).toFloat();
    newOrigin[0] = montageWizard->field(ImportMontage::Zeiss::FieldNames::OriginX).toFloat();
    newOrigin[1] = montageWizard->field(ImportMontage::Zeiss::FieldNames::OriginY).toFloat();
    newOrigin[2] = montageWizard->field(ImportMontage::Zeiss::FieldNames::OriginZ).toFloat();
  }
  if(convertToGrayscale)
  {
    colorWeights[0] = montageWizard->field(ImportMontage::Zeiss::FieldNames::ColorWeightingR).toFloat();
    colorWeights[1] = montageWizard->field(ImportMontage::Zeiss::FieldNames::ColorWeightingG).toFloat();
    colorWeights[2] = montageWizard->field(ImportMontage::Zeiss::FieldNames::ColorWeightingB).toFloat();
  }
  importZeissMontage = filterFactory->createImportZeissMontageFilter(configFilePath, dataContainerPrefix, cellAttrMatrixName, attributeArrayName, metadataAttrMatrixName, importAllMetadata,
                                                                     convertToGrayscale, changeOrigin, changeSpacing, colorWeights, newOrigin, newSpacing);

  if(!importZeissMontage)
  {
    // Error!
  }

  pipeline->pushBack(importZeissMontage);

  if(m_DisplayMontage || m_DisplayOutline)
  {
    importZeissMontage->preflight();
    DataContainerArray::Pointer dca = importZeissMontage->getDataContainerArray();
    QStringList dcNames = dca->getDataContainerNames();
    int rowCount = importZeissMontage->property("RowCount").toInt();
    int colCount = importZeissMontage->property("ColumnCount").toInt();
    IntVec3_t montageSize = {colCount, rowCount, 1};
    double tileOverlap = 0.0;

    bool changeOverlap = montageWizard->field(ImportMontage::Zeiss::FieldNames::ChangeTileOverlap).toBool();
    if(changeOverlap)
    {
      tileOverlap = montageWizard->field(ImportMontage::Zeiss::FieldNames::TileOverlap).toDouble();
    }

    AbstractFilter::Pointer itkRegistrationFilter = filterFactory->createPCMTileRegistrationFilter(montageSize, dcNames, cellAttrMatrixName, attributeArrayName);
    pipeline->pushBack(itkRegistrationFilter);

    DataArrayPath montagePath("MontageDC", "MontageAM", "MontageData");
    AbstractFilter::Pointer itkStitchingFilter = filterFactory->createTileStitchingFilter(montageSize, dcNames, cellAttrMatrixName, attributeArrayName, montagePath, tileOverlap);
    pipeline->pushBack(itkStitchingFilter);
  }

  addPipelineToQueue(pipeline);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::addPipelineToQueue(FilterPipeline::Pointer pipeline)
{
  VSMontageImporter::Pointer importer = VSMontageImporter::New(pipeline);
  connect(importer.get(), &VSMontageImporter::resultReady, this, &IMFViewer_UI::handleMontageResults);

  m_Internals->queueWidget->addDataImporter(pipeline->getName(), importer);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::executePipeline(FilterPipeline::Pointer pipeline, DataContainerArray::Pointer dca)
{
  pipeline->addMessageReceiver(this);
  m_PipelineWorker = new PipelineWorker();
  connect(m_PipelineWorker, SIGNAL(finished()), this, SLOT(pipelineWorkerFinished()));
  connect(m_PipelineWorker, &PipelineWorker::resultReady, this, &IMFViewer_UI::handleMontageResults);

  m_PipelineWorker->addPipeline(pipeline, dca);

  m_PipelineWorkerThread = new QThread;
  m_PipelineWorker->moveToThread(m_PipelineWorkerThread);
  connect(m_PipelineWorkerThread, SIGNAL(started()), m_PipelineWorker, SLOT(process()));
  connect(m_PipelineWorkerThread, SIGNAL(finished()), this, SLOT(pipelineThreadFinished()));
  connect(m_PipelineWorker, SIGNAL(finished()), m_PipelineWorkerThread, SLOT(quit()));

  m_PipelineWorkerThread->start();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::handleDatasetResults(VSFileNameFilter* textFilter, VSDataSetFilter* filter)
{
  // Check if any data was imported
  if(filter->getOutput())
  {
    VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);
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
void IMFViewer_UI::handleMontageResults(FilterPipeline::Pointer pipeline, int err)
{
  if(err >= 0)
  {
    DataContainerArray::Pointer dca = pipeline->getDataContainerArray();
    QStringList pipelineNameTokens = pipeline->getName().split("_", QString::SplitBehavior::SkipEmptyParts);
    int slice = 0;
    if(pipelineNameTokens.size() > 1)
    {
      slice = pipelineNameTokens[1].toInt();
    }

    // If Display Montage was selected, remove non-stitched image data containers
    if(m_DisplayMontage)
    {
      for(DataContainer::Pointer dc : dca->getDataContainers())
      {
        if(dc->getName() == "MontageDC")
        {
          ImageGeom::Pointer imageGeom = dc->getGeometryAs<ImageGeom>();
          if(imageGeom)
          {
            float origin[3];
            imageGeom->getOrigin(origin);
            origin[2] += slice;
            imageGeom->setOrigin(origin);
          }
        }
        else
        {
          dca->removeDataContainer(dc->getName());
        }
      }
    }

    VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);
    baseWidget->importPipelineOutput(pipeline, dca);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importData(const QString& filePath)
{
  VSDatasetImporter::Pointer importer = VSDatasetImporter::New(filePath);
  connect(importer.get(), &VSDatasetImporter::resultReady, this, &IMFViewer_UI::handleDatasetResults);

  QFileInfo fi(filePath);
  m_Internals->queueWidget->addDataImporter(fi.fileName(), importer);
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
    importPipeline(executePipelineWizard);
  }

  delete executePipelineWizard;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importPipeline(ExecutePipelineWizard* executePipelineWizard)
{
  m_DisplayMontage = executePipelineWizard->field(ImportMontage::FieldNames::DisplayMontage).toBool();
  m_DisplayOutline = executePipelineWizard->field(ImportMontage::FieldNames::DisplayOutlineOnly).toBool();

  VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);
  VSFilterViewModel* filterViewModel = baseWidget->getActiveViewWidget()->getFilterViewModel();
  if(m_DisplayMontage)
  {
    filterViewModel->setDisplayType(ImportMontageWizard::DisplayType::Montage);
  }
  else if(m_DisplayOutline)
  {
    filterViewModel->setDisplayType(ImportMontageWizard::DisplayType::Outline);
  }
  else
  {
    filterViewModel->setDisplayType(ImportMontageWizard::DisplayType::SideBySide);
  }

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
      addPipelineToQueue(pipelineFromJson);
    }
    else if(executionType == ExecutePipelineWizard::ExecutionType::OnLoadedData)
    {
      int startFilter = executePipelineWizard->field(ExecutePipeline::FieldNames::StartFilter).toInt();
      int selectedDataset = executePipelineWizard->field(ExecutePipeline::FieldNames::SelectedDataset).toInt();

      // Construct Data Container Array with selected Dataset
      DataContainerArray::Pointer dca = DataContainerArray::New();
      VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);
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
            bool isSIMPL = dynamic_cast<VSSIMPLDataContainerFilter*>(childFilter);
            if(isSIMPL)
            {
              VSSIMPLDataContainerFilter* dcFilter = dynamic_cast<VSSIMPLDataContainerFilter*>(childFilter);
              if(dcFilter != nullptr)
              {
                DataContainer::Pointer dataContainer = dcFilter->getWrappedDataContainer()->m_DataContainer;
                dca->addDataContainer(dataContainer);
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
        FloatVec3_t newSpacing = {spacingX, spacingY, spacingZ};
        float originX = executePipelineWizard->field(ExecutePipeline::FieldNames::OriginX).toFloat();
        float originY = executePipelineWizard->field(ExecutePipeline::FieldNames::OriginY).toFloat();
        float originZ = executePipelineWizard->field(ExecutePipeline::FieldNames::OriginZ).toFloat();
        FloatVec3_t newOrigin = {originX, originY, originZ};
        QVariant var;

        // For each data container, add a new filter
        for(QString dcName : dcNames)
        {
          AbstractFilter::Pointer setOriginResolutionFilter = filterFactory->createSetOriginResolutionFilter(dcName, changeSpacing, changeOrigin, newSpacing, newOrigin);

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

      addPipelineToQueue(pipeline);
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::processErrorMessage(const QString& errorMessage, int code)
{
  //  statusBar()->showMessage(errorMessage);
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
  connect(importDataAction, &QAction::triggered, [=] { IMFViewer_UI::importData(); });
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
