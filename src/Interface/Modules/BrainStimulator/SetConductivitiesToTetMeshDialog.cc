/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

#include <Interface/Modules/BrainStimulator/SetConductivitiesToTetMeshDialog.h>
#include <Core/Algorithms/BrainStimulator/SetConductivitiesToTetMeshAlgorithm.h>

using namespace SCIRun::Gui;
using namespace SCIRun::Dataflow::Networks;
using namespace SCIRun::Core::Algorithms::BrainStimulator;

SetConductivitiesToTetMeshDialog::SetConductivitiesToTetMeshDialog(const std::string& name, ModuleStateHandle state,
  QWidget* parent /* = 0 */)
  : ModuleDialogGeneric(state, parent)
{
  setupUi(this);
  setWindowTitle(QString::fromStdString(name));
  fixSize();
  
  connect(skin_,  SIGNAL(valueChanged(int)), this, SLOT(push()));
  connect(skull_, SIGNAL(valueChanged(int)), this, SLOT(push()));
  connect(CSF_,   SIGNAL(valueChanged(int)), this, SLOT(push()));
  connect(GM_,    SIGNAL(valueChanged(int)), this, SLOT(push()));
  connect(WM_,    SIGNAL(valueChanged(int)), this, SLOT(push()));
  connect(electrode_, SIGNAL(valueChanged(int)), this, SLOT(push()));
}

void SetConductivitiesToTetMeshDialog::push()
{
  if (!pulling_)
  {
    state_->setValue(SetConductivitiesToTetMeshAlgorithm::skin(), skin_->value());
    state_->setValue(SetConductivitiesToTetMeshAlgorithm::skull(), skull_->value());
    state_->setValue(SetConductivitiesToTetMeshAlgorithm::CSF(), CSF_->value());
    state_->setValue(SetConductivitiesToTetMeshAlgorithm::GM(), GM_->value());
    state_->setValue(SetConductivitiesToTetMeshAlgorithm::WM(), WM_->value());
    state_->setValue(SetConductivitiesToTetMeshAlgorithm::electrode(), electrode_->value());
  } 
}

void SetConductivitiesToTetMeshDialog::pull()
{
  Pulling p(this);
  //keepTypeCheckBox_->setChecked(state_->getValue(SetFieldDataAlgo::keepTypeCheckBox).getBool());
  
}

