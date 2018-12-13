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

#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>

#include "SIMPLib/DataContainers/DataContainerArray.h"

class QtSSettings;

class IMFViewer_UI : public QMainWindow
{
  Q_OBJECT

public:
  IMFViewer_UI(QWidget* parent = 0);
  ~IMFViewer_UI();

  /**
   * @brief Saves this session to a file
   * @return
   */
  void saveSession();

  /**
   * @brief Loads the session from a file
   * @return
   */
  void loadSession();

  /**
   * @brief Returns the QMenuBar for the window
   * @return
   */
  QMenuBar* getMenuBar();

protected:
  /**
   * @brief setupGui
   */
  void setupGui();

  /**
   * @brief createApplicationMenu
   */
  void createMenu();

  /**
   * @brief readSettings
   */
  void readSettings();

  /**
   * @brief readWindowSettings
   * @param prefs
   */
  void readWindowSettings(QtSSettings* prefs);

  /**
   * @brief writeSettings
   */
  void writeSettings();

  /**
   * @brief writeWindowSettings
   * @param prefs
   */
  void writeWindowSettings(QtSSettings* prefs);

protected slots:
  /**
   * @brief importData
   */
  void importData();

  /**
   * @brief openRecentFile
   */
  void openRecentFile();

  /**
   * @brief updateRecentFileList
   * @param file
   */
  void updateRecentFileList(const QString& file);

private:
  class vsInternals;
  vsInternals* m_Internals;

  QMenuBar* m_MenuBar = nullptr;
  QMenu* m_RecentFilesMenu = nullptr;
  QAction* m_ClearRecentsAction = nullptr;

  QString m_OpenDialogLastDirectory = "";

  /**
   * @brief loadSession
   * @param filePath
   * @return
   */
  void loadSessionFromFile(const QString& filePath);

  IMFViewer_UI(const IMFViewer_UI&);   // Copy Constructor Not Implemented
  void operator=(const IMFViewer_UI&); // Operator '=' Not Implemented
};
