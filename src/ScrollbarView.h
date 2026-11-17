#pragma once
#include <algorithm>


class IScrollbarView {
public:
    virtual ~IScrollbarView() = default;
    virtual void SetMetrics(int total_lines, int visible_lines) = 0;
    virtual void SetPosition(int current_line) = 0;
};

class NullScrollbarView : public IScrollbarView {
public:
    void SetMetrics(int, int) override {}
    void SetPosition(int) override {}
};