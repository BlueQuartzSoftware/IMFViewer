# --------------------------------------------------------------------
# Any Class that inherits from QObject, either directly or through the heirarchy needs to have its header listed here
set(IMFViewer_Dialogs_Utilities_Moc_HDRS
  ${IMFViewer_SOURCE_DIR}/Dialogs/Utilities/DREAM3DFileItemDelegate.h
  ${IMFViewer_SOURCE_DIR}/Dialogs/Utilities/DREAM3DFileTreeModel.h
  )
# --------------------------------------------------------------------
# Run Qts automoc program to generate some source files that get compiled
QT5_WRAP_CPP( IMFViewer_Dialogs_Utilities_Generated_MOC_SRCS ${IMFViewer_Dialogs_Utilities_Moc_HDRS})
set_source_files_properties( ${IMFViewer_Dialogs_Utilities_Generated_MOC_SRCS} PROPERTIES GENERATED TRUE)
set_source_files_properties( ${IMFViewer_Dialogs_Utilities_Generated_MOC_SRCS} PROPERTIES HEADER_FILE_ONLY TRUE)

set(IMFViewer_Dialogs_Utilities_HDRS
  ${IMFViewer_SOURCE_DIR}/Dialogs/Utilities/DREAM3DFileItem.h
)

set(IMFViewer_Dialogs_Utilities_SRCS
  ${IMFViewer_SOURCE_DIR}/Dialogs/Utilities/DREAM3DFileItem.cpp
  ${IMFViewer_SOURCE_DIR}/Dialogs/Utilities/DREAM3DFileItemDelegate.cpp
  ${IMFViewer_SOURCE_DIR}/Dialogs/Utilities/DREAM3DFileTreeModel.cpp
)

cmp_IDE_SOURCE_PROPERTIES( "Utilities" "${IMFViewer_Dialogs_Utilities_HDRS};${IMFViewer_Dialogs_Utilities_Moc_HDRS}" "${IMFViewer_Dialogs_Utilities_SRCS}" "${PROJECT_INSTALL_HEADERS}")
cmp_IDE_SOURCE_PROPERTIES( "Generated/Utilities" "" "${IMFViewer_Dialogs_Utilities_Generated_MOC_SRCS}" "0")

#-- Put the generated files into their own group for IDEs
cmp_IDE_GENERATED_PROPERTIES("Generated/moc" "${IMFViewer_Dialogs_Utilities_Generated_MOC_SRCS}" "")

set(IMFViewer_Dialogs_Utilities_HDRS
  ${IMFViewer_Dialogs_Utilities_HDRS}
  ${IMFViewer_Dialogs_Utilities_Moc_HDRS}  # Add the headers that get Moc'ed here so they show up in solutions/IDEs/Project files
)

set(IMFViewer_Dialogs_Utilities_SRCS
  ${IMFViewer_Dialogs_Utilities_SRCS}
  ${IMFViewer_Dialogs_Utilities_Generated_MOC_SRCS}
)

