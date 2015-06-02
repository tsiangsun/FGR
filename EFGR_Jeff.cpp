/* This code calculates equilibrium Fermi Golden Rule rate
 in Condon case, Brownian oscillator model
 compare with linearized semiclassical methods
 To compile: g++ -o EFGR EFGR.cpp -llapack -lrefblas -lgfortran
 (c) Xiang Sun 2015
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
using namespace std;

string nameapp = "b1e1_";
double beta = 1;//0.2;//1;//5;
double eta = 1; //0.2;//1;//5;
const double DAcoupling = 0.1;
double s = 1; //Noneq. initial shift of parimary mode
double tp_fix = 5; //fixed t' for noneq FGR rate k(t',omega_DA) with scan omega_DA
double omega_DA_fix = 3; //fixed omega_DA, with scan tp

//const double DeltaTau =0.002; //time slice for t' griding
//const double tp_max = 40; //scanning tp option, DeltaTau as step
//const double Deltatp = 0.2;

double Omega = 0.05; //primary mode freq
double y_0 = 1.0; //shift of primary mode

const int n_omega = 200;
const double d_omega = 0.1;//0.1;//0.002;for gaussian//0.1; for ohmic
//const double omega_max = 20;//1;//20;//2.5 for gaussian// 20 for ohmic
const double d_omega_eff = 0.2; //for effective SD sampling rate
const double omega_c = 1; //cutoff freq for ohmic

const int LEN = 512;//512;//512;//1024; //number of t choices 1024 for gaussian//512 for ohmic
const double DeltaT=0.2;//0.2;//0.3; for gaussian//0.2 for ohmic //FFT time sampling interval
const double T0= -DeltaT*(LEN*0.5);//-DeltaT*LEN/2+DeltaT/2;

const double pi=3.14159265358979324;
const double RT_2PI= sqrt(2*pi);
const double hbar = 1;
//for gaussian spectral density
const double sigma = 0.1;
const double omega_op = 1.0;

void FFT(int dir, int m, double *x, double *y); //Fast Fourier Transform, 2^m data
void DFT(int dir, int m, double *x, double *y); //Discrete Fourier Transform
double S_omega_ohmic(double omega, double eta); //ohmic with decay spectral density
double S_omega_drude(double omega, double eta);//another spectral density
double S_omega_gaussian(double omega, double eta, double sigma, double omega_op);//gaussian spectral density
double J_omega_ohmic(double omega, double eta);//bath Ohmic SD
double J_omega_ohmic_eff(double omega, double eta); //effective SD for Ohmic bath
void Integrand_LSC(double omega, double t, double &re, double &im);
void Integrand_LSC_inh(double omega, double t, double &re, double &im);
void Integrand_CL_avg(double omega, double t, double &re, double &im);
void Integrand_CL_donor(double omega, double t, double &re, double &im);
void Integrand_2cumu(double omega, double t, double &re, double &im);
void Integrand_2cumu_inh(double omega, double t, double &re, double &im);
void Integrand_NE_exact(double omega, double tp, double tau,  double shift, double req, double &re, double &im);
double Integrate(double *data, int n, double dx);
double Sum(double *data, int n);

extern "C" {
    void dsyev_(const char &JOBZ, const char &UPLO, const int &N, double *A,
                const int &LDA, double *W, double *WORK, const int &LWORK,
                int &INFO);
}


int main (int argc, char *argv[]) {
    
    stringstream ss;
    string emptystr("");
    string filename;
    string idstr("");
    
    
    int mm(0), nn(1); // nn = 2^mm is number of (complex) data to FFT
    
    while (nn < LEN ) {
        mm++;
        nn *= 2;
    } //nn is the first 2^m that larger LEN
    
    double *corr1 = new double [nn];
    double *corr2 = new double [nn];
    
    double *corr1_orig = new double [nn]; //shifted origin to T0
    double *corr2_orig = new double [nn];
    
    double t;
    int i, j, a, b;
    double omega;
    int w; //count of omega
    double integ_re[n_omega];
    double integ_im[n_omega];
    
    ofstream outfile;
    ofstream outfile1;
    
    double integral_re, integral_im;
    integral_re = integral_im = 0;
    
    double SD[n_omega];
    double J_eff[n_omega];
    
    int M; //time slice for tau = 0 --> tp
    int m; //index of tau
    double shift = T0 / DeltaT;
    double N = nn;
    //cout << "shift = " << shift << endl;
    double linear_accum_re;
    double linear_accum_im;
    double linear_re;
    double linear_im;
    double temp_re;
    double temp_im;
    double c_eff[n_omega];//c from "effective" SD
    double req_eff[n_omega];//req from "effective" SD
    double Er(0); //reorganization energy from discrete normal modes
    double Er_bath(0);//reorganization energy for Ohmic bath
    double Er_eff(0); //reorganization energy for effective drude SD
    double Er_eff_Jw(0);
    double Er_eff_RRww(0);
    double a_parameter(0);
    double a_parameter_eff(0);

    
    /*
    //calculating Er from integrating the effective SD
    outfile.open("Er_eff.dat");
    for (eta = 2; eta < 10; eta += 0.02)
    {//eta=0.0;
        
        //setting up spectral density
        for (w = 1; w < n_omega; w++) J_eff[w] = J_omega_ohmic_eff(w*d_omega_eff, eta);
        for (w = 1; w < n_omega; w++) SD[w] = S_omega_ohmic(w*d_omega, eta); //Ohmic spectral density
        
        //outfile1.open("J_eff(omega).dat");
        //for (w = 1; w< n_omega; w++) outfile1 << J_eff[w] << endl;
        //outfile1.close();
        //outfile1.clear();
        
        Er_eff_RRww = 0;
        Er_eff_Jw = 0;
        a_parameter_eff = 0;
        
        for (w = 1; w < n_omega; w++) {
            //eq min for each "eff" normal mode
            req_eff[w] = sqrt(8.0 * d_omega_eff * J_eff[w] / pi / pow(w*d_omega_eff,3));
            //c_alpha for each "eff" normal mode
            c_eff[w] = sqrt( 2.0 / pi * J_eff[w] * d_omega_eff * d_omega_eff * w);
            
            Er_eff_Jw += J_eff[w] / (w * d_omega_eff) * d_omega_eff * 4.0/pi;
            Er_eff_RRww += 0.5 * req_eff[w] * req_eff[w]  * w * d_omega_eff * w * d_omega_eff;
            
            a_parameter_eff += 0.25 * pow(w * d_omega_eff,3) *req_eff[w]*req_eff[w] /tanh(beta*hbar* w * d_omega_eff *0.5);
        }
        //cout << "Er_eff = " << Er_eff << endl;
        
        //cout << "Er_eff_Jw   = " << Er_eff_Jw << endl;
        //cout << "Er_eff_RRww = " << Er_eff_RRww << endl;
        outfile << Er_eff_Jw  << endl;
        
        //cout << "a_parameter_eff = " << a_parameter_eff << endl;
    }
    outfile.close();
    outfile.clear();
     */
    

    // ********** BEGIN of Normal mode analysis ***********
    
    //Dimension of matrix (Check this every time)
    int dim = n_omega;
    
    //-------- initiate LAPACK -----------
    int col = dim, row = dim;
    //Allocate memory for the eigenvlues
    double *eig_val = new double [row];
    //Allocate memory for the matrix
    double **matrix = new double* [col];
    matrix[0] = new double [col*row];
    for (int i = 1; i < col; ++i)
        matrix[i] = matrix[i-1] + row;
    //Parameters for dsyev_ in LAPACK
    int lwork = 6*col, info;
    double *work = new double [lwork];
    //-------------------------------------
    
    double TT_ns[n_omega][n_omega];
    //transformation matrix: [normal mode]=[[TT_ns]]*[system-bath]
    //TT_ns * D_matrix * T_sn = diag, eigenvectors are row-vector of TT_ns
    double omega_nm[n_omega]; //normal mode frequencies
    double req_nm[n_omega]; //req of normal modes (acceptor shift)
    double c_nm[n_omega];//coupling strength of normal modes
    double S_array[n_omega];//Huang-Rhys factor for normal modes
    
    double D_matrix[n_omega][n_omega];// the Hessian matrix
    
    double c_bath[n_omega];//secondary bath mode coupling coefficients
    double Diag_matrix[n_omega][n_omega];

    
    // BEGIN loop - calculating Er from discrete normal modes with ohmic bath modes. scanning eta
    
    outfile1.open("Er_eta.dat");
    for (eta = 1; eta <= 2 ; eta += 1)
    {
        //eta=1.0;
        cout << "eta = " << eta << endl;
        cout << "c_bath[] = " << endl;
        //secondary bath mode min shift coefficients (for EXACT discrete normal mode analysis)
        Er_bath=0;
        for (w = 1; w < n_omega; w++) {
            //Ohmic SD:
            c_bath[w] = sqrt( 2.0 / pi * J_omega_ohmic(w*d_omega, eta) * d_omega * d_omega * w);
            //Gaussian SD:
            //c_bath[w] = sqrt( 2.0 / pi * S_omega_gaussian(w*d_omega, eta, sigma, omega_op) * d_omega * d_omega * w);
            Er_bath += 2.0 * c_bath[w] * c_bath[w] / (w*d_omega * w*d_omega);
            cout << c_bath[w] << endl;
        }
        
        cout << "Er_bath = " << Er_bath << endl; //checked for eta linearality
        
        for (i=0; i< n_omega; i++) for (j=0; j<n_omega ;j++) D_matrix[i][j] = 0;
        D_matrix[0][0] = Omega*Omega;
        for (w =1 ; w < n_omega ; w++) {
            D_matrix[0][0] += pow(c_bath[w]/(w*d_omega) ,2);
            D_matrix[0][w] = D_matrix[w][0] = c_bath[w];
            D_matrix[w][w] = pow(w*d_omega ,2);
        }
        
        //cout << "Hession matrix D:" << endl;
        for (i=0; i < dim; i++) {
            for (j=0; j < dim; j++) {
                matrix[j][i] = D_matrix[i][j]; //switch i j to match with Fortran array memory index
                //cout << D_matrix[i][j] << " ";
            }
            //cout << endl;
        }
        
        //diagonalize matrix, the eigenvectors transpose is in result matrix => TT_ns.
        dsyev_('V', 'L', col, matrix[0], col, eig_val, work, lwork, info); //diagonalize matrix
        if (info != 0) cout << "Lapack failed. " << endl;
        
        for (i=0; i < dim; i++) omega_nm[i] = sqrt(eig_val[i]);
        
        //outfile.open("normal_mode_freq.dat");
        //for (i=0; i < dim; i++) outfile << omega_nm[i] << endl;
        //outfile.close();
        //outfile.clear();
        
        //cout << "eigen values = ";
        //for (i=0; i < dim; i++) cout << eig_val[i] <<"    ";
        //cout << endl;
        
        for (i=0; i < dim; i++)
            for (j=0; j < dim; j++) TT_ns[i][j] = matrix[i][j];
        
        /*
         cout << "diagonalized Hessian matrix: " << endl;
         for (i=0; i < dim; i++) {
         for (j=0; j < dim; j++) {
         for (a=0; a < dim; a++)
         for (b=0; b < dim; b++) Diag_matrix[i][j] += TT_ns[i][a]*D_matrix[a][b]*TT_ns[j][b]; //TT_ns * D * T_sn
         cout << Diag_matrix[i][j] << "    " ;
         }
         cout << endl;
         }
         
         cout << endl;
         cout << "transformation matrix TT_ns (TT_ns * D * T_sn = diag, eigenvectors are row-vector of TT_ns): " << endl;
         for (i=0; i < dim; i++) {
         for (j=0; j < dim; j++) cout << TT_ns[i][j] << "    " ;
         cout << endl;
         }
         */
        
        // the coefficients of linear electronic coupling in normal modes (gamma[j]=TT_ns[j][0]*gamma_y), here gamma_y=1
        double gamma_nm[n_omega];
        for (i=0; i<n_omega; i++) gamma_nm[i] = TT_ns[i][0];
        
        double shift_NE[n_omega]; //the s_j shifting for initial sampling
        for (i=0; i<n_omega; i++) shift_NE[i] = s * gamma_nm[i];
        
        //req of normal modes (acceptor's potential energy min shift)
        for (i = 0; i < n_omega; i++) {
            req_nm[i] = 1 * TT_ns[i][0];
            for (a = 1; a < n_omega; a++) req_nm[i] -= TT_ns[i][a] * c_bath[a] / (a*d_omega * a*d_omega);
        }
        
        outfile.open("Huang-Rhys.dat");
        for (i = 0; i < n_omega; i++) {
            //tilde c_j coupling strength normal mode
            c_nm[i] = req_nm[i] * omega_nm[i] * omega_nm[i];
            req_nm[i] *= 2.0 * y_0;
            //discrete Huang-Rhys factor as follows
            S_array[i] = omega_nm[i] * req_nm[i] * req_nm[i] * 0.5;
            outfile << S_array[i] << endl;
        }
        outfile.close();
        outfile.clear();
        
        // ******** END of Normal mode analysis **************
        
        //the exact reorganization energy Er for Marcus theory
        Er = 0;
        a_parameter = 0;
        
        //for (i = 0; i < n_omega; i++) Er += 2.0 * c_nm[i] * c_nm[i] / (omega_nm[i] * omega_nm[i]);
        for (i = 0; i < n_omega; i++) Er += 0.5 * omega_nm[i] * omega_nm[i] * req_nm[i] * req_nm[i]; //S_array[i] * omega_nm[i];
        
        for (i = 0; i < n_omega; i++) a_parameter += 0.5 * S_array[i] * omega_nm[i] * omega_nm[i] /tanh(beta*hbar* omega_nm[i] *0.5);
        
        outfile1 << Er << endl;
        cout << "Er = " << Er << endl;
        cout << "~~~~~~~~" << endl;
    }
    outfile1.close();
    outfile1.clear();
    
    
    //-------------- Summary ----------------
    
    cout << "-----THERMAL CONDITION------- " << endl;
    cout << "normal modes n_omega = " << n_omega << endl;
    cout << "d_omega = " << d_omega << endl;
    cout << "beta = " << beta << endl;
    //cout << "eta = " << eta << endl;
    //cout << "initial shift s = " << s << endl;
    cout << "--------- END of EFGR in Condon case --------" << endl;
    
    
    
    return 0;
}



/********* SUBROUTINE *************/


//spectral densities

double S_omega_ohmic(double omega, double etaa) {
    return etaa * omega * exp(-1 * omega / omega_c);
}

double S_omega_drude(double omega, double etaa) {
    return etaa * omega /(1 + omega*omega);
}

double S_omega_gaussian(double omega, double etaa, double sigma, double omega_op) {
    return   0.5 / hbar * etaa * omega * exp(-(omega - omega_op)*(omega - omega_op)/(2*sigma*sigma))/RT_2PI/sigma;
}

double J_omega_ohmic(double omega, double etaa) {
    //notice definition J(omega) is different from S(omega)
    //J_omega = pi/2 * sum_a c_a^2 / omega_a delta(omega - omega_a)
    return etaa * omega * exp(-1 * omega / omega_c);
}

double J_omega_ohmic_eff(double omega, double etaa) {
    //(normal mode) effective SD for Ohmic bath DOF
    //J_omega = pi/2 * sum_a c_a^2 / omega_a delta(omega - omega_a)
    return etaa * omega * pow(Omega,4) / ( pow(Omega*Omega - omega*omega, 2) + etaa*etaa*omega*omega);
}

//min-to-min energy as Fourier transform frequency
void Integrand_LSC(double omega, double t, double &re, double &im) {
    re = (1-cos(omega*t))/tanh(beta*hbar*omega/2);
    im = sin(omega*t);
    return;
}

void Integrand_LSC_inh(double omega, double t, double &re, double &im) {
    re = omega*omega*t*t/2/tanh(beta*hbar*omega/2);
    im = omega*t;
    return;
}

void Integrand_CL_avg(double omega, double t, double &re, double &im) {
    re = (1-cos(omega*t))*2/(beta*hbar*omega);
    im = sin(omega*t);
    return;
}

void Integrand_CL_donor(double omega, double t, double &re, double &im) {
    re = (1-cos(omega*t))*2/(beta*hbar*omega);
    im = omega*t;
    return;
}

void Integrand_2cumu(double omega, double t, double &re, double &im) {
    re = (1-cos(omega*t))*2/(beta*hbar*omega);
    im = omega*t;
    return;
}

void Integrand_2cumu_inh(double omega, double t, double &re, double &im) {
    re = omega*t*t/(beta*hbar);
    im = omega*t;
    return;
}

//noneq FGR

void Integrand_NE_exact(double omega, double tp, double tau, double shift, double req, double &re, double &im) {//including Huang-Rhys factor S_j
    re = omega*req*req*0.5*(1-cos(omega*tau))/tanh(beta*hbar*omega/2);
    im = omega*req*req*0.5*sin(omega*tau) + omega*req*shift* (sin(omega*tp) - sin(omega*tp-omega*tau));
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

double Sum(double *data, int n){
    double I = 0;
    for (int i=0; i< n; i++) {
        I += data[i];
    }
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
    
    // times STEPS*DeltaT forward FFT (time --> freq)
    /*if (dir == 1) {
     for (i=0; i<n; i++) {
     x[i] *= 1;//DeltaT;
     y[i] *= 1;//DeltaT;
     }
     }*/
    
    // Scaling for inverse transform
    
    if (dir == -1) {
        for (i=0;i<n;i++) {
            x[i] /= n;
            y[i] /= n;
        }
    }
    
    /*
     //for symmetrical FT,
     double sqn;
     sqn = sqrt(n);
     for (i=0;i<n;i++) {
     x[i] /= sqn;
     y[i] /= sqn;
     }
     */
    
    return;
}


void DFT(int dir, int m, double *x, double *y) {
    /*
     This code computes an in-place complex-to-complex DFT by direct approach
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
    int n,i,j,k,N;
    
    // Calculate the number of points
    N = 1;
    for (i=0;i<m;i++)
        N *= 2;
    
    double *re = new double [N];
    double *im = new double [N];
    for (n = 0; n < N; n++) re[n] = im[n] = 0;
    double w = 2 * pi / N;
    
    
    if (abs(dir) != 1 ) cout << "error from DFT subroutine: dir ill defined"<< endl;
    
    for (n=0; n<N; n++) {
        for (k=0; k<N; k++) {
            re[n] += x[k] * cos(w*n*k) + dir * y[k] * sin(w*n*k);
            im[n] += y[k] * cos(w*n*k) - dir * x[k] * sin(w*n*k);
        }
    }
    
    for (n=0; n<N; n++) {
        x[n] = re[n];
        y[n] = im[n];
    }
    
    if (dir == -1)
        for (n=0; n<N;n++) {
            x[n] /= N;
            y[n] /= N;
        }
    
    delete [] re;
    delete [] im;
    return;
}



