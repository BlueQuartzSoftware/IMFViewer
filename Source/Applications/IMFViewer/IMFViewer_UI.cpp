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

#include "IMFViewer.h"

#include <QtCore/QSharedPointer>
#include <QtCore/QSignalMapper>
#include <QtCore/QMap>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QMessageBox>

#include <iostream>

#include "IMFViewerLib/Common/Constants.h"

#include "H5Support/QH5Utilities.h"
#include "H5Support/H5Utilities.h"
#include "H5Support/HDF5ScopedFileSentinel.h"

#include "OptionsDialogs/RootOptionsDialog.h"
#include "OptionsDialogs/CampaignOptionsDialog.h"
#include "OptionsDialogs/EbsdRawOptions.h"
#include "OptionsDialogs/EbsdReducedOptions.h"
#include "OptionsDialogs/EbsdMetaOptions.h"

#include "STTreeWidgetItems/OriginShiftTreeWidgetItem.h"
#include "STTreeWidgetItems/DirectRemapTreeWidgetItem.h"
#include "STTreeWidgetItems/PolynomialTreeWidgetItem.h"
#include "STTreeWidgetItems/RotationTreeWidgetItem.h"
#include "STTreeWidgetItems/StrideTreeWidgetItem.h"

#include "TableItemDelegates/AttributesTableItemDelegate.h"

#include "DataCampaigns/EbsdDataCampaign.h"
#include "DataCampaigns/BSEDataCampaign.h"

#include "CSDFTreeWidgetItem.h"
#include "AddCampaignWidget.h"
#include "AddRawWidget.h"
#include "AddReducedWidget.h"
#include "AddMetaWidget.h"
#include "AddSpatialTransformWidget.h"
#include "RemoveCampaignDialog.h"
#include "CSDFInProgress.h"
#include "IMFViewerApplication.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewer::IMFViewer(QWidget* parent) :
  QMainWindow(parent),
  m_LastOpenDirectory(""),
  m_OpenFilePath(""),
  m_CSDFSourcePath(""),
  m_Root(NULL),
  m_WorkerThread(NULL),
  m_CSDFObject(NULL)
{
  setupUi(this);

  setupGui();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewer::~IMFViewer()
{
  
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
CSDFTreeWidgetItem* IMFViewer::AddTreeItem(QString name, CSDFTreeWidgetItem::ObjectType type, int index, CSDFTreeWidgetItem* parent, AbstractOptionsDialog* optionsDialog, const QString &csdfFilePath)
{
  CSDFTreeWidgetItem* item = new CSDFTreeWidgetItem(type, optionsDialog, csdfFilePath);
  item->setText(0, name);

  if (type == CSDFTreeWidgetItem::Node || type == CSDFTreeWidgetItem::STFolder)
  {
    item->setIcon(0, QIcon(":/folder_blue.png"));
  }
  else if (type == CSDFTreeWidgetItem::Leaf)
  {
    item->setIcon(0, QIcon(":/text.png"));
  }
  else if (type == CSDFTreeWidgetItem::Root)
  {
    item->setFlags(item->flags() | Qt::ItemIsEditable);
  }

  if (NULL != parent)
  {
    parent->insertCSDFChild(index, item);
  }

  return item;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QList<QString> IMFViewer::CampaignTypes()
{
  QList<QString> types;

  types.push_back(IMFViewerProj::CampaignTypes::EBSDCampaign);
  types.push_back(IMFViewerProj::CampaignTypes::BSECampaign);

  return types;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::setupGui()
{
  // Set the splitter stretch factors
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);

  /* Setting a custom item delegate, so that we can restrict input to attributes based on 
   * the type of input it should be (scalar, string, etc.). This is why we need to pass it
   * both the tree widget and the table widget. */
  AttributesTableItemDelegate* dlg = new AttributesTableItemDelegate(treeWidget, attributesTable);
  attributesTable->setItemDelegate(dlg);

  treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(treeWidget, SIGNAL(filesToBeImported(QList<QString>)), this, SLOT(importCSDFFiles(QList<QString>)));
  
  connect(treeWidget, SIGNAL(customContextMenuRequested(const QPoint&)), csdfApp, SLOT(on_treeWidgetContextMenuRequested(const QPoint&)));
  connect(this, SIGNAL(actionNew_triggered()), csdfApp, SLOT(on_actionNew_triggered()));
  connect(this, SIGNAL(actionOpen_triggered()), csdfApp, SLOT(on_actionOpen_triggered()));

  connect(actionAddCampaign, SIGNAL(triggered()), this, SLOT(addCampaign()));

  // Create the right-click contextual menu
  m_Menu = new QMenu(this);

  // Resize the columns to contents
  attributesTable->horizontalHeader()->setSectionResizeMode(Name, QHeaderView::ResizeToContents);
  attributesTable->horizontalHeader()->setSectionResizeMode(Value, QHeaderView::ResizeToContents);

  // Disable the attributes table until there is a root item in the navigation tree
  attributesTable->setDisabled(true);

  // Disable the Go button and Campaign menu until the root has been created
  goBtn->setDisabled(true);
  menuCampaign->setDisabled(true);

  // Run this slot once, to produce error messages
  on_outputFile_textChanged(outputFile->text());

  // Register the IMFViewerState enumeration type with the meta system
  qRegisterMetaType<IMFViewer::IMFViewerState>("IMFViewer::IMFViewerState");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::on_actionNew_triggered()
{
  emit actionNew_triggered();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::on_actionOpen_triggered()
{
  emit actionOpen_triggered();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::on_actionImportCSDFFile_triggered()
{
  QStringList files = QFileDialog::getOpenFileNames(this, "Select Files To Import", m_LastOpenDirectory, "CSDF Files (*.csdf)");
  if (files.size() <= 0) { return; }

  importCSDFFiles(files);

  QFileInfo fi(files[0]);
  m_LastOpenDirectory = fi.absolutePath();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::on_actionExitIMFViewer_triggered()
{
  csdfApp->quit();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::on_goBtn_clicked()
{
  if (goBtn->text().compare("Cancel") == 0)
  {
    qDebug() << "Canceling from GUI...." << "\n";
    finishWriting();
    emit writeCanceled();
    return;
  }

  progressBar->show();


  if (m_WorkerThread != NULL)
  {
    m_WorkerThread->wait(); // Wait until the thread is complete
    if (m_WorkerThread->isFinished() == true)
    {
      delete m_WorkerThread;
      m_WorkerThread = NULL;
      delete m_CSDFObject;
      m_CSDFObject = NULL;
    }
  }
  m_WorkerThread = new QThread(); // Create a new Thread Resource

  progressBar->setMinimum(0);
  if (m_DataCampaignMap.size() > 0)
  {
    progressBar->setMaximum(m_DataCampaignMap.size() * IMFViewerProj::StatusBarMultiplier);
  }
  else
  {
    progressBar->setMaximum(1 * IMFViewerProj::StatusBarMultiplier);
  }

  QString outputFilePath = outputFile->text();

  QList<QSharedPointer<DataCampaign> > list = m_DataCampaignMap.values();
  for (int i = 0; i < list.size(); i++)
  {
    DataCampaign* campaign = list[i].data();
    // If the use clicks on the "Cancel" button send a message to the DataCampaign object
    connect(this, SIGNAL(writeCanceled()), campaign, SLOT(cancelWriting()), Qt::DirectConnection);
  }

  m_CSDFObject = new CSDFInProgress();
  m_CSDFObject->setTreeRoot(m_Root);
  m_CSDFObject->setCSDFSourcePath(m_CSDFSourcePath);
  m_CSDFObject->setDataCampaigns(m_DataCampaignMap);
  m_CSDFObject->setOutputPath(outputFilePath);
  connect(m_CSDFObject, SIGNAL(taskFinished(int)), this, SLOT(updateProgressBar(int)));
  connect(m_CSDFObject, SIGNAL(showStatusMessage(const QString&, IMFViewer::IMFViewerState)), this, SLOT(updateStatusBar(const QString&, IMFViewer::IMFViewerState)));
  connect(m_CSDFObject, SIGNAL(showErrorMessage(const QString&)), this, SLOT(showErrorMessage(const QString&)));

  // Move the FilterPipeline object into the thread that we just created.
  m_CSDFObject->moveToThread(m_WorkerThread);

  /* Connect the signal 'started()' from the QThread to the 'run' slot of the
  * PipelineBuilder object. Since the PipelineBuilder object has been moved to another
  * thread of execution and the actual QThread lives in *this* thread then the
  * type of connection will be a Queued connection.
  */
  // When the thread starts its event loop, start the PipelineBuilder going
  connect(m_WorkerThread, SIGNAL(started()), m_CSDFObject, SLOT(run()));

  // If the cancel button is pressed, then tell the QThread to stop its event loop
  connect(this, SIGNAL(writeCanceled()), m_WorkerThread, SLOT(quit()));

  // When the PipelineBuilder ends then tell the QThread to stop its event loop
  connect(m_CSDFObject, SIGNAL(writeFinished()), m_WorkerThread, SLOT(quit()));

  // When the QThread finishes, tell this object that it has finished.
  connect(m_WorkerThread, SIGNAL(finished()), this, SLOT(finishWriting()));

  menuFile->setDisabled(true);
  selectBtn->setDisabled(true);
  outputFile->setDisabled(true);

  for (int row = 0; row < attributesTable->rowCount(); row++)
  {
    QTableWidgetItem* item = attributesTable->item(row, Value);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  }

  m_WorkerThread->start();
  emit writeStarted();
  goBtn->setText("Cancel");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::finishWriting()
{
  menuFile->setEnabled(true);
  selectBtn->setEnabled(true);
  outputFile->setEnabled(true);
  progressBar->setValue(0);
  goBtn->setText("Go");

  for (int row = 0; row < attributesTable->rowCount(); row++)
  {
      QTableWidgetItem* item = attributesTable->item(row, Value);
      item->setFlags(item->flags() | Qt::ItemIsEditable);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::on_selectBtn_clicked()
{
  QString filePath = QFileDialog::getSaveFileName(this, tr("Select Output File"), m_LastOpenDirectory, tr("CSDF Files (*.csdf)"));
  if (filePath.isEmpty()) { return; }

  QFileInfo fi(filePath);
  m_LastOpenDirectory = fi.path();

  outputFile->setText(filePath);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::on_outputFile_textChanged(const QString &text)
{
  if (text.isEmpty())
  {
    toState(Error);
    outputFile->setStyleSheet("QLineEdit\n{\nborder: 2px groove FireBrick\n}");
    statusBar()->showMessage("The output file is empty.  Please provide an output file path.");
    return;
  }

  QFileInfo fi(text);
  QDir dir(fi.path());

  if ( fi.completeSuffix() != "csdf" || dir.exists() == false )
  {
    toState(Error);
    outputFile->setStyleSheet("QLineEdit\n{\nborder: 2px groove FireBrick\n}");
    
    if (fi.completeSuffix() != "csdf")
    {
      statusBar()->showMessage("The provided output file path does not have the CSDF file extension.");
    }
    else if (dir.exists() == false)
    {
      statusBar()->showMessage("The provided output file path does not point to a valid location on the file system.");
    }
  }
  else
  {
    toState(Default);
    statusBar()->clearMessage();
    outputFile->setStyleSheet("");
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addCampaign()
{
  AddCampaignWidget* acWidget = new AddCampaignWidget(this);
  acWidget->setWindowFlags(Qt::Window);
  acWidget->setWindowModality(Qt::ApplicationModal);
  acWidget->exec();

  int result = acWidget->result();
  if (result == QDialog::Accepted)
  {
    acWidget->hide();

    QString campaignName = acWidget->getCampaignName();
    QString campaignType = acWidget->getCampaignType();
    QString rawFolderName = acWidget->getRawFolderName();
    QString reducedFolderName = acWidget->getReducedFolderName();
    QString metaFolderName = acWidget->getMetaFolderName();

    DataCampaign* campaign = addCampaign(campaignName, campaignType, m_DataCampaignMap.count());
    if (NULL != campaign)
    {
      if (campaignType == IMFViewerProj::CampaignTypes::EBSDCampaign)
      {
        campaign->addRawFolder(rawFolderName, true);
        campaign->addReducedFolder(reducedFolderName, true);
        campaign->addMetaFolder(metaFolderName, true);
      }
      else if (campaignType == IMFViewerProj::CampaignTypes::BSECampaign)
      {
        campaign->addReducedFolder(reducedFolderName, true);
        campaign->addMetaFolder(metaFolderName, true);
      }
    }
  }

  delete acWidget;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataCampaign* IMFViewer::addCampaign(const QString &campaignName, const QString &campaignType, int insert, const QString &csdfFilePath)
{
  if (NULL != m_Root->childByName(campaignName))
  {
    QMessageBox::critical(this, "Add Campaign Error", "'" + campaignName + "' could not be added, because a campaign with that name already exists.", QMessageBox::Ok, QMessageBox::Ok);
    return NULL;
  }

  DataCampaign* campaign;
  if (campaignType == IMFViewerProj::CampaignTypes::EBSDCampaign)
  {
    campaign = new EbsdDataCampaign(campaignName, campaignType, csdfFilePath, this);
    getRootOptionsDialog()->addCampaignRow(campaignName, IMFViewerProj::CampaignTypes::EBSDCampaign, insert);
  }
  else if (campaignType == IMFViewerProj::CampaignTypes::BSECampaign)
  {
    campaign = new BSEDataCampaign(campaignName, campaignType, csdfFilePath, this);
    getRootOptionsDialog()->addCampaignRow(campaignName, IMFViewerProj::CampaignTypes::BSECampaign, insert);
  }
  else
  {
    return NULL;
  }

  connect(campaign, SIGNAL(taskFinished(int)), this, SLOT(updateProgressBar(int)));
  connect(campaign, SIGNAL(showStatusMessage(const QString&, IMFViewer::IMFViewerState)), this, SLOT(updateStatusBar(const QString&, IMFViewer::IMFViewerState)));
  CSDFTreeWidgetItem* campaignRoot = campaign->getCampaignRoot();
  campaignRoot->addAttribute(IMFViewerProj::AttributeNames::CampaignType, campaignType, AttributeData::String, true);
  m_Root->insertCSDFChild(insert, campaignRoot);
  campaignRoot->setExpanded(true);
  QSharedPointer<DataCampaign> dcShared = QSharedPointer<DataCampaign>(campaign);
  m_DataCampaignMap.insert(campaignName, dcShared);

  return campaign;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::removeCampaign(const QString &campaignName)
{
  DataCampaign* campaign = m_DataCampaignMap.value(campaignName).data();
  CSDFTreeWidgetItem* campaignRoot = campaign->getCampaignRoot();
  CSDFTreeWidgetItem* currentItem = treeWidget->csdfCurrentItem();
  if (NULL != currentItem && NULL != campaignRoot)
  {
    CSDFTreeWidgetItem* tempPtr = currentItem;
    while (NULL != tempPtr)
    {
      /* This is a loop that walks up the subTree to see if subTreeRoot is an ancestor of currentItem.
      We need to do this so that we can clear and disable the table if the user removes
      a campaign while the currently selected item happens to be in that campaign. */
      if (tempPtr == campaignRoot)
      {
        attributesTable->clearContents();
        attributesTable->setDisabled(true);
      }

      tempPtr = tempPtr->csdfParent();
    }
  }

  m_DataCampaignMap.remove(campaignName);
  m_Root->removeCSDFChild(campaignRoot);
  delete campaignRoot;
  getRootOptionsDialog()->removeCampaignRow(campaignName);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::removeCampaign(DataCampaign* campaign)
{
  CSDFTreeWidgetItem* campaignRoot = campaign->getCampaignRoot();
  QString campaignName = campaignRoot->text(CSDFTreeWidgetItem::Name);
  CSDFTreeWidgetItem* currentItem = treeWidget->csdfCurrentItem();
  if (NULL != currentItem && NULL != campaignRoot)
  {
    CSDFTreeWidgetItem* tempPtr = currentItem;
    while (NULL != tempPtr)
    {
      /* This is a loop that walks up the subTree to see if subTreeRoot is an ancestor of currentItem.
      We need to do this so that we can clear and disable the table if the user removes
      a campaign while the currently selected item happens to be in that campaign. */
      if (tempPtr == campaignRoot)
      {
        attributesTable->clearContents();
        attributesTable->setDisabled(true);
      }

      tempPtr = tempPtr->csdfParent();
    }
  }

  m_DataCampaignMap.remove(campaignName);
  m_Root->removeCSDFChild(campaignRoot);
  delete campaignRoot;
  getRootOptionsDialog()->removeCampaignRow(campaignName);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addRawFolder(const QString &csdfFilePath)
{
  AddRawWidget* addRawWidget = new AddRawWidget();
  addRawWidget->exec();

  int result = addRawWidget->result();
  if (result == QDialog::Accepted)
  {
    CSDFTreeWidgetItem* currentItem = treeWidget->csdfCurrentItem();
    if (NULL == currentItem)
    {
      return;
    }

    QString campaignName = currentItem->text(CSDFTreeWidgetItem::Name);
    QString rawName = addRawWidget->getRawFolderName();

    DataCampaign* campaign = m_DataCampaignMap.value(campaignName).data();
    CSDFTreeWidgetItem* campaignRoot = campaign->getCampaignRoot();

    if (NULL != campaignRoot->childByName(rawName))
    {
      QMessageBox::critical(this, "Add Folder Error", "The folder '" + rawName + "' could not be added, because a folder with that name already exists.", QMessageBox::Ok, QMessageBox::Ok);
      return;
    }

    if (csdfFilePath.isEmpty() == true)
    {
      campaign->addRawFolder(rawName, true, csdfFilePath);
    }
    else
    {
      campaign->addRawFolder(rawName, false, csdfFilePath);
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addReducedFolder(const QString &csdfFilePath)
{
  AddReducedWidget* addReducedWidget = new AddReducedWidget();
  addReducedWidget->exec();

  int result = addReducedWidget->result();
  if (result == QDialog::Accepted)
  {
    CSDFTreeWidgetItem* currentItem = treeWidget->csdfCurrentItem();
    if (NULL == currentItem)
    {
      return;
    }

    QString campaignName = currentItem->text(CSDFTreeWidgetItem::Name);
    QString reducedName = addReducedWidget->getReducedFolderName();

    DataCampaign* campaign = m_DataCampaignMap.value(campaignName).data();
    CSDFTreeWidgetItem* campaignRoot = campaign->getCampaignRoot();

    if (NULL != campaignRoot->childByName(reducedName))
    {
      QMessageBox::critical(this, "Add Folder Error", "The folder '" + reducedName + "' could not be added, because a folder with that name already exists.", QMessageBox::Ok, QMessageBox::Ok);
      return;
    }

    if (csdfFilePath.isEmpty() == true)
    {
      campaign->addReducedFolder(reducedName, true, csdfFilePath);
    }
    else
    {
      campaign->addReducedFolder(reducedName, false, csdfFilePath);
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addMetaFolder(const QString &csdfFilePath)
{
  AddMetaWidget* addMetaWidget = new AddMetaWidget();
  addMetaWidget->exec();

  int result = addMetaWidget->result();
  if (result == QDialog::Accepted)
  {
    CSDFTreeWidgetItem* currentItem = treeWidget->csdfCurrentItem();
    if (NULL == currentItem)
    {
      return;
    }

    QString campaignName = currentItem->text(CSDFTreeWidgetItem::Name);
    QString metaName = addMetaWidget->getMetaFolderName();

    DataCampaign* campaign = m_DataCampaignMap.value(campaignName).data();
    CSDFTreeWidgetItem* campaignRoot = campaign->getCampaignRoot();

    if (NULL != campaignRoot->childByName(metaName))
    {
      QMessageBox::critical(this, "Add Folder Error", "The folder '" + metaName + "' could not be added, because a folder with that name already exists.", QMessageBox::Ok, QMessageBox::Ok);
      return;
    }

    if (csdfFilePath.isEmpty() == true)
    {
      campaign->addMetaFolder(metaName, true, csdfFilePath);
    }
    else
    {
      campaign->addMetaFolder(metaName, false, csdfFilePath);
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addSpatialTransform()
{
  QSharedPointer<AddSpatialTransformWidget> addST = QSharedPointer<AddSpatialTransformWidget>(new AddSpatialTransformWidget());
  connect(addST.data(), SIGNAL(originShiftTransformAdded(AddSpatialTransformWidget::STValues, QVector<double>)), this, SLOT(addOriginShiftTransform(AddSpatialTransformWidget::STValues, QVector<double>)));
  connect(addST.data(), SIGNAL(polynomialTransformAdded(AddSpatialTransformWidget::STValues, const QString &, QVector<double>, QVector<double>, QVector<double>)), this, SLOT(addPolynomialTransform(AddSpatialTransformWidget::STValues, const QString &, QVector<double>, QVector<double>, QVector<double>)));
  connect(addST.data(), SIGNAL(strideTransformAdded(AddSpatialTransformWidget::STValues, const QString &, QVector<double>, QVector<int>, QVector<int>)), this, SLOT(addStrideTransform(AddSpatialTransformWidget::STValues, const QString &, QVector<double>, QVector<int>, QVector<int>)));
  connect(addST.data(), SIGNAL(directRemapTransformAdded(AddSpatialTransformWidget::STValues)), this, SLOT(addDirectRemapTransform(AddSpatialTransformWidget::STValues)));
  connect(addST.data(), SIGNAL(rotationTransformAdded(AddSpatialTransformWidget::STValues, QVector<double>)), this, SLOT(addRotationTransform(AddSpatialTransformWidget::STValues, QVector<double>)));
  addST->exec();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addOriginShiftTransform(AddSpatialTransformWidget::STValues values, QVector<double> translation)
{
  CSDFTreeWidgetItem* stFolder = treeWidget->csdfCurrentItem();
  addOriginShiftTransform(values, translation, stFolder);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addOriginShiftTransform(AddSpatialTransformWidget::STValues values, QVector<double> translation, CSDFTreeWidgetItem* stFolder)
{
  if (NULL == stFolder)
  {
    return;
  }

  OriginShiftTreeWidgetItem* osItem = new OriginShiftTreeWidgetItem(translation, NULL, "");
  osItem->setText(0, values.TransformName);
  osItem->setIcon(0, QIcon(":/folder_blue.png"));
  stFolder->insertCSDFChild(stFolder->childCount(), osItem);

  if (treeWidget->isItemExpanded(stFolder) == false)
  {
    treeWidget->expandItem(stFolder);
  }
  
  osItem->addAttribute(IMFViewerProj::AttributeNames::TransformType, values.TransformType, AttributeData::String, true);
  osItem->addAttribute(IMFViewerProj::AttributeNames::TransformOrder, values.TransformOrder, AttributeData::Int, true);
  osItem->addAttribute(IMFViewerProj::AttributeNames::TransformDirection, values.TransformDirection, AttributeData::String, true);

  AddTreeItem(IMFViewerProj::STDatasetNames::Translation, CSDFTreeWidgetItem::Leaf, osItem->childCount(), osItem, NULL, "");

  treeWidget->expandItem(osItem);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addStrideTransform(AddSpatialTransformWidget::STValues values, const QString &units, QVector<double> voxelSize, QVector<int> dimensions, QVector<int> stride)
{
  CSDFTreeWidgetItem* stFolder = treeWidget->csdfCurrentItem();
  addStrideTransform(values, units, voxelSize, dimensions, stride, stFolder);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addStrideTransform(AddSpatialTransformWidget::STValues values, const QString &units, QVector<double> voxelSize, QVector<int> dimensions, QVector<int> stride, CSDFTreeWidgetItem* stFolder)
{
  if (NULL == stFolder)
  {
    return;
  }

  StrideTreeWidgetItem* sItem = new StrideTreeWidgetItem(units, voxelSize, dimensions, stride, NULL, "");
  sItem->setText(0, values.TransformName);
  sItem->setIcon(0, QIcon(":/folder_blue.png"));
  stFolder->insertCSDFChild(stFolder->childCount(), sItem);

  if (treeWidget->isItemExpanded(stFolder) == false)
  {
    treeWidget->expandItem(stFolder);
  }

  sItem->addAttribute(IMFViewerProj::AttributeNames::TransformType, values.TransformType, AttributeData::String, true);
  sItem->addAttribute(IMFViewerProj::AttributeNames::TransformOrder, values.TransformOrder, AttributeData::Int, true);
  sItem->addAttribute(IMFViewerProj::AttributeNames::TransformDirection, values.TransformDirection, AttributeData::String, true);

  AddTreeItem(IMFViewerProj::STDatasetNames::VoxelSize, CSDFTreeWidgetItem::Leaf, sItem->childCount(), sItem, NULL, "");
  AddTreeItem(IMFViewerProj::STDatasetNames::Units, CSDFTreeWidgetItem::Leaf, sItem->childCount(), sItem, NULL, "");
  AddTreeItem(IMFViewerProj::STDatasetNames::Dims, CSDFTreeWidgetItem::Leaf, sItem->childCount(), sItem, NULL, "");
  AddTreeItem(IMFViewerProj::STDatasetNames::Stride, CSDFTreeWidgetItem::Leaf, sItem->childCount(), sItem, NULL, "");

  treeWidget->expandItem(sItem);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addDirectRemapTransform(AddSpatialTransformWidget::STValues values)
{
  CSDFTreeWidgetItem* stFolder = treeWidget->csdfCurrentItem();
  addDirectRemapTransform(values, stFolder);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addDirectRemapTransform(AddSpatialTransformWidget::STValues values, CSDFTreeWidgetItem* stFolder)
{
  if (NULL == stFolder)
  {
    return;
  }

  DirectRemapTreeWidgetItem* drItem = new DirectRemapTreeWidgetItem(NULL, "");
  drItem->setText(0, values.TransformName);
  drItem->setIcon(0, QIcon(":/folder_blue.png"));
  stFolder->insertCSDFChild(stFolder->childCount(), drItem);

  if (treeWidget->isItemExpanded(stFolder) == false)
  {
    treeWidget->expandItem(stFolder);
  }

  drItem->addAttribute(IMFViewerProj::AttributeNames::TransformType, values.TransformType, AttributeData::String, true);
  drItem->addAttribute(IMFViewerProj::AttributeNames::TransformOrder, values.TransformOrder, AttributeData::Int, true);
  drItem->addAttribute(IMFViewerProj::AttributeNames::TransformDirection, values.TransformDirection, AttributeData::String, true);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addPolynomialTransform(AddSpatialTransformWidget::STValues values, const QString &degree, QVector<double> xCoefficients, QVector<double> yCoefficients, QVector<double> zCoefficients)
{
  CSDFTreeWidgetItem* stFolder = treeWidget->csdfCurrentItem();
  addPolynomialTransform(values, degree, xCoefficients, yCoefficients, zCoefficients, stFolder);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addPolynomialTransform(AddSpatialTransformWidget::STValues values, const QString &degree, QVector<double> xCoefficients, QVector<double> yCoefficients, QVector<double> zCoefficients, CSDFTreeWidgetItem* stFolder)
{
  if (NULL == stFolder)
  {
    return;
  }

  PolynomialTreeWidgetItem* pItem = new PolynomialTreeWidgetItem(degree, xCoefficients, yCoefficients, zCoefficients, NULL, "");
  pItem->setText(0, values.TransformName);
  pItem->setIcon(0, QIcon(":/folder_blue.png"));
  stFolder->insertCSDFChild(stFolder->childCount(), pItem);

  if (treeWidget->isItemExpanded(stFolder) == false)
  {
    treeWidget->expandItem(stFolder);
  }

  pItem->addAttribute(IMFViewerProj::AttributeNames::TransformType, values.TransformType, AttributeData::String, true);
  pItem->addAttribute(IMFViewerProj::AttributeNames::TransformOrder, values.TransformOrder, AttributeData::Int, true);
  pItem->addAttribute(IMFViewerProj::AttributeNames::TransformDirection, values.TransformDirection, AttributeData::String, true);

  AddTreeItem(IMFViewerProj::STDatasetNames::PolynomialDeg, CSDFTreeWidgetItem::Leaf, pItem->childCount(), pItem, NULL, "");
  AddTreeItem(IMFViewerProj::STDatasetNames::XCo, CSDFTreeWidgetItem::Leaf, pItem->childCount(), pItem, NULL, "");
  AddTreeItem(IMFViewerProj::STDatasetNames::YCo, CSDFTreeWidgetItem::Leaf, pItem->childCount(), pItem, NULL, "");
  AddTreeItem(IMFViewerProj::STDatasetNames::ZCo, CSDFTreeWidgetItem::Leaf, pItem->childCount(), pItem, NULL, "");

  treeWidget->expandItem(pItem);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addRotationTransform(AddSpatialTransformWidget::STValues values, QVector<double> angle)
{
  CSDFTreeWidgetItem* stFolder = treeWidget->csdfCurrentItem();
  addRotationTransform(values, angle, stFolder);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addRotationTransform(AddSpatialTransformWidget::STValues values, QVector<double> angle, CSDFTreeWidgetItem* stFolder)
{
  if (NULL == stFolder)
  {
    return;
  }

  RotationTreeWidgetItem* rItem = new RotationTreeWidgetItem(angle, NULL, "");
  rItem->setText(0, values.TransformName);
  rItem->setIcon(0, QIcon(":/folder_blue.png"));
  stFolder->insertCSDFChild(stFolder->childCount(), rItem);

  if (treeWidget->isItemExpanded(stFolder) == false)
  {
    treeWidget->expandItem(stFolder);
  }

  rItem->addAttribute(IMFViewerProj::AttributeNames::TransformType, values.TransformType, AttributeData::String, true);
  rItem->addAttribute(IMFViewerProj::AttributeNames::TransformOrder, values.TransformOrder, AttributeData::Int, true);
  rItem->addAttribute(IMFViewerProj::AttributeNames::TransformDirection, values.TransformDirection, AttributeData::String, true);

  AddTreeItem(IMFViewerProj::STDatasetNames::Angle, CSDFTreeWidgetItem::Leaf, rItem->childCount(), rItem, NULL, "");

  treeWidget->expandItem(rItem);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::removeSpatialTransform()
{
  CSDFTreeWidgetItem* item = treeWidget->csdfCurrentItem();
  if (NULL == item)
  {
    return;
  }

  CSDFTreeWidgetItem* parent = item->csdfParent();
  if (NULL == parent)
  {
    return;
  }

  parent->removeCSDFChild(item);
  delete item;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::renameRootItem()
{
  treeWidget->editItem(m_Root, CSDFTreeWidgetItem::Name);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::editRootItem()
{
  QList<QTreeWidgetItem*> itemList = treeWidget->selectedItems();

  if (itemList.size() == 1)
  {
    CSDFTreeWidgetItem* item = dynamic_cast<CSDFTreeWidgetItem*>(itemList[0]);
    if (NULL != item)
    {
      getRootOptionsDialog()->exec();
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::updateCampaigns(QTableWidget* campaignTable)
{
  QMap<QString, RootOptionsDialog::CampaignEntryValues> campaignAvailabilityMap = getRootOptionsDialog()->getCampaignAvailabilityMap();

  QList<QString> childNames = m_Root->childNames();
  for (int i = 0; i < childNames.size(); i++)
  {
    QString childName = childNames[i];
    DataCampaign* campaign = m_DataCampaignMap.value(childName).data();
    if (campaignAvailabilityMap.contains(childName) == false)
    {
      removeCampaign(campaign);
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::editCampaign()
{
  QList<QTreeWidgetItem*> itemList = treeWidget->selectedItems();

  if (itemList.size() == 1)
  {
    CSDFTreeWidgetItem* item = dynamic_cast<CSDFTreeWidgetItem*>(itemList[0]);
    if (NULL != item)
    {
      AbstractOptionsDialog* campaignOptions = m_DataCampaignMap.value(item->text(0))->getCampaignRoot()->getFolderOptionsDialog();
      campaignOptions->exec();

      int result = campaignOptions->result();
      if (result == QDialog::Accepted)
      {
        campaignOptions_applyPressed();
      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::syncTablesToTree(QMap<QString, bool> map, DataCampaign* campaign, const QString &folderType)
{
  CSDFTreeWidgetItem* campaignRoot = campaign->getCampaignRoot();

  for (QMap<QString, bool>::iterator iter = map.begin(); iter != map.end(); ++iter)
  {
    QString name = iter.key();
    bool enabled = iter.value();

    // If the tree does not have the table item
    CSDFTreeWidgetItem* child = campaignRoot->childByName(name);
    if (NULL == child || child->attributeValue(IMFViewerProj::AttributeNames::FolderType) != folderType)
    {
      if (folderType == IMFViewerProj::FolderNames::Raw)
      {
        campaign->addRawFolder(name, false);
      }
      else if (folderType == IMFViewerProj::FolderNames::Reduced)
      {
        campaign->addReducedFolder(name, false);
      }
      else if (folderType == IMFViewerProj::FolderNames::Meta)
      {
        campaign->addMetaFolder(name, false);
      }
    }

    campaign->changeFolderAvailability(name, enabled);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::rootOptions_applyPressed()
{
  QMap<QString, RootOptionsDialog::CampaignEntryValues> campaignMap = getRootOptionsDialog()->getCampaignAvailabilityMap();
  QList<QString> childNames = m_Root->childNames();
  for (int i = 0; i < childNames.size(); i++)
  {
    QString childName = childNames[i];
    if (childName != IMFViewerProj::FolderNames::ST && campaignMap.contains(childName) == false)
    {
      removeCampaign(childName);
    }
  }

  for (QMap<QString, RootOptionsDialog::CampaignEntryValues>::iterator iter = campaignMap.begin(); iter != campaignMap.end(); ++iter)
  {
    QString name = iter.key();
    RootOptionsDialog::CampaignEntryValues values = iter.value();

    // If the tree does not have the table item
    CSDFTreeWidgetItem* child = m_Root->childByName(name);
    if (NULL == child)
    {
      addCampaign(name, values.type, values.insert);
    }

    changeCampaignAvailability(name, values.enabled);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::changeCampaignAvailability(const QString &campaignName, bool available)
{
  CSDFTreeWidgetItem* child = m_Root->childByName(campaignName);
  if (NULL == child)
  {
    return;
  }

  if (available == true)
  {
    m_Root->insertChild(m_Root->childCount(), child);
  }
  else
  {
    // We want to remove the child from the visible tree, but not from the root's internal list of children
    m_Root->removeChild(child);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::campaignOptions_applyPressed()
{
  QList<QTreeWidgetItem*> itemList = treeWidget->selectedItems();

  if (itemList.size() == 1)
  {
    CSDFTreeWidgetItem* item = dynamic_cast<CSDFTreeWidgetItem*>(itemList[0]);
    if (NULL != item)
    {
      CampaignOptionsDialog* campaignOptions = m_DataCampaignMap.value(item->text(0))->getCampaignOptionsDialog();

      DataCampaign* campaign = m_DataCampaignMap.value(campaignOptions->getCampaignName()).data();
      CSDFTreeWidgetItem* campaignRoot = campaign->getCampaignRoot();
      QMap<QString, bool> rawAvailabilityMap = campaignOptions->getRawFolderAvailabilityMap();
      QMap<QString, bool> reducedAvailabilityMap = campaignOptions->getReducedFolderAvailabilityMap();
      QMap<QString, bool> metaAvailabilityMap = campaignOptions->getMetaFolderAvailabilityMap();

      QList<QString> childNames = campaignRoot->childNames();
      for (int i = 0; i < childNames.size(); i++)
      {
        QString childName = childNames[i];
        if (campaignRoot->childByName(childName)->attributeValue(IMFViewerProj::AttributeNames::FolderType) == IMFViewerProj::FolderNames::Raw && rawAvailabilityMap.contains(childName) == false)
        {
          campaign->removeRawFolder(childName);
        }
        else if (campaignRoot->childByName(childName)->attributeValue(IMFViewerProj::AttributeNames::FolderType) == IMFViewerProj::FolderNames::Reduced && reducedAvailabilityMap.contains(childName) == false)
        {
          campaign->removeReducedFolder(childName);
        }
        else if (campaignRoot->childByName(childName)->attributeValue(IMFViewerProj::AttributeNames::FolderType) == IMFViewerProj::FolderNames::Meta && metaAvailabilityMap.contains(childName) == false)
        {
          campaign->removeMetaFolder(childName);
        }
      }

      syncTablesToTree(rawAvailabilityMap, campaign, IMFViewerProj::FolderNames::Raw);
      syncTablesToTree(reducedAvailabilityMap, campaign, IMFViewerProj::FolderNames::Reduced);
      syncTablesToTree(metaAvailabilityMap, campaign, IMFViewerProj::FolderNames::Meta);
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::editRawFolder()
{
  QList<QTreeWidgetItem*> itemList = treeWidget->selectedItems();

  if (itemList.size() == 1)
  {
    CSDFTreeWidgetItem* raw = dynamic_cast<CSDFTreeWidgetItem*>(itemList[0]);
    if (NULL != raw)
    {
      AbstractOptionsDialog* rawOptions = raw->getFolderOptionsDialog();
      if (NULL != rawOptions)
      {
        rawOptions->exec();
      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::editReducedFolder()
{
  QList<QTreeWidgetItem*> itemList = treeWidget->selectedItems();

  if (itemList.size() == 1)
  {
    CSDFTreeWidgetItem* reduced = dynamic_cast<CSDFTreeWidgetItem*>(itemList[0]);
    if (NULL != reduced)
    {
      AbstractOptionsDialog* reducedOptions = reduced->getFolderOptionsDialog();
      if (NULL != reducedOptions)
      {
        reducedOptions->exec();
      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::editMetaFolder()
{
  QList<QTreeWidgetItem*> itemList = treeWidget->selectedItems();

  if (itemList.size() == 1)
  {
    CSDFTreeWidgetItem* meta = dynamic_cast<CSDFTreeWidgetItem*>(itemList[0]);
    if (NULL != meta)
    {
      AbstractOptionsDialog* metaOptions = meta->getFolderOptionsDialog();
      if (NULL != metaOptions)
      {
        metaOptions->exec();
      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::on_treeWidget_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
  if (NULL != current)
  {
    CSDFTreeWidgetItem* currentItem = dynamic_cast<CSDFTreeWidgetItem*>(current);
    if (NULL != currentItem)
    {
      attributesTable->clearContents();
      attributesTable->setRowCount(0);

      // Populate the table with the current item's attributes
      QVector<AttributeData> attributes = currentItem->getAttributes();
      for (int i = 0; i < attributes.size(); i++)
      {
        if (isRunning() == true)
        {
          addAttribute(attributes[i].name, attributes[i].value.toString(), true);
        }
        else
        {
          addAttribute(attributes[i].name, attributes[i].value.toString(), attributes[i].readOnly);
        }
      }

      if (attributesTable->isEnabled() == false)
      {
        attributesTable->setEnabled(true);
      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::on_attributesTable_itemChanged(QTableWidgetItem* tableItem)
{
  // Store the values from the table into the previous item
  if (NULL != tableItem)
  {
    QTableWidgetItem* nameItem;
    QTableWidgetItem* valueItem;
    if (tableItem->column() == Name)
    {
      nameItem = tableItem;
      valueItem = attributesTable->item(tableItem->row(), Value);
    }
    else
    {
      nameItem = attributesTable->item(tableItem->row(), Name);
      valueItem = tableItem;
    }

    CSDFTreeWidgetItem* treeItem = dynamic_cast<CSDFTreeWidgetItem*>(treeWidget->currentItem());
    if (NULL != treeItem)
    {
      QVector<AttributeData> attributes = treeItem->getAttributes();
      if (tableItem->row() < attributes.size())
      {
        QVector<AttributeData> attributes = treeItem->getAttributes();
        bool readOnly = attributes[tableItem->row()].readOnly;
        AttributeData::DataType type = attributes[tableItem->row()].type;

        if (NULL == nameItem && NULL == valueItem)
        {
          treeItem->updateAttribute(tableItem->row(), "", "", type, readOnly);
        }
        else if (NULL == nameItem)
        {
          treeItem->updateAttribute(tableItem->row(), "", valueItem->text(), type, readOnly);
        }
        else if (NULL == valueItem)
        {
          treeItem->updateAttribute(tableItem->row(), nameItem->text(), "", type, readOnly);
        }
        else
        {
          treeItem->updateAttribute(tableItem->row(), nameItem->text(), valueItem->text(), type, readOnly);
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::addAttribute(QString name, QString value, bool readOnly)
{
  int row = attributesTable->rowCount();
  attributesTable->insertRow(row);

  QTableWidgetItem* nameItem = new QTableWidgetItem(name);
  nameItem->setTextAlignment(Qt::AlignCenter);
  nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
  attributesTable->setItem(row, Name, nameItem);

  QTableWidgetItem* valueItem = new QTableWidgetItem(value);
  valueItem->setTextAlignment(Qt::AlignCenter);
  attributesTable->setItem(row, Value, valueItem);

  if (readOnly == true)
  {
    valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
  }
  else
  {
    valueItem->setFlags(valueItem->flags() | Qt::ItemIsEditable);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::removeAttribute(int row)
{
  attributesTable->removeRow(row);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::treeWidgetCustomContextMenuRequested(const QPoint& pos)
{
  // Delete the old menu and return all memory used by old QActions
  delete m_Menu;

  // Allocate memory for a new menu
  m_Menu = new QMenu(this);

  // Build another menu
  CSDFTreeWidgetItem* item = dynamic_cast<CSDFTreeWidgetItem*>(treeWidget->itemAt(pos));
  bool showMenu = true;
  if (NULL != item)
  {
    if (isRootItem(item) == true)
    {
      QAction* addCampaignAction = new QAction("Add Campaign", m_Menu);
      connect(addCampaignAction, SIGNAL(triggered()), this, SLOT(addCampaign()));
      m_Menu->addAction(addCampaignAction);
      {
        QAction* separator = new QAction(this);
        separator->setSeparator(true);
        m_Menu->addAction(separator);
      }
      QAction* editRootAction = new QAction("Edit '" + item->text(CSDFTreeWidgetItem::Name) + "'", m_Menu);
      connect(editRootAction, SIGNAL(triggered()), this, SLOT(editRootItem()));
      m_Menu->addAction(editRootAction);
      {
        QAction* separator = new QAction(this);
        separator->setSeparator(true);
        m_Menu->addAction(separator);
      }
      QAction* renameRootAction = new QAction("Rename '" + item->text(CSDFTreeWidgetItem::Name) + "'", m_Menu);
      connect(renameRootAction, SIGNAL(triggered()), this, SLOT(renameRootItem()));
      m_Menu->addAction(renameRootAction);
    }
    else if (isCampaignItem(item) == true)
    {
      QAction* addRawAction = new QAction("Add Raw Folder", m_Menu);
      connect(addRawAction, SIGNAL(triggered()), this, SLOT(addRawFolder()));
      m_Menu->addAction(addRawAction);
      QAction* addReducedAction = new QAction("Add Reduced Folder", m_Menu);
      connect(addReducedAction, SIGNAL(triggered()), this, SLOT(addReducedFolder()));
      m_Menu->addAction(addReducedAction);
      QAction* addMetaAction = new QAction("Add Meta Folder", m_Menu);
      connect(addMetaAction, SIGNAL(triggered()), this, SLOT(addMetaFolder()));
      m_Menu->addAction(addMetaAction);
      {
        QAction* separator = new QAction(this);
        separator->setSeparator(true);
        m_Menu->addAction(separator);
      }
      QAction* editCampaignAction = new QAction("Edit '" + item->text(CSDFTreeWidgetItem::Name) + "'", m_Menu);
      connect(editCampaignAction, SIGNAL(triggered()), this, SLOT(editCampaign()));
      m_Menu->addAction(editCampaignAction);
      {
        QAction* separator = new QAction(this);
        separator->setSeparator(true);
        m_Menu->addAction(separator);
      }
      QAction* removeCampaignAction = new QAction("Remove '" + item->text(CSDFTreeWidgetItem::Name) + "'", m_Menu);
      QSignalMapper* mapper = new QSignalMapper(m_Menu);
      connect(removeCampaignAction, SIGNAL(triggered()), mapper, SLOT(map()));
      mapper->setMapping(removeCampaignAction, item->text(CSDFTreeWidgetItem::Name));
      connect(mapper, SIGNAL(mapped(const QString&)), this, SLOT(removeCampaign(const QString&)));
      m_Menu->addAction(removeCampaignAction);
    }
    else if (isRawItem(item) == true)
    {
      QAction* editRawAction = new QAction("Edit '" + item->text(CSDFTreeWidgetItem::Name) + "'", m_Menu);
      connect(editRawAction, SIGNAL(triggered()), this, SLOT(editRawFolder()));
      if (item->fromCSDFFile() == true)
      {
        editRawAction->setDisabled(true);
      }
      m_Menu->addAction(editRawAction);
      {
        QAction* separator = new QAction(this);
        separator->setSeparator(true);
        m_Menu->addAction(separator);
      }
      DataCampaign* campaign = dataCampaign(item);
      QAction* removeRawAction = new QAction("Remove '" + item->text(CSDFTreeWidgetItem::Name) + "'", m_Menu);
      QSignalMapper* mapper = new QSignalMapper(m_Menu);
      connect(removeRawAction, SIGNAL(triggered()), mapper, SLOT(map()));
      mapper->setMapping(removeRawAction, item->text(CSDFTreeWidgetItem::Name));
      connect(mapper, SIGNAL(mapped(const QString&)), campaign, SLOT(removeRawFolder(const QString&)));
      m_Menu->addAction(removeRawAction);
    }
    else if (isReducedItem(item) == true)
    {
      QAction* editReducedAction = new QAction("Edit '" + item->text(CSDFTreeWidgetItem::Name) + "'", m_Menu);
      connect(editReducedAction, SIGNAL(triggered()), this, SLOT(editReducedFolder()));
      if (item->fromCSDFFile() == true)
      {
        editReducedAction->setDisabled(true);
      }
      m_Menu->addAction(editReducedAction);
      {
        QAction* separator = new QAction(this);
        separator->setSeparator(true);
        m_Menu->addAction(separator);
      }
      DataCampaign* campaign = dataCampaign(item);
      QAction* removeReducedAction = new QAction("Remove '" + item->text(CSDFTreeWidgetItem::Name) + "'", m_Menu);
      QSignalMapper* mapper = new QSignalMapper(m_Menu);
      connect(removeReducedAction, SIGNAL(triggered()), mapper, SLOT(map()));
      mapper->setMapping(removeReducedAction, item->text(CSDFTreeWidgetItem::Name));
      connect(mapper, SIGNAL(mapped(const QString&)), campaign, SLOT(removeReducedFolder(const QString&)));
      m_Menu->addAction(removeReducedAction);
    }
    else if (isMetaItem(item) == true)
    {
      QAction* editMetaAction = new QAction("Edit '" + item->text(CSDFTreeWidgetItem::Name) + "'", m_Menu);
      connect(editMetaAction, SIGNAL(triggered()), this, SLOT(editMetaFolder()));
      if (item->fromCSDFFile() == true)
      {
        editMetaAction->setDisabled(true);
      }
      m_Menu->addAction(editMetaAction);
      {
        QAction* separator = new QAction(this);
        separator->setSeparator(true);
        m_Menu->addAction(separator);
      }
      DataCampaign* campaign = dataCampaign(item);
      QAction* removeMetaAction = new QAction("Remove '" + item->text(CSDFTreeWidgetItem::Name) + "'", m_Menu);
      QSignalMapper* mapper = new QSignalMapper(m_Menu);
      connect(removeMetaAction, SIGNAL(triggered()), mapper, SLOT(map()));
      mapper->setMapping(removeMetaAction, item->text(CSDFTreeWidgetItem::Name));
      connect(mapper, SIGNAL(mapped(const QString&)), campaign, SLOT(removeMetaFolder(const QString&)));
      m_Menu->addAction(removeMetaAction);
    }
    else if (isSTItem(item) == true)
    {
      QAction* addSTAction = new QAction("Add Spatial Transform", m_Menu);
      connect(addSTAction, SIGNAL(triggered()), this, SLOT(addSpatialTransform()));
      m_Menu->addAction(addSTAction);
    }
    else if (isSTDataset(item) == true)
    {
      QAction* removeSTAction = new QAction("Remove \"" + item->text(CSDFTreeWidgetItem::Name) + "\"", m_Menu);
      connect(removeSTAction, SIGNAL(triggered()), this, SLOT(removeSpatialTransform()));
      m_Menu->addAction(removeSTAction);
    }
    else
    {
      showMenu = false;
    }
  }
  else
  {
    m_Menu->addAction(actionClearFileStructure);
  }

  if (showMenu == true)
  {
    if (isRunning() == true)
    {
      m_Menu->setDisabled(true);
    }
    else
    {
      m_Menu->setEnabled(true);
    }

    // Execute the menu
    m_Menu->exec(treeWidget->viewport()->mapToGlobal(pos));
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::on_actionClearFileStructure_triggered()
{
  QMutableMapIterator<QString, QSharedPointer<DataCampaign> > iter(m_DataCampaignMap);
  while (iter.hasNext())
  {
    iter.next();

    QString campaignName = iter.key();
    removeCampaign(campaignName);
  }

  if (NULL != m_Root)
  {
    delete m_Root;
    m_Root = NULL;
  }

  attributesTable->clearContents();
  attributesTable->setRowCount(0);
  attributesTable->setDisabled(true);

  menuCampaign->setDisabled(true);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer::isRootItem(CSDFTreeWidgetItem* item)
{
  if (NULL == item)
  {
    return false;
  }

  if (item->hasAttribute(IMFViewerProj::AttributeNames::PartID))
  {
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer::isCampaignItem(CSDFTreeWidgetItem* item)
{
  if (NULL == item)
  {
    return false;
  }

  if (item->hasAttribute(IMFViewerProj::AttributeNames::CampaignType))
  {
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer::isRawItem(CSDFTreeWidgetItem* item)
{
  if (item->hasAttribute(IMFViewerProj::AttributeNames::FolderType) && item->attributeValue(IMFViewerProj::AttributeNames::FolderType).canConvert<QString>() && item->attributeValue(IMFViewerProj::AttributeNames::FolderType).toString() == IMFViewerProj::FolderNames::Raw)
  {
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer::isReducedItem(CSDFTreeWidgetItem* item)
{
  if (item->hasAttribute(IMFViewerProj::AttributeNames::FolderType) && item->attributeValue(IMFViewerProj::AttributeNames::FolderType).canConvert<QString>() && item->attributeValue(IMFViewerProj::AttributeNames::FolderType).toString() == IMFViewerProj::FolderNames::Reduced)
  {
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer::isMetaItem(CSDFTreeWidgetItem* item)
{
  if (item->hasAttribute(IMFViewerProj::AttributeNames::FolderType) && item->attributeValue(IMFViewerProj::AttributeNames::FolderType).canConvert<QString>() && item->attributeValue(IMFViewerProj::AttributeNames::FolderType).toString() == IMFViewerProj::FolderNames::Meta)
  {
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer::isSTItem(CSDFTreeWidgetItem* item)
{
  if (NULL == item)
  {
    return false;
  }

  if (item->getObjectType() == CSDFTreeWidgetItem::STFolder)
  {
    return true;
  }
  else
  {
    return false;
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer::isSTDataset(CSDFTreeWidgetItem* item)
{
  if (NULL == item)
  {
    return false;
  }

  CSDFTreeWidgetItem* parent = item->csdfParent();
  if (NULL == parent)
  {
    return false;
  }

  if (isSTItem(parent) && item->getObjectType() == CSDFTreeWidgetItem::Node)
  {
    return true;
  }
  else
  {
    return false;
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::updateProgressBar(int value)
{
  progressBar->setValue(value);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::updateStatusBar(const QString &msg, IMFViewer::IMFViewerState state)
{
  toState(state);
  statusbar->showMessage(msg);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer::isRunning()
{
  if (NULL != m_WorkerThread && m_WorkerThread->isRunning())
  {
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer::hasRootItem()
{
  if (NULL != m_Root)
  {
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::createRootItem(const QString &fileName, const int &partId, const QString &csdfFilePath)
{
  RootOptionsDialog* rootOptions = new RootOptionsDialog(this);
  connect(rootOptions, SIGNAL(applyBtnPressed()), this, SLOT(rootOptions_applyPressed()));
  m_Root = AddTreeItem(fileName, CSDFTreeWidgetItem::Root, -1, NULL, rootOptions, csdfFilePath);
  m_Root->addAttribute(IMFViewerProj::AttributeNames::PartID, partId, AttributeData::Int, false);
  treeWidget->addTopLevelItem(m_Root);
  m_Root->setExpanded(true);

  m_STFolder = IMFViewer::AddTreeItem(IMFViewerProj::FolderNames::ST, CSDFTreeWidgetItem::STFolder, m_Root->childCount(), m_Root);

  attributesTable->setEnabled(true);

  QString outputFilePath = outputFile->text();
  QFileInfo fi(outputFilePath);
  if (outputFile->text().isEmpty() == false && fi.completeSuffix() == "csdf")
  {
    goBtn->setEnabled(true);
  }

  treeWidget->setCurrentItem(m_Root);
  treeWidget->setFocus();

  menuCampaign->setEnabled(true);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::openCSDFFile(const QString &filePath)
{
  QFileInfo fi(filePath);
  hid_t fileId = QH5Utilities::openFile(filePath);
  if (fileId < 0)
  {
    showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCheck permissions or if the file is in use by another program.");
    return;
  }

  HDF5ScopedFileSentinel sentinel(&fileId, true);

  QList<QString> groupList;
  int err = QH5Utilities::getGroupObjects(fileId, H5Utilities::H5Support_ANY, groupList);
  if (err < 0)
  {
    showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not retrieve root item.");
    return;
  }

  if (groupList.size() < 1)
  {
    showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nUnable to locate root item.");
    return;
  }
  else if (groupList.size() > 1)
  {
    showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nThis is a non-standard CSDF file, and cannot be opened using this tool.");
    return;
  }

  QString rootName = groupList[0];
  int partID = 0;

  err = QH5Lite::readScalarAttribute(fileId, rootName, IMFViewerProj::AttributeNames::PartID, partID);
  if (err < 0)
  {
    showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nUnable to locate the file's Part ID.");
    return;
  }
  
  // Create the root item
  createRootItem(rootName, partID, filePath);

  hid_t rootId = H5Gopen(fileId, rootName.toStdString().c_str(), H5P_DEFAULT);
  if (rootId < 0)
  {
    showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nThere was an error opening the root item.");
    return;
  }
  sentinel.addGroupId(&rootId);

  QList<QString> campaignList;
  err = QH5Utilities::getGroupObjects(rootId, H5Utilities::H5Support_ANY, campaignList);
  if (err < 0)
  {
    showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not retrieve groups from the root item.");
    return;
  }

  hid_t stFolderId = H5Gopen(rootId, m_STFolder->text(CSDFTreeWidgetItem::Name).toStdString().c_str(), H5P_DEFAULT);
  if (stFolderId < 0)
  {
    showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nThere was an error opening the root-level Spatial Transforms folder.");
    return;
  }

  // Open the Spatial Transforms folder
  readSpatialTransforms(stFolderId, m_STFolder);

  // Add the campaigns
  for (int i = 0; i < campaignList.size(); i++)
  {
    QString possibleCampaignName = campaignList[i];

    if (possibleCampaignName != IMFViewerProj::FolderNames::ST)
    {
      QString campaignName = possibleCampaignName;
      QString campaignType;
      err = QH5Lite::readStringAttribute(rootId, campaignName, IMFViewerProj::AttributeNames::CampaignType, campaignType);
      if (err < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read '" + campaignName + "' campaign type.");
        return;
      }

      hid_t campaignId = H5Gopen(rootId, campaignName.toStdString().c_str(), H5P_DEFAULT);
      if (campaignId < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nThere was an error opening the campaign item.");
        return;
      }

      HDF5ScopedGroupSentinel sentinel(&campaignId, true);

      DataCampaign* campaign = addCampaign(campaignName, campaignType, m_DataCampaignMap.count(), filePath);
      connect(campaign, SIGNAL(spatialTransformDataNeeded(hid_t, CSDFTreeWidgetItem*)), this, SLOT(readSpatialTransforms(hid_t, CSDFTreeWidgetItem*)));
      err = campaign->openDataCampaign(campaignId, filePath);

      if (err < 0)
      {
        // Store the error message before we delete the campaign...
        QString errMsg = campaign->getErrorMessage();

        m_DataCampaignMap.clear();

        attributesTable->clearContents();
        while (attributesTable->rowCount() > 0)
        {
          attributesTable->removeRow(0);
        }

        delete m_Root;
        m_Root = NULL;

        showErrorMessage(errMsg);
        return;
      }
    }
  }

  // Store the CSDF source path
  m_CSDFSourcePath = filePath;

  if (statusbar->styleSheet().isEmpty() == false)
  {
    statusbar->setStyleSheet("");
  }

  statusbar->showMessage("The CSDF file '" + fi.fileName() + "' was opened successfully.");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
template<typename T>
QVector<T> IMFViewer::toVector(std::vector<T> stdVector)
{
  QVector<T> vector;
  for (int i = 0; i < stdVector.size(); i++)
  {
    vector.push_back(stdVector[i]);
  }

  return vector;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::readSpatialTransforms(hid_t stFolderId, CSDFTreeWidgetItem* stFolder)
{
  QFileInfo fi(stFolder->getCSDFFilePath());
  int err = 0;

  QList<QString> transformGroups;
  err = QH5Utilities::getGroupObjects(stFolderId, H5Utilities::H5Support_GROUP, transformGroups);
  if (err < 0)
  {
    showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not retrieve groups from the Spatial Transforms folder at path \"" + QH5Utilities::getObjectPath(stFolderId) + "\".");
    return;
  }

  for (int i = 0; i < transformGroups.size(); i++)
  {
    AddSpatialTransformWidget::STValues values;

    QString transformGroupName = transformGroups[i];
    values.TransformName = transformGroupName;

    QString transformType;
    err = QH5Lite::readStringAttribute(stFolderId, transformGroupName, IMFViewerProj::AttributeNames::TransformType, transformType);
    if (err < 0)
    {
      showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::AttributeNames::TransformType + "\" attribute from Spatial Transforms folder at path \"" + QH5Utilities::getObjectPath(stFolderId) + "/" + transformGroupName + "\".");
      return;
    }
    values.TransformType = transformType;

    int transformOrder;
    err = QH5Lite::readScalarAttribute(stFolderId, transformGroupName, IMFViewerProj::AttributeNames::TransformOrder, transformOrder);
    if (err < 0)
    {
      showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::AttributeNames::TransformOrder + "\" attribute from Spatial Transforms folder at path \"" + QH5Utilities::getObjectPath(stFolderId) + "/" + transformGroupName + "\".");
      return;
    }
    values.TransformOrder = transformOrder;

    QString transformDirection;
    err = QH5Lite::readStringAttribute(stFolderId, transformGroupName, IMFViewerProj::AttributeNames::TransformDirection, transformDirection);
    if (err < 0)
    {
      showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::AttributeNames::TransformDirection + "\" attribute from Spatial Transforms folder at path \"" + QH5Utilities::getObjectPath(stFolderId) + "/" + transformGroupName + "\".");
      return;
    }
    values.TransformDirection = transformDirection;

    hid_t stId = H5Gopen(stFolderId, transformGroupName.toStdString().c_str(), H5P_DEFAULT);
    if (stId < 0)
    {
      showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nThere was an error opening the Spatial Transform at path \"" + QH5Utilities::getObjectPath(stFolderId) + "/" + transformGroupName + "\".");
      return;
    }
    HDF5ScopedGroupSentinel stSentinel(&stId, true);

    if (transformType == IMFViewerProj::STTypes::OriginShift)
    {
      std::vector<double> translationData;
      err = QH5Lite::readVectorDataset(stId, IMFViewerProj::STDatasetNames::Translation, translationData);
      if (err < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::STDatasetNames::Translation + "\" dataset from Spatial Transform at path \"" + QH5Utilities::getObjectPath(stId) + "\".");
        return;
      }
      QVector<double> translation = toVector(translationData);

      addOriginShiftTransform(values, translation, stFolder);
    }
    else if (transformType == IMFViewerProj::STTypes::Polynomial)
    {
      std::vector<double> xCoefficientData;
      err = QH5Lite::readVectorDataset(stId, IMFViewerProj::STDatasetNames::XCo, xCoefficientData);
      if (err < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::STDatasetNames::XCo + "\" dataset from Spatial Transform at path \"" + QH5Utilities::getObjectPath(stId) + "\".");
        return;
      }
      QVector<double> xCoefficient = toVector(xCoefficientData);

      std::vector<double> yCoefficientData;
      err = QH5Lite::readVectorDataset(stId, IMFViewerProj::STDatasetNames::YCo, yCoefficientData);
      if (err < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::STDatasetNames::YCo + "\" dataset from Spatial Transform at path \"" + QH5Utilities::getObjectPath(stId) + "\".");
        return;
      }
      QVector<double> yCoefficient = toVector(yCoefficientData);

      std::vector<double> zCoefficientData;
      err = QH5Lite::readVectorDataset(stId, IMFViewerProj::STDatasetNames::ZCo, zCoefficientData);
      if (err < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::STDatasetNames::ZCo + "\" dataset from Spatial Transform at path \"" + QH5Utilities::getObjectPath(stId) + "\".");
        return;
      }
      QVector<double> zCoefficient = toVector(zCoefficientData);

      QString polynomialDegree;
      err = QH5Lite::readStringDataset(stId, IMFViewerProj::STDatasetNames::PolynomialDeg, polynomialDegree);
      if (err < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::STDatasetNames::PolynomialDeg + "\" dataset from Spatial Transform at path \"" + QH5Utilities::getObjectPath(stId) + "\".");
        return;
      }

      addPolynomialTransform(values, polynomialDegree, xCoefficient, yCoefficient, zCoefficient, stFolder);
    }
    else if (transformType == IMFViewerProj::STTypes::Rotation)
    {
      std::vector<double> angleData;
      err = QH5Lite::readVectorDataset(stId, IMFViewerProj::STDatasetNames::Angle, angleData);
      if (err < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::STDatasetNames::Angle + "\" dataset from Spatial Transform at path \"" + QH5Utilities::getObjectPath(stId) + "\".");
        return;
      }
      QVector<double> angle = toVector(angleData);

      addRotationTransform(values, angle, stFolder);
    }
    else if (transformType == IMFViewerProj::STTypes::Stride)
    {
      QString units;
      err = QH5Lite::readStringDataset(stId, IMFViewerProj::STDatasetNames::Units, units);
      if (err < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::STDatasetNames::Units + "\" dataset from Spatial Transform at path \"" + QH5Utilities::getObjectPath(stId) + "\".");
        return;
      }

      std::vector<double> voxelSizeData;
      err = QH5Lite::readVectorDataset(stId, IMFViewerProj::STDatasetNames::VoxelSize, voxelSizeData);
      if (err < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::STDatasetNames::VoxelSize + "\" dataset from Spatial Transform at path \"" + QH5Utilities::getObjectPath(stId) + "\".");
        return;
      }
      QVector<double> voxelSize = toVector(voxelSizeData);

      std::vector<int> dimsData;
      err = QH5Lite::readVectorDataset(stId, IMFViewerProj::STDatasetNames::Dims, dimsData);
      if (err < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::STDatasetNames::Dims + "\" dataset from Spatial Transform at path \"" + QH5Utilities::getObjectPath(stId) + "\".");
        return;
      }
      QVector<int> dimensions = toVector(dimsData);

      std::vector<int> strideData;
      err = QH5Lite::readVectorDataset(stId, IMFViewerProj::STDatasetNames::Stride, strideData);
      if (err < 0)
      {
        showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nCould not read \"" + IMFViewerProj::STDatasetNames::Stride + "\" dataset from Spatial Transform at path \"" + QH5Utilities::getObjectPath(stId) + "\".");
        return;
      }
      QVector<int> stride = toVector(strideData);

      addStrideTransform(values, units, voxelSize, dimensions, stride, stFolder);
    }
    else if (transformType == IMFViewerProj::STTypes::DirectRemap)
    {
      addDirectRemapTransform(values, stFolder);
    }
    else
    {
      showErrorMessage("The CSDF file '" + fi.fileName() + "' could not be opened.\nSpatial Transform at path \"" + QH5Utilities::getObjectPath(stId) + "\" has an invalid Spatial Transform type, \"" + transformType + "\".");
      return;
    }
  }

  QH5Utilities::closeHDF5Object(stFolderId);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::importCSDFFiles(QList<QString> fileList)
{
  for (int i = 0; i < fileList.size(); i++)
  {
    QString filePath = fileList[i];

    // If this an empty instance, open the first file's contents into the instance
    if (NULL == m_Root)
    {
      openCSDFFile(filePath);
      continue;
    }
    
    int origPartId = m_Root->attributeValue(IMFViewerProj::AttributeNames::PartID).toInt();

    QFileInfo fi(filePath);
    hid_t fileId = QH5Utilities::openFile(filePath);
    if (fileId < 0)
    {
      statusBar()->showMessage("The CSDF file '" + fi.fileName() + "' could not be imported. Check permissions or if the file is in use by another program.");
      return;
    }

    HDF5ScopedFileSentinel sentinel(&fileId, true);

    QList<QString> groupList;
    int err = QH5Utilities::getGroupObjects(fileId, H5Utilities::H5Support_ANY, groupList);
    if (err < 0)
    {
      statusBar()->showMessage("The CSDF file '" + fi.fileName() + "' could not be imported. Could not retrieve groups from '" + fi.fileName() + "'.");
      return;
    }

    if (groupList.size() != 1)
    {
      statusBar()->showMessage("The CSDF file '" + fi.fileName() + "' could not be imported. There is not one root item.");
      return;
    }

    QString rootName = groupList[0];
    int partID = 0;

    err = QH5Lite::readScalarAttribute(fileId, rootName, IMFViewerProj::AttributeNames::PartID, partID);
    if (err < 0)
    {
      statusBar()->showMessage("The CSDF file '" + fi.fileName() + "' could not be imported. There was an error reading the Part ID.");
      return;
    }

    if (partID != origPartId)
    {
      QFileInfo sourceFi(m_CSDFSourcePath);
      QMessageBox::critical(this, "Import Error", "The CSDF file '" + fi.fileName() + "' could not be imported.\n'" + fi.fileName() + "' does not have the same part ID as the root item.", QMessageBox::Ok, QMessageBox::Ok);
      continue;
    }

    importCSDFFile(filePath);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::importCSDFFile(QString filePath)
{
  QFileInfo fi(filePath);
  QFileInfo sourceFi(m_CSDFSourcePath);
  hid_t fileId = QH5Utilities::openFile(filePath);
  if (fileId < 0)
  {
    statusbar->showMessage("The CSDF file '" + fi.fileName() + "' could not be imported. Check permissions or if the file is in use by another program.");
    return;
  }

  HDF5ScopedFileSentinel sentinel(&fileId, true);

  QList<QString> rootList;
  int err = QH5Utilities::getGroupObjects(fileId, H5Utilities::H5Support_ANY, rootList);
  if (err < 0)
  {
    statusbar->showMessage("The CSDF file '" + fi.fileName() + "' could not be imported. Could not retrieve groups from '" + fi.fileName() + "'.");
    return;
  }

  if (rootList.size() != 1)
  {
    statusbar->showMessage("The CSDF file '" + fi.fileName() + "' could not be imported. There is not one root item.");
    return;
  }

  QString rootName = rootList[0];

  hid_t rootId = H5Gopen(fileId, rootName.toStdString().c_str(), H5P_DEFAULT);
  if (rootId < 0)
  {
    statusbar->showMessage("The CSDF file '" + fi.fileName() + "' could not be imported. There was an error opening the root item.");
    return;
  }
  sentinel.addGroupId(&rootId);

  QList<QString> campaignList;
  err = QH5Utilities::getGroupObjects(rootId, H5Utilities::H5Support_ANY, campaignList);
  if (err < 0)
  {
    statusbar->showMessage("The CSDF file '" + fi.fileName() + "' could not be imported. Could not retrieve groups from '" + fi.fileName() + "'.");
    return;
  }

  // Add the campaigns
  for (int i = 0; i < campaignList.size(); i++)
  {
    QString campaignName = campaignList[i];
    int index = m_DataCampaignMap.count();

    if (campaignName != IMFViewerProj::FolderNames::ST)
    {
      if (NULL != m_Root->childByName(campaignName))
      {
        QMessageBox warningBox(this);
        warningBox.addButton("Replace This Campaign", QMessageBox::AcceptRole);
        warningBox.addButton("Skip This Campaign", QMessageBox::RejectRole);
        warningBox.setIcon(QMessageBox::Warning);
        warningBox.setWindowTitle("Merging \"" + sourceFi.fileName() + "\" and \"" + fi.fileName() + "\"");
        warningBox.setText("The CSDF file \"" + sourceFi.fileName() + "\" already contains a campaign named \"" + campaignName + "\".");
        int result = warningBox.exec();

        if (result == QMessageBox::AcceptRole)
        {
          // Replace the campaign
          CSDFTreeWidgetItem* campaignItem = m_Root->childByName(campaignName);
          index = m_Root->indexOfChild(campaignItem);
          removeCampaign(campaignName);
        }
        else
        {
          // Skip this campaign
          continue;
        }
      }

      QString campaignType;
      err = QH5Lite::readStringAttribute(rootId, campaignName, IMFViewerProj::AttributeNames::CampaignType, campaignType);
      if (err < 0)
      {
        statusbar->showMessage("The CSDF file '" + fi.fileName() + "' could not be imported. Could not read '" + campaignName + "' campaign type.");
        return;
      }

      hid_t campaignId = H5Gopen(rootId, campaignName.toStdString().c_str(), H5P_DEFAULT);
      if (campaignId < 0)
      {
        statusbar->showMessage("The CSDF file '" + fi.fileName() + "' could not be imported. There was an error opening the campaign item.");
        return;
      }

      HDF5ScopedGroupSentinel sentinel(&campaignId, true);

      DataCampaign* campaign = addCampaign(campaignName, campaignType, index, filePath);
      connect(campaign, SIGNAL(spatialTransformDataNeeded(hid_t, CSDFTreeWidgetItem*)), this, SLOT(readSpatialTransforms(hid_t, CSDFTreeWidgetItem*)));
      err = campaign->openDataCampaign(campaignId, filePath);

      if (err < 0)
      {
        m_DataCampaignMap.clear();

        attributesTable->clearContents();
        while (attributesTable->rowCount() > 0)
        {
          attributesTable->removeRow(0);
        }
        return;
      }
    }
  }

  statusbar->showMessage("The CSDF file '" + fi.fileName() + "' was imported successfully.");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataCampaign* IMFViewer::dataCampaign(CSDFTreeWidgetItem* item)
{
  if (NULL == item)
  {
    return NULL;
  }
  else if (isCampaignItem(item))
  {
    return m_DataCampaignMap.value(item->text(CSDFTreeWidgetItem::Name)).data();
  }

  return dataCampaign(item->csdfParent());
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
RootOptionsDialog* IMFViewer::getRootOptionsDialog()
{
  return dynamic_cast<RootOptionsDialog*>(m_Root->getFolderOptionsDialog());
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::showErrorMessage(const QString msg)
{
  QMessageBox::critical(NULL, "IMFViewer Error", msg, QMessageBox::Ok, QMessageBox::Ok);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewer::toState(IMFViewerState state)
{
  if (state == Error)
  {
    // Set the status bar text to bold red, and disable the Go button
    statusbar->setStyleSheet("QStatusBar\n{\nfont-weight: bold;\ncolor: FireBrick;\n}");
    goBtn->setDisabled(true);
  }
  else if (state == Success)
  {
    // Set the status bar text to bold green, and enable the Go button
    statusbar->setStyleSheet("QStatusBar\n{\nfont-weight: bold;\ncolor: #186118;\n}");
    goBtn->setEnabled(true);
  }
  else
  {
    // Set the status bar back to its default text attributes, and enable the Go button
    statusbar->setStyleSheet("");
    goBtn->setEnabled(true);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFViewer::event(QEvent* event)
{
  if (event->type() == QEvent::StatusTip)
  {
    return true;
  }

  return QMainWindow::event(event);
}


