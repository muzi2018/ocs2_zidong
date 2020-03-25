/******************************************************************************
Copyright (c) 2017, Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#include <gtest/gtest.h>
#include <cstdlib>
#include <ctime>
#include <iostream>

#include <ocs2_oc/rollout/TimeTriggeredRollout.h>
#include <ocs2_oc/test/EXP1.h>

#include <ocs2_ocs2/OCS2.h>

using namespace ocs2;

enum { STATE_DIM = 2, INPUT_DIM = 1 };

TEST(exp1_gslq_test, exp1_ocs2_test) {
  SLQ_Settings slqSettings;
  slqSettings.ddpSettings_.displayInfo_ = false;
  slqSettings.ddpSettings_.displayShortSummary_ = false;
  slqSettings.useNominalTimeForBackwardPass_ = true;
  slqSettings.ddpSettings_.maxNumIterations_ = 30;
  slqSettings.ddpSettings_.nThreads_ = 3;
  slqSettings.ddpSettings_.maxNumIterations_ = 30;
  slqSettings.ddpSettings_.noStateConstraints_ = true;
  slqSettings.ddpSettings_.checkNumericalStability_ = false;
  slqSettings.ddpSettings_.absTolODE_ = 1e-10;
  slqSettings.ddpSettings_.relTolODE_ = 1e-7;
  slqSettings.ddpSettings_.maxNumStepsPerSecond_ = 10000;

  Rollout_Settings rolloutSettings;
  rolloutSettings.absTolODE_ = 1e-10;
  rolloutSettings.relTolODE_ = 1e-7;
  rolloutSettings.maxNumStepsPerSecond_ = 10000;

  GDDP_Settings gddpSettings;
  gddpSettings.displayInfo_ = false;
  gddpSettings.checkNumericalStability_ = false;
  gddpSettings.nThreads_ = 3;
  gddpSettings.absTolODE_ = 1e-10;
  gddpSettings.relTolODE_ = 1e-7;
  gddpSettings.maxNumIterationForLQ_ = 10;
  gddpSettings.maxNumStepsPerSecond_ = 10000;

  NLP_Settings nlpSettings;
  nlpSettings.displayInfo_ = true;
  nlpSettings.minRelCost_ = 0.001;
  nlpSettings.maxIterations_ = 20;
  nlpSettings.maxLearningRate_ = 1.0;
  nlpSettings.minLearningRate_ = 0.01;
  nlpSettings.useAscendingLineSearchNLP_ = false;

  // logic rule
  std::vector<double> initEventTimes{1.0, 2.0};
  std::vector<size_t> subsystemsSequence{0, 1, 2};
  std::shared_ptr<ModeScheduleManager<STATE_DIM, INPUT_DIM>> modeScheduleManagerPtr(
      new ModeScheduleManager<STATE_DIM, INPUT_DIM>({initEventTimes, subsystemsSequence}));

  double startTime = 0.0;
  double finalTime = 3.0;

  // partitioning times
  std::vector<double> partitioningTimes;
  partitioningTimes.push_back(startTime);
  partitioningTimes.push_back(1.0);
  partitioningTimes.push_back(2.0);
  partitioningTimes.push_back(finalTime);

  Eigen::Vector2d initState(2.0, 3.0);

  /******************************************************************************************************/
  /******************************************************************************************************/
  /******************************************************************************************************/
  // system dynamics
  EXP1_System systemDynamics(modeScheduleManagerPtr);
  TimeTriggeredRollout<STATE_DIM, INPUT_DIM> timeTriggeredRollout(systemDynamics, rolloutSettings);

  // system derivatives
  EXP1_SystemDerivative systemDerivative(modeScheduleManagerPtr);

  // system constraints
  EXP1_SystemConstraint systemConstraint;

  // system cost functions
  EXP1_CostFunction systemCostFunction(modeScheduleManagerPtr);

  // system operatingTrajectories
  Eigen::Matrix<double, STATE_DIM, 1> stateOperatingPoint = Eigen::Matrix<double, STATE_DIM, 1>::Zero();
  Eigen::Matrix<double, INPUT_DIM, 1> inputOperatingPoint = Eigen::Matrix<double, INPUT_DIM, 1>::Zero();
  EXP1_SystemOperatingTrajectories operatingTrajectories(stateOperatingPoint, inputOperatingPoint);

  /******************************************************************************************************/
  /******************************************************************************************************/
  /******************************************************************************************************/
  // OCS2
  OCS2<STATE_DIM, INPUT_DIM> ocs2(&timeTriggeredRollout, &systemDerivative, &systemConstraint, &systemCostFunction, &operatingTrajectories,
                                  slqSettings, modeScheduleManagerPtr, nullptr, gddpSettings, nlpSettings);

  // run ocs2 using LQ method for computing the derivatives
  ocs2.gddpSettings().useLQForDerivatives_ = true;
  ocs2.run(startTime, initState, finalTime, partitioningTimes, initEventTimes);
  Eigen::VectorXd optimizedEventTimes_LQ;
  ocs2.getParameters(optimizedEventTimes_LQ);
  double optimizedCost_LQ;
  ocs2.getCost(optimizedCost_LQ);

  // run ocs2 using BVP method for computing the derivatives
  ocs2.gddpSettings().useLQForDerivatives_ = false;
  ocs2.run(startTime, initState, finalTime, partitioningTimes, initEventTimes);
  Eigen::VectorXd optimizedEventTimes_BVP;
  ocs2.getParameters(optimizedEventTimes_BVP);
  double optimizedCost_BVP;
  ocs2.getCost(optimizedCost_BVP);

  /******************************************************************************************************/
  /******************************************************************************************************/
  /******************************************************************************************************/
  std::cerr << "### Initial event times are:        ["
            << Eigen::Map<Eigen::VectorXd>(initEventTimes.data(), initEventTimes.size()).transpose() << "]\n";
  std::cerr << "### Optimum cost LQ method:         " << optimizedCost_LQ << "\n";
  std::cerr << "### Optimum event times LQ method:  [" << optimizedEventTimes_LQ.transpose() << "]\n";
  std::cerr << "### Optimum cost BVP method:        " << optimizedCost_BVP << "\n";
  std::cerr << "### Optimum event times BVP method: [" << optimizedEventTimes_BVP.transpose() << "]\n";

  const double optimumCost = 5.444;
  const std::vector<double> optimumEventTimes{0.23, 1.02};

  ASSERT_NEAR(optimizedCost_LQ, optimumCost, 10 * slqSettings.ddpSettings_.minRelCost_)
      << "MESSAGE: OCS2 failed in the EXP1 using LQ approach for calculating derivatives!";
  ASSERT_NEAR(optimizedCost_BVP, optimumCost, 10 * slqSettings.ddpSettings_.minRelCost_)
      << "MESSAGE: OCS2 failed in the EXP1 using BVP approach for calculating derivatives!";
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
