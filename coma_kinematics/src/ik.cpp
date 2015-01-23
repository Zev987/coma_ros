/*!
 * \ik.cpp
 * \brief Solves two-segment inverse kinematics for parallel continuum manipulators
 *
 * Solves two-segment inverse kinematics for parallel continuum manipulators
 *
 * \author Steven Kordell, WPI - spkordell@wpi.edu
 * \date January 18, 2014
 */

#include <coma_kinematics/ik.h>

using namespace std;

using ceres::AutoDiffCostFunction;
using ceres::NumericDiffCostFunction;
using ceres::CostFunction;
using ceres::Problem;
using ceres::Solver;
using ceres::Solve;

using Eigen::Vector3d;
using Eigen::Matrix3d;

double ik::rad(double degrees) {
	return degrees * (M_PI / 180.0);
}

ik::ik() {
	// a private handle for this ROS node (allows retrieval of relative parameters)
	ros::NodeHandle private_nh("~");

	// create the ROS topics
	//step_cmd_out = node.advertise < coma_serial::teleop_command > ("/serial_node/step_cmd", 1000);
	//resp_in = node.subscribe < std_msgs::Char > ("/serial_node/resp", 100, &ik::resp_cback, this);

	//initialize system parameters
	double r_in = 0.06126; 	//radius of inner leg hole pattern
	double r_out = 0.06770; //radius of outer leg hole pattern
	double deg = 10; //separation in degrees between the hole pairs that are close

	ikfunctor = new IKFunctor;

	//position of the leg constraints in the local frame at the bottom
	ikfunctor->p1_init = *(new Vector3d(r_in * cos(rad(0 - deg)),
			r_in * sin(rad(0 - deg)), 0));
	ikfunctor->p2_init = *(new Vector3d(r_in * cos(rad(0 + deg)),
			r_in * sin(rad(0 + deg)), 0));
	ikfunctor->p3_init = *(new Vector3d(r_in * cos(rad(120 - deg)),
			r_in * sin(rad(120 - deg)), 0));
	ikfunctor->p4_init = *(new Vector3d(r_in * cos(rad(120 + deg)),
			r_in * sin(rad(120 + deg)), 0));
	ikfunctor->p5_init = *(new Vector3d(r_in * cos(rad(240 - deg)),
			r_in * sin(rad(240 - deg)), 0));
	ikfunctor->p6_init = *(new Vector3d(r_in * cos(rad(240 + deg)),
			r_in * sin(rad(240 + deg)), 0));
	ikfunctor->p7_init = *(new Vector3d(r_out * cos(rad(60 - deg)),
			r_out * sin(rad(60 - deg)), 0));
	ikfunctor->p8_init = *(new Vector3d(r_out * cos(rad(60 + deg)),
			r_out * sin(rad(60 + deg)), 0));
	ikfunctor->p9_init = *(new Vector3d(r_out * cos(rad(180 - deg)),
			r_out * sin(rad(180 - deg)), 0));
	ikfunctor->p10_init = *(new Vector3d(r_out * cos(rad(180 + deg)),
			r_out * sin(rad(180 + deg)), 0));
	ikfunctor->p11_init = *(new Vector3d(r_out * cos(rad(300 - deg)),
			r_out * sin(rad(300 - deg)), 0));
	ikfunctor->p12_init = *(new Vector3d(r_out * cos(rad(300 + deg)),
			r_out * sin(rad(300 + deg)), 0));

	//position of the leg constraints in the local frame at the top
	ikfunctor->p6_final = *(new Vector3d(r_in * cos(rad(0 - deg)),
			r_in * sin(rad(0 - deg)), 0));
	ikfunctor->p1_final = *(new Vector3d(r_in * cos(rad(0 + deg)),
			r_in * sin(rad(0 + deg)), 0));
	ikfunctor->p2_final = *(new Vector3d(r_in * cos(rad(120 - deg)),
			r_in * sin(rad(120 - deg)), 0));
	ikfunctor->p3_final = *(new Vector3d(r_in * cos(rad(120 + deg)),
			r_in * sin(rad(120 + deg)), 0));
	ikfunctor->p4_final = *(new Vector3d(r_in * cos(rad(240 - deg)),
			r_in * sin(rad(240 - deg)), 0));
	ikfunctor->p5_final = *(new Vector3d(r_in * cos(rad(240 + deg)),
			r_in * sin(rad(240 + deg)), 0));
	ikfunctor->p12_final = *(new Vector3d(r_out * cos(rad(0 - deg)),
			r_out * sin(rad(0 - deg)), 0));
	ikfunctor->p7_final = *(new Vector3d(r_out * cos(rad(0 + deg)),
			r_out * sin(rad(0 + deg)), 0));
	ikfunctor->p8_final = *(new Vector3d(r_out * cos(rad(120 - deg)),
			r_out * sin(rad(120 - deg)), 0));
	ikfunctor->p9_final = *(new Vector3d(r_out * cos(rad(120 + deg)),
			r_out * sin(rad(120 + deg)), 0));
	ikfunctor->p10_final = *(new Vector3d(r_out * cos(rad(240 - deg)),
			r_out * sin(rad(240 - deg)), 0));
	ikfunctor->p11_final = *(new Vector3d(r_out * cos(rad(240 + deg)),
			r_out * sin(rad(240 + deg)), 0));

	ROS_INFO("COMA IK Solver Started");
}

void ik::solve() {
	//this matrix is the u matrix of unknowns described in the REACH paper.
	//boost::array<double, 7*12> guess_init;
	//std::vector<double> guess_init(7*12);
	double guess_init[GS];

	for (unsigned int i = 0; i < GS - 12; i++) {
		guess_init[i] = 0;
	}
	for (unsigned int i = GS - 12; i < GS; i++) {
		guess_init[i] = 0.13; //initialize leg lengths to 0.30 m
	}
	/*
	 for (unsigned int i = 0; i < 7*12; i++) {
	 cout << guess_init[i] << '\t';
	 }
	 cout << endl;
	 */

	//set desired forces, moments, position, and rotation
	Vector3d F(0, 0, 0); //applied force at end effector
	Vector3d L(0, 0, 0); //applied moment at end effector
	Matrix3d Rd = cosserat_rod::hat(*(new Vector3d(rad(0), rad(0), rad(-60)))); //desired end effector orientation
	Eigen::MatrixExponential < Matrix3d > Rdm(Rd);
	Rdm.compute(Rd);
	Vector3d pd(0.0, 0.0, 0.6); //desired end effector position

	ikfunctor->F = F;
	ikfunctor->L = L;
	ikfunctor->Rd = Rd;
	ikfunctor->pd = pd;

	//build the problem
	Problem problem;

	//set up the residual
	CostFunction* cost_function = new NumericDiffCostFunction<IKFunctor,
			ceres::CENTRAL, GS - 6, GS>(ikfunctor);
	problem.AddResidualBlock(cost_function, NULL, guess_init);

	//run the solver
	Solver::Options options;
	options.linear_solver_type = ceres::DENSE_QR;
	options.minimizer_progress_to_stdout = true;
	options.max_num_iterations = 500;
	options.parameter_tolerance = 1E-12;
	options.function_tolerance = 1E-12;
	Solver::Summary summary;
	Solve(options, &problem, &summary);

	std::cout << summary.BriefReport() << "\n";

	/*
	 for (unsigned int i = 0; i < 7 * 12; i++) {
	 cout << guess_init[i] << '\t';
	 }
	 cout << endl;
	 */

	for (unsigned int i = 6 * 12; i < 7 * 12; i++) {
		cout << guess_init[i] << endl;
	}
	//cout << endl;

}

//TODO: rename node "ik_server"
int main(int argc, char **argv) {
	// initialize ROS and the node
	ros::init(argc, argv, "ik");

	//initialize the solver
	ik solver;

	//solve the problem
	solver.solve();

	return EXIT_SUCCESS;
}
