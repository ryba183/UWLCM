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

struct calc_exner
{
  setup::real_t operator()(setup::real_t p) const
  {return libcloudphxx::common::theta_std::exner<setup::real_t>(p * si::pascals);}
  BZ_DECLARE_FUNCTOR(calc_exner)
};

// common forcing functions
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
    //this->vert_grad_fwd(F, alpha, params.dz);
    alpha(ijk) = F(ijk);

    // change of rv[1/s] = latent heating[W/m^3] / lat_heat_of_evap[J/kg] / density[kg/m^3]
    if(params.ForceParameters.surf_latent_flux_in_watts_per_square_meter)
      alpha(ijk).reindex(this->zero) /= (libcloudphxx::common::const_cp::l_tri<real_t>() * si::kilograms / si::joules) * (*params.rhod)(this->vert_idx);
//    else
  //    alpha(ijk).reindex(this->zero) *= -1; // negative gradient means inflow

    // large-scale vertical wind
    subsidence(ix::rv); // TODO: in case 1, rv here should be in step n+1, calc it explicitly as rv + 0.5 * dt * rhs(rv); 
                        // could also be calculated implicitly, but we would need implicit rv^n+1 in other cells
    
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
    // surface flux
    /*
    if(params.ForceParameters.surf_sensible_flux_in_watts_per_square_meter)
    {
      surf_sens();
      nancheck(F(ijk), "sensible surf forcing");
      // beta as tmp storage
      beta(ijk) = F(ijk);
    }
    else
      beta(ijk) = 0.;
    */

    // radiation
    radiation(rv);
    nancheck(beta(ijk), "radiation");
    // add fluxes from radiation and surface
//    F(ijk) += beta(ijk);
  //    nancheck(F(ijk), "sensible surf forcing + radiation");
    // sum of th flux, F(j) is upward flux through the bottom of the j-th cell
    this->vert_grad_fwd(F, alpha, params.dz);
    alpha(ijk) *= -1; // negative gradient means inflow
    nancheck(alpha(ijk), "sum of th flux");

    if(params.ForceParameters.surf_sensible_flux_in_watts_per_square_meter)
    {
      surf_sens();
      nancheck(F(ijk), "sensible surf forcing");
      // beta as tmp storage
      alpha(ijk) += F(ijk);
    }
  
    // change of theta[K/s] = heating[W/m^3] / exner / c_p[J/K/kg] / this->rhod[kg/m^3]
    alpha(ijk).reindex(this->zero) /=  calc_exner()((*params.p_e)(this->vert_idx)) * 
      calc_c_p()(rv(ijk).reindex(this->zero)) * 
      (*params.rhod)(this->vert_idx);

      nancheck2(alpha(ijk), this->state(ix::th)(ijk), "change of theta");

    // surf flux if it is specified as mean(theta*w)
    if(!params.ForceParameters.surf_sensible_flux_in_watts_per_square_meter)
    {
      surf_sens();
      nancheck(F(ijk), "sensible surf forcing");
//      this->vert_grad_fwd(F, beta, params.dz);
  //    nancheck(beta(ijk), "grad of sensible surf forcing");
  //    alpha(ijk) += - beta(ijk); // negative gradient of upward flux means inflow
      alpha(ijk) += F(ijk);
    }
  
    // large-scale vertical wind
    subsidence(ix::th); // TODO: in case 1 th here should be in step n+1, calc it explicitly as th + 0.5 * dt * rhs(th);
                        //       could also be calculated implicitly, but we would need implicit th^n+1 in other cells
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
void slvr_common<ct_params_t>::w_src(typename parent_t::arr_t &th, typename parent_t::arr_t &rv, const int at)
{
  const auto &ijk = this->ijk;
  // buoyancy
  buoyancy(th, rv);
  alpha(ijk) = 0.5 * F(ijk); // halved, because it is applied trapezoidaly
  if(at == 0) // subsidence added explicitly, so updated only at n
  {
    // large-scale vertical wind
    subsidence(ix::w); // TODO: in case 1, w here should be in step n+1, calc it explicitly as w + 0.5 * dt * rhs(w); 
                       //       could also be calculated implicitly, but we would need implicit w^n+1 in other cells;
                       //       also include absorber in w^n+1 estimate...

    alpha(ijk) += F(ijk);
  }
}
