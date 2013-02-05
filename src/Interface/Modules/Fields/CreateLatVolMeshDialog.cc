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

#include <Interface/Modules/Fields/CreateLatVolMeshDialog.h>
#include <Modules/Fields/CreateLatVolMesh.h>
#include <Dataflow/Network/ModuleStateInterface.h>  //TODO: extract into intermediate

using namespace SCIRun::Gui;
using namespace SCIRun::Dataflow::Networks;
using namespace SCIRun::Modules::Fields;

CreateLatVolMeshDialog::CreateLatVolMeshDialog(const std::string& name, ModuleStateHandle state,
  QWidget* parent /* = 0 */)
  : ModuleDialogGeneric(state, parent)
{
  setupUi(this);
  setWindowTitle(QString::fromStdString(name));
  fixSize();
  //executeButton_->setEnabled(false);
  
  //connect(executeButton_, SIGNAL(clicked()), this, SIGNAL(executeButtonPressed()));
  //TODO: here is where to start on standardizing module dialog buttons.
  //connect(buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), this, SLOT(pushMatrixToState()));
  xSizeSpinBox_->setValue(16);
  ySizeSpinBox_->setValue(16);
  zSizeSpinBox_->setValue(16);
  dataAtNodesButton_->setChecked(true);
  elementSizeNormalizedButton_->setChecked(true);
  push();

  connect(xSizeSpinBox_, SIGNAL(valueChanged(int)), this, SLOT(push()));
  connect(ySizeSpinBox_, SIGNAL(valueChanged(int)), this, SLOT(push()));
  connect(zSizeSpinBox_, SIGNAL(valueChanged(int)), this, SLOT(push()));
}

void CreateLatVolMeshDialog::push()
{
  std::cout << "CLVMD::push()" << std::endl;
  state_->setValue(CreateLatVolMesh::XSize, xSizeSpinBox_->value());
  state_->setValue(CreateLatVolMesh::YSize, ySizeSpinBox_->value());
  state_->setValue(CreateLatVolMesh::ZSize, zSizeSpinBox_->value());
}

void CreateLatVolMeshDialog::pull()
{
  std::cout << "CLVMD::pull()" << std::endl;
  //matrixTextEdit_->setPlainText(QString::fromStdString(state_->getValue(CreateMatrixModule::TextEntry).getString()));
}
