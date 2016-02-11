#include "HandPoseGenerator_state.h"
#include <place_hand_dany.hpp>

HandPoseGenerator::HandPoseGenerator()
{
  nh.param<int>("/demo", demo, 0);
  nh.param<int>("/max_number_obj", Number_obj, 5);

  /*reads geometry information*/
  nh.param<std::string>("/BasicGeometriesNode/geometries_topic", geometries_topic_, "/BasicGeometriesNode/geometries");
  stream_subscriber_ = nh.subscribe(geometries_topic_, 1, &HandPoseGenerator::HandPoseGeneratorCallback, this);
  /*sends obstacle informations*/
  // nh.param<std::string>("/right_arm/PotentialFieldControl/topic_obstacle", obstacles_topic_left, "/right_arm/PotentialFieldControl/obstacles");
  // obstacles_publisher_left = nh.advertise<desperate_housewife::fittedGeometriesArray > (obstacles_topic_left.c_str(),1);

  std::string string_temp_o;
    
  nh.param<std::string>("/right_arm/PotentialFieldControl/topic_obstacle", string_temp_o, "obstacles");
  obstacles_topic_right = std::string("/right_arm/PotentialFieldControl/") + string_temp_o;

  // nh.param<std::string>("/right_arm/PotentialFieldControl/topic_obstacle", obstacles_topic_right, "/right_arm/PotentialFieldControl/obstacles");
  obstacles_publisher_right = nh.advertise<desperate_housewife::fittedGeometriesArray > (obstacles_topic_right.c_str(),1);

  /*sends information about the remove or grasp objects */
  nh.param<std::string>("/right_arm/objects_info", obj_info_topic_r, "/right_arm/objects_info");
  objects_info_right_pub = nh.advertise<std_msgs::UInt16 > (obj_info_topic_r.c_str(),1, this);
  // nh.param<std::string>("/PotentialFieldControl/objects_info_l", obj_info_topic_l, "/right_arm/objects_info");
  // objects_info_left_pub = nh.advertise<std_msgs::UInt16 > (obj_info_topic_l.c_str(),1, this);

  /*config parameteres*/
  nh.param<std::string>("/right_arm/PotentialFieldControl/tip_name", right_hand_frame_ , "right_hand_palm_ref_link");
  nh.param<std::string>("/right_arm/PotentialFieldControl/root_name", base_frame_, "world");
  // nh.param<std::string>("/PotentialFieldControl/left_hand_frame", left_hand_frame_, "left_hand_palm_ref_link");
  // nh.param<std::string>("/right_arm/PotentialFieldControl/right_hand_frame", right_hand_frame_, "right_hand_palm_ref_link");
   
  /*sends hand pose*/
  // nh.param<std::string>("/right_arm/PotentialFieldControl/topic_desired_reference", desired_hand_pose_right_topic_, "/right_arm/PotentialFieldControl/command");
  // desired_hand_publisher_right = nh.advertise<desperate_housewife::handPoseSingle > (desired_hand_pose_right_topic_.c_str(),1);

  std::string string_temp;
    
  nh.param<std::string>("/right_arm/PotentialFieldControl/topic_desired_reference", string_temp, "command");
  desired_hand_pose_right_topic_ = std::string("/right_arm/PotentialFieldControl/") + string_temp;

  desired_hand_publisher_right = nh.advertise<desperate_housewife::handPoseSingle > (desired_hand_pose_right_topic_.c_str(),1);


  // nh.param<std::string>("/left_arm/PotentialFieldControl/command", desired_hand_pose_left_topic_, "/left_arm/PotentialFieldControl/command");
  // desired_hand_publisher_left = nh.advertise<desperate_housewife::handPoseSingle > (desired_hand_pose_left_topic_.c_str(),1);

  nh.param<bool>("/use_both_arm", use_both_arm, true);

	id_class = static_cast<int>(transition_id::Gen_pose);

  ROS_INFO("HandPoseGenerator: %d", id_class);

	nh.param<std::string>("/right_arm/PotentialFieldControl/error_id", error_topic_right, "/right_arm/PotentialFieldControl/error_id");
  error_sub_right = nh.subscribe(error_topic_right, 1, &HandPoseGenerator::Error_info_right, this);


  	double x,y,z,rotx,roty,rotz;
  /*treshold error*/
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

    finish = false;
    step = 1;
    stop = 0;

}




void HandPoseGenerator::Error_info_right(const desperate_housewife::Error_msg::ConstPtr& error_msg)
{
    tf::twistMsgToKDL (error_msg->error_, e_);

    id_msgs = error_msg->id;
    // std::cout<<"error: "<<error_msg->data[0]<<error_msg->data[1]<<error_msg->data[2]<<std::endl;
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
	
	if(finish == true)
	{
		results[transition::Error_arrived] = true;
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
      
    DesiredHandPose.home = 0;
    std_msgs::UInt16 Obj_info;  /*msg for desperate_mind with object's information*/

    desperate_housewife::fittedGeometriesArray obstaclesMsg;

    // if(step == 1)
    if((id_msgs != id_class))
  	{
      if ( cylinder_geometry.geometries.size() == 1)
	    {
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
		          	ObjorObst = 0;
		            // Obj_info.data = 0;  /*flag for desperate_mind code.*/
		            ROS_DEBUG("Graspable objects");
	        }  
	        
	        DesiredHandPose.id = id_class;
	        desired_hand_publisher_right.publish( DesiredHandPose );
	        finish = false;
	        // step = 0;

	        /*to show with rviz   */ 
	        tf::Transform tfHandTrasform;
	        tf::poseMsgToTF( DesiredHandPose.pose, tfHandTrasform);
	        tf_desired_hand_pose.sendTransform( tf::StampedTransform( tfHandTrasform, ros::Time::now(), base_frame_.c_str(), right_hand_frame_ .c_str()) );
	    }
      else
      {
        DesperateDemo1(cylinder_geometry);
        
      }
  	}
  	else
  	{ 
  	    if((id_msgs == id_class) && (IsEqual(e_)))
      	{
          std::cout<<"same id send mes"<<std::endl;
      		Obj_info.data = ObjorObst;
      		objects_info_right_pub.publish(Obj_info);
      		finish = true;
      		stop = 0;
      		
  	    }
  	}
	
  //     /*if there are more than a user defined number of object */
  //     else if (msg->geometries.size() >= (uint) Number_obj )
  //     {
  //     	ObjorObst = 2;
  //       Overturn(); //da finire
  //       finish = true;
  //       // Obj_info.data = 2;
  //       // objects_info_left_pub.publish(Obj_info);
  //       // objects_info_right_pub.publish(Obj_info);
  //       // stop = 1;
  //     }
  //     else
  //     {      
  //       switch(demo)
  //       {
  //         case 0: 
  //          DesperateDemo1(msg); /*take graspable object with obstacle avoidance*/
  //          break;
  //         case 1:
  //          DesperateDemo2(msg); /*take graspable object with removing the obstalce */
  //          break;
  //         case 2:
  //           ROS_ERROR("IMPOSSIBLE DEMO.. Demo does not exist");
  //           break;
  //       }
  //     }
  //   }
  //   /*until the robot doesn't arrived at home stay still.*/
  //   else
  //     return;
       
  // }
}
  

desperate_housewife::handPoseSingle HandPoseGenerator::generateHandPose( desperate_housewife::fittedGeometriesSingle geometry, int cyl_nbr )
{
  desperate_housewife::handPoseSingle hand_pose_local;
  hand_pose_local.obj = 1;

  if ( isGeometryGraspable ( geometry ))
  {
    hand_pose_local.whichArm = whichArm( geometry.pose , cyl_nbr);
    // std::cout<<"^^^^^^^hand_pose_local.whichArm^^^^ : "<<hand_pose_local.whichArm <<std::endl;
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
       ROS_DEBUG("is equal");
      return true;
    }
    else
    {
      ROS_DEBUG("is not equal");
      ROS_DEBUG("error linear: E_pf_abs.vel.data[0] %g E_pf_abs.vel.data[1] %g  E_pf_abs.vel.data[2]: %g",  E_pf_abs.vel.data[0],E_pf_abs.vel.data[1], E_pf_abs.vel.data[2]);
      ROS_DEBUG("error agular: E_pf_abs.rot.data[0] %g E_pf_abs.rot.data[1] %g E_pf_abs.rot.data[2] %g", E_pf_abs.rot.data[0], E_pf_abs.rot.data[1], E_pf_abs.rot.data[2]);
      return false;
    }
}

void HandPoseGenerator::DesperateDemo1(desperate_housewife::fittedGeometriesArray msg)
{
    if(stop == 0)
    {
      ROS_INFO("***DEMO1, take first graspable object with obstacle avoidance***");
      std::vector< desperate_housewife::fittedGeometriesSingle > objects_vec;
          
      desperate_housewife::fittedGeometriesArray obstaclesMsg;
      std_msgs::UInt16 Obj_info;
      
      for (unsigned int i=0; i< msg.geometries.size(); i++)
      {
          objects_vec.push_back(msg.geometries[i]);
      }
      
      /* sort the cylinder by the shortes distance from softhand */
      std::sort(objects_vec.begin(), objects_vec.end(), [](desperate_housewife::fittedGeometriesSingle first, desperate_housewife::fittedGeometriesSingle second) {
            double distfirst = std::sqrt( first.pose.position.x*first.pose.position.x + first.pose.position.y*first.pose.position.y + first.pose.position.z*first.pose.position.z);
            double distsecond = std::sqrt( second.pose.position.x*second.pose.position.x + second.pose.position.y*second.pose.position.y + second.pose.position.z*second.pose.position.z);
            return (distfirst < distsecond); });

        int obj_grasp = 0;
           
      /*find the first graspagle geometry */   
      for(unsigned int k=0; k < objects_vec.size(); k++)
      {
          desperate_housewife::handPoseSingle DesiredHandPose;
          desperate_housewife::fittedGeometriesSingle obstacle;

          DesiredHandPose = generateHandPose( objects_vec[k], k );

          if(DesiredHandPose.isGraspable != true)
          {
            
              desperate_housewife::fittedGeometriesSingle pos_obj_temp1;
              pos_obj_temp1.pose = objects_vec[k].pose;
                  
              for(unsigned int h=0; h < objects_vec[k].info.size(); h++)
              {
                  pos_obj_temp1.info.push_back( objects_vec[k].info[h]);
              }

              obstaclesMsg.geometries.push_back( pos_obj_temp1);
          }

          else
          {
              Obj_info.data = 0;
              obj_grasp = obj_grasp + 1;
              ROS_DEBUG("Graspable objects");

              /*send all other cylinders like obstalcle*/
              for (unsigned int i_ = k + 1; i_ < objects_vec.size(); i_++ )
              {                   
                  desperate_housewife::fittedGeometriesSingle pos_obj_temp;
                  pos_obj_temp.pose = objects_vec[i_].pose;
                
                  for(unsigned int h=0; h < objects_vec[i_].info.size(); h++)
                  {
                    pos_obj_temp.info.push_back( objects_vec[i_].info[h]);
                    // std::cout<<"objects_vec[k].info[0]: "<<objects_vec[h].info[0]<<std::endl;
                    // std::cout<<"objects_vec[k].info[1]: "<<objects_vec[h].info[1]<<std::endl;
                  }

                  obstaclesMsg.geometries.push_back( pos_obj_temp);
              }
                 
              // if (DesiredHandPose.whichArm == 1) /*left arm*/
              // {
              //     desired_hand_publisher_left.publish( DesiredHandPose );
              //     obstacles_publisher_left.publish(obstaclesMsg);
              //     objects_info_left_pub.publish(Obj_info);
              //     stop = 1;
              // } 

              // else right arm
              // {
              DesiredHandPose.id = id_class;
              desired_hand_publisher_right.publish( DesiredHandPose );
              obstacles_publisher_right.publish(obstaclesMsg);
              finish = false;
                  // objects_info_right_pub.publish(Obj_info);
                  stop = 1;    
              // }
              // ROS_INFO("pubblico tf");
              // tf::Transform tfHandTrasform;
              // tf::poseMsgToTF( DesiredHandPose.pose, tfHandTrasform);
              // tf_desired_hand_pose.sendTransform( tf::StampedTransform( tfHandTrasform, ros::Time::now(), base_frame_.c_str(), right_hand_frame_ .c_str()) );
            
              break;
          }
      }

      /*if there aren't graspable object call the funciont overtun */
      if(obj_grasp == 0)
      {
        Overturn(); 
        finish = false;
      }
    }
    else
      finish = false;
}




// void  HandPoseGenerator::DesperateDemo2(const desperate_housewife::fittedGeometriesArray::ConstPtr& msg)
// {
//     ROS_INFO("***DEMO2, take first graspable without obstacles avoidance***");
//     std::vector< desperate_housewife::fittedGeometriesSingle > objects_vec;
//     desperate_housewife::handPoseSingle DesiredHandPose;
//     desperate_housewife::fittedGeometriesSingle obstacle;
//     desperate_housewife::fittedGeometriesArray obstaclesMsg;
//     std_msgs::UInt16 Obj_info;
    
//     for (unsigned int i=0; i< msg->geometries.size(); i++)
//     {
//         objects_vec.push_back(msg->geometries[i]);
//     }

//      // std::vector< desperate_housewife::fittedGeometriesSingle > vect_sort = SortCylinder();
    
//     /*sort the cylinder by the shortes distance from softhand */
//     std::sort(objects_vec.begin(), objects_vec.end(), [](desperate_housewife::fittedGeometriesSingle first, desperate_housewife::fittedGeometriesSingle second) {
//         double distfirst = std::sqrt( first.pose.position.x*first.pose.position.x + first.pose.position.y*first.pose.position.y + first.pose.position.z*first.pose.position.z);
//         double distsecond = std::sqrt( second.pose.position.x*second.pose.position.x + second.pose.position.y*second.pose.position.y + second.pose.position.z*second.pose.position.z);
//         return (distfirst > distsecond); });

//     DesiredHandPose = generateHandPose( objects_vec[0], 0 );
//     if (!DesiredHandPose.isGraspable )
//       {
//           ROS_DEBUG("Object to Reject");
//           DesiredHandPose.pose = ObstacleReject(objects_vec[0], DesiredHandPose.whichArm);
//           Obj_info.data = 1; 
//           tf::Transform tfHandTrasform2;
//           tf::poseMsgToTF( DesiredHandPose.pose, tfHandTrasform2); 
//           tf_desired_hand_pose.sendTransform( tf::StampedTransform( tfHandTrasform2, ros::Time::now(), base_frame_.c_str(),"ObstacleReject") );
//       }
//     else
//     {
//           ROS_DEBUG("Graspable objects");
//           Obj_info.data = 0; //flag to grasp object in the desperate_mind code
//           tf::Transform tfHandTrasform;
//           tf::poseMsgToTF( DesiredHandPose.pose, tfHandTrasform);
//           tf_desired_hand_pose.sendTransform( tf::StampedTransform( tfHandTrasform, ros::Time::now(), base_frame_.c_str(), right_hand_frame_ .c_str()) ); 
//     }

//       if (DesiredHandPose.whichArm == 1) 
//       {
//           desired_hand_publisher_left.publish(DesiredHandPose);
//           stop = 1; /* flag to stop this procedure */
//           objects_info_left_pub.publish(Obj_info);
//       }
//       else
//       {
//           desired_hand_publisher_right.publish(DesiredHandPose);
//           stop = 1;
//           objects_info_right_pub.publish(Obj_info);
//       }


// }