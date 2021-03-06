#include "drake/examples/Cars/car_simulation.h"

#include "gtest/gtest.h"

#include "drake/examples/Cars/car_simulation.h"
#include "drake/examples/Cars/gen/euler_floating_joint_state.h"
#include "drake/examples/Cars/gen/simple_car_state.h"

namespace drake {
namespace examples {
namespace cars {
namespace test {
namespace {

GTEST_TEST(CarSimulationTest, SimpleCarVisualizationAdapter) {
  // The "device under test".
  auto dut = CreateSimpleCarVisualizationAdapter();

  const double time = 0.0;
  const Drake::NullVector<double> state_vector{};
  SimpleCarState<double> input_vector{};
  EulerFloatingJointState<double> output_vector{};
  output_vector = dut->output(time, state_vector, input_vector);

  EXPECT_DOUBLE_EQ(output_vector.x(), 0.0);
  EXPECT_DOUBLE_EQ(output_vector.y(), 0.0);
  EXPECT_DOUBLE_EQ(output_vector.z(), 0.0);
  EXPECT_DOUBLE_EQ(output_vector.roll(), 0.0);
  EXPECT_DOUBLE_EQ(output_vector.pitch(), 0.0);
  EXPECT_DOUBLE_EQ(output_vector.yaw(), 0.0);

  input_vector.set_x(22.0);
  input_vector.set_y(33.0);
  input_vector.set_heading(1.0);
  input_vector.set_velocity(11.0);

  output_vector = dut->output(time, state_vector, input_vector);

  EXPECT_DOUBLE_EQ(output_vector.x(), 22.0);
  EXPECT_DOUBLE_EQ(output_vector.y(), 33.0);
  EXPECT_DOUBLE_EQ(output_vector.z(), 0.0);
  EXPECT_DOUBLE_EQ(output_vector.roll(), 0.0);
  EXPECT_DOUBLE_EQ(output_vector.pitch(), 0.0);
  EXPECT_DOUBLE_EQ(output_vector.yaw(), 1.0);
}

}  // namespace
}  // namespace test
}  // namespace cars
}  // namespace examples
}  // namespace drake
