/*!******************************************************************
 * \file blob2d.cxx
 *
 *       2D simulations 
 *
 *        NR Walkden, B Dudson  20 January 2012
 *******************************************************************/

#include <bout/physicsmodel.hxx>   // Commonly used BOUT++ components
#include <derivs.hxx>              // To use DDZ()
#include <invert_laplace.hxx>      // Laplacian inversion

/// 2D drift-reduced model, mainly used for blob studies
/// 
///
class Blob2D : public PhysicsModel {
private:
  // Evolving variables
  Field3D n,omega;                                ///< Density and vorticity

  // Auxilliary variables
  Field3D phi;                                    ///< Electrostatic potential
  
  // Parameters
  BoutReal rho_s;       ///< Bohm gyro radius
  BoutReal Omega_i;     ///< Ion cyclotron frequency
  BoutReal c_s;         ///< Bohm sound speed
  BoutReal n0;          ///< Reference density
  
  //Constants to calculate the parameters
  BoutReal Te0;   ///< Isothermal temperature [eV]
  BoutReal B0;    ///< Constant magnetic field [T]
  BoutReal m_i;   ///< Ion mass [kg]
  BoutReal m_e;   ///< Electron mass [kg]
  BoutReal e;     ///< Electron charge [C]
  
  BoutReal D_n,D_vort; ///< Diffusion coefficients
  BoutReal R_c;   ///< Radius of curvature
  BoutReal L_par; ///< Parallel connection length
  
  //Model options 
  bool boussinesq;                                ///< Use the Boussinesq approximation in vorticity
  bool compressible;                              ///< If allow inclusion of n grad phi term in density evolution
  bool sheath;                                    ///< Sheath connected?

  Laplacian *phiSolver;  ///< Performs Laplacian inversions to calculate phi
  
protected:
  int init(bool restarting) { 
    
    /******************Reading options *****************/
    
    Options *globalOptions = Options::getRoot();
    Options *options = globalOptions->getSection("model");
    
    // Load system parameters
    options->get("Te0", Te0, 30);         // Temp in eV
    options->get("e", e, 1.602e-19);   
    options->get("m_i",m_i,2*1.667e-27); 
    options->get("m_e",m_e,9.11e-31);
  
    options->get("n0",n0,1e19);           // Background density in cubic m
    options->get("D_vort",D_vort,0);      // Viscous diffusion coefficient
    options->get("D_n",D_n,0);            // Density diffusion coefficient
  
    options->get("R_c",   R_c,  1.5);     // Radius of curvature
    options->get("L_par", L_par, 10);     // Parallel connection length 
    OPTION(options, B0, 0.35);            // Value of magnetic field strength

    // System option switches
  
    OPTION(options, compressible,false);   // Include compressible ExB term in density equation
    OPTION(options, boussinesq,true);      // Use Boussinesq approximation in vorticity
    OPTION(options, sheath, true);         // Sheath closure
  
    /***************Calculate the Parameters **********/
  
    Omega_i = e*B0/m_i;           // Cyclotron Frequency
    c_s = sqrt(e * Te0/m_i);      // Bohm sound speed
    rho_s = c_s/Omega_i;          // Bohm gyro-radius
  
    output.write("\n\n\t----------Parameters: ------------ \n\tOmega_i = %e /s,\n\tc_s = %e m/s,\n\trho_s = %e m\n",
                 Omega_i, c_s, rho_s);

    // Calculate delta_*, blob size scaling
    output.write("\tdelta_* = rho_s * (dn/n) * %e ", pow( L_par*L_par / (R_c * rho_s), 1./5) );
  
    /************ Create a solver for potential ********/

    if(boussinesq) {
      phiSolver = Laplacian::create(Options::getRoot()->getSection("phiBoussinesq")); // BOUT.inp section "phiBoussinesq"
    }else {
      phiSolver = Laplacian::create(Options::getRoot()->getSection("phiSolver")); // BOUT.inp section "phiSolver"
    }
    phi = 0.0; // Starting guess for first solve (if iterative)
  
    /************ Tell BOUT++ what to solve ************/
  
    SOLVE_FOR2(n, omega);
  
    // Output phi  
    SAVE_REPEAT(phi);                
    SAVE_ONCE3(rho_s, c_s, Omega_i);
 
    return 0;
  }

  int rhs(BoutReal t) {
  
    // Run communications 
    ////////////////////////////////////////////////////////////////////////////
    mesh->communicate(n,omega);

    //Invert div(n grad(phi)) = grad(n) grad(phi) + n Delp_perp^2(phi) = omega
    //////////////////////////////////////////////////////////////////////////// 

    if(!boussinesq) {
      // Including full density in vorticit inversion
      phiSolver->setCoefC(n);  // Update the 'C' coefficient. See invert_laplace.hxx
      phi = phiSolver->solve(omega / n, phi);  // Use previous solution as guess
    }else {
      // Background density only (1 in normalised units)
      phi = phiSolver->solve(omega); 
    }
  
    mesh->communicate(phi);
  
    // Density Evolution
    /////////////////////////////////////////////////////////////////////////////
  
    ddt(n) = -bracket(phi,n,BRACKET_ARAKAWA)    // ExB term
      + 2*DDZ(n)*(rho_s/R_c)               // Curvature term
      + D_n*Delp2(n)
      ;                                         // Diffusion term
    if(compressible){
      ddt(n) -= 2*n*DDZ(phi)*(rho_s/R_c);       // ExB Compression term
    }

    if(sheath) {
      ddt(n) += n*phi*(rho_s/L_par); // - (n - 1)*(rho_s/L_par);      // Sheath closure
    }

    // Vorticity evolution
    /////////////////////////////////////////////////////////////////////////////

    ddt(omega) = -bracket(phi,omega, BRACKET_ARAKAWA)                     // ExB term
      + 2*DDZ(n)*(rho_s/R_c)/n                                   // Curvature term
      + D_vort*Delp2(omega)/n                                    // Viscous diffusion term
      ;

    if(sheath) {
      ddt(omega) += phi * (rho_s/L_par);
    }
  
    return 0;
  }
  
};

// Define a standard main() function
BOUTMAIN(Blob2D);
