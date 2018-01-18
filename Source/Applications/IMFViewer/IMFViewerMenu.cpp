/* ============================================================================
* Copyright (c) 2010, Michael A. Jackson (BlueQuartz Software)
* Copyright (c) 2010, Dr. Michael A. Groeber (US Air Force Research Laboratories
* All rights reserved.
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
* Neither the name of Michael A. Groeber, Michael A. Jackson, the US Air Force,
* BlueQuartz Software nor the names of its contributors may be used to endorse
* or promote products derived from this software without specific prior written
* permission.
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
*  This code was written under United States Air Force Contract number
*                           FA8650-07-D-5800
*
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#include "IMFViewerMenu.h"

#include <iostream>

#include <QtCore/QDebug>

#include "IMFViewerApplication.h"


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewerMenu::IMFViewerMenu() :
  m_MenuBar(NULL),

  // File Menu
  m_MenuFile(NULL),
  m_ActionNew(NULL)
{
  initialize();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IMFViewerMenu::~IMFViewerMenu()
{
  qDebug() << "~IMFViewerMenu";
  if (m_MenuBar) { delete m_MenuBar; }
  if (m_MenuFile) { delete m_MenuFile; }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IMFViewerMenu::initialize()
{
  m_MenuBar = new QMenuBar(NULL);
  m_MenuBar->setObjectName(QStringLiteral("m_GlobalMenu"));
  m_MenuBar->setGeometry(QRect(0, 0, 1104, 21));

  m_MenuFile = new QMenu(m_MenuBar);
  m_MenuFile->setObjectName(QStringLiteral("menuFile"));


  m_ActionNew = new QAction(this);
  m_ActionNew->setObjectName(QStringLiteral("actionNew"));


  m_ActionNew->setText(QApplication::translate("DREAM3D_UI", "New...", 0));
  m_ActionNew->setShortcut(QApplication::translate("DREAM3D_UI", "Ctrl+N", 0));

  // Connections
  connect(m_ActionNew, SIGNAL(triggered()), csdfApp, SLOT(on_actionNew_triggered()));

  // Add the actions to their respective menus
  m_MenuBar->addAction(m_MenuFile->menuAction());
  m_MenuFile->addAction(m_ActionNew);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QMenuBar* IMFViewerMenu::getMenuBar()
{
  return m_MenuBar;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QMenu* IMFViewerMenu::getFileMenu()
{
  return m_MenuFile;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QAction* IMFViewerMenu::getNew()
{
  return m_ActionNew;
}



