// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkAxisFollower
 * @brief   a subclass of vtkFollower that ensures that
 * data is always parallel to the axis defined by a vtkAxisActor.
 *
 * vtkAxisFollower is a subclass of vtkFollower that always follows its
 * specified axis. More specifically it will not change its position or scale,
 * but it will continually update its orientation so that it is aligned with
 * the axis and facing at angle to the camera to provide maximum visibility.
 * This is typically used for text labels for 3d plots.
 * @sa
 * vtkActor vtkFollower vtkCamera vtkAxisActor vtkCubeAxesActor
 */

#ifndef vtkAxisFollower_h
#define vtkAxisFollower_h

#include "vtkFollower.h"
#include "vtkRenderingAnnotationModule.h" // For export macro
#include "vtkWrappingHints.h"             // For VTK_MARSHALAUTO

#include "vtkWeakPointer.h" // For vtkWeakPointer

// Forward declarations.
VTK_ABI_NAMESPACE_BEGIN
class vtkAxisActor;
class vtkRenderer;

class VTKRENDERINGANNOTATION_EXPORT VTK_MARSHALAUTO vtkAxisFollower : public vtkFollower
{
public:
  vtkTypeMacro(vtkAxisFollower, vtkFollower);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Creates a follower with no camera set
   */
  static vtkAxisFollower* New();

  ///@{
  /**
   * Set axis that needs to be followed.
   */
  virtual void SetAxis(vtkAxisActor*);
  virtual vtkAxisActor* GetAxis();
  ///@}

  ///@{
  /**
   * Set/Get state of auto center mode where additional
   * translation will be added to make sure the underlying
   * geometry has its pivot point at the center of its bounds.
   */
  vtkSetMacro(AutoCenter, vtkTypeBool);
  vtkGetMacro(AutoCenter, vtkTypeBool);
  vtkBooleanMacro(AutoCenter, vtkTypeBool);
  ///@}

  ///@{
  /**
   * Enable / disable use of distance based LOD. If enabled the actor
   * will not be visible at a certain distance from the camera.
   * Default is false.
   */
  vtkSetMacro(EnableDistanceLOD, int);
  vtkGetMacro(EnableDistanceLOD, int);
  ///@}

  ///@{
  /**
   * Set distance LOD threshold (0.0 - 1.0).This determines at what fraction
   * of camera far clip range, actor is not visible.
   * Default is 0.80.
   */
  vtkSetClampMacro(DistanceLODThreshold, double, 0.0, 1.0);
  vtkGetMacro(DistanceLODThreshold, double);
  ///@}

  ///@{
  /**
   * Enable / disable use of view angle based LOD. If enabled the actor
   * will not be visible at a certain view angle.
   * Default is true.
   */
  vtkSetMacro(EnableViewAngleLOD, int);
  vtkGetMacro(EnableViewAngleLOD, int);
  ///@}

  ///@{
  /**
   * Set view angle LOD threshold (0.0 - 1.0).This determines at what view
   * angle to geometry will make the geometry not visible.
   * Default is 0.34.
   */
  vtkSetClampMacro(ViewAngleLODThreshold, double, 0.0, 1.0);
  vtkGetMacro(ViewAngleLODThreshold, double);
  ///@}

  ///@{
  /**
   * Set/Get the desired screen offset from the axis.
   * Convenience method, using a zero horizontal offset
   */
  double GetScreenOffset();
  void SetScreenOffset(double offset);
  ///@}

  ///@{
  /**
   * Set/Get the desired screen offset from the axis.
   * first component is horizontal, second is vertical.
   */
  vtkSetVector2Macro(ScreenOffsetVector, double);
  vtkGetVector2Macro(ScreenOffsetVector, double);
  ///@}

  ///@{
  /**
   * This causes the actor to be rendered. It in turn will render the actor's
   * property, texture map and then mapper. If a property hasn't been
   * assigned, then the actor will create one automatically.
   */
  void Render(vtkRenderer* ren) override;
  ///@}

  /**
   * Overridden to disable this function, and use ComputeTransformMatrix instead, as
   * we need a renderer to compute the transform matrix
   */
  void ComputeMatrix() override {}

  /**
   * Generate the matrix based on ivars. This method overloads its superclasses
   * ComputeMatrix() method due to the special vtkFollower matrix operations.
   */
  virtual void ComputeTransformMatrix(vtkRenderer* ren);

  /**
   * Shallow copy of a follower. Overloads the virtual vtkProp method.
   */
  void ShallowCopy(vtkProp* prop) override;

  /**
   * Calculate scale factor to maintain same size of a object
   * on the screen.
   */
  static double AutoScale(
    vtkViewport* viewport, vtkCamera* camera, double screenSize, double position[3]);

protected:
  vtkAxisFollower();
  ~vtkAxisFollower() override;

  void CalculateOrthogonalVectors(
    double Rx[3], double Ry[3], double Rz[3], vtkAxisActor* axis1, double* dop, vtkRenderer* ren);

  void ComputeRotationAndTranlation(vtkRenderer* ren, double translation[3], double Rx[3],
    double Ry[3], double Rz[3], vtkAxisActor* axis);

  // \NOTE: Not used as of now.
  VTK_DEPRECATED_IN_9_6_0("Unmaintained method, please do not use.")
  void ComputerAutoCenterTranslation(const double& autoScaleFactor, double translation[3]);

  int TestDistanceVisibility();
  void ExecuteViewAngleVisibility(double normal[3]);

  bool IsTextUpsideDown(double* a, double* b);

  vtkTypeBool AutoCenter;

  int EnableDistanceLOD;
  double DistanceLODThreshold;

  int EnableViewAngleLOD;
  double ViewAngleLODThreshold;

  double ScreenOffsetVector[2];

  vtkWeakPointer<vtkAxisActor> Axis;

private:
  int TextUpsideDown;
  int VisibleAtCurrentViewAngle;

  vtkAxisFollower(const vtkAxisFollower&) = delete;
  void operator=(const vtkAxisFollower&) = delete;

  // hide the two parameter Render() method from the user and the compiler.
  void Render(vtkRenderer*, vtkMapper*) override {}
};

VTK_ABI_NAMESPACE_END
#endif // vtkAxisFollower_h
