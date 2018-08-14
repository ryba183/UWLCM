//TODO: move calc_forces and forcings to case class?
#pragma once
#include "forcings.hpp"

// helper functors
struct calc_c_p
{
  setup::real_t operator()(setup::real_t rv) const
  {return libcloudphxx::common::moist_air::c_p<setup::real_t>(rv) * si::kilograms * si::kelvins / si::joules;}
  BZ_DECLARE_FUNCTOR(calc_c_p)
};

struct calc_T
{
  setup::real_t operator()(setup::real_t th, setup::real_t rhod) const
  {return libcloudphxx::common::theta_dry::T<setup::real_t>(th * si::kelvins, rhod * si::kilograms / si::metres  / si::metres / si::metres) / si::kelvins;}
  BZ_DECLARE_FUNCTOR2(calc_T)
};

// forcing functions
// TODO: make functions return blitz arrays to avoid unnecessary copies
template <class ct_params_t>
void slvr_common<ct_params_t>::rv_src()
{
  const auto &ijk = this->ijk;
  if(params.rv_src)
  {
    // surface flux
    surf_latent();
    // sum of rv flux
    alpha(ijk) = F(ijk);

    // change of rv[1/s] = latent heating[W/m^3] / lat_heat_of_evap[J/kg] / density[kg/m^3]
    if(params.ForceParameters.surf_latent_flux_in_watts_per_square_meter)
      alpha(ijk).reindex(this->zero) /= (libcloudphxx::common::const_cp::l_tri<real_t>() * si::kilograms / si::joules) * (*params.rhod)(this->vert_idx);

    // large-scale vertical wind
    subsidence(ix::rv);
    
    alpha(ijk) += F(ijk);
  }
  else
    alpha(ijk) = 0.;

  beta(ijk) = 0.;
  // nudging, todo: use some other coeff than vab_coeff
//  alpha(ijk).reindex(this->zero) += (*this->mem->vab_coeff)(ijk).reindex(this->zero) * (*params.rv_e)(this->vert_idx); // TODO: its a constant, cache it
//  beta(ijk) = - (*this->mem->vab_coeff)(ijk);
}

template <class ct_params_t>
void slvr_common<ct_params_t>::th_src(typename parent_t::arr_t &rv)
{
  const auto &ijk = this->ijk;
  if(params.th_src)
  {
    // -- heating --

    // radiation
    radiation(rv);
    nancheck(beta(ijk), "radiation");
    // sum of th flux, F(j) is upward flux through the bottom of the j-th cell
    this->vert_grad_fwd(F, alpha, params.dz);
    alpha(ijk) *= -1; // negative gradient means inflow
    nancheck(alpha(ijk), "sum of th flux");

    // surface flux
    if(params.ForceParameters.surf_sensible_flux_in_watts_per_square_meter)
    {
      surf_sens();
      nancheck(F(ijk), "sensible surf forcing");
      alpha(ijk) += F(ijk);
    }
  
    // change of theta[K/s] = heating[W/m^3] * theta[K] / T[K] / c_p[J/K/kg] / this->rhod[kg/m^3]
    alpha(ijk).reindex(this->zero) *= this->state(ix::th)(ijk).reindex(this->zero) / 
      calc_c_p()(rv(ijk).reindex(this->zero)) / 
      calc_T()(this->state(ix::th)(ijk).reindex(this->zero), (*params.rhod)(this->vert_idx)) /
      (*params.rhod)(this->vert_idx);

    nancheck2(alpha(ijk), this->state(ix::th)(ijk), "change of theta");

    // surf flux if it is specified as mean(theta*w)
    if(!params.ForceParameters.surf_sensible_flux_in_watts_per_square_meter)
    {
      surf_sens();
      nancheck(F(ijk), "sensible surf forcing");
      alpha(ijk) += F(ijk);
    }
  
    // large-scale vertical wind
    subsidence(ix::th); 
    nancheck(F(ijk), "subsidence");
    alpha(ijk) += F(ijk);
    nancheck(alpha(ijk), "alpha in th_src");
  }
  else
    alpha(ijk) = 0.;

  beta(ijk) = 0.;
  // nudging, todo: use some other coeff than vab_coeff
  //alpha(ijk).reindex(this->zero) += (*this->mem->vab_coeff)(ijk).reindex(this->zero) * (*params.th_e)(this->vert_idx);
  //beta(ijk) = - (*this->mem->vab_coeff)(ijk);
}

template <class ct_params_t>
void slvr_common<ct_params_t>::w_src(typename parent_t::arr_t &th, typename parent_t::arr_t &rv)
{
  const auto &ijk = this->ijk;
  // buoyancy
  buoyancy(th, rv);
  alpha(ijk) = F(ijk);
  // large-scale vertical wind
  subsidence(ix::w); 

  alpha(ijk) += F(ijk);
}


