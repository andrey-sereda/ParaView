/*=========================================================================

  Program:   ParaView
  Module:    vtkPVSourceWidget.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPVSourceWidget.h"

#include "vtkPVApplication.h"
#include "vtkPVProcessModule.h"
#include "vtkClientServerStream.h"

vtkCxxRevisionMacro(vtkPVSourceWidget, "1.10");

//----------------------------------------------------------------------------
vtkPVSourceWidget::vtkPVSourceWidget()
{
  this->SourceID.ID = 0;
  this->OutputID.ID = 0;
}

//----------------------------------------------------------------------------
vtkPVSourceWidget::~vtkPVSourceWidget()
{

  if (this->OutputID.ID)
    {
    vtkPVApplication* pvApp = 
      vtkPVApplication::SafeDownCast(this->Application);
    if (pvApp)
      {
      pvApp->GetProcessModule()->DeleteStreamObject(this->OutputID);
      pvApp->GetProcessModule()->SendStreamToServer();
      }
    }

  if (this->SourceID.ID)
    {
    vtkPVApplication* pvApp = 
      vtkPVApplication::SafeDownCast(this->Application);
    if (pvApp)
      {  
      pvApp->GetProcessModule()->DeleteStreamObject(this->SourceID);
      pvApp->GetProcessModule()->SendStreamToServer();
      }
    }

  this->SourceID.ID = 0;
  this->OutputID.ID = 0;
}

//----------------------------------------------------------------------------
void vtkPVSourceWidget::SaveInBatchScript(ofstream *file)
{
  if (this->SourceID.ID)
    {
    return;
    } 
  vtkPVProcessModule* pm = this->GetPVApplication()->GetProcessModule();
  pm->GetStream() << vtkClientServerStream::Invoke 
                  << this->SourceID << "GetClassName" 
                  << vtkClientServerStream::End;
  pm->SendStreamToClient();
  const char* dataClassName;
  if(pm->GetLastClientResult().GetArgument(0, 0, &dataClassName))
    {
    *file << dataClassName << " pvTemp" << this->SourceID.ID << "\n";
    }
  // This will loop through all parts and call SaveInBatchScriptForPart.
  this->Superclass::SaveInBatchScript(file);
}

//----------------------------------------------------------------------------
void vtkPVSourceWidget::SaveInBatchScriptForPart(ofstream *file, 
                                                 vtkClientServerID sourceID)
{
  if (sourceID.ID == 0 || this->VariableName == NULL)
    {
    return;
    } 

  *file << "\t" << "pvTemp" << sourceID << " Set" << this->VariableName
        << " pvTemp" << this->SourceID.ID << endl;
}

//----------------------------------------------------------------------------
void vtkPVSourceWidget::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << "Source Tcl name: " << (this->SourceID)
     << endl;
  os << "Output Tcl Name: " << (this->OutputID)
     << endl;
}

//----------------------------------------------------------------------------
vtkClientServerID vtkPVSourceWidget::GetObjectByName(const char* name)
{
  if(!strcmp(name, "Output"))
    {
    return this->OutputID;
    }
  if(!strcmp(name, "Source"))
    {
    return this->SourceID;
    }
  vtkClientServerID id = {0};
  return id;
}

