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

#include "IMFController.h"

#include <QtCore/QFileInfo>
#include <QtCore/QMimeDatabase>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

#include "IMFViewer/Dialogs/LoadHDF5FileDialog.h"
#include "IMFViewer/IMFViewer_UI.h"

#include "SIMPLib/Utilities/SIMPLH5DataReader.h"
#include "SIMPLib/Utilities/SIMPLH5DataReaderRequirements.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFController::IMFController(QObject* parent) :
  QObject(parent)
{
  setupGui();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFController::~IMFController()
{
  
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFController::setupGui()
{

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFController::importFile(IMFViewer_UI* instance)
{
  QMimeDatabase db;
  QStringList pngFileExts;

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
  QString filter = tr("All Files (*.*);;"
                      "DREAM.3D Files (*.dream3d);;"
                   "Image Files (%1 %2 %3);;"
                      "VTK Files (*.vtk *.vti *.vtp *.vtr *.vts *.vtu);;"
                      "STL Files (*.stl)").arg(pngSuffixStr).arg(tiffSuffixStr).arg(jpegSuffixStr);
  QString filePath = QFileDialog::getOpenFileName(instance, "Open Input File", m_OpenDialogLastDirectory, filter);
  if (filePath.isEmpty())
  {
    return;
  }

  QMimeType mimeType = db.mimeTypeForFile(filePath, QMimeDatabase::MatchContent);
  QString mimeName = mimeType.name();

  QFileInfo fi(filePath);
  QString ext = fi.completeSuffix().toLower();
  bool success = false;
  if (ext == "dream3d")
  {
    success = openDREAM3DFile(filePath, instance);
  }
  else if (mimeType.inherits("image/png") || mimeType.inherits("image/tiff") || mimeType.inherits("image/jpeg"))
  {
    instance->importData(filePath);
    success = true;
  }
  else if (ext == "vtk" || ext == "vti" || ext == "vtp" || ext == "vtr"
           || ext == "vts" || ext == "vtu")
  {
    instance->importData(filePath);
    success = true;
  }
  else if (ext == "stl")
  {
    success = openSTLFile(filePath, instance);
  }
  else
  {
    QMessageBox::critical(instance, "Invalid File Type",
                          tr("IMF Viewer failed to open the file because the file extension, '.%1', is not supported by the "
                             "application.").arg(ext), QMessageBox::StandardButton::Ok);
    return;
  }

  if (success)
  {
    m_OpenDialogLastDirectory = filePath;
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFController::openDREAM3DFile(const QString &filePath, IMFViewer_UI* instance)
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
      QMessageBox::critical(instance, "Empty File",
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
      QMessageBox::critical(instance, "Invalid Data",
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
      instance->importDataContainerArray(filePath, dca);
      return true;
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool IMFController::openSTLFile(const QString &filePath, IMFViewer_UI* instance)
{
  return true;
}

