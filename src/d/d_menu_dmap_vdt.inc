void dMenu_Dmap_c::presentAnims() {
    if (mMapCtrl == NULL || mpDrawBg == NULL) {
        return;
    }

    mPresenting = true;
    if (mOpenCloseDir > 0) {
        isOpen();
    } else if (mOpenCloseDir < 0) {
        isClose();
    } else {
        mpDrawBg->addGoldFrameAlphaRate();

        if (0.0f == mpDrawBg->field_0xd94 && 0.0f == mpDrawBg->field_0xd98) {
            for (int i = 0; i < 1; i++) {
                dusk::vdt::present_looping(mpDrawBg->field_0xdc8[i], mpDrawBg->field_0xd28[i],
                                                  0.4f);
                mpDrawBg->mMapScreen[i]->animation();
            }
        }
        mpDrawBg->clearIconInfo();
        presentMapView();
        if (field_0x17e == 1 || field_0x17e == 2) {
            mpDrawBg->iconScaleAnm();
        }
    }
    mPresenting = false;

    presentLayoutAnims();
}

void dMenu_Dmap_c::presentLayoutAnims() {
    mpDrawBg->setAllTrans(field_0x104, field_0x108);

    if (mpDrawBg->mpBackTexture != NULL) {
        f32 var_f31 = 100.0f * mpDrawBg->field_0xd88;
        f32 sp24 = mpDrawBg->field_0xd80 / var_f31;
        f32 sp20 = mpDrawBg->field_0xd84 / var_f31;

        const ResTIMG* back_timg = mpDrawBg->mpBackTexture->getTexture(0)->getTexInfo();
        field_0x13c = mMapCtrl->getPixelStageSizeX();
        field_0x140 = mMapCtrl->getPixelStageSizeZ();
        field_0x134 = mMapCtrl->getPixelCenterX();
        field_0x138 = mMapCtrl->getPixelCenterZ();

        f32 temp_f30;
        f32 var_f29;
        f32 var_f27;
        if (field_0x13c > field_0x140) {
            temp_f30 = back_timg->width * var_f31;
            var_f29 = mMapCtrl->getStageMapSizeX();
            var_f27 = field_0x13c * (temp_f30 / var_f29);
        } else {
            temp_f30 = back_timg->height * var_f31;
            var_f29 = mMapCtrl->getStageMapSizeZ();
            var_f27 = field_0x140 * (temp_f30 / var_f29);
        }

        f32 sp1C[2];
        sp1C[0] = var_f27;
        sp1C[1] = var_f27;

        f32 sp14[2];
        sp14[0] = -(field_0x134 + (100.0f * (sp24 * mMapCtrl->getPixelPerCm())));
        sp14[1] = -(field_0x138 + 100.0f * (sp20 * mMapCtrl->getPixelPerCm()));

        CPaneMgr sp70;
        //!@bug It's unclear what this is supposed to be, but a stack pointer being converted to a bool is probably not intended.
        u8 sp40[0x30];
        Vec sp34 = sp70.getGlobalVtxCenter(mpDrawBg->getMapPane(), (bool)sp40, 0);
        mpDrawBg->mpBackTexture->move(sp14[0] + (sp34.x - (sp1C[0] / 2)),
                                      sp14[1] + (sp34.y - (sp1C[1] / 2)));
        mpDrawBg->mpBackTexture->resize(sp1C[0], sp1C[1]);
    }
}

void dMenu_Dmap_c::presentMapView() {
    const bool can_stick_scroll = m_process != 0 && field_0x183 != 0 && mMapCtrl->isEndZoomIn();
    f32 disp_center_x = mMapCtrl->getDispCenterX();
    f32 disp_center_z = mMapCtrl->getDispCenterZ();
    bool scrolled = false;
    if (can_stick_scroll) {
        f32 sp28 = g_fmapHIO.mScrollSpeedSlowBound < g_fmapHIO.mScrollSpeedFastBound ?
                       g_fmapHIO.mScrollSpeedSlowBound :
                       g_fmapHIO.mScrollSpeedFastBound;
        f32 sp24 = g_fmapHIO.mScrollSpeedSlowBound > g_fmapHIO.mScrollSpeedFastBound ?
                       g_fmapHIO.mScrollSpeedSlowBound :
                       g_fmapHIO.mScrollSpeedFastBound;
        f32 stick_value = mpCStick->getValueStick();
        s16 stick_angle = mpCStick->getAngleStick();
        if (stick_value >= sp28 && field_0x181 != 2) {
            scrolled = true;
            f32 var_f31 = mMapCtrl->getStageMapSizeX();
            if (var_f31 < mMapCtrl->getStageMapSizeZ()) {
                var_f31 = mMapCtrl->getStageMapSizeZ();
            }
            f32 var_f29 = stick_value < sp24 ? g_fmapHIO.mScrollSpeedDungeonMapSlow :
                                               g_fmapHIO.mScrollSpeedDungeonMapFast;
            f32 temp_f28 = (var_f29 / 100.0f) * var_f31 * dusk::game_clock::original_frames();
            f32 sp18 = temp_f28 * cM_ssin(stick_angle);
            f32 sp14 = temp_f28 * cM_scos(stick_angle);
            mMapCtrl->setPlusZoomCenterX(dusk::getSettings().game.enableMirrorMode ? -sp18 : sp18);
            mMapCtrl->setPlusZoomCenterZ(sp14);
        }
    }
    mMapCtrl->move();
    if (can_stick_scroll) {
        if (scrolled && (disp_center_x != mMapCtrl->getDispCenterX() ||
                         disp_center_z != mMapCtrl->getDispCenterZ()))
        {
            Z2GetAudioMgr()->seStartLevel(Z2SE_SY_MAP_SCROLL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
        if (mpDrawBg->mpMeterHaihai != NULL && isMapMoveState() &&
            (getCMessageNum() == 0x37B || getCMessageNum() == 0x569))
        {
            mpDrawBg->field_0xdda = 0;
            if (mMapCtrl->getDispCenterX() > mMapCtrl->getZoomCenterMinX()) {
                mpDrawBg->field_0xdda |= 1;
            }
            if (mMapCtrl->getDispCenterX() < mMapCtrl->getZoomCenterMaxX()) {
                mpDrawBg->field_0xdda |= 4;
            }
            if (mMapCtrl->getDispCenterZ() > mMapCtrl->getZoomCenterMinZ()) {
                mpDrawBg->field_0xdda |= 8;
            }
            if (mMapCtrl->getDispCenterZ() < mMapCtrl->getZoomCenterMaxZ()) {
                mpDrawBg->field_0xdda |= 2;
            }
            mpDrawBg->mpMeterHaihai->_execute(0);
        }
    }
    getIconPos(mMapCtrl->getDispFloorNo(), mMapCtrl->getMapBlendPer());
    getIconPos(mMapCtrl->getDispFloor2No(), 1.0f - mMapCtrl->getMapBlendPer());
}
