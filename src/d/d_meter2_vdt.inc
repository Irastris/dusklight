#include "dusk/interp/vdt.h"

#include "d/d_meter_HIO.h"
#include "d/d_meter_map.h"
#include "d/d_meter2_info.h"

void dMeter2_c::presentAnims() {
    if (mpMeterDraw != NULL) {
        mpMeterDraw->present();

        f32 vessel_x;
        f32 vessel_y;
        f32 vessel_s;
        f32 vessel_a;
        if ((mStatus & 0x100) || daPy_getPlayerActorClass()->checkHawkWait() ||
            daPy_getPlayerActorClass()->checkGrassWhistle() ||
            g_meter2_info.mItemExplainWindowStatus != 0) {
            vessel_x = g_drawHIO.mLightDrop.mVesselTalkPosX;
            vessel_y = g_drawHIO.mLightDrop.mVesselTalkPosY;
            vessel_s = g_drawHIO.mLightDrop.mVesselTalkScale;
            vessel_a = g_drawHIO.mLightDrop.mVesselAlpha[1];
        } else {
            vessel_x = g_drawHIO.mLightDrop.mVesselPosX;
            vessel_y = g_drawHIO.mLightDrop.mVesselPosY;
            vessel_s = g_drawHIO.mLightDrop.mVesselScale;
            vessel_a = g_drawHIO.mLightDrop.mVesselAlpha[0];
        }
        if (mpMeterDraw->lightDropRowUsed()) {
            dusk::vdt::present_addCalc2(&mVesselPosX, vessel_x, 1.0f, 10.0f, 0.1f);
            dusk::vdt::present_addCalc2(&mVesselPosY, vessel_y, 1.0f, 10.0f, 0.1f);
            dusk::vdt::present_addCalc2(&mVesselScale, vessel_s, 0.2f, 1.0f, 0.1f);
            dusk::vdt::present_addCalc2(&mVesselAlpha, vessel_a, 0.2f, 1.0f, 0.1f);
            mpMeterDraw->field_0x6fc = mVesselAlpha;
            mpMeterDraw->presentVessel(mVesselPosX, mVesselPosY, mVesselScale);
        }

        f32 btn_ax[2];
        f32 btn_ay[2];
        if (isShowLightDrop()) {
            btn_ax[0] = g_drawHIO.mButtonAVesselPosX;
            btn_ay[0] = g_drawHIO.mButtonAVesselPosY;
            btn_ax[1] = 0.0f;
            btn_ay[1] = 0.0f;
        } else if (isShowFlag(0)) {
            if (isShowFlag(1)) {
                for (int i = 0; i < 2; i++) {
                    btn_ax[i] = g_drawHIO.mButtonATalkPosX[i];
                    btn_ay[i] = g_drawHIO.mButtonATalkPosY[i];
                }
            } else {
                for (int i = 0; i < 2; i++) {
                    btn_ax[i] = g_drawHIO.mButtonATalkAPosX[i];
                    btn_ay[i] = g_drawHIO.mButtonATalkAPosY[i];
                }
            }
        } else {
            btn_ax[0] = g_drawHIO.mButtonAPosX;
            btn_ay[0] = g_drawHIO.mButtonAPosY;
            btn_ax[1] = 0.0f;
            btn_ay[1] = 0.0f;
        }
        for (int i = 0; i < 2; i++) {
            dusk::vdt::present_addCalc2(&mButtonATalkPosX[i], btn_ax[i], 1.0f, 10.0f, 0.1f);
            dusk::vdt::present_addCalc2(&mButtonATalkPosY[i], btn_ay[i], 1.0f, 10.0f, 0.1f);
        }
        mpMeterDraw->presentButtonA(mButtonATalkPosX[0], mButtonATalkPosY[0], mButtonATalkPosX[1], mButtonATalkPosY[1], 1.0f);

        f32 btn_bx[2];
        f32 btn_by[2];
        if (isShowLightDrop()) {
            btn_bx[0] = g_drawHIO.mButtonBVesselPosX;
            btn_by[0] = g_drawHIO.mButtonBVesselPosY;
            btn_bx[1] = 0.0f;
            btn_by[1] = 0.0f;
        } else if (isShowFlag(1)) {
            for (int i = 0; i < 2; i++) {
                btn_bx[i] = g_drawHIO.mButtonBTalkPosX[i];
                btn_by[i] = g_drawHIO.mButtonBTalkPosY[i];
            }
        } else {
            for (int i = 0; i < 2; i++) {
                if (field_0x128 != 0 && i == 0) {
                    btn_bx[i] = g_drawHIO.mButtonBWolfPosX;
                    btn_by[i] = g_drawHIO.mButtonBWolfPosY;
                } else {
                    btn_bx[i] = 0.0f;
                    btn_by[i] = 0.0f;
                }
            }
        }
        for (int i = 0; i < 2; i++) {
            dusk::vdt::present_addCalc2(&field_0x148[i], btn_bx[i], 1.0f, 10.0f, 0.1f);
            dusk::vdt::present_addCalc2(&field_0x150[i], btn_by[i], 1.0f, 10.0f, 0.1f);
        }
        mpMeterDraw->presentButtonB(field_0x148[0], field_0x150[0], field_0x148[1], field_0x150[1], 1.0f);

        f32 cross_x = mButtonCrossOFFPosX + (((f32)field_0x1b4 / (f32)g_drawHIO.mButtonCrossMoveFrame) * (mButtonCrossONPosX - mButtonCrossOFFPosX));
        f32 cross_y = 0.0f;
        if (mpMap != NULL) {
            if (mpMap->isDispPosInsideFlg()) {
                cross_y = mButtonCrossONPosY + ((mpMap->getMapDispEdgeTop() - mpMeterDraw->getButtonCrossParentInitTransY()) - 15.0f);
            } else {
                cross_y = mButtonCrossOFFPosY;
            }
        }
        dusk::vdt::present_addCalc2(&field_0x15c, cross_y, 0.5f, 50.0f, 0.5f);
        mpMeterDraw->presentButtonCross(cross_x, field_0x15c);
    }
}

void dMeter2_c::presentMap() {
    if (mpMeterDraw == NULL || mpMap == NULL) {
        return;
    }
    mpMeterDraw->presentMapAlpha(mpMap);
}
