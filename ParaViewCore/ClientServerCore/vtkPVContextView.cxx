/*=========================================================================

  Program:   ParaView
  Module:    vtkPVContextView.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPVContextView.h"

#include "vtkCamera.h"
#include "vtkContextInteractorStyle.h"
#include "vtkContextView.h"
#include "vtkExtractVOI.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkMultiProcessStream.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkProcessModule.h"
#include "vtkPVDataRepresentation.h"
#include "vtkPVOptions.h"
#include "vtkPVSynchronizedRenderWindows.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkSmartPointer.h"
#include "vtkTileDisplayHelper.h"
#include "vtkTilesHelper.h"
#include "vtkTimerLog.h"
#include "vtkUnsignedCharArray.h"
#include "vtkWindowToImageFilter.h"

//----------------------------------------------------------------------------
vtkPVContextView::vtkPVContextView()
{
  this->RenderWindow = this->SynchronizedWindows->NewRenderWindow();
  this->ContextView = vtkContextView::New();
  this->ContextView->SetRenderWindow(this->RenderWindow);

  // Disable interactor on server processes (or batch processes), since
  // otherwise the vtkContextInteractorStyle triggers renders on changes to the
  // vtkContextView which is bad and can cause deadlock (BUG #122651).
  if (this->SynchronizedWindows->GetMode() !=
    vtkPVSynchronizedRenderWindows::BUILTIN &&
    this->SynchronizedWindows->GetMode() !=
    vtkPVSynchronizedRenderWindows::CLIENT)
    {
    vtkContextInteractorStyle* style = vtkContextInteractorStyle::SafeDownCast(
      this->ContextView->GetInteractor()->GetInteractorStyle());
    if (style)
      {
      style->SetScene(NULL);
      }
    this->ContextView->SetInteractor(NULL);
    }

}

//----------------------------------------------------------------------------
vtkPVContextView::~vtkPVContextView()
{
  vtkTileDisplayHelper::GetInstance()->EraseTile(this->Identifier);

  this->RenderWindow->Delete();
  this->ContextView->Delete();
}

//----------------------------------------------------------------------------
void vtkPVContextView::Initialize(unsigned int id)
{
  if (this->Identifier == id)
    {
    // already initialized
    return;
    }
  this->SynchronizedWindows->AddRenderWindow(id, this->RenderWindow);
  this->SynchronizedWindows->AddRenderer(id, this->ContextView->GetRenderer());
  this->Superclass::Initialize(id);
}

//----------------------------------------------------------------------------
void vtkPVContextView::Update()
{
  vtkMultiProcessController* s_controller =
    this->SynchronizedWindows->GetClientServerController();
  vtkMultiProcessController* d_controller =
    this->SynchronizedWindows->GetClientDataServerController();
  vtkMultiProcessController* p_controller =
    vtkMultiProcessController::GetGlobalController();
  vtkMultiProcessStream stream;

  if (this->SynchronizedWindows->GetLocalProcessIsDriver())
    {
    std::vector<int> need_delivery;
    int num_reprs = this->GetNumberOfRepresentations();
    for (int cc=0; cc < num_reprs; cc++)
      {
      vtkPVDataRepresentation* repr = vtkPVDataRepresentation::SafeDownCast(
        this->GetRepresentation(cc));
      if (repr && repr->GetNeedUpdate())
        {
        need_delivery.push_back(cc);
        }
      }
    stream << static_cast<int>(need_delivery.size());
    for (size_t cc=0; cc < need_delivery.size(); cc++)
      {
      stream << need_delivery[cc];
      }

    if (s_controller)
      {
      s_controller->Send(stream, 1, 9998878);
      }
    if (d_controller)
      {
      d_controller->Send(stream, 1, 9998878);
      }
    if (p_controller)
      {
      p_controller->Broadcast(stream, 0);
      }
    }
  else
    {
    if (s_controller)
      {
      s_controller->Receive(stream, 1, 9998878);
      }
    if (d_controller)
      {
      d_controller->Receive(stream, 1, 9998878);
      }
    if (p_controller)
      {
      p_controller->Broadcast(stream, 0);
      }
    }

  int size;
  stream >> size;
  for (int cc=0; cc < size; cc++)
    {
    int index;
    stream >> index;
    vtkPVDataRepresentation* repr = vtkPVDataRepresentation::SafeDownCast(
      this->GetRepresentation(index));
    if (repr)
      {
      repr->MarkModified();
      }
    }
  this->Superclass::Update();
}

//----------------------------------------------------------------------------
void vtkPVContextView::StillRender()
{
  vtkTimerLog::MarkStartEvent("Still Render");
  this->Render(false);
  vtkTimerLog::MarkEndEvent("Still Render");
}

//----------------------------------------------------------------------------
void vtkPVContextView::InteractiveRender()
{
  vtkTimerLog::MarkStartEvent("Interactive Render");
  this->Render(true);
  vtkTimerLog::MarkEndEvent("Interactive Render");
}

//----------------------------------------------------------------------------
void vtkPVContextView::Render(bool vtkNotUsed(interactive))
{
  this->SynchronizedWindows->SetEnabled(this->InTileDisplayMode());
  this->SynchronizedWindows->BeginRender(this->GetIdentifier());

  // Call Render() on local render window only on the client (or root node in
  // batch mode).
 if (this->SynchronizedWindows->GetLocalProcessIsDriver())
   {
   this->ContextView->Render();
   }

 if (!this->SynchronizedWindows->GetLocalProcessIsDriver() &&
   this->InTileDisplayMode())
   {

   int old = this->RenderWindow->GetSwapBuffers();
   this->RenderWindow->SetSwapBuffers(0);
   this->ContextView->Render();
   vtkSynchronizedRenderers::vtkRawImage image;
   image.Capture(this->ContextView->GetRenderer());
   this->RenderWindow->SetSwapBuffers(old);

   double viewport[4];
   this->ContextView->GetRenderer()->GetViewport(viewport);

   int tile_dims[2], tile_mullions[2];
   this->SynchronizedWindows->GetTileDisplayParameters(tile_dims, tile_mullions);

   double tile_viewport[4];
   this->GetRenderWindow()->GetTileViewport(tile_viewport);

   double physical_viewport[4];
   vtkSmartPointer<vtkTilesHelper> tilesHelper = vtkSmartPointer<vtkTilesHelper>::New();
   tilesHelper->SetTileDimensions(tile_dims);
   tilesHelper->SetTileMullions(tile_mullions);
   tilesHelper->SetTileWindowSize(this->GetRenderWindow()->GetActualSize());
   if (tilesHelper->GetPhysicalViewport(viewport,
     vtkMultiProcessController::GetGlobalController()->GetLocalProcessId(),
     physical_viewport))
     {
     vtkTileDisplayHelper::GetInstance()->SetTile(
       this->Identifier,
       physical_viewport,
       this->ContextView->GetRenderer(),
       image);
     }
   else
     {
     vtkTileDisplayHelper::GetInstance()->EraseTile(this->Identifier);
     }

   vtkTileDisplayHelper::GetInstance()->FlushTiles(this->Identifier,
     this->ContextView->GetRenderer()->GetActiveCamera()->GetLeftEye());
   }

 this->SynchronizedWindows->SetEnabled(false);
}

#include <math.h>
int ComputeMagnification(const int full_size[2], int window_size[2])
{
  int magnification = 1;

  // If fullsize > viewsize, then magnification is involved.
  int temp = static_cast<int>(ceil(
      static_cast<double>(full_size[0])/static_cast<double>(window_size[0])));
  magnification = (temp> magnification)? temp: magnification;

  temp = static_cast<int>(ceil(
    static_cast<double>(full_size[1])/static_cast<double>(window_size[1])));
  magnification = (temp > magnification)? temp : magnification;
  window_size[0] = full_size[0]/magnification;
  window_size[1] = full_size[1]/magnification;
  return magnification;
}

//----------------------------------------------------------------------------
void vtkPVContextView::SendImageToRenderServers()
{
  int size[2];
  this->SynchronizedWindows->GetClientServerController()->Receive(
    size, 2, 1, 238903);
  int actual_size[2], prev_size[2];
  actual_size[0] = this->GetRenderWindow()->GetSize()[0];
  actual_size[1] = this->GetRenderWindow()->GetSize()[1];
  prev_size[0] = actual_size[0];
  prev_size[1] = actual_size[1];

  int magnification = ComputeMagnification(size, actual_size);
  this->RenderWindow->SetSize(actual_size);

  this->ContextView->Render();

  vtkWindowToImageFilter* w2i = vtkWindowToImageFilter::New();
  w2i->SetInput(this->GetRenderWindow());
  w2i->SetMagnification(magnification);
  w2i->ReadFrontBufferOff();
  w2i->ShouldRerenderOff();
  w2i->Update();

  this->SynchronizedWindows->BroadcastToRenderServer(w2i->GetOutput());
  //vtkPNGWriter* writer = vtkPNGWriter::New();
  //writer->SetFileName("/tmp/client.png");
  //writer->SetInput(w2i->GetOutput());
  //writer->Write();
  //writer->Delete();

  this->RenderWindow->SetSize(prev_size);
  w2i->Delete();
}

namespace
{
  int vtkMinInt(double x, double y)
    {
    return static_cast<int>(x < y? x : y);
    }
}
//----------------------------------------------------------------------------
void vtkPVContextView::ReceiveImageToFromClient()
{
  double viewport[4];
  this->ContextView->GetRenderer()->GetViewport(viewport);

  int size[2];
  size[0] = this->GetRenderWindow()->GetSize()[0];
  size[1] = this->GetRenderWindow()->GetSize()[1];
  size[0] *= static_cast<int>(viewport[2]-viewport[0]);
  size[1] *= static_cast<int>(viewport[3]-viewport[1]);
  if (this->SynchronizedWindows->GetClientServerController())
    {
    this->SynchronizedWindows->GetClientServerController()->Send(
      size, 2, 1, 238903);
    }

  vtkImageData* image = vtkImageData::New();
  this->SynchronizedWindows->BroadcastToRenderServer(image);
  //vtkPNGWriter* writer = vtkPNGWriter::New();
  //writer->SetFileName("/tmp/server.1.png");
  //writer->SetInput(image);
  //writer->Write();
  //writer->Delete();

  int tile_dims[2], tile_mullions[2];
  this->SynchronizedWindows->GetTileDisplayParameters(tile_dims, tile_mullions);

  double tile_viewport[4];
  this->GetRenderWindow()->GetTileViewport(tile_viewport);

  int image_dims[3];
  image->GetDimensions(image_dims);

  // Extract sub-section from that image based on what will be project on the
  // current tile.
  vtkExtractVOI* voi = vtkExtractVOI::New();
  voi->SetInput(image);
  voi->SetVOI(
    vtkMinInt(1.0, (tile_viewport[0]-viewport[0]) / (viewport[2] -
        viewport[0]))*(image_dims[0]-1),
    vtkMinInt(1.0, (tile_viewport[2]-viewport[0]) / (viewport[2] -
        viewport[0]))*(image_dims[0]-1),
    vtkMinInt(1.0, (tile_viewport[1]-viewport[1]) / (viewport[3] -
        viewport[1]))*(image_dims[1]-1),
    vtkMinInt(1.0, (tile_viewport[3]-viewport[1]) / (viewport[3] -
        viewport[1]))*(image_dims[1]-1),
    0, 0);
  voi->Update();
  image->ShallowCopy(voi->GetOutput());
  voi->Delete();

  //writer = vtkPNGWriter::New();
  //writer->SetFileName("/tmp/server.1a.png");
  //writer->SetInput(image);
  //writer->Write();
  //writer->Delete();

  double physical_viewport[4];
  vtkSmartPointer<vtkTilesHelper> tilesHelper = vtkSmartPointer<vtkTilesHelper>::New();
  tilesHelper->SetTileDimensions(tile_dims);
  tilesHelper->SetTileMullions(tile_mullions);
  tilesHelper->SetTileWindowSize(this->GetRenderWindow()->GetActualSize());
  tilesHelper->GetPhysicalViewport(viewport,
    vtkMultiProcessController::GetGlobalController()->GetLocalProcessId(),
    physical_viewport);

  vtkSynchronizedRenderers::vtkRawImage tile;
  tile.Initialize(image->GetDimensions()[0],
    image->GetDimensions()[1],
    vtkUnsignedCharArray::SafeDownCast(image->GetPointData()->GetScalars()));
  tile.MarkValid();

  vtkTileDisplayHelper::GetInstance()->SetTile(this->Identifier,
    physical_viewport,
    this->ContextView->GetRenderer(), tile);
  image->Delete();
}

//----------------------------------------------------------------------------
void vtkPVContextView::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
