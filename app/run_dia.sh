#!/bin/bash 
DIA_APP_PATH="/home/mihara/montsuqi/monpe/app"
DIA_BASE_PATH="$DIA_APP_PATH/.."
DIA_LIBS_PATH="$DIA_APP_PATH/.libs"

DIA_LIB_PATH="$DIA_BASE_PATH/objects//:$DIA_BASE_PATH/plug-ins//"
DIA_PLUGIN_PATH="$DIA_BASE_PATH/plug-ins"
DIA_SHAPE_PATH="$DIA_BASE_PATH/shapes"
DIA_INT_SHAPE_PATH="$DIA_BASE_PATH/objects"
DIA_SHEET_PATH="$DIA_BASE_PATH/sheets"
DIA_PYTHON_PATH="$DIA_BASE_PATH/plug-ins/python"
DIA_XSLT_PATH="$DIA_BASE_PATH/plug-ins/xslt"

export DIA_BASE_PATH DIA_LIB_PATH DIA_SHAPE_PATH DIA_INT_SHAPE_PATH DIA_SHEET_PATH DIA_PLUGIN_PATH DIA_PYTHON_PATH DIA_XSLT_PATH
if [ "x$DEBUGGER" != "x" ] ; then 
  if [ ! -f "$DIA_LIBS_PATH/lt-dia" -o "$DIA_LIBS_PATH/lt-dia" -ot "$DIA_LIBS_PATH/dia" ] ; then
    echo "libtool relink stage necessary before debugging is possible."
    echo "please run $0 once without a debugger."
    exit 1
  fi
   $DEBUGGER "$DIA_LIBS_PATH/lt-dia" "$@"
else
   "$DIA_APP_PATH/dia" "$@"
fi
