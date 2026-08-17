void dFile_select_c::presentLayoutAnims() {
    dusk::vdt::present_toward(mBaseMoveAnmFrame, (f32)mBaseMoveAnmFrameMax, mBaseMoveAnm,
                                     mBaseMovePane != NULL ? mBaseMovePane->getPanePtr() : NULL);

    if (mBaseSubPane != NULL && mBaseSubPane->mTransform == mBaseMoveAnm && mSelectNum != 0xFF) {
        if (field_0x00e0[mSelectNum] != field_0x00ec) {
            dusk::vdt::present_toward(field_0x00e0[mSelectNum], (f32)field_0x00ec,
                                             mBaseMoveAnm);
            for (int i = 0; i < 3; i++) {
                J2DPane* pane = mSelFilePanes[i]->getPanePtr();
                if (pane != NULL && pane->mTransform != NULL) {
                    pane->animationTransform();
                }
            }
            mBaseSubPane->animationTransform();
        }
    } else {
        for (int i = 0; i < 3; i++) {
            J2DPane* pane = mSelFilePanes[i]->getPanePtr();
            if (pane == NULL) {
                continue;
            }
            if (pane->mTransform == mBaseMoveAnm) {
                dusk::vdt::present_toward(field_0x00e0[i], (f32)SelEndFrameTbl[i],
                                                 mBaseMoveAnm, pane);
            } else if (pane->mTransform == field_0x0088) {
                dusk::vdt::present_toward(field_0x00e0[i], (f32)SelStartFrameTbl[i],
                                                 field_0x0088, pane);
            }
        }
    }

    dusk::vdt::present_toward(field_0x0358, (f32)field_0x035c, m3mBck, m3mMenuPane);

    for (int i = 0; i < 3; i++) {
        J2DPane* pane = m3mSelPane[i]->getPanePtr();
        if (pane == NULL) {
            continue;
        }
        if (pane->mTransform == m3mBck) {
            dusk::vdt::present_toward(field_0x034c[i], (f32)MenuSelStartFrameTbl[i], m3mBck,
                                             pane);
        } else if (pane->mTransform == m3mBck2) {
            dusk::vdt::present_toward(field_0x034c[i], (f32)MenuSelEndFrameTbl[i], m3mBck2,
                                             pane);
        }
    }

    if (field_0x0100 != field_0x0104) {
        J2DPane* yn0 = mYnSelPane[0]->getPanePtr();
        J2DPane* yn1 = mYnSelPane[1]->getPanePtr();
        if ((yn0 != NULL && yn0->mTransform == mYnSelBck3) ||
            (yn1 != NULL && yn1->mTransform == mYnSelBck3))
        {
            dusk::vdt::present_toward(field_0x0100, (f32)field_0x0104, mYnSelBck3);
            if (yn0 != NULL && yn0->mTransform == mYnSelBck3) {
                yn0->animationTransform();
            }
            if (yn1 != NULL && yn1->mTransform == mYnSelBck3) {
                yn1->animationTransform();
            }
        }
    }

    for (int i = 0; i < 2; i++) {
        J2DPane* pane = mYnSelPane[i]->getPanePtr();
        if (pane == NULL) {
            continue;
        }
        if (pane->mTransform == mYnSelBck) {
            dusk::vdt::present_toward(field_0x00f8[i], (f32)YnSelStartFrameTbl[i], mYnSelBck,
                                             pane);
        } else if (pane->mTransform == mYnSelBck2) {
            dusk::vdt::present_toward(field_0x00f8[i], (f32)YnSelEndFrameTbl[i], mYnSelBck2,
                                             pane);
        }
    }

    for (int i = 0; i < 2; i++) {
        if (mCpSelPane[i] == NULL) {
            continue;
        }
        J2DPane* pane = mCpSelPane[i]->getPanePtr();
        if (pane == NULL) {
            continue;
        }
        if (pane->mTransform == mCpSelBck) {
            dusk::vdt::present_toward(field_0x02b4[i], 99.0f, mCpSelBck, pane);
        } else if (pane->mTransform == mCpSelBck2) {
            dusk::vdt::present_toward(field_0x02b4[i], 109.0f, mCpSelBck2, pane);
        }
    }

    dusk::vdt::present_toward(field_0x0130, (f32)field_0x0134, field_0x0090, mErrorMsgPane);

    if (mBaseSubPane != NULL && mBaseSubPane->mTransform == field_0x009c) {
        dusk::vdt::present_toward(field_0x0110, (f32)field_0x0114, field_0x009c,
                                         mBaseSubPane);
    }

    dusk::vdt::present_toward(field_0x0120, (f32)field_0x0124, field_0x0094, mNameBasePane);
}

void dFile_select_c::presentNameInputFade() {
    bool fadeOut;
    switch (mDataSelProc) {
    case DATASELPROC_NAME_INPUT_FADE:
    case DATASELPROC_BACK_NAME_INPUT_MOVE0:
        fadeOut = true;
        break;
    case DATASELPROC_NAME_INPUT2_MOVE:
    case DATASELPROC_BACK_NAME_INPUT_MOVE:
        if (mDoRst::isReset()) {
            return;
        }
        fadeOut = false;
        break;
    default:
        return;
    }

    mFadeTimer -= dusk::game_clock::original_frames();
    if (mFadeTimer < 0.0f) {
        mFadeTimer = 0.0f;
    }

#if PLATFORM_GCN
    f32 t = mFadeTimer / 15.0f;
    u8 alpha = fadeOut ? (1.0f - t) * 255.0f : t * 255.0f;
    mpFadePict->setAlpha(alpha);
#else
    (void)fadeOut;
#endif
}

void dFile_select_c::presentAnims() {
    if (mpFileWarning != NULL) {
        mpFileWarning->presentAnims();
    }
    presentNameInputFade();
    selFileWakuAnm();
    bookIconAnm();
    dataDelEffAnm();
    dataCopyEffAnm();
    fileSel.Scr->animation();
    mYnSel.ScrYn->animation();
    m3mSel.Scr3m->animation();
    mSelDt.ScrDt->animation();
    if (mCpSel.isShow) {
        selCopyFileWakuAnm();
        copyBookIconAnm();
        mCpSel.Scr->animation();
    }
    presentLayoutAnims();
    fileSelectWide();
}
