/* Verbatim uploaded Section method bodies; see provenance.json. */

#include "engine_compat.h"

namespace P2Engine {

void Section::init()
{
}

void Section::loading()
{
	doLoadingStart();
	do {
		bool isLoading;
		do {
			beginFrame();
			beginRender();

			endRender();
			isLoading = doLoading();
			endFrame();
		} while (isLoading);
	} while (sys->isDvdErrorOccured());
}

void Section::fadeIn()
{
	drawInit(*mGraphics, Section::Zero);

	s32 fadeTimer = ROUND_F32_TO_U8(mTimeStep / SINGLE_FRAME_LENGTH);
	if (mDisplay->mFader) {
		mDisplay->mFader->startFadeIn(fadeTimer);
	}

	for (f32 x = 0.0f; x < mTimeStep;) {
		beginFrame();
		beginRender();

		draw(*mGraphics);

		endRender();
		endFrame();

		if (!sys->isDvdErrorOccured()) {
			x += SINGLE_FRAME_LENGTH;
		}
	}
}

void Section::main()
{
#if defined(VERSION_US_DEMO)
	mOsTime = OSGetTime();
	drawInit(*mGraphics, Section::One);
	Game::GameConfig* config = &Game::gGameConfig;
#else
	drawInit(*mGraphics, Section::One);
#endif
	do {
		sys->mTimers->newFrame();

		beginFrame();
		beginRender();

		sys->mTimers->_start("draw", true);
		draw(*mGraphics);
		sys->mTimers->_stop("draw");
		endRender();

		sys->mTimers->_start("update", true);
		update();
		sys->mTimers->_stop("update");
		endFrame();
#if defined(VERSION_US_DEMO)
		if ((config->mParms.mE3version() || config->mParms.mNintendoVersion()) && forceReset()) {
			if (mDemoController1->isButtonHeld(JUTGamePad::PRESS_ANY) || mDemoController2->isButtonHeld(JUTGamePad::PRESS_ANY)) {
				mTimer = 0.0f;
			}
			mTimer += sys->getDeltaTime();
			if (mTimer > 180.0f) {
				sys->resetOn(false);
			}
		}
#endif
	} while (!mIsLoadingDVD && mIsMainActive);
	// Don't draw or render while loading from DVD

	s32 timer = ROUND_F32_TO_U8(mTimeStep / SINGLE_FRAME_LENGTH);
	PSMGetSceneMgrCheck()->doStopMainSeqCheck(timer);

	if (mDisplay->mFader) {
		mDisplay->mFader->startFadeOut(timer);
	}
}

void Section::fadeOut()
{
	drawInit(*mGraphics, Section::Two);
	for (f32 x = 0.0f; x < mTimeStep;) {
		beginFrame();

		beginRender();
		draw(*mGraphics);
		endRender();

		update();

		endFrame();

		if (!sys->isDvdErrorOccured()) {
			x += SINGLE_FRAME_LENGTH;
		}
	}

	for (; !isFinishable() || sys->isDvdErrorOccured();) {
		beginFrame();
		beginRender();
		endRender();
		endFrame();
	}
}

void Section::run()
{
#if defined(VERSION_JP)
	JUT_ASSERTLINE(532, mDisplay, "no Display manager.\n");
#else
	JUT_ASSERTLINE(543, mDisplay, "no Display manager.\n");
#endif

	mDisplay->waitBlanking(1);

	loading();
	fadeIn();
	main();
	fadeOut();

	if (mIsLoadingDVD) {
		do {
			do {
				beginFrame();
				beginRender();
				endRender();
				endFrame();
			} while (sys->dvdLoadSyncAllNoBlock());
		} while (gTHPReaderDvdAccess);
	}
}

bool Section::beginFrame()
{
	return sys->beginFrame();
}

void Section::endFrame()
{
	sys->endFrame();
}

void Section::beginRender()
{
	sys->beginRender();
}

void Section::endRender()
{
	if (mDisplayWiper) {
		mDisplayWiper->update();
	}

	sys->endRender();
}

bool Section::update()
{
	bool isActive = false;

	if (!sys->isDvdErrorOccured()) {
		isActive = doUpdate();
	} else {
		sys->mCardMgr->update();
	}

	return isActive;
}

void Section::draw(Graphics& gfx)
{
	if (!sys->isDvdErrorOccured()) {
		doDraw(gfx);
	}
}

} // namespace P2Engine
