
#ifndef INTEGRATOR_H
#define INTEGRATOR_H

// library headers
#include <eigen3/Eigen/Dense>
// standard headers
#include <map>

class Integrator {
    std::map<int, Eigen::VectorXd> _NewtonCotesCoefficients;
    std::map<int, Eigen::VectorXd> _HoloborodkoCoefficients;
  public:
    Integrator();
    double NewtonCotes(int, const Eigen::VectorXd&, const Eigen::VectorXd&);
    double Holoborodko(int, const Eigen::VectorXd&, const Eigen::VectorXd&);
};

#endif

