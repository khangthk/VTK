import sys
import vtkmodules.test.Testing

try:
    import numpy
except ImportError:
    print("This test requires numpy!")
    vtkmodules.test.Testing.skip()

from vtkmodules.vtkCommonCore import (
    vtkDoubleArray,
    vtkFloatArray,
    vtkIntArray,
    vtkPoints,
    vtkSOADataArrayTemplate,
)
from vtkmodules.vtkCommonDataModel import (
    vtkDataSetAttributes,
    vtkImageData,
    vtkMultiBlockDataSet,
    vtkPolyData,
    vtkStructuredGrid,
    vtkTable,
)
from vtkmodules.vtkFiltersCore import vtkElevationFilter
from vtkmodules.vtkFiltersGeneral import (
    vtkBrownianPoints,
    vtkMultiBlockDataGroupFilter,
)
from vtkmodules.vtkFiltersSources import vtkSphereSource
from vtkmodules.vtkImagingCore import vtkRTAnalyticSource
import vtkmodules.numpy_interface.dataset_adapter as dsa
import vtkmodules.numpy_interface.algorithms as algs

w = vtkRTAnalyticSource()

bp = vtkBrownianPoints()
bp.SetInputConnection(w.GetOutputPort())
bp.Update()

elev = vtkElevationFilter()
elev.SetInputConnection(bp.GetOutputPort())
elev.SetLowPoint(-10, 0, 0)
elev.SetHighPoint(10, 0, 0)
elev.SetScalarRange(0, 20)

g = vtkMultiBlockDataGroupFilter()
g.AddInputConnection(elev.GetOutputPort())
g.AddInputConnection(elev.GetOutputPort())

g.Update()

elev2 = vtkElevationFilter()
elev2.SetInputConnection(bp.GetOutputPort())
elev2.SetLowPoint(0, -10, 0)
elev2.SetHighPoint(0, 10, 0)
elev2.SetScalarRange(0, 20)

g2 = vtkMultiBlockDataGroupFilter()
g2.AddInputConnection(elev2.GetOutputPort())
g2.AddInputConnection(elev2.GetOutputPort())

g2.Update()

elev3 = vtkElevationFilter()
elev3.SetInputConnection(bp.GetOutputPort())
elev3.SetLowPoint(0, 0, -10)
elev3.SetHighPoint(0, 0, 10)
elev3.SetScalarRange(0, 20)

elev3.Update()

dobj = vtkImageData()
dobj.DeepCopy(elev3.GetOutput())
ds1 = dsa.WrapDataObject(dobj)
elev_copy = numpy.copy(ds1.PointData['Elevation'])
elev_copy[1] = numpy.nan
ghosts = numpy.zeros(ds1.GetNumberOfPoints(), dtype=numpy.uint8)
ghosts[1] = vtkDataSetAttributes.DUPLICATEPOINT
ds1.PointData.append(ghosts, vtkDataSetAttributes.GhostArrayName())
assert algs.make_point_mask_from_NaNs(ds1, elev_copy)[1] == vtkDataSetAttributes.DUPLICATEPOINT | vtkDataSetAttributes.HIDDENPOINT

cell_array = numpy.zeros(ds1.GetNumberOfCells())
cell_array[1] = numpy.nan
assert algs.make_cell_mask_from_NaNs(ds1, cell_array)[1] == vtkDataSetAttributes.HIDDENCELL

g3 = vtkMultiBlockDataGroupFilter()
g3.AddInputConnection(elev3.GetOutputPort())
g3.AddInputConnection(elev3.GetOutputPort())

g3.Update()

cd = dsa.CompositeDataSet(g.GetOutput())
randomVec = cd.PointData['BrownianVectors']
elev = cd.PointData['Elevation']

cd2 = dsa.CompositeDataSet(g2.GetOutput())
elev2 = cd2.PointData['Elevation']

cd3 = dsa.CompositeDataSet(g3.GetOutput())
elev3 = cd3.PointData['Elevation']

npa = randomVec.Arrays[0]

def compare(arr, tol):
    assert algs.all(algs.abs(arr) < tol)

# Test operators
compare(1 + randomVec - 1 - randomVec, 1E-4)

assert (1 + randomVec).DataSet is randomVec.DataSet

# Test slicing and indexing
randomVecSlice = randomVec[:9261]
compare(randomVecSlice[randomVecSlice[:,0] > 0.2] - npa[npa[:,0] > 0.2], 1E-7)
compare(randomVecSlice[algs.where(randomVecSlice[:,0] > 0.2)] - npa[numpy.where(npa[:,0] > 0.2)], 1E-7)
compare(randomVecSlice[:, 0:2] - npa[:, 0:2], 1E-6)

# Test ufunc
compare(algs.cos(randomVec) - numpy.cos(npa), 1E-7)
assert algs.cos(randomVec).DataSet is randomVec.DataSet
assert numpy.all(numpy.asarray(numpy.in1d(elev, [0,1])) == [item in [0, 1] for item in elev])

# Various numerical ops implemented in VTK
g = algs.gradient(elev)
assert algs.all(g[0] == (1, 0, 0))

v = algs.make_vector(elev, g[:,0], elev)
assert algs.all(algs.gradient(v) == [[1, 0, 1], [0, 0, 0], [0, 0, 0]])

v = algs.make_vector(elev, g[:,0], elev2)
assert algs.all(algs.curl(v) == [1, 0, 0])

v = algs.make_vector(elev, elev2, 2*elev3)
g = algs.gradient(v)
assert g.DataSet is v.DataSet
assert algs.all(algs.det(g) == 2)

assert algs.all(algs.eigenvalue(g) == [2, 1, 1])

assert algs.all(randomVec[:,0] == randomVec[:,0])

int_array1 = numpy.array([1, 0, 1], dtype=int)
int_array2 = numpy.array([0, 1, 0], dtype=int)
assert algs.all(algs.bitwise_or(int_array1, int_array2) == 1)
assert algs.all(algs.bitwise_or(int_array1, dsa.NoneArray) == int_array1)
assert algs.all(algs.bitwise_or(dsa.NoneArray, int_array1) == int_array1)

comp_array1 = dsa.VTKCompositeDataArray([int_array1, int_array2])
comp_array2 = dsa.VTKCompositeDataArray([int_array2, int_array1])
comp_array3 = dsa.VTKCompositeDataArray([int_array2, dsa.NoneArray])
assert algs.all(algs.bitwise_or(comp_array1, comp_array2) == 1)
assert algs.all(algs.bitwise_or(comp_array1, dsa.NoneArray) == comp_array1)
assert algs.all(algs.bitwise_or(dsa.NoneArray, comp_array1) == comp_array1)
assert algs.all(algs.bitwise_or(comp_array1, comp_array3) == dsa.VTKCompositeDataArray([algs.bitwise_or(int_array1, int_array2), int_array2]))

ssource = vtkSphereSource()
ssource.Update()

output = ssource.GetOutput()

fd = vtkFloatArray()
fd.SetNumberOfTuples(11)
fd.FillComponent(0, 5)
fd.SetName("field array")

output.GetFieldData().AddArray(fd)

g2 = vtkMultiBlockDataGroupFilter()
g2.AddInputData(output)
g2.AddInputData(output)

g2.Update()

sphere = dsa.CompositeDataSet(g2.GetOutput())

vn = algs.vertex_normal(sphere)
compare(algs.mag(vn) - 1, 1E-6)

sn = algs.surface_normal(sphere)
compare(algs.mag(sn) - 1, 1E-6)

dot = algs.dot(vn, vn)
assert dot.DataSet is sphere
compare(dot - 1, 1E-6)
assert algs.all(algs.cross(vn, vn) == [0, 0, 0])

fd = sphere.FieldData['field array']
assert algs.all(fd == 5)
assert algs.shape(fd) == (22,)

assert vn.DataSet is sphere

# --------------------------------------

na = dsa.NoneArray

# Test operators
assert (1 + na - 1 - randomVec) is na

# Test slicing and indexing
assert na[:, 0] is na
assert (na > 0) is na

# Test ufunc
assert algs.cos(na) is na

# Various numerical ops implemented in VTK
assert algs.gradient(na) is na
assert algs.cross(na, na) is na
assert algs.cross(v.Arrays[0], na) is na
assert algs.cross(na, v.Arrays[0]) is na

assert algs.make_vector(na, g[:,0], elev) is na

pd = vtkPolyData()
pdw = dsa.WrapDataObject(pd)
pdw.PointData.append(na, 'foo')
assert pdw.PointData.GetNumberOfArrays() == 0

# --------------------------------------

na2 = dsa.VTKCompositeDataArray([randomVec.Arrays[0], na])

# Test operators
assert (1 + na2 - 1 - randomVec).Arrays[1] is na

# Test slicing and indexing
assert na2[:, 0].Arrays[1] is na

# Test ufunc
assert algs.cos(na2).Arrays[1] is na

# Various numerical ops implemented in VTK
assert algs.gradient(na2).Arrays[1] is na
assert algs.cross(na2, na2).Arrays[1] is na
assert algs.cross(v, na2).Arrays[1] is na
assert algs.cross(na2, v).Arrays[1] is na

assert algs.make_vector(na2[:, 0], elev, elev).Arrays[1] is na
assert algs.make_vector(elev, elev, na2[:, 0]).Arrays[1] is na
assert algs.make_vector(elev, na2[:, 0], elev).Arrays[1] is na

mb = vtkMultiBlockDataSet()
mb.SetBlock(0, pd)
pd2 = vtkPolyData()
mb.SetBlock(1, pd2)
globalArray = vtkIntArray()
globalArray.SetName("global")
globalArray.SetNumberOfTuples(2)
globalArray.SetValue(0, 1)
globalArray.SetValue(1, 2)
mb.GetFieldData().AddArray(globalArray)
mbw = dsa.WrapDataObject(mb)

mbw.PointData.append(dsa.NoneArray, 'foo')
assert mbw.GetBlock(0).GetPointData().GetNumberOfArrays() == 0
assert mbw.GetBlock(1).GetPointData().GetNumberOfArrays() == 0

mbw.PointData.append(na2, 'foo')
assert mbw.GetBlock(0).GetPointData().GetNumberOfArrays() == 1
assert mbw.GetBlock(1).GetPointData().GetNumberOfArrays() == 0
assert mbw.GetBlock(0).GetPointData().GetArray(0).GetName() == 'foo'

mbw.PointData.append(algs.max(na2), "maxfoo")
assert mbw.GetBlock(0).GetPointData().GetNumberOfArrays() == 2
assert mbw.GetBlock(1).GetPointData().GetNumberOfArrays() == 1
assert mbw.GetBlock(0).GetPointData().GetArray(1).GetName() == 'maxfoo'

assert len(mbw.GlobalData.keys()) == 1
assert mbw.GlobalData['global'][0] == 1
assert mbw.GlobalData['global'][1] == 2

# --------------------------------------

mb = vtkMultiBlockDataSet()
mb.SetBlock(0, vtkImageData())
mb.SetBlock(1, vtkImageData())
assert dsa.WrapDataObject(mb).Points is na

mb = vtkMultiBlockDataSet()
mb.SetBlock(0, vtkStructuredGrid())
mb.SetBlock(1, vtkImageData())
assert dsa.WrapDataObject(mb).Points is na

mb = vtkMultiBlockDataSet()
sg = vtkStructuredGrid()
sg.SetPoints(vtkPoints())
mb.SetBlock(0, sg)
mb.SetBlock(1, vtkImageData())
assert dsa.WrapDataObject(mb).Points.Arrays[0] is not na
assert dsa.WrapDataObject(mb).Points.Arrays[1] is na

# --------------------------------------
# try appending scalars
ssource = vtkSphereSource()
ssource.Update()
output = ssource.GetOutput()
pdw = dsa.WrapDataObject(output)
original_arrays = pdw.PointData.GetNumberOfArrays()
pdw.PointData.append(12, "twelve")
pdw.PointData.append(12.12, "twelve-point-twelve")
assert pdw.PointData.GetNumberOfArrays() == (2 + original_arrays)

# create a table
table = dsa.WrapDataObject(vtkTable())
table.RowData.append(numpy.ones(5), "ones")
table.RowData.append(2*numpy.ones(5), "twos")
assert table.GetNumberOfRows() == 5
assert table.GetNumberOfColumns() == 2

# --------------------------------------
# test matmul

a = numpy.ones((10, 3, 3))
a[:,:,1] = 5
x = numpy.ones((10, 3))
x[:,0] = 2

# matrix-vector product
numpy_b = numpy.matmul(a[0], x[0])
b = algs.matmul(a, x)
assert numpy.array_equal(b[0], numpy_b)

# vector-matrix product
numpy_b = numpy.matmul(x[0], a[0])
b = algs.matmul(x, a)
assert numpy.array_equal(b[0], numpy_b)

# vector-vector product
numpy_b = numpy.matmul(a[0], a[0])
b = algs.matmul(a, a)
assert numpy.array_equal(b[0], numpy_b)

numpy_b = numpy.matmul(x[0], x[0])
b = algs.matmul(x, x)
assert numpy.array_equal(b[0], numpy_b)

# matrix-matrix product
b = a + 5
numpy_c = numpy.matmul(a[0], b[0])
c = algs.matmul(a, b)
assert numpy.array_equal(c[0], numpy_c)

# test matmul for AOS and SOA arrays

aSOA = vtkSOADataArrayTemplate['float64']()
aAOS = vtkDoubleArray()
aSOA.SetNumberOfComponents(9)
aSOA.SetNumberOfTuples(2)
aAOS.SetNumberOfComponents(9)
aAOS.SetNumberOfTuples(2)

for t in range(2):
    for i in range(9):
        aSOA.SetComponent(t, i, i)
        aAOS.SetComponent(t, i, i)

aSOAVTK = dsa.vtkDataArrayToVTKArray(aSOA)
aAOSVTK = dsa.vtkDataArrayToVTKArray(aAOS)

xAOS = vtkDoubleArray()
xAOS.SetNumberOfComponents(3)
xAOS.SetNumberOfTuples(2)
xAOS.Fill(1)
xAOSVTK = dsa.vtkDataArrayToVTKArray(xAOS)

b1 = algs.matmul(aSOAVTK, xAOSVTK)
b2 = algs.matmul(aAOSVTK, xAOSVTK)

assert numpy.array_equal(b1, b2)
