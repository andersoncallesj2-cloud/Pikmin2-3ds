/* Adapted from projectPiki src/plugProjectKandoU/naviState.cpp,
 * NaviFollowState::exec normal movement branch, CC0,
 * 999140f3521e22619547d5d61862a6577ea73882.
 * Engine animation, enemy-punch and transition objects are outside this adapter. */
#include "original_follow.h"
#include <math.h>
bool original_follow_velocity(const float leader[3],const float target[3],const float follower[3],float speed,float output[3]){
    float sep[3]={leader[0]-follower[0],leader[1]-follower[1],leader[2]-follower[2]};
    float targetDist=sqrtf(sep[0]*sep[0]+sep[1]*sep[1]+sep[2]*sep[2]);
    if(targetDist>430.0f)return false;
    float newSpeed=speed;
    if(targetDist<30.0f)newSpeed=0.0f;
    for(int i=0;i<3;i++){
        float velocity=targetDist>0?sep[i]/targetDist*newSpeed:0;
        if(targetDist<60.0f)velocity=velocity*.5f+target[i]*.5f;
        output[i]=velocity;
    }
    return true;
}
