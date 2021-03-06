/* This code calculates Fermi Golden Rule rate constant for charge transfer
 using multimode harmonic oscillator model, under different approximation levels
 (c) Xiang Sun 2014
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include "r_1279.h"  // r1279 random number generator
using namespace std;

const int NFILE = 100;
const int MIN = 1;

double beta = 0.1;//0.5;//0.1;//4;

const double hbar = 1;
const double eta = 1;
const double DAcoupling = 0.1;
//for gaussian spectral density
const double sigma = 0.1;
const double omega_op = 1.0;
const double omega_max = 20; //20 for ohmic, 2.5 for gaussian
const double d_omega = 0.1; //0.1 for ohmic, 0.025 for gaussian;
const int n_omega = static_cast<int>(omega_max / d_omega);

const int MCN = 10000;//1000000; //Monte Carlo sampling number

const int N = n_omega; //number of degrees of freedom
const int LEN = 512;// 512 for ohmic and 1024 for gaussian; //number of t choices in FFT
const int STEPS = 1; //for correlation function skip steps
const double DeltaT=0.2; // 0.2 for ohmic and 0.3 for gaussian; //FFT time sampling interval
const double T0= -DeltaT*LEN/2; //FFT time sampling starting point
const double pi=3.14159265358979;
const double RT_2PI= sqrt(2*pi);

double DT=0.002; //MD time step
double DTSQ2 = DT * DT * 0.5;
double DT2 = DT/2.0;
double ABSDT= abs(DT);



void FFT(int dir, int m, double *x, double *y); //Fast Fourier Transform, 2^m data
double J_omega_ohmic(double omega, double eta); //ohmic with decay spectral density
double J_omega_drude(double omega, double eta);//another spectral density
double J_omega_gaussian(double omega, double eta, double sigma, double omega_op);//gaussian spectral density
void Integrand_QM(double omega, double t, double &re, double &im);
double Integrate(double *data, int n, double dx);
double GAUSS(long *seed);
void MOVEA(double R[], double V[], double F[]);
void MOVEB(double V[], double F[]);
void force_avg(double R[], double F[], double omega[], double req[]);
void force_donor(double R[], double F[], double omega[], double req[]);
double DU(double R[], double omega[], double req[]);
double DUi(double R[], double omega[], double req[], int i);
int Job_finished(int &jobdone, int count, int total);
void propagate_donor(double R0[], double V0[], double t, double omega[], double Rt[]);


int main (int argc, char *argv[]) {
    
    int id; //jobid
    int i,j,k;
    stringstream ss;
    string emptystr("");
    string nameapp =   "Ohm_b0.1_5e4_";
    string nameappeq = "Ohm_b0.1_1e8_";
    string filename;
    string idstr("");
    
    if (argc > 1) {
        ss << argv[1];
        idstr += ss.str();
        ss >> id;
        ss.clear();
    }
 
    double *SD = new double [n_omega];
    double omega;
    int w; //count of omega
    
    //setting up spectral density
    for (w = 0; w < n_omega; w++) SD[w] = J_omega_ohmic(w*d_omega, eta); //Ohmic spectral density
    //for (w = 0; w < n_omega; w++) SD[w] = J_omega_drude(w*d_omega, eta); //Drude spectral density
    //for (w = 0; w < n_omega; w++) SD[w] = J_omega_gaussian(w*d_omega, eta, sigma, omega_op);


    cout << "---------- 2cumu --------------" << endl;
    cout << ">>> Job id # " << id << endl;
    int mm(0), nn(1); // nn = 2^mm is number of (complex) data to FFT
    
	while (nn < LEN ) {
		mm++;
		nn *= 2;
	} //nn is the first 2^m that larger than or equal to LEN
    
	double *corr1 = new double [nn];
	double *corr2 = new double [nn];
    
    double *corr1_orig = new double [nn]; //shifted origin to T0
    double *corr2_orig = new double [nn];
    
    double t;
    double shift = T0 / DeltaT;
    double NN = nn;
    
    int jobdone(0);
    
    double *integ_re = new double [n_omega];
    double *integ_im = new double [n_omega];
    
    ofstream outfile;
    ifstream infile;
    
    double integral_re, integral_im;
    integral_re = integral_im = 0;
    
    double Er=0;
    double a_parameter=0;
    
    double *integrand = new double [n_omega];
    for (w = 0; w < n_omega; w++) integrand[w] = SD[w] * w *d_omega;
    integrand[0]=0;
    Er = Integrate(integrand, n_omega, d_omega);
    
    for (w = 1; w < n_omega; w++) integrand[w] = SD[w] * w*d_omega * w*d_omega /tanh(beta*hbar* w * d_omega*0.5);
    integrand[0]=0;
    a_parameter = 0.5 * Integrate(integrand, n_omega, d_omega);
    cout << "Er = " << Er << endl;
    cout << "a_parameter = " << a_parameter << endl;
    
    
    //min-to-min energy Fourier frequency
    cout << "DeltaT = " << DeltaT << endl;
    cout << "LEN = " << LEN << endl;
    cout << "df = " << 1.0/LEN/DeltaT << endl;
    cout << "f_max = " << 0.5/DeltaT << endl;
    cout << "beta = " << beta << endl;
    cout << " eta = " << eta << endl;
    
    // LSC approximation (numerical)
    int MDlen;
    int tau;
    double omega_array[n_omega];
    int LENMD = static_cast<int>(LEN*DeltaT/DT); //number of steps for MD
    int NMD;
    
    
    double req[n_omega]; //acceptor equilibrium position
        
    double ducl0_avg(0);//for <du>_D
    double cuu0_avg(0);//for <du^2>_D
    
    double t_array[LEN];  // FFT time variable t = T0, T0+DeltaT...
    
    req[0]=0;
    for (w = 1; w < n_omega; w++) {// (1/pi*h)^n Prod_omega tanh(beta*hbar*omega/2)
        omega_array[w] = w * d_omega;
        req[w] = sqrt(d_omega*SD[w]*2*hbar/omega_array[w]);
        //sigma_x_cl[w] = 1/(omega_array[w]*sqrt(beta));
        //sigma_p_cl[w] = 1/sqrt(beta);
    }
    
    for (i=0; i < LEN; i++) {//prepare FFT time variable
        t_array[i] = T0 + DeltaT * i;
    }
    
    int NMD_forward = static_cast<int>(abs(t_array[LEN-1]/DT));
    int NMD_backward = static_cast<int>(abs(t_array[0]/DT));
    
    if(NMD_forward > NMD_backward) NMD = NMD_forward; //get max of NMD_forward and NMD_backward
    else NMD = NMD_backward;
    
    
    double *cuu_eq_D_forward = new double [NMD]; //for 2nd cumu
    double *cuu_eq_D_forward_accum = new double [NMD]; //for 2nd cumu analytical
    double *cuu_eq_D_forward_ana = new double [NMD]; //for 2nd cumu

    for (i=0; i< NMD; i++) cuu_eq_D_forward[i] = cuu_eq_D_forward_accum[i] = 0;
    
    cout << "Length of Cuu = " << NMD << endl;
    double integral;
    double *integrand_marcus = new double [LENMD];
    double du;
    string fnum;
    
    //analytical approach
    double ducl0_avg_ana(0);
    for (i=1;i<N;i++) ducl0_avg_ana += -0.5*omega_array[i]*omega_array[i]*req[i]*req[i];
    DT=ABSDT;
    for (tau =0 ; tau< NMD; tau++) {//NMD_forward
        for (w=1; w<N; w++) cuu_eq_D_forward_ana[tau] += omega_array[w]*omega_array[w]*req[w]*req[w]*cos(omega_array[w]*tau*DT)/beta;//completely analytical
    }
    outfile.open((emptystr+"cuu_ana_"+nameapp+"AVG"+fnum+".dat").c_str());
    for (i=0; i< NMD; i++) outfile << cuu_eq_D_forward_ana[i] << endl;
    outfile.close();
    outfile.clear();
    //2nd cumulant numerical approach
    
    int du0_count(0);
    int cuu0_count(0);
    
    for (j= 0; j< NFILE; j++) {
        ss.str("");
        ss << j+MIN;
        fnum = ss.str();
        
        filename = "<du>_eq_D_" + nameappeq;
        infile.open((filename+fnum+".dat").c_str());
        if (infile) {
            infile >> du;
            ducl0_avg += du;
            du0_count++;
        }
        infile.close();
        infile.clear();
        
        filename = "<du^2>_eq_D_" + nameappeq;
        infile.open((filename+fnum+".dat").c_str());
        if (infile) {
            infile >> du;
            cuu0_avg += du;
            cuu0_count++;
        }
        infile.close();
        infile.clear();
        
        filename = "cuu_eq_D_" + nameapp;
        infile.open((filename+fnum+".dat").c_str());
        for (i=0; i< NMD; i++) {//C(t) = <du(0)*du(t)>, not yet substracted  - <du>^2 in harmonic-surfaces
            infile >> cuu_eq_D_forward[i];
            cuu_eq_D_forward_accum[i] += cuu_eq_D_forward[i];//+du*du;
        }
        infile.close();
        infile.clear();
    }
    for (i=0; i<NMD;i++) {//average files
        cuu_eq_D_forward[i] =  cuu_eq_D_forward_accum[i] / NFILE;
    }
    ducl0_avg /= du0_count;
    cuu0_avg /= cuu0_count;
    cout << setprecision(10) << "** <du>_eq_D = " << ducl0_avg << endl;
    cout << setprecision(10) << "** <du^2>_eq_D = " << cuu0_avg << endl;
    
    for (i=0;i< NMD;i++) {
        //cuu_eq_D_forward[i] -= ducl0_avg * ducl0_avg; //numerical <du>_D
        cuu_eq_D_forward[i] -= ducl0_avg_ana * ducl0_avg_ana; //analytical <du>_D
    }
    outfile.open((emptystr+"cuu_eq_D_"+nameapp+"AVG"+fnum+".dat").c_str());
    for (i=0; i< NMD; i++) outfile << cuu_eq_D_forward[i] << endl;
    outfile.close();
    outfile.clear();

  
    DT = ABSDT;
    for (i=0; i< LEN; i++) {
        if (t_array[i] >= 0) {
            MDlen = static_cast<int>(abs(t_array[i] / DT));
            
            //test analytical
            for (j=0; j< MDlen; j++) integrand_marcus[j] = (t_array[i] - j*DT) * cuu_eq_D_forward_ana[j];
            integral = Integrate(integrand_marcus, MDlen, DT);
            cout << i << " ana  " << integral << endl;
            //numerical
            for (j=0; j< MDlen; j++) integrand_marcus[j] = (t_array[i] - j*DT) * cuu_eq_D_forward[j];
            integral = Integrate(integrand_marcus, MDlen, DT);
            cout << i << " num  " << integral << endl<<endl;
            corr1[i] = cos(t_array[i]*ducl0_avg/hbar) * exp(-integral/hbar/hbar);
            corr2[i] = sin(t_array[i]*ducl0_avg/hbar) * exp(-integral/hbar/hbar);
            //cout << i<<"    " <<corr1[i] <<" " <<corr2[i] << endl;
        }
        else {
            MDlen = static_cast<int>(abs(t_array[i] / DT));
            for (j=0; j< MDlen; j++) integrand_marcus[j] = (t_array[i] + j*DT) * cuu_eq_D_forward[j];
            integral = Integrate(integrand_marcus, MDlen, -DT); //check ABSDT or DT?
            corr1[i] = cos(t_array[i]*ducl0_avg/hbar) * exp(-integral/hbar/hbar);
            corr2[i] = sin(t_array[i]*ducl0_avg/hbar) * exp(-integral/hbar/hbar);
        }
    }
    
    ss.str("");
    ss << NFILE;
    fnum = ss.str();
    
     outfile.open((emptystr+"2cumu_t_re_"+nameapp+"AVG"+fnum+".dat").c_str());
     for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
     outfile.close();
     outfile.clear();
     
     outfile.open((emptystr+"2cumu_t_im_"+nameapp+"AVG"+fnum+".dat").c_str());
     for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
     outfile.close();
     outfile.clear();
     
     FFT(-1, mm, corr1, corr2);//notice its inverse FT
     
     for(i=0; i<nn; i++) { //shift time origin
     corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/NN) - corr2[i] * sin(-2*pi*i*shift/NN);
     corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/NN) + corr1[i] * sin(-2*pi*i*shift/NN);
     }
     
     
     outfile.open((emptystr+"2cumu_re_"+nameapp+"AVG"+fnum+".dat").c_str());
     for (i=0; i<nn/2; i++) outfile << corr1_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
     outfile.close();
     outfile.clear();
     
     outfile.open((emptystr+"2cumu_im_"+nameapp+"AVG"+fnum+".dat").c_str());
     for (i=0; i<nn/2; i++) outfile << corr2_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
     outfile.close();
     outfile.clear();
     
    
    //2cumu + inhomogeneous limit (Marcus)
    double c0 = cuu0_avg - ducl0_avg*ducl0_avg; //C(0)=<du^2> - <du>^2
    for (i=0; i< LEN; i++) {
        corr1[i] = cos(t_array[i]*ducl0_avg/hbar) * exp(-0.5*t_array[i]*t_array[i]*c0/hbar/hbar);
        corr2[i] = sin(t_array[i]*ducl0_avg/hbar) * exp(-0.5*t_array[i]*t_array[i]*c0/hbar/hbar);
        
    }
    outfile.open((emptystr+"2cui_t_re_"+nameapp+"AVG.dat").c_str());
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open((emptystr+"2cui_t_im_"+nameapp+"AVG.dat").c_str());
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(-1, mm, corr1, corr2);//notice its inverse FT
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/NN) - corr2[i] * sin(-2*pi*i*shift/NN);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/NN) + corr1[i] * sin(-2*pi*i*shift/NN);
    }
    
    
    outfile.open((emptystr+"2cui_re_"+nameapp+"AVG.dat").c_str());
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open((emptystr+"2cui_im_"+nameapp+"AVG.dat").c_str());
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    cout << "Done: job # " << nameapp  << endl;
    
    return 0;
}



/********* SUBROUTINE *************/


//spectral densities

double J_omega_ohmic(double omega, double eta) {
    return eta * omega * exp(-1 * omega);
}

double J_omega_drude(double omega, double eta) {
    return eta * omega /(1 + omega*omega);
}

double J_omega_gaussian(double omega, double eta, double sigma, double omega_op) {
    return   0.5 / hbar * eta * omega * exp(-(omega - omega_op)*(omega - omega_op)/(2*sigma*sigma))/RT_2PI/sigma;
}


//min-to-min energy as Fourier transform frequency
void Integrand_QM(double omega, double t, double &re, double &im) {
    re = (1-cos(omega*t))/tanh(beta*hbar*omega/2);
    im = sin(omega*t);
    return;
}


double Integrate(double *data, int n, double dx){
    double I =0;
    I += (data[0]+data[n-1])/2;
    for (int i=1; i< n-1; i++) {
        I += data[i];
    }
    I *= dx;
    return I;
}


void FFT(int dir, int m, double *x, double *y)
{/*
  This code computes an in-place complex-to-complex FFT Written by Paul Bourke
  x and y are the real and imaginary arrays of N=2^m points.
  dir =  1 gives forward transform
  dir = -1 gives reverse transform
  Formula: forward
  N-1
  ---
  1   \           - i 2 pi k n / N
  X(n) = ----  >   x(k) e                       = forward transform
  1   /                                    n=0..N-1
  ---
  k=0
  
  Formula: reverse
  N-1
  ---
  1  \           i 2 pi k n / N
  X(n) =  ---  >   x(k) e                  = reverse transform
  N  /                               n=0..N-1
  ---
  k=0
  */
    
    int n,i,i1,j,k,i2,l,l1,l2;
    double c1,c2,tx,ty,t1,t2,u1,u2,z;
    
    // Calculate the number of points
    n = 1;
    for (i=0;i<m;i++)
        n *= 2;
    
    // Do the bit reversal
    i2 = n >> 1; //i2 = (010 ...0)_2,second highest bit of n=(100 ...0)_2
    j = 0; //reversely bit accumulater from the second highest bit, i2.
    for (i=0;i<n-1;i++) {
        if (i < j) {
            tx = x[i]; //swap(i,j)
            ty = y[i];
            x[i] = x[j];
            y[i] = y[j];
            x[j] = tx;
            y[j] = ty;
        }
        //to find the highest non-one bit, k, from the second highest bit
        k = i2;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k; //add 1 reversly
    }
    
    // Compute the Radix-2 FFT: Cooley-Tukey Algorithm
    c1 = -1.0; // c1+i*c2 = -1 = c^(i 2Pi/2) = W_2, def W_N^j = e^(i 2j*Pi/N)
    c2 = 0.0;
    l2 = 1;
    for (l=0;l<m;l++) {
        l1 = l2;
        l2 <<= 1;
        u1 = 1.0;
        u2 = 0.0;
        for (j=0;j<l1;j++) {
            for (i=j;i<n;i+=l2) {
                //Butterfly calculation of x,y[i] and x,y[i1]:
                //t1+i*t2 =(u1+i*u2)(x[i1]+i*y[i2]) where u1+i*u2=W_N^j=e^(i 2j*Pi/N)
                i1 = i + l1;
                t1 = u1 * x[i1] - u2 * y[i1];
                t2 = u1 * y[i1] + u2 * x[i1];
                x[i1] = x[i] - t1;
                y[i1] = y[i] - t2;
                x[i] += t1;
                y[i] += t2;
            }
            // i1+i*u2 *= c1+i*c2, or W_N
            z =  u1 * c1 - u2 * c2;
            u2 = u1 * c2 + u2 * c1;
            u1 = z;
        }
        //c1+i*c2 = sqrt(c1+i*c2) eg. W_2 --> W_4 ...
        c2 = sqrt((1.0 - c1) / 2.0);
        if (dir == 1)
            c2 = -c2;
        c1 = sqrt((1.0 + c1) / 2.0);
    }
    
    // Scaling for inverse transform
    
    if (dir == -1) {
        for (i=0;i<n;i++) {
            x[i] /= n;
            y[i] /= n;
        }
    }
    
    return;
}

// generate Normal distribution of random variable(rannum) from a uniform distribution ran()
double GAUSS(long *seed) {
    double A1 = 3.949846138;
    double A3 = 0.252408784;
    double A5 = 0.076542912;
    double A7 = 0.008355968;
    double A9 = 0.029899776;
    double SUM, R, R2, random, rannum;
    SUM = 0.0;
    
    for (int i=1; i<=12; i++) {
        //random = ran2(seed);
        //random = r1279();
        random = ((double)rand() / (double)(RAND_MAX));
        SUM += random;
    }
    R = (SUM - 6.0)/4.0;
    R2 = R*R;
    rannum = ((((A9*R2+A7)*R2+A5)*R2+A3)*R2+A1)*R;
    return (rannum);
}

// SUROUTINE TO PERFORM VELOCITY VERLET ALGORITHM
void MOVEA(double R[], double V[], double F[]) {
    double xx;
    for (int i=1; i<N; i++) {
        xx = R[i] + DT*V[i] + DTSQ2*F[i];
        //pbc(xx, yy, zz);
        R[i] = xx;
        V[i] += DT2*F[i];
    }
    return;
}


void MOVEB(double V[], double F[]) {
	//always call MOVEB after call force** to update force F[]
    for (int i=1; i<N; i++) {
        V[i] += DT2*F[i];
    }
    return;
}

void force_avg(double R[], double F[], double omega[], double req[]) {
    //avg harmonic oscillator potential
    for (int i=1; i<N; i++) {
        F[i] = - omega[i] * omega[i] * (R[i]- req[i] * 0.5);
    }
    return;
}

void force_donor(double R[], double F[], double omega[], double req[]) {
    //donor harmonic oscillator potential
    for (int i=1; i<N; i++) {
        F[i] = - omega[i] * omega[i] * R[i];
    }
    return;
}

double DU(double R[], double omega[], double req[]) {
    double du=0;
    for (int i=1; i<N; i++) du += req[i]*omega[i]*omega[i]*R[i]-0.5*req[i]*req[i]*omega[i]*omega[i];
    return du;
}

double DUi(double R[], double omega[], double req[], int i) {
    double du=0;
    du = req[i]*omega[i]*omega[i]*R[i]-0.5*req[i]*req[i]*omega[i]*omega[i];
    return du;
}

void propagate_donor(double R0[], double V0[], double t, double omega[], double Rt[]){
    for (int i=1; i<N; i++) Rt[i] = R0[i] * cos(omega[i]*t) + V0[i]/omega[i] * sin(omega[i]*t);
    return;
}


int Job_finished(int &jobdone, int count, int total) {
	int tenpercent;
	tenpercent = static_cast<int> (10 * static_cast<double> (count)/ static_cast<double> (total) );
	if ( tenpercent > jobdone ) {
        jobdone = tenpercent;
        cout << "Job finished "<< jobdone <<"0%. " << endl;
	}
    
	return tenpercent;
}
