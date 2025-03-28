#!/usr/bin/env python
from vtkmodules.vtkFiltersCore import vtkHedgeHog
from vtkmodules.vtkFiltersGeometry import vtkStructuredGridGeometryFilter
from vtkmodules.vtkIOParallel import vtkMultiBlockPLOT3DReader
from vtkmodules.vtkRenderingCore import (
    vtkActor,
    vtkActor2D,
    vtkCamera,
    vtkLight,
    vtkPolyDataMapper,
    vtkRenderWindow,
    vtkRenderWindowInteractor,
    vtkRenderer,
    vtkTextMapper,
    vtkTextProperty,
)
import vtkmodules.vtkInteractionStyle
import vtkmodules.vtkRenderingFreeType
import vtkmodules.vtkRenderingOpenGL2
from vtkmodules.util.misc import vtkGetDataRoot
VTK_DATA_ROOT = vtkGetDataRoot()

#
# All Plot3D vector functions
#
# Create the RenderWindow, Renderer and both Actors
#
renWin = vtkRenderWindow()
renWin.SetMultiSamples(0)
ren1 = vtkRenderer()
ren1.SetBackground(.8, .8, .2)
renWin.AddRenderer(ren1)
iren = vtkRenderWindowInteractor()
iren.SetRenderWindow(renWin)

vectorLabels = ["Velocity", "Vorticity", "Momentum", "Pressure_Gradient"]
vectorFunctions = ["200", "201", "202", "210"]

camera = vtkCamera()

light = vtkLight()

# All text actors will share the same text prop
textProp = vtkTextProperty()
textProp.SetFontSize(10)
textProp.SetFontFamilyToArial()
textProp.SetColor(.3, 1, 1)

i = 0
for vectorFunction in vectorFunctions:
    exec("pl3d" + vectorFunction + " = vtkMultiBlockPLOT3DReader()")
    eval("pl3d" + vectorFunction).SetXYZFileName(
      VTK_DATA_ROOT + "/Data/bluntfinxyz.bin")
    eval("pl3d" + vectorFunction).SetQFileName(
      VTK_DATA_ROOT + "/Data/bluntfinq.bin")
    eval("pl3d" + vectorFunction).SetVectorFunctionNumber(int(vectorFunction))
    eval("pl3d" + vectorFunction).Update()

    output = eval("pl3d" + vectorFunction).GetOutput().GetBlock(0)

    exec("plane" + vectorFunction + " = vtkStructuredGridGeometryFilter()")
    eval("plane" + vectorFunction).SetInputData(output)
    eval("plane" + vectorFunction).SetExtent(25, 25, 0, 100, 0, 100)

    exec("hog" + vectorFunction + " = vtkHedgeHog()")
    eval("hog" + vectorFunction).SetInputConnection(
      eval("plane" + vectorFunction).GetOutputPort())

    maxnorm = output.GetPointData().GetVectors().GetMaxNorm()

    eval("hog" + vectorFunction).SetScaleFactor(1.0 / maxnorm)

    exec("mapper" + vectorFunction + " = vtkPolyDataMapper()")
    eval("mapper" + vectorFunction).SetInputConnection(
      eval("hog" + vectorFunction).GetOutputPort())

    exec("actor" + vectorFunction + " = vtkActor()")
    eval("actor" + vectorFunction).SetMapper(eval("mapper" + vectorFunction))

    exec("ren" + vectorFunction + " = vtkRenderer()")
    eval("ren" + vectorFunction).SetBackground(0.5, .5, .5)
    eval("ren" + vectorFunction).SetActiveCamera(camera)
    eval("ren" + vectorFunction).AddLight(light)

    renWin.AddRenderer(eval("ren" + vectorFunction))

    eval("ren" + vectorFunction).AddActor(eval("actor" + vectorFunction))

    exec("textMapper" + vectorFunction + " = vtkTextMapper()")
    eval("textMapper" + vectorFunction).SetInput(vectorLabels[i])
    eval("textMapper" + vectorFunction).SetTextProperty(textProp)
    exec("text" + vectorFunction + " = vtkActor2D()")

    eval("text" + vectorFunction).SetMapper(eval("textMapper" + vectorFunction))
    eval("text" + vectorFunction).SetPosition(2, 5)
    eval("ren" + vectorFunction).AddViewProp(eval("text" + vectorFunction))
    i += 1

#
# now layout renderers
column = 1
row = 1
deltaX = 1.0 / 2.0
deltaY = 1.0 / 2.0
for vectorFunction in vectorFunctions:
    eval("ren" + vectorFunction).SetViewport(
      (column - 1) * deltaX + (deltaX * .05),
      (row - 1) * deltaY + (deltaY * .05),
      column * deltaX - (deltaX * .05),
      row * deltaY - (deltaY * .05))
    column += 1
    if (column > 2):
        column = 1
        row += 1

camera.SetViewUp(1, 0, 0)
camera.SetFocalPoint(0, 0, 0)
camera.SetPosition(.4, -.5, -.75)
ren200.ResetCamera()
camera.Dolly(1.25)

for vectorFunction in vectorFunctions:
    eval("ren" + vectorFunction).ResetCameraClippingRange()

light.SetPosition(camera.GetPosition())
light.SetFocalPoint(camera.GetFocalPoint())

renWin.SetSize(350, 350)

renWin.Render()

iren.Initialize()
#iren.Start()
