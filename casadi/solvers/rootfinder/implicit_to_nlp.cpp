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


#include "implicit_to_nlp.hpp"
#include "casadi/core/function/nlpsol.hpp"

using namespace std;
namespace casadi {

  extern "C"
  int CASADI_ROOTFINDER_NLPSOL_EXPORT
  casadi_register_rootfinder_nlpsol(Rootfinder::Plugin* plugin) {
    plugin->creator = ImplicitToNlp::creator;
    plugin->name = "nlpsol";
    plugin->doc = ImplicitToNlp::meta_doc.c_str();
    plugin->version = CASADI_VERSION;
    return 0;
  }

  extern "C"
  void CASADI_ROOTFINDER_NLPSOL_EXPORT casadi_load_rootfinder_nlpsol() {
    Rootfinder::registerPlugin(casadi_register_rootfinder_nlpsol);
  }

  ImplicitToNlp::ImplicitToNlp(const std::string& name, const Function& f)
    : Rootfinder(name, f) {
  }

  ImplicitToNlp::~ImplicitToNlp() {
    clear_memory();
  }

  Options ImplicitToNlp::options_
  = {{&Rootfinder::options_},
     {{"nlpsol",
       {OT_STRING,
        "Name of solver."}},
      {"nlpsol_options",
       {OT_DICT,
        "Options to be passed to solver."}}
     }
  };

  void ImplicitToNlp::init(const Dict& opts) {
    // Call the base class initializer
    Rootfinder::init(opts);

    // Default options
    string nlpsol_plugin;
    Dict nlpsol_options;

    // Read user options
    for (auto&& op : opts) {
      if (op.first=="nlpsol") {
        nlpsol_plugin = op.second.to_string();
      } else if (op.first=="nlpsol_options") {
        nlpsol_options = op.second;
      }
    }

    // Free variable in the NLP
    MX u = MX::sym("u", sparsity_in(iin_));

    // So that we can pass it on to createParent
    std::vector<MX> inputs;
    for (int i=0; i<n_in(); ++i) {
      if (i!=iin_) {
        stringstream ss;
        ss << "p" << i;
        inputs.push_back(MX::sym(ss.str(), sparsity_in(i)));
      }
    }
    MX p = veccat(inputs);

    // Dummy NLP objective
    MX nlp_f = 0;

    // NLP constraints
    std::vector< MX > args_call(n_in());
    args_call[iin_] = u;
    for (int i=0, i2=0; i<n_in(); ++i)
      if (i!=iin_) args_call[i] = inputs[i2++];
    MX nlp_g = oracle_(args_call).at(iout_);

    // We're going to use two-argument objective and constraints to allow the use of parameters
    MXDict nlp = {{"x", u}, {"p", p}, {"f", nlp_f}, {"g", nlp_g}};

    // Create an Nlpsol instance
    casadi_assert_message(!nlpsol_plugin.empty(), "'nlpsol' option has not been set");
    solver_ = nlpsol("nlpsol", nlpsol_plugin, nlp, nlpsol_options);
    alloc(solver_);

    // Allocate storage for variable bounds
    alloc_w(n_, true); // lbx
    alloc_w(n_, true); // ubx

    // Allocate storage for NLP solver parameters
    alloc_w(oracle_.nnz_in() - nnz_in(iin_), true);

    // Allocate storage for NLP primal solution
    alloc_w(n_, true);
  }

  void ImplicitToNlp::set_work(void* mem, const double**& arg, double**& res,
                        int*& iw, double*& w) const {
      Rootfinder::set_work(mem, arg, res, iw, w);
      auto m = static_cast<ImplicitToNlpMemory*>(mem);
      m->lbx = w; w += n_;
      m->ubx = w; w += n_;
      m->p = w; w += oracle_.nnz_in() - nnz_in(iin_);
      m->x = w; w += n_;
   }

  void ImplicitToNlp::solve(void* mem) const {
    auto m = static_cast<ImplicitToNlpMemory*>(mem);

    // Buffers for calling the NLP solver
    fill_n(m->arg, static_cast<int>(NLPSOL_NUM_IN), nullptr);
    fill_n(m->res, static_cast<int>(NLPSOL_NUM_OUT), nullptr);

    // Initial guess
    m->arg[NLPSOL_X] = m->iarg[iin_];

    // Nonlinear bounds
    m->arg[NLPSOL_LBG] = 0;
    m->arg[NLPSOL_UBG] = 0;

    // Variable bounds
    fill_n(m->lbx, n_, -std::numeric_limits<double>::infinity());
    m->arg[NLPSOL_LBX] = m->lbx;
    fill_n(m->ubx, n_,  std::numeric_limits<double>::infinity());
    m->arg[NLPSOL_UBX] = m->ubx;
    for (int k=0; k<u_c_.size(); ++k) {
      if (u_c_[k] > 0) m->lbx[k] = 0;
      if (u_c_[k] < 0) m->ubx[k] = 0;
    }

    // NLP parameters
    m->arg[NLPSOL_P] = m->p;
    double* pi = m->p;
    for (int i=0; i<n_in(); ++i) {
      if (i!=iin_) {
        int n = oracle_.nnz_in(i);
        casadi_copy(m->iarg[i], n, pi);
        pi += n;
      }
    }

    // Primal solution
    m->res[NLPSOL_X] = m->x;

    // Solve the NLP
    solver_(m->arg, m->res, m->iw, m->w, 0);
    m->solver_stats = solver_.stats();

    // Get the implicit variable
    casadi_copy(m->x, n_, m->ires[iout_]);

    // Check if any auxilary outputs to evaluate
    bool has_aux = false;
    for (int i=0; i<n_out(); ++i) {
      if (i!=iout_ && m->ires[i]) {
        has_aux = true;
        break;
      }
    }

    // Evaluate auxilary outputs, if necessary
    if (has_aux) {
      copy_n(m->iarg, n_in(), m->arg);
      m->arg[iin_] = m->x;
      copy_n(m->ires, n_out(), m->res);
      m->res[iout_] = 0;
      oracle_(m->arg, m->res, m->iw, m->w, 0);
    }
  }

} // namespace casadi
