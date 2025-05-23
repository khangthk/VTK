// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkPropPicker
 * @brief   pick an actor/prop using graphics hardware
 *
 * vtkPropPicker is used to pick an actor/prop given a selection
 * point (in display coordinates) and a renderer. This class uses
 * graphics hardware/rendering system to pick rapidly (as compared
 * to using ray casting as does vtkCellPicker and vtkPointPicker).
 * This class determines the actor/prop and pick position in world
 * coordinates; point and cell ids are not determined.
 *
 * @sa
 * vtkPicker vtkWorldPointPicker vtkCellPicker vtkPointPicker
 */

#ifndef vtkPropPicker_h
#define vtkPropPicker_h

#include "vtkAbstractPropPicker.h"
#include "vtkRenderingCoreModule.h" // For export macro
#include "vtkWrappingHints.h"       // For VTK_MARSHALAUTO

VTK_ABI_NAMESPACE_BEGIN
class vtkProp;
class vtkWorldPointPicker;

class VTKRENDERINGCORE_EXPORT VTK_MARSHALAUTO vtkPropPicker : public vtkAbstractPropPicker
{
public:
  static vtkPropPicker* New();

  vtkTypeMacro(vtkPropPicker, vtkAbstractPropPicker);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Perform the pick and set the PickedProp ivar. If something is picked, a
   * 1 is returned, otherwise 0 is returned.  Use the GetViewProp() method
   * to get the instance of vtkProp that was picked.  Props are picked from
   * the renderers list of pickable Props.
   */
  int PickProp(double selectionX, double selectionY, vtkRenderer* renderer);

  /**
   * Perform a pick from the user-provided list of vtkProps and not from the
   * list of vtkProps that the render maintains.
   */
  int PickProp(
    double selectionX, double selectionY, vtkRenderer* renderer, vtkPropCollection* pickfrom);

  /**
   * override superclasses' Pick() method.
   */
  int Pick(double selectionX, double selectionY, double selectionZ, vtkRenderer* renderer) override;
  int Pick(double selectionPt[3], vtkRenderer* renderer)
  {
    return this->Pick(selectionPt[0], selectionPt[1], selectionPt[2], renderer);
  }

  /**
   * Perform pick operation with selection point provided. The
   * selectionPt is in world coordinates.
   * Return non-zero if something was successfully picked.
   */
  int Pick3DPoint(double selectionPt[3], vtkRenderer* ren) override;

  /**
   * Perform the pick and set the PickedProp ivar. If something is picked, a
   * 1 is returned, otherwise 0 is returned.  Use the GetViewProp() method
   * to get the instance of vtkProp that was picked.  Props are picked from
   * the renderers list of pickable Props.
   */
  int PickProp3DPoint(double pos[3], vtkRenderer* renderer);

  /**
   * Perform a pick from the user-provided list of vtkProps and not from the
   * list of vtkProps that the render maintains.
   */
  int PickProp3DPoint(double pos[3], vtkRenderer* renderer, vtkPropCollection* pickfrom);

  /**
   * Perform a pick from the user-provided list of vtkProps.
   */
  virtual int PickProp3DRay(double selectionPt[3], double eventWorldOrientation[4],
    vtkRenderer* renderer, vtkPropCollection* pickfrom);

  /**
   * Perform pick operation with selection point provided. The
   * selectionPt is in world coordinates.
   * Return non-zero if something was successfully picked.
   */
  int Pick3DRay(double selectionPt[3], double orient[4], vtkRenderer* ren) override;

protected:
  vtkPropPicker();
  ~vtkPropPicker() override;

  void Initialize() override;

  vtkPropCollection* PickFromProps;

  // Used to get x-y-z pick position
  vtkWorldPointPicker* WorldPointPicker;

private:
  vtkPropPicker(const vtkPropPicker&) = delete;
  void operator=(const vtkPropPicker&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
