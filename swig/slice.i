/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010-2014 Joel Andersson, Joris Gillis, Moritz Diehl,
 *                            K.U. Leuven. All rights reserved.
 *    Copyright (C) 2011-2014 Greg Horn
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifndef CASADI_SLICE_I
#define CASADI_SLICE_I

%include "printable_object.i"

%include <casadi/core/matrix/slice.hpp>

namespace casadi{

#ifndef SWIGXML

%fragment("to"{Slice}, "header") {
  int to_Slice(GUESTOBJECT *p, casadi::Slice *m) {
    if (is_a(p, $descriptor(casadi::Slice *))) {
      casadi::Slice *mp;
      if (SWIG_ConvertPtr(p, (void **) &mp, $descriptor(casadi::Slice *), 0) == -1)
        return false;
      if(m) *m = *mp;
      return true;
    }
#ifdef SWIGPYTHON
    if (PyInt_Check(p)) {
      if (m) {
        m->start_ = PyInt_AsLong(p);
        m->stop_ = m->start_+1;
        if (m->stop_==0) m->stop_ = std::numeric_limits<int>::max();
      }
      return true;
    } else if (PySlice_Check(p)) {
      PySliceObject *r = (PySliceObject*)(p);
      if (m) {
        m->start_ = (r->start == Py_None || PyInt_AsLong(r->start) < std::numeric_limits<int>::min()) 
          ? std::numeric_limits<int>::min() : PyInt_AsLong(r->start);
        m->stop_  = (r->stop ==Py_None || PyInt_AsLong(r->stop)> std::numeric_limits<int>::max())
          ? std::numeric_limits<int>::max() : PyInt_AsLong(r->stop);
        if(r->step !=Py_None) m->step_  = PyInt_AsLong(r->step);
      }
      return true;
    } else {
      return false;
    }
#else
    return false;
#endif
  }
}
%casadi_typemaps_constref(Slice, PRECEDENCE_SLICE, casadi::Slice)

%fragment("to"{IndexList}, "header", fragment="to"{Slice}) {
  int to_IndexList(GUESTOBJECT *p, casadi::IndexList *m) {
    if (is_a(p, $descriptor(casadi::IndexList *))) {
      casadi::IndexList *mp;
      if (SWIG_ConvertPtr(p, (void **) &mp, $descriptor(casadi::IndexList *), 0) == -1) return false;
      if (m) *m = *mp;
      return true;
    }
    if (meta< int >::toCpp(p, 0, *meta< int >::name)) {
      if (m) m->type = casadi::IndexList::INT;
      meta< int >::toCpp(p, m ? &m->i : 0, *meta< int >::name);
    } else if (meta< std::vector<int> >::toCpp(p, 0, *meta< std::vector<int> >::name)) {
      if (m) m->type = casadi::IndexList::IVECTOR;
      return meta< std::vector<int> >::toCpp(p, m ? &m->iv : 0, *meta< std::vector<int> >::name);
    } else if (to_Slice(p, 0)) {
      if (m) m->type = casadi::IndexList::SLICE;
      return to_Slice(p, m ? &m->slice : 0);
    } else {
      return false;
    }
    return true;
  }
}
%casadi_typemaps_constref(IndexList, PRECEDENCE_IndexVector, casadi::IndexList)

#endif // SWIGXML

} // namespace casadi

#endif // CASADI_SLICE_I
