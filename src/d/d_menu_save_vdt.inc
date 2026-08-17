void dMenu_save_c::presentLayoutAnims() {
    if (mWarning != NULL) {
        mWarning->presentAnims();
    }

    dusk::vdt::present_toward(mDataBaseMoveAnmFrame, (f32)mDataBaseMoveFrameMax, field_0x40, mpSelectMoveBase != NULL ? mpSelectMoveBase->getPanePtr() : NULL);

    bool sharedSel = true;
    for (int i = 0; i < 3; i++) {
        J2DPane* pane = mpSelData[i] != NULL ? mpSelData[i]->getPanePtr() : NULL;
        if (pane == NULL || pane->mTransform != field_0x40) {
            sharedSel = false;
            break;
        }
    }
    if (sharedSel && mSelectedFile != 0xFF) {
        if (field_0x74[mSelectedFile] != field_0x80) {
            dusk::vdt::present_toward(field_0x74[mSelectedFile], (f32)field_0x80, field_0x40);
            for (int i = 0; i < 3; i++) {
                mpSelData[i]->getPanePtr()->animationTransform();
            }
        }
    } else {
        for (int i = 0; i < 3; i++) {
            J2DPane* pane = mpSelData[i] != NULL ? mpSelData[i]->getPanePtr() : NULL;
            if (pane == NULL) {
                continue;
            }
            if (pane->mTransform == field_0x40) {
                dusk::vdt::present_toward(field_0x74[i], (f32)SelEndFrameTbl[i], field_0x40, pane);
            } else if (pane->mTransform == field_0x44) {
                dusk::vdt::present_toward(field_0x74[i], (f32)SelStartFrameTbl[i], field_0x44, pane);
            }
        }
    }

    if (mYesNoMoveAnmFrame != mYesNoMoveAnmMax) {
        J2DPane* yn0 = mpNoYes[0] != NULL ? mpNoYes[0]->getPanePtr() : NULL;
        J2DPane* yn1 = mpNoYes[1] != NULL ? mpNoYes[1]->getPanePtr() : NULL;
        if ((yn0 != NULL && yn0->mTransform == field_0x48) ||
            (yn1 != NULL && yn1->mTransform == field_0x48))
        {
            dusk::vdt::present_toward(mYesNoMoveAnmFrame, (f32)mYesNoMoveAnmMax, field_0x48);
            if (yn0 != NULL && yn0->mTransform == field_0x48) {
                yn0->animationTransform();
            }
            if (yn1 != NULL && yn1->mTransform == field_0x48) {
                yn1->animationTransform();
            }
        }
    }

    for (int i = 0; i < 2; i++) {
        J2DPane* pane = mpNoYes[i] != NULL ? mpNoYes[i]->getPanePtr() : NULL;
        if (pane == NULL) {
            continue;
        }
        if (pane->mTransform == field_0x40) {
            dusk::vdt::present_toward(field_0x8c[i], (f32)YnSelStartFrameTbl[i][mYesNoSelAnmCol], field_0x40, pane);
        } else if (pane->mTransform == field_0x44) {
            dusk::vdt::present_toward(field_0x8c[i], (f32)YnSelEndFrameTbl[i][mYesNoSelAnmCol], field_0x44, pane);
        }
    }

    dusk::vdt::present_toward(field_0xb8, (f32)field_0xbc, field_0x4c, field_0xb4);
}
