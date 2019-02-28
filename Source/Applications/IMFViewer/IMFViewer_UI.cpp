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
#include "SIMPLib/Filtering/FilterFactory.hpp"
#include "SIMPLib/Filtering/FilterManager.h"
#include "SIMPLib/Filtering/FilterPipeline.h"
#include "SIMPLib/Filtering/QMetaObjectUtilities.h"
#include "SIMPLib/Plugin/PluginManager.h"
#include "SIMPLib/Utilities/SIMPLH5DataReader.h"
#include "SIMPLib/Utilities/SIMPLH5DataReaderRequirements.h"

#include "SVWidgetsLib/QtSupport/QtSRecentFileList.h"
#include "SVWidgetsLib/QtSupport/QtSSettings.h"
#include "SVWidgetsLib/Widgets/SVStyle.h"

#include "SIMPLVtkLib/Wizards/ImportMontage/ImportMontageWizard.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/MontageWorker.h"
#include "SIMPLVtkLib/Wizards/ImportMontage/TileConfigFileGenerator.h"

#include "ImportMontage/ImportMontageConstants.h"

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

  m_displayMontage = montageWizard->field(ImportMontage::FieldNames::DisplayMontage).toBool();
  m_displayOutline = montageWizard->field(ImportMontage::FieldNames::DisplayOutlineOnly).toBool();

  if(m_displayMontage)
  {
    m_DisplayType = ImportMontageWizard::DisplayType::Montage;
  }
  else if(m_displayOutline)
  {
    m_DisplayType = ImportMontageWizard::DisplayType::Outline;
  }
  else
  {
    m_DisplayType = ImportMontageWizard::DisplayType::SideBySide;
  }

  if(result == QDialog::Accepted)
  {
    ImportMontageWizard::InputType inputType = montageWizard->field(ImportMontage::FieldNames::InputType).value<ImportMontageWizard::InputType>();
    VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);

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
      QMessageBox::critical(this, "Invalid File Type",
                            tr("IMF Viewer failed to detect the proper data file type from the given input file.  "
                               "Supported file types are DREAM3D, VTK, STL and Image files, as well as Fiji and Robomet configuration files."),
                            QMessageBox::StandardButton::Ok);
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
  double tileOverlap = montageWizard->field(ImportMontage::Generic::FieldNames::TileOverlap).toDouble();
  QString montageName = montageWizard->field(ImportMontage::Generic::FieldNames::MontageName).toString();
  montageWizard->setField(ImportMontage::Fiji::FieldNames::MontageName, montageName);
  montageWizard->setField(ImportMontage::Fiji::FieldNames::TileOverlap, tileOverlap);
  montageWizard->setField(ImportMontage::Fiji::FieldNames::DataFilePath, fijiFilePath);
  montageWizard->setField(ImportMontage::Fiji::FieldNames::NumberOfRows, numOfRows);
  montageWizard->setField(ImportMontage::Fiji::FieldNames::NumberOfColumns, numOfCols);

  importFijiMontage(montageWizard);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::processPipelineMessage(const PipelineMessage& pipelineMsg)
{
  if(pipelineMsg.getType() == PipelineMessage::MessageType::StatusMessage)
  {
    QString str = pipelineMsg.generateStatusString();
    statusBar()->showMessage(str);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::handleMontageResults(int err)
{
  if(err >= 0)
  {
    DataContainerArray::Pointer dca = m_pipeline->getDataContainerArray();
    // If Display Montage was selected, remove non-stitched image data containers
    if(m_displayMontage)
    {
      for(DataContainer::Pointer dc : dca->getDataContainers())
      {
        if(dc->getName() != "MontageDC")
        {
          dca->removeDataContainer(dc->getName());
        }
      }
    }
    VSMainWidgetBase* baseWidget = dynamic_cast<VSMainWidgetBase*>(m_Internals->vsWidget);
    baseWidget->importPipelineOutput(m_pipeline, dca);
    baseWidget->getActiveViewWidget()->getFilterViewModel()->setDisplayType(m_DisplayType);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importDREAM3DMontage(ImportMontageWizard* montageWizard)
{
  m_pipeline = FilterPipeline::New();
  QString montageName = montageWizard->field(ImportMontage::DREAM3D::FieldNames::MontageName).toString();
  m_pipeline->setName(montageName);

  QString dataFilePath = montageWizard->field(ImportMontage::DREAM3D::FieldNames::DataFilePath).toString();

  SIMPLH5DataReader reader;
  bool success = reader.openFile(dataFilePath);
  if(success)
  {
    connect(&reader, &SIMPLH5DataReader::errorGenerated, [=](const QString& title, const QString& msg, const int& code) { QMessageBox::critical(this, title, msg, QMessageBox::StandardButton::Ok); });

    DataContainerArrayProxy dream3dProxy = montageWizard->field(ImportMontage::DREAM3D::FieldNames::Proxy).value<DataContainerArrayProxy>();

    m_dataContainerArray = reader.readSIMPLDataUsingProxy(dream3dProxy, false);
    if(m_dataContainerArray.get() == nullptr)
    {
      reader.closeFile();
      return;
    }

    reader.closeFile();

    // Instantiate DREAM3D File Reader filter
    QString filterName = "DataContainerReader";
    FilterManager* fm = FilterManager::Instance();
    IFilterFactory::Pointer factory = fm->getFactoryFromClassName(filterName);
    AbstractFilter::Pointer dataContainerReader;

    if(factory.get() != nullptr)
    {
      dataContainerReader = factory->create();
      if(dataContainerReader.get() != nullptr)
      {
        dataContainerReader->setDataContainerArray(m_dataContainerArray);

        QVariant var;

        // Set input file
        var.setValue(dataFilePath);
        if(!setFilterProperty(dataContainerReader, "InputFile", var))
        {
          return;
        }

        // Set data container array proxy
        var.setValue(dream3dProxy);
        if(!setFilterProperty(dataContainerReader, "InputFileDataContainerArrayProxy", var))
        {
          return;
        }

        m_pipeline->pushBack(dataContainerReader);
      }
      else
      {
        statusBar()->showMessage(tr("Could not create filter from %1 factory. Aborting.").arg(filterName));
        return;
      }
    }
    else
    {
      statusBar()->showMessage(tr("Could not create %1 factory. Aborting.").arg(filterName));
      return;
    }

    if(m_displayMontage || m_displayOutline)
    {
      QStringList dcNames = m_dataContainerArray->getDataContainerNames();

      int rowCount = montageWizard->field(ImportMontage::DREAM3D::FieldNames::NumberOfRows).toInt();
      int colCount = montageWizard->field(ImportMontage::DREAM3D::FieldNames::NumberOfColumns).toInt();

      performMontaging(montageWizard, dcNames, ImportMontageWizard::InputType::DREAM3D, rowCount, colCount);
    }
    runPipelineThread();
  }
  else
  {
    statusBar()->showMessage(tr("Unable to open file '%1'. Aborting.").arg(dataFilePath));
    return;
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importFijiMontage(ImportMontageWizard* montageWizard)
{
  m_pipeline = FilterPipeline::New();
  QString montageName = montageWizard->field(ImportMontage::Fiji::FieldNames::MontageName).toString();
  m_pipeline->setName(montageName);

  // Instantiate Import Fiji Montage filter
  QString filterName = "ITKImportFijiMontage";
  FilterManager* fm = FilterManager::Instance();
  IFilterFactory::Pointer factory = fm->getFactoryFromClassName(filterName);
  m_dataContainerArray = DataContainerArray::New();
  AbstractFilter::Pointer importFijiMontageFilter;

  // Set up the Import Fiji Montage filter
  if(factory.get() != nullptr)
  {
    importFijiMontageFilter = factory->create();
    if(importFijiMontageFilter.get() != nullptr)
    {
      importFijiMontageFilter->setDataContainerArray(m_dataContainerArray);

      QVariant var;

      // Set the path for the Fiji Configuration File
      QString fijiConfigFilePath = montageWizard->field(ImportMontage::Fiji::FieldNames::DataFilePath).toString();
      var.setValue(fijiConfigFilePath);
      if(!setFilterProperty(importFijiMontageFilter, "FijiConfigFilePath", var))
      {
        return;
      }

      // Set the Data Container Prefix
      var.setValue(montageWizard->field(ImportMontage::Fiji::FieldNames::DataContainerPrefix));
      if(!setFilterProperty(importFijiMontageFilter, "DataContainerPrefix", var))
      {
        return;
      }

      // Set the Cell Attribute Matrix Name
      var.setValue(montageWizard->field(ImportMontage::Fiji::FieldNames::CellAttributeMatrixName));
      if(!setFilterProperty(importFijiMontageFilter, "CellAttributeMatrixName", var))
      {
        return;
      }

      // Set the Image Array Name
      var.setValue(montageWizard->field(ImportMontage::Fiji::FieldNames::ImageArrayName));
      if(!setFilterProperty(importFijiMontageFilter, "AttributeArrayName", var))
      {
        return;
      }

      m_pipeline->pushBack(importFijiMontageFilter);
    }
    else
    {
      statusBar()->showMessage(tr("Could not create filter from %1 factory. Aborting.").arg(filterName));
      return;
    }
  }
  else
  {
    statusBar()->showMessage(tr("Could not create %1 factory. Aborting.").arg(filterName));
    return;
  }

  if(m_displayMontage || m_displayOutline)
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
		factory = fm->getFactoryFromClassName("SetOriginResolutionImageGeom");
		AbstractFilter::Pointer setOriginResolutionFilter;
		float spacingX = montageWizard->field(ImportMontage::Fiji::FieldNames::SpacingX).toFloat();
		float spacingY = montageWizard->field(ImportMontage::Fiji::FieldNames::SpacingY).toFloat();
		float spacingZ = montageWizard->field(ImportMontage::Fiji::FieldNames::SpacingZ).toFloat();
		FloatVec3_t newSpacing = { spacingX, spacingY, spacingZ };
		float originX = montageWizard->field(ImportMontage::Fiji::FieldNames::OriginX).toFloat();
		float originY = montageWizard->field(ImportMontage::Fiji::FieldNames::OriginY).toFloat();
		float originZ = montageWizard->field(ImportMontage::Fiji::FieldNames::OriginZ).toFloat();
		FloatVec3_t newOrigin = { originX, originY, originZ };
		QVariant var;

		if(factory.get() != nullptr)
		{

			// For each data container, add a new filter 
			for(QString dcName : dcNames)
			{
				setOriginResolutionFilter = factory->create();
				if(setOriginResolutionFilter.get() != nullptr)
				{
					// Set the data container name
					var.setValue(dcName);
					if(!setFilterProperty(setOriginResolutionFilter, "DataContainerName", var))
					{
						return;
					}

					// Set the change spacing boolean flag (change resolution)
					var.setValue(changeSpacing);
					if(!setFilterProperty(setOriginResolutionFilter, "ChangeResolution", var))
					{
						return;
					}

					// Set the spacing
					var.setValue(newSpacing);
					if(!setFilterProperty(setOriginResolutionFilter, "Resolution", var))
					{
						return;
					}

					// Set the change origin boolean flag (change resolution)
					var.setValue(changeOrigin);
					if(!setFilterProperty(setOriginResolutionFilter, "ChangeOrigin", var))
					{
						return;
					}

					// Set the origin
					var.setValue(newOrigin);
					if(!setFilterProperty(setOriginResolutionFilter, "Origin", var))
					{
						return;
					}

					m_pipeline->pushBack(setOriginResolutionFilter);
				}
			}
		}
	}

    int rowCount = montageWizard->field(ImportMontage::Fiji::FieldNames::NumberOfRows).toInt();
    int colCount = montageWizard->field(ImportMontage::Fiji::FieldNames::NumberOfColumns).toInt();

    performMontaging(montageWizard, dcNames, ImportMontageWizard::InputType::Fiji, rowCount, colCount);
  }

  // Run the pipeline
  runPipelineThread();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importRobometMontage(ImportMontageWizard* montageWizard)
{
  m_pipeline = FilterPipeline::New();
  QString montageName = montageWizard->field(ImportMontage::Robomet::FieldNames::MontageName).toString();
  m_pipeline->setName(montageName);

  // Instantiate Import RoboMet Montage filter
  QString filterName = "ITKImportRoboMetMontage";
  FilterManager* fm = FilterManager::Instance();
  IFilterFactory::Pointer factory = fm->getFactoryFromClassName(filterName);
  DataContainerArray::Pointer dca = DataContainerArray::New();
  AbstractFilter::Pointer importRoboMetMontageFilter;

  // Set up the Generate Montage Configuration filter
  if(factory.get() != nullptr)
  {
    importRoboMetMontageFilter = factory->create();
    if(importRoboMetMontageFilter.get() != nullptr)
    {
      importRoboMetMontageFilter->setDataContainerArray(dca);

      QVariant var;

      // Set the path for the Robomet Configuration File
      QString configFilePath = montageWizard->field(ImportMontage::Robomet::FieldNames::DataFilePath).toString();
      var.setValue(configFilePath);
      if(!setFilterProperty(importRoboMetMontageFilter, "RegistrationFile", var))
      {
        return;
      }

      // Set the Data Container Prefix
      var.setValue(montageWizard->field(ImportMontage::Robomet::FieldNames::DataContainerPrefix));
      if(!setFilterProperty(importRoboMetMontageFilter, "DataContainerPrefix", var))
      {
        return;
      }

      // Set the Cell Attribute Matrix Name
      var.setValue(montageWizard->field(ImportMontage::Robomet::FieldNames::CellAttributeMatrixName));
      if(!setFilterProperty(importRoboMetMontageFilter, "CellAttributeMatrixName", var))
      {
        return;
      }

      // Set the Image Array Name
      var.setValue(montageWizard->field(ImportMontage::Robomet::FieldNames::ImageArrayName));
      if(!setFilterProperty(importRoboMetMontageFilter, "AttributeArrayName", var))
      {
        return;
      }

      // Slice number
      int sliceNumber = montageWizard->field(ImportMontage::Robomet::FieldNames::SliceNumber).toInt();
      var.setValue(sliceNumber);
      if(!setFilterProperty(importRoboMetMontageFilter, "SliceNumber", var))
      {
        return;
      }

      // Image file prefix
      QString imageFilePrefix = montageWizard->field(ImportMontage::Robomet::FieldNames::ImageFilePrefix).toString();
      var.setValue(imageFilePrefix);
      if(!setFilterProperty(importRoboMetMontageFilter, "ImageFilePrefix", var))
      {
        return;
      }

      // Image file suffix
      QString imageFileSuffix = montageWizard->field(ImportMontage::Robomet::FieldNames::ImageFileSuffix).toString();
      var.setValue(imageFileSuffix);
      if(!setFilterProperty(importRoboMetMontageFilter, "ImageFileSuffix", var))
      {
        return;
      }

      // Image file extension
      QString imageFileExtension = montageWizard->field(ImportMontage::Robomet::FieldNames::ImageFileExtension).toString();
      var.setValue(imageFileExtension);
      if(!setFilterProperty(importRoboMetMontageFilter, "ImageFileExtension", var))
      {
        return;
      }

      m_pipeline->pushBack(importRoboMetMontageFilter);
    }
    else
    {
      statusBar()->showMessage(tr("Could not create filter from %1 factory. Aborting.").arg(filterName));
      return;
    }
  }
  else
  {
    statusBar()->showMessage(tr("Could not create %1 factory. Aborting.").arg(filterName));
    return;
  }

  if(m_displayMontage || m_displayOutline)
  {
    importRoboMetMontageFilter->preflight();
    DataContainerArray::Pointer dca = importRoboMetMontageFilter->getDataContainerArray();
    QStringList dcNames = dca->getDataContainerNames();

    ImportMontageWizard::InputType inputType = montageWizard->field(ImportMontage::FieldNames::InputType).value<ImportMontageWizard::InputType>();

    int rowCount = montageWizard->field(ImportMontage::Robomet::FieldNames::NumberOfRows).toInt();
    int colCount = montageWizard->field(ImportMontage::Robomet::FieldNames::NumberOfColumns).toInt();

    performMontaging(montageWizard, dcNames, ImportMontageWizard::InputType::Robomet, rowCount, colCount);
  }

  runPipelineThread();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::importZeissMontage(ImportMontageWizard* montageWizard)
{
  m_pipeline = FilterPipeline::New();
  QString montageName = montageWizard->field(ImportMontage::Zeiss::FieldNames::MontageName).toString();
  m_pipeline->setName(montageName);

  // Instantiate Import AxioVision V4 Montage filter
  QString filterName = "ImportAxioVisionV4Montage";
  FilterManager* fm = FilterManager::Instance();
  IFilterFactory::Pointer factory = fm->getFactoryFromClassName(filterName);
  DataContainerArray::Pointer dca = DataContainerArray::New();
  AbstractFilter::Pointer importZeissMontageFilter;

  // Set up the Generate Montage Configuration filter
  if(factory.get() != nullptr)
  {
    importZeissMontageFilter = factory->create();
    if(importZeissMontageFilter.get() != nullptr)
    {
      importZeissMontageFilter->setDataContainerArray(dca);

      QVariant var;

      // Set the path for the Robomet Configuration File
      QString configFilePath = montageWizard->field(ImportMontage::Zeiss::FieldNames::DataFilePath).toString();
      var.setValue(configFilePath);
      if(!setFilterProperty(importZeissMontageFilter, "InputFile", var))
      {
        return;
      }

      // Set the Data Container Prefix
      QString dataContainerPrefix = montageWizard->field(ImportMontage::Zeiss::FieldNames::DataContainerPrefix).toString();
      var.setValue(dataContainerPrefix);
      if(!setFilterProperty(importZeissMontageFilter, "DataContainerName", var))
      {
        return;
      }

      // Set the Cell Attribute Matrix Name
      QString cellAttrMatrixName = montageWizard->field(ImportMontage::Zeiss::FieldNames::CellAttributeMatrixName).toString();
      var.setValue(cellAttrMatrixName);
      if(!setFilterProperty(importZeissMontageFilter, "CellAttributeMatrixName", var))
      {
        return;
      }

      // Set the Image Array Name
      QString attributeArrayName = montageWizard->field(ImportMontage::Zeiss::FieldNames::ImageDataArrayName).toString();
      var.setValue(attributeArrayName);
      if(!setFilterProperty(importZeissMontageFilter, "ImageDataArrayName", var))
      {
        return;
      }

      // Set the Metadata Attribute MatrixName
      QString metadataAttrMatrixName = montageWizard->field(ImportMontage::Zeiss::FieldNames::MetadataAttrMatrixName).toString();
      var.setValue(metadataAttrMatrixName);
      if(!setFilterProperty(importZeissMontageFilter, "MetaDataAttributeMatrixName", var))
      {
        return;
      }

      // Set Import All Metadata
      var.setValue(true);
      if(!setFilterProperty(importZeissMontageFilter, "ImportAllMetaData", var))
      {
        return;
      }

      // Set Convert to Grayscale
      bool convertToGrayscale = montageWizard->field(ImportMontage::Zeiss::FieldNames::ConvertToGrayscale).toBool();
      var.setValue(convertToGrayscale);
      if(!setFilterProperty(importZeissMontageFilter, "ConvertToGrayScale", var))
      {
        return;
      }

	  if(convertToGrayscale)
	  {
      float colorWeightR = montageWizard->field(ImportMontage::Zeiss::FieldNames::ColorWeightingR).toFloat();
      float colorWeightG = montageWizard->field(ImportMontage::Zeiss::FieldNames::ColorWeightingG).toFloat();
      float colorWeightB = montageWizard->field(ImportMontage::Zeiss::FieldNames::ColorWeightingB).toFloat();
		  FloatVec3_t colorWeights = { colorWeightR, colorWeightG, colorWeightB };
		  var.setValue(colorWeights);
		  if(!setFilterProperty(importZeissMontageFilter, "ColorWeights", var))
		  {
			  return;
		  }
	  }

	  // Set Origin
    bool changeOrigin = montageWizard->field(ImportMontage::Zeiss::FieldNames::ChangeOrigin).toBool();
	  var.setValue(changeOrigin);
	  if(!setFilterProperty(importZeissMontageFilter, "ChangeOrigin", var))
	  {
		  return;
	  }

	  if(changeOrigin)
	  {
      float originX = montageWizard->field(ImportMontage::Zeiss::FieldNames::OriginX).toFloat();
      float originY = montageWizard->field(ImportMontage::Zeiss::FieldNames::OriginY).toFloat();
      float originZ = montageWizard->field(ImportMontage::Zeiss::FieldNames::OriginZ).toFloat();
		  FloatVec3_t newOrigin = { originX, originY, originZ };
		  var.setValue(newOrigin);
		  if(!setFilterProperty(importZeissMontageFilter, "Origin", var))
		  {
			  return;
		  }
	  }

	  // Set Spacing
    bool changeSpacing = montageWizard->field(ImportMontage::Zeiss::FieldNames::ChangeSpacing).toBool();
	  var.setValue(changeSpacing);
	  if(!setFilterProperty(importZeissMontageFilter, "ChangeSpacing", var))
	  {
		  return;
	  }

	  if(changeSpacing)
	  {
      float spacingX = montageWizard->field(ImportMontage::Zeiss::FieldNames::SpacingX).toFloat();
      float spacingY = montageWizard->field(ImportMontage::Zeiss::FieldNames::SpacingY).toFloat();
      float spacingZ = montageWizard->field(ImportMontage::Zeiss::FieldNames::SpacingZ).toFloat();
		  FloatVec3_t newSpacing = { spacingX, spacingY, spacingZ };
		  var.setValue(newSpacing);
		  if(!setFilterProperty(importZeissMontageFilter, "Spacing", var))
		  {
			  return;
		  }
	  }

      m_pipeline->pushBack(importZeissMontageFilter);
    }
    else
    {
      statusBar()->showMessage(tr("Could not create filter from %1 factory. Aborting.").arg(filterName));
      return;
    }
  }
  else
  {
    statusBar()->showMessage(tr("Could not create %1 factory. Aborting.").arg(filterName));
    return;
  }

  if(m_displayMontage || m_displayOutline)
  {
    importZeissMontageFilter->preflight();
    DataContainerArray::Pointer dca = importZeissMontageFilter->getDataContainerArray();
    QStringList dcNames = dca->getDataContainerNames();
    int rowCount = importZeissMontageFilter->property("RowCount").toInt();
    int colCount = importZeissMontageFilter->property("ColumnCount").toInt();

    performMontaging(montageWizard, dcNames, ImportMontageWizard::InputType::Zeiss, rowCount, colCount);
  }

  runPipelineThread();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::performMontaging(ImportMontageWizard* montageWizard, QStringList dataContainerNames, ImportMontageWizard::InputType inputType, int rowCount, int colCount)
{
  // Instantiate Generate Montage filter
  QString filterName = "ITKPCMTileRegistration";
  FilterManager* fm = FilterManager::Instance();
  IFilterFactory::Pointer factory = fm->getFactoryFromClassName(filterName);
  AbstractFilter::Pointer itkRegistrationFilter;
  AbstractFilter::Pointer itkStitchingFilter;

  QString cellAttrMatrixName;
  QString commonDataArrayName;
  double tileOverlap = 0.0;
  switch(inputType)
  {
    case ImportMontageWizard::InputType::DREAM3D:
    {
      cellAttrMatrixName = montageWizard->field(ImportMontage::DREAM3D::FieldNames::CellAttributeMatrixName).toString();
      commonDataArrayName = montageWizard->field(ImportMontage::DREAM3D::FieldNames::ImageArrayName).toString();
      tileOverlap = montageWizard->field(ImportMontage::DREAM3D::FieldNames::TileOverlap).toDouble();
	  break;
    }
    case ImportMontageWizard::InputType::Fiji:
    {
      cellAttrMatrixName = montageWizard->field(ImportMontage::Fiji::FieldNames::CellAttributeMatrixName).toString();
      commonDataArrayName = montageWizard->field(ImportMontage::Fiji::FieldNames::ImageArrayName).toString();
      bool changeOverlap = montageWizard->field(ImportMontage::Fiji::FieldNames::ChangeTileOverlap).toBool();
      if (changeOverlap)
      {
        tileOverlap = montageWizard->field(ImportMontage::Fiji::FieldNames::TileOverlap).toDouble();
      }
	  break;
    }
    case ImportMontageWizard::InputType::Generic:
    {
      // The Generic montage creates a Fiji file and uses it to import images
      cellAttrMatrixName = montageWizard->field(ImportMontage::Fiji::FieldNames::CellAttributeMatrixName).toString();
      commonDataArrayName = montageWizard->field(ImportMontage::Fiji::FieldNames::ImageArrayName).toString();
      tileOverlap = montageWizard->field(ImportMontage::Fiji::FieldNames::TileOverlap).toDouble();
	  break;
    }
    case ImportMontageWizard::InputType::Robomet:
    {
      cellAttrMatrixName = montageWizard->field(ImportMontage::Robomet::FieldNames::CellAttributeMatrixName).toString();
      commonDataArrayName = montageWizard->field(ImportMontage::Robomet::FieldNames::ImageArrayName).toString();
      bool changeOverlap = montageWizard->field(ImportMontage::Robomet::FieldNames::ChangeTileOverlap).toBool();
      if (changeOverlap)
      {
        tileOverlap = montageWizard->field(ImportMontage::Robomet::FieldNames::TileOverlap).toDouble();
      }
	  break;
    }
    case ImportMontageWizard::InputType::Zeiss:
    {
      cellAttrMatrixName = montageWizard->field(ImportMontage::Zeiss::FieldNames::CellAttributeMatrixName).toString();
      commonDataArrayName = montageWizard->field(ImportMontage::Zeiss::FieldNames::ImageDataArrayName).toString();
      bool changeOverlap = montageWizard->field(ImportMontage::Zeiss::FieldNames::ChangeTileOverlap).toBool();
      if (changeOverlap)
      {
        tileOverlap = montageWizard->field(ImportMontage::Zeiss::FieldNames::TileOverlap).toDouble();
      }
	  break;
    }
  }

  // Set up the Generate Montage Configuration filter
  if(factory.get() != nullptr)
  {
    itkRegistrationFilter = factory->create();
    filterName = "ITKStitchMontage";
    factory = fm->getFactoryFromClassName(filterName);
    itkStitchingFilter = factory->create();
    if(itkRegistrationFilter.get() != nullptr && itkStitchingFilter.get() != nullptr)
    {
      itkRegistrationFilter->setDataContainerArray(m_dataContainerArray);
      itkStitchingFilter->setDataContainerArray(m_dataContainerArray);

      QVariant var;

      // Set montage size
      IntVec3_t montageSize = {colCount, rowCount, 1};
      var.setValue(montageSize);
      if(!setFilterProperty(itkRegistrationFilter, "MontageSize", var) || !setFilterProperty(itkStitchingFilter, "MontageSize", var))
      {
        return;
      }

      // Set Common Attribute Matrix Name
      var.setValue(cellAttrMatrixName);
      if(!setFilterProperty(itkRegistrationFilter, "CommonAttributeMatrixName", var) || !setFilterProperty(itkStitchingFilter, "CommonAttributeMatrixName", var))
      {
        return;
      }

      // Set Common Data Array Name
      var.setValue(commonDataArrayName);
      if(!setFilterProperty(itkRegistrationFilter, "CommonDataArrayName", var) || !setFilterProperty(itkStitchingFilter, "CommonDataArrayName", var))
      {
        return;
      }

      // Set the list of image data containers
      var.setValue(dataContainerNames);
      if(!setFilterProperty(itkRegistrationFilter, "ImageDataContainers", var) || !setFilterProperty(itkStitchingFilter, "ImageDataContainers", var))
      {
        return;
      }

      // Stitching specific properties

      // Tile overlap
      var.setValue(tileOverlap);
      if(!setFilterProperty(itkStitchingFilter, "TileOverlap", var))
      {
        return;
      }

      // Montage results data array path components
      QString montageDataContainer = "MontageDC";
      var.setValue(montageDataContainer);
      if(!setFilterProperty(itkStitchingFilter, "MontageDataContainerName", var))
      {
        return;
      }

      QString montageAttriubeMatrixName = "MontageAM";
      var.setValue(montageAttriubeMatrixName);
      if(!setFilterProperty(itkStitchingFilter, "MontageAttributeMatrixName", var))
      {
        return;
      }

      QString montageDataArrayName = "MontageData";
      var.setValue(montageDataArrayName);
      if(!setFilterProperty(itkStitchingFilter, "MontageDataArrayName", var))
      {
        return;
      }

      m_pipeline->pushBack(itkRegistrationFilter);
      m_pipeline->pushBack(itkStitchingFilter);
    }
    else
    {
      statusBar()->showMessage(tr("Could not create filter from %1 factory. Aborting.").arg(filterName));
      return;
    }
  }
  else
  {
    statusBar()->showMessage(tr("Could not create %1 factory. Aborting.").arg(filterName));
    return;
  }
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

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer_UI::runPipelineThread()
{
  // Run Pipeline
  m_workerThread = new QThread;
  MontageWorker* montageWorker = new MontageWorker(m_pipeline);
  montageWorker->moveToThread(m_workerThread);
  m_pipeline->addMessageReceiver(this);
  connect(m_workerThread, SIGNAL(started()), montageWorker, SLOT(process()));
  connect(montageWorker, SIGNAL(finished()), m_workerThread, SLOT(quit()));
  connect(montageWorker, SIGNAL(finished()), montageWorker, SLOT(deleteLater()));
  connect(m_workerThread, SIGNAL(finished()), m_workerThread, SLOT(deleteLater()));
  connect(montageWorker, &MontageWorker::resultReady, this, &IMFViewer_UI::handleMontageResults);
  m_workerThread->start();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer_UI::setFilterProperty(AbstractFilter::Pointer filter, const char* propertyName, QVariant value)
{
  if(!filter->setProperty(propertyName, value))
  {
    statusBar()->showMessage(tr("%1: %2 property not set. Aborting.").arg(filter->getHumanLabel()).arg(propertyName));
    return false;
  }
  return true;
}
