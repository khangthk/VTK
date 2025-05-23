// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkOpenQubeElectronicData
 * @brief   Provides access to and storage of
 * electronic data calculated by OpenQube.
 */

#ifndef vtkOpenQubeElectronicData_h
#define vtkOpenQubeElectronicData_h

#include "vtkAbstractElectronicData.h"
#include "vtkDomainsChemistryModule.h" // For export macro
#include "vtkNew.h"                    // for vtkNew

namespace OpenQube
{
class BasisSet;
class Cube;
}

VTK_ABI_NAMESPACE_BEGIN

class vtkImageData;
class vtkDataSetCollection;

class VTKDOMAINSCHEMISTRY_EXPORT vtkOpenQubeElectronicData : public vtkAbstractElectronicData
{
public:
  static vtkOpenQubeElectronicData* New();
  vtkTypeMacro(vtkOpenQubeElectronicData, vtkAbstractElectronicData);
  void PrintSelf(ostream& os, vtkIndent indent);

  /**
   * Returns `VTK_OPEN_QUBE_ELECTRONIC_DATA`.
   */
  int GetDataObjectType() VTK_FUTURE_CONST override { return VTK_OPEN_QUBE_ELECTRONIC_DATA; }

  /**
   * Returns the number of molecular orbitals in the OpenQube::BasisSet.
   */
  vtkIdType GetNumberOfMOs();

  /**
   * Returns the number of electrons in the molecule.
   */
  unsigned int GetNumberOfElectrons();

  /**
   * Returns the vtkImageData for the requested molecular orbital. The data
   * will be calculated when first requested, and cached for later requests.
   */
  vtkImageData* GetMO(vtkIdType orbitalNumber);

  /**
   * Returns vtkImageData for the molecule's electron density. The data
   * will be calculated when first requested, and cached for later requests.
   */
  vtkImageData* GetElectronDensity();

  ///@{
  /**
   * Set/Get the OpenQube::BasisSet object used to generate the image data
   */
  vtkSetMacro(BasisSet, OpenQube::BasisSet*);
  vtkGetMacro(BasisSet, OpenQube::BasisSet*);
  ///@}

  ///@{
  /**
   * Set/Get the padding around the molecule used in determining the image
   * limits. Default: 2.0
   */
  vtkSetMacro(Padding, double);
  vtkGetMacro(Padding, double);
  ///@}

  ///@{
  /**
   * Set/Get the interval distance between grid points. Default: 0.1
   */
  vtkSetMacro(Spacing, double);
  vtkGetMacro(Spacing, double);
  ///@}

  ///@{
  /**
   * Get the collection of cached images
   */
  vtkGetNewMacro(Images, vtkDataSetCollection);
  ///@}

  /**
   * Deep copies the data object into this.
   */
  virtual void DeepCopy(vtkDataObject* obj);

protected:
  vtkOpenQubeElectronicData();
  ~vtkOpenQubeElectronicData() override;

  ///@{
  /**
   * Calculates and returns the requested vtkImageData. The data is added to
   * the cache, but the cache is not searched in this function.
   */
  vtkImageData* CalculateMO(vtkIdType orbitalNumber);
  vtkImageData* CalculateElectronDensity();
  ///@}

  /**
   * Converts an OpenQube::Cube object into vtkImageData.
   */
  void FillImageDataFromQube(OpenQube::Cube* qube, vtkImageData* image);

  /**
   * Cache of calculated image data.
   */
  vtkNew<vtkDataSetCollection> Images;

  /**
   * The OpenQube::BasisSet object used to calculate the images.
   */
  OpenQube::BasisSet* BasisSet;

  /**
   * Used to determine the spacing of the image data.
   */
  double Spacing;

private:
  vtkOpenQubeElectronicData(const vtkOpenQubeElectronicData&) = delete;
  void operator=(const vtkOpenQubeElectronicData&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
