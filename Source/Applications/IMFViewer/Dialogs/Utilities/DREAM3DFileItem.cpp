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
#include "DREAM3DFileItem.h"

#include <QtCore/QStringList>
#include <QtGui/QColor>

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DREAM3DFileItem::DREAM3DFileItem(const QVector<QVariant>& data, DREAM3DFileItem* parent)
: m_CheckState(Qt::Unchecked)
, m_ItemType(DREAM3DFileItem::ItemType::Unknown)
, m_Icon(QIcon())
, m_ItemTooltip("")
, m_ItemData(data)
, m_ParentItem(parent)
{

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DREAM3DFileItem::~DREAM3DFileItem()
{
  qDeleteAll(m_ChildItems);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DREAM3DFileItem* DREAM3DFileItem::child(int number)
{
  return m_ChildItems.value(number);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int DREAM3DFileItem::childCount() const
{
  return m_ChildItems.count();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int DREAM3DFileItem::childNumber() const
{
  if(m_ParentItem)
  {
    return m_ParentItem->m_ChildItems.indexOf(const_cast<DREAM3DFileItem*>(this));
  }

  return 0;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int DREAM3DFileItem::columnCount() const
{
  return m_ItemData.count();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QVariant DREAM3DFileItem::data(int column) const
{
  return m_ItemData.value(column);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileItem::insertChild(int position, DREAM3DFileItem* child)
{
  m_ChildItems.insert(position, child);
  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileItem::insertChildren(int position, int count, int columns)
{
  if(position < 0 || position > m_ChildItems.size())
  {
    return false;
  }

  for(int row = 0; row < count; ++row)
  {
    QVector<QVariant> data(columns);
    DREAM3DFileItem* item = new DREAM3DFileItem(data, this);
    insertChild(position, item);
  }

  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileItem::insertColumns(int position, int columns)
{
  if(position < 0 || position > m_ItemData.size())
  {
    return false;
  }

  for(int column = 0; column < columns; ++column)
  {
    m_ItemData.insert(position, QVariant());
  }

  foreach(DREAM3DFileItem* child, m_ChildItems)
  {
    child->insertColumns(position, columns);
  }

  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DREAM3DFileItem* DREAM3DFileItem::parent()
{
  return m_ParentItem;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileItem::removeChild(int position)
{
  m_ChildItems.removeAt(position);
  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileItem::removeChildren(int position, int count)
{
  if(position < 0 || position + count > m_ChildItems.size())
  {
    return false;
  }

  for(int row = 0; row < count; ++row)
  {
    delete m_ChildItems.takeAt(position);
  }

  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileItem::removeColumns(int position, int columns)
{
  if(position < 0 || position + columns > m_ItemData.size())
  {
    return false;
  }

  for(int column = 0; column < columns; ++column)
  {
    m_ItemData.remove(position);
  }

  foreach(DREAM3DFileItem* child, m_ChildItems)
  {
    child->removeColumns(position, columns);
  }

  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
bool DREAM3DFileItem::setData(int column, const QVariant& value)
{
  if(column < 0 || column >= m_ItemData.size())
  {
    return false;
  }

  m_ItemData[column] = value;
  return true;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void DREAM3DFileItem::setParent(DREAM3DFileItem* parent)
{
  m_ParentItem = parent;
}
