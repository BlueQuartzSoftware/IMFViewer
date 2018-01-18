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


#ifndef _IMFViewer_H_
#define _IMFViewer_H_

#include <QtCore/QObject>
#include <QtCore/QThread>

#include "AddSpatialTransformWidget.h"

#include "ui_IMFViewer.h"

class RootOptionsDialog;
class CampaignOptionsDialog;
class CSDFInProgress;
class DataCampaign;

class IMFViewer : public QMainWindow, public Ui::IMFViewer
{
    Q_OBJECT

  public:

    enum TableIndices
    {
      Name,
      Value
    };

    enum IMFViewerState
    {
      Default,
      Error,
      Success
    };

    IMFViewer(QWidget* parent = 0);
    ~IMFViewer();

    static CSDFTreeWidgetItem* AddTreeItem(QString name, CSDFTreeWidgetItem::ObjectType type, int index, CSDFTreeWidgetItem* parent = NULL, AbstractOptionsDialog* optionsDialog = NULL, const QString &csdfFilePath = "");

    static QList<QString> CampaignTypes();

    bool isRunning();

    bool hasRootItem();

    void createRootItem(const QString &fileName, const int &partId, const QString &csdfFilePath = "");

    DataCampaign* addCampaign(const QString &campaignName, const QString &campaignType, int index, const QString &csdfFilePath = "");

    QTreeWidget* getTreeWidget();

    RootOptionsDialog* getRootOptionsDialog();

    void openCSDFFile(const QString &filePath);

    void importCSDFFile(QString filePath);

    bool event(QEvent* event);

  public slots:

    /**
    * @brief onCustomContextMenuRequested
    * @param pos
    */
    void treeWidgetCustomContextMenuRequested(const QPoint& pos);

    /**
    * @brief importCSDFFiles
    * @param fileList
    */
    void importCSDFFiles(QList<QString> fileList);

    /**
    * @brief showErrorMessage
    * @param msg
    */
    void showErrorMessage(const QString msg);

    /**
    * @brief on_actionClearFileView_triggered
    */
    void on_actionClearFileStructure_triggered();

  protected:
    void setupGui();

    bool isRootItem(CSDFTreeWidgetItem* item);
    bool isCampaignItem(CSDFTreeWidgetItem* item);
    bool isRawItem(CSDFTreeWidgetItem* item);
    bool isReducedItem(CSDFTreeWidgetItem* item);
    bool isMetaItem(CSDFTreeWidgetItem* item);
    bool isSTItem(CSDFTreeWidgetItem* item);
    bool isSTDataset(CSDFTreeWidgetItem* item);

    void addAttribute(QString name, QString value, bool readOnly);
    void removeAttribute(int row);

  protected slots:
    void on_actionNew_triggered();
    void on_actionOpen_triggered();
    void on_actionImportCSDFFile_triggered();
    void on_actionExitIMFViewer_triggered();

    void on_goBtn_clicked();
    void on_selectBtn_clicked();
    void campaignOptions_applyPressed();
    void rootOptions_applyPressed();

    void on_attributesTable_itemChanged(QTableWidgetItem* item);
    void on_treeWidget_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void on_outputFile_textChanged(const QString &text);

    /* Add Functions */
    void addCampaign();
    void addRawFolder(const QString &csdfFilePath = "");
    void addReducedFolder(const QString &csdfFilePath = "");
    void addMetaFolder(const QString &csdfFilePath = "");

    void addSpatialTransform();
    void addOriginShiftTransform(AddSpatialTransformWidget::STValues values, QVector<double> translation);
    void addDirectRemapTransform(AddSpatialTransformWidget::STValues values);
    void addStrideTransform(AddSpatialTransformWidget::STValues values, const QString &units, QVector<double> voxelSize, QVector<int> dimensions, QVector<int> stride);
    void addPolynomialTransform(AddSpatialTransformWidget::STValues values, const QString &polyDegree, QVector<double> xCoefficients, QVector<double> yCoefficients, QVector<double> zCoefficients);
    void addRotationTransform(AddSpatialTransformWidget::STValues values, QVector<double> angle);

    // Rename Root Function
    void renameRootItem();

    /* Edit Functions */
    void editRootItem();
    void editCampaign();
    void editRawFolder();
    void editReducedFolder();
    void editMetaFolder();

    /* Update Functions */
    void updateProgressBar(int value);
    void updateStatusBar(const QString &msg, IMFViewer::IMFViewerState state);
    void updateCampaigns(QTableWidget* campaignTable);

    /* Remove Functions */
    void removeCampaign(const QString &campaignName);
    void removeCampaign(DataCampaign* campaign);
    void removeSpatialTransform();

    void changeCampaignAvailability(const QString &campaignName, bool available);

    void finishWriting();

  signals:
    void writeStarted();
    void writeCanceled();
    void writeFinished();

    void IMFViewerChangedState(IMFViewer* window);
    void actionNew_triggered();
    void actionOpen_triggered();

  private slots:
    void readSpatialTransforms(hid_t stFolderId, CSDFTreeWidgetItem* stFolder);

  private:
    DataCampaign* dataCampaign(CSDFTreeWidgetItem* item);

    void syncTablesToTree(QMap<QString, bool> map, DataCampaign* campaign, const QString &folderType);

    void toState(IMFViewerState state);

    template<typename T>
    QVector<T> toVector(std::vector<T> stdVector);

    void addOriginShiftTransform(AddSpatialTransformWidget::STValues values, QVector<double> translation, CSDFTreeWidgetItem* stFolder);
    void addDirectRemapTransform(AddSpatialTransformWidget::STValues values, CSDFTreeWidgetItem* stFolder);
    void addStrideTransform(AddSpatialTransformWidget::STValues values, const QString &units, QVector<double> voxelSize, QVector<int> dimensions, QVector<int> stride, CSDFTreeWidgetItem* stFolder);
    void addPolynomialTransform(AddSpatialTransformWidget::STValues values, const QString &polyDegree, QVector<double> xCoefficients, QVector<double> yCoefficients, QVector<double> zCoefficients, CSDFTreeWidgetItem* stFolder);
    void addRotationTransform(AddSpatialTransformWidget::STValues values, QVector<double> angle, CSDFTreeWidgetItem* stFolder);

    QString                                                 m_LastOpenDirectory;
    QString                                                 m_OpenFilePath;
    QString                                                 m_CSDFSourcePath;

    CSDFTreeWidgetItem*                                     m_Root;
    CSDFTreeWidgetItem*                                     m_STFolder;

    QMap<QString, QSharedPointer<DataCampaign> >            m_DataCampaignMap;

    QMenu*                                                  m_Menu;

    QThread*                                                m_WorkerThread;
    CSDFInProgress*                                         m_CSDFObject;
};

#endif
