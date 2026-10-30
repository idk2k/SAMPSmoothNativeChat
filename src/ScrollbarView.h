#pragma once
#include <algorithm>

// јбстракци€ визуального скроллбара.
// ѕозвол€ет заменить DXUT-скроллбар на кастомный
// без изменени€ физики и рендера чата.
class IScrollbarView {
public:
    virtual ~IScrollbarView() = default;

    // total_lines Ч всего строк в истории;
    // visible_lines Ч сколько строк помещаетс€ в окно чата.
    virtual void SetMetrics(int total_lines, int visible_lines) = 0;

    // current_line Ч верхн€€ видима€ строка (из ScrollPhysics).
    virtual void SetPosition(int current_line) = 0;
};

// «аглушка: ничего не рисует и не синхронизирует.
// ”добна дл€ отладки или если скроллбар временно отключЄн.
class NullScrollbarView : public IScrollbarView {
public:
    void SetMetrics(int, int) override {}
    void SetPosition(int) override {}
};