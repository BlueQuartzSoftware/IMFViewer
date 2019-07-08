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
#include "SIMPLib/Filtering/FilterPipeline.h"

#include "SIMPLVtkLib/Dialogs/AbstractImportMontageDialog.h"
#include "SIMPLVtkLib/Dialogs/FijiListWidget.h"
#include "SIMPLVtkLib/QtWidgets/VSQueueWidget.h"
#include "SIMPLVtkLib/Visualization/VisualFilters/VSAbstractFilter.h"

class QtSSettings;
class ImportMontageWizard;
class ExecutePipelineWizard;
class VSQueueWidget;
class PipelineWorker;
class VSFileNameFilter;
class VSDataSetFilter;
class PerformMontageWizard;

class IMFViewer_UI : public QMainWindow
{
  Q_OBJECT

public:
  IMFViewer_UI(QWidget* parent = nullptr);
  ~IMFViewer_UI() override;

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

  /**
   * @brief Saves an image to a file
   * @return
   */
  void saveImage();

  /**
   * @brief Saves filter data to a DREAM3D File
   * @return
   */
  void saveDream3d();

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
   * @brief readDockWidgetSettings
   * @param prefs
   * @param dw
   */
  void readDockWidgetSettings(QtSSettings* prefs, QDockWidget* dw);

  /**
   * @brief writeSettings
   */
  void writeSettings();

  /**
   * @brief writeWindowSettings
   * @param prefs
   */
  void writeWindowSettings(QtSSettings* prefs);

  /**
   * @brief writeDockWidgetSettings
   * @param prefs
   * @param dw
   */
  void writeDockWidgetSettings(QtSSettings* prefs, QDockWidget* dw);

protected slots:
  /**
   * @brief importData
   */
  void importData();

  /**
   * @brief importData
   */
  void importData(const QString& filePath);

  /**
   * @brief importImages
   * @param filePaths
   */
  void importImages(const QStringList& filePaths);

  /**
   * @brief Import a pipeline to execute
   * @param executePipelineWizard
   */
  void importPipeline(ExecutePipelineWizard* executePipelineWizard);

  /**
   * @brief executePipeline
   */
  void executePipeline();

  /**
   * @brief performMontage
   */
  void performMontage();

  /**
   * @brief openRecentFile
   */
  void openRecentFile();

  /**
   * @brief updateRecentFileList
   * @param file
   */
  void updateRecentFileList(const QString& file);

  /**
   * @brief processStatusMessage
   * @param statusMessage
   */
  void processStatusMessage(const QString& statusMessage);

  /**
   * @brief handleDatasetResults
   * @param textFilter
   * @param filter
   */
  void handleDatasetResults(VSFileNameFilter* textFilter, VSDataSetFilter* filter);

  /**
   * @brief handleMontageResults
   * @param pipeline
   * @param err
   */
  void handleMontageResults(const FilterPipeline::Pointer &pipeline, int err);

  /**
   * @brief listenSelectionChanged
   * @param filters
   */
  void listenSelectionChanged(VSAbstractFilter::FilterListType filters);

private:
  class vsInternals;
  vsInternals* m_Ui;

  QMenuBar* m_MenuBar = nullptr;
  QMenu* m_RecentFilesMenu = nullptr;
  QMenu* m_MenuThemes = nullptr;
  QAction* m_ClearRecentsAction = nullptr;

  QActionGroup* m_ThemeActionGroup = nullptr;

  QString m_OpenDialogLastDirectory = "";
  AbstractImportMontageDialog::DisplayType m_DisplayType = AbstractImportMontageDialog::DisplayType::NotSpecified;

  std::vector<FilterPipeline::Pointer> m_Pipelines;

  QThread* m_PipelineWorkerThread = nullptr;
  PipelineWorker* m_PipelineWorker = nullptr;

  /**
   * @brief createThemeMenu
   * @param actionGroup
   * @param parent
   * @return
   */
  QMenu* createThemeMenu(QActionGroup* actionGroup, QWidget* parent = nullptr);

  /**
   * @brief loadSession
   * @param filePath
   * @return
   */
  void loadSessionFromFile(const QString& filePath);

  /**
   * @brief importGenericMontage
   */
  void importGenericMontage();

  /**
   * @brief importDREAM3DMontage
   */
  void importDREAM3DMontage();

  /**
   * @brief importFijiMontage
   */
  void importFijiMontage();

  using SpacingTuple = std::tuple<float, float, float>;
  using OriginTuple = std::tuple<float, float, float>;
  using ColorWeightingTuple = std::tuple<float, float, float>;

  /**
   * @brief importFijiMontage
   * @param montageName
   * @param fijiListInfo
   * @param overrideSpacing
   * @param spacing
   * @param overrideOrigin
   * @param origin
   * @param montageStart
   * @param montageEnd
   */
  void importFijiMontage(const QString& montageName, FijiListInfo_t fijiListInfo, bool overrideSpacing, FloatVec3Type spacing, bool overrideOrigin, FloatVec3Type origin, IntVec3Type montageStart,
                         IntVec3Type montageEnd, int32_t lengthUnit);

  /**
   * @brief importRobometMontage
   */
  void importRobometMontage();

  /**
   * @brief importZeissMontage
   */
  void importZeissMontage();

  /**
   * @brief importZeissZenMontage
   */
  void importZeissZenMontage();

  /**
   * @brief runPipeline
   * @param pipeline
   */
  void addPipelineToQueue(const FilterPipeline::Pointer &pipeline);

  /**
   * @brief executePipeline
   * @param pipeline
   */
  void executePipeline(const FilterPipeline::Pointer &pipeline, const DataContainerArray::Pointer &dca);

  /**
  * @brief Build a custom data container array for montaging
  * @param dataContainerArray
  * @param montageDatasets
  */
  std::pair<int, int> buildCustomDCA(const DataContainerArray::Pointer &dca, VSAbstractFilter::FilterListType montageDatasets);

  IMFViewer_UI(const IMFViewer_UI&);   // Copy Constructor Not Implemented
  void operator=(const IMFViewer_UI&); // Operator '=' Not Implemented
};
