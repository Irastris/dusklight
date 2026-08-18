void dMsgObject_c::presentAnims() {
    if (mpScrnDraw == NULL || mpOutFont == NULL) {
        return;
    }

    const u16 status = getStatusLocal();
    f32 target = 0.0f;
    if (status == 2) {
        if (isPlaceMessage() || isStaffMessage()) {
            target = g_MsgObject_HIO_c.mStageTitleFadeIn;
        } else if (isBossMessage()) {
            target = g_MsgObject_HIO_c.mBossNameFadeIn;
        } else if (isBookMessage()) {
            target = g_MsgObject_HIO_c.mBoxAppearFrame + g_MsgObject_HIO_c.mWaitFrame +
                     g_MsgObject_HIO_c.mLightAppearFrame;
        } else {
            target = g_MsgObject_HIO_c.mBoxAppearFrame;
        }
        dusk::vdt::advance_toward_frame(field_0x16a, target, 1.0f);
        if (isKanbanMessage() || isPlaceMessage() || isStaffMessage() || isBossMessage()) {
            f32 ratio = (f32)field_0x16a / target;
            mpScrnDraw->fukiScale(ratio);
            mpScrnDraw->fukiAlpha(ratio);
            mpOutFont->setAlphaRatio(ratio);
        } else if (isBookMessage()) {
            if (field_0x16a <= g_MsgObject_HIO_c.mBoxAppearFrame) {
                f32 ratio = (f32)field_0x16a / g_MsgObject_HIO_c.mBoxAppearFrame;
                mpScrnDraw->fukiAlpha(ratio);
                mpOutFont->setAlphaRatio(ratio);
            } else {
                mpScrnDraw->fukiAlpha(1.0f);
                mpOutFont->setAlphaRatio(1.0f);
            }
            s16 waitEnd = g_MsgObject_HIO_c.mBoxAppearFrame + g_MsgObject_HIO_c.mWaitFrame;
            if (field_0x16a >= waitEnd &&
                field_0x16a <= waitEnd + g_MsgObject_HIO_c.mLightAppearFrame)
            {
                f32 scale = (f32)(field_0x16a - waitEnd) / g_MsgObject_HIO_c.mLightAppearFrame;
                mpScrnDraw->fukiScale(scale);
            }
        } else {
            f32 ratio = (f32)field_0x16a / target;
            mpScrnDraw->fukiScale(1.0f);
            mpScrnDraw->fukiAlpha(ratio);
            mpOutFont->setAlphaRatio(ratio);
        }
    } else if (status == 17) {
        if (isKanbanMessage() || isBookMessage()) {
            target = g_MsgObject_HIO_c.field_0x304;
        } else if (isPlaceMessage() || isStaffMessage()) {
            target = g_MsgObject_HIO_c.mStageTitleFadeOut;
        } else if (isBossMessage()) {
            target = g_MsgObject_HIO_c.mBossNameFadeOut;
        } else {
            target = 5.0f;
        }
        dusk::vdt::advance_toward_frame(field_0x16a, target, 1.0f);
        f32 ratio = (f32)field_0x16a / target;
        mpScrnDraw->fukiAlpha(1.0f - ratio);
        if (isBookMessage()) {
            mpScrnDraw->fukiScale(1.0f - ratio);
        }
        mpOutFont->setAlphaRatio(1.0f - ratio);
    } else if (status == 6) {
        if (isBookMessage() && field_0x16a > 0) {
            dusk::vdt::advance_toward_frame(field_0x16a, 0.0f, 1.0f);
            f32 alpha = (10 - field_0x16a) / 10.0f;
            mpScrnDraw->fontAlpha(alpha);
            mpOutFont->setAlphaRatio(alpha);
        }
        jmessage_tReference* pRef = (jmessage_tReference*)mpRenProc->getReference();
        if (pRef->getCharAllAlphaRate() < 1.0f) {
            pRef->addCharAllAlphaRate();
            f32 alpha = pRef->getCharAllAlphaRate();
            mpScrnDraw->setCharAlphaRate(alpha);
            mpOutFont->setAlphaRatio(alpha);
        }
    } else if (status == 5 && isBookMessage() &&
               (isSend() || field_0x16a > 0))
    {
        dusk::vdt::advance_toward_frame(field_0x16a, 10.0f, 1.0f);
        if (field_0x16a <= 10) {
            f32 alpha = (10 - field_0x16a) / 10.0f;
            mpScrnDraw->fontAlpha(alpha);
            mpOutFont->setAlphaRatio(alpha);
        }
    }
}
