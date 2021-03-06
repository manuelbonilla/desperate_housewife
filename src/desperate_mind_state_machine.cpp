#include <desperate_State_machine.h>
#include <state.h>
#include <state_machine.hpp>
#include <Home_state.h>
#include <SoftHand_states_close.h>
#include <SoftHand_states_open.h>
// #include <Grasp_state.h>
#include <Wait_msgs_.h>
#include <Trash_position.h>
#include <Removed_state.h>
#include <Place_table_state.h>
#include <overtune_state.h>
#include <starting_state.h>
#include <std_msgs/String.h>
#include <exit_state.h>
#include <HandPoseGenerator_state.h>
#include <Steady_state.h>

// using namespace dual_manipulation::state_manager;

Desp_state_server::Desp_state_server() : aspin(1)
{
  aspin.start();
  init();
  state_pub  = node.advertise<std_msgs::String>("state_machine_change", 5);
  loop_thread = std::thread(&Desp_state_server::loop, this);
}

void Desp_state_server::init()
{
  // auto start = new starting_state();
  auto Home = new Home_state(data);
  auto Wait_HandPoseGen_msg = new Wait_msgs(data);
  auto Close_Softhand = new SoftHand_close(data);
  auto Open_Softhand = new SoftHand_open(data);
  auto Trash_Position = new Pos_trash(data);
  auto Obj_To_Removed = new Removed_moves(data);
  auto Overtune_ = new Overtune_state(data);
  auto Hand_pose = new HandPoseGenerator(data);
  auto wait_state = new steady_state();

  std::vector<std::tuple < state<transition>*, transition_type, state<transition>* > > Grafo{
    //------initial state---------+--------- command -----------------------------------+-- final state---- +
    // std::make_tuple( start        , std::make_pair(transition::started,true)      ,    Home),
    std::make_tuple( Home                 , std::make_pair(transition::Error_arrived, true)          , Hand_pose                ),
    std::make_tuple( Wait_HandPoseGen_msg , std::make_pair(transition::Grasp_Obj, true)              , Close_Softhand           ),
    std::make_tuple( Wait_HandPoseGen_msg , std::make_pair(transition::Removed_Obj, true)            , Obj_To_Removed           ),
    std::make_tuple( Wait_HandPoseGen_msg , std::make_pair(transition::Overtune_table, true)         , Close_Softhand           ),
    std::make_tuple( Close_Softhand       , std::make_pair(transition::Overtune_table, true)         , Overtune_                ),
    std::make_tuple( Close_Softhand       , std::make_pair(transition::Wait_Closed_Softhand, true)   , Trash_Position           ),
    std::make_tuple( Trash_Position       , std::make_pair(transition::Error_arrived, true)          , Open_Softhand            ),
    std::make_tuple( Open_Softhand        , std::make_pair(transition::Wait_Open_Softhand, true)     , Home                     ),
    std::make_tuple( Obj_To_Removed       , std::make_pair(transition::Error_arrived, true)          , Open_Softhand            ),
    std::make_tuple( Overtune_            , std::make_pair(transition::Error_arrived, true)          , Open_Softhand            ),
    std::make_tuple( Hand_pose            , std::make_pair(transition::Error_arrived, true)          , Wait_HandPoseGen_msg     ),
    std::make_tuple( Hand_pose            , std::make_pair(transition::failed, true)                 , wait_state               ),
    std::make_tuple( wait_state           , std::make_pair(transition::Geometries_ok, true)          , Hand_pose                ),
    std::make_tuple( Hand_pose            , std::make_pair(transition::home_reset, true)             , Open_Softhand            ),
    std::make_tuple( Close_Softhand       , std::make_pair(transition::home_reset, true)             , Open_Softhand            ),
    std::make_tuple( Open_Softhand        , std::make_pair(transition::home_reset, true)             , Home                     ),
    std::make_tuple( Obj_To_Removed       , std::make_pair(transition::home_reset, true)             , Open_Softhand            ),
    std::make_tuple( Trash_Position       , std::make_pair(transition::home_reset, true)             , Open_Softhand            ),
  };

  sm.insert(Grafo);
  this->Grafo = Grafo;
  current_state = Home;
}



void Desp_state_server::loop()
{
  /*Rules:
     *
     * Current state -> Run
     * Check user commands
     * If the current state is complete
     * . Check current state results
     * . For each transition
     *   - try to change state
     *   - if the state changed, drop all the other transitions
     *   - else check next transition
     * Else do nothing
     */

  while (current_state->get_type() != "exit_state" && ros::ok())
  {
    current_state->run();
    usleep(5000); /*wait*/

    if (current_state->isComplete())
    {
      auto temp_map = current_state->getResults();
      for (auto temp : temp_map)
        transition_map[temp.first] = temp.second;
    }
    for (auto trigger : transition_map)
    {
      auto temp_state = sm.evolve_state_machine(current_state, trigger);
      if (temp_state != current_state)
      {
        current_state = temp_state;
        current_state->reset();
        std_msgs::String s;
        s.data = current_state->get_type();
        state_pub.publish(s);
        ROS_INFO_STREAM("- new state type: " << current_state->get_type());
        transition_map.clear();
        break;
      }
    }
  }
}

void Desp_state_server::join()
{
  return loop_thread.join();
}

void Desp_state_server::reset()
{
  std::map<state<transition>*, bool> deleted;
  for (auto line : Grafo)
  {
    if (deleted.count(std::get<0>(line)))
      continue;
    else
    {
      delete (std::get<0>(line));
      deleted[std::get<0>(line)] = true;
    }
  }

  init();
}


Desp_state_server::~Desp_state_server()
{
  current_state = new exit_state(data);
  if (loop_thread.joinable()) join();
}