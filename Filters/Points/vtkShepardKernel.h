// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkShepardKernel
 * @brief   a Shepard method interpolation kernel
 *
 *
 * vtkShepardKernel is an interpolation kernel that uses the method of
 * Shepard to perform interpolation. The weights are computed as 1/r^p, where
 * r is the distance to a neighbor point within the kernel radius R; and p
 * (the power parameter) is a positive exponent (typically p=2).
 *
 * @warning
 * The weights are normalized sp that SUM(Wi) = 1. If a neighbor point p
 * precisely lies on the point to be interpolated, then the interpolated
 * point takes on the values associated with p.
 *
 * @sa
 * vtkPointInterpolator vtkPointInterpolator2D vtkInterpolationKernel
 * vtkGaussianKernel vtkSPHKernel vtkShepardKernel
 */

#ifndef vtkShepardKernel_h
#define vtkShepardKernel_h

#include "vtkFiltersPointsModule.h" // For export macro
#include "vtkGeneralizedKernel.h"

VTK_ABI_NAMESPACE_BEGIN
class vtkIdList;
class vtkDoubleArray;

class VTKFILTERSPOINTS_EXPORT vtkShepardKernel : public vtkGeneralizedKernel
{
public:
  ///@{
  /**
   * Standard methods for instantiation, obtaining type information, and printing.
   */
  static vtkShepardKernel* New();
  vtkTypeMacro(vtkShepardKernel, vtkGeneralizedKernel);
  void PrintSelf(ostream& os, vtkIndent indent) override;
  ///@}

  // Reuse any superclass signatures that we don't override.
  using vtkGeneralizedKernel::ComputeWeights;

  /**
   * Given a point x, a list of basis points pIds, and a probability
   * weighting function prob, compute interpolation weights associated with
   * these basis points.  Note that basis points list pIds, the probability
   * weighting prob, and the weights array are provided by the caller of the
   * method, and may be dynamically resized as necessary. The method returns
   * the number of weights (pIds may be resized in some cases). Typically
   * this method is called after ComputeBasis(), although advanced users can
   * invoke ComputeWeights() and provide the interpolation basis points pIds
   * directly. The probably weighting prob are numbers 0<=prob<=1 which are
   * multiplied against the interpolation weights before normalization. They
   * are estimates of local confidence of weights. The prob may be nullptr in
   * which all probabilities are considered =1.
   */
  vtkIdType ComputeWeights(
    double x[3], vtkIdList* pIds, vtkDoubleArray* prob, vtkDoubleArray* weights) override;

  ///@{
  /**
   * Set / Get the power parameter p. By default p=2. Values (which must be
   * a positive, real value) != 2 may affect performance significantly.
   */
  vtkSetClampMacro(PowerParameter, double, 0.001, 100);
  vtkGetMacro(PowerParameter, double);
  ///@}

protected:
  vtkShepardKernel();
  ~vtkShepardKernel() override;

  // The exponent of the weights, =2 by default (l2 norm)
  double PowerParameter;

private:
  vtkShepardKernel(const vtkShepardKernel&) = delete;
  void operator=(const vtkShepardKernel&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
