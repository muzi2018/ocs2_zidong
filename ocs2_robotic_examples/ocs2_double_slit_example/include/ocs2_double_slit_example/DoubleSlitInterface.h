#pragma once

// C++
#include <iostream>
#include <string>

// OCS2
#include <ocs2_core/Dimensions.h>
#include <ocs2_core/constraint/ConstraintBase.h>
#include <ocs2_core/initialization/SystemOperatingPoint.h>
#include <ocs2_core/misc/loadEigenMatrix.h>
#include <ocs2_mpc/MPC_PI.h>
#include <ocs2_mpc/MPC_SLQ.h>
#include <ocs2_robotic_tools/common/RobotInterfaceBase.h>

// Double Slit
#include "ocs2_double_slit_example/DoubleSlitBarrierCost.h"
#include "ocs2_double_slit_example/DoubleSlitDynamics.h"
#include "ocs2_double_slit_example/definitions.h"

namespace ocs2 {
namespace double_slit {

class DoubleSlitInterface final : public RobotInterfaceBase<DoubleSlit::STATE_DIM_, DoubleSlit::INPUT_DIM_> {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  using dim_t = ocs2::Dimensions<DoubleSlit::STATE_DIM_, DoubleSlit::INPUT_DIM_>;
  using scalar_t = double;

  using DoubleSlitConstraint = ocs2::ConstraintBase<dim_t::STATE_DIM_, dim_t::INPUT_DIM_>;
  using DoubleSlitOperatingPoint = ocs2::SystemOperatingPoint<dim_t::STATE_DIM_, dim_t::INPUT_DIM_>;

  using mpc_t = ocs2::MPC_SLQ<dim_t::STATE_DIM_, dim_t::INPUT_DIM_>;
  using pi_mpc_t = ocs2::MPC_PI<dim_t::STATE_DIM_, dim_t::INPUT_DIM_>;

  /**
   * Constructor
   * @param [in] taskFileFolderName: The name of the folder containing task file
   */
  explicit DoubleSlitInterface(const std::string& taskFileFolderName);

  /**
   * Destructor
   */
  ~DoubleSlitInterface() = default;

  void setupOptimizer(const std::string& taskFile) override;

  /**
   * Gets a pointer to the internal SLQ-MPC class.
   *
   * @return Pointer to the internal MPC
   */
  mpc_t::Ptr getMPCPtr() { return mpcPtr_; }

  /**
   * @brief getDynamicsPtr
   * @return pointer to the internal system dynamics
   */
  DoubleSlitDynamics::Ptr getDynamicsPtr() { return linearSystemDynamicsPtr_; }

  /**
   * Gets a pointer to the internal PI-MPC class
   * @return Pointer to PI MPC
   */
  pi_mpc_t* getPiMpcPtr() { return piMpcPtr_.get(); }

  /**
   * @brief doubleSlitPotentialWall models the potential wall of our problem
   * @param x state
   * @param t time
   * @return cost
   */
  scalar_t doubleSlitPotentialWall(dim_t::state_vector_t x, scalar_t t) const;

  void loadSettings(const std::string& taskFile) override;

  /**************
   * Variables
   **************/
  std::string taskFile_;

  mpc_t::Ptr mpcPtr_;
  std::unique_ptr<pi_mpc_t> piMpcPtr_;

  DoubleSlitDynamics::Ptr linearSystemDynamicsPtr_;

  std::unique_ptr<DoubleSlitBarrierCost> costPtr_;
  DoubleSlitConstraint::Ptr linearSystemConstraintPtr_;
  DoubleSlitOperatingPoint::Ptr linearSystemOperatingPointPtr_;

  // cost parameters
  dim_t::state_matrix_t qM_;
  dim_t::input_matrix_t rM_;
  dim_t::state_matrix_t qMFinal_;
  dim_t::state_vector_t xNominal_;
  dim_t::input_vector_t uNominal_;
  scalar_t barrierLowerEnd_;
  scalar_t barrierUpperEnd_;
  scalar_t barrierTimePos_;

  size_t numPartitions_ = 0;
  dim_t::scalar_array_t partitioningTimes_;
};

}  // namespace double_slit
}  // namespace ocs2