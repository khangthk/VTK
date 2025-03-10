// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkCleanUnstructuredGrid.h"
#include "vtkExtractSurface.h"
#include "vtkForceStaticMesh.h"
#include "vtkGenerateTimeSteps.h"
#include "vtkHDFReader.h"
#include "vtkHDFWriter.h"
#include "vtkLogger.h"
#include "vtkNew.h"
#include "vtkPointDataToCellData.h"
#include "vtkPolyData.h"
#include "vtkSpatioTemporalHarmonicsSource.h"
#include "vtkTestUtilities.h"
#include "vtkTesting.h"
#include "vtkTransform.h"
#include "vtkTransformFilter.h"
#include "vtkUnstructuredGrid.h"

namespace
{
enum supportedDataSetTypes
{
  vtkUnstructuredGridType,
  vtkPolyDataType
};

struct WriterConfigOptions
{
  bool UseExternalTimeSteps;
  bool UseExternalPartitions;
  std::string FileNameSuffix;
};
}

//----------------------------------------------------------------------------
bool TestTemporalData(const std::string& tempDir, const std::string& dataRoot,
  const std::string& baseName, const WriterConfigOptions& config)
{
  // Open original temporal HDF data
  const std::string basePath = dataRoot + "/Data/" + baseName;
  vtkNew<vtkHDFReader> baseHDFReader;
  baseHDFReader->SetFileName(basePath.c_str());

  // Write the data to a file using the vtkHDFWriter
  vtkNew<vtkHDFWriter> HDFWriter;
  HDFWriter->SetInputConnection(baseHDFReader->GetOutputPort());
  std::string tempPath = tempDir + "/HDFWriter_";
  tempPath += baseName + ".vtkhdf" + config.FileNameSuffix;
  HDFWriter->SetFileName(tempPath.c_str());
  HDFWriter->SetUseExternalTimeSteps(config.UseExternalTimeSteps);
  HDFWriter->SetUseExternalPartitions(config.UseExternalPartitions);
  HDFWriter->SetWriteAllTimeSteps(true);
  HDFWriter->SetChunkSize(100);
  HDFWriter->SetCompressionLevel(4);
  HDFWriter->Write();

  vtkLog(INFO,
    "Testing " << tempPath << " with options Ext time steps: " << config.UseExternalTimeSteps
               << " ext partitions: " << config.UseExternalPartitions);
  // Read the data just written
  vtkNew<vtkHDFReader> HDFReader;
  if (!HDFReader->CanReadFile(tempPath.c_str()))
  {
    vtkLog(ERROR, "vtkHDFReader can not read file: " << tempPath);
    return false;
  }
  HDFReader->SetFileName(tempPath.c_str());
  HDFReader->Update();
  // Read the original data from the beginning
  vtkNew<vtkHDFReader> HDFReaderBaseline;
  HDFReaderBaseline->SetFileName(basePath.c_str());
  HDFReaderBaseline->Update();
  // Make sure both have the same number of timesteps
  int totalTimeStepsXML = HDFReaderBaseline->GetNumberOfSteps();
  int totalTimeStepsHDF = HDFReader->GetNumberOfSteps();
  if (totalTimeStepsXML != totalTimeStepsHDF)
  {
    vtkLog(ERROR,
      "total time steps in both HDF files do not match: " << totalTimeStepsHDF << " instead of "
                                                          << totalTimeStepsXML);
    return false;
  }

  // Compare the data at each timestep from both readers
  for (int i = 0; i < totalTimeStepsXML; i++)
  {
    std::cout << "Comparing timestep " << i << std::endl;
    HDFReaderBaseline->SetStep(i);
    HDFReaderBaseline->Update();

    HDFReader->SetStep(i);
    HDFReader->Update();

    // Time values must be the same
    if (HDFReader->GetTimeValue() != HDFReaderBaseline->GetTimeValue())
    {
      vtkLog(ERROR,
        "timestep value does not match : " << HDFReader->GetTimeValue() << " instead of "
                                           << HDFReaderBaseline->GetTimeValue());
      return false;
    }

    if (!vtkTestUtilities::CompareDataObjects(
          HDFReaderBaseline->GetOutput(), HDFReader->GetOutput()))
    {
      vtkLog(ERROR, "data objects do not match");
      return false;
    }
  }
  return true;
}

//----------------------------------------------------------------------------
bool TestTemporalStaticMesh(
  const std::string& tempDir, const std::string& baseName, int dataSetType)
{
  /*
   * At the time this test has been written, the reader only support static mesh for partitioned
   * data set. We can't use use both the merge parts & the cache at the same time, which cause every
   * static to be read as a partitioned dataset with at least one partition. The writer doesn't
   * support writing partitioned dataset yet so we can't test static mesh writing properly since we
   * can't read non partitioned static data.
   */
  // Custom static mesh source
  vtkNew<vtkSpatioTemporalHarmonicsSource> harmonics;
  harmonics->ClearHarmonics();
  harmonics->AddHarmonic(1, 0, 0.6283, 0.6283, 0.6283, 0);
  harmonics->AddHarmonic(3, 0, 0.6283, 0, 0, 1.5708);
  harmonics->AddHarmonic(2, 0, 0, 0.6283, 0, 3.1416);
  harmonics->AddHarmonic(1, 0, 0, 0, 0.6283, 4.1724);

  vtkSmartPointer<vtkAlgorithm> datasetTypeSpecificFilter;

  if (dataSetType == ::supportedDataSetTypes::vtkUnstructuredGridType)
  {
    datasetTypeSpecificFilter =
      vtkSmartPointer<vtkCleanUnstructuredGrid>::Take(vtkCleanUnstructuredGrid::New());
  }
  else if (dataSetType == ::supportedDataSetTypes::vtkPolyDataType)
  {
    datasetTypeSpecificFilter = vtkSmartPointer<vtkExtractSurface>::Take(vtkExtractSurface::New());
  }
  datasetTypeSpecificFilter->SetInputConnection(0, harmonics->GetOutputPort(0));

  vtkNew<vtkPointDataToCellData> pointDataToCellData;
  pointDataToCellData->SetPassPointData(true);
  pointDataToCellData->SetInputConnection(0, datasetTypeSpecificFilter->GetOutputPort(0));

  vtkNew<vtkForceStaticMesh> staticMesh;
  staticMesh->SetInputConnection(0, pointDataToCellData->GetOutputPort(0));

  // Write the data to a file using the vtkHDFWriter
  vtkNew<vtkHDFWriter> HDFWriter;
  HDFWriter->SetInputConnection(staticMesh->GetOutputPort());
  std::string tempPath = tempDir + "/HDFWriter_";
  tempPath += baseName + ".vtkhdf";
  HDFWriter->SetFileName(tempPath.c_str());
  HDFWriter->SetWriteAllTimeSteps(true);
  HDFWriter->SetCompressionLevel(1);
  if (!HDFWriter->Write())
  {
    vtkLog(ERROR, "An error occurred while writing the static mesh HDF file");
    return false;
  }
  /* TODO
   * Once the reader supports both MergeParts & UseCache used together,
   * this test will need to be updated by reading the output file and checking
   * it corresponds to the source, as well as checking the MeshMTime values.
   */
  return true;
}

//----------------------------------------------------------------------------
int TestHDFWriterTemporal(int argc, char* argv[])
{
  // Get temporary testing directory
  char* tempDirCStr =
    vtkTestUtilities::GetArgOrEnvOrDefault("-T", argc, argv, "VTK_TEMP_DIR", "Testing/Temporary");
  std::string tempDir{ tempDirCStr };
  delete[] tempDirCStr;

  // Get data directory
  vtkNew<vtkTesting> testHelper;
  testHelper->AddArguments(argc, argv);
  if (!testHelper->IsFlagSpecified("-D"))
  {
    vtkLog(ERROR, "-D /path/to/data was not specified.");
    return EXIT_FAILURE;
  }
  std::string dataRoot = testHelper->GetDataRoot();
  bool result = true;

  // Run tests : read data, write it, read the written data and compare to the original
  std::vector<std::string> baseNames = { "transient_sphere.hdf",
    "temporal_unstructured_grid.vtkhdf", "transient_harmonics.hdf" };
  std::vector<WriterConfigOptions> configs{ { false, false, "_NoExtTimeNoExtPart" },
    { false, true, "_NoExtTimeExtPart" }, { true, false, "_ExtTimeNoExtPart" },
    { true, true, "_ExtTimeExtPart" } };

  // Test the whole matrix "file" x "config"
  for (const auto& config : configs)
  {
    for (const auto& fileName : baseNames)
    {
      result &= TestTemporalData(tempDir, dataRoot, fileName, config);
    }
  }

  result &= TestTemporalStaticMesh(
    tempDir, "transient_static_sphere_ug_source", ::supportedDataSetTypes::vtkUnstructuredGridType);
  result &= TestTemporalStaticMesh(
    tempDir, "transient_static_sphere_polydata_source", ::supportedDataSetTypes::vtkPolyDataType);
  return result ? EXIT_SUCCESS : EXIT_FAILURE;
}
