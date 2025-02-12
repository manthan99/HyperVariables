/// This file is subject to the terms and conditions defined in
/// the 'LICENSE' file, which is part of this repository.

#pragma once

#include <memory>

#include <glog/logging.h>
#include <Eigen/LU>

#include "variables/distortions/forward.hpp"

#include "variables/cartesian.hpp"
#include "variables/jacobian.hpp"

namespace hyper {

template <typename TScalar>
class AbstractDistortion<TScalar, true>
    : public AbstractVariable<TScalar> {
 public:
  using Scalar = std::remove_const_t<TScalar>;
  using PixelRef = Eigen::Ref<const typename Traits<Pixel<Scalar>>::Base>;

  /// Allocates a Jacobian.
  /// \return Allocated Jacobian.
  auto allocatePixelDistortionJacobian() const -> DynamicInputJacobian<Pixel<Scalar>>;

  /// Distorts a pixel.
  /// \param pixel Pixel to distort.
  /// \param raw_J_p_p Pixel Jacobian.
  /// \param raw_J_p_d  Distortion Jacobian.
  /// \return Distorted pixel.
  virtual auto distort(const PixelRef& pixel, Scalar* raw_J_p_p, Scalar* raw_J_p_d) const -> Pixel<Scalar> = 0;

  /// Undistorts a pixel.
  /// \param pixel Pixel to undistort.
  /// \param raw_J_p_p Pixel Jacobian.
  /// \param raw_J_p_d  Distortion Jacobian.
  /// \return Undistorted pixel.
  virtual auto undistort(const PixelRef& pixel, Scalar* raw_J_p_p, Scalar* raw_J_p_d) const -> Pixel<Scalar>;

  /// Maps a distortion.
  /// \param raw_distortion Raw distortion.
  /// \return Mapped distortion.
  virtual auto map(const Scalar* raw_distortion) const -> std::unique_ptr<AbstractDistortion<const Scalar>> = 0;

  /// Maps a distortion.
  /// \param raw_distortion Raw distortion.
  /// \return Mapped distortion.
  virtual auto map(Scalar* raw_distortion) const -> std::unique_ptr<AbstractDistortion<Scalar>> = 0;
};

template <typename TScalar>
class AbstractDistortion<TScalar, false>
    : public AbstractDistortion<TScalar, true> {
 public:
  using Scalar = std::remove_const_t<TScalar>;

  /// Sets the default parameters.
  virtual auto setDefault() -> AbstractDistortion& = 0;

  /// Perturbs this.
  /// \param scale Perturbation scale.
  virtual auto perturb(const Scalar& scale) -> AbstractDistortion& = 0;
};

template <typename TScalar>
auto AbstractDistortion<TScalar, true>::allocatePixelDistortionJacobian() const -> DynamicInputJacobian<Pixel<Scalar>> {
  const auto [_, size] = this->memory();
  return {Traits<Pixel<Scalar>>::kNumParameters, size};
}

template <typename TScalar>
auto AbstractDistortion<TScalar, true>::undistort(const PixelRef& pixel, Scalar* raw_J_p_p, Scalar* raw_J_p_d) const -> Pixel<Scalar> {
  Pixel<Scalar> output = pixel;
  Jacobian<Pixel<Scalar>> J_p_p, J_p_p_i;

  for (auto i = 0; i <= NumericVariableTraits<Scalar>::kMaxNumDistortionSteps; ++i) {
    const auto b = (distort(output, J_p_p_i.data(), nullptr) - pixel).eval();
    J_p_p.noalias() = J_p_p_i.inverse();

    if (NumericVariableTraits<Scalar>::kDistortionTolerance2 < b.dot(b)) {
      DLOG_IF(WARNING, i == NumericVariableTraits<Scalar>::kMaxNumDistortionSteps) << "Maximum number of iterations reached.";
      DLOG_IF_EVERY_N(WARNING, J_p_p_i.determinant() < NumericVariableTraits<Scalar>::kSmallAngleTolerance, NumericVariableTraits<Scalar>::kMaxNumDistortionSteps) << "Numerical issues detected.";
      output.noalias() -= J_p_p * b;
    } else {
      break;
    }
  }

  if (raw_J_p_p) {
    Eigen::Map<Jacobian<Pixel<Scalar>>>{raw_J_p_p}.noalias() = J_p_p;
  }

  if (raw_J_p_d) {
    const auto [_, size] = this->memory();
    auto J_p_d_i = DynamicInputJacobian<Pixel<Scalar>>{Traits<Pixel<Scalar>>::kNumParameters, size};
    distort(output, nullptr, J_p_d_i.data());
    Eigen::Map<DynamicInputJacobian<Pixel<Scalar>>>{raw_J_p_d, Traits<Pixel<Scalar>>::kNumParameters, size}.noalias() = Scalar{-1} * J_p_p * J_p_d_i;
  }

  return output;
}

} // namespace hyper
