#include <cmath>
#include <vector>
#include <unordered_map>

#include "drake/systems/plants/collision/DrakeCollision.h"
#include "drake/systems/plants/collision/Model.h"
#include "gtest/gtest.h"

using Eigen::Isometry3d;
using Eigen::Vector3d;
using Eigen::AngleAxisd;

namespace DrakeCollision {
namespace {

// Structure used to hold the analytical solution of the tests.
// It stores the collision point on the surface of a collision body in both
// world and body frames.
struct SurfacePoint {
  SurfacePoint() {}
  SurfacePoint(Vector3d wf, Vector3d bf) : world_frame(wf), body_frame(bf) {}
  // Eigen variables are left uninitalized by default.
  Vector3d world_frame;
  Vector3d body_frame;
};

// Solutions are accessed by collision element id using an std::unordered_set.
// DrakeCollision::Model returns the collision detection results as a vector of
// DrakeCollision::PointPair entries. Each entry holds a reference to the pair
// of collision elements taking part in the collision. Collision elements are
// referenced by their id.
// The order in which the pair of elements is stored in a PointPair cannot
// be guaranteed, and therefore we cannot guarantee the return of
// PointPair::getIdA() and PointPair::getIdB() in our tests.
// This means we cannot guarantee that future versions of the underlying
// implementation (say Bullet, FCL) won't change this order (since unfortunately
// id's are merely a memory address cast to an integer).
// The user only has access to collision elements by id.
// To provide a unique mapping between id's and the analytical solution to the
// contact point on a specific element here we use an `std::unordered_set` to
// map id's to a `SurfacePoint` structure holding the analytical solution on
// both body and world frames.
typedef std::unordered_map<DrakeCollision::ElementId, SurfacePoint>
    ElementToSurfacePointMap;

// GENERAL REMARKS ON THE TESTS PERFORMED
// A series of canonical tests are performed. These are Box_vs_Sphere,
// SmallBoxSittingOnLargeBox and NonAlignedBoxes.
// These tests are performed using the following algorithms:
// - closestPointsAllToAll: O(N^2) checking all pairs of collision elements.
//   Results are in bodies' frames.
// - collisionPointsAllToAll: Uses collision library dispatching.
//   Results are in world frame.
// - potentialCollisionPoints: randomly samples collision elements' pose
//   searching for potential collision points. Results are in the bodies'
//   frames.

// CLEARING CACHED RESULTS TEST
// Test ClearCachedResults tests the method Model::ClearCachedResults.
// This method clears results cached by the model so that future collision
// queries are performed from scratch and do not depend on past history.

// REMARKS ON MULTICONTACT
// Results from Model::potentialCollisionPoints are affected by previous calls
// to Model::collisionPointsAllToAll even if cached results are cleared with
// Model::ClearCachedResults. This is the reason why multicontact tests using
// Model::potentialCollisionPoints are performed separately in their own tests.
// For instance, for the NonAlignedBoxes there is a separate test called
// NonAlignedBoxes_multi performed with Model::potentialCollisionPoints.
// It seems like Bullet is storing some additional configuration setup with the
// call to Model::collisionPointsAllToAll that persists throughout subsequent
// calls to Model::potentialCollisionPoints.
// Therefore, DO NOT mix these calls since they might produce erroneous results.
// Notice also that the tolerance used for these tests is much higher. The
// reason is that Bullet randomly changes the pose of the collision elements
// searching for potential collision points. This is the additional source of
// error introduced.

/*
 * Three bodies (cube (1 m edges) and two spheres (0.5 m radii) arranged like
 *this
 *
 *                             *****
 *                           **     **
 *                           *   3   * --+--
 *                           **     **   |
 *                             *****     |
 *                                       |
 *          ^                           2 m
 *        y |                            |
 *          |                            |
 *      +---+---+              *****     |
 *      |   |   |            **      *   |
 *      |   +---+---->       *   2   * --+--
 *      | 1     |   x        **     **
 *      +-------+              *****
 *          |                    |
 *          +------ 2 m ---------+
 *          |                    |
 *
 */
GTEST_TEST(ModelTest, closestPointsAllToAll) {
  // Set up the geometry.
  Isometry3d T_body1_to_world, T_body2_to_world, T_body3_to_world,
      T_elem2_to_body;
  T_body1_to_world.setIdentity();
  T_body2_to_world.setIdentity();
  T_body3_to_world.setIdentity();
  T_body2_to_world.translation() << 1, 0, 0;
  T_elem2_to_body.setIdentity();
  T_elem2_to_body.translation() << 1, 0, 0;
  T_body3_to_world.translation() << 2, 2, 0;
  // rotate 90 degrees in z
  T_body3_to_world.linear() =
      AngleAxisd(M_PI_2, Vector3d::UnitZ()).toRotationMatrix();

  // Numerical precision tolerance to perform floating point comparisons.
  // For these very simple setup tests are expected to pass to machine
  // precision. More complex geometries might require a looser tolerance.
  const double tolerance = Eigen::NumTraits<double>::epsilon();

  DrakeShapes::Box geometry_1(Vector3d(1, 1, 1));
  DrakeShapes::Sphere geometry_2(0.5);
  DrakeShapes::Sphere geometry_3(0.5);
  Element element_1(geometry_1);
  Element element_2(geometry_2, T_elem2_to_body);
  Element element_3(geometry_3);

  // Populate the model.
  std::shared_ptr<Model> model = newModel();
  ElementId id1 = model->addElement(element_1);
  ElementId id2 = model->addElement(element_2);
  ElementId id3 = model->addElement(element_3);
  model->updateElementWorldTransform(id1, T_body1_to_world);
  model->updateElementWorldTransform(id2, T_body2_to_world);
  model->updateElementWorldTransform(id3, T_body3_to_world);

  // Compute the closest points.
  const std::vector<ElementId> ids_to_check = {id1, id2, id3};
  std::vector<PointPair> points;
  model->closestPointsAllToAll(ids_to_check, true, points);
  ASSERT_EQ(3, points.size());

  // Check the closest point between object 1 and object 2.
  // TODO(david-german-tri): Migrate this test to use Eigen matchers once
  // they are available.
  EXPECT_EQ(id1, points[0].getIdA());
  EXPECT_EQ(id2, points[0].getIdB());
  EXPECT_NEAR(1.0, points[0].getDistance(), tolerance);
  // Normal is on body B expressed in the world's frame.
  // Points are in the local frame of the body.
  EXPECT_TRUE(points[0].getNormal().isApprox(Vector3d(-1, 0, 0)));
  EXPECT_TRUE(points[0].getPtA().isApprox(Vector3d(0.5, 0, 0)));
  EXPECT_TRUE(points[0].getPtB().isApprox(Vector3d(0.5, 0, 0)));

  // Check the closest point between object 1 and object 3.
  EXPECT_EQ(id1, points[1].getIdA());
  EXPECT_EQ(id3, points[1].getIdB());
  // exact_distance =
  // distance_between_centers -
  // box_center_to_corner_distance -
  // sphere_center_to_surface_distance =
  // = sqrt(8.0) - 1.0/sqrt(2.0) - 1/2.
  double exact_distance = sqrt(8.0) - 1.0 / sqrt(2.0) - 0.5;
  EXPECT_NEAR(exact_distance, points[1].getDistance(), tolerance);
  // Normal is on body B expressed in the world's frame.
  // Points are in the local frame of the body.
  EXPECT_TRUE(
      points[1].getNormal().isApprox(Vector3d(-sqrt(2) / 2, -sqrt(2) / 2, 0)));
  EXPECT_TRUE(points[1].getPtA().isApprox(Vector3d(0.5, 0.5, 0)));
  // Notice the y component is positive given that the body's frame is rotated
  // 90 degrees around the z axis.
  // Therefore x_body = y_world, y_body=-x_world and z_body=z_world
  EXPECT_TRUE(
      points[1].getPtB().isApprox(Vector3d(-sqrt(2) / 4, sqrt(2) / 4, 0)));

  // Check the closest point between object 2 and object 3.
  EXPECT_EQ(id2, points[2].getIdA());
  EXPECT_EQ(id3, points[2].getIdB());
  EXPECT_NEAR(1.0, points[2].getDistance(), tolerance);
  // Normal is on body B expressed in the world's frame.
  // Points are in the local frame of the body.
  EXPECT_TRUE(points[2].getNormal().isApprox(Vector3d(0, -1, 0)));
  EXPECT_TRUE(points[2].getPtA().isApprox(Vector3d(1, 0.5, 0)));
  EXPECT_TRUE(points[2].getPtB().isApprox(Vector3d(-0.5, 0, 0)));
}

// A sphere of diameter 1.0 is placed on top of a box with sides of length 1.0.
// The sphere overlaps with the box with its deepest penetration point (the
// bottom) 0.25 units into the box (negative distance). Only one contact point
// is expected when colliding with a sphere.
class BoxVsSphereTest : public ::testing::Test {
 public:
  void SetUp() override {
    DrakeShapes::Box box(Vector3d(1.0, 1.0, 1.0));
    DrakeShapes::Sphere sphere(0.5);

    Element colliding_box(box);
    Element colliding_sphere(sphere);

    // Populate the model.
    model_ = newModel();
    box_id_ = model_->addElement(colliding_box);
    sphere_id_ = model_->addElement(colliding_sphere);

    // Access the analytical solution to the contact point on the surface of
    // each collision element by element id.
    // Solutions are expressed in world and body frames.
    solution_ = {
        /*           world frame     , body frame  */
        {box_id_,    {{0.0,  1.0, 0.0}, {0.0,  0.5, 0.0}}},
        {sphere_id_, {{0.0, 0.75, 0.0}, {0.0, -0.5, 0.0}}}};

    // Body 1 pose
    Isometry3d box_pose;
    box_pose.setIdentity();
    box_pose.translation() = Vector3d(0.0, 0.5, 0.0);
    model_->updateElementWorldTransform(box_id_, box_pose);

    // Body 2 pose
    Isometry3d sphere_pose;
    sphere_pose.setIdentity();
    sphere_pose.translation() = Vector3d(0.0, 1.25, 0.0);
    model_->updateElementWorldTransform(sphere_id_, sphere_pose);
  }

 protected:
  double tolerance_;
  std::unique_ptr<Model> model_;
  ElementId box_id_, sphere_id_;
  ElementToSurfacePointMap solution_;
};

TEST_F(BoxVsSphereTest, SingleContact) {
  // Numerical precision tolerance to perform floating point comparisons.
  // Its magnitude was chosen to be the minimum value for which these tests can
  // successfully pass.
  tolerance_ = 2.0e-9;

  // List of collision points.
  std::vector<PointPair> points;

  // Collision test performed with Model::closestPointsAllToAll.
  const std::vector<ElementId> ids_to_check = {box_id_, sphere_id_};
  model_->closestPointsAllToAll(ids_to_check, true, points);
  ASSERT_EQ(1, points.size());
  EXPECT_NEAR(-0.25, points[0].getDistance(), tolerance_);
  // Points are in the bodies' frame on the surface of the corresponding body.
  EXPECT_TRUE(points[0].getNormal().isApprox(Vector3d(0.0, -1.0, 0.0)));
  EXPECT_TRUE(
      points[0].getPtA().isApprox(solution_[points[0].getIdA()].body_frame));
  EXPECT_TRUE(
      points[0].getPtB().isApprox(solution_[points[0].getIdB()].body_frame));

  // Collision test performed with Model::collisionPointsAllToAll.
  // Not using margins.
  points.clear();
  model_->collisionPointsAllToAll(false, points);
  ASSERT_EQ(1, points.size());
  EXPECT_NEAR(-0.25, points[0].getDistance(), tolerance_);
  // Points are in the world frame on the surface of the corresponding body.
  // That is why getPtA() is generally different from getPtB(), unless there is
  // an exact non-penetrating collision.
  // WARNING:
  // This convention is different from the one used by closestPointsAllToAll
  // which computes points in the local frame of the body.
  // TODO(amcastro-tri): make these two conventions match? does this interfere
  // with any Matlab functionality?
  EXPECT_TRUE(points[0].getNormal().isApprox(Vector3d(0.0, -1.0, 0.0)));
  EXPECT_TRUE(
      points[0].getPtA().isApprox(solution_[points[0].getIdA()].world_frame));
  EXPECT_TRUE(
      points[0].getPtB().isApprox(solution_[points[0].getIdB()].world_frame));

  // Collision test performed with Model::collisionPointsAllToAll.
  // Using margins.
  points.clear();
  model_->collisionPointsAllToAll(true, points);
  ASSERT_EQ(1, points.size());
  EXPECT_NEAR(-0.25, points[0].getDistance(), tolerance_);
  // Points are in the world frame on the surface of the corresponding body.
  // That is why getPtA() is generally different from getPtB(), unless there is
  // an exact non-penetrating collision.
  // WARNING:
  // This convention is different from the one used by closestPointsAllToAll
  // which computes points in the local frame of the body.
  // TODO(amcastro-tri): make these two conventions match? does this interfere
  // with any Matlab functionality?
  EXPECT_TRUE(points[0].getNormal().isApprox(Vector3d(0.0, -1.0, 0.0)));
  EXPECT_TRUE(
      points[0].getPtA().isApprox(solution_[points[0].getIdA()].world_frame));
  EXPECT_TRUE(
      points[0].getPtB().isApprox(solution_[points[0].getIdB()].world_frame));

  // Calls to BulletModel::potentialCollisionPoints cannot be mixed.
  // Therefore throw an exception if user attempts to call
  // potentialCollisionPoints after calling another dispatch method.
  points.clear();
  EXPECT_THROW(points = model_->potentialCollisionPoints(false),
               std::runtime_error);
}

// This test is exactly the same as the previous Box_vs_Sphere test.
// The difference is that a multi-contact algorithm is being used.
// See REMARKS ON MULTICONTACT at the top of this file.
TEST_F(BoxVsSphereTest, MultiContact) {
  // Numerical precision tolerance to perform floating point comparisons.
  // Its magnitude was chosen to be the minimum value for which these tests can
  // successfully pass.
  tolerance_ = 2.0e-4;

  // List of collision points.
  std::vector<PointPair> points;

  // Collision test performed with Model::potentialCollisionPoints.
  points.clear();
  points = model_->potentialCollisionPoints(false);

  ASSERT_EQ(1, points.size());
  EXPECT_NEAR(-0.25, points[0].getDistance(), tolerance_);
  // Points are in the bodies' frame on the surface of the corresponding body.
  EXPECT_TRUE(points[0].getNormal().isApprox(Vector3d(0.0, -1.0, 0.0)));

  // Ensures the vertical coordinate is computed within tolerance_.
  EXPECT_NEAR(points[0].getPtA().y(),
              solution_[points[0].getIdA()].body_frame.y(), tolerance_);
  EXPECT_NEAR(points[0].getPtB().y(),
              solution_[points[0].getIdB()].body_frame.y(), tolerance_);

  // Notice however that the x and z coordinates are computed with a much
  // larger error.
  EXPECT_TRUE(points[0].getPtA().isApprox(
      solution_[points[0].getIdA()].body_frame, tolerance_ * 200));
  EXPECT_TRUE(points[0].getPtB().isApprox(
      solution_[points[0].getIdB()].body_frame, tolerance_ * 200));
}

// This test seeks to find out whether DrakeCollision::Model can report
// collision manifolds. To this end, a small cube with unit length sides is
// placed on top of a large cube with sides of length 5.0. The smaller cube is
// placed such that it intersects the large box. Therefore the intersection
// between the two boxes is not just a single point but the (squared) perimeter
// all around the smaller box (the manifold).
//
// Unfortunately these tests show that DrakeCollision::Model only reports a
// single (randomly chosen) point at one of the smaller box corners. In previous
// runs this was the corner at (0.5, 0.5, z) where z = 5.0 for the top of the
// large box and z = 4.9 for the bottom of the smaller box.
class SmallBoxSittingOnLargeBox: public ::testing::Test {
 public:
  void SetUp() override {
    // Boxes centered around the origin in their local frames.
    DrakeShapes::Box large_box(Vector3d(5.0, 5.0, 5.0));
    DrakeShapes::Box small_box(Vector3d(1.0, 1.0, 1.0));

    Element colliding_large_box(large_box);
    Element colliding_small_box(small_box);

    // Populate the model.
    model_ = newModel();
    large_box_id_ = model_->addElement(colliding_large_box);
    small_box_id_ = model_->addElement(colliding_small_box);

    // Access the analytical solution to the contact point on the surface of
    // each collision element by element id.
    // Solutions are expressed in world and body frames.
    solution_ = {
        /*              world frame    , body frame  */
        {large_box_id_, {{0.0, 5.0, 0.0}, {0.0,  2.5, 0.0}}},
        {small_box_id_, {{0.0, 4.9, 0.0}, {0.0, -0.5, 0.0}}}};

    // Large body pose
    Isometry3d large_box_pose;
    large_box_pose.setIdentity();
    large_box_pose.translation() = Vector3d(0.0, 2.5, 0.0);
    model_->updateElementWorldTransform(large_box_id_, large_box_pose);

    // Small body pose
    Isometry3d small_box_pose;
    small_box_pose.setIdentity();
    small_box_pose.translation() = Vector3d(0.0, 5.4, 0.0);
    model_->updateElementWorldTransform(small_box_id_, small_box_pose);
  }

 protected:
  double tolerance_;
  std::unique_ptr<Model> model_;
  ElementId small_box_id_, large_box_id_;
  ElementToSurfacePointMap solution_;
};

TEST_F(SmallBoxSittingOnLargeBox, SingleContact) {
  // Numerical precision tolerance to perform floating point comparisons.
  // Its magnitude was chosen to be the minimum value for which these tests can
  // successfully pass.
  tolerance_ = 2.0e-9;

  // List of collision points.
  std::vector<PointPair> points;

  // Unfortunately DrakeCollision::Model is randomly selecting one of the small
  // box's corners instead of reporting a manifold describing the perimeter of
  // the square where both boxes intersect.
  // Therefore it is impossible to assert if that choice would change with
  // future releases (say just because tolerances changed).
  // What we can test for sure is:
  // 1. The penetration depth.
  // 2. The vertical position of the collision point (since for any of the four
  //    corners of the small box is the same.

  // Collision test performed with Model::closestPointsAllToAll.
  const std::vector<ElementId> ids_to_check = {large_box_id_, small_box_id_};
  model_->closestPointsAllToAll(ids_to_check, true, points);
  ASSERT_EQ(1, points.size());
  EXPECT_NEAR(-0.1, points[0].getDistance(), tolerance_);
  EXPECT_TRUE(points[0].getNormal().isApprox(Vector3d(0.0, -1.0, 0.0)));
  // Collision points are reported on each of the respective bodies' frames.
  // Only test for vertical position.
  EXPECT_NEAR(points[0].getPtA().y(),
              solution_[points[0].getIdA()].body_frame.y(), tolerance_);
  EXPECT_NEAR(points[0].getPtB().y(),
              solution_[points[0].getIdB()].body_frame.y(), tolerance_);

  // Collision test performed with Model::collisionPointsAllToAll.
  // Not using margins.
  points.clear();
  model_->collisionPointsAllToAll(false, points);

  // Unfortunately DrakeCollision::Model's manifold has one point for this case.
  // Best for physics simulations would be DrakeCollision::Model to return at
  // least the four corners of the smaller box. However it randomly picks one
  // corner.
  ASSERT_EQ(1, points.size());
  EXPECT_NEAR(-0.1, points[0].getDistance(), tolerance_);
  // Collision points are reported in the world's frame.
  // Only test for vertical position.
  EXPECT_NEAR(points[0].getPtA().y(),
              solution_[points[0].getIdA()].world_frame.y(), tolerance_);
  EXPECT_NEAR(points[0].getPtB().y(),
              solution_[points[0].getIdB()].world_frame.y(), tolerance_);

  // Collision test performed with Model::collisionPointsAllToAll.
  // Using margins.
  points.clear();
  model_->collisionPointsAllToAll(true, points);

  ASSERT_EQ(1, points.size());
  EXPECT_NEAR(-0.1, points[0].getDistance(), tolerance_);
  // Collision points are reported in the world's frame.
  // Only test for vertical position.
  EXPECT_NEAR(points[0].getPtA().y(),
              solution_[points[0].getIdA()].world_frame.y(), tolerance_);
  EXPECT_NEAR(points[0].getPtB().y(),
              solution_[points[0].getIdB()].world_frame.y(), tolerance_);

  points.clear();
  EXPECT_THROW(points = model_->potentialCollisionPoints(false),
               std::runtime_error);
}

// This test is exactly the same as the previous SmallBoxSittingOnLargeBox.
// The difference is that a multi-contact algorithm is being used.
// See REMARKS ON MULTICONTACT at the top of this file.
TEST_F(SmallBoxSittingOnLargeBox, MultiContact) {
  // Numerical precision tolerance to perform floating point comparisons.
  // Its magnitude was chosen to be the minimum value for which these tests can
  // successfully pass.
  tolerance_ = 2.0e-9;

  // List of collision points.
  std::vector<PointPair> points;

  // Unfortunately DrakeCollision::Model is randomly selecting the contact
  // points. It is not even clear at this stage how many points in the manifold
  // we should expect.
  // What we can test for sure are:
  // 1. The penetration depth.
  // 2. The vertical position of the collision point (since for any of the four
  //    corners of the small box is the same).

  // Collision test performed with Model::potentialCollisionPoints.
  points = model_->potentialCollisionPoints(false);
  ASSERT_EQ(4, points.size());
  for (int i = 0; i < points.size(); ++i) {
    EXPECT_NEAR(-0.1, points[i].getDistance(), tolerance_);
    EXPECT_TRUE(points[i].getNormal().isApprox(Vector3d(0.0, -1.0, 0.0)));
    // Collision points are reported on each of the respective bodies' frames.
    // This is consistent with the return by Model::closestPointsAllToAll.
    // Only test for vertical position.
    EXPECT_NEAR(points[i].getPtA().y(),
                solution_[points[i].getIdA()].body_frame.y(), tolerance_);
    EXPECT_NEAR(points[i].getPtB().y(),
                solution_[points[i].getIdB()].body_frame.y(), tolerance_);
  }
}

// This test seeks to find out whether DrakeCollision::Model can report
// collision manifolds. To this end two unit length boxes are placed on top of
// one another. The box sitting on top is rotated by 45 degrees so that the
// contact area would consist of an octagon. If DrakeCollision::Model can report
// manifolds, the manifold would consist of the perimeter of this octagon.

// Unfortunately these tests show that DrakeCollision::Model only reports a
// single (randomly chosen) point within this octagonal contact area.
class NonAlignedBoxes: public ::testing::Test {
 public:
  void SetUp() override {
    // Boxes centered around the origin in their local frames.
    DrakeShapes::Box box1(Vector3d(1.0, 1.0, 1.0));
    DrakeShapes::Box box2(Vector3d(1.0, 1.0, 1.0));

    Element colliding_box1(box1);
    Element colliding_box2(box2);

    // Populate the model.
    model_ = newModel();
    box1_id_ = model_->addElement(colliding_box1);
    box2_id_ = model_->addElement(colliding_box1);

    // Access the analytical solution to the contact point on the surface of
    // each collision element by element id.
    // Solutions are expressed in world and body frames.
    solution_ = {
        /*         world frame    , body frame  */
        {box1_id_, {{0.0, 1.0, 0.0}, {0.0,  0.5, 0.0}}},
        {box2_id_, {{0.0, 0.9, 0.0}, {0.0, -0.5, 0.0}}}};

    // Box 1 pose.
    Isometry3d box1_pose;
    box1_pose.setIdentity();
    box1_pose.translation() = Vector3d(0.0, 0.5, 0.0);
    model_->updateElementWorldTransform(box1_id_, box1_pose);

    // Box 2 pose.
    // Rotate box 2 45 degrees around the y axis so that it does not alight with
    // box 1.
    Isometry3d box2_pose;
    box2_pose.setIdentity();
    box2_pose.translation() = Vector3d(0.0, 1.4, 0.0);
    box2_pose.linear() =
        AngleAxisd(M_PI_4, Vector3d::UnitY()).toRotationMatrix();
    model_->updateElementWorldTransform(box2_id_, box2_pose);
  }

 protected:
  double tolerance_;
  std::unique_ptr<Model> model_;
  ElementId box1_id_, box2_id_;
  ElementToSurfacePointMap solution_;
};

TEST_F(NonAlignedBoxes, SingleContact) {
  // Numerical precision tolerance to perform floating point comparisons.
  // Its magnitude was chosen to be the minimum value for which these tests can
  // successfully pass.
  tolerance_ = 3.0e-9;

  // List of collision points.
  std::vector<PointPair> points;

  // Unfortunately DrakeCollision::Model is randomly selecting one of the small
  // box's corners instead of reporting a manifold describing the perimeter of
  // the square where both boxes intersect.
  // Therefore it is impossible to assert if that choice would change with
  // future releases (say just because tolerances changed).
  // What we can test for sure is:
  // 1. The penetration depth.
  // 2. The vertical position of the collision point (since for any of the four
  //    corners of the small box is the same.
  // Collision test performed with Model::closestPointsAllToAll.
  const std::vector<ElementId> ids_to_check = {box1_id_, box2_id_};
  model_->closestPointsAllToAll(ids_to_check, true, points);
  ASSERT_EQ(1, points.size());
  EXPECT_NEAR(-0.1, points[0].getDistance(), tolerance_);
  EXPECT_TRUE(points[0].getNormal().isApprox(Vector3d(0.0, -1.0, 0.0)));
  // Collision points are reported on each of the respective bodies' frames.
  // Only test for vertical position.
  EXPECT_NEAR(points[0].getPtA().y(),
              solution_[points[0].getIdA()].body_frame.y(), tolerance_);
  EXPECT_NEAR(points[0].getPtB().y(),
              solution_[points[0].getIdB()].body_frame.y(), tolerance_);

  // Collision test performed with Model::collisionPointsAllToAll.
  points.clear();
  model_->collisionPointsAllToAll(false, points);
  // Unfortunately DrakeCollision::Model's manifold has one point for this case.
  // Best for physics simulations would be DrakeCollision::Model to return at
  // least the four corners of the smaller box. However it randomly picks one
  // corner.
  ASSERT_EQ(1, points.size());
  EXPECT_NEAR(-0.1, points[0].getDistance(), tolerance_);
  EXPECT_TRUE(points[0].getNormal().isApprox(Vector3d(0.0, -1.0, 0.0)));
  // Collision points are reported in the world's frame.
  // Only test for vertical position.
  EXPECT_NEAR(points[0].getPtA().y(),
              solution_[points[0].getIdA()].world_frame.y(), tolerance_);
  EXPECT_NEAR(points[0].getPtB().y(),
              solution_[points[0].getIdB()].world_frame.y(), tolerance_);

  points.clear();
  EXPECT_THROW(points = model_->potentialCollisionPoints(false),
               std::runtime_error);
}

// This test is exactly the same as the previous NonAlignedBoxes.
// The difference is that a multi-contact algorithm is being used.
// See REMARKS ON MULTICONTACT at the top of this file.
TEST_F(NonAlignedBoxes, MultiContact) {
  // Numerical precision tolerance to perform floating point comparisons.
  // Its magnitude was chosen to be the minimum value for which these tests can
  // successfully pass.
  tolerance_ = 5.0e-4;

  // List of collision points.
  std::vector<PointPair> points;

  // Collision test performed with Model::potentialCollisionPoints.
  points.clear();
  points = model_->potentialCollisionPoints(false);
  ASSERT_EQ(4, points.size());

  for (int i = 0; i < points.size(); ++i) {
    EXPECT_NEAR(-0.1, points[i].getDistance(), tolerance_);
    EXPECT_TRUE(points[i].getNormal().isApprox(Vector3d(0.0, -1.0, 0.0),
                                               tolerance_ * 50));
    // Collision points are reported on each of the respective bodies' frames.
    // This is consistent with the return by Model::closestPointsAllToAll.
    // Only test for vertical position.
    EXPECT_NEAR(points[i].getPtA().y(),
                solution_[points[0].getIdA()].body_frame.y(), tolerance_);
    EXPECT_NEAR(points[i].getPtB().y(),
                solution_[points[0].getIdB()].body_frame.y(), tolerance_);
  }
}

// Tests and illustrates the use of Model::ClearCachedResults to perform a
// collision query that returns a single result.
TEST_F(SmallBoxSittingOnLargeBox, ClearCachedResults) {
  // Numerical precision tolerance to perform floating point comparisons.
  // Its magnitude was chosen to be the minimum value for which these tests can
  // successfully pass.
  tolerance_ = 2.0e-9;

  // Large body pose
  Isometry3d large_box_pose;
  large_box_pose.setIdentity();
  large_box_pose.translation() = Vector3d(0.0, 2.5, 0.0);
  model_->updateElementWorldTransform(large_box_id_, large_box_pose);

  // Small body pose
  Isometry3d small_box_pose;
  small_box_pose.setIdentity();
  small_box_pose.translation() = Vector3d(0.0, 5.4, 0.0);
  model_->updateElementWorldTransform(small_box_id_, small_box_pose);

  // List of collision points.
  std::vector<PointPair> points;

  // Not clearing cached results
  for (int i = 0; i < 4; ++i) {
    // Small disturbance so that tests are slightly different causing Bullet's
    // dispatcher to cache these results.
    if (i == 0) small_box_pose.translation() = Vector3d(   0.0, 5.4,    0.0);
    if (i == 1) small_box_pose.translation() = Vector3d(1.0e-3, 5.4,    0.0);
    if (i == 2) small_box_pose.translation() = Vector3d(   0.0, 5.4, 1.0e-3);
    if (i == 3) small_box_pose.translation() = Vector3d(1.0e-3, 5.4, 1.0e-3);
    model_->updateElementWorldTransform(small_box_id_, small_box_pose);

    // Notice that the results vector is cleared every time so that results
    // do not accumulate.
    points.clear();

    model_->collisionPointsAllToAll(false, points);
  }

  // If Bullet cached those tests then there should be four results.
  ASSERT_EQ(4, points.size());

  for (int i = 0; i < points.size(); ++i) {
    EXPECT_NEAR(-0.1, points[i].getDistance(), tolerance_);
    EXPECT_TRUE(
        points[i].getNormal().isApprox(Vector3d(0.0, -1.0, 0.0), tolerance_));
    // Only test for vertical position.
    EXPECT_NEAR(points[i].getPtA().y(),
                solution_[points[0].getIdA()].world_frame.y(), tolerance_);
    EXPECT_NEAR(points[i].getPtB().y(),
                solution_[points[0].getIdB()].world_frame.y(), tolerance_);
  }

  // Clearing cached results
  for (int i = 0; i < 4; ++i) {
    // Small disturbance so that tests are slightly different causing Bullet's
    // dispatcher to cache these results.
    if (i == 0) small_box_pose.translation() = Vector3d(   0.0, 5.4,    0.0);
    if (i == 1) small_box_pose.translation() = Vector3d(1.0e-3, 5.4,    0.0);
    if (i == 2) small_box_pose.translation() = Vector3d(   0.0, 5.4, 1.0e-3);
    if (i == 3) small_box_pose.translation() = Vector3d(1.0e-3, 5.4, 1.0e-3);
    model_->updateElementWorldTransform(small_box_id_, small_box_pose);

    // Notice that the results vector is cleared every time so that results
    // do not accumulate.
    points.clear();

    // Clear cache before each test.
    model_->ClearCachedResults(false);
    model_->collisionPointsAllToAll(false, points);
  }

  // Since cache was cleared on every test, only one point is expected.
  ASSERT_EQ(1, points.size());
  EXPECT_NEAR(-0.1, points[0].getDistance(), tolerance_);
  EXPECT_TRUE(points[0].getNormal().isApprox(Vector3d(0.0, -1.0, 0.0)));
  // Only test for vertical position.
  EXPECT_NEAR(points[0].getPtA().y(),
              solution_[points[0].getIdA()].world_frame.y(), tolerance_);
  EXPECT_NEAR(points[0].getPtB().y(),
              solution_[points[0].getIdB()].world_frame.y(), tolerance_);
}

}  // namespace
}  // namespace DrakeCollision
