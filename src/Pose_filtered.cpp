
#include <ros/ros.h>
#include <ros/console.h>
#include <hand_pose_filtered.h>

int main(int argc, char **argv)
{
  ros::init(argc, argv, "Hand_Pose_node");
  HandPoseFIltered node;
  ROS_INFO("[Hand_Pose_node] Node is ready");

  double spin_rate = 10;
  ros::param::get("~spin_rate",spin_rate);
  ROS_DEBUG( "Spin Rate %lf", spin_rate);

  ros::Rate rate(spin_rate); 

  while (node.nh.ok())
  {
    ros::spinOnce(); 
    rate.sleep();
  }
  ROS_ERROR("QUI");
  return 0;
}



HandPoseFIltered::HandPoseFIltered()
{
  nh.param<std::string>("/PotentialFieldControl/desired_hand_frame", desired_hand_frame_, "desired_hand_pose_filtered");
  nh.param<std::string>("/PotentialFieldControl/base_frame", base_frame_, "vito_anchor");
	sub_command_left = nh.subscribe("left_arm/PotentialFieldControl/desired_hand_left_filter", 1, &HandPoseFIltered::HandPoseFIlteredCallback_left, this);
  sub_command_right = nh.subscribe("right_arm/PotentialFieldControl/desired_hand_right_filter", 1, &HandPoseFIltered::HandPoseFIlteredCallback_right, this);

  nh.param<std::string>("/right_arm/PotentialFieldControl/desired_hand_right_pose", desired_hand_right_pose_topic_, "/right_arm/PotentialFieldControl/desired_hand_right_pose");
 	desired_hand_right_pose_publisher_ = nh.advertise<desperate_housewife::handPoseSingle > (desired_hand_right_pose_topic_.c_str(),1);

 	nh.param<std::string>("/left_arm/PotentialFieldControl/desired_hand_left_pose", desired_hand_left_pose_topic_, "/left_arm/PotentialFieldControl/desired_hand_left_pose");
 	desired_hand_left_pose_publisher_ = nh.advertise<desperate_housewife::handPoseSingle > (desired_hand_left_pose_topic_.c_str(),1);
}

void HandPoseFIltered::HandPoseFIlteredCallback_left(const desperate_housewife::handPoseSingle::ConstPtr& msg)
{
    
  Controll(msg);

}

void HandPoseFIltered::HandPoseFIlteredCallback_right(const desperate_housewife::handPoseSingle::ConstPtr& msg)
{
    
  Controll(msg);

}

void HandPoseFIltered::Controll(const desperate_housewife::handPoseSingle::ConstPtr& msg) 
{
    
    if(first_step == 0)
    {    
      if( msg->whichArm  == 1) //left
     {
        desired_hand_left_pose_publisher_.publish( msg );
     }
     else
     {
       desired_hand_right_pose_publisher_.publish( msg );
     } 
       first_step = 1;
    }

    else
    { 
      tf::poseMsgToKDL(msg->pose, pose_obj);
      if(dot_step == 0)
      {
        Pose_obj_stable.pose = msg->pose;
         pose_last = pose_obj;
         Pose_obj_stable.whichArm = msg->whichArm;
         Pose_obj_stable.isGraspable = msg->isGraspable;
         dot_step = 1;
      }
        double diff_pose = (diff(pose_last.p, pose_obj.p)).Norm();
        double diff_pose_rot = (diff(pose_last.M, pose_obj.M)).Norm();
        std::cout<<"diff_pose_rot: "<<diff_pose_rot<<std::endl;

        if(diff_pose < 0.02)
        {
         std::cout<<"pubblico"<<std::endl;
         if( Pose_obj_stable.whichArm  == 1) //left
         {
            desired_hand_left_pose_publisher_.publish( Pose_obj_stable );
         }
         else
         {
           desired_hand_right_pose_publisher_.publish( Pose_obj_stable );
         }
          tf::Transform tfHandTrasform;
          tf::poseMsgToTF( Pose_obj_stable.pose, tfHandTrasform);
          tf_desired_hand_pose.sendTransform( tf::StampedTransform( tfHandTrasform, ros::Time::now(), base_frame_.c_str(), desired_hand_frame_.c_str()) );
        }
        else
          dot_step = 0;

    }



    
    // if(first_step == 0)
    // {  	
    //   	Pose_obj_stable.pose = msg->pose;
    //   	pose_last = pose_obj;
    //   	Pose_obj_stable.whichArm = msg->whichArm;
    //   	Pose_obj_stable.isGraspable = msg->isGraspable;
    // 	  first_step = 1;
    // }

    // double diff_pose = (diff(pose_last.p, pose_obj.p)).Norm();
    // double diff_pose_rot = (diff(pose_last.M, pose_obj.M)).Norm();
    // std::cout<<"diff_pose_rot: "<<diff_pose_rot<<std::endl;

    // if(diff_pose < 0.02)
    // {
    // 	std::cout<<"pubblico"<<std::endl;
    // 	if(	Pose_obj_stable.whichArm  == 1) //left
    // 	{
	 		//   desired_hand_left_pose_publisher_.publish( Pose_obj_stable );
    // 	}
    // 	else
    // 	{
    // 		desired_hand_right_pose_publisher_.publish( Pose_obj_stable );
    // 	}

    //   tf::Transform tfHandTrasform;
    //       tf::poseMsgToTF( Pose_obj_stable.pose, tfHandTrasform);
    //       tf_desired_hand_pose.sendTransform( tf::StampedTransform( tfHandTrasform, ros::Time::now(), base_frame_.c_str(), desired_hand_frame_.c_str()) );
   	
   	// }
   	// else
   	// {
   	// 	first_step = 0;
   	// 	std::cout<<"mi muovo"<<std::endl;
   	// }
}