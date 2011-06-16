
// project headers
#include "gsr_analyzer.h"
#include "dht_analyzer.h"
#include "mva_analyzer.h"
#include "gsr_curve.h"
#include "gnuplot.h"
#include "gsl_fit_poly.h"
#include "gsl_fit_epe.h"
#include "gsl_fit_exp.h"
#include "differentiator.h"
// library headers
#include <eigen3/Eigen/Dense>
#include <gsl/gsl_const_mksa.h>
#include <gsl/gsl_sort_double.h>
#include <gsl/gsl_permute.h>
#include <gsl/gsl_fit.h>
// standard headers
#include <iostream>
#include <fstream>

using namespace std;
using namespace Eigen;

// main trigger to perform GSR analysis of the event
void GsrAnalyzer::analyze(Event& event) {
  DhtAnalyzer dht; // initialize dHT analyzer
  MvaAnalyzer mva; // initialize MVA analyzer
  GsrResults  gsr; // structure for GSR results

  dht.analyze(event); // carry dHT analysis for the event

  mva.analyzePmvab(event); // carry projected MVA anaysis to get initial axes

  // make a run of axes searching algorithm, save the results in the run
  // structure
  GsrRun run = loopAxes(event, 0, 1, 90, 0, 1, 360);
  // quaternions, needed to turn to optimized axes
  Quaterniond qTheta;
  Quaterniond qPhi;

  qTheta = AngleAxisd(run.optTheta*M_PI/180, event.pmvab().axes.y);
  qPhi = AngleAxisd(run.optPhi*M_PI/180, event.pmvab().axes.z);
  run.axes.z = (qPhi*(qTheta*event.pmvab().axes.z)).normalized();
  run.axes.x = (event.dht().Vht.dot(run.axes.z)*run.axes.z-
                event.dht().Vht).normalized();
  run.axes.y = run.axes.z.cross(run.axes.x).normalized();
  run.curve = GsrCurve(event, run.axes);
  run.curve.initBranches("extremums").computeResidue();

  computeMap(event, run);
  gsr.runs.push_back(run);

  cout << gsr.runs[0].optTheta << ' ' << gsr.runs[0].optPhi << endl;

  ofstream myfile;

  myfile.open ("./rm_original.dat");
  myfile << gsr.runs[0].originalResidue << endl;
  myfile.close();

  myfile.open ("./rm_combined.dat");
  myfile << gsr.runs[0].combinedResidue << endl;
  myfile.close();
}

// run one loop of axes search algorithm
GsrRun GsrAnalyzer::loopAxes(Event& event,
                             double minTheta, double dTheta, double maxTheta,
                             double minPhi,   double dPhi,   double maxPhi)
{
  double theta, phi; // temporary theta and phi angles
  Axes axes; // temporary coordinate system
  GsrRun run; // holds the information about the current run

  // save axes limits in run structure
  run.minTheta = minTheta;
  run.dTheta   = dTheta;
  run.maxTheta = maxTheta;
  run.minPhi   = minPhi;
  run.dPhi     = dPhi;
  run.maxPhi   = maxPhi;

  // define residue matrix size
  run.originalResidue = MatrixXd::Zero(int((maxTheta-minTheta)/dTheta)+1,
                                       int((maxPhi-minPhi)/dPhi)+1);
  run.combinedResidue = MatrixXd::Zero(int((maxTheta-minTheta)/dTheta)+1,
                                       int((maxPhi-minPhi)/dPhi)+1);
  run.branchLength    = MatrixXd::Zero(int((maxTheta-minTheta)/dTheta)+1,
                                       int((maxPhi-minPhi)/dPhi)+1);

  // temporary quaternions for making axes rotations
  Quaterniond qTheta;
  Quaterniond qPhi;

  int i, k; // angle counters

  GsrCurve curve;
  Vector3d zTheta;

  i = 0; // starting from 1st row
  theta = minTheta; // initialize theta
  while (theta <= maxTheta) { // begin iteration through theta angles
    // initialize theta quaternion
    qTheta = AngleAxisd(theta*M_PI/180, event.pmvab().axes.y);
    zTheta = qTheta*event.pmvab().axes.z; // rotate z axis around PMVA y axis
    phi = minPhi; // initialize phi
    k = 0; // starting from 1st column
    while (phi <= maxPhi) { // begin iteration through phi angles
      // initialize phi quaternion
      qPhi = AngleAxisd(phi*M_PI/180, event.pmvab().axes.z);
      axes.z = (qPhi*zTheta).normalized(); // rotate z axis around PMVA z axis
      // initialize x axis
      axes.x = (event.dht().Vht.dot(axes.z)*axes.z-
                event.dht().Vht).normalized();
      // complement with y axis
      axes.y = axes.z.cross(axes.x).normalized();
      // create Pt(A) curve in new axes
      curve = GsrCurve(event, axes);
      // initialize branches and compute the residue
      curve.initBranches("extremums").computeResidue();
      // save residue into a matrix
      run.originalResidue(i,k) = curve.originalResidue();
      run.combinedResidue(i,k) = curve.combinedResidue();
      run.branchLength(i,k) = curve.branchLength();
      phi += dPhi; // make a step in phi
      k++; // move to the next column
    } // end iteration through phi angles
    theta += dTheta; // make a step in theta step
    i++; // move to the next row
  } // end iteration through theta angles

  // searching for the minimum residue direction
  int iTheta, iPhi; // index of optimal angles
  // get indices
//  run.originalResidue.minCoeff(&iTheta, &iPhi);
  run.combinedResidue.minCoeff(&iTheta, &iPhi);
  // translate indices into optimal angles
  run.optTheta = minTheta + iTheta*dTheta;
  run.optPhi = minPhi + iPhi*dPhi;

  return run; // return
}

// compute magnetic field map for a given GSR run
GsrRun& GsrAnalyzer::computeMap(Event& event, GsrRun& run) {
  // copy the data into external Dara object, we need a copy because we
  // will have to project it onto new coordinate system
  Data data(event.dataNarrow());

  // project the data into optimized coordinate system
  data.project(run.axes);

  // compute the dx step
  double dx = -event.dht().Vht.dot(run.axes.x)*event.config().samplingInterval*
               run.curve.size()/event.config().Nx;
  // first estimate for the dy step
  double dy = event.config().ratio*dx;
  // number of Y-nodes
  int Ny = floor((event.config().maxY-event.config().minY)*
           GSL_CONST_MKSA_ASTRONOMICAL_UNIT/dy);
  // final value for the Y step
  dy = (event.config().maxY-event.config().minY)*
       GSL_CONST_MKSA_ASTRONOMICAL_UNIT/Ny;
  // number of steps into positive Y direction
  int NyUp = ceil(event.config().maxY*GSL_CONST_MKSA_ASTRONOMICAL_UNIT/dy);
  // number of steps into negative Y direction
  int NyDown = -floor(event.config().minY*GSL_CONST_MKSA_ASTRONOMICAL_UNIT/dy);
  // final number of Y-nodes
  Ny = NyDown + 1 + NyUp;
  // initialize the vector of Y nodes
  VectorXd Y = VectorXd::Zero(Ny);
  // and fill it with actual Y values
  for (int i = -NyDown; i <= NyUp; i++) {
    Y(NyDown+i) = i*dy;
  }
  // number of X nodes
  int Nx = event.config().Nx;

  // copy branches of Pt(A) into separate Curve objects, we need the copies
  // because we will need to resample them
  Curve APtIn(run.curve.branches()[0]);
  Curve APtOut(run.curve.branches()[1]);

//  Gnuplot gp1("gnuplot -persist");
//	gp1 << "p '-' w p t 'PtIn(A)', '-' w p t 'PtOut(A)'\n";
//  gp1.send(curveIn).send(curveOut);

  // resample the branches curves to the new lattice
  APtIn.resample(Nx);
  APtOut.resample(Nx);

//  Gnuplot gp2("gnuplot -persist");
//	gp2 << "p '-' w p t 'PtIn(A)', '-' w p t 'PtOut(A)'\n";
//  gp2.send(curveIn).send(curveOut);

//  Gnuplot gp3("gnuplot -persist");
//  gp3 << "p '-' w l t 'Pt(A)'\n";
//  gp3.send((Curve)run.curve);

  // initialize arrays for the branches

  double *AarrAll  = new double[2*Nx],
         *PtArrAll = new double[2*Nx];
  // fill the arrays with values of the both branches
  for (int i = 0; i < 2*Nx; i++) {
    if (i < Nx) { // get values from the inward branch
      AarrAll[i] = APtIn.cols().x(i);
      PtArrAll[i] = APtIn.cols().y(i);
    } else { // get values from the outward branch
      AarrAll[i] = APtOut.cols().x(i-Nx);
      PtArrAll[i] = APtOut.cols().y(i-Nx);
    }
  }

  // map arrays into vectors
  Map<VectorXd> Aall(AarrAll, 2*Nx);
  Map<VectorXd> PtAll(PtArrAll, 2*Nx);

  // build a Pt(A) curve from the data vectors
  Curve APtAll(Aall, PtAll);

  // sort and unique the curve
  APtAll.sort().unique();

  int nAll = APtAll.size(); // length of the curve

  // change size of the data arrays
  AarrAll = (double*) realloc(AarrAll, nAll*sizeof(double));
  PtArrAll = (double*) realloc(PtArrAll, nAll*sizeof(double));
  // reinitialize the data arrays
  AarrAll  = const_cast<double*>(APtAll.cols().x.data());
  PtArrAll = const_cast<double*>(APtAll.cols().y.data());

  // find the slope of the curve
  double lCoeff[2]; // linear coefficients array
  // do the fitting
  gsl_fit_poly(nAll, 1, AarrAll, PtArrAll, lCoeff);
  // initialize the slope, TODO: sign function
  int slope = (lCoeff[1] > 0 ? 1 : -1);

  double pCoeff[event.config().order+1]; // polynomial coefficients
  // do the fitting
  gsl_fit_poly(nAll, event.config().order, AarrAll, PtArrAll, pCoeff);

  // compute 1st derivative of the polynomial
  double dpCoeff[event.config().order];
  for (int i = 0; i <= event.config().order-1; i++) {
    dpCoeff[i] = pCoeff[i+1]*(i+1);
  }

  // compute 2nd derivative of the polynomial
  double ddpCoeff[event.config().order-1];
  for (int i = 0; i <= event.config().order-2; i++) {
    ddpCoeff[i] = dpCoeff[i+1]*(i+1);
  }

  double Ab, Ac; // boundary and center value of A
  VectorXd Atmp; // wide range A, just for plotting
  int AbIndex; // index of the
  if (slope > 0) { // Ab < Ac
    Ab = APtAll.cols().x.minCoeff(&AbIndex);
    Ac = APtAll.cols().x.maxCoeff();
    Atmp = VectorXd::LinSpaced(1000, APtAll.cols().x(0)-150,
                                     APtAll.cols().x(nAll-1)+5);
  } else { // Ac < Ab
    Ab = APtAll.cols().x.maxCoeff(&AbIndex);
    Ac = APtAll.cols().x.minCoeff();
    Atmp = VectorXd::LinSpaced(1000, APtAll.cols().x(0)-5,
                                     APtAll.cols().x(nAll-1)+150);
  }

  // do not allow the Ab to pass the zero derivative point of the polynomial
  while (slope > 0 && gsl_fit_poly_eval(Ab, dpCoeff,
                                        event.config().order-1) < 0 ||
         slope < 0 && gsl_fit_poly_eval(Ab, dpCoeff,
                                        event.config().order-1) > 0)
  {
    if (slope > 0) {
      Ab = APtAll.cols().x(++AbIndex);
    } else {
      Ab = APtAll.cols().x(--AbIndex);
    }
  }

  /*
  // estimate the coefficients of the exponent in the boundary part of Pt(A)
  double e2 = gsl_fit_poly_eval(xb, dpCoeff, event.config().order-1)/
              gsl_fit_poly_eval(xb, pCoeff, event.config().order);
  double e1 = gsl_fit_poly_eval(xb, pCoeff, event.config().order)/
              exp(e2*xb);
  double eCoeff[] = {e1, e2};
  // estimate the coefficients of the exponent in the central part of Pt(A)
  double E2 = gsl_fit_poly_eval(xc, ddpCoeff, event.config().order-2)/
              gsl_fit_poly_eval(xc, dpCoeff, event.config().order-1);
  double E1 = gsl_fit_poly_eval(xc, dpCoeff, event.config().order-1)/
              E2/exp(E2*xc);
  double E3 = gsl_fit_poly_eval(xc, pCoeff, event.config().order)-
              E1*exp(E2*xc);
  double ECoeff[] = {E1, E2, E3};
//  double coeff0[] = {1, -slope, 1, slope};
//  double coeff[4];
  */

  double coeff[3]; // fitting coefficients for exponent

//  gsl_fit_epe(nAll, xArrAll, yArrAll, xc, xb, pCoeff, event.config().order,
//              eCoeff, ECoeff, coeff0, coeff);

  // fit the exponent to Pt(A)
  gsl_fit_exp(nAll, AarrAll, PtArrAll, coeff);

  // initialize temporary Pt vector, for plotting only
  VectorXd PtTmp = VectorXd::Zero(1000);
  // fill it with evaluated values for the fitted Pt(A)
  for (int i = 0; i < 1000; i++) {
//    PtTmp(i) = gsl_fit_epe_eval_f(xTmp[i], xc, xb, coeff, pCoeff,
//                                 event.config().order, eCoeff, ECoeff);
    PtTmp(i) = gsl_fit_exp_eval_f(Atmp[i], coeff);
  }

  // initialize temporary fitted curve, for plotting only
  Curve APtTmp(Atmp, PtTmp);

  Gnuplot gp5("gnuplot -persist");
  gp5 <<
    "p '-' w p t 'Pt(A) in', '-' w p t 'Pt(A) out', '-' w l t 'Pt(A) fit'\n";
  gp5.send(APtIn).send(APtOut).send(APtTmp);

  // initialize temporary dPt/dA vector, for plotting only
  VectorXd dPtTmp = VectorXd::Zero(1000);
  // fill it with derivatives
  for (int i = 0; i < 1000; i++) {
//    dPtTmp(i) = gsl_fit_epe_eval_df(xTmp[i], xc, xb, coeff, pCoeff, dpCoeff,
//                                   event.config().order, eCoeff, ECoeff);
    dPtTmp(i) = gsl_fit_exp_eval_df(Atmp[i], coeff);
  }

  // initialize temporary derivative of the fitted curve, for plotting only
  Curve AdPtTmp(Atmp, dPtTmp);

  Gnuplot gp6("gnuplot -persist");
  gp6 << "p '-' w l t 'dPt/dA'\n";
  gp6.send(AdPtTmp);

  // initialize and fill in the X coordinates vector (on a lattice)
  VectorXd X = VectorXd::Zero(Nx);
  for (int i = 0; i < Nx; i++) {
    X(i) = i*dx;
  }

  // copy A, B and Pth to new vector objects, needed for resampling
  VectorXd A(run.curve.cols().x);
  VectorXd Bx(data.cols().Bx);
  VectorXd By(data.cols().By);
  VectorXd Bz(data.cols().Bz);
  VectorXd Pth(data.cols().Pth);

  // resample physical parameters to use on a lattice
  Curve::resample(A, Nx);
  Curve::resample(Bx, Nx);
  Curve::resample(By, Nx);
  Curve::resample(Bz, Nx);
  Curve::resample(Pth, Nx);

  // initialize matrices for A, Bx and Bz maps
  MatrixXd Axy = MatrixXd::Zero(Ny, Nx),
           Bxy = MatrixXd::Zero(Ny, Nx),
           Bzy = MatrixXd::Zero(Ny, Nx);

  // fill initialize A and Bx values to the matrices, i.e. measured spacecraft
  // data
  Axy.row(NyDown) = A;
  Bxy.row(NyDown) = Bx;

  // initialize vectors for some important derivatives
  VectorXd d2A_dx2 = VectorXd::Zero(Nx),
           dPt_dA  = VectorXd::Zero(Nx),
           d2A_dy2 = VectorXd::Zero(Nx);

  // initialize differentiator
  Differentitor differentiator;

  cout << NyUp << " steps up and " << NyDown << " steps down with step size " << dy << endl;

  // reconstruct the upper part of the map using recursive solver
  for (int i = 1; i <= NyUp; i++) {
    // 2nd derivative of A by x using Holoborodko2 filter, smoothed with
    // weighted average prior to differenting
    d2A_dx2 = differentiator.Holoborodko2(7,
      Curve::weightedAverage(Axy.row(NyDown+i-1), 1-(abs(i)-1)/Ny/3), dx);
    // evaluate the 1st derivative of Pt by A using fitting curve
    for (int k = 0; k < Nx; k++) {
//      dPt_dA(k) = gsl_fit_epe_eval_df(Axy(NyDown+i-1, k), xc, xb, coeff,
//        pCoeff, dpCoeff, event.config().order, eCoeff, ECoeff);
      dPt_dA(k) = gsl_fit_exp_eval_df(Axy(NyDown+i-1, k), coeff);
    }
    // compute 2nd derivative of A by y using Grad-Shafranov equation
    d2A_dy2 = -d2A_dx2-GSL_CONST_MKSA_VACUUM_PERMEABILITY*dPt_dA;
    // write the next row of A, GS equation used
    Axy.row(NyDown+i) = Axy.row(NyDown+i-1)+Bxy.row(NyDown+i-1)*dy+
                        d2A_dy2.transpose()*pow(dy, 2)/2;
    // smooth it with weighted average
    Axy.row(NyDown+i) = Curve::weightedAverage(Axy.row(NyDown+i),
                                               1-abs(i)/Ny/3);
    // wtite the next row of Bx
    Bxy.row(NyDown+i) = Bxy.row(NyDown+i-1)+d2A_dy2.transpose()*dy;
    // smooth it with weighted average
    Bxy.row(NyDown+i) = Curve::weightedAverage(Bxy.row(NyDown+i),
                                               1-abs(i)/Ny/3);
  }

  // reconstruct the lower part of teh map using recursive solver,
  // everything is the same as for the upper part
  for (int i = -1; i >= -NyDown; i--) {
    d2A_dx2 = differentiator.Holoborodko2(7,
      Curve::weightedAverage(Axy.row(NyDown+i+1), 1-(abs(i)-1)/Ny/3), dx);
    for (int k = 0; k < Nx; k++) {
//      dPt_dA(k) = gsl_fit_epe_eval_df(Axy(NyDown+i+1, k), xc, xb, coeff,
//        pCoeff, dpCoeff, event.config().order, eCoeff, ECoeff);
      dPt_dA(k) = gsl_fit_exp_eval_df(Axy(NyDown+i+1, k), coeff);
    }
    d2A_dy2 = -d2A_dx2-GSL_CONST_MKSA_VACUUM_PERMEABILITY*dPt_dA;
    Axy.row(NyDown+i) = Axy.row(NyDown+i+1)-Bxy.row(NyDown+i+1)*dy+
                        d2A_dy2.transpose()*pow(dy, 2)/2;
    Axy.row(NyDown+i) = Curve::weightedAverage(Axy.row(NyDown+i),
                                               1-abs(i)/Ny/3);
    Bxy.row(NyDown+i) = Bxy.row(NyDown+i+1)-d2A_dy2.transpose()*dy;
    Bxy.row(NyDown+i) = Curve::weightedAverage(Bxy.row(NyDown+i),
                                               1-abs(i)/Ny/3);
  }

  // initialize the Bz(A) curve
  Curve ABz(A, Bz);

  // fit it with exponent
  double e[3]; // exponent fitting coefficients
  gsl_fit_exp(Nx, A.data(), Bz.data(), e); // fit it
  VectorXd BzFit = VectorXd::Zero(Nx); // vector of fitted Bz values
  // fill it by evaluating the fitting function
  for (int i = 0; i < Nx; i++) {
    BzFit(i) = gsl_fit_exp_eval_f(A(i), e);
  }
  // initialize fitted Bz(A)
  Curve ABzFit(A, BzFit);
  // sort and unique it
  ABzFit.sort().unique();

  Gnuplot gp7("gnuplot -persist");
  gp7 << "p '-' w p t 'Bz(A)', '-' w l t 'Bz(A) fitted'\n";
  gp7.send(ABz).send(ABzFit);

  // calculate the Bz map using the fitted Bz(A)
  for (int i = -NyDown; i <= NyUp; i++) {
    for (int k = 0; k < Nx; k++) {
      Bzy(NyDown+i, k) = gsl_fit_exp_eval_f(Axy(NyDown+i, k), e);
    }
  }

  ofstream myfile;
  myfile.open ("./map.dat");
  myfile << Bzy << endl;
  myfile.close();

  myfile.open ("./a.dat");
  myfile << Axy << endl;
  myfile.close();

  // free dynamic arrays
//  delete [] AarrAll;
//  delete [] PtArrAll;
}

