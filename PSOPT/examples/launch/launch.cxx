//////////////////////////////////////////////////////////////////////////
//////////////////             launch.cxx           //////////////////////
//////////////////////////////////////////////////////////////////////////
////////////////           PSOPT  Example             ////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////// Title:         Multiphase vehicle launch         ////////////////
//////// Last modified: 05 January 2009                   ////////////////
//////// Reference:     GPOPS Manual                      ////////////////
//////// (See PSOPT handbook for full reference)          ////////////////
//////////////////////////////////////////////////////////////////////////
////////     Copyright (c) Victor M. Becerra, 2009        ////////////////
//////////////////////////////////////////////////////////////////////////
//////// This is part of the PSOPT software library, which ///////////////
//////// is distributed under the terms of the GNU Lesser ////////////////
//////// General Public License (LGPL)                    ////////////////
//////////////////////////////////////////////////////////////////////////

#include "psopt.h"


//////////////////////////////////////////////////////////////////////////
///////////////////  Declare auxiliary functions  ////////////////////////
//////////////////////////////////////////////////////////////////////////

void oe2rv(DMatrix& oe, double mu, DMatrix* ri, DMatrix* vi);

void rv2oe(adouble* rv, adouble* vv, double mu, adouble* oe);


//////////////////////////////////////////////////////////////////////////
/////////  Declare an auxiliary structure to hold local constants  ///////
//////////////////////////////////////////////////////////////////////////

struct Constants {
  DMatrix* omega_matrix;
  double mu;
  double cd;
  double sa;
  double rho0;
  double H;
  double Re;
  double g0;
  double thrust_srb;
  double thrust_first;
  double thrust_second;
  double ISP_srb;
  double ISP_first;
  double ISP_second;
};

typedef struct Constants Constants_;


//////////////////////////////////////////////////////////////////////////
///////////////////  Define the end point (Mayer) cost function //////////
//////////////////////////////////////////////////////////////////////////

adouble endpoint_cost(adouble* initial_states, adouble* final_states,
                      adouble* parameters,adouble& t0, adouble& tf,
                      adouble* xad, int iphase, Workspace* workspace)
{
    adouble retval;
    adouble mass_tf = final_states[6];

    if (iphase < 4)
      retval = 0.0;

    if (iphase== 4)
      retval = -mass_tf;

    return retval;
}

//////////////////////////////////////////////////////////////////////////
///////////////////  Define the integrand (Lagrange) cost function  //////
//////////////////////////////////////////////////////////////////////////

adouble integrand_cost(adouble* states, adouble* controls, adouble* parameters,
                     adouble& time, adouble* xad, int iphase, Workspace* workspace)

{
    return  0.0;
}



//////////////////////////////////////////////////////////////////////////
///////////////////  Define the DAE's ////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void dae(adouble* derivatives, adouble* path, adouble* states,
         adouble* controls, adouble* parameters, adouble& time,
         adouble* xad, int iphase, Workspace* workspace)
{

    int j;
    
    Constants_& CONSTANTS = *( (Constants_ *) workspace->problem->user_data );

    adouble* x = states;
    adouble* u = controls;

    adouble  r[3]; r[0]=x[0]; r[1]=x[1]; r[2]=x[2];

    adouble  v[3]; v[0]=x[3]; v[1]=x[4]; v[2]=x[5];

    adouble  m = x[6];

    double T_first, T_second, T_srb, T_tot, m1dot, m2dot, mdot;

    adouble rad = sqrt( dot( r, r, 3)  );

    DMatrix& omega_matrix = *CONSTANTS.omega_matrix;

    adouble vrel[3];
    for (j=0;j<3;j++)
      vrel[j] =  v[j] - omega_matrix(j+1,1)*r[0] -omega_matrix(j+1,2)*r[1] - omega_matrix(j+1,3)*r[2];


    adouble speedrel = sqrt( dot(vrel,vrel,3) );
    adouble altitude = rad-CONSTANTS.Re;

    adouble rho = CONSTANTS.rho0*exp(-altitude/CONSTANTS.H);
    double   a1 = CONSTANTS.rho0*CONSTANTS.sa*CONSTANTS.cd;
    adouble  a2 = a1*exp(-altitude/CONSTANTS.H);
    adouble  bc = (rho/(2*m))*CONSTANTS.sa*CONSTANTS.cd;



    adouble bcspeed    = bc*speedrel;

    adouble Drag[3];
    for(j=0;j<3;j++) Drag[j] =  - (vrel[j]*bcspeed);



    adouble muoverradcubed = (CONSTANTS.mu)/(pow(rad,3));
    adouble grav[3];
    for(j=0;j<3;j++) grav[j] = -muoverradcubed*r[j];


   if (iphase==1) {
     T_srb = 6*CONSTANTS.thrust_srb;
     T_first = CONSTANTS.thrust_first;
     T_tot = T_srb+T_first;
     m1dot = -T_srb/(CONSTANTS.g0*CONSTANTS.ISP_srb);
     m2dot = -T_first/(CONSTANTS.g0*CONSTANTS.ISP_first);
     mdot = m1dot+m2dot;
   }
   else if (iphase==2) {
     T_srb = 3*CONSTANTS.thrust_srb;
     T_first = CONSTANTS.thrust_first;
     T_tot = T_srb+T_first;
     m1dot = -T_srb/(CONSTANTS.g0*CONSTANTS.ISP_srb);
     m2dot = -T_first/(CONSTANTS.g0*CONSTANTS.ISP_first);
     mdot = m1dot+m2dot;
   }
   else if (iphase==3) {
     T_first = CONSTANTS.thrust_first;
     T_tot = T_first;
     mdot = -T_first/(CONSTANTS.g0*CONSTANTS.ISP_first);
   }
   else if (iphase==4) {
     T_second = CONSTANTS.thrust_second;
     T_tot = T_second;
     mdot = -T_second/(CONSTANTS.g0*CONSTANTS.ISP_second);
   }


   adouble Toverm = T_tot/m;

   adouble thrust[3];

   for(j=0;j<3;j++) thrust[j] = Toverm*u[j];

   adouble rdot[3];
   for(j=0;j<3;j++) rdot[j] = v[j];

   adouble vdot[3];
   for(j=0;j<3;j++)  vdot[j] = thrust[j]+Drag[j]+grav[j];


   derivatives[0] = rdot[0];
   derivatives[1] = rdot[1];
   derivatives[2] = rdot[2];
   derivatives[3] = vdot[0];
   derivatives[4] = vdot[1];
   derivatives[5] = vdot[2];
   derivatives[6] = mdot;

   path[0] = dot( controls, controls, 3);

}

////////////////////////////////////////////////////////////////////////////
///////////////////  Define the events function ////////////////////////////
////////////////////////////////////////////////////////////////////////////

void events(adouble* e, adouble* initial_states, adouble* final_states,
            adouble* parameters,adouble& t0, adouble& tf, adouble* xad,
            int iphase, Workspace* workspace)
{


   Constants_& CONSTANTS = *( (Constants_ *) workspace->problem->user_data );
   
   adouble rv[3]; rv[0]=final_states[0]; rv[1]=final_states[1]; rv[2]=final_states[2];
   adouble vv[3]; vv[0]=final_states[3]; vv[1]=final_states[4]; vv[2]=final_states[5];

   adouble oe[6];

   int j;

   if(iphase==1) {
       // These events are related to the initial state conditions in phase 1
       for(j=0;j<7;j++) e[j] = initial_states[j];
   }

   if (iphase==4) {
     // These events are related to the final states in phase 4
     rv2oe( rv, vv, CONSTANTS.mu, oe );
     for(j=0;j<5;j++) e[j]=oe[j];

   }



}


///////////////////////////////////////////////////////////////////////////
///////////////////  Define the phase linkages function ///////////////////
///////////////////////////////////////////////////////////////////////////

void linkages( adouble* linkages, adouble* xad, Workspace* workspace)
{

    double m_tot_first   = 104380.0;
    double m_prop_first  = 95550.0;
    double m_dry_first   = m_tot_first-m_prop_first;
    double m_tot_srb     = 19290.0;
    double m_prop_srb    = 17010.0;
    double m_dry_srb     = m_tot_srb-m_prop_srb;

    int index=0;

    auto_link(linkages, &index, xad, 1, 2, workspace );
    linkages[index-2]-= 6*m_dry_srb;
    auto_link(linkages, &index, xad, 2, 3, workspace );
    linkages[index-2]-= 3*m_dry_srb;
    auto_link(linkages, &index, xad, 3, 4, workspace );
    linkages[index-2]-= m_dry_first;

}



////////////////////////////////////////////////////////////////////////////
///////////////////  Define the main routine ///////////////////////////////
////////////////////////////////////////////////////////////////////////////

int main(void)
{
////////////////////////////////////////////////////////////////////////////
///////////////////  Declare key structures ////////////////////////////////
////////////////////////////////////////////////////////////////////////////

    Alg  algorithm;
    Sol  solution;
    Prob problem;

////////////////////////////////////////////////////////////////////////////
///////////////////  Register problem name  ////////////////////////////////
////////////////////////////////////////////////////////////////////////////

    problem.name        		= "Multiphase vehicle launch";
    problem.outfilename                 = "launch.txt";

////////////////////////////////////////////////////////////////////////////
///////////////////  Declare an instance of Constants structure /////////////
////////////////////////////////////////////////////////////////////////////

    
    Constants_ CONSTANTS;    
    
    problem.user_data = (void*) &CONSTANTS;



////////////////////////////////////////////////////////////////////////////
////////////  Define problem level constants & do level 1 setup ////////////
////////////////////////////////////////////////////////////////////////////

    problem.nphases   			= 4;
    problem.nlinkages                   = 24;

    psopt_level1_setup(problem);

/////////////////////////////////////////////////////////////////////////////
/////////   Define phase related information & do level 2 setup  ////////////
/////////////////////////////////////////////////////////////////////////////

    problem.phases(1).nstates   = 7;
    problem.phases(1).ncontrols = 3;
    problem.phases(1).nevents   = 7;
    problem.phases(1).npath     = 1;

    problem.phases(2).nstates   = 7;
    problem.phases(2).ncontrols = 3;
    problem.phases(2).nevents   = 0;
    problem.phases(2).npath     = 1;


    problem.phases(3).nstates   = 7;
    problem.phases(3).ncontrols = 3;
    problem.phases(3).nevents   = 0;
    problem.phases(3).npath     = 1;

    problem.phases(4).nstates   = 7;
    problem.phases(4).ncontrols = 3;
    problem.phases(4).nevents   = 5;
    problem.phases(4).npath     = 1;

    problem.phases(1).nodes     = "[5, 15]";
    problem.phases(2).nodes     = "[5, 15]";
    problem.phases(3).nodes     = "[5, 15]";
    problem.phases(4).nodes     = "[5, 15]";





    psopt_level2_setup(problem, algorithm);

////////////////////////////////////////////////////////////////////////////
///////////////////  Declare DMatrix objects to store results //////////////
////////////////////////////////////////////////////////////////////////////

    DMatrix x, u, t, H;

////////////////////////////////////////////////////////////////////////////
///////////////////  Initialize CONSTANTS and //////////////////////////////
///////////////////  declare local variables  //////////////////////////////
////////////////////////////////////////////////////////////////////////////


    double omega            = 7.29211585e-5; // Earth rotation rate (rad/s)
    DMatrix omega_matrix(3,3);

    omega_matrix(1,1) = 0.0;   omega_matrix(1,2) = -omega;   omega_matrix(1,3)=0.0;
    omega_matrix(2,1) = omega; omega_matrix(2,2) = 0.0;      omega_matrix(2,3)=0.0;
    omega_matrix(3,1) = 0.0;   omega_matrix(3,2) = 0.0;      omega_matrix(3,3)=0.0;

    CONSTANTS.omega_matrix = &omega_matrix; // Rotation rate matrix (rad/s)
    CONSTANTS.mu = 3.986012e14;       // Gravitational parameter (m^3/s^2)
    CONSTANTS.cd = 0.5;               // Drag coefficient
    CONSTANTS.sa = 4*pi;              // Surface area (m^2)
    CONSTANTS.rho0 = 1.225;           // sea level gravity (kg/m^3)
    CONSTANTS.H = 7200.0;             // Density scale height (m)
    CONSTANTS.Re = 6378145.0;         // Radius of earth (m)
    CONSTANTS.g0 = 9.80665;           // sea level gravity (m/s^2)

    double lat0 = 28.5*pi/180.0;               // Geocentric Latitude of Cape Canaveral
    double x0 = CONSTANTS.Re*cos(lat0);      // x component of initial position
    double z0 = CONSTANTS.Re*sin(lat0);      // z component of initial position
    double y0 = 0;
    DMatrix r0(3,1,  x0, y0, z0);
    DMatrix v0 = omega_matrix*r0;

    double bt_srb = 75.2;
    double bt_first = 261.0;
    double bt_second = 700.0;

    double t0 = 0;
    double t1 = 75.2;
    double t2 = 150.4;
    double t3 = 261.0;
    double t4 = 961.0;

    double m_tot_srb     = 19290.0;
    double m_prop_srb    = 17010.0;
    double m_dry_srb     = m_tot_srb-m_prop_srb;
    double m_tot_first   = 104380.0;
    double m_prop_first  = 95550.0;
    double m_dry_first   = m_tot_first-m_prop_first;
    double m_tot_second  = 19300.0;
    double m_prop_second = 16820.0;
    double m_dry_second  = m_tot_second-m_prop_second;
    double m_payload     = 4164.0;
    double thrust_srb    = 628500.0;
    double thrust_first  = 1083100.0;
    double thrust_second = 110094.0;
    double mdot_srb      = m_prop_srb/bt_srb;
    double ISP_srb       = thrust_srb/(CONSTANTS.g0*mdot_srb);
    double mdot_first    = m_prop_first/bt_first;
    double ISP_first     = thrust_first/(CONSTANTS.g0*mdot_first);
    double mdot_second   = m_prop_second/bt_second;
    double ISP_second     = thrust_second/(CONSTANTS.g0*mdot_second);

    double af = 24361140.0;
    double ef = 0.7308;
    double incf = 28.5*pi/180.0;
    double Omf = 269.8*pi/180.0;
    double omf = 130.5*pi/180.0;
    double nuguess = 0;
    double cosincf = cos(incf);
    double cosOmf = cos(Omf);
    double cosomf = cos(omf);
    DMatrix oe(6,1, af, ef, incf, Omf, omf, nuguess);

    DMatrix rout(3,1);
    DMatrix vout(3,1);

    oe2rv(oe,CONSTANTS.mu, &rout, &vout);

    rout.Transpose();
    vout.Transpose();

    double m10 = m_payload+m_tot_second+m_tot_first+9*m_tot_srb;
    double m1f = m10-(6*mdot_srb+mdot_first)*t1;
    double m20 = m1f-6*m_dry_srb;
    double m2f = m20-(3*mdot_srb+mdot_first)*(t2-t1);
    double m30 = m2f-3*m_dry_srb;
    double m3f = m30-mdot_first*(t3-t2);
    double m40 = m3f-m_dry_first;
    double m4f = m_payload;

    CONSTANTS.thrust_srb    = thrust_srb;
    CONSTANTS.thrust_first  = thrust_first;
    CONSTANTS.thrust_second = thrust_second;
    CONSTANTS.ISP_srb       = ISP_srb;
    CONSTANTS.ISP_first     = ISP_first;
    CONSTANTS.ISP_second    = ISP_second;


    double rmin = -2*CONSTANTS.Re;
    double rmax = -rmin;
    double vmin = -10000.0;
    double vmax = -vmin;

////////////////////////////////////////////////////////////////////////////
///////////////////  Enter problem bounds information //////////////////////
////////////////////////////////////////////////////////////////////////////


    int iphase;

    problem.bounds.lower.times = "[0, 75.2, 150.4, 261.0, 261.0]";
    problem.bounds.upper.times = "[0, 75.2, 150.4  261.0, 961.0]";



    // Phase 1 bounds

    iphase =  1;




    problem.phases(iphase).bounds.lower.states(1) = rmin;
    problem.phases(iphase).bounds.upper.states(1) = rmax;
    problem.phases(iphase).bounds.lower.states(2) = rmin;
    problem.phases(iphase).bounds.upper.states(2) = rmax;
    problem.phases(iphase).bounds.lower.states(3) = rmin;
    problem.phases(iphase).bounds.upper.states(3) = rmax;
    problem.phases(iphase).bounds.lower.states(4) = vmin;
    problem.phases(iphase).bounds.upper.states(4) = vmax;
    problem.phases(iphase).bounds.lower.states(5) = vmin;
    problem.phases(iphase).bounds.upper.states(5) = vmax;
    problem.phases(iphase).bounds.lower.states(6) = vmin;
    problem.phases(iphase).bounds.upper.states(6) = vmax;
    problem.phases(iphase).bounds.lower.states(7) = m1f;
    problem.phases(iphase).bounds.upper.states(7) = m10;


    problem.phases(iphase).bounds.lower.controls(1) = -1.0;
    problem.phases(iphase).bounds.upper.controls(1) =  1.0;
    problem.phases(iphase).bounds.lower.controls(2) = -1.0;
    problem.phases(iphase).bounds.upper.controls(2) =  1.0;
    problem.phases(iphase).bounds.lower.controls(3) = -1.0;
    problem.phases(iphase).bounds.upper.controls(3) =  1.0;

    problem.phases(iphase).bounds.lower.path(1)     = 1.0;
    problem.phases(iphase).bounds.upper.path(1)     = 1.0;


    // The following bounds fix the initial state conditions in phase 0.

    problem.phases(iphase).bounds.lower.events(1) = r0(1);
    problem.phases(iphase).bounds.upper.events(1) = r0(1);
    problem.phases(iphase).bounds.lower.events(2) = r0(2);
    problem.phases(iphase).bounds.upper.events(2) = r0(2);
    problem.phases(iphase).bounds.lower.events(3) = r0(3);
    problem.phases(iphase).bounds.upper.events(3) = r0(3);
    problem.phases(iphase).bounds.lower.events(4) = v0(1);
    problem.phases(iphase).bounds.upper.events(4) = v0(1);
    problem.phases(iphase).bounds.lower.events(5) = v0(2);
    problem.phases(iphase).bounds.upper.events(5) = v0(2);
    problem.phases(iphase).bounds.lower.events(6) = v0(3);
    problem.phases(iphase).bounds.upper.events(6) = v0(3);
    problem.phases(iphase).bounds.lower.events(7) = m10;
    problem.phases(iphase).bounds.upper.events(7) = m10;

    // Phase 2 bounds

    iphase =  2;




    problem.phases(iphase).bounds.lower.states(1) = rmin;
    problem.phases(iphase).bounds.upper.states(1) = rmax;
    problem.phases(iphase).bounds.lower.states(2) = rmin;
    problem.phases(iphase).bounds.upper.states(2) = rmax;
    problem.phases(iphase).bounds.lower.states(3) = rmin;
    problem.phases(iphase).bounds.upper.states(3) = rmax;
    problem.phases(iphase).bounds.lower.states(4) = vmin;
    problem.phases(iphase).bounds.upper.states(4) = vmax;
    problem.phases(iphase).bounds.lower.states(5) = vmin;
    problem.phases(iphase).bounds.upper.states(5) = vmax;
    problem.phases(iphase).bounds.lower.states(6) = vmin;
    problem.phases(iphase).bounds.upper.states(6) = vmax;
    problem.phases(iphase).bounds.lower.states(7) = m2f;
    problem.phases(iphase).bounds.upper.states(7) = m20;

    problem.phases(iphase).bounds.lower.controls(1) = -1.0;
    problem.phases(iphase).bounds.upper.controls(1) =  1.0;
    problem.phases(iphase).bounds.lower.controls(2) = -1.0;
    problem.phases(iphase).bounds.upper.controls(2) =  1.0;
    problem.phases(iphase).bounds.lower.controls(3) = -1.0;
    problem.phases(iphase).bounds.upper.controls(3) =  1.0;

    problem.phases(iphase).bounds.lower.path(1)     = 1.0;
    problem.phases(iphase).bounds.upper.path(1)     = 1.0;

    // Phase 3 bounds

    iphase =  3;




    problem.phases(iphase).bounds.lower.states(1) = rmin;
    problem.phases(iphase).bounds.upper.states(1) = rmax;
    problem.phases(iphase).bounds.lower.states(2) = rmin;
    problem.phases(iphase).bounds.upper.states(2) = rmax;
    problem.phases(iphase).bounds.lower.states(3) = rmin;
    problem.phases(iphase).bounds.upper.states(3) = rmax;
    problem.phases(iphase).bounds.lower.states(4) = vmin;
    problem.phases(iphase).bounds.upper.states(4) = vmax;
    problem.phases(iphase).bounds.lower.states(5) = vmin;
    problem.phases(iphase).bounds.upper.states(5) = vmax;
    problem.phases(iphase).bounds.lower.states(6) = vmin;
    problem.phases(iphase).bounds.upper.states(6) = vmax;
    problem.phases(iphase).bounds.lower.states(7) = m3f;
    problem.phases(iphase).bounds.upper.states(7) = m30;

    problem.phases(iphase).bounds.lower.controls(1) = -1.0;
    problem.phases(iphase).bounds.upper.controls(1) =  1.0;
    problem.phases(iphase).bounds.lower.controls(2) = -1.0;
    problem.phases(iphase).bounds.upper.controls(2) =  1.0;
    problem.phases(iphase).bounds.lower.controls(3) = -1.0;
    problem.phases(iphase).bounds.upper.controls(3) =  1.0;

    problem.phases(iphase).bounds.lower.path(1)     = 1.0;
    problem.phases(iphase).bounds.upper.path(1)     = 1.0;

    // Phase 4 bounds

    iphase =  4;




    problem.phases(iphase).bounds.lower.states(1) = rmin;
    problem.phases(iphase).bounds.upper.states(1) = rmax;
    problem.phases(iphase).bounds.lower.states(2) = rmin;
    problem.phases(iphase).bounds.upper.states(2) = rmax;
    problem.phases(iphase).bounds.lower.states(3) = rmin;
    problem.phases(iphase).bounds.upper.states(3) = rmax;
    problem.phases(iphase).bounds.lower.states(4) = vmin;
    problem.phases(iphase).bounds.upper.states(4) = vmax;
    problem.phases(iphase).bounds.lower.states(5) = vmin;
    problem.phases(iphase).bounds.upper.states(5) = vmax;
    problem.phases(iphase).bounds.lower.states(6) = vmin;
    problem.phases(iphase).bounds.upper.states(6) = vmax;
    problem.phases(iphase).bounds.lower.states(7) = m4f;
    problem.phases(iphase).bounds.upper.states(7) = m40;

    problem.phases(iphase).bounds.lower.controls(1) = -1.0;
    problem.phases(iphase).bounds.upper.controls(1) =  1.0;
    problem.phases(iphase).bounds.lower.controls(2) = -1.0;
    problem.phases(iphase).bounds.upper.controls(2) =  1.0;
    problem.phases(iphase).bounds.lower.controls(3) = -1.0;
    problem.phases(iphase).bounds.upper.controls(3) =  1.0;

    problem.phases(iphase).bounds.lower.path(1)     = 1.0;
    problem.phases(iphase).bounds.upper.path(1)     = 1.0;

    problem.phases(iphase).bounds.lower.events(1)     = af;
    problem.phases(iphase).bounds.lower.events(2)     = ef;
    problem.phases(iphase).bounds.lower.events(3)     = incf;
    problem.phases(iphase).bounds.lower.events(4)     = Omf;
    problem.phases(iphase).bounds.lower.events(5)     = omf;
    problem.phases(iphase).bounds.upper.events(1)     = af;
    problem.phases(iphase).bounds.upper.events(2)     = ef;
    problem.phases(iphase).bounds.upper.events(3)     = incf;
    problem.phases(iphase).bounds.upper.events(4)     = Omf;
    problem.phases(iphase).bounds.upper.events(5)     = omf;

////////////////////////////////////////////////////////////////////////////
///////////////////  Define & register initial guess ///////////////////////
////////////////////////////////////////////////////////////////////////////

    iphase = 1;

    problem.phases(iphase).guess.states = zeros(7,5);

    problem.phases(iphase).guess.states(1, colon()) = linspace( r0(1), r0(1), 5);
    problem.phases(iphase).guess.states(2, colon()) = linspace( r0(2), r0(2), 5);
    problem.phases(iphase).guess.states(3, colon()) = linspace( r0(3), r0(3), 5);
    problem.phases(iphase).guess.states(4, colon()) = linspace( v0(1), v0(1), 5);
    problem.phases(iphase).guess.states(5, colon()) = linspace( v0(2), v0(2), 5);
    problem.phases(iphase).guess.states(6, colon()) = linspace( v0(3), v0(3), 5);
    problem.phases(iphase).guess.states(7, colon()) = linspace( m10  , m1f  , 5);

    problem.phases(iphase).guess.controls = zeros(3,5);

    problem.phases(iphase).guess.controls(1,colon()) = ones( 1, 5);
    problem.phases(iphase).guess.controls(2,colon()) = zeros(1, 5);
    problem.phases(iphase).guess.controls(3,colon()) = zeros(1, 5);


    problem.phases(iphase).guess.time = linspace(t0,t1, 5);


    iphase = 2;

    problem.phases(iphase).guess.states = zeros(7,5);

    problem.phases(iphase).guess.states(1, colon()) = linspace( r0(1), r0(1), 5);
    problem.phases(iphase).guess.states(2, colon()) = linspace( r0(2), r0(2), 5);
    problem.phases(iphase).guess.states(3, colon()) = linspace( r0(3), r0(3), 5);
    problem.phases(iphase).guess.states(4, colon()) = linspace( v0(1), v0(1), 5);
    problem.phases(iphase).guess.states(5, colon()) = linspace( v0(2), v0(2), 5);
    problem.phases(iphase).guess.states(6, colon()) = linspace( v0(3), v0(3), 5);
    problem.phases(iphase).guess.states(7, colon()) = linspace( m20  , m2f  , 5);

    problem.phases(iphase).guess.controls = zeros(3,5);

    problem.phases(iphase).guess.controls(1,colon()) = ones( 1, 5);
    problem.phases(iphase).guess.controls(2,colon()) = zeros(1, 5);
    problem.phases(iphase).guess.controls(3,colon()) = zeros(1, 5);


    problem.phases(iphase).guess.time = linspace(t1,t2, 5);

    iphase = 3;

    problem.phases(iphase).guess.states = zeros(7,5);

    problem.phases(iphase).guess.states(1, colon()) = linspace( rout(1), rout(1), 5);
    problem.phases(iphase).guess.states(2, colon()) = linspace( rout(2), rout(2), 5);
    problem.phases(iphase).guess.states(3, colon()) = linspace( rout(3), rout(3), 5);
    problem.phases(iphase).guess.states(4, colon()) = linspace( vout(1), vout(1), 5);
    problem.phases(iphase).guess.states(5, colon()) = linspace( vout(2), vout(2), 5);
    problem.phases(iphase).guess.states(6, colon()) = linspace( vout(3), vout(3), 5);
    problem.phases(iphase).guess.states(7, colon()) = linspace( m30  , m3f  , 5);

    problem.phases(iphase).guess.controls = zeros(3,5);

    problem.phases(iphase).guess.controls(1,colon()) = zeros( 1, 5);
    problem.phases(iphase).guess.controls(2,colon()) = zeros( 1, 5);
    problem.phases(iphase).guess.controls(3,colon()) = ones(  1, 5);


    problem.phases(iphase).guess.time = linspace(t2,t3, 5);

    iphase = 4;

    problem.phases(iphase).guess.states = zeros(7,5);

    problem.phases(iphase).guess.states(1, colon()) = linspace( rout(1), rout(1), 5);
    problem.phases(iphase).guess.states(2, colon()) = linspace( rout(2), rout(2), 5);
    problem.phases(iphase).guess.states(3, colon()) = linspace( rout(3), rout(3), 5);
    problem.phases(iphase).guess.states(4, colon()) = linspace( vout(1), vout(1), 5);
    problem.phases(iphase).guess.states(5, colon()) = linspace( vout(2), vout(2), 5);
    problem.phases(iphase).guess.states(6, colon()) = linspace( vout(3), vout(3), 5);
    problem.phases(iphase).guess.states(7, colon()) = linspace( m40  , m4f  , 5);

    problem.phases(iphase).guess.controls = zeros(3,5);

    problem.phases(iphase).guess.controls(1,colon()) = zeros( 1, 5);
    problem.phases(iphase).guess.controls(2,colon()) = zeros( 1, 5);
    problem.phases(iphase).guess.controls(3,colon()) = ones(  1, 5);

    problem.phases(iphase).guess.time = linspace(t3,t4, 5);


////////////////////////////////////////////////////////////////////////////
///////////////////  Register problem functions  ///////////////////////////
////////////////////////////////////////////////////////////////////////////


    problem.integrand_cost 	= &integrand_cost;
    problem.endpoint_cost 	= &endpoint_cost;
    problem.dae 		= &dae;
    problem.events 		= &events;
    problem.linkages		= &linkages;

////////////////////////////////////////////////////////////////////////////
///////////////////  Enter algorithm options  //////////////////////////////
////////////////////////////////////////////////////////////////////////////


    algorithm.nlp_method                  	= "IPOPT";
    algorithm.scaling                     	= "automatic";
    algorithm.derivatives                 	= "automatic";
    algorithm.nlp_iter_max                	= 500;
//    algorithm.mesh_refinement                   = "automatic";
//    algorithm.collocation_method = "trapezoidal";
    algorithm.ode_tolerance			= 1.e-5;

////////////////////////////////////////////////////////////////////////////
///////////////////  Now call PSOPT to solve the problem   //////////////////
////////////////////////////////////////////////////////////////////////////

    psopt(solution, problem, algorithm);

////////////////////////////////////////////////////////////////////////////
///////////  Extract relevant variables from solution structure   //////////
////////////////////////////////////////////////////////////////////////////

    x = (solution.get_states_in_phase(1) || solution.get_states_in_phase(2) ||
         solution.get_states_in_phase(3) || solution.get_states_in_phase(4) );
    u = (solution.get_controls_in_phase(1) || solution.get_controls_in_phase(2) ||
         solution.get_controls_in_phase(3) || solution.get_controls_in_phase(4) );
    t = (solution.get_time_in_phase(1) || solution.get_time_in_phase(2) ||
         solution.get_time_in_phase(3) || solution.get_time_in_phase(4) );


////////////////////////////////////////////////////////////////////////////
///////////  Save solution data to files if desired ////////////////////////
////////////////////////////////////////////////////////////////////////////

    x.Save("x.dat");
    u.Save("u.dat");
    t.Save("t.dat");

////////////////////////////////////////////////////////////////////////////
///////////  Plot some results if desired (requires gnuplot) ///////////////
////////////////////////////////////////////////////////////////////////////


    DMatrix r = x(colon(1,3),colon());

    DMatrix v = x(colon(4,6),colon());

    DMatrix altitude = Sqrt(sum(elemProduct(r,r)))/1000.0;

    DMatrix speed = Sqrt(sum(elemProduct(v,v)));

    plot(t,altitude,problem.name, "time (s)", "position (km)");


    plot(t,speed,problem.name, "time (s)", "speed (m/s)");

    plot(t,u,problem.name,"time (s)", "u");


    plot(t,altitude,problem.name, "time (s)", "position (km)", "alt",
                                  "pdf", "launch_position.pdf");

    plot(t,speed,problem.name, "time (s)", "speed (m/s)", "speed",
                               "pdf", "launch_speed.pdf");

    plot(t,u,problem.name,"time (s)", "u (dimensionless)", "u1 u2 u3",
                               "pdf", "launch_control.pdf");



}

////////////////////////////////////////////////////////////////////////////
////////////////// Define auxiliary functions //////////////////////////////
////////////////////////////////////////////////////////////////////////////


void rv2oe(adouble* rv, adouble* vv, double mu, adouble* oe)
{
       int j;

       adouble K[3]; K[0] = 0.0; K[1]=0.0; K[2]=1.0;

       adouble hv[3];
       cross(rv,vv, hv);

       adouble nv[3];
       cross(K, hv, nv);

       adouble n  = sqrt(  dot(nv,nv,3) );

       adouble h2 = dot(hv,hv,3);

       adouble v2 = dot(vv,vv,3);

       adouble r         = sqrt(dot(rv,rv,3));

       adouble ev[3];
       for(j=0;j<3;j++)  ev[j] = 1/mu *( (v2-mu/r)*rv[j] - dot(rv,vv,3)*vv[j] );

       adouble p         = h2/mu;

	adouble e  = sqrt(dot(ev,ev,3));		// eccentricity
	adouble a  = p/(1-e*e);	  			// semimajor axis
	adouble i  = acos(hv[2]/sqrt(h2));		// inclination



#define USE_SMOOTH_HEAVISIDE
        double a_eps = 0.1;

#ifndef USE_SMOOTH_HEAVISIDE
 	adouble Om = acos(nv[0]/n);			// RAAN
 	if ( nv[1] < -DMatrix::GetEPS() ){		// fix quadrant
 		Om = 2*pi-Om;
 	}
#endif

#ifdef USE_SMOOTH_HEAVISIDE

        adouble Om =  smooth_heaviside( (nv[1]+DMatrix::GetEPS()), a_eps )*acos(nv[0]/n)
                     +smooth_heaviside( -(nv[1]+DMatrix::GetEPS()), a_eps )*(2*pi-acos(nv[0]/n));
#endif

#ifndef USE_SMOOTH_HEAVISIDE
 	adouble om = acos(dot(nv,ev,3)/n/e);		// arg of periapsis
 	if ( ev[2] < 0 ) {				// fix quadrant
 		om = 2*pi-om;
 	}
#endif

#ifdef USE_SMOOTH_HEAVISIDE
        adouble om =  smooth_heaviside( (ev[2]), a_eps )*acos(dot(nv,ev,3)/n/e)
                     +smooth_heaviside( -(ev[2]), a_eps )*(2*pi-acos(dot(nv,ev,3)/n/e));
#endif

#ifndef USE_SMOOTH_HEAVISIDE
 	adouble nu = acos(dot(ev,rv,3)/e/r);		// true anomaly
 	if ( dot(rv,vv,3) < 0 ) {			// fix quadrant
 		nu = 2*pi-nu;
 	}
#endif

#ifdef USE_SMOOTH_HEAVISIDE
       adouble nu =  smooth_heaviside( dot(rv,vv,3), a_eps )*acos(dot(ev,rv,3)/e/r)
                     +smooth_heaviside( -dot(rv,vv,3), a_eps )*(2*pi-acos(dot(ev,rv,3)/e/r));
#endif

        oe[0] = a;
        oe[1] = e;
        oe[2] = i;
        oe[3] = Om;
        oe[4] = om;
        oe[5] = nu;

        return;

}


void oe2rv(DMatrix& oe, double mu, DMatrix* ri, DMatrix* vi)
{
	double a=oe(1), e=oe(2), i=oe(3), Om=oe(4), om=oe(5), nu=oe(6);
	double p = a*(1-e*e);
	double r = p/(1+e*cos(nu));
	DMatrix rv(3,1);
        rv(1) = r*cos(nu);
        rv(2) = r*sin(nu);
        rv(3) = 0.0;

	DMatrix vv(3,1);

        vv(1) = -sin(nu);
        vv(2) = e+cos(nu);
        vv(3) = 0.0;
        vv    *= sqrt(mu/p);

	double cO = cos(Om),  sO = sin(Om);
	double co = cos(om),  so = sin(om);
	double ci = cos(i),   si = sin(i);

	DMatrix R(3,3);
        R(1,1)=  cO*co-sO*so*ci; R(1,2)=  -cO*so-sO*co*ci; R(1,3)=  sO*si;
	R(2,1)=	 sO*co+cO*so*ci; R(2,2)=  -sO*so+cO*co*ci; R(2,3)=-cO*si;
	R(3,1)=	  so*si;         R(3,2)=    co*si;         R(3,3)=  ci;

	*ri = R*rv;
	*vi = R*vv;

        return;
}

////////////////////////////////////////////////////////////////////////////
///////////////////////      END OF FILE     ///////////////////////////////
////////////////////////////////////////////////////////////////////////////
