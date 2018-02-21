/* ============================================================================
* Copyright (c) 2012 Michael A. Jackson (BlueQuartz Software)
* Copyright (c) 2012 Dr. Michael A. Groeber (US Air Force Research Laboratories)
* Copyright (c) 2012 Joseph B. Kleingers (Student Research Assistant)
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
* Neither the name of Michael A. Groeber, Michael A. Jackson, Joseph B. Kleingers,
* the US Air Force, BlueQuartz Software nor the names of its contributors may be
* used to endorse or promote products derived from this software without specific
* prior written permission.
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

#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QMenuBar>

#include <QtWidgets/QApplication>

#define imfViewerApp (static_cast<IMFViewerApplication *>(qApp))

class IMFViewer;
class IMFViewerMenu;
class IMFViewer_UI;
class IMFController;

class IMFViewerApplication : public QApplication
{
  Q_OBJECT

public:
  IMFViewerApplication(int& argc, char** argv);
  virtual ~IMFViewerApplication();

  IMFViewer_UI* getNewIMFViewerInstance();

private:
  IMFViewer_UI*                           m_ActiveInstance = nullptr;

  /**
   * @brief loadStyleSheet
   * @param sheetName
   */
  void loadStyleSheet(const QString &sheetName);

  /**
   * @brief setActiveInstance
   * @param instance
   */
  void setActiveInstance(IMFViewer_UI* instance);

  IMFViewerApplication(const IMFViewerApplication&); // Copy Constructor Not Implemented
  void operator=(const IMFViewerApplication&); // Operator '=' Not Implemented
};

