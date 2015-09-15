#include <phobic_hand_pose.h>

#include <chrono>

std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

int main(int argc, char** argv)
{
  ros::init(argc, argv, "Phobic_HandPose");
  ros::NodeHandle node_hand;
  phobic_hand phobic_local_hand(node_hand);
  ros::Subscriber reader;

  t1 = std::chrono::high_resolution_clock::now();
  reader = node_hand.subscribe(node_hand.resolveName("INFO_CYLINDER"), 1, &phobic_hand::HandPoseCallback, &phobic_local_hand);
  std::cout << "Duration subscriber\t"  << std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - t1 ).count() << std::endl;
  t1 = std::chrono::high_resolution_clock::now();

  double spin_rate;
  ros::param::get("~spin_rate",spin_rate);
  ROS_INFO( "Spin Rate %lf", spin_rate);
  ros::param::get("~base_frame", phobic_local_hand.base_frame_topic);
  ROS_INFO( "Base Frame Link %s", phobic_local_hand.base_frame_topic.c_str());
  ros::param::get("~right_hand_frame", phobic_local_hand.right_hand_frame_topic);
  ROS_INFO( "Right Hand Frame Link %s", phobic_local_hand.right_hand_frame_topic.c_str());
  ros::param::get("~left_hand_frame", phobic_local_hand.left_hand_frame_topic);
  ROS_INFO( "Left Hand Frame Link %s", phobic_local_hand.left_hand_frame_topic.c_str());
  ros::param::get("~cylinders_topic", phobic_local_hand.cylinders_topic);
  ROS_INFO( "Cylinders Topic %s", phobic_local_hand.cylinders_topic.c_str());

  ros::Rate loop_rate( spin_rate ); // 1Hz
  while (ros::ok())
    {

      loop_rate.sleep();
      ros::spinOnce();
    }
  return 0;
}


void phobic_hand::HandPoseCallback(const desperate_housewife::cyl_info cyl_msg)
{
  // To the first we take an informations from cilynders and robot.

  if(cyl_msg.dimension <= 0)
    {
      ROS_INFO("There are not a objects in the scene");
    }
  else
    {
      ROS_DEBUG("There are a objects in the scene, start the motion planning");

      Goal.resize(cyl_msg.dimension);

      ROS_DEBUG("cyl_msg size: %d", cyl_msg.dimension);

      if(check_robot == true)
        {

          listener_info.waitForTransform(base_frame_topic.c_str(), right_hand_frame_topic.c_str() , ros::Time::now(), ros::Duration(1));
          listener_info.lookupTransform(base_frame_topic.c_str(), right_hand_frame_topic.c_str() , ros::Time(0), SoftHand_r);

          listener_info.waitForTransform(base_frame_topic.c_str(), left_hand_frame_topic.c_str() , ros::Time::now(), ros::Duration(1));
          listener_info.lookupTransform(base_frame_topic.c_str(), left_hand_frame_topic.c_str() , ros::Time(0), SoftHand_l);

          Eigen::Matrix4d local_pos_hand_l, local_pos_hand_r;
          local_pos_hand_l = FromTFtoEigen(SoftHand_l);
          local_pos_hand_r = FromTFtoEigen(SoftHand_r);
          Pos_HAND_l.x = local_pos_hand_l(0,3);
          Pos_HAND_l.y = local_pos_hand_l(1,3);
          Pos_HAND_l.z = local_pos_hand_l(2,3);

          Pos_HAND_r.x = local_pos_hand_r(0,3);
          Pos_HAND_r.y = local_pos_hand_r(1,3);
          Pos_HAND_r.z = local_pos_hand_r(2,3);
        }
      else
        {	//solo per provarlo
          Pos_HAND_r.x = 0.1;
          Pos_HAND_r.y = -0.1;
          Pos_HAND_r.z = 0.2;

          Pos_HAND_l.x = -0.1;
          Pos_HAND_l.y = -0.1;
          Pos_HAND_l.z = 0.2;
        }

      //read the cylinder informations in tf::StampedTransform
      for (int i = 0; i < cyl_msg.dimension; i++)
        {
          // cylinders_topic.append('_');
          listener_info.lookupTransform(base_frame_topic.c_str(), cylinders_topic.c_str() + std::string("_") +  std::to_string(i) , ros::Time(0), Goal[i] );

          cyl_height = cyl_msg.length[i];
          cyl_radius = cyl_msg.radius[i];
          cyl_info = cyl_msg.Info[i]; //standing or liyng
          cyl_v = cyl_msg.vol[i]; // full or empty

          ROS_DEBUG("prima di getcyl");

          GetCylPos(Goal[i], i);;

        }

      Goal.clear();
      Send();
      std::cout << "Duration First Call back\t"  << std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - t1 ).count() << std::endl;
      t1 = std::chrono::high_resolution_clock::now();

    }
}



void phobic_hand::GetCylPos( tf::StampedTransform &object, int &i)
{	
  ROS_DEBUG("get goal position");

  T_vito_c = FromTFtoEigen(object);
  frame_cylinder = T_vito_c.inverse(); //T_c_vito
  // std::cout<< T_vito_c <<"frame T_vito_c" <<std::endl<<std::flush;

  if(Test_obj == true)
    {

      pcl::PointXYZ goal_v_c_pos;
      goal_v_c_pos.x = T_vito_c(0,3);
      goal_v_c_pos.y = T_vito_c(1,3);
      goal_v_c_pos.z = T_vito_c(2,3);
      WhichArm(goal_v_c_pos);

      SetHandPosition(i);
    }

  else
    {	//set obstacles repulsion force
      obstacle_position.x = T_vito_c(0,3);
      obstacle_position.y = T_vito_c(1,3);
      obstacle_position.z = T_vito_c(2,3);
      T_k_ob = T_vito_c;

    }
}

void phobic_hand::SetHandPosition(int &u)
{

  Eigen::Matrix4d M_desired_local; // in cyl frame
  Eigen::Vector4d Point_desired,Pos_ori_hand; //in cyl frame
  Eigen::Vector4d translation; //in cyl frame
  Eigen::Vector3d x(1,0,0);
  Eigen::Vector3d y(0,1,0), z_d, z(0,0,1);
  Eigen::Matrix4d T_K_H;
  tf::StampedTransform T_K_vito_ancor;
  Eigen::Vector4d local;

  Eigen::Matrix4d Rot_z;
  Rot_z.row(0)<< -1,0,0,0;
  Rot_z.row(1)<< 0,-1,0,0;
  Rot_z.row(2)<< 0,0,1,0;
  Rot_z.row(3)<< 0,0,0,1;

  if((cyl_info == 0) && (cyl_v == 0))
    {
      if (Arm == true) //left arm
        {
          M_desired_local.col(0) << x, 0;
          M_desired_local.col(1) << -z.cross(x),0;
        }
      else
        {
          M_desired_local.col(0) << -x, 0;	//right arm
          M_desired_local.col(1) << -z.cross(-x),0;
        }
      Point_desired(0) = cyl_radius + 0.05;
      Point_desired(1) = 0;
      Point_desired(2) = cyl_height*0.5 + 0.05;	//da rivedere!!
      Point_desired(3) = 1;
      ROS_DEBUG("cyl dritto e vuoto");

      M_desired_local.col(2) << -z , 0;
      M_desired_local.col(3) << Point_desired;
      T_w_h = T_vito_c * M_desired_local*Rot_z ;

    }

  else if(((cyl_info == 0) && (cyl_v != 0)) && (cyl_radius< max_radius))
    {
      if (Arm == true) //left arm
        {
          M_desired_local.col(0) << x, 0;
          M_desired_local.col(1) << -z.cross(x),0;
        }

      else
        {
          M_desired_local.col(0) << -x, 0;	//right arm
          M_desired_local.col(1) << -z.cross(-x),0;
        }
      Point_desired(0) = 0;
      Point_desired(1) = 0;
      Point_desired(2) = cyl_height*0.5 + 0.05;; //da rivedere
      Point_desired(3) = 1;
      ROS_DEBUG("cyl dritto e pieno");
      M_desired_local.col(2) << -z, 0;
      M_desired_local.col(3) << Point_desired;
      T_w_h = T_vito_c * M_desired_local*Rot_z ;
    }

  else if ((cyl_info != 0) && (cyl_radius < max_radius))
    {
      if (Arm == true) //left arm
        {
          M_desired_local.col(0) << -z, 0;
          M_desired_local.col(1) << -y.cross(-z), 0;	//da rifare
        }

      else
        {
          M_desired_local.col(0) << z, 0;	//right arm
          M_desired_local.col(1) << -y.cross(z), 0;	//da rifare
        }
      Point_desired(0) = 0;
      Point_desired(1) = cyl_radius + 0.05;
      Point_desired(2) = 0; //da rivedere
      Point_desired(3) = 1;
      ROS_DEBUG("cyl piegato");
      // M_desired_local.col(1) << -y.cross(z), 0;	//da rifare
      M_desired_local.col(2) << -y, 0;
      M_desired_local.col(3) << Point_desired;
      T_w_h = T_vito_c * M_desired_local ;
    }
  else
    {
      ROS_INFO("caso non contemplato");
    }

  ROS_DEBUG("Set hand final position");
  geometry_msgs::Pose local_sh_pose;
  fromEigenToPose( T_w_h ,local_sh_pose);
  Hand_pose.push_back(local_sh_pose );

  //just for view in rzv
  tf::Transform local_tf_pos;
  tf::poseMsgToTF(local_sh_pose, local_tf_pos);
  int a =0;
  std::string sh= "hand_desired_pose" + std::to_string(a);
  tf_br.sendTransform(tf::StampedTransform(local_tf_pos, ros::Time::now(), base_frame_topic.c_str(), sh.c_str()));
  a++;
}

void phobic_hand::WhichArm(pcl::PointXYZ Pos)
{
  std::pair<double, pcl::PointXYZ> distance_HtO;
  double dist_lTo, dist_rTo;	//in kinect frame
  dist_lTo = GetDistance(Pos, Pos_HAND_l).first;
  dist_rTo = GetDistance(Pos, Pos_HAND_r).first;

  if(dist_lTo < dist_rTo)
    {
      Arm = true;
      ROS_DEBUG("Vito uses a: LEFT ARM");
    }
  else
    {
      Arm = false;
      ROS_DEBUG("Vito uses a: RIGHT ARM");
    }
}

std::pair<double, pcl::PointXYZ> phobic_hand::GetDistance(pcl::PointXYZ &obj1, pcl::PointXYZ &obj2 )
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


bool phobic_hand::objectORostacles()
{
  bool OrO;

  // if((cyl_radius.front() > max_radius) && (cyl_height.front() > max_lenght))
  if((cyl_radius > max_radius) && (cyl_height > max_lenght))
    {
      OrO= false;
      ROS_INFO("object is a obstacles");
    }
  else
    {
      OrO = true;
      ROS_INFO("object is a goal");
    }

  return OrO;
}



Eigen::Matrix4d FromTFtoEigen(tf::StampedTransform &object)
{	
  Eigen::Quaterniond transf_quad(object.getRotation().getW(),object.getRotation().getX(),object.getRotation().getY(),object.getRotation().getZ());
  transf_quad.normalize();
  Eigen::Vector3d translation(object.getOrigin().x(),object.getOrigin().y(),object.getOrigin().z());
  Eigen::Matrix3d rotation(transf_quad.toRotationMatrix());

  Eigen::Matrix4d Matrix_transf;

  Matrix_transf.row(0) << rotation.row(0), translation[0];
  Matrix_transf.row(1) << rotation.row(1), translation[1];
  Matrix_transf.row(2) << rotation.row(2), translation[2];
  Matrix_transf.row(3) << 0,0,0,1;

  return Matrix_transf;
}


pcl::PointXYZ phobic_hand::Take_Pos(tf::StampedTransform &M_tf)
{
  pcl::PointXYZ Pos_vito;
  Eigen::Matrix4d Link_eigen;

  Link_eigen = FromTFtoEigen(M_tf);

  Pos_vito.x = Link_eigen(0,3);
  Pos_vito.y = Link_eigen(1,3);
  Pos_vito.z = Link_eigen(2,3);

  return Pos_vito;
}

void phobic_hand::Send()
{ 	
  ROS_DEBUG("send pose hand msg");
  desperate_housewife::hand msg;

  ROS_DEBUG("Hand_pose.size() %lu", Hand_pose.size() );

  for (unsigned int i = 0; i < Hand_pose.size(); i++)
    {
      if(Test_obj == true) // if is object
        {
          if(Arm == true)//left
            {
              msg.whichArm.push_back(0);
            }

          else
            {
              msg.whichArm.push_back(1); //right
            }

          msg.hand_Pose.push_back(Hand_pose[i]);
        }
      else
        {
          msg.whichArm.push_back(2); //obstacles VA CAMBIATO IL FRAME ORA È IN CYL VA MESSO IN WORD
          //msg.hand_Pose.position.push_back(obstacle_position);
          Eigen::Vector4d ob_pos_word(0,0,0,0), local_ob_pos(0,0,0,0);
          local_ob_pos << obstacle_position.x, obstacle_position.y, obstacle_position.z, 0;
          ob_pos_word = T_K_VA_eigen * local_ob_pos;

          msg.hand_Pose[i].position.x = ob_pos_word[0];
          msg.hand_Pose[i].position.y = ob_pos_word[1];
          msg.hand_Pose[i].position.z = ob_pos_word[2];

        }

      // hand_info.publish(msg);
    }
  hand_info.publish(msg);
  msg.whichArm.clear();
  msg.hand_Pose.clear();
  Hand_pose.clear();
}



void phobic_hand::fromEigenToPose(Eigen::Matrix4d &tranfs_matrix, geometry_msgs::Pose &Hand_pose)
{
  Eigen::Matrix<double,3,3> Tmatrix;
  Tmatrix = tranfs_matrix.block<3,3>(0,0) ;
  Eigen::Quaterniond quat_eigen_hand(Tmatrix);
  Hand_pose.orientation.x = quat_eigen_hand.x();
  Hand_pose.orientation.y = quat_eigen_hand.y();
  Hand_pose.orientation.z = quat_eigen_hand.z();
  Hand_pose.orientation.w = quat_eigen_hand.w();
  Hand_pose.position.x = tranfs_matrix(0,3);
  Hand_pose.position.y = tranfs_matrix(1,3);
  Hand_pose.position.z = tranfs_matrix(2,3);

}
