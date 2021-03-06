Advanced Options
=========
IMFViewer has advanced options for performing a montage and executing pipelines:

1. [Perform Montage](#performMontage)
2. [Execute Pipeline](#executePipeline)

![DREAM3D Import Montage](Images/Advanced-Options-Menu.png)

In **IMF Viewer**, the **File** menu option contains options for performing a montage and executing a pipeline.

---

<a name="performMontage">
## Perform Montage ##
</a>

![Perform Montage](Images/Perform-Montage.png)

The **Perform Montage** menu option allows the user to perform a montage of selected image datasets. The selected datasets are shown in a read-only list. The resulting montage can be named using the **Montage Name** text entry. Selecting **Perform Stitching Only** will ensure the current positions for selected image datasets are kept and the registration filter is not run. The **Save to File** option allows the user to select an output file for the montage results.

---

<a name="executePipeline">
## Execute Pipeline ##
</a>

![Execute Pipeline](Images/Execute-Pipeline.png)

The **Execute Pipeline** menu option allows the user to execute a pipeline. The pipeline can be executed either directly from the file system or on a loaded dataset. If the **Execute a Pipeline From Disk** is selected, the user needs only to select a pipeline .json file and click **OK**. However, all filters in the JSON file must be available for it to run successfully. If the user selects **Execute a Pipeline on a Loaded Dataset**, then an additional page is shown after clicking **Next >**.

![Execute Pipeline Advanced](Images/Execute-Pipeline-Advanced.png)

The further options include selecting a starting filter, a loaded dataset, the display type, and setting the origin and/or spacing for the image geometry. The **Starting Filter** is the filter in the pipeline to start the execution of the pipeline. This is for skipping any dataset loading or other unnecessary filters. The **Image Dataset** is the input data for the pipeline execution. The three radio buttons are for selecting the display type: **Display Montage**, **Display Tiles Side by Side**, or **Display Outline Only**. The **Advanced** section contains options to change the origin and/or spacing of all input image geometry. Filter parameter values in the selected pipeline file should match appropriately with the input dataset. For example, a cell attribute matrix name in a particular filter's parameters should match the input data containers.