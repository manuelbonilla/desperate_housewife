#include "HandPoseGenerator_state.h"
#include <place_hand_dany.hpp>
#include <getClosestObject.hpp>

HandPoseGenerator::HandPoseGenerator(shared& m):data(m)
{
    nh.param<int>("/demo", demo, 0);
    nh.param<int>("/max_number_obj", Number_obj, 5);

    /*reads geometry information*/
    nh.param<std::string>("/BasicGeometriesNode/geometries_topic", geometries_topic_, "/BasicGeometriesNode/geometries");
    stream_subscriber_ = nh.subscribe(geometries_topic_, 1, &HandPoseGenerator::HandPoseGeneratorCallback, this);
    
    /*sends obstacle informations*/
    std::string string_temp_obst_right; 
    nh.param<std::string>("/right_arm/PotentialFieldControl/topic_obstacle", string_temp_obst_right, "obstacles");
    obstacles_topic_right = std::string("/right_arm/PotentialFieldControl/") + string_temp_obst_right;
    obstacles_publisher_right = nh.advertise<desperate_housewife::fittedGeometriesArray > (obstacles_topic_right.c_str(),1);

    std::string string_temp_obst_left; 
    nh.param<std::string>("/left_arm/PotentialFieldControl/topic_obstacle", string_temp_obst_left, "obstacles");
    obstacles_topic_left = std::string("/left_arm/PotentialFieldControl/") + string_temp_obst_left;
    obstacles_publisher_left = nh.advertise<desperate_housewife::fittedGeometriesArray > (obstacles_topic_left.c_str(),1);

    /*sends information about the remove or grasp objects */
    nh.param<std::string>("/right_arm/objects_info", obj_info_topic_r, "/right_arm/objects_info");
    objects_info_right_pub = nh.advertise<std_msgs::UInt16 > (obj_info_topic_r.c_str(),1, this);
    nh.param<std::string>("/left_arm/PotentialFieldControl/objects_info_l", obj_info_topic_l, "/left_arm/objects_info");
    objects_info_left_pub = nh.advertise<std_msgs::UInt16 > (obj_info_topic_l.c_str(),1, this);

    /*config parameteres*/
    nh.param<std::string>("/right_arm/PotentialFieldControl/tip_name", right_hand_frame_ , "right_hand_palm_ref_link");
    nh.param<std::string>("/right_arm/PotentialFieldControl/root_name", base_frame_, "world");
    nh.param<std::string>("/left_arm/PotentialFieldControl/tip_name", left_hand_frame_, "left_hand_palm_ref_link");
     
    /*sends hand pose*/
    std::string string_temp_right;  
    nh.param<std::string>("/right_arm/PotentialFieldControl/topic_desired_reference", string_temp_right, "command");
    desired_hand_pose_right_topic_ = std::string("/right_arm/PotentialFieldControl/") + string_temp_right;
    desired_hand_publisher_right = nh.advertise<desperate_housewife::handPoseSingle > (desired_hand_pose_right_topic_.c_str(),1);

    std::string string_temp_left;  
    nh.param<std::string>("/left_arm/PotentialFieldControl/topic_desired_reference", string_temp_left, "command");
    desired_hand_pose_left_topic_ = std::string("/left_arm/PotentialFieldControl/") + string_temp_left;
    desired_hand_publisher_left = nh.advertise<desperate_housewife::handPoseSingle > (desired_hand_pose_left_topic_.c_str(),1);

    // nh.param<bool>("/use_both_arm", use_both_arm, true);
     nh.param<double>("/SoftHandDistanceFrame", SoftHandDistanceFrame, 0.2);

  	id_class = static_cast<int>(transition_id::Gen_pose);
    // id_arm = 3;

    ROS_INFO("HandPoseGenerator: %d", id_class);

    /*read the error*/
  	nh.param<std::string>("/right_arm/PotentialFieldControl/error_id", error_topic_right, "/right_arm/PotentialFieldControl/error_id");
    error_sub_right = nh.subscribe(error_topic_right, 1,&HandPoseGenerator::Error_info_right, this);

    nh.param<std::string>("/left_arm/PotentialFieldControl/error_id", error_topic_left, "/left_arm/PotentialFieldControl/error_id");
    error_sub_left = nh.subscribe(error_topic_left, 1, &HandPoseGenerator::Error_info_left, this);

    /*send the msg to close hand*/
    nh.param<std::string>("/right_hand/joint_trajectory_controller/command", hand_close_right, "/right_hand/joint_trajectory_controller/command");
    hand_publisher_right = nh.advertise<trajectory_msgs::JointTrajectory>(hand_close_right.c_str(), 1000);

    nh.param<std::string>("/left_hand/joint_trajectory_controller/command", hand_close_left, "/left_hand/joint_trajectory_controller/command");
    hand_publisher_left = nh.advertise<trajectory_msgs::JointTrajectory>(hand_close_left.c_str(), 1000);

    pub_projection = nh.advertise<visualization_msgs::Marker>("projection", 10);


    
    marker_publisher_ = nh.advertise<visualization_msgs::Marker >( "obstacles_in_control", 1 );
    /*treshold error*/
  	double x,y,z,rotx,roty,rotz;
  
    nh.param<double>("/error/pos/x",x,0.01);
    nh.param<double>("/error/pos/y",y,0.01);
    nh.param<double>("/error/pos/z",z,0.01);
    nh.param<double>("/error/rot/x",rotx,0.01);
    nh.param<double>("/error/rot/y",roty,0.01);
    nh.param<double>("/error/rot/z",rotz,0.01);

    KDL::Vector vel;
    KDL::Vector rot;
    vel.data[0] = x;
    vel.data[1] = y;
    vel.data[2] = z;
    rot.data[0] = rotx;
    rot.data[1] = roty;
    rot.data[2] = rotz;
    E_t.vel = vel;
    E_t.rot = rot;
    Arm_r = false;
    Arm_l = false;
    finish = false;
    failed = false;
    /*to initialize the error*/
    vect_error.resize(2);
    KDL::Twist temp;
    SetToZero(temp);
    vect_error[0] = temp;
    vect_error[1] = temp;
    send_msg = 0;
}


void HandPoseGenerator::Error_info_right(const desperate_housewife::Error_msg::ConstPtr& error_msg)
{
    tf::twistMsgToKDL (error_msg->error_, e_r);

    id_msgs = error_msg->id;
    id_arm_msg = error_msg->id_arm;
    vect_error[0] = e_r;
    Arm_r = true;
}


void HandPoseGenerator::Error_info_left(const desperate_housewife::Error_msg::ConstPtr& error_msg)
{
    tf::twistMsgToKDL (error_msg->error_, e_l);

    id_msgs = error_msg->id;
    id_arm_msg = error_msg->id_arm;
    vect_error[1] = e_l;
    Arm_l = true;
}



void HandPoseGenerator::HandPoseGeneratorCallback(const desperate_housewife::fittedGeometriesArray::ConstPtr& msg)
{
    cylinder_geometry.geometries.resize(msg->geometries.size());

  	for(unsigned int i = 0; i < msg->geometries.size(); i++ )
  	{
  		cylinder_geometry.geometries[i] = msg->geometries[i];
  	}
}

std::map< transition, bool > HandPoseGenerator::getResults()
{

	std::map< transition, bool > results;
	
	if((finish == true) && (failed == false))
	{
		results[transition::Error_arrived] = true;
    finish = false;
	}
  if((failed == true) && (finish == true))
  {
    results[transition::failed] = true;
    finish = false;
    send_msg = 0;
  }
  return results;
}

bool HandPoseGenerator::isComplete()
{
    return finish;
}

std::string HandPoseGenerator::get_type()
{
    return "HandPoseGenerator";
}


void HandPoseGenerator::run()
{
    // std::cout<<"start controller"<<std::endl;
    ROS_DEBUG("start controller");
    desperate_housewife::handPoseSingle DesiredHandPose;
      
    // DesiredHandPose.home = 0;
    std_msgs::UInt16 Obj_info;  /*msg for desperate_mind with object's information*/

    desperate_housewife::fittedGeometriesArray obstaclesMsg;
    
    e_ = vect_error[0] + vect_error[1]; //sum of error norma uno

    if((id_msgs != id_class) && IsEqual(e_) && (send_msg == 0))
  	{
      if ( cylinder_geometry.geometries.size() == 1)
	    {   send_msg = 1;   
	        DesiredHandPose = generateHandPose( cylinder_geometry.geometries[0], 0 );

	          /*check if is graspable (send hand desired pose) or not (remove object)*/
	        if(DesiredHandPose.isGraspable != true)
	        {
		            ROS_DEBUG("Object to Reject");
		            DesiredHandPose.pose = ObstacleReject(cylinder_geometry.geometries[0] , DesiredHandPose.whichArm);
		            ObjorObst = 1;
	        }

	        else
	        {
		          	ObjorObst = 0; /*flag for desperate_mind code.*/
		            ROS_DEBUG("Graspable objects");
	        }  

          data.arm_to_use = DesiredHandPose.whichArm;
          if(DesiredHandPose.whichArm == 1)
          {
              DesiredHandPose.id = id_class;
              desired_hand_publisher_left.publish( DesiredHandPose );
              finish = false;
          }
          else
          {
              DesiredHandPose.id = id_class;
              desired_hand_publisher_right.publish( DesiredHandPose );
              finish = false;
          }
         
	    }
      else if (static_cast<int>(cylinder_geometry.geometries.size()) >= Number_obj)
      { 
        if ((Arm_l == true) && (Arm_r == true))
          Overturn();
        else
        {
            failed = true;
            ROS_ERROR("There are more than: %d ", Number_obj);
        }
      }
      else
      {
          std::vector< desperate_housewife::fittedGeometriesSingle > objects_vec;
          send_msg = 1;   
          desperate_housewife::fittedGeometriesArray obstaclesMsg;
          std_msgs::UInt16 Obj_info;
         
          for (unsigned int i=0; i< cylinder_geometry.geometries.size(); i++)
          {
              objects_vec.push_back(cylinder_geometry.geometries[i]);
              objects_vec[i].id = i;
          }
          
          /* sort the cylinder by the shortes distance from softhand */
          // std::sort(objects_vec.begin(), objects_vec.end(), [](desperate_housewife::fittedGeometriesSingle first, desperate_housewife::fittedGeometriesSingle second) {
          //       double distfirst = std::sqrt( first.pose.position.x*first.pose.position.x + first.pose.position.y*first.pose.position.y + first.pose.position.z*first.pose.position.z);
          //       double distsecond = std::sqrt( second.pose.position.x*second.pose.position.x + second.pose.position.y*second.pose.position.y + second.pose.position.z*second.pose.position.z);
          //       return (distfirst > distsecond); });

          std::vector< desperate_housewife::fittedGeometriesSingle > objects_vec_sorted = getClosestObject( objects_vec);


          switch(demo)
          {
            case 0:
            {
              DesperateDemo1(objects_vec_sorted);
              break;
            }
            case 1:
            {
              DesperateDemo2(objects_vec_sorted);
              break;
            } 
          }       
      }
  	}
  	else if((id_msgs == id_class)  && (IsEqual(e_)))
    {
          // ROS_INFO("Sens msgs to whait_msg_state");
      		Obj_info.data = ObjorObst;
      		objects_info_right_pub.publish(Obj_info);
      		finish = true;
          send_msg = 0;
          failed = false;
  	}
    else if((id_msgs == id_class) && (!IsEqual(e_)))
    finish = false;
}
  

desperate_housewife::handPoseSingle HandPoseGenerator::generateHandPose( desperate_housewife::fittedGeometriesSingle geometry, int cyl_nbr )
{
  desperate_housewife::handPoseSingle hand_pose_local;
  // hand_pose_local.obj = 1;

  if ( isGeometryGraspable ( geometry ))
  {
    hand_pose_local.whichArm = whichArm( geometry.pose , cyl_nbr);
    // ROS_INFO_STREAM("Using Arm : "<< (hand_pose_local.whichArm == 0 ? "right" : "left") );
    hand_pose_local.pose = placeHand( geometry, hand_pose_local.whichArm );
    hand_pose_local.isGraspable = true;
  }
  else
  {
    hand_pose_local.pose = geometry.pose;
    hand_pose_local.isGraspable = false;
    hand_pose_local.whichArm = whichArm( geometry.pose, cyl_nbr );
  }

  return hand_pose_local;
}

bool HandPoseGenerator::isGeometryGraspable ( desperate_housewife::fittedGeometriesSingle geometry )
{
  /*comparision between ration and treshold and cylinder radius with another threshold*/
  if (geometry.info[geometry.info.size() - 1] >=55 && geometry.info[0] < 0.10)
  {
    return true;
  }
  
  return false;
}


bool HandPoseGenerator::IsEqual(KDL::Twist E_pf)
{
    KDL::Twist E_pf_abs;
    // std::cout<<"IsEqual"<<std::endl;
    E_pf_abs.vel.data[0] = std::abs(E_pf.vel.data[0] );
    E_pf_abs.vel.data[1] = std::abs(E_pf.vel.data[1] );
    E_pf_abs.vel.data[2] = std::abs(E_pf.vel.data[2] );
    E_pf_abs.rot.data[0] = std::abs(E_pf.rot.data[0] );
    E_pf_abs.rot.data[1] = std::abs(E_pf.rot.data[1] );
    E_pf_abs.rot.data[2] = std::abs(E_pf.rot.data[2] );


    if (  (E_pf_abs.vel.data[0] < E_t.vel.data[0]) &&
          (E_pf_abs.vel.data[1] < E_t.vel.data[1]) &&
          (E_pf_abs.vel.data[2] < E_t.vel.data[2]) &&
          (E_pf_abs.rot.data[0] < E_t.rot.data[0]) &&
          (E_pf_abs.rot.data[1] < E_t.rot.data[1]) &&
          (E_pf_abs.rot.data[2] < E_t.rot.data[2]) )
    {
       // ROS_DEBUG("is equal");
      return true;
    }
    else
    {
      // ROS_DEBUG("is not equal");
      // ROS_DEBUG("error linear: E_pf_abs.vel.data[0] %g E_pf_abs.vel.data[1] %g  E_pf_abs.vel.data[2]: %g",  E_pf_abs.vel.data[0],E_pf_abs.vel.data[1], E_pf_abs.vel.data[2]);
      // ROS_DEBUG("error agular: E_pf_abs.rot.data[0] %g E_pf_abs.rot.data[1] %g E_pf_abs.rot.data[2] %g", E_pf_abs.rot.data[0], E_pf_abs.rot.data[1], E_pf_abs.rot.data[2]);
      return false;
    }
}

void HandPoseGenerator::DesperateDemo1( std::vector<desperate_housewife::fittedGeometriesSingle> cyl_)
{
      ROS_INFO("*** DEMO 0, take first graspable object with obstacle avoidance ***");
          
      desperate_housewife::fittedGeometriesArray obstaclesMsg;
      
      int obj_grasp = 0;
      
      if(cyl_.size() == 0)
      {
        failed = true;
        finish = true;
        ROS_INFO("No objects on table");
      }
      else 
      {   
        /*find the first graspagle geometry */   
        for(unsigned int k = 0; k < cyl_.size(); k++)
        {
            desperate_housewife::handPoseSingle DesiredHandPose;
            desperate_housewife::fittedGeometriesSingle obstacle;
            failed = false;
            DesiredHandPose = generateHandPose( cyl_[k], cyl_[k].id );

            if(DesiredHandPose.isGraspable != true)
            {
                desperate_housewife::fittedGeometriesSingle pos_obj_temp1;
                pos_obj_temp1.pose = cyl_[k].pose;
                    
                for(unsigned int h=0; h < cyl_[k].info.size(); h++)
                {
                    pos_obj_temp1.info.push_back( cyl_[k].info[h]);
                }

                obstaclesMsg.geometries.push_back( pos_obj_temp1);
                generateMarkerMessages(pos_obj_temp1,1,k);
            }

            else
            {
                ObjorObst = 0;
                obj_grasp = obj_grasp + 1;
                ROS_DEBUG("Graspable objects");
                generateMarkerMessages( cyl_[k],0,10);
                /*send all other cylinders like obstalcle*/
                for (unsigned int i_ = k + 1; i_ <  cyl_.size(); i_++ )
                {                   
                    desperate_housewife::fittedGeometriesSingle pos_obj_temp;
                    pos_obj_temp.pose =  cyl_[i_].pose;
                  
                    for(unsigned int h=0; h <  cyl_[i_].info.size(); h++)
                    {
                      pos_obj_temp.info.push_back(  cyl_[i_].info[h]);
                    }

                    obstaclesMsg.geometries.push_back( pos_obj_temp);
                    generateMarkerMessages(pos_obj_temp,1,i_);
                }
              
                data.arm_to_use = DesiredHandPose.whichArm;
                
                if (DesiredHandPose.whichArm == 1) /*left arm*/
                {
                    DesiredHandPose.id = id_class;
                    desired_hand_publisher_left.publish( DesiredHandPose );
                    obstacles_publisher_left.publish(obstaclesMsg);
                    finish = false;
                } 

                else /*right arm*/
                {
                  DesiredHandPose.id = id_class;
                  desired_hand_publisher_right.publish( DesiredHandPose );
                  obstacles_publisher_right.publish(obstaclesMsg);
                  finish = false;
                }

                break;
            }
        }
   
        /*if there aren't graspable object call the funciont overtun */
        if((obj_grasp == 0) && (Arm_l == true) && (Arm_r == true))
        {
          Overturn(); 
          finish = false;
        }
      }


      
   
}

void  HandPoseGenerator::DesperateDemo2(std::vector<desperate_housewife::fittedGeometriesSingle> cyl_)
{
      ROS_INFO("*** DEMO1, take first graspable object without obstacles avoidance ***");

      desperate_housewife::handPoseSingle DesiredHandPose;
      
      if (cyl_.size() > 0)
      {
        DesiredHandPose = generateHandPose( cyl_[0], cyl_[0].id );
 
        if (!DesiredHandPose.isGraspable )
        {
              ROS_DEBUG("Object to Reject");
              DesiredHandPose.pose = ObstacleReject(cyl_[0], DesiredHandPose.whichArm);
              ObjorObst = 1; 
              // ROS_INFO("TO remove");
        }
        else
        {
              ROS_DEBUG("Graspable objects");
              ObjorObst = 0; //flag to grasp object in the desperate_mind code
              // ROS_INFO("TO GRASP");
        }

        data.arm_to_use = DesiredHandPose.whichArm;
          
        if (DesiredHandPose.whichArm == 1) 
        {
              DesiredHandPose.id = id_class;
              desired_hand_publisher_left.publish(DesiredHandPose);
              finish = false;
        }
        else
        {
          DesiredHandPose.id = id_class;
          desired_hand_publisher_right.publish(DesiredHandPose);
          finish = false;
        }
      }
      else
      {
        failed = true;
        finish = true;
        // send_msg = 0;
        ROS_INFO("No objects on table");
      }

}




void HandPoseGenerator::generateMarkerMessages( desperate_housewife::fittedGeometriesSingle cylMsg, int obstorobj, int  i)
{
    visualization_msgs::Marker marker;
    std::string obst_name;

    marker.header.frame_id = "world";
    marker.header.stamp = ros::Time();
    marker.ns = "";
    marker.id = i;
    marker.type = visualization_msgs::Marker::CYLINDER;
    marker.action = visualization_msgs::Marker::ADD;
    
    marker.pose.position =cylMsg.pose.position;
    marker.pose.orientation =cylMsg.pose.orientation;

    marker.scale.x =cylMsg.info[0]*2;
    marker.scale.y =cylMsg.info[0]*2;
    marker.scale.z =cylMsg.info[1];

    marker.color.a = 1.0; // for the clearness
    
    if(obstorobj == 1) //ostacles
    {
        obst_name= "obstacle_" + std::to_string(i);
        marker.color.r = 1.0;
        marker.color.g = 0.0;
        marker.color.b = 0.0;
    }
    else
    {
        obst_name= "to_grasp_" ;
        marker.color.r = 0.0;
        marker.color.g = 1.0;
        marker.color.b = 0.5;
    }
   
    marker.lifetime = ros::Duration(100);
    marker_publisher_.publish(marker); 
    tf::Transform tfGeomTRansform;
    tf::poseMsgToTF(cylMsg.pose, tfGeomTRansform );
    tf_geometriesTransformations_.sendTransform( tf::StampedTransform( tfGeomTRansform, ros::Time::now(), "world", obst_name.c_str()) );
 
}
