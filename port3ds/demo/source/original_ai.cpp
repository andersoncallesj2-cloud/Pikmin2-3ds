// Original ActFree::exec body (CC0 projectPiki 999140f). Native shims below
// replace Piki world access and ActBore/ActGather; this is not the full Brain.
#include <stdint.h>
extern "C" bool game_ai_work(void*,unsigned,bool);
extern "C" void game_ai_idle(void*,unsigned,bool);
struct Vector3f{float x,y,z;Vector3f(float v=0):x(v),y(v),z(v){}};
static uint32_t random_state=1;static float randFloat(){random_state=random_state*1664525u+1013904223u;return (random_state>>8)/16777216.f;}
typedef uint16_t u16;
namespace Game {struct Piki{void *game;unsigned index;Vector3f mTargetVelocity;struct InvokeAIFreeArg{bool mDoSimpleCheck=false;};bool invokeAIFree(InvokeAIFreeArg& a){return game_ai_work(game,index,a.mDoSimpleCheck);}};}
namespace PikiAI {
enum {ACTEXEC_Success=0,ACTEXEC_Continue=1,ACTEXEC_Fail=2,PIKIAI_FREE_DEFAULT=0,PIKIAI_FREE_GATHER=1,PIKIAI_FREE_BORE=2};
struct NativeAction{unsigned timer=0;void init(void*){timer=60;}int exec(){if(timer)timer--;return timer?ACTEXEC_Continue:ACTEXEC_Success;}void finish(){timer=0;}};
struct ActFree{Game::Piki *mParent;NativeAction *mGather,*mBore;int mState=PIKIAI_FREE_DEFAULT;u16 mDelayTimer=150;int exec();};
int ActFree::exec()
{
	switch (mState) {
	case PIKIAI_FREE_GATHER: {
		// If we finished the gather state
		if (mGather->exec() == ACTEXEC_Success) {
			mState = PIKIAI_FREE_DEFAULT;

			// Wait for a bit of time to cool off
			u16 frameDelay = 30 * randFloat();
			mDelayTimer    = frameDelay + 150;
		}
		break;
	}

	case PIKIAI_FREE_BORE: {
		int status = mBore->exec();

		// if another action is available, stop being bored.
		Game::Piki::InvokeAIFreeArg settings;
		settings.mDoSimpleCheck = true; // just check if something is available - don't start it yet
		if (mParent->invokeAIFree(settings)) {
			mBore->finish();
		}

		// Assuming we finished or failed being bored, we'll be free again
		if (status == 0 || status == 2) {
			mState      = PIKIAI_FREE_DEFAULT;
			mDelayTimer = 90;
		}
		break;
	}

	default: {
		// We aren't moving anywhere anymore
		mParent->mTargetVelocity = Vector3f(0.0f);

		Game::Piki::InvokeAIFreeArg settings;
		// this will start the next available action (if there is one, and if piki is updateable)
		if (mParent->invokeAIFree(settings)) {
			return ACTEXEC_Success;
		}

		// If the delay timer is done we have a 50/50 chance of starting a boredom state
		if (mDelayTimer) {
			mDelayTimer--;
		} else if (randFloat() > 0.5f) {
			mBore->init(nullptr);
			mState = PIKIAI_FREE_BORE;
		}

		break;
	}
	}

	return ACTEXEC_Continue;
}
} // namespace PikiAI
static Game::Piki parents[100];static PikiAI::NativeAction gather[100],bore[100];static PikiAI::ActFree actions[100];static float clocks[100];
extern "C" void original_ai_reset(){for(unsigned i=0;i<100;i++){actions[i]=PikiAI::ActFree{};actions[i].mParent=&parents[i];actions[i].mGather=&gather[i];actions[i].mBore=&bore[i];clocks[i]=0;}}
extern "C" void original_ai_free(void *game,unsigned index,float dt){if(index>=100)return;parents[index].game=game;parents[index].index=index;clocks[index]+=dt;while(clocks[index]>=1.f/30){actions[index].exec();game_ai_idle(game,index,actions[index].mState==PikiAI::PIKIAI_FREE_BORE);clocks[index]-=1.f/30;}}
