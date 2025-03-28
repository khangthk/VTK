// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkImageCityBlockDistance
 * @brief   1,2 or 3D distance map.
 *
 * vtkImageCityBlockDistance creates a distance map using the city block
 * (Manhattan) distance measure.  The input is a mask.  Zero values are
 * considered boundaries.  The output pixel is the minimum of the input pixel
 * and the distance to a boundary (or neighbor value + 1 unit).
 * distance values are calculated in pixels.
 * The filter works by taking 6 passes (for 3d distance map): 2 along each
 * axis (forward and backward). Each pass keeps a running minimum distance.
 * For some reason, I preserve the sign if the distance.  If the input
 * mask is initially negative, the output distances will be negative.
 * Distances maps can have inside (negative regions)
 * and outsides (positive regions).
 */

#ifndef vtkImageCityBlockDistance_h
#define vtkImageCityBlockDistance_h

#include "vtkImageDecomposeFilter.h"
#include "vtkImagingGeneralModule.h" // For export macro

VTK_ABI_NAMESPACE_BEGIN
class VTKIMAGINGGENERAL_EXPORT vtkImageCityBlockDistance : public vtkImageDecomposeFilter
{
public:
  static vtkImageCityBlockDistance* New();
  vtkTypeMacro(vtkImageCityBlockDistance, vtkImageDecomposeFilter);
  void PrintSelf(ostream& os, vtkIndent indent) override;

protected:
  vtkImageCityBlockDistance();
  ~vtkImageCityBlockDistance() override = default;

  int IterativeRequestUpdateExtent(vtkInformation* in, vtkInformation* out) override;
  int IterativeRequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;

  void AllocateOutputScalars(
    vtkImageData* outData, int* updateExtent, int* wholeExtent, vtkInformation* outInfo);

private:
  vtkImageCityBlockDistance(const vtkImageCityBlockDistance&) = delete;
  void operator=(const vtkImageCityBlockDistance&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
