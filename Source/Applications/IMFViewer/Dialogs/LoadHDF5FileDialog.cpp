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

#include "LoadHDF5FileDialog.h"

#include "SIMPLib/DataContainers/DataContainerArrayProxy.h"

#include "IMFViewer/Dialogs/Utilities/DREAM3DFileTreeModel.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
LoadHDF5FileDialog::LoadHDF5FileDialog(DataContainerArrayProxy proxy, QWidget* parent) :
  QDialog(parent),
  m_Proxy(proxy)
{
  setupUi(this);

  setupGui();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
LoadHDF5FileDialog::~LoadHDF5FileDialog()
{
  
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void LoadHDF5FileDialog::setupGui()
{
  DREAM3DFileTreeModel* model = new DREAM3DFileTreeModel();
  connect(model, &DREAM3DFileTreeModel::dataChanged, [=] {
    bool enabled = false;
    for (int i = 0; i < model->rowCount(); i++)
    {
      QModelIndex dcIndex = model->index(i, DREAM3DFileItem::Name);
      if (model->isChecked(dcIndex) == true)
      {
        enabled = true;
      }
    }

    loadBtn->setEnabled(enabled);
  });

  treeView->setModel(model);

  model->populateTreeWithProxy(m_Proxy);

  QModelIndexList indexes = model->match(model->index(0, 0), Qt::DisplayRole, "*", -1, Qt::MatchWildcard|Qt::MatchRecursive);
  for (int i = 0; i < indexes.size(); i++)
  {
    QModelIndex index = indexes[i];
    treeView->expand(index);
  }

  connect(loadBtn, &QPushButton::clicked, [=] {
    accept();
  });

  connect(cancelBtn, &QPushButton::clicked, [=] {
    reject();
  });

  loadBtn->setDisabled(true);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataContainerArrayProxy LoadHDF5FileDialog::getLoadProxy()
{
  DREAM3DFileTreeModel* model = static_cast<DREAM3DFileTreeModel*>(treeView->model());
  if (model == nullptr)
  {
    return m_Proxy;
  }

  QStringList dcNames = model->getSelectedDataContainerNames();
  for (int i = 0; i < dcNames.size(); i++)
  {

  }

  return m_Proxy;
}

