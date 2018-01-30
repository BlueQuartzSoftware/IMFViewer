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

#pragma once

#include <QtCore/QList>
#include <QtCore/QVariant>
#include <QtCore/QVector>

#include <QtGui/QIcon>

#include "SIMPLib/Common/SIMPLibSetGetMacros.h"

class DREAM3DFileItem
{
  public:
    DREAM3DFileItem(const QVector<QVariant>& data, DREAM3DFileItem* parent = 0);
    virtual ~DREAM3DFileItem();

    enum ColumnData
    {
      Name
    };

    enum class ItemType : unsigned int
    {
      Unknown,
      DataContainer,
      AttributeMatrix,
      DataArray,
    };

    SIMPL_INSTANCE_PROPERTY(Qt::CheckState, CheckState)
    SIMPL_INSTANCE_PROPERTY(DREAM3DFileItem::ItemType, ItemType)
    SIMPL_INSTANCE_PROPERTY(QIcon, Icon)
    SIMPL_INSTANCE_PROPERTY(QString, ItemTooltip)

    DREAM3DFileItem* child(int number);
    DREAM3DFileItem* parent();

    int childCount() const;
    int columnCount() const;

    QVariant data(int column) const;
    bool setData(int column, const QVariant& value);

    bool insertChild(int position, DREAM3DFileItem* child);
    bool insertChildren(int position, int count, int columns);
    bool insertColumns(int position, int columns);

    bool removeChild(int position);
    bool removeChildren(int position, int count);
    bool removeColumns(int position, int columns);

    int childNumber() const;

    void setParent(DREAM3DFileItem* parent);

  private:
    QList<DREAM3DFileItem*>               m_ChildItems;
    QVector<QVariant>                     m_ItemData;
    DREAM3DFileItem*                      m_ParentItem;

    DREAM3DFileItem(const DREAM3DFileItem&);    // Copy Constructor Not Implemented
    void operator=(const DREAM3DFileItem&);  // Operator '=' Not Implemented
};
