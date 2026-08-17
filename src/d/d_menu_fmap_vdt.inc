bool dMenu_Fmap_c::advanceZoom(f32 end, FmapZoomApply apply) {
    if (mPresenting) {
        dusk::vdt::advance_toward_frame(mZoomLevel, end, 1.0f);
    }

    if (apply == FMAP_ZOOM_REGION) {
        mpDraw2DBack->zoomMapCalc(mZoomLevel / 10.0f);
    } else if (apply == FMAP_ZOOM_SPOT) {
        field_0x1ec = mZoomLevel / 10.0f;
        mpDraw2DBack->zoomMapCalc2(field_0x1ec);
    }

    if (mZoomLevel != end) {
        return false;
    }
    mZoomLevel = end;
    return !mPresenting;
}

void dMenu_Fmap_c::presentZoomView() {
    switch (mProcess) {
    case PROC_ZOOM_ALL_TO_REGION:
        advanceZoom(10.0f, FMAP_ZOOM_REGION);
        break;
    case PROC_ZOOM_REGION_TO_ALL:
        advanceZoom(0.0f, FMAP_ZOOM_REGION);
        break;
    case PROC_TO_PORTAL_WARP_MAP:
        advanceZoom(0.0f, FMAP_ZOOM_NONE);
        break;
    case PROC_ZOOM_REGION_TO_SPOT:
    case PROC_YAMIBOSS_DEMO4:
        advanceZoom(10.0f, FMAP_ZOOM_SPOT);
        break;
    case PROC_ZOOM_SPOT_TO_REGION:
        advanceZoom(0.0f, FMAP_ZOOM_SPOT);
        break;
    case PROC_PORTAL_DEMO1:
        if (dMsgObject_getMessageID() >= 2008 || mZoomLevel < 10) {
            advanceZoom(0.0f, FMAP_ZOOM_REGION);
        }
        break;
    case PROC_PORTAL_DEMO3:
        if (dMsgObject_getMessageID() >= 2010 || mZoomLevel > 0) {
            advanceZoom(10.0f, FMAP_ZOOM_REGION);
        }
        break;
    case PROC_YAMIBOSS_DEMO2:
        if (dMsgObject_getMsgObjectClass()->getStatus() == 0xe) {
            mZoomLevel += dusk::game_clock::original_frames();
        }
        break;
    case PROC_LIGHT_DEMO1:
    case PROC_HOWL_DEMO2:
        mZoomLevel += dusk::game_clock::original_frames();
        break;
    case PROC_TABLE_DEMO1:
        advanceZoom(10.0f, FMAP_ZOOM_NONE);
        if (mZoomLevel > 0) {
            mpDraw2DBack->zoomMapCalc(mZoomLevel / 10.0f);
        }
        break;
    case PROC_TABLE_DEMO2:
    case PROC_HOWL_DEMO1:
        advanceZoom(10.0f, FMAP_ZOOM_NONE);
        if (mZoomLevel > 0) {
            field_0x1ec = mZoomLevel / 10.0f;
            mpDraw2DBack->zoomMapCalc2(field_0x1ec);
        }
        break;
    default:
        break;
    }
}

void dMenu_Fmap_c::presentAnims() {
    mPresenting = true;
    if (mOpenCloseDir > 0) {
        isOpen();
    } else if (mOpenCloseDir < 0) {
        isClose();
    } else {
        presentZoomView();

        if (mProcess == PROC_ALL_MAP) {
            mpDraw2DBack->allmap_move2(mpStick);
        } else if (mProcess == PROC_REGION_MAP) {
            mpDraw2DBack->regionMapMove(mpStick);
        } else if (mProcess == PROC_SPOT_MAP) {
            mpDraw2DBack->stageMapMove(mpStick, 1, true);
        } else if (mProcess == PROC_PORTAL_WARP_MAP || mProcess == PROC_PORTAL_DEMO5) {
            mpDraw2DBack->regionMapMove(mpStick);
        }

        mpDraw2DTop->_execute();
        mpDraw2DBack->calcBackAlpha(mProcess == PROC_PORTAL_WARP_FORBID);
        if (mSpotTextureFadeAlpha != 1.0f) {
            dusk::vdt::present_addCalc2(&mSpotTextureFadeAlpha, 1.0f, 0.4f, 0.5f, 0.1f);
            mpDraw2DBack->setSpotTextureFadeAlpha(mSpotTextureFadeAlpha);
        }
    }
    if (mpDraw2DBack != NULL && mpDraw2DTop != NULL) {
        mpDraw2DBack->btkAnimeLoop(g_fmHIO.mBackAnimeStep);
        mpDraw2DTop->btkAnimeLoop(g_fmHIO.mTopAnimeStep);
        mpDraw2DTop->setMoyaAlpha(g_fmHIO.mMoyaAlpha);
    }
    mPresenting = false;
}
