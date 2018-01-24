/* ============================================================================
* Copyright (c) 2009-2016 BlueQuartz Software, LLC
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

#include "DREAM3DFileTreeModel.h"

#include <QtCore/QFileInfo>
#include <QtCore/QDir>

#include <QtGui/QFont>

#include "SIMPLib/DataContainers/DataContainerArrayProxy.h"

#include "IMFViewer/Dialogs/Utilities/DREAM3DFileItem.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DREAM3DFileTreeModel::DREAM3DFileTreeModel(QObject* parent)
: QAbstractItemModel(parent)
{
  QVector<QVariant> vector;
  vector.push_back("Name");
  rootItem = new DREAM3DFileItem(vector);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DREAM3DFileTreeModel::~DREAM3DFileTreeModel()
{
  delete rootItem;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int DREAM3DFileTreeModel::columnCount(const QModelIndex& parent) const
{
  return rootItem->columnCount();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QVariant DREAM3DFileTreeModel::data(const QModelIndex& index, int role) const
{
  if(!index.isValid())
  {
    return QVariant();
  }

  DREAM3DFileItem* item = getItem(index);

  if(role == Qt::DisplayRole)
  {
    return item->data(index.column());
  }
  else if (role == Qt::CheckStateRole)
  {
    return static_cast<int>(item->isChecked() ? Qt::Checked : Qt::Unchecked );
  }
  else if(role == Qt::BackgroundRole)
  {
    return QVariant();
  }
  else if(role == Qt::ForegroundRole)
  {
    return QColor(Qt::black);
  }
  else if(role == Qt::ToolTipRole)
  {
    return QVariant();
  }
  else if (role == Qt::FontRole)
  {
    DREAM3DFileItem* item = getItem(index);
    if (item->getItemType() == DREAM3DFileItem::ItemType::DataContainer)
    {
      QFont font;
      font.setBold(true);
      return font;
    }
    else
    {
      return QVariant();
    }
  }
  else if(role == Qt::DecorationRole)
  {
    QModelIndex nameIndex = this->index(index.row(), DREAM3DFileItem::Name, index.parent());
    if(nameIndex == index)
    {
      DREAM3DFileItem* item = getItem(index);
      return item->getIcon();
    }
    else
    {
      return QVariant();
    }
  }

  return QVariant();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
Qt::ItemFlags DREAM3DFileTreeModel::flags(const QModelIndex& index) const
{
  if(!index.isValid())
  {
    return 0;
  }

  DREAM3DFileItem* item = getItem(index);
  if(item->getItemType() == DREAM3DFileItem::ItemType::DataContainer)
  {
    // This is a node
    return (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
  }
  else if (item->getItemType() == DREAM3DFileItem::ItemType::AttributeMatrix)
  {
    // This is a leaf
    return (0);
  }
  else if (item->getItemType() == DREAM3DFileItem::ItemType::DataArray)
  {
    return (Qt::ItemNeverHasChildren);
  }

  return QAbstractItemModel::flags(index);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DREAM3DFileItem* DREAM3DFileTreeModel::getItem(const QModelIndex& index) const
{
  if(index.isValid())
  {
    DREAM3DFileItem* item = static_cast<DREAM3DFileItem*>(index.internalPointer());
    if(item)
    {
      return item;
    }
  }
  return rootItem;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QVariant DREAM3DFileTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
  {
    return rootItem->data(section);
  }

  return QVariant();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QModelIndex DREAM3DFileTreeModel::index(int row, int column, const QModelIndex& parent) const
{
  if(parent.isValid() && parent.column() != 0)
  {
    return QModelIndex();
  }

  DREAM3DFileItem* parentItem = getItem(parent);

  DREAM3DFileItem* childItem = parentItem->child(row);
  if(childItem)
  {
    return createIndex(row, column, childItem);
  }
  else
  {
    return QModelIndex();
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileTreeModel::insertRows(int position, int rows, const QModelIndex& parent)
{
  DREAM3DFileItem* parentItem = getItem(parent);
  bool success;

  beginInsertRows(parent, position, position + rows - 1);
  success = parentItem->insertChildren(position, rows, rootItem->columnCount());
  endInsertRows();

  return success;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileTreeModel::removeRows(int position, int rows, const QModelIndex& parent)
{
  DREAM3DFileItem* parentItem = getItem(parent);
  bool success = true;

  beginRemoveRows(parent, position, position + rows - 1);
  success = parentItem->removeChildren(position, rows);
  endRemoveRows();

  return success;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QModelIndex DREAM3DFileTreeModel::parent(const QModelIndex& index) const
{
  if(!index.isValid())
  {
    return QModelIndex();
  }

  DREAM3DFileItem* childItem = getItem(index);
  DREAM3DFileItem* parentItem = childItem->parent();

  if(parentItem == rootItem)
  {
    return QModelIndex();
  }

  return createIndex(parentItem->childNumber(), 0, parentItem);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int DREAM3DFileTreeModel::rowCount(const QModelIndex& parent) const
{
  DREAM3DFileItem* parentItem = getItem(parent);

  return parentItem->childCount();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  DREAM3DFileItem* item = getItem(index);

  if(role == Qt::DecorationRole)
  {
    item->setIcon(value.value<QIcon>());
    emit dataChanged(index, index);
  }
  else if(role == Qt::ToolTipRole)
  {
    item->setItemTooltip(value.toString());
    emit dataChanged(index, index);
  }
  else if (role == Qt::CheckStateRole)
  {
    if (itemType(index) != DREAM3DFileItem::ItemType::DataContainer)
    {
      return false;
    }

    QModelIndexList indexesChanged;

    setChecked(index, value.toBool());
    indexesChanged.push_back(index);

    QString dcName = data(index, Qt::DisplayRole).toString();
    DataContainerProxy dcProxy = m_Proxy.dataContainers.value(dcName);
    dcProxy.flag = value.toBool();

    for (int i = 0; i < rowCount(index); i++)
    {
      QModelIndex amIndex = this->index(i, DREAM3DFileItem::Name, index);
      setChecked(amIndex, value.toBool());
      indexesChanged.push_back(amIndex);

      QString amName = data(amIndex, Qt::DisplayRole).toString();
      AttributeMatrixProxy amProxy = dcProxy.attributeMatricies.value(amName);
      amProxy.flag = value.toBool();

      for (int i = 0; i < rowCount(amIndex); i++)
      {
        QModelIndex daIndex = this->index(i, DREAM3DFileItem::Name, amIndex);
        setChecked(daIndex, value.toBool());
        indexesChanged.push_back(daIndex);

        QString daName = data(daIndex, Qt::DisplayRole).toString();
        DataArrayProxy daProxy = amProxy.dataArrays.value(daName);
        daProxy.flag = value.toBool();
        amProxy.dataArrays[daName] = daProxy;
      }

      dcProxy.attributeMatricies[amName] = amProxy;
    }

    m_Proxy.dataContainers[dcName] = dcProxy;

    emit dataChanged(indexesChanged[0], indexesChanged[indexesChanged.size() - 1]);
  }
  else
  {
    item->setData(index.column(), value);
    emit dataChanged(index, index);
  }

  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DREAM3DFileItem::ItemType DREAM3DFileTreeModel::itemType(const QModelIndex &index) const
{
  DREAM3DFileItem* item = getItem(index);
  return item->getItemType();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DREAM3DFileTreeModel::setItemType(const QModelIndex &index, DREAM3DFileItem::ItemType itemType)
{
  DREAM3DFileItem* item = getItem(index);
  item->setItemType(itemType);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileTreeModel::isChecked(const QModelIndex &index) const
{
  DREAM3DFileItem* item = getItem(index);
  return item->isChecked();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DREAM3DFileTreeModel::setChecked(const QModelIndex &index, bool checked)
{
  DREAM3DFileItem* item = getItem(index);
  item->setChecked(checked);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileTreeModel::isEmpty()
{
  if(rowCount(QModelIndex()) <= 0)
  {
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DREAM3DFileTreeModel::populateTreeWithProxy(DataContainerArrayProxy proxy)
{
  for (QMap<QString, DataContainerProxy>::iterator dcIter = proxy.dataContainers.begin(); dcIter != proxy.dataContainers.end(); dcIter++)
  {
    QString dcName = dcIter.key();
    DataContainerProxy dcProxy = dcIter.value();

    int rowPos = rowCount();
    insertRow(rowPos);
    QModelIndex dcIndex = index(rowPos, DREAM3DFileItem::Name);
    setItemType(dcIndex, DREAM3DFileItem::ItemType::DataContainer);
    setData(dcIndex, dcName, Qt::DisplayRole);

    for (QMap<QString, AttributeMatrixProxy>::iterator amIter = dcProxy.attributeMatricies.begin(); amIter != dcProxy.attributeMatricies.end(); amIter++)
    {
      QString amName = amIter.key();
      AttributeMatrixProxy amProxy = amIter.value();

      int rowPos = rowCount(dcIndex);
      insertRow(rowPos, dcIndex);
      QModelIndex amIndex = index(rowPos, DREAM3DFileItem::Name, dcIndex);
      setItemType(amIndex, DREAM3DFileItem::ItemType::AttributeMatrix);
      setData(amIndex, amName, Qt::DisplayRole);

      for (QMap<QString, DataArrayProxy>::iterator daIter = amProxy.dataArrays.begin(); daIter != amProxy.dataArrays.end(); daIter++)
      {
        QString daName = daIter.key();
        DataArrayProxy daProxy = daIter.value();

        int rowPos = rowCount(amIndex);
        insertRow(rowPos, amIndex);
        QModelIndex daIndex = index(rowPos, DREAM3DFileItem::Name, amIndex);
        setItemType(daIndex, DREAM3DFileItem::ItemType::DataArray);
        setData(daIndex, daName, Qt::DisplayRole);
      }
    }

    // Set the check state after adding all the items to the Data Container, so that the check state propagates through the whole Data Container sub-tree
    setData(dcIndex, dcProxy.flag, Qt::CheckStateRole);
  }

  m_Proxy = proxy;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DataContainerArrayProxy DREAM3DFileTreeModel::getModelProxy()
{
  return m_Proxy;
}


