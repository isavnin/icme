
#include "branched_curve.h"

#include <eigen3/Eigen/Dense>
#include <gsl/gsl_version.h>

#include <iostream>
#include <algorithm>
#include <limits>

using namespace std;
using namespace Eigen;

// initialize branches of the curve
BranchedCurve& BranchedCurve::initBranches() {

  // copy curve data vectors
  VectorXd x = _vectors.x;
  VectorXd y = _vectors.y;

  // smooth the curve data
  filterRunningAverage(x, 5);
  filterRunningAverage(y, 5);

  // let X be always with positive camber
  if (abs(x.minCoeff()) > abs(x.maxCoeff())) x = -x;

  // point of X maximum
  int maxIndexX;
  x.maxCoeff(&maxIndexX);

  // initial guess for branches limits
  _isBranched = true;
  _minLeftIndex = 0;
  _maxIndex = maxIndexX;
  _minRightIndex = x.size();

  // if X maximum is at the boundaries of X then no branches
  if (maxIndexX == 0 || maxIndexX == x.size()-1) {
    _isBranched = false;
  } else {
    // initialize minimums on both sides of X maximum
    int minLeftIndexX, minRightIndexX;
    double minLeftX = x.head(maxIndexX+1).minCoeff(&minLeftIndexX);
    double minRightX = x.tail(x.size()-maxIndexX).minCoeff(&minRightIndexX);
    minRightIndexX += maxIndexX;

    // align the minimums, so that they are equal Y-valued
    // if the left minimum is higher than the right one
    if (minLeftX >= minRightX) {
      // go from the maximum to the right minimum
      for (int i = maxIndexX; i < x.size(); i++) {
        if (x(i) < minLeftX) {
          minRightIndexX = i-1;
          break;
        }
      }
    } else { // if the right minimum is higher than the left one
      // go from the maximum to the left minimum
      for (int i = maxIndexX; i >= 0; i--) {
        if (x(i) < minRightX) {
          minLeftIndexX = i+1;
          break;
        }
      }
    }

    // save branches boundaries
    _minLeftIndex = minLeftIndexX;
    _minRightIndex = minRightIndexX;

//    cout << _minLeftIndex << ':' << _maxIndex << ':' << _minRightIndex << endl;
  }

  // initialize branch curves
  if (_isBranched) {
    initBranches(_minLeftIndex, _maxIndex, _minRightIndex);
  }

  return *this; // chained method
}

// initialize branches by the boudaries
BranchedCurve& BranchedCurve::initBranches(int minLeftIndex,
                                           int maxIndex,
                                           int minRightIndex)
{
  _branches.clear(); // clear branches
  // initialize left minimum to maximum branch
  _branches.push_back(
    Curve(_vectors.x.segment(minLeftIndex, maxIndex-minLeftIndex+1),
          _vectors.y.segment(minLeftIndex, maxIndex-minLeftIndex+1)));

  // initialize maximum to right minimum branch
  _branches.push_back(
    Curve(_vectors.x.segment(maxIndex, minRightIndex-maxIndex+1),
          _vectors.y.segment(maxIndex, minRightIndex-maxIndex+1)));

  return *this; // chained method
}

// compute residues of the curve branches
BranchedCurve& BranchedCurve::computeResidue() {

  // minimal length of the branch
  _branchLength = min(_maxIndex-_minLeftIndex+1, _minRightIndex-_maxIndex+1);

  // interpolation type used for resampling
  const gsl_interp_type* interp_type = gsl_interp_linear;

  /* if GSL_VERSION >= 1.15 */
  /* if (_isBranched && m >= gsl_interp_type_min_size(interp_type)) { */
  /* else */
  const int interp_min_size = 2;
  if (_isBranched && _branchLength >= interp_min_size && _branchLength > 0.3*size()) {
  /* endif */
    // copy the curve branches
    Curve curveIn(_branches[0]);
    Curve curveOut(_branches[1]);

    // init the limits for the resampled branches
    double minX = max(curveIn.cols().x.minCoeff(),
                      curveOut.cols().x.minCoeff());
    double maxX = min(curveIn.cols().x.maxCoeff(),
                      curveOut.cols().x.maxCoeff());

    // resample the branches
    curveIn.resample(minX, maxX, 2*size(), interp_type);
    curveOut.resample(minX, maxX, 2*size(), interp_type);

    // init the limits of the Y values of the curves
    double minY = min(curveIn.cols().y.minCoeff(),
                      curveOut.cols().y.minCoeff());
    double maxY = max(curveIn.cols().y.maxCoeff(),
                      curveOut.cols().y.maxCoeff());

    // compute residue
    _residue = (curveIn.cols().y-curveOut.cols().y).norm()/abs(maxY-minY);
    _combinedResidue = _residue*size()/2/_branchLength; // new normalization
  } else { // if the curve is not branched
    _residue = numeric_limits<double>::infinity();
    _combinedResidue = numeric_limits<double>::infinity();
  }
}

