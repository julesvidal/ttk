#include <ttkStringArrayConverter.h>

#include <vtkDataObject.h> // For port info
#include <vtkObjectFactory.h> // for new macro

#include <vtkFieldData.h>
#include <vtkPointData.h>
#include <vtkStringArray.h>

#include <ttkUtils.h>

#include <map>
#include <set>

vtkStandardNewMacro(ttkStringArrayConverter);

ttkStringArrayConverter::ttkStringArrayConverter() {
  this->setDebugMsgPrefix("StringArrayConverter");

  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

int ttkStringArrayConverter::FillInputPortInformation(int port,
                                                      vtkInformation *info) {
  if(port == 0) {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkDataSet");
    return 1;
  }
  return 0;
}

int ttkStringArrayConverter::FillOutputPortInformation(int port,
                                                       vtkInformation *info) {
  if(port == 0) {
    info->Set(ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT(), 0);
    return 1;
  }
  return 0;
}

int ttkStringArrayConverter::RequestData(vtkInformation *request,
                                         vtkInformationVector **inputVector,
                                         vtkInformationVector *outputVector) {
  const auto input = vtkDataSet::GetData(inputVector[0]);
  auto output = vtkDataSet::GetData(outputVector);

  if(input == nullptr || output == nullptr) {
    this->printErr("Null input data, aborting");
    return 0;
  }

  // point data
  const auto pd = input->GetPointData();
  // string array
  const auto sa = vtkStringArray::SafeDownCast(
    pd->GetAbstractArray(this->InputStringArray.data()));

  if(sa == nullptr) {
    this->printErr("Cannot find any string array with the name "
                   + this->InputStringArray);
    return 0;
  }

  const auto nvalues = sa->GetNumberOfTuples();

  std::set<std::string> values{};

  for(vtkIdType i = 0; i < nvalues; ++i) {
    values.emplace(sa->GetValue(i));
  }

  std::map<std::string, size_t> valInd{};

  {
    size_t i = 0;
    for(const auto &el : values) {
      valInd[el] = i;
      i++;
    }
  }

  // shallow-copy input
  output->ShallowCopy(input);

  // filter output point data
  const auto pdo = output->GetPointData();
  pdo->RemoveArray(this->InputStringArray.data());
  vtkNew<vtkIdTypeArray> ia{};
  ia->SetName(this->InputStringArray.data());
  ia->SetNumberOfTuples(nvalues);
  for(vtkIdType i = 0; i < nvalues; ++i) {
    ia->SetValue(i, valInd[sa->GetValue(i)]);
  }

  pdo->AddArray(ia);

  // store correspondance map in output field data
  vtkNew<vtkStringArray> strArray{};
  vtkNew<vtkIdTypeArray> indArray{};
  strArray->SetName("String Values");
  indArray->SetName("Replacement Values");
  strArray->SetNumberOfComponents(1);
  indArray->SetNumberOfComponents(1);

  for(const auto &cpl : valInd) {
    strArray->InsertNextValue(cpl.first.data());
    indArray->InsertNextValue(cpl.second);
  }

  const auto fd = output->GetFieldData();
  fd->AddArray(strArray);
  fd->AddArray(indArray);

  return 1;
}
