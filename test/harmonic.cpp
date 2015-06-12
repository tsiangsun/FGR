/* This code calculates Fermi Golden Rule rate constant for charge transfer
   using multiple harmonic oscillator model, under different approximation levels
   (c) Xiang Sun 2014
*/

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
using namespace std;

double beta = 1;//0.2;//1;//5;
double eta = 1; //0.2;//1;//5;
const double DAcoupling = 0.1;

//for gaussian spectral density
const double sigma = 0.1;
const double omega_op = 1.0;

const double omega_max = 20;//2.5 for gaussian// 20 for ohmic
const double d_omega = 0.1;//0.002;//0.1;//0.002;for gaussian//0.1; for ohmic

const double Omega = 0.1; //primary mode freq
const double y_0 = 1.0; //shift of primary mode

const int LEN = 512;//512;//1024; //number of t choices 1024 for gaussian//512 for ohmic
const double DeltaT=0.2;//0.3; for gaussian//0.2 or ohmic //FFT time sampling interval
const double T0= -DeltaT*LEN/2;//-DeltaT*LEN/2+DeltaT/2;
const double pi=3.14159265358979;
const double RT_2PI= sqrt(2*pi);
const double hbar = 1;


void FFT(int dir, int m, double *x, double *y); //Fast Fourier Transform, 2^m data
double S_omega_ohmic(double omega, double eta); //ohmic with decay spectral density
double S_omega_drude(double omega, double eta);//another spectral density
double S_omega_gaussian(double omega, double eta, double sigma, double omega_op);//gaussian spectral density
double S_omega_eff(double omega, double eta, double Omega, double y_0); //effective S(w) for GOA model with ohmic bath
void Integrand_LSC(double omega, double t, double &re, double &im);
void Integrand_LSC_inh(double omega, double t, double &re, double &im);
void Integrand_CL_avg(double omega, double t, double &re, double &im);
void Integrand_CL_donor(double omega, double t, double &re, double &im);
void Integrand_2cumu(double omega, double t, double &re, double &im);
void Integrand_2cumu_inh(double omega, double t, double &re, double &im);
void Integrand_LSC_v(double omega, double t, double &re, double &im);
void Integrand_LSC_inh_v(double omega, double t, double &re, double &im);
void Integrand_CL_avg_v(double omega, double t, double &re, double &im);
void Integrand_CL_donor_v(double omega, double t, double &re, double &im);
void Integrand_2cumu_v(double omega, double t, double &re, double &im);
void Integrand_2cumu_inh_v(double omega, double t, double &re, double &im);

double Integrate(double *data, int n, double dx);


int main (int argc, char *argv[]) {
    
    stringstream ss;
    string emptystr("");
    string nameapp = "Eff_";
    string filename;
    string idstr("");
    
    ss.str("");
    ss << "b" << beta;
    ss << "e" << eta;
    nameapp += ss.str();

    
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
    int i;
    double omega;
    int w; //count of omega
    int n_omega = static_cast<int>(omega_max / d_omega);
    double *integ_re = new double [n_omega];
    double *integ_im = new double [n_omega];
    
    ofstream outfile;
    
    double integral_re, integral_im;
    integral_re = integral_im = 0;
    
    double Er=0;
    double a_parameter=0;
    double *SD = new double [n_omega];
    
    //setting up spectral density
    for (w = 0; w < n_omega; w++) SD[w] = S_omega_ohmic(w*d_omega, eta); //Ohmic spectral density
    //for (w = 0; w < n_omega; w++) SD[w] = S_omega_drude(w*d_omega, eta); //Drude spectral density
    //for (w = 0; w < n_omega; w++) SD[w] = S_omega_gaussian(w*d_omega, eta, sigma, omega_op);
    //for (w = 0; w < n_omega; w++) SD[w] = S_omega_eff(w*d_omega, eta, Omega, y_0); //Effective SD for GOA model with ohmic bath at limit of omega_c -> infty
    
    outfile.open("SpectralDensity.dat");
    for (i=0; i< n_omega; i++) outfile << SD[i] << endl;
    outfile.close();
    outfile.clear();
    
    //calculate Er from SD[w]
    double *integrand = new double [n_omega];
    for (w = 0; w < n_omega; w++) integrand[w] = SD[w] * w *d_omega;
    integrand[0] = 0;
    Er = Integrate(integrand, n_omega, d_omega);
    cout << "Er = " << Er << endl;
    
    /*
    //calculate Er from J(omega)
    double Er_J(0);
    for (w = 1; w < n_omega; w++) integrand[w] = SD[w] / (w * d_omega);
    integrand[0]=0;
    Er_J = Integrate(integrand, n_omega, d_omega) *4.0/pi;
    cout << "Er_J = " << Er_J << endl;
    
    //calculate Er from discrete c_bath[n_omega]
    double Er_bath(0);
    double c_bath[n_omega];
    for (w = 1; w < n_omega; w++) {
        c_bath[w] = sqrt( 2.0 / pi * SD[w] * d_omega * d_omega * w);
        Er_bath += 2.0 * c_bath[w] * c_bath[w] / (w*d_omega * w*d_omega);
    }
    cout << "Er_bath = " << Er_bath << endl;
    
    double Er_req = 0;
    //calculate Er from discrete req_bath[n_omega]
    double req_bath[n_omega];
    for (w = 1; w < n_omega; w++) {
        //req_bath[w] = 2*c_bath[w] / (w*d_omega * w*d_omega);//from c_bath from J(omega)
        //req_bath[w] = sqrt(2.0 * SD[w] / w);//from S(omega)
        req_bath[w] = sqrt(8.0 * d_omega * SD[w] / pi / pow(w*d_omega, 3));
        Er_req += 0.5 * req_bath[w] * req_bath[w] * (w*d_omega * w*d_omega);
    }
    cout << "Er_req = " << Er_req << endl;
    */
    

    for (w = 1; w < n_omega; w++) integrand[w] = SD[w] * w*d_omega * w*d_omega /tanh(beta*hbar* w * d_omega*0.5);
    integrand[0]=0;
    a_parameter = 0.5 * Integrate(integrand, n_omega, d_omega);
    cout << "a_parameter = " << a_parameter << endl;
    
    
    double shift = T0 / DeltaT;
    double N = nn;
    cout << "shift = " << shift << endl;
    
    /*
    //testing numerical integral of omega with analytical high- and low-temperature limits
    ofstream outfile_a_re; //analytical
    ofstream outfile_a_im;
    ofstream outfile_n_re; //numerial
    ofstream outfile_n_im;
    
    //high-temperature
    outfile_a_re.open("Iw_HT_a_re.dat");
    outfile_a_im.open("Iw_HT_a_im.dat");
    outfile_n_re.open("Iw_HT_n_re.dat");
    outfile_n_im.open("Iw_HT_n_im.dat");
    
    beta = 0.00001;
    cout << "High-temp, beta = " << beta << endl;
    
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0] =0;
        integ_im[0] =0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_LSC(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        outfile_n_re << integral_re << endl;
        outfile_n_im << integral_im << endl;
        
        outfile_a_re << eta * 2 /beta/hbar *(1-1/(1+t*t)) << endl;
        outfile_a_im << eta * 2 * t / ((1+t*t)*(1+t*t)) << endl;

    }
    outfile_a_re.close();
    outfile_a_re.clear();
    outfile_a_im.close();
    outfile_a_im.clear();
    outfile_n_re.close();
    outfile_n_re.clear();
    outfile_n_im.close();
    outfile_n_im.clear();
    
    
    //low-temperature
    outfile_a_re.open("Iw_LT_a_re.dat");
    outfile_a_im.open("Iw_LT_a_im.dat");
    outfile_n_re.open("Iw_LT_n_re.dat");
    outfile_n_im.open("Iw_LT_n_im.dat");
    
    beta = 10000;
    cout << "Low-temp, beta = " << beta << endl;
    
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0] =0;
        integ_im[0] =0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_LSC(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        outfile_n_re << integral_re << endl;
        outfile_n_im << integral_im << endl;
        
        outfile_a_re << eta * t*t *(3+t*t)/((1+t*t)*(1+t*t)) << endl;
        outfile_a_im << eta * 2 * t / ((1+t*t)*(1+t*t)) << endl;
        
    }
    outfile_a_re.close();
    outfile_a_re.clear();
    outfile_a_im.close();
    outfile_a_im.clear();
    outfile_n_re.close();
    outfile_n_re.clear();
    outfile_n_im.close();
    outfile_n_im.clear();
    */
    
    
    
    
    //min-to-min energy Fourier frequency
    
    //LSC approximation
    for (i = 0; i < nn; i++) corr1[i] = corr2[i] = 0; //zero padding
    for (i = 0; i < LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0] = 0;
        integ_im[0] = 0;
        for (w = 0; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_LSC(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }

    outfile.open("LSC_t_re.dat");
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("LSC_t_im.dat");
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(-1, mm, corr1, corr2);//notice its inverse FT

    for(i=0; i<nn; i++) { //shift time origin 
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(-2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(-2*pi*i*shift/N);
    }
 
    
    outfile.open("LSC_re.dat");
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    
    outfile.close();
    outfile.clear();
    
    outfile.open("LSC_im.dat");
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    
    
    //LSC inhomogeneous limit
    for (i = 0; i < nn; i++) corr1[i] = corr2[i] = 0;
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0]=integ_im[0]=0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_LSC_inh(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }
    
    outfile.open("inh_t_re.dat");
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("inh_t_im.dat");
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(-1, mm, corr1, corr2);
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(-2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(-2*pi*i*shift/N);
    }
    
    outfile.open("inh_re.dat");
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("inh_im.dat");
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();

    //Classical sampling with average potential dynamics
    for (i = 0; i < nn; i++) corr1[i] = corr2[i] = 0;
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0]=integ_im[0]=0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_CL_avg(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }
    
    outfile.open("CL_avg_t_re.dat");
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("CL_avg_t_im.dat");
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(-1, mm, corr1, corr2);
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(-2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(-2*pi*i*shift/N);
    }
    
    outfile.open("CL_avg_re.dat");
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("CL_avg_im.dat");
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    
    //Classical sampling with donor potential dynamics (freq shifting)
    for (i = 0; i < nn; i++) corr1[i] = corr2[i] = 0;
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0]=integ_im[0]=0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_CL_donor(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        //integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) ;
        corr2[i] = 0;
        //corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        //corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }
    
    outfile.open("CL_donor_t_re.dat");
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("CL_donor_t_im.dat");
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(-1, mm, corr1, corr2);
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(-2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(-2*pi*i*shift/N);
    }
    
    //shift freq -omega_0=-Er
    int shift_f(0);
    shift_f = static_cast<int> (Er/(1.0/LEN/DeltaT)/(2*pi)+0.5);
    
    outfile.open("CL_donor_re.dat");
    for (i=nn-shift_f; i<nn; i++) outfile << corr1_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    for (i=0; i<nn-shift_f; i++) outfile << corr1_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("CL_donor_im.dat");
    for (i=nn-shift_f; i<nn; i++) outfile << corr2_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    for (i=0; i<nn-shift_f; i++) outfile << corr2_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    
    /*
    //Classical sampling with donor potential dynamics (no freq shifting)
    for (i = 0; i < nn; i++) corr1[i] = corr2[i] = 0;
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0]=integ_im[0]=0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_CL_donor(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }
    
    outfile.open("CL_donor_t_re.dat");
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("CL_donor_t_im.dat");
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(-1, mm, corr1, corr2);
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(-2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(-2*pi*i*shift/N);
    }
    
    outfile.open("CL_donor_re.dat");
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("CL_donor_im.dat");
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    */
    
    /*
    //second order cumulant limit
    
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        for (w = 0; w < n_omega; w++) {
            omega = (w+1) * d_omega;
            Integrand_2cumu(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }
    
    FFT(1, mm, corr1, corr2);
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(2*pi*i*shift/N);
    }
    
    outfile.open("2cumu_re.dat");
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("2cumu_im.dat");
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
     */
    
    //second order cumulant, inhomogeneous limit (Marcus)
    for (i = 0; i < nn; i++) corr1[i] = corr2[i] = 0;
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0]=integ_im[0]=0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_2cumu_inh(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }
    
    outfile.open("2cumu_inh_t_re.dat");
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("2cumu_inh_t_im.dat");
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(-1, mm, corr1, corr2);
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(-2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(-2*pi*i*shift/N);
    }
    
    outfile.open("2cumu_inh_re.dat");
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("2cumu_inh_im.dat");
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i]*LEN*DeltaT*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    /*
    //vertical energy Fourier frequency
    
    //LSC approximation
    
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0] =0;
        integ_im[0] =0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_LSC_v(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }
    
    outfile.open("v_LSC_t_re.dat");
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("v_LSC_t_im.dat");
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(1, mm, corr1, corr2);
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(2*pi*i*shift/N);
    }
    
    
    outfile.open("v_LSC_re.dat");
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i] << endl;
    
    outfile.close();
    outfile.clear();
    
    outfile.open("v_LSC_im.dat");
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i] << endl;
    outfile.close();
    outfile.clear();
    
    
    
    //LSC inhomogeneous limit (equal to exact quantum)
    
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0]=integ_im[0]=0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_LSC_inh_v(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }
    
    outfile.open("v_inh_t_re.dat");
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("v_inh_t_im.dat");
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(1, mm, corr1, corr2);
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(2*pi*i*shift/N);
    }
    
    outfile.open("v_inh_re.dat");
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("v_inh_im.dat");
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i] << endl;
    outfile.close();
    outfile.clear();
    
    //Classical sampling with average potential dynamics
    
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0]=integ_im[0]=0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_CL_avg_v(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }
    
    outfile.open("v_CL_avg_t_re.dat");
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("v_CL_avg_t_im.dat");
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(1, mm, corr1, corr2);
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(2*pi*i*shift/N);
    }
    
    outfile.open("v_CL_avg_re.dat");
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("v_CL_avg_im.dat");
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i] << endl;
    outfile.close();
    outfile.clear();
    
    
    //Classical sampling with donor potential dynamics
    
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0]=integ_im[0]=0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_CL_donor_v(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }
    
    outfile.open("v_CL_donor_t_re.dat");
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("v_CL_donor_t_im.dat");
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(1, mm, corr1, corr2);
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(2*pi*i*shift/N);
    }
    
    outfile.open("v_CL_donor_re.dat");
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("v_CL_donor_im.dat");
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i] << endl;
    outfile.close();
    outfile.clear();
    
    
    //second order cumulant, inhomogeneous limit
    
    for (i=0; i< LEN; i++) {
        t = T0 + DeltaT * i;
        integ_re[0]=integ_im[0]=0;
        for (w = 1; w < n_omega; w++) {
            omega = w * d_omega;
            Integrand_2cumu_inh_v(omega, t, integ_re[w], integ_im[w]);
            integ_re[w] *= SD[w];
            integ_im[w] *= SD[w];
        }
        integral_re = Integrate(integ_re, n_omega, d_omega);
        integral_im = Integrate(integ_im, n_omega, d_omega);
        
        corr1[i] = exp(-1 * integral_re) * cos(integral_im);
        corr2[i] = -1 * exp(-1 * integral_re) * sin(integral_im);
    }
    
    outfile.open("v_2cumu_inh_t_re.dat");
    for (i=0; i< LEN; i++) outfile << corr1[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("v_2cumu_inh_t_im.dat");
    for (i=0; i< LEN; i++) outfile << corr2[i] << endl;
    outfile.close();
    outfile.clear();
    
    FFT(1, mm, corr1, corr2);
    
    for(i=0; i<nn; i++) { //shift time origin
        corr1_orig[i] = corr1[i] * cos(2*pi*i*shift/N) - corr2[i] * sin(2*pi*i*shift/N);
        corr2_orig[i] = corr2[i] * cos(2*pi*i*shift/N) + corr1[i] * sin(2*pi*i*shift/N);
    }
    
    outfile.open("v_2cumu_inh_re.dat");
    for (i=0; i<nn/2; i++) outfile << corr1_orig[i] << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("v_2cumu_inh_im.dat");
    for (i=0; i<nn/2; i++) outfile << corr2_orig[i] << endl;
    outfile.close();
    outfile.clear();
    
    */
    
    
    
    
    cout << "DeltaT = " << DeltaT << endl;
    cout << "N = " << LEN << endl;
    cout << "df = " << 1.0/LEN/DeltaT << endl;
    cout << "f_max = " << 0.5/DeltaT << endl;
    cout << "beta = " << beta << endl;
    cout << " eta = " << eta << endl;
    
    double df= 1.0/LEN/DeltaT;
    double dE = df * 2 * pi;
    outfile.open("marcus-levich.dat");
    for (i=0; i<nn/2; i++) outfile << sqrt(pi/a_parameter) * exp(-(dE*i*hbar-Er)*(dE*i*hbar-Er)/(4 * hbar*a_parameter))*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    outfile.open("marcus.dat");
    for (i=0; i<nn/2; i++) outfile << sqrt(beta*pi/Er) * exp(-beta*(dE*i*hbar-Er)*(dE*i*hbar-Er)/(4 * Er))*DAcoupling*DAcoupling << endl;
    outfile.close();
    outfile.clear();
    
    cout << "Done." << endl;





    return 0;
}



/********* SUBROUTINE *************/


//spectral densities

double S_omega_ohmic(double omega, double eta) {
    return eta * omega * exp(-1 * omega);
}

double S_omega_drude(double omega, double eta) {
    return eta * omega /(1 + omega*omega);
}

double S_omega_gaussian(double omega, double eta, double sigma, double omega_op) {
    return   0.5 / hbar * eta * omega * exp(-(omega - omega_op)*(omega - omega_op)/(2*sigma*sigma))/RT_2PI/sigma;
}

double S_omega_eff(double omega, double eta, double Omega, double y_0) {
    return  4.0 / (pi * omega) * eta * pow(Omega,4) * y_0 * y_0 / ( pow(Omega*Omega - omega*omega, 2) + eta*eta*omega*omega);
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

//vertical energy as Fourier transform frequency

void Integrand_LSC_v(double omega, double t, double &re, double &im) {
    re = (1-cos(omega*t))/tanh(beta*hbar*omega/2);
    im = sin(omega*t)-omega*t;
    return;
}


void Integrand_LSC_inh_v(double omega, double t, double &re, double &im) {
    re = omega*omega*t*t/2/tanh(beta*hbar*omega/2);
    im = 0;
    return;
}

void Integrand_CL_avg_v(double omega, double t, double &re, double &im) {
    re = (1-cos(omega*t))*2/(beta*hbar*omega);
    im = sin(omega*t)-omega*t;
    return;
}

void Integrand_CL_donor_v(double omega, double t, double &re, double &im) {
    re = (1-cos(omega*t))*2/(beta*hbar*omega);
    im = 0;
    return;
}

void Integrand_2cumu_v(double omega, double t, double &re, double &im) {
    re = (1-cos(omega*t))*2/(beta*hbar*omega);
    im = 0;
    return;
}

void Integrand_2cumu_inh_v(double omega, double t, double &re, double &im) {
    re = omega*t*t/(beta*hbar);
    im = 0;
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





