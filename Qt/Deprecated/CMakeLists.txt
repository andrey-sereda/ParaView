#==========================================================================
#
#     Program: ParaView
#
#     Copyright (c) 2005-2008 Sandia Corporation, Kitware Inc.
#     All rights reserved.
#
#     ParaView is a free software; you can redistribute it and/or modify it
#     under the terms of the ParaView license version 1.2.
#
#     See License_v1.2.txt for the full ParaView license.
#     A copy of this license can be obtained by contacting
#     Kitware Inc.
#     28 Corporate Drive
#     Clifton Park, NY 12065
#     USA
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#==========================================================================
set (Module_SRCS
  pqColorScaleEditor.cxx
  pqColorScaleEditor.h
  pqTransferFunctionChartViewWidget.cxx
  pqTransferFunctionChartViewWidget.h
)

set (Module_MOC_HDRS
  pqColorScaleEditor.h
  pqTransferFunctionChartViewWidget.h
)

set (Module_UI_FILES
  pqColorScaleDialog.ui
)

#-----------------------------------------------------------------------------------
if (PARAVIEW_QT_VERSION VERSION_GREATER "4")
  qt5_wrap_cpp(__Module_MOC_SRCS ${Module_MOC_HDRS})
  qt5_wrap_ui(__Module_UI_SRCS ${Module_UI_FILES})
else ()
  qt4_wrap_cpp(__Module_MOC_SRCS ${Module_MOC_HDRS})
  qt4_wrap_ui(__Module_UI_SRCS ${Module_UI_FILES})
endif ()
set(${vtk-module}_NO_HeaderTest 1)
vtk_module_library(${vtk-module}
  ${Module_SRCS}
  ${__Module_RESOURCE_SRCS}
  ${__Module_MOC_SRCS}
  ${__Module_UI_SRCS})
