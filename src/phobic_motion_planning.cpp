#include "phobic_motion_planning.h"

int main(int argc, char** argv)
{
	ros::init(argc, argv, "Phobic_whife_MP");
	ros::NodeHandle node_mp;
	phobic_mp phobic_local_mp(node_mp); 
	ros::Subscriber reader;
	reader = node_mp.subscribe(node_mp.resolveName("INFO_CYLINDER"), 1, &phobic_mp::MotionPlanningCallback, &phobic_local_mp);

	ros::Rate loop_rate( 1 ); // 5Hz
	while (ros::ok())
	{

		loop_rate.sleep();
		ros::spinOnce();

	}
	

	return 0;
	
}


void phobic_mp::MotionPlanningCallback(const desperate_housewife::cyl_info cyl_msg)
{
	
	// To the first we take an informations from cilynders and robot.
	if(cyl_msg.dimension <= 0)
	{
		ROS_INFO("There are not a objects in the scene");
	}
	else
	{
		ROS_INFO("There are a objects in the scene, start the motion planning");
		
		Goal.resize(cyl_msg.dimension);
		
		//read the cylinder informations in tf::StampedTransform
		for (int i = 0; i < cyl_msg.dimension; i++)
		{

			listener_info.lookupTransform("/camera_rgb_optical_frame", "cilindro_" + std::to_string(i) , ros::Time(0), Goal[i] );
			cyl_height[i] = cyl_msg.length[i];
			cyl_radius[i] = cyl_msg.radius[i];
		}
		
		//read the robot informations in tf::StampedTransform. Vito has 7 link and 6 joint
		for(int i = 0; i<=7 ; i++)
		{
			listener_info.lookupTransform("/camera_rgb_optical_frame", "left_arm_" + std::to_string(i) + "_joint" , ros::Time(0), Vito_desperate.Link_left[i] );
			listener_info.lookupTransform("/camera_rgb_optical_frame", "right_arm_" + std::to_string(i) + "_joint" , ros::Time(0), Vito_desperate.Link_right[i] );
			Vito_desperate.robot_position_left.push_back(Take_Pos(Vito_desperate.Link_left[i]));
			Vito_desperate.robot_position_right.push_back(Take_Pos(Vito_desperate.Link_right[i]));
		}

		// //Soft Hand information

		listener_info.lookupTransform("/camera_rgb_optical_frame", "right_hand_palm_link" , ros::Time(0), Vito_desperate.SoftHand_r );
		listener_info.lookupTransform("/camera_rgb_optical_frame", "left_hand_palm_link" , ros::Time(0), Vito_desperate.SoftHand_l );
                              
		// // with this informations we can make a MP 

		for (int i = 0; i <  cyl_msg.dimension; i++)
		{
			SetPotentialField( *Goal.begin());
		}


	}

		


}

void phobic_mp::SetPotentialField( tf::StampedTransform object)
{
	//Set robot potential fields 
	
	Eigen::Matrix4d frame_eigen;
	frame_eigen = FromTFtoEigen(object);
	frame_kinect = frame_eigen.inverse();

	SetPotentialField_robot(Force_repulsion);


	// test for setting the potential field
	bool Test_obj = true;
	// if true the object is a goal, otherwise is a obstacles

	//Test_obj = objectORostacles(object)
	
	if(Test_obj == true)
	{	
		goal_position.x = frame_kinect(0,3);
		goal_position.y = frame_kinect(1,3);
		goal_position.z = frame_kinect(2,3);

		SetAttraciveField( goal_position);
		SetRepulsiveFiled( goal_position);
	}

	else
	{	//set obstacles repulsion force
		
		obstacle_position.x = frame_kinect(0,3);
		obstacle_position.y = frame_kinect(1,3);
		obstacle_position.z = frame_kinect(2,3);
	}


	Calculate_force();


	Goal.erase(Goal.begin());




}


// bool phobic_mp::objectORostacles(tf::StampedTransform frame)
// {

	
// 	cyl_height.front() ;
// 	cyl_radius[i] =cyl_msg.radius;






// 	return true;
// }





Eigen::Matrix4d FromTFtoEigen(tf::StampedTransform object)
{
	Eigen::Quaterniond transf_quad(object.getRotation().getW(),object.getRotation().getX(),object.getRotation().getY(),object.getRotation().getZ());
	transf_quad.normalize();
	Eigen::Vector3d translation(object.getOrigin().x(),object.getOrigin().y(),object.getOrigin().z());
	Eigen::Matrix3d rotation(transf_quad.toRotationMatrix());
	
	Eigen::Matrix4d Matrix_transf;

	Matrix_transf.row(0) << rotation.row(0), translation[0];
	Matrix_transf.row(1) << rotation.row(1), translation[1];
	Matrix_transf.row(2) << rotation.row(2), translation[2];
	Matrix_transf.row(3) << 0,0,01;

	return Matrix_transf;

}


void phobic_mp::SetAttraciveField( pcl::PointXYZ Pos)
{
	
	std::pair<double, pcl::PointXYZ> distance_HtO;
	

	Vito_desperate.Pos_HAND_l = Take_Pos( Vito_desperate.SoftHand_l);
	Vito_desperate.Pos_HAND_r = Take_Pos( Vito_desperate.SoftHand_r);
	double dist_lTo, dist_rTo;

	dist_lTo = GetDistance(goal_position, Vito_desperate.Pos_HAND_l).first;
	dist_rTo = GetDistance(goal_position, Vito_desperate.Pos_HAND_r).first;

	if(dist_lTo <= dist_rTo)
	{
		Arm = true; 
		distance_HtO = GetDistance(Vito_desperate.Pos_HAND_l, Pos );
		ROS_INFO("LEFT ARM");
	}
	else
	{
		Arm = false;
		distance_HtO = GetDistance(Vito_desperate.Pos_HAND_r, Pos );
		ROS_INFO("RIGHT ARM");
	}	


	Force_attractive.push_back(0.5*P_goal*pow(distance_HtO.first,2));
}


void phobic_mp::SetPotentialField_robot(std::vector<double> &Force_repulsion)
{
	
	std::vector<std::pair<double, pcl::PointXYZ>> distance_link;
	std::vector<pcl::PointXYZ>* robot_link_position;
	//robot_link_position->resize(Vito_desperate.robot_position_left.size());
	
	//Arm=true is left arm, Arm = false is right arm
	if(Arm = true)
	{
		robot_link_position = &Vito_desperate.robot_position_left;

	}
	else
	{
		robot_link_position = &Vito_desperate.robot_position_right;
	}	


	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[2], (*robot_link_position)[1]).first,GetDistance((*robot_link_position)[2], (*robot_link_position)[1]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[3], (*robot_link_position)[1]).first,GetDistance((*robot_link_position)[3], (*robot_link_position)[1]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[3], (*robot_link_position)[2]).first,GetDistance((*robot_link_position)[3], (*robot_link_position)[2]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[4], (*robot_link_position)[1]).first,GetDistance((*robot_link_position)[4], (*robot_link_position)[1]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[4], (*robot_link_position)[2]).first,GetDistance((*robot_link_position)[4], (*robot_link_position)[2]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[4], (*robot_link_position)[3]).first,GetDistance((*robot_link_position)[4], (*robot_link_position)[3]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[5], (*robot_link_position)[1]).first,GetDistance((*robot_link_position)[5], (*robot_link_position)[1]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[5], (*robot_link_position)[2]).first,GetDistance((*robot_link_position)[5], (*robot_link_position)[2]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[5], (*robot_link_position)[3]).first,GetDistance((*robot_link_position)[5], (*robot_link_position)[3]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[5], (*robot_link_position)[4]).first,GetDistance((*robot_link_position)[5], (*robot_link_position)[4]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[6], (*robot_link_position)[1]).first,GetDistance((*robot_link_position)[6], (*robot_link_position)[1]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[6], (*robot_link_position)[2]).first,GetDistance((*robot_link_position)[6], (*robot_link_position)[2]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[6], (*robot_link_position)[3]).first,GetDistance((*robot_link_position)[6], (*robot_link_position)[3]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[6], (*robot_link_position)[4]).first,GetDistance((*robot_link_position)[6], (*robot_link_position)[4]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[6], (*robot_link_position)[5]).first,GetDistance((*robot_link_position)[6], (*robot_link_position)[5]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[7], (*robot_link_position)[1]).first,GetDistance((*robot_link_position)[7], (*robot_link_position)[1]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[7], (*robot_link_position)[2]).first,GetDistance((*robot_link_position)[7], (*robot_link_position)[2]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[7], (*robot_link_position)[3]).first,GetDistance((*robot_link_position)[7], (*robot_link_position)[3]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[7], (*robot_link_position)[4]).first,GetDistance((*robot_link_position)[7], (*robot_link_position)[4]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[7], (*robot_link_position)[5]).first,GetDistance((*robot_link_position)[7], (*robot_link_position)[5]).second));
	distance_link.push_back(std::make_pair(GetDistance((*robot_link_position)[7], (*robot_link_position)[6]).first,GetDistance((*robot_link_position)[7], (*robot_link_position)[6]).second));
	
	// Repulsive fields = K/distance^2 (1/distance -1/influence) partial_derivative_vector

	for (int i=0; i <= distance_link.size(); i++)
	{
		std::vector<double> vec_Temp;
		vec_Temp.push_back(distance_link[i].second.x);
		vec_Temp.push_back(distance_link[i].second.y);
		vec_Temp.push_back(distance_link[i].second.z);

		Force_repulsion.push_back( (P_obj/pow(distance_link[i].first,2)) * (1/distance_link[i].first - 1/influence) * vec_Temp[i] );
	}
}



void phobic_mp::SetRepulsiveFiled(pcl::PointXYZ Pos)
{
	std::vector<std::pair<double, pcl::PointXYZ>> distance_local_obj;


	for (int i=2; i <= Vito_desperate.robot_position_left.size();i++)
	{	
		distance_local_obj.push_back(std::make_pair(GetDistance(Pos,Vito_desperate.robot_position_left[i]).first, GetDistance(Pos,Vito_desperate.robot_position_left[i]).second)); 

	}

	for (int i=0; i <= distance_local_obj.size(); i++)
	{
		std::vector<double> vec_Temp;
		vec_Temp.push_back(distance_local_obj[i].second.x);
		vec_Temp.push_back(distance_local_obj[i].second.y);
		vec_Temp.push_back(distance_local_obj[i].second.z);

		if(distance_local_obj[i].first <= influence)
		{
			Force_repulsion.push_back( (P_obj/pow(distance_local_obj[i].first,2)) * (1/distance_local_obj[i].first - 1/influence) * vec_Temp[i] );
		}
		else
		{
			Force_repulsion.push_back(0);
		}
	}

}


std::pair<double, pcl::PointXYZ> phobic_mp::GetDistance(pcl::PointXYZ obj1, pcl::PointXYZ obj2 )
{
	std::pair<double, pcl::PointXYZ> distance_local;
	pcl::PointXYZ local_point;
	
	distance_local.first = sqrt(pow(obj1.x-obj2.x,2) + pow(obj1.y-obj2.y,2) + pow(obj1.z-obj2.z,2));  
	local_point.x = (obj1.x-obj2.x);
	local_point.y = (obj1.y-obj2.y);
	local_point.z = (obj1.z-obj2.z);
	distance_local.second =  local_point;

	return distance_local;
}



pcl::PointXYZ phobic_mp::Take_Pos(tf::StampedTransform M_tf)
{
	pcl::PointXYZ Pos_vito;
	Eigen::Matrix4d Link_eigen;
	
	Link_eigen = FromTFtoEigen(M_tf);

	Pos_vito.x = Link_eigen(0,3);
	Pos_vito.y = Link_eigen(1,3);
	Pos_vito.z = Link_eigen(2,3);

	return Pos_vito;
}





void phobic_mp::Calculate_force()
{
	double repulsive_local;
	double attractive_local;
	
	for(int i=0;i < Force_repulsion.size();i++)
	{
		repulsive_local = Force_repulsion[i] +  Force_repulsion[i+1];

	}

	if(Force_attractive.size() > 1)
	{
		for (int i=0; i< Force_attractive.size(); i++)
		{
			attractive_local = Force_attractive[i] +  Force_attractive[i+1];
		}
	}

	else
	{
		attractive_local = *Force_attractive.begin();

	}


	Force = attractive_local + repulsive_local;
}
