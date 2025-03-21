// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkPythonUtil.h"
#include "vtkABINamespace.h"
#include "vtkPythonOverload.h"

#include "PyVTKMethodDescriptor.h"

#include "vtkSystemIncludes.h"

#include "vtkObject.h"
#include "vtkPythonCommand.h"
#include "vtkSmartPyObject.h"
#include "vtkVariant.h"
#include "vtkWeakPointer.h"
#include "vtkWindows.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// for uintptr_t
#ifdef _MSC_VER
#include <stddef.h>
#else
#include <cstdint>
#endif

VTK_ABI_NAMESPACE_BEGIN
//------------------------------------------------------------------------------
// A ghost object, can be used to recreate a deleted PyVTKObject
class PyVTKObjectGhost
{
public:
  PyVTKObjectGhost()
    : vtk_class(nullptr)
    , vtk_dict(nullptr)
  {
  }

  vtkWeakPointerBase vtk_ptr;
  PyTypeObject* vtk_class;
  PyObject* vtk_dict;
};

//------------------------------------------------------------------------------
// There are six maps associated with the Python wrappers

// Map VTK objects to python objects (this is also the cornerstone
// of the vtk/python garbage collection system, because it contains
// exactly one pointer reference for each VTK object known to python)
class vtkPythonObjectMap
  : public std::map<vtkObjectBase*, std::pair<PyObject*, std::atomic<int32_t>>>
{
public:
  ~vtkPythonObjectMap();

  void add(vtkObjectBase* key, PyObject* value);
  void remove(vtkObjectBase* key);
};

// Call Delete instead of relying on vtkSmartPointer, so that crashes
// caused by deletion are easier to follow in the debug stack trace
vtkPythonObjectMap::~vtkPythonObjectMap()
{
  iterator i;
  for (i = this->begin(); i != this->end(); ++i)
  {
    for (int j = 0; j < i->second.second; ++j)
    {
      i->first->Delete();
    }
  }
}

void vtkPythonObjectMap::add(vtkObjectBase* key, PyObject* value)
{
  key->Register(nullptr);
  iterator i = this->find(key);
  if (i == this->end())
  {
    (*this)[key] = std::make_pair(value, 1);
  }
  else
  {
    i->second.first = value;
    ++i->second.second;
  }
}

void vtkPythonObjectMap::remove(vtkObjectBase* key)
{
  iterator i = this->find(key);
  if (i != this->end())
  {
    // Save the object. The iterator will become invalid if the iterator is
    // erased.
    vtkObjectBase* obj = i->first;
    // Remove it from the map if necessary.
    if (!--i->second.second)
    {
      this->erase(i);
    }
    // Remove a reference to the object. This must be done *after* removing it
    // from the map (if needed) because if there's a callback which reacts when
    // the reference is dropped, it might call RemoveObjectFromMap as well. If
    // it still exists in the map at that point, this becomes an infinite loop.
    obj->Delete();
  }
}

// Keep weak pointers to VTK objects that python no longer has
// references to.  Python keeps the python 'dict' for VTK objects
// even when they pass leave the python realm, so that if those
// VTK objects come back, their 'dict' can be restored to them.
// Periodically the weak pointers are checked and the dicts of
// VTK objects that have been deleted are tossed away.
class vtkPythonGhostMap : public std::map<vtkObjectBase*, PyVTKObjectGhost>
{
};

// Keep track of all the VTK classes that python knows about.
class vtkPythonClassMap : public std::map<std::string, PyVTKClass>
{
};

// Map the Pythonic class names to the ones given by GetClassName().
// These differ only for templated classes derived from vtkObjectBase,
// where GetClassName() returns typeid(T).name() which in general is
// not a valid Python name.
class vtkPythonClassNameMap : public std::map<std::string, std::string>
{
};

// Like the ClassMap, for types not derived from vtkObjectBase.
class vtkPythonSpecialTypeMap : public std::map<std::string, PyVTKSpecialType>
{
};

// Keep track of all the C++ namespaces that have been wrapped.
class vtkPythonNamespaceMap : public std::map<std::string, PyObject*>
{
};

// Keep track of all the C++ enums that have been wrapped.
class vtkPythonEnumMap : public std::map<std::string, PyTypeObject*>
{
};

// Keep track of all the VTK-Python extension modules
class vtkPythonModuleList : public std::vector<std::string>
{
};

// Keep track of all vtkPythonCommand instances.
class vtkPythonCommandList : public std::vector<vtkWeakPointer<vtkPythonCommand>>
{
public:
  ~vtkPythonCommandList()
  {
    iterator iter;
    for (iter = this->begin(); iter != this->end(); ++iter)
    {
      if (iter->GetPointer())
      {
        iter->GetPointer()->obj = nullptr;
        iter->GetPointer()->ThreadState = nullptr;
      }
    }
  }
  void findAndErase(vtkPythonCommand* ptr)
  {
    this->erase(std::remove(this->begin(), this->end(), ptr), this->end());
  }
};

//------------------------------------------------------------------------------
// The singleton for vtkPythonUtil

static vtkPythonUtil* vtkPythonMap = nullptr;

// destructs the singleton when python exits
void vtkPythonUtilDelete()
{
  delete vtkPythonMap;
  vtkPythonMap = nullptr;
}

// constructs the singleton
void vtkPythonUtilCreateIfNeeded()
{
  if (vtkPythonMap == nullptr)
  {
    vtkPythonMap = new vtkPythonUtil();
    Py_AtExit(vtkPythonUtilDelete);
  }
}

//------------------------------------------------------------------------------
vtkPythonUtil::vtkPythonUtil()
{
  this->ObjectMap = new vtkPythonObjectMap;
  this->GhostMap = new vtkPythonGhostMap;
  this->ClassMap = new vtkPythonClassMap;
  this->ClassNameMap = new vtkPythonClassNameMap;
  this->SpecialTypeMap = new vtkPythonSpecialTypeMap;
  this->NamespaceMap = new vtkPythonNamespaceMap;
  this->EnumMap = new vtkPythonEnumMap;
  this->ModuleList = new vtkPythonModuleList;
  this->PythonCommandList = new vtkPythonCommandList;
}

//------------------------------------------------------------------------------
vtkPythonUtil::~vtkPythonUtil()
{
  delete this->ObjectMap;
  delete this->GhostMap;
  delete this->ClassMap;
  delete this->ClassNameMap;
  delete this->SpecialTypeMap;
  delete this->NamespaceMap;
  delete this->EnumMap;
  delete this->ModuleList;
  delete this->PythonCommandList;
}

//------------------------------------------------------------------------------
void vtkPythonUtil::Initialize()
{
  // create the singleton
  vtkPythonUtilCreateIfNeeded();
  // finalize our custom MethodDescriptor type
  PyType_Ready(&PyVTKMethodDescriptor_Type);
}

//------------------------------------------------------------------------------
bool vtkPythonUtil::IsInitialized()
{
  return (vtkPythonMap != nullptr);
}

//------------------------------------------------------------------------------
void vtkPythonUtil::RegisterPythonCommand(vtkPythonCommand* cmd)
{
  if (cmd)
  {
    vtkPythonMap->PythonCommandList->push_back(cmd);
  }
}

//------------------------------------------------------------------------------
void vtkPythonUtil::UnRegisterPythonCommand(vtkPythonCommand* cmd)
{
  if (cmd && vtkPythonMap)
  {
    vtkPythonMap->PythonCommandList->findAndErase(cmd);
  }
}

//------------------------------------------------------------------------------
PyTypeObject* vtkPythonUtil::AddSpecialTypeToMap(
  PyTypeObject* pytype, PyMethodDef* methods, PyMethodDef* constructors, vtkcopyfunc copyfunc)
{
  const char* classname = vtkPythonUtil::StripModuleFromType(pytype);

  // lets make sure it isn't already there
  vtkPythonSpecialTypeMap::iterator i = vtkPythonMap->SpecialTypeMap->find(classname);
  if (i == vtkPythonMap->SpecialTypeMap->end())
  {
    i = vtkPythonMap->SpecialTypeMap->insert(i,
      vtkPythonSpecialTypeMap::value_type(
        classname, PyVTKSpecialType(pytype, methods, constructors, copyfunc)));
  }

  return i->second.py_type;
}

//------------------------------------------------------------------------------
PyVTKSpecialType* vtkPythonUtil::FindSpecialType(const char* classname)
{
  if (vtkPythonMap)
  {
    vtkPythonSpecialTypeMap::iterator it = vtkPythonMap->SpecialTypeMap->find(classname);

    if (it != vtkPythonMap->SpecialTypeMap->end())
    {
      return &it->second;
    }
  }

  return nullptr;
}

//------------------------------------------------------------------------------
void vtkPythonUtil::AddObjectToMap(PyObject* obj, vtkObjectBase* ptr)
{
#ifdef VTKPYTHONDEBUG
  vtkGenericWarningMacro("Adding an object to map ptr = " << ptr);
#endif

  ((PyVTKObject*)obj)->vtk_ptr = ptr;
  vtkPythonMap->ObjectMap->add(ptr, obj);

#ifdef VTKPYTHONDEBUG
  vtkGenericWarningMacro("Added object to map obj= " << obj << " " << ptr);
#endif
}

//------------------------------------------------------------------------------
void vtkPythonUtil::RemoveObjectFromMap(PyObject* obj)
{
  PyVTKObject* pobj = (PyVTKObject*)obj;

#ifdef VTKPYTHONDEBUG
  vtkGenericWarningMacro("Deleting an object from map obj = " << pobj << " " << pobj->vtk_ptr);
#endif

  if (vtkPythonMap && vtkPythonMap->ObjectMap->count(pobj->vtk_ptr))
  {
    vtkWeakPointerBase wptr;

    // check for customized class or dict
    if (pobj->vtk_class->py_type != Py_TYPE(pobj) || PyDict_Size(pobj->vtk_dict))
    {
      wptr = pobj->vtk_ptr;
    }

    vtkPythonMap->ObjectMap->remove(pobj->vtk_ptr);

    // if the VTK object still exists, then make a ghost
    if (wptr.GetPointer())
    {
      // List of attrs to be deleted
      std::vector<PyObject*> delList;

      // Erase ghosts of VTK objects that have been deleted
      vtkPythonGhostMap::iterator i = vtkPythonMap->GhostMap->begin();
      while (i != vtkPythonMap->GhostMap->end())
      {
        if (!i->second.vtk_ptr.GetPointer())
        {
          delList.push_back((PyObject*)i->second.vtk_class);
          delList.push_back(i->second.vtk_dict);
          vtkPythonMap->GhostMap->erase(i++);
        }
        else
        {
          ++i;
        }
      }

      // Add this new ghost to the map
      PyVTKObjectGhost& g = (*vtkPythonMap->GhostMap)[pobj->vtk_ptr];
      g.vtk_ptr = wptr;
      g.vtk_class = Py_TYPE(pobj);
      g.vtk_dict = pobj->vtk_dict;
      Py_INCREF(g.vtk_class);
      Py_INCREF(g.vtk_dict);

      // Delete attrs of erased objects.  Must be done at the end.
      for (size_t j = 0; j < delList.size(); j++)
      {
        Py_DECREF(delList[j]);
      }
    }
  }
}

//------------------------------------------------------------------------------
PyObject* vtkPythonUtil::FindObject(vtkObjectBase* ptr)
{
  PyObject* obj = nullptr;

  if (ptr && vtkPythonMap)
  {
    vtkPythonObjectMap::iterator i = vtkPythonMap->ObjectMap->find(ptr);
    if (i != vtkPythonMap->ObjectMap->end())
    {
      obj = i->second.first;
    }
    if (obj)
    {
      Py_INCREF(obj);
      return obj;
    }
  }
  else
  {
    Py_INCREF(Py_None);
    return Py_None;
  }

  // search weak list for object, resurrect if it is there
  vtkPythonGhostMap::iterator j = vtkPythonMap->GhostMap->find(ptr);
  if (j != vtkPythonMap->GhostMap->end())
  {
    if (j->second.vtk_ptr.GetPointer())
    {
      obj = PyVTKObject_FromPointer(j->second.vtk_class, j->second.vtk_dict, ptr);
    }
    Py_DECREF(j->second.vtk_class);
    Py_DECREF(j->second.vtk_dict);
    vtkPythonMap->GhostMap->erase(j);
  }

  return obj;
}

//------------------------------------------------------------------------------
PyObject* vtkPythonUtil::GetObjectFromPointer(vtkObjectBase* ptr)
{
  PyObject* obj = vtkPythonUtil::FindObject(ptr);

  if (obj == nullptr)
  {
    // create a new object
    PyVTKClass* vtkclass = nullptr;
    vtkPythonClassMap::iterator k = vtkPythonMap->ClassMap->find(ptr->GetClassName());
    if (k != vtkPythonMap->ClassMap->end())
    {
      vtkclass = &k->second;
    }

    // if the class was not in the map, then find the nearest base class
    // that is, and associate ptr->GetClassName() with that base class
    if (vtkclass == nullptr)
    {
      const char* classname = ptr->GetClassName();
      vtkclass = vtkPythonUtil::FindNearestBaseClass(ptr);
      vtkPythonClassMap::iterator i = vtkPythonMap->ClassMap->find(classname);
      if (i == vtkPythonMap->ClassMap->end())
      {
        vtkPythonMap->ClassMap->insert(i, vtkPythonClassMap::value_type(classname, *vtkclass));
      }
    }

    obj = PyVTKObject_FromPointer(vtkclass->py_type, nullptr, ptr);
  }

  return obj;
}

//------------------------------------------------------------------------------
const char* vtkPythonUtil::PythonicClassName(const char* classname)
{
  const char* cp = classname;

  /* check for non-alphanumeric chars */
  if (isalpha(*cp) || *cp == '_')
  {
    do
    {
      cp++;
    } while (isalnum(*cp) || *cp == '_');
  }

  if (*cp != '\0')
  {
    /* look up class and get its pythonic name */
    PyTypeObject* pytype = vtkPythonUtil::FindBaseTypeObject(classname);
    if (pytype)
    {
      classname = vtkPythonUtil::StripModuleFromType(pytype);
    }
  }

  return classname;
}

//------------------------------------------------------------------------------
const char* vtkPythonUtil::VTKClassName(const char* pyname)
{
  if (vtkPythonMap && pyname != nullptr)
  {
    vtkPythonClassNameMap::iterator it = vtkPythonMap->ClassNameMap->find(pyname);
    if (it != vtkPythonMap->ClassNameMap->end())
    {
      return it->second.c_str();
    }
  }

  return pyname;
}

//------------------------------------------------------------------------------
const char* vtkPythonUtil::StripModule(const char* tpname)
{
  const char* cp = tpname;
  const char* strippedname = tpname;
  while (*cp != '\0')
  {
    if (*cp++ == '.')
    {
      strippedname = cp;
    }
  }
  return strippedname;
}

//------------------------------------------------------------------------------
const char* vtkPythonUtil::StripModuleFromType(PyTypeObject* pytype)
{
  return vtkPythonUtil::StripModule(vtkPythonUtil::GetTypeName(pytype));
}

//------------------------------------------------------------------------------
const char* vtkPythonUtil::StripModuleFromObject(PyObject* ob)
{
  return vtkPythonUtil::StripModuleFromType(Py_TYPE(ob));
}

//------------------------------------------------------------------------------
const char* vtkPythonUtil::GetTypeName(PyTypeObject* pytype)
{
#ifdef PY_LIMITED_API
  vtkSmartPyObject tname = PyType_GetName(pytype);
  return PyUnicode_AsUTF8AndSize(tname, nullptr);
#else
  return pytype->tp_name;
#endif
}

//------------------------------------------------------------------------------
const char* vtkPythonUtil::GetTypeNameForObject(PyObject* ob)
{
  return vtkPythonUtil::GetTypeName(Py_TYPE(ob));
}

//------------------------------------------------------------------------------
PyTypeObject* vtkPythonUtil::AddClassToMap(
  PyTypeObject* pytype, PyMethodDef* methods, const char* classname, vtknewfunc constructor)
{
  // lets make sure it isn't already there
  vtkPythonClassMap::iterator i = vtkPythonMap->ClassMap->find(classname);
  if (i == vtkPythonMap->ClassMap->end())
  {
    i = vtkPythonMap->ClassMap->insert(i,
      vtkPythonClassMap::value_type(
        classname, PyVTKClass(pytype, methods, classname, constructor)));

    // if Python type name differs from VTK ClassName, store in ClassNameMap
    // (this only occurs for templated classes, due to their GetClassName()
    // implementation in their type macro in vtkSetGet.h)
    const char* pyname = vtkPythonUtil::StripModuleFromType(pytype);
    if (strcmp(pyname, classname) != 0)
    {
      vtkPythonMap->ClassNameMap->insert(
        vtkPythonMap->ClassNameMap->end(), vtkPythonClassNameMap::value_type(pyname, classname));
    }
  }

  return i->second.py_type;
}

//------------------------------------------------------------------------------
PyVTKClass* vtkPythonUtil::FindClass(const char* classname)
{
  if (vtkPythonMap)
  {
    vtkPythonClassMap::iterator it = vtkPythonMap->ClassMap->find(classname);
    if (it != vtkPythonMap->ClassMap->end())
    {
      return &it->second;
    }
  }

  return nullptr;
}

//------------------------------------------------------------------------------
// this is a helper function to find the nearest base class for an
// object whose class is not in the ClassDict
PyVTKClass* vtkPythonUtil::FindNearestBaseClass(vtkObjectBase* ptr)
{
  PyVTKClass* nearestbase = nullptr;
  int maxdepth = 0;
  int depth;

  for (vtkPythonClassMap::iterator classes = vtkPythonMap->ClassMap->begin();
       classes != vtkPythonMap->ClassMap->end(); ++classes)
  {
    PyVTKClass* pyclass = &classes->second;

    if (ptr->IsA(pyclass->vtk_name))
    {
      PyTypeObject* base =
#if PY_VERSION_HEX >= 0x030A0000
        (PyTypeObject*)PyType_GetSlot(pyclass->py_type, Py_tp_base)
#else
        pyclass->py_type->tp_base
#endif
        ;
      // count the hierarchy depth for this class
      for (depth = 0; base != nullptr; depth++)
      {
#if PY_VERSION_HEX >= 0x030A0000
        base = (PyTypeObject*)PyType_GetSlot(base, Py_tp_base);
#else
        base = base->tp_base;
#endif
      }
      // we want the class that is furthest from vtkObjectBase
      if (depth > maxdepth)
      {
        maxdepth = depth;
        nearestbase = pyclass;
      }
    }
  }

  return nearestbase;
}

//------------------------------------------------------------------------------
vtkObjectBase* vtkPythonUtil::GetPointerFromObject(PyObject* obj, const char* result_type)
{
  vtkObjectBase* ptr;

  // convert Py_None to nullptr every time
  if (obj == Py_None)
  {
    return nullptr;
  }

  // check to ensure it is a vtk object
  if (!PyVTKObject_Check(obj))
  {
    obj = PyObject_GetAttrString(obj, "__vtk__");
    if (obj)
    {
      PyObject* arglist = Py_BuildValue("()");
      PyObject* result = PyObject_Call(obj, arglist, nullptr);
      Py_DECREF(arglist);
      Py_DECREF(obj);
      if (result == nullptr)
      {
        return nullptr;
      }
      if (!PyVTKObject_Check(result))
      {
        PyErr_SetString(PyExc_TypeError, "__vtk__() doesn't return a VTK object");
        Py_DECREF(result);
        return nullptr;
      }
      else
      {
        ptr = ((PyVTKObject*)result)->vtk_ptr;
        Py_DECREF(result);
      }
    }
    else
    {
#ifdef VTKPYTHONDEBUG
      vtkGenericWarningMacro("Object " << obj << " is not a VTK object!!");
#endif
      PyErr_SetString(PyExc_TypeError, "method requires a VTK object");
      return nullptr;
    }
  }
  else
  {
    ptr = ((PyVTKObject*)obj)->vtk_ptr;
  }

#ifdef VTKPYTHONDEBUG
  vtkGenericWarningMacro("Checking into obj " << obj << " ptr = " << ptr);
#endif

  if (ptr->IsA(result_type))
  {
#ifdef VTKPYTHONDEBUG
    vtkGenericWarningMacro("Got obj= " << obj << " ptr= " << ptr << " " << result_type);
#endif
    return ptr;
  }
  else
  {
    char error_string[2048];
#ifdef VTKPYTHONDEBUG
    vtkGenericWarningMacro("vtk bad argument, type conversion failed.");
#endif
    snprintf(error_string, sizeof(error_string), "method requires a %.500s, a %.500s was provided.",
      vtkPythonUtil::PythonicClassName(result_type),
      vtkPythonUtil::PythonicClassName(ptr->GetClassName()));
    PyErr_SetString(PyExc_TypeError, error_string);
    return nullptr;
  }
}

//----------------
// union of long int and pointer
union vtkPythonUtilPointerUnion
{
  void* p;
  uintptr_t l;
};

//----------------
// union of long int and pointer
union vtkPythonUtilConstPointerUnion
{
  const void* p;
  uintptr_t l;
};

//------------------------------------------------------------------------------
PyObject* vtkPythonUtil::GetObjectFromObject(PyObject* arg, const char* type)
{
  union vtkPythonUtilPointerUnion u;
  PyObject* tmp = nullptr;

  if (PyUnicode_Check(arg))
  {
    tmp = PyUnicode_AsUTF8String(arg);
    arg = tmp;
  }

  if (PyBytes_Check(arg))
  {
    vtkObjectBase* ptr;
    char* ptrText = PyBytes_AsString(arg);

    char typeCheck[1024]; // typeCheck is currently not used
    unsigned long long l;
    int i = sscanf(ptrText, "_%llx_%s", &l, typeCheck);
    u.l = static_cast<uintptr_t>(l);

    if (i <= 0)
    {
      i = sscanf(ptrText, "Addr=0x%llx", &l);
      u.l = static_cast<uintptr_t>(l);
    }
    if (i <= 0)
    {
      i = sscanf(ptrText, "%p", &u.p);
    }
    if (i <= 0)
    {
      Py_XDECREF(tmp);
      PyErr_SetString(
        PyExc_ValueError, "could not extract hexadecimal address from argument string");
      return nullptr;
    }

    ptr = static_cast<vtkObjectBase*>(u.p);

    if (!ptr->IsA(type))
    {
      char error_string[2048];
      snprintf(error_string, sizeof(error_string),
        "method requires a %.500s address, a %.500s address was provided.", type,
        ptr->GetClassName());
      Py_XDECREF(tmp);
      PyErr_SetString(PyExc_TypeError, error_string);
      return nullptr;
    }

    Py_XDECREF(tmp);
    return vtkPythonUtil::GetObjectFromPointer(ptr);
  }

  Py_XDECREF(tmp);
  PyErr_SetString(PyExc_TypeError, "method requires a string argument");
  return nullptr;
}

//------------------------------------------------------------------------------
void* vtkPythonUtil::GetPointerFromSpecialObject(
  PyObject* obj, const char* result_type, PyObject** newobj)
{
  if (vtkPythonMap == nullptr)
  {
    PyErr_SetString(PyExc_TypeError, "method requires a vtkPythonMap");
    return nullptr;
  }

  const char* object_type = vtkPythonUtil::StripModuleFromObject(obj);

  // do a lookup on the desired type
  vtkPythonSpecialTypeMap::iterator it = vtkPythonMap->SpecialTypeMap->find(result_type);
  if (it != vtkPythonMap->SpecialTypeMap->end())
  {
    PyVTKSpecialType* info = &it->second;

    // first, check if object is the desired type
    if (PyObject_TypeCheck(obj, info->py_type))
    {
      return ((PyVTKSpecialObject*)obj)->vtk_ptr;
    }

    // try to construct the special object from the supplied object
    PyObject* sobj = nullptr;

    PyMethodDef* meth = vtkPythonOverload::FindConversionMethod(info->vtk_constructors, obj);

    // If a constructor signature exists for "obj", call it
    if (meth && meth->ml_meth)
    {
      PyObject* args = PyTuple_Pack(1, obj);
      PyObject* func = PyCFunction_New(meth, nullptr);
      if (func)
      {
        sobj = PyObject_Call(func, args, nullptr);
        Py_DECREF(func);
      }
      Py_DECREF(args);
    }

    if (sobj && newobj)
    {
      *newobj = sobj;
      return ((PyVTKSpecialObject*)sobj)->vtk_ptr;
    }
    else if (sobj)
    {
      char error_text[2048];
      Py_DECREF(sobj);
      snprintf(error_text, sizeof(error_text), "cannot pass %.500s as a non-const %.500s reference",
        object_type, result_type);
      PyErr_SetString(PyExc_TypeError, error_text);
      return nullptr;
    }

    // If a TypeError occurred, clear it and set our own error
    PyObject* ex = PyErr_Occurred();
    if (ex != nullptr)
    {
      if (PyErr_GivenExceptionMatches(ex, PyExc_TypeError))
      {
        PyErr_Clear();
      }
      else
      {
        return nullptr;
      }
    }
  }

#ifdef VTKPYTHONDEBUG
  vtkGenericWarningMacro("vtk bad argument, type conversion failed.");
#endif

  char error_string[2048];
  snprintf(error_string, sizeof(error_string), "method requires a %.500s, a %.500s was provided.",
    result_type, object_type);
  PyErr_SetString(PyExc_TypeError, error_string);

  return nullptr;
}

//------------------------------------------------------------------------------
void vtkPythonUtil::AddNamespaceToMap(PyObject* module)
{
  if (!PyVTKNamespace_Check(module))
  {
    return;
  }

  const char* name = PyVTKNamespace_GetName(module);
  // let's make sure it isn't already there
  vtkPythonNamespaceMap::iterator i = vtkPythonMap->NamespaceMap->find(name);
  if (i != vtkPythonMap->NamespaceMap->end())
  {
    return;
  }

  (*vtkPythonMap->NamespaceMap)[name] = module;
}

//------------------------------------------------------------------------------
// This method is called from PyVTKNamespace_Delete
void vtkPythonUtil::RemoveNamespaceFromMap(PyObject* obj)
{
  if (vtkPythonMap && PyVTKNamespace_Check(obj))
  {
    const char* name = PyVTKNamespace_GetName(obj);
    vtkPythonNamespaceMap::iterator it = vtkPythonMap->NamespaceMap->find(name);
    if (it != vtkPythonMap->NamespaceMap->end() && it->second == obj)
    {
      // The map has a pointer to the object, but does not hold a
      // reference, therefore there is no decref.
      vtkPythonMap->NamespaceMap->erase(it);
    }
  }
}

//------------------------------------------------------------------------------
PyObject* vtkPythonUtil::FindNamespace(const char* name)
{
  if (vtkPythonMap)
  {
    vtkPythonNamespaceMap::iterator it = vtkPythonMap->NamespaceMap->find(name);
    if (it != vtkPythonMap->NamespaceMap->end())
    {
      return it->second;
    }
  }

  return nullptr;
}

//------------------------------------------------------------------------------
void vtkPythonUtil::AddEnumToMap(PyTypeObject* enumtype, const char* name)
{
  // Only add to map if it isn't already there
  vtkPythonEnumMap::iterator i = vtkPythonMap->EnumMap->find(name);
  if (i == vtkPythonMap->EnumMap->end())
  {
    (*vtkPythonMap->EnumMap)[name] = enumtype;
  }
}

//------------------------------------------------------------------------------
PyTypeObject* vtkPythonUtil::FindEnum(const char* name)
{
  PyTypeObject* pytype = nullptr;

  if (vtkPythonMap)
  {
    vtkPythonEnumMap::iterator it = vtkPythonMap->EnumMap->find(name);
    if (it != vtkPythonMap->EnumMap->end())
    {
      pytype = it->second;
    }
  }

  return pytype;
}

//------------------------------------------------------------------------------
PyTypeObject* vtkPythonUtil::FindBaseTypeObject(const char* name)
{
  PyVTKClass* info = vtkPythonUtil::FindClass(name);
  if (info)
  {
    // in case of override, drill down to get the original (non-override) type,
    // that's what we need to use for the base class of other wrapped classes
    for (PyTypeObject* pytype = info->py_type; pytype != nullptr;
         pytype =
#if PY_VERSION_HEX >= 0x030A0000
           (PyTypeObject*)PyType_GetSlot(pytype, Py_tp_base)
#else
           pytype->tp_base
#endif
    )
    {
      if (strcmp(vtkPythonUtil::StripModuleFromType(pytype), name) == 0)
      {
        return pytype;
      }
    }
    return info->py_type;
  }

  return nullptr;
}

//------------------------------------------------------------------------------
PyTypeObject* vtkPythonUtil::FindClassTypeObject(const char* name)
{
  PyVTKClass* info = vtkPythonUtil::FindClass(name);
  if (info)
  {
    return info->py_type;
  }

  return nullptr;
}

//------------------------------------------------------------------------------
PyTypeObject* vtkPythonUtil::FindSpecialTypeObject(const char* name)
{
  PyVTKSpecialType* info = vtkPythonUtil::FindSpecialType(name);
  if (info)
  {
    return info->py_type;
  }

  return nullptr;
}

//------------------------------------------------------------------------------
bool vtkPythonUtil::ImportModule(const char* fullname, PyObject* globals)
{
  // strip all but the final part of the path
  const char* name = std::strrchr(fullname, '.');
  if (name == nullptr)
  {
    name = fullname;
  }
  else if (name[0] == '.')
  {
    name++;
  }

  // check whether the module is already loaded
  if (vtkPythonMap)
  {
    vtkPythonModuleList* ml = vtkPythonMap->ModuleList;
    if (std::find(ml->begin(), ml->end(), name) != ml->end())
    {
      return true;
    }
  }

  PyObject* m = nullptr;

  if (fullname == name || (fullname[0] == '.' && &fullname[1] == name))
  {
    // try relative import
    m = PyImport_ImportModuleLevel(name, globals, nullptr, nullptr, 1);
    if (!m)
    {
      PyErr_Clear();
    }
  }

  if (!m)
  {
    // try absolute import
    m = PyImport_ImportModule(fullname);
  }

  if (!m)
  {
    PyErr_Clear();
    return false;
  }

  Py_DECREF(m);
  return true;
}

//------------------------------------------------------------------------------
void vtkPythonUtil::AddModule(const char* name)
{
  vtkPythonMap->ModuleList->push_back(name);

  // Register module name into pending list for deferred side module loading
  PyObject* pModule = PyImport_ImportModule("vtkmodules");
  PyObject* pFunc = PyObject_GetAttrString(pModule, "on_vtk_module_init");
  PyObject* pArgs = PyTuple_New(1);
  PyTuple_SetItem(pArgs, 0, PyUnicode_FromString(name));
  PyObject* execVal = PyObject_CallObject(pFunc, pArgs);
  Py_DECREF(execVal);
  Py_DECREF(pArgs);
  Py_DECREF(pFunc);
  Py_DECREF(pModule);
}

//------------------------------------------------------------------------------
// mangle a void pointer into a SWIG-style string
char* vtkPythonUtil::ManglePointer(const void* ptr, const char* type)
{
  static char ptrText[128];
  int ndigits = 2 * (int)sizeof(void*);
  union vtkPythonUtilConstPointerUnion u;
  u.p = ptr;
  snprintf(ptrText, sizeof(ptrText), "_%*.*llx_%s", ndigits, ndigits,
    static_cast<unsigned long long>(u.l), type);

  return ptrText;
}

//------------------------------------------------------------------------------
// unmangle a void pointer from a SWIG-style string
void* vtkPythonUtil::UnmanglePointer(char* ptrText, int* len, const char* type)
{
  int i;
  union vtkPythonUtilPointerUnion u;
  char text[1024];
  char typeCheck[1024];
  typeCheck[0] = '\0';

  // Do some minimal checks that it might be a swig pointer.
  if (*len < 256 && *len > 4 && ptrText[0] == '_')
  {
    strncpy(text, ptrText, *len);
    text[*len] = '\0';
    i = *len;
    // Allow one null byte, in case trailing null is part of *len
    if (i > 0 && text[i - 1] == '\0')
    {
      i--;
    }
    // Verify that there are no other null bytes
    while (i > 0 && text[i - 1] != '\0')
    {
      i--;
    }

    // If no null bytes, then do a full check for a swig pointer
    if (i == 0)
    {
      unsigned long long l;
      i = sscanf(text, "_%llx_%s", &l, typeCheck);
      u.l = static_cast<uintptr_t>(l);

      if (strcmp(type, typeCheck) == 0)
      { // successfully unmangle
        *len = 0;
        return u.p;
      }
      else if (i == 2)
      { // mangled pointer of wrong type
        *len = -1;
        return nullptr;
      }
    }
  }

  // couldn't unmangle: return string as void pointer if it didn't look
  // like a SWIG mangled pointer
  return (void*)ptrText;
}

//------------------------------------------------------------------------------
Py_hash_t vtkPythonUtil::VariantHash(const vtkVariant* v)
{
  Py_hash_t h = -1;

  // This uses the same rules as the vtkVariant "==" operator.
  // All types except for vtkObject are converted to strings.
  // Quite inefficient, but it gets the job done.  Fortunately,
  // the python vtkVariant is immutable, so its hash can be cached.

  switch (v->GetType())
  {
    case VTK_OBJECT:
    {
      h = _Py_HashPointer(v->ToVTKObject());
      break;
    }

    default:
    {
      std::string s = v->ToString();
      PyObject* tmp = PyUnicode_FromString(s.c_str());
      h = PyObject_Hash(tmp);
      Py_DECREF(tmp);
      break;
    }
  }

  return h;
}

//------------------------------------------------------------------------------
void vtkPythonVoidFunc(void* arg)
{
  PyObject *arglist, *result;
  PyObject* func = (PyObject*)arg;

  // Sometimes it is possible for the function to be invoked after
  // Py_Finalize is called, this will cause nasty errors so we return if
  // the interpreter is not initialized.
  if (Py_IsInitialized() == 0)
  {
    return;
  }

#ifndef VTK_NO_PYTHON_THREADS
  vtkPythonScopeGilEnsurer gilEnsurer(true);
#endif

  arglist = Py_BuildValue("()");

  result = PyObject_Call(func, arglist, nullptr);
  Py_DECREF(arglist);

  if (result)
  {
    Py_XDECREF(result);
  }
  else
  {
    if (PyErr_ExceptionMatches(PyExc_KeyboardInterrupt))
    {
      cerr << "Caught a Ctrl-C within python, exiting program.\n";
      Py_Exit(1);
    }
    PyErr_Print();
  }
}

//------------------------------------------------------------------------------
void vtkPythonVoidFuncArgDelete(void* arg)
{
  PyObject* func = (PyObject*)arg;

  // Sometimes it is possible for the function to be invoked after
  // Py_Finalize is called, this will cause nasty errors so we return if
  // the interpreter is not initialized.
  if (Py_IsInitialized() == 0)
  {
    return;
  }

#ifndef VTK_NO_PYTHON_THREADS
  vtkPythonScopeGilEnsurer gilEnsurer(true);
#endif

  if (func)
  {
    Py_DECREF(func);
  }
}

//------------------------------------------------------------------------------
PyGetSetDef* vtkPythonUtil::FindGetSetDescriptor(PyTypeObject* pytype, PyObject* key)
{
  // Check if tp_dict is present
  if (pytype->tp_dict != nullptr && PyDict_Check(pytype->tp_dict))
  {
    // Check if the attribute is in the dictionary
    PyObject* attr = PyDict_GetItem(pytype->tp_dict, key);
    if (attr != nullptr)
    {
      PyDescrObject* descr = (PyDescrObject*)attr;
      if (pytype == descr->d_type || PyType_IsSubtype(pytype, descr->d_type))
      {
        PyGetSetDescrObject* getsetDescr = (PyGetSetDescrObject*)descr;
        if (getsetDescr->d_getset != nullptr)
        {
          return getsetDescr->d_getset;
        }
      }
    }
  }
  // Recursively check in base types
  if (pytype->tp_base != nullptr)
  {
    return vtkPythonUtil::FindGetSetDescriptor(pytype->tp_base, key);
  }
  // No matching getset descriptor found
  return nullptr;
}
VTK_ABI_NAMESPACE_END
