namespace {

struct PresentScale {
    f32 start;
    f32 end;
    u8 calc;
};

struct PresentColor {
    JUtility::TColor startBlack;
    JUtility::TColor endBlack;
    JUtility::TColor startWhite;
    JUtility::TColor endWhite;
    u8 calc;
};

void color_anime_interpolate(JUtility::TColor& black, JUtility::TColor& white, f32 rate,
    JUtility::TColor startBlack, JUtility::TColor endBlack, JUtility::TColor startWhite,
    JUtility::TColor endWhite) {
    if (startBlack.r != endBlack.r) {
        black.r = startBlack.r + rate * (endBlack.r - startBlack.r);
    } else {
        black.r = endBlack.r;
    }

    if (startBlack.g != endBlack.g) {
        black.g = startBlack.g + rate * (endBlack.g - startBlack.g);
    } else {
        black.g = endBlack.g;
    }

    if (startBlack.b != endBlack.b) {
        black.b = startBlack.b + rate * (endBlack.b - startBlack.b);
    } else {
        black.b = endBlack.b;
    }

    if (startBlack.a != endBlack.a) {
        black.a = startBlack.a + rate * (endBlack.a - startBlack.a);
    } else {
        black.a = endBlack.a;
    }

    if (startWhite.r != endWhite.r) {
        white.r = startWhite.r + rate * (endWhite.r - startWhite.r);
    } else {
        white.r = endWhite.r;
    }

    if (startWhite.g != endWhite.g) {
        white.g = startWhite.g + rate * (endWhite.g - startWhite.g);
    } else {
        white.g = endWhite.g;
    }

    if (startWhite.b != endWhite.b) {
        white.b = startWhite.b + rate * (endWhite.b - startWhite.b);
    } else {
        white.b = endWhite.b;
    }

    if (startWhite.a != endWhite.a) {
        white.a = startWhite.a + rate * (endWhite.a - startWhite.a);
    } else {
        white.a = endWhite.a;
    }
}

dusk::vdt::Table<CPaneMgr, PresentScale> s_scales{};
dusk::vdt::Table<CPaneMgr, PresentColor> s_colors{};

f32& scale_timer(CPaneMgr* pane) {
    return pane->mScaleAnime;
}

f32& color_timer(CPaneMgr* pane) {
    return pane->mColorAnime;
}

void apply_scale(CPaneMgr* pane, const PresentScale& payload, f32 rate) {
    f32 tmp = payload.start + rate * (payload.end - payload.start);
    pane->getPanePtr()->scale(pane->getInitScaleX() * tmp, pane->getInitScaleY() * tmp);
}

void apply_color(CPaneMgr* pane, const PresentColor& payload, f32 rate) {
    JUtility::TColor black;
    JUtility::TColor white;
    color_anime_interpolate(black, white, rate, payload.startBlack, payload.endBlack,
        payload.startWhite, payload.endWhite);
    pane->setBlackWhite(black, white);
}

void advance_pane() {
    s_scales.drain(&scale_timer, &apply_scale);
    s_colors.drain(&color_timer, &apply_color);
}

}  // namespace

void cancel_scale(CPaneMgr* pane) {
    s_scales.unlink(pane);
}

void cancel_color(CPaneMgr* pane) {
    s_colors.unlink(pane);
}

bool request_scale(CPaneMgr* pane, s16 timer, f32 start, f32 end, u8 calc) {
    dusk::vdt::register_advance(&advance_pane);
    return s_scales.request(pane, timer, PresentScale{start, end, calc});
}

bool request_color(CPaneMgr* pane, s16 timer, JUtility::TColor startBlack,
    JUtility::TColor endBlack, JUtility::TColor startWhite, JUtility::TColor endWhite, u8 calc) {
    dusk::vdt::register_advance(&advance_pane);
    return s_colors.request(pane, timer,
        PresentColor{startBlack, endBlack, startWhite, endWhite, calc});
}
