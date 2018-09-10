#pragma once
#include "slvr_common.hpp"

template <class ct_params_t>
class slvr_sgs : public slvr_common<ct_params_t>
{
  using parent_t = slvr_common<ct_params_t>;

  public:
  using real_t = typename ct_params_t::real_t;
  using ix = typename ct_params_t::ix;

  protected:

  real_t prandtl_num;

  typename parent_t::arr_t &rcdsn_num, &tdef_sq;
  arrvec_t<typename parent_t::arr_t> &tmp_grad, &hlpr;

  template <int nd = ct_params_t::n_dims> 
  void calc_rcdsn_num(typename std::enable_if<nd == 2>::type* = 0)
  {
    auto g = (libcloudphxx::common::earth::g<setup::real_t>() / si::metres_per_second_squared);
    rcdsn_num(this->ijk).reindex(this->zero) = g * 0.5 * (
                                       tmp_grad[ct_params_t::n_dims - 1](this->i, this->j - h).reindex(this->zero)
                                     + tmp_grad[ct_params_t::n_dims - 1](this->i, this->j + h).reindex(this->zero)
                                     ) / ((*this->params.th_ref)(this->vert_idx) * tdef_sq(this->ijk).reindex(this->zero));
  }

  template <int nd = ct_params_t::n_dims> 
  void calc_rcdsn_num(typename std::enable_if<nd == 3>::type* = 0)
  {
    auto g = (libcloudphxx::common::earth::g<setup::real_t>() / si::metres_per_second_squared);
    //if (this->rank == 0) std::cout << "in rcdsn: " << this->params.th_ref << std::endl;
    //auto test = max((*this->params.th_ref)(this->vert_idx));
    rcdsn_num(this->ijk).reindex(this->zero) = g * 0.5 * (
                                       tmp_grad[ct_params_t::n_dims - 1](this->i, this->j, this->k - h).reindex(this->zero)
                                     + tmp_grad[ct_params_t::n_dims - 1](this->i, this->j, this->k + h).reindex(this->zero)
                                     ) / ((*this->params.th_ref)(this->vert_idx) * tdef_sq(this->ijk).reindex(this->zero));
  }


  void multiply_sgs_visc()
  {
    //static_assert(static_cast<stress_diff_t>(ct_params_t::stress_diff) == compact,
    //              "UWLCM smagorinsky model requires compact stress differencing");

    auto& tht = this->state(ix::th);
    this->xchng_pres(tht, this->ijk);

    this->vert_grad_cmpct(tht, tmp_grad[ct_params_t::n_dims - 1], params.dz);
    
    tdef_sq(this->ijk) = formulae::stress::calc_tdef_sq_cmpct<ct_params_t::n_dims>(this->tau, this->ijk);

    calc_rcdsn_num();

    this->k_m(this->ijk).reindex(this->zero) = where(
                                 rcdsn_num(this->ijk).reindex(this->zero) / prandtl_num < 1,
                                 pow(this->smg_c * (*this->params.mix_len)(this->vert_idx), 2)
                                 * sqrt(tdef_sq(this->ijk).reindex(this->zero)
                                        * (1 - rcdsn_num(this->ijk).reindex(this->zero) / prandtl_num)),
                                 0
                                );
    this->k_m(this->hrzntl_slice(0)) = this->k_m(this->hrzntl_slice(1));

    //this->k_m(this->ijk).reindex(this->zero) = pow(this->smg_c * (*this->params.mix_len)(this->vert_idx), 2)
    //                                         * sqrt(tdef_sq(this->ijk).reindex(this->zero));

    this->xchng_sclr(this->k_m, this->ijk, 1);
    

    // havo to use modified ijkm due to shared-memory parallelisation, otherwise overlapping ranges
    // would lead to double multiplications
    // TODO: better way ?
    auto ijkm_aux = this->ijkm;
    if (this->rank > 0)
      ijkm_aux[0] = this->ijk[0];

    formulae::stress::multiply_tnsr_cmpct<ct_params_t::n_dims, ct_params_t::opts>(this->tau, 1.0, this->k_m, *this->mem->G, ijkm_aux);

    this->xchng_sgs_tnsr_offdiag(this->tau, this->tau_srfc, this->ijk, this->ijkm);
    
    this->mem->barrier();
    if (this->rank == 0)
    {
      //std::cout << "resolved energy: " << 0.5 * sum(pow2(this->vips()[0](this->domain)) + pow2(this->vips()[1](this->domain)))  << std::endl;
      std::cout << "tdef_sq: " << min(tdef_sq(this->domain)) << " " << max(tdef_sq(this->domain)) << std::endl;
      std::cout << "rcdsn: " << min(rcdsn_num(this->domain)) << " " << max(rcdsn_num(this->domain)) << std::endl;
      std::cout << "k_m:   " << min(this->k_m(this->domain)) << " " << max(this->k_m(this->domain)) << std::endl;
      std::cout << "tau0:   " << min(this->tau[0](this->domain)) << " " << max(this->tau[0](this->domain)) << std::endl;
      std::cout << "tau1:   " << min(this->tau[1](this->domain)) << " " << max(this->tau[1](this->domain)) << std::endl;
      std::cout << "tau2:   " << min(this->tau[2](this->domain)) << " " << max(this->tau[2](this->domain)) << std::endl;
      //if (this->timestep % static_cast<int>(this->outfreq) == 0)
      //{
      //  std::cout << "k_m profile" << std::endl;
      //  for (int k = 0; k < 301; ++k)
      //  {
      //    std::cout << k << ' ' << sum(this->k_m(rng_t(0, 128), rng_t(0, 128), k)) / (129. * 129.) << std::endl;
      //  }
      //}
    }
    this->mem->barrier();
  }
  
  void record_flux(int s)
  {
    if (s != ix::th && s != ix::rv && s != ix::rc) return;

    if (this->timestep % static_cast<int>(this->outfreq) == 0)
    {

      real_t conv_fctr = 1.;

      if (s == ix::th)
      {
        auto conv_fctr_sens = (libcloudphxx::common::moist_air::c_pd<real_t>() * si::kilograms * si::kelvins / si::joules);
        conv_fctr = conv_fctr_sens;
      }
      else if (s == ix::rv || s == ix::rc)
      {
        auto conv_fctr_lat = (libcloudphxx::common::const_cp::l_tri<real_t>() * si::kilograms / si::joules);
        conv_fctr = conv_fctr_lat;
      }

      this->vert_aver_cmpct(tmp_grad[ct_params_t::n_dims - 1], hlpr[1], conv_fctr);
      this->mem->barrier();

      std::string name;
      if (s == ix::th)
      {
        name = "sgs_tht_flux";
      }
      else if (s == ix::rv)
      {
        name = "sgs_rv_flux";
      }
      else if (s == ix::rc)
      {
        name = "sgs_rc_flux";
      }

      if (this->rank == 0)
      {
        this->record_aux_dsc(name, hlpr[1]);
      }
      this->mem->barrier();
    }
  }
  
  void save_sgs_fields()
  {
    if (this->timestep % static_cast<int>(this->outfreq) == 0)
    {
      hlpr[0](this->ijk).reindex(this->zero) = pow2(this->k_m(this->ijk).reindex(this->zero)
                                                    / (this->c_m * (*this->params.mix_len)(this->vert_idx)));

      this->mem->barrier();
      if (this->rank == 0)
      {
        std::cout << "test u: " << min(this->state(ix::u)(this->domain)) << ' ' << max(this->state(ix::u)(this->domain)) << std::endl;
        std::cout << "test w: " << min(this->state(ix::w)(this->domain)) << ' ' << max(this->state(ix::w)(this->domain)) << std::endl;
        std::cout << "test tht: " << min(this->state(ix::th)(this->domain)) << ' ' << max(this->state(ix::th)(this->domain)) << std::endl;
        std::cout << "test rv: " << min(this->state(ix::rv)(this->domain)) << ' ' << max(this->state(ix::rv)(this->domain)) << std::endl;

        //std::cout << "test tht1: " << grad_tht[1](0, -1) << std::endl;
        //std::cout << "test tht2: " << grad_tht[1](0, 0) << std::endl;
        //std::cout << "test tht3: " << grad_tht[1](0, 1) << std::endl;
        //std::cout << "test tht4: " << hlpr[1](0, 0) << std::endl;
        //std::cout << "test tht5: " << hlpr[1](0, 1) << std::endl;
        //std::cout << "test tht6: " << this->k_m(0, 0) << std::endl;
        //std::cout << "test tht7: " << this->k_m(0, 1) << std::endl;
        std::cout << "recording sgs" << std::endl;
        this->record_aux_dsc("tke", hlpr[0]);
        this->record_aux_dsc("p", this->Phi);
      }
      this->mem->barrier();
    }
  }
  
  void sgs_scalar_forces(const std::vector<int> &sclr_indices) override
  {
    for (const auto s : sclr_indices)
    {
      auto& field = this->state(s);

      this->xchng_pres(field, this->ijk);

      formulae::nabla::calc_grad_cmpct<parent_t::n_dims>(tmp_grad, field, this->ijk, this->ijkm, this->dijk);

      // document why
      this->mem->barrier();
      
      formulae::stress::multiply_vctr_cmpct<ct_params_t::n_dims, ct_params_t::opts>(tmp_grad,
                                                                                    1.0 / prandtl_num,
                                                                                    this->k_m,
                                                                                    *this->mem->G,
                                                                                    this->ijk);
      if (s == ix::th)
      {
        this->xchng_sgs_vctr(tmp_grad, this->surf_flux_sens, this->ijk);
      }
      else if (s == ix::rv)
      {
        this->xchng_sgs_vctr(tmp_grad , this->surf_flux_lat , this->ijk);
      }
      else
      {
        this->xchng_sgs_vctr(tmp_grad , this->surf_flux_zero, this->ijk);
      }

      record_flux(s);

      //hlpr[1](this->ijk) = 2 * 
      //  formulae::stress::flux_div_cmpct<parent_t::n_dims, ct_params_t::opts>(tmp_grad,
      //                                                                        *this->mem->G,
      //                                                                        this->ijk,
      //                                                                        this->dijk);
      //
      //this->rhs.at(s)(this->ijk) += 2 * hlpr[1](this->ijk);
    
      this->rhs.at(s)(this->ijk) += 2 * formulae::stress::flux_div_cmpct<parent_t::n_dims, ct_params_t::opts>(
                                          tmp_grad,
                                          *this->mem->G,
                                          this->ijk,
                                          this->dijk
                                    );
    }
    
    
    //hlpr[0](this->ijk) = 2 *

    //hlpr[1](this->ijk) = 2 * 
    //  formulae::stress::flux_div_cmpct<parent_t::n_dims, ct_params_t::opts>(grad_rv,
    //                                                                        *this->mem->G,
    //                                                                        this->ijk,
    //                                                                        this->dijk);
    //
    //this->rhs.at(ix::rv)(this->ijk) += 2 * hlpr[1](this->ijk);

    //if (this->rank == 0)
    //{
    //  std::cout << "timestep: " << this->timestep << std::endl;
    //  std::cout << "prandtl num: " << prandtl_num << std::endl;
    //  std::cout << "th sgs forces: " << min(hlpr[0](this->domain)) << ' ' << max(hlpr[0](this->domain)) << std::endl;
    //  std::cout << "rv sgs forces: " << min(hlpr[1](this->domain)) << ' ' << max(hlpr[1](this->domain)) << std::endl;
    //}
  }

  void hook_ante_loop(int nt) 
  {
    parent_t::hook_ante_loop(nt); 
  }

  void hook_ante_step()
  {
    parent_t::hook_ante_step(); 
  }

  void hook_post_step()
  {
    parent_t::hook_post_step();
    save_sgs_fields();
    sgs_scalar_forces({ix::th, ix::rv});
  }

  public:

  struct rt_params_t : parent_t::rt_params_t 
  { 
    real_t prandtl_num;
    setup::arr_1D_t *mix_len;
  };

  // per-thread copy of params
  rt_params_t params;

  // ctor
  slvr_sgs( 
    typename parent_t::ctor_args_t args, 
    const rt_params_t &p
  ) : 
    parent_t(args, p),
    params(p),
    prandtl_num(p.prandtl_num),
    rcdsn_num(args.mem->tmp[__FILE__][0][0]),
    tdef_sq(args.mem->tmp[__FILE__][0][1]),
    tmp_grad(args.mem->tmp[__FILE__][1]),
    hlpr(args.mem->tmp[__FILE__][2])
  {}

  static void alloc(typename parent_t::mem_t *mem, const int &n_iters)
  {
    parent_t::alloc(mem, n_iters);
    parent_t::alloc_tmp_sclr(mem, __FILE__, 2); // rcdsn_num, tdef_sq
    parent_t::alloc_tmp_vctr(mem, __FILE__); // tmp_grad
    parent_t::alloc_tmp_sclr(mem, __FILE__, 4); // hlpr
  }
};
