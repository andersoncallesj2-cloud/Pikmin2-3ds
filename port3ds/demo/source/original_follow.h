#ifndef P2_ORIGINAL_FOLLOW_H
#define P2_ORIGINAL_FOLLOW_H
#include <stdbool.h>
/* Native-vector adaptation of the normal branch of NaviFollowState::exec. */
bool original_follow_velocity(const float leader[3],const float leader_target_velocity[3],const float follower[3],float speed,float output[3]);
#endif
