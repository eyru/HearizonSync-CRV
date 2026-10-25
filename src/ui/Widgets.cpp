#include "Widgets.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/Theme.h"
#include "core/Util.h"

namespace hs {

// ---------------------------------------------------------------- 基础绘制
void roundRect(HDC dc, const RECT& rc, int radius) {
    int r = radius * 2;
    if (r > rc.right - rc.left) r = rc.right - rc.left;
    if (r > rc.bottom - rc.top) r = rc.bottom - rc.top;
    if (r <= 0) {
        Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
        return;
    }
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, r, r);
}

void drawText(HDC dc, const std::wstring& text, const RECT& rc, COLORREF color,
              HFONT font, UINT format) {
    if (text.empty()) return;
    HFONT old = font ? static_cast<HFONT>(SelectObject(dc, font)) : nullptr;
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    RECT r = rc;
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &r, format);
    if (old) SelectObject(dc, old);
}

// ---------------------------------------------------------------- Button
void Button::create(HWND parent, int id, const std::string& text, int x, int y, int w,
                    int h) {
    (void)parent;
    (void)id;
    text_ = toWide(text);
    rect_ = {x, y, x + w, y + h};
}

void Button::paint(HDC dc, const RECT& client) {
    if (!visible_) return;
    RECT rc = rect_;
    // 居中按钮：如果 rect 宽度为 0，按 client 计算
    if (rc.right <= rc.left) return;

    COLORREF bg = hover_ ? hoverColor_ : bg_;
    if (pressed_) bg = hoverColor_;

    HBRUSH brush = CreateSolidBrush(bg);
    HPEN pen = CreatePen(PS_SOLID, 1, bg);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, brush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    roundRect(dc, rc, radius_);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    UINT fmt = anchorLeft_ ? (DT_LEFT | DT_VCENTER | DT_SINGLELINE)
                           : (DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT textRc = rc;
    if (anchorLeft_) textRc.left += 14;
    drawText(dc, text_, textRc, fg_, font_, fmt);

    if (drawGear_) {
        // 矢量齿轮：8 个齿 + 中孔，比字体里的 U+2699 清晰得多
        const int cx = (rc.left + rc.right) / 2;
        const int cy = (rc.top + rc.bottom) / 2;
        int rOuter = (rc.right - rc.left) * 24 / 100;
        int rInner = rOuter * 72 / 100;
        int tooth = rOuter * 26 / 100;
        if (rOuter < 4) rOuter = 4;
        if (rInner < 3) rInner = 3;
        if (tooth < 2) tooth = 2;

        HPEN pen = CreatePen(PS_SOLID, 2, fg_);
        HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, GetStockObject(NULL_BRUSH)));

        const int kTeeth = 8;
        for (int i = 0; i < kTeeth; ++i) {
            double ang = i * (2.0 * 3.14159265358979 / kTeeth);
            int x1 = cx + static_cast<int>(std::cos(ang) * rInner);
            int y1 = cy + static_cast<int>(std::sin(ang) * rInner);
            int x2 = cx + static_cast<int>(std::cos(ang) * (rInner + tooth));
            int y2 = cy + static_cast<int>(std::sin(ang) * (rInner + tooth));
            MoveToEx(dc, x1, y1, nullptr);
            LineTo(dc, x2, y2);
        }
        Ellipse(dc, cx - rInner, cy - rInner, cx + rInner, cy + rInner);

        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
}

void Button::onMouseMove(int x, int y) {
    bool inside = x >= rect_.left && x < rect_.right && y >= rect_.top &&
                  y < rect_.bottom;
    if (inside != hover_) {
        hover_ = inside;
    }
}

bool Button::onMouseDown(int x, int y) {
    if (!visible_) return false;
    if (x >= rect_.left && x < rect_.right && y >= rect_.top && y < rect_.bottom) {
        pressed_ = true;
        return true;
    }
    return false;
}

bool Button::onMouseUp(int x, int y) {
    bool wasPressed = pressed_;
    pressed_ = false;
    if (!visible_ || !wasPressed) return false;
    if (x >= rect_.left && x < rect_.right && y >= rect_.top && y < rect_.bottom) {
        if (onClick_) onClick_();
        return true;
    }
    return false;
}

void Button::setText(const std::string& t) { text_ = toWide(t); }

void Button::setColors(COLORREF bg, COLORREF hoverColor, COLORREF fg) {
    bg_ = bg;
    hoverColor_ = hoverColor;
    fg_ = fg;
}

// ---------------------------------------------------------------- CheckBox
std::wstring CheckBox::toWideLocal(const std::string& s) { return toWide(s); }

void CheckBox::create(HWND parent, const std::string& text, int x, int y, int w,
                      int h) {
    (void)parent;
    text_ = toWide(text);
    rect_ = {x, y, x + w, y + h};
}

int CheckBox::measuredWidth(HDC dc) const {
    HFONT f = font_ ? font_ : uiFont(9);
    HFONT old = static_cast<HFONT>(SelectObject(dc, f));
    SIZE sz{};
    GetTextExtentPoint32W(dc, text_.c_str(), static_cast<int>(text_.size()), &sz);
    SelectObject(dc, old);
    const UiScale scale{Dpi::uiScale()};
    return sz.cx + 15 * scale + 12 * scale + 12 * scale;
}

void CheckBox::paint(HDC dc) {
    int h = rect_.bottom - rect_.top;
    int box = 15;
    const UiScale scale{Dpi::uiScale()};
    box = 15 * scale;

    RECT boxRc{rect_.left, rect_.top + (h - box) / 2, rect_.left + box,
               rect_.top + (h - box) / 2 + box};
    COLORREF border = checked_ ? theme().accent : (hover_ ? theme().border2 : theme().border);
    HBRUSH brush = CreateSolidBrush(checked_ ? theme().accent : theme().panel);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH oldB = static_cast<HBRUSH>(SelectObject(dc, brush));
    HPEN oldP = static_cast<HPEN>(SelectObject(dc, pen));
    roundRect(dc, boxRc, 4 * scale);
    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(brush);
    DeleteObject(pen);

    if (checked_) {
        HPEN checkPen = CreatePen(PS_SOLID, 2 * scale, RGB(0x03, 0x21, 0x1A));
        HPEN old = static_cast<HPEN>(SelectObject(dc, checkPen));
        int cx = boxRc.left + box / 2;
        int cy = boxRc.top + box / 2;
        MoveToEx(dc, cx - box / 5, cy, nullptr);
        LineTo(dc, cx - box / 12, cy + box / 5);
        LineTo(dc, cx + box / 4, cy - box / 5);
        SelectObject(dc, old);
        DeleteObject(checkPen);
    }

    RECT textRc{boxRc.right + 8 * scale, rect_.top, rect_.right, rect_.bottom};
    drawText(dc, text_, textRc, checked_ ? theme().text : theme().text2, font_,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

bool CheckBox::onMouseDown(int x, int y) {
    return x >= rect_.left && x < rect_.right && y >= rect_.top && y < rect_.bottom;
}

bool CheckBox::onMouseUp(int x, int y) {
    if (!onMouseDown(x, y)) return false;
    checked_ = !checked_;
    if (onChange_) onChange_(checked_);
    return true;
}

void CheckBox::onMouseMove(int x, int y) {
    hover_ = x >= rect_.left && x < rect_.right && y >= rect_.top &&
             y < rect_.bottom;
}

// ---------------------------------------------------------------- ComboBox
void ComboBox::create(HWND parent, int x, int y, int w, int h) {
    (void)parent;
    rect_ = {x, y, x + w, y + h};
}

void ComboBox::setItems(const std::vector<std::string>& items) {
    items_ = items;
    itemsW_.clear();
    disabled_.assign(items.size(), false);
    for (const auto& s : items) itemsW_.push_back(toWide(s));
    if (selected_ >= static_cast<int>(items_.size())) selected_ = 0;
}

void ComboBox::setSelected(int index) {
    if (index < 0 || index >= static_cast<int>(items_.size())) return;
    if (disabled_.size() > static_cast<size_t>(index) && disabled_[index]) return;
    if (selected_ == index) return;
    selected_ = index;
    if (onChange_) onChange_(index);
}

std::string ComboBox::selectedText() const {
    if (selected_ >= 0 && selected_ < static_cast<int>(items_.size()))
        return items_[selected_];
    return std::string();
}

void ComboBox::setDisabledItems(const std::vector<std::string>& names) {
    disabled_.assign(items_.size(), false);
    for (const auto& n : names) {
        for (size_t i = 0; i < items_.size(); ++i) {
            if (items_[i] == n) disabled_[i] = true;
        }
    }
}

void ComboBox::paint(HDC dc) {
    const UiScale scale{Dpi::uiScale()};
    COLORREF border = hover_ && enabled_ ? theme().border2 : theme().border;
    HBRUSH brush = CreateSolidBrush(theme().card);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH oldB = static_cast<HBRUSH>(SelectObject(dc, brush));
    HPEN oldP = static_cast<HPEN>(SelectObject(dc, pen));
    roundRect(dc, rect_, 9 * scale);
    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(brush);
    DeleteObject(pen);

    COLORREF fg = enabled_ ? theme().text : theme().faint;
    RECT textRc = rect_;
    textRc.left += 13 * scale;
    textRc.right -= 26 * scale;
    if (selected_ >= 0 && selected_ < static_cast<int>(itemsW_.size())) {
        drawText(dc, itemsW_[selected_], textRc, fg, font_,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // 下拉箭头
    int cx = rect_.right - 14 * scale;
    int cy = (rect_.top + rect_.bottom) / 2;
    HPEN arrowPen = CreatePen(PS_SOLID, 2 * scale, enabled_ ? theme().dim : theme().faint);
    HPEN old = static_cast<HPEN>(SelectObject(dc, arrowPen));
    MoveToEx(dc, cx - 4 * scale, cy - 2 * scale, nullptr);
    LineTo(dc, cx, cy + 2 * scale);
    LineTo(dc, cx + 4 * scale, cy - 2 * scale);
    SelectObject(dc, old);
    DeleteObject(arrowPen);
}

bool ComboBox::onMouseUp(int x, int y) {
    if (!enabled_) return false;
    if (popupOpen_) return false;   // 弹层由 paintPopup 处理
    if (x >= rect_.left && x < rect_.right && y >= rect_.top && y < rect_.bottom) {
        popupOpen_ = true;
        // 计算弹层位置
        int itemH = (rect_.bottom - rect_.top);
        int totalH = itemH * static_cast<int>(items_.size()) + 2;
        int px = rect_.left;
        int py = rect_.bottom + 4;
        // 下方放不下就向上翻转，避免弹层超出窗口
        if (boundsH_ > 0 && py + totalH > boundsH_) {
            py = rect_.top - totalH - 4;
            if (py < 0) py = 0;
        }
        popupRect_ = {px, py, px + (rect_.right - rect_.left), py + totalH};
        return true;
    }
    return false;
}

void ComboBox::onMouseMove(int x, int y) {
    hover_ = x >= rect_.left && x < rect_.right && y >= rect_.top && y < rect_.bottom;
    hoverIndex_ = -1;
    if (popupOpen_) {
        int itemH = rect_.bottom - rect_.top;
        if (x >= popupRect_.left && x < popupRect_.right &&
            y >= popupRect_.top && y < popupRect_.bottom) {
            int idx = (y - popupRect_.top - 1) / itemH;
            if (idx >= 0 && idx < static_cast<int>(items_.size())) hoverIndex_ = idx;
        }
    }
}

void ComboBox::paintPopup(HDC dc) {
    if (!popupOpen_) return;
    int itemH = rect_.bottom - rect_.top;

    HBRUSH frame = CreateSolidBrush(theme().border2);
    RECT outer = popupRect_;
    FillRect(dc, &outer, frame);
    DeleteObject(frame);

    RECT inner{outer.left + 1, outer.top + 1, outer.right - 1, outer.bottom - 1};
    HBRUSH bg = CreateSolidBrush(theme().panel);
    FillRect(dc, &inner, bg);
    DeleteObject(bg);

    for (size_t i = 0; i < itemsW_.size(); ++i) {
        RECT itemRc{inner.left, static_cast<LONG>(inner.top + 1 + i * itemH),
                    inner.right, static_cast<LONG>(inner.top + 1 + (i + 1) * itemH)};
        bool off = i < disabled_.size() && disabled_[i];
        bool isHover = static_cast<int>(i) == hoverIndex_ && !off;
        if (isHover) {
            HBRUSH hb = CreateSolidBrush(theme().cardHover);
            FillRect(dc, &itemRc, hb);
            DeleteObject(hb);
        }
        COLORREF fg;
        if (off) {
            fg = theme().faint;
        } else if (static_cast<int>(i) == selected_) {
            fg = theme().accent2;
        } else {
            fg = theme().text2;
        }
        RECT tr = itemRc;
        tr.left += 14;
        drawText(dc, itemsW_[i], tr, fg, font_, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

bool ComboBox::popupHitTest(int x, int y) {
    if (!popupOpen_) return false;
    int itemH = rect_.bottom - rect_.top;
    if (x >= popupRect_.left && x < popupRect_.right && y >= popupRect_.top &&
        y < popupRect_.bottom) {
        int idx = (y - popupRect_.top - 1) / itemH;
        if (idx >= 0 && idx < static_cast<int>(items_.size())) {
            bool off = idx < static_cast<int>(disabled_.size()) && disabled_[idx];
            if (!off) {
                popupOpen_ = false;
                setSelected(idx);
            }
        }
        return true;
    }
    popupOpen_ = false;
    return false;
}

// ---------------------------------------------------------------- TextView
void TextView::create(HWND parent, int x, int y, int w, int h) {
    (void)parent;
    rect_ = {x, y, x + w, y + h};
    width_ = w;
}

void TextView::append(const std::string& text) {
    if (text.empty()) return;
    if (lines_.empty()) {
        lines_.push_back(text);
        linesW_.push_back(toWide(text));
    } else {
        lines_.back() += text;
        linesW_.back() = toWide(lines_.back());
    }
    ensureVisible();
}

void TextView::appendLine(const std::string& text) {
    lines_.push_back(text);
    linesW_.push_back(toWide(text));
    ensureVisible();
}

void TextView::setPartial(const std::string& text) {
    partialUtf8_ = text;
    partial_ = toWide(text);
    ensureVisible();
}

void TextView::clearPartial() {
    partial_.clear();
    partialUtf8_.clear();
    ensureVisible();
}

void TextView::clear() {
    lines_.clear();
    linesW_.clear();
    partial_.clear();
    partialUtf8_.clear();
    scroll_ = 0;
    contentHeight_ = 0;
}

std::string TextView::text() const {
    std::string out;
    for (size_t i = 0; i < lines_.size(); ++i) {
        if (i) out += '\n';
        out += lines_[i];
    }
    return out;
}

void TextView::ensureVisible() {
    int total = static_cast<int>(lines_.size()) + (partial_.empty() ? 0 : 1);
    contentHeight_ = total * lineHeight_;
    int viewH = rect_.bottom - rect_.top;
    if (contentHeight_ > viewH) {
        scroll_ = contentHeight_ - viewH;
    } else {
        scroll_ = 0;
    }
}

void TextView::paint(HDC dc) {
    HBRUSH bg = CreateSolidBrush(bg_);
    FillRect(dc, &rect_, bg);
    DeleteObject(bg);

    int first = scroll_ / lineHeight_;
    if (first < 0) first = 0;
    int y = rect_.top - (scroll_ % lineHeight_);
    for (size_t i = static_cast<size_t>(first); i < linesW_.size(); ++i) {
        if (y >= rect_.bottom) break;
        RECT lineRc{rect_.left + 8, y, rect_.right - 8, y + lineHeight_};
        drawText(dc, linesW_[i], lineRc, fg_, font_,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += lineHeight_;
    }
    if (!partial_.empty() && y < rect_.bottom) {
        RECT lineRc{rect_.left + 8, y, rect_.right - 8, y + lineHeight_};
        drawText(dc, partial_, lineRc, faint_, font_,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

// ---------------------------------------------------------------- ScrollBar
void ScrollBar::setRange(int contentH, int viewH) {
    contentH_ = contentH;
    viewH_ = viewH;
    if (pos_ > contentH_ - viewH_) pos_ = std::max(0, contentH_ - viewH_);
}

void ScrollBar::setPosition(int p) {
    pos_ = p;
    int maxPos = std::max(0, contentH_ - viewH_);
    if (pos_ < 0) pos_ = 0;
    if (pos_ > maxPos) pos_ = maxPos;
}

void ScrollBar::paint(HDC dc) {
    if (contentH_ <= viewH_ || viewH_ <= 0) return;
    int h = rect_.bottom - rect_.top;
    int thumbH = std::max(24, h * viewH_ / std::max(1, contentH_));
    int maxPos = std::max(1, contentH_ - viewH_);
    int trackH = h - thumbH;
    int thumbY = rect_.top + (trackH * pos_) / maxPos;

    HBRUSH track = CreateSolidBrush(theme().panel);
    RECT tr = rect_;
    FillRect(dc, &tr, track);
    DeleteObject(track);

    HBRUSH thumbBrush = CreateSolidBrush(hover_ ? theme().border2 : theme().border);
    RECT thumbRc{rect_.left + 1, thumbY, rect_.right - 1, thumbY + thumbH};
    roundRect(dc, thumbRc, 4);
    HPEN pen = CreatePen(PS_SOLID, 1, hover_ ? theme().border2 : theme().border);
    HPEN old = static_cast<HPEN>(SelectObject(dc, pen));
    HBRUSH oldB = static_cast<HBRUSH>(SelectObject(dc, thumbBrush));
    roundRect(dc, thumbRc, 4);
    SelectObject(dc, old);
    SelectObject(dc, oldB);
    DeleteObject(pen);
    DeleteObject(thumbBrush);
}

bool ScrollBar::onMouseDown(int x, int y) {
    if (contentH_ <= viewH_) return false;
    if (x < rect_.left || x >= rect_.right || y < rect_.top || y >= rect_.bottom)
        return false;
    dragging_ = true;
    dragStartY_ = y;
    dragStartPos_ = pos_;
    return true;
}

void ScrollBar::onMouseMove(int x, int y) {
    hover_ = x >= rect_.left && x < rect_.right && y >= rect_.top &&
             y < rect_.bottom;
    if (!dragging_) return;
    int h = rect_.bottom - rect_.top;
    int thumbH = std::max(24, h * viewH_ / std::max(1, contentH_));
    int trackH = h - thumbH;
    if (trackH <= 0) return;
    int maxPos = std::max(1, contentH_ - viewH_);
    int delta = y - dragStartY_;
    setPosition(dragStartPos_ + delta * maxPos / trackH);
}

void ScrollBar::onMouseUp() { dragging_ = false; }

}  // namespace hs
