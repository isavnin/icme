
// project headers
#include "config.h"
#include "data.h"
#include "event.h"
#include "mva_analyzer.h"
#include "gsr_analyzer.h"
#include "gnuplot.h"
#include "plotter.h"
// library headers
#include "engine.h"
#include <eigen3/Eigen/Dense>
// standard headers
#include <string>
#include <iostream>
#include <sstream>

using namespace std;
using namespace Eigen;
using namespace My;

// this is the main driver function of the whole project
int main(int argc, char* argv[]) {

  // parse command line arguments
  const char eventQuality = (argc > 1 ? *argv[1] : 'g'); // event quality
  // path to config file
  const string configPath = (argc > 2 ? argv[2] : "./res/config");
  // path to directory with data files
  const string dataDir = (argc > 3 ? argv[3] : "./res");

  Config config; // config object, holds config for all events
  ostringstream dataPathStream; // srting stream for data path string
  string dataPath; // path to data file

  // initialize analyzers
  MvaAnalyzer mva; // MVA analysis class
  GsrAnalyzer gsr; // GSR analysis class

  // read the config data from the file to the config object and then
  // filter it
  config.readFile(configPath).filter(eventQuality);

  // start iterating through the events that should be analyzed
  for (int iEvent = 0; iEvent < config.rows().size(); iEvent++) {
    // determine the path to the data file dependant on spacecraft
    // push the path to data files
    dataPathStream << dataDir << '/';
    if (config.row(iEvent).spacecraft == "WIND") {
      dataPathStream << "wind_";
    } else if (config.row(iEvent).spacecraft == "ACE") {
      dataPathStream << "ace_";
    } else if (config.row(iEvent).spacecraft == "STA") {
      dataPathStream << "stereo_a_";
    } else if (config.row(iEvent).spacecraft == "STB") {
      dataPathStream << "stereo_b_";
    } else { // throw an error - unknown spacecraft
      cout << "Unknown spacecraft" << endl;
    }
    // finalize by adding resolution
    dataPathStream << config.row(iEvent).samplingInterval << ".dat";
    // save the data path
    dataPath = dataPathStream.str();
    // clear the stream
    dataPathStream.clear();

    // create Time objects for wider limits of the event, initially copy
    // existing time limits
    Time* preBeginTime = new Time(config.row(iEvent).beginTime);
    Time* postEndTime = new Time(config.row(iEvent).endTime);
    // widen time limits
    preBeginTime->add(-3, "hour");
    postEndTime->add(3, "hour");
    // create Data object for wider time limits
    Data* dataWide = new Data(); // create dynamic object for data
    dataWide->readFile(dataPath, *preBeginTime, *postEndTime);
    // create Data object for original time limits
    Data* dataNarrow = new Data(*dataWide);
    // filter narrow Data object out of wider one
    dataNarrow->filter(config.row(iEvent).beginTime,
                       config.row(iEvent).endTime);
    // create dynamic object to store all event data and results of analysis
    Event* event = new Event(config.row(iEvent), *dataWide, *dataNarrow);

    // perform GSR analysis if required
    if (config.row(iEvent).toGsr) {

      gsr.analyze(*event);

      if (config.row(iEvent).toPlot) { // plot

        // plot residue maps through Matlab
        Plotter plotter;
        /*
        VectorXd phi, theta;
        for (int i = 0; i < (*event).gsr().runs.size(); i++) {

          theta = VectorXd::LinSpaced(
            ((*event).gsr().runs[i].maxTheta-(*event).gsr().runs[i].minTheta)/
              (*event).gsr().runs[i].dTheta+1,
            (*event).gsr().runs[i].minTheta,
            (*event).gsr().runs[i].maxTheta);

          phi = VectorXd::LinSpaced(
            ((*event).gsr().runs[i].maxPhi-(*event).gsr().runs[i].minPhi)/
              (*event).gsr().runs[i].dPhi+1,
            (*event).gsr().runs[i].minPhi,
            (*event).gsr().runs[i].maxPhi);

          plotter.plotResidueMap((*event).gsr().runs[i].originalResidue,
                                 theta, phi,
                                 (*event).gsr().runs[i].optTheta,
                                 (*event).gsr().runs[i].optPhi);

          plotter.plotResidueMap((*event).gsr().runs[i].combinedResidue,
                                 theta, phi,
                                 (*event).gsr().runs[i].optTheta,
                                 (*event).gsr().runs[i].optPhi);
        }
        */

        // plot Pt(A) through Gnuplot
        Gnuplot APt("gnuplot -persist");
        APt << "p '-' w p t 'Pt(A) in', "
              << "'-' w p t 'Pt(A) out', "
              << "'-' w l t 'Pt(A) fit'\n";
        APt.send((*event).gsr().runs.back().APtInCurve).
            send((*event).gsr().runs.back().APtOutCurve).
            send((*event).gsr().runs.back().APtFitCurve);

        // plot dPt/dA through Gnuplot
        Gnuplot AdPt("gnuplot -persist");
        AdPt << "p '-' w l t 'dPt/dA fit'\n";
        AdPt.send((*event).gsr().runs.back().AdPtFitCurve);

        // plot Bz(A) through Gnuplot
        Gnuplot ABz("gnuplot -persist");
        ABz << "p '-' w p t 'Bz(A)', '-' w l t 'Bz(A) fit'\n";
        ABz.send((*event).gsr().runs.back().ABzCurve).
            send((*event).gsr().runs.back().ABzFitCurve);

        // plot magnetic field map through Matlab

        plotter.plotMagneticMap((*event).gsr().runs.back().Axy,
                                (*event).gsr().runs.back().Bz,
                                (*event).gsr().runs.back().X,
                                (*event).gsr().runs.back().Y);
      }
    }

    // perform MVA analysis if required
    if (config.row(iEvent).toMva) mva.analyze(*event);

    // delete dynamic objects
    delete dataWide;
    delete dataNarrow;
    delete preBeginTime;
    delete postEndTime;
    delete event;
  } // end of iteration through the events
}

