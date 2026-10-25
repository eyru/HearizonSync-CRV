// Hearizon Sync C.R.V. - 自绘控件
#pragma once

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

namespace hs {

// 圆角矩形路径
void roundRect(HDC dc, const RECT& rc, int radius);

// 绘制文本（自动换行、居中/左对齐）
void drawText(HDC dc, const std::wstring& text, const RECT& rc, COLORREF color,
              HFONT font, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE);

// 圆角按钮
class Button {
public:
    Button() = default;
    void create(HWND parent, int id, const std::string& text, int x, int y, int w, int h);
    void paint(HDC dc, const RECT& client);
    void onMouseMove(int x, int y);
    bool onMouseDown(int x, int y);
    bool onMouseUp(int x, int y);
    void setText(const std::string& t);
    void setColors(COLORREF bg, COLORREF hover, COLORREF fg);
    void setVisible(bool v) { visible_ = v; }
    bool visible() const { return visible_; }
    void setRect(const RECT& r) { rect_ = r; }
    RECT rect() const { return rect_; }
    void setOnClick(std::function<void()> cb) { onClick_ = std::move(cb); }
    void setFont(HFONT f) { font_ = f; }
    bool isHover() const { return hover_; }
    void setRadius(int r) { radius_ = r; }
    void setAnchorLeft(bool v) { anchorLeft_ = v; }

private:
    RECT rect_{0, 0, 0, 0};
    std::wstring text_;
    COLORREF bg_ = RGB(0x16, 0x24, 0x1F);
    COLORREF hoverColor_ = RGB(0x1C, 0x2E, 0x27);
    COLORREF fg_ = RGB(0xB8, 0xC9, 0xC2);
    bool hover_ = false;
    bool pressed_ = false;
    bool visible_ = true;
    bool anchorLeft_ = false;
    int radius_ = 8;
    HFONT font_ = nullptr;
    std::function<void()> onClick_;
};

// 圆角复选框
class CheckBox {
public:
    void create(HWND parent, const std::string& text, int x, int y, int w, int h);
    void paint(HDC dc);
    bool onMouseUp(int x, int y);
    bool onMouseDown(int x, int y);
    void onMouseMove(int x, int y);
    bool checked() const { return checked_; }
    void setChecked(bool v) { checked_ = v; }
    void setOnChange(std::function<void(bool)> cb) { onChange_ = std::move(cb); }
    void setRect(const RECT& r) { rect_ = r; }
    RECT rect() const { return rect_; }
    int measuredWidth(HDC dc) const;
    void setText(const std::string& t) { text_ = toWideLocal(t); }

private:
    static std::wstring toWideLocal(const std::string& s);
    RECT rect_{0, 0, 0, 0};
    std::wstring text_;
    bool checked_ = false;
    bool hover_ = false;
    HFONT font_ = nullptr;
    std::function<void(bool)> onChange_;
};

// 下拉框（自绘，点击弹出列表）
class ComboBox {
public:
    void create(HWND parent, int x, int y, int w, int h);
    void paint(HDC dc);
    bool onMouseUp(int x, int y);
    void onMouseMove(int x, int y);
    void setItems(const std::vector<std::string>& items);
    void setSelected(int index);
    int selected() const { return selected_; }
    std::string selectedText() const;
    void setEnabled(bool e) { enabled_ = e; }
    bool enabled() const { return enabled_; }
    void setDisabledItems(const std::vector<std::string>& names);
    void setRect(const RECT& r) { rect_ = r; }
    RECT rect() const { return rect_; }
    void setOnChange(std::function<void(int)> cb) { onChange_ = std::move(cb); }
    void setFont(HFONT f) { font_ = f; }
    // 弹出列表绘制（由主窗口在 WM_PAINT 后叠加）
    bool popupOpen() const { return popupOpen_; }
    void paintPopup(HDC dc);
    bool popupHitTest(int x, int y);   // 返回是否消费该点击
    void closePopup() { popupOpen_ = false; }

private:
    RECT rect_{0, 0, 0, 0};
    RECT popupRect_{0, 0, 0, 0};
    std::vector<std::string> items_;
    std::vector<std::wstring> itemsW_;
    std::vector<bool> disabled_;
    int selected_ = 0;
    int hoverIndex_ = -1;
    bool hover_ = false;
    bool enabled_ = true;
    bool popupOpen_ = false;
    HFONT font_ = nullptr;
    std::function<void(int)> onChange_;
};

// 文本显示区（支持追加、滚动）
class TextView {
public:
    void create(HWND parent, int x, int y, int w, int h);
    void paint(HDC dc);
    void append(const std::string& text);
    void appendLine(const std::string& text);
    void clear();
    void setScroll(int pos) { scroll_ = pos; }
    int scroll() const { return scroll_; }
    int contentHeight() const { return contentHeight_; }
    void setRect(const RECT& r) { rect_ = r; }
    RECT rect() const { return rect_; }
    void setFont(HFONT f) { font_ = f; }
    void setColors(COLORREF fg, COLORREF bg) { fg_ = fg; bg_ = bg; }
    void setLineHeight(int h) { lineHeight_ = h; }
    void ensureVisible();
    std::string text() const;

private:
    void reflow(HDC dc);

    RECT rect_{0, 0, 0, 0};
    std::vector<std::string> lines_;
    std::vector<std::wstring> linesW_;
    int scroll_ = 0;
    int contentHeight_ = 0;
    int lineHeight_ = 22;
    COLORREF fg_ = RGB(0xE8, 0xF0, 0xEC);
    COLORREF bg_ = RGB(0x11, 0x1B, 0x17);
    HFONT font_ = nullptr;
    int width_ = 0;
};

// 滚动条
class ScrollBar {
public:
    void paint(HDC dc);
    bool onMouseDown(int x, int y);
    void onMouseMove(int x, int y);
    void onMouseUp();
    void setRect(const RECT& r) { rect_ = r; }
    void setRange(int contentH, int viewH);
    int position() const { return pos_; }
    void setPosition(int p);
    bool dragging() const { return dragging_; }

private:
    RECT rect_{0, 0, 0, 0};
    int contentH_ = 0;
    int viewH_ = 0;
    int pos_ = 0;
    bool dragging_ = false;
    int dragStartY_ = 0;
    int dragStartPos_ = 0;
    bool hover_ = false;
};

}  // namespace hs
