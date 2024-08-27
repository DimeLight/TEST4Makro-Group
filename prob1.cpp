// SDK EDITION https://github.com/DimeLight
// Сергиенко Дмитрий Константинович tg = @DimeLight / mail = dimkvin@ya.ru
// #include <iostream> // а зачем
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

struct image {
    int width, height;
    int format; // 0=GRAY   1=RGB   2=BGR
    uint8_t* data;
};

struct box {
    int x1, y1; // левая верхняя точка рамки
    int x2, y2; // правая нижняя точка рамки
    int type; // 0=FACE   1=GUN   2=MASK
};

struct frame {
    image img;
    std::vector<box> boxes;
};

//  функция преобразует формат изображения из RGB в BGR
bool rgb2bgr(image& img)
{
    // мой код
    switch (img.format) { // отсеиваем по формату
    case 1: { // в случае если формат = 1 - RGB
        unsigned pixelcount = img.height * img.width; // чтобы не обращаться к памяти и не считать каждый раз количество пикселей
        uint8_t temp; // пустышка
        for (unsigned i = 0; // i всегда > 0 почему бы не задействовать пол диапазона значений
             i < pixelcount; // по каждому пикселю
             i += 3) { // прыгаем через 3 значения, которые 1 пиксель
            // меняем местами 0 и 2 элементы пикселя (r и b) через пустышку
            temp = img.data[i + 2];
            img.data[i + 2] = img.data[i];
            img.data[i] = temp;
        };
    }
        img.format = 2; // Меняем формат = 2 - BGR
        return true; // rgb2bgr получилось
    default: // во всех остальных случаях
        return false; // rgb2bgr не получилось
    }
}

// Учитывая специфику относительности координат - могут потребоваться рокировки
int& GetLeftFromBox(box& boxe) // левая сторона
{
    return boxe.x1;
}
int& GetRightFromBox(box& boxe) // правая сторона
{
    return boxe.x2;
}
int& GetUpFromBox(box& boxe) // верхняя сторона
{
    return boxe.y1;
}
int& GetDownFromBox(box& boxe) // нижняя сторона
{
    return boxe.y2;
}
void SetLeftFromBox(box& boxe, int value) // установить левую сторону
{
    boxe.x1 = value;
}
void SetRightFromBox(box& boxe, int value) // установить правую сторону
{
    boxe.x2 = value;
}
void SetUpFromBox(box& boxe, int value) // установить верхнюю сторону
{
    boxe.y1 = value;
}
void SetDownFromBox(box& boxe, int value) // установить нижнюю сторону
{
    boxe.y2 = value;
}
// рокировки

// возвращает площадь прямоугольника
int BoxArea(int Right, int Left, int Top, int Bottom)
{
    return (Right - Left) * (Top - Bottom); // ширина внутреннего прямоугольника = промежутку между правой и левой координатой x * высота внутреннего прямоугольника = промежутку между верхней и нижней координатой y
}

// это чтобы "безопасно" хранить коробку с площадью вместе
struct boxWithArea {
    boxWithArea(box& box, int Area)
        : box(box)
        , Area(Area) {};
    boxWithArea(box& b)
        : box(b)
        , Area(BoxArea(GetRightFromBox(b), GetLeftFromBox(b), GetUpFromBox(b), GetDownFromBox(b))) {};
    int& GetArea() { return Area; }
    box& GetBox() { return box; }

private:
    box& box;
    int Area;
};

// возвращает рамку пересечения двух прямоугольников рамок *даже если её площадь не существует (.type = b1.type)
box TwoBoxUniBox(box& b1, box& b2)
{
    box b;
    using namespace std;
    // при пересечении прямоугольников их объединение будет иметь координаты
    SetLeftFromBox(b, max(GetLeftFromBox(b1), GetLeftFromBox(b2))); // координаты x точек левой стороны = правой (max) точки из двух левых
    SetRightFromBox(b, min(GetRightFromBox(b1), GetRightFromBox(b2))); // координаты x точек правой стороны = левой (min) точки из двух правых
    SetUpFromBox(b, min(GetUpFromBox(b1), GetUpFromBox(b2))); // координаты y точек верхней стороны = нижней (min) точки из двух верхних
    SetDownFromBox(b, max(GetDownFromBox(b1), GetDownFromBox(b2))); // координаты y точек нижней стороны = верхней (max) точки из двух нижних
    b.type = b1.type;
    return b; // объединение
}

// какую рамку из двух оставлять и/или изменять следует определить в этой процедуре
box ItsTimeToChoiseBox(box a, box b)
{
    return a;
}

// возвращает рамку, центрированную на месте пересечения a и b, по площади примерно как площадь "a или b"
box ItsTimeToChoiseBox(box& UniBox, float IoU)
{
    // собираем стороны и центр
    int &Right = GetRightFromBox(UniBox), &Left = GetLeftFromBox(UniBox),
        &Up = GetUpFromBox(UniBox), &Down = GetDownFromBox(UniBox),
        // вычисляем середину и полустороны будущего прямоугольника рамки
        centerX = Right - Left, centerY = Up - Down,
        HalfWidth = round(IoU * (centerX - Left)), HalfHeight = round(IoU * (centerY - Down));
    // вычисляем середину и стороны прямоугольника рамки
    box XBox; // ваяем что отправим (X - какбы знак умножить, ассоциация с масштабированием)
    XBox.type = UniBox.type;
    SetLeftFromBox(XBox, centerX - HalfWidth);
    SetRightFromBox(XBox, centerX + HalfWidth);
    SetUpFromBox(XBox, centerY + HalfHeight);
    SetDownFromBox(XBox, centerY + HalfHeight);
    return XBox;
}

//  функция очищает кадр, оставляя одну рамку для общих объектов
//  объект считается общим для двух box, если их IoU >= threshold
//   "!!!" D(x) { x > 0 - "правее",y > 0 - "выше", 0 < threshold}
// т.е. ось абсцисс (OX) должна быть направленна "вправо", а ось ординат (OY) должна быть направленна "вверх"
void frame_clean(frame& f, float threshold)
{
    //  мой код
    if (0 > threshold) // ничего не делаем если с threshold косяк
        return;
    for (auto boxe1 = f.boxes.begin(); boxe1 < f.boxes.end() - 1; boxe1++) // первый (внешний) обход всех рамок объектов
        for (auto boxe2 = boxe1 + 1; boxe2 < f.boxes.end(); boxe2++) { // второй (внутренний) обход оставшихся (и новых) рамок объектов (место в паре не имеет значение, поэтому не сравниваем те что уже сравнили со всеми)
            if (boxe1->type != boxe2->type)
                continue; // учитывать ли параметр type, может ли он быть null на момент очистки frame и т.д. ?
            box UniBox = TwoBoxUniBox(*boxe1, *boxe2); // ищем пересечение двух прямоугольников
            boxWithArea UniBoxA(UniBox); // находим площадь пересечения
            if (UniBoxA.GetArea() < 0) // если ширина или высота геометрически несуществуют, то пересечения нет (пересечения в 1 точке "возможны", т.е. считаются за пересечения)
                continue; // пересечения нет - следующий
            unsigned Area = boxWithArea(*boxe1).GetArea() + boxWithArea(*boxe2).GetArea() - UniBoxA.GetArea(); // Находим общую площадь обоих прямоугольников​
            float IoU = Area / UniBoxA.GetArea(); // Рассчитываем IoU = ОбщаяПлощадь​ / Пересечение чтобы сравнивать с threshold​
            if (IoU >= threshold) { //  объект считается общим для двух box, если их IoU >= threshold
                f.boxes.push_back(ItsTimeToChoiseBox(UniBox, IoU)); // добавляем в вектор выбранную рамку
                // f.boxes.push_back(ItsTimeToChoiseBox(*boxe1, *boxe2)); // альтернатива
                f.boxes.erase(boxe2); // удаляем тот что ближе к концу, чтобы не потерять первый
                f.boxes.erase(boxe2); // удаляем тот что ближе к началу
                boxe1 = f.boxes.begin(); // начинаем заново первый for
                boxe2 = boxe1 + 1; // начинаем заново второй for
            };
        }
}

//  функция объединяет обнаруженные объекты из двух кадров в один
//  объект считается общим для двух box, если их IOU >= threshold
//  гарантируется, что f1.img == f2.img
//   "!!!" D(x) { x > 0 - "правее",y > 0 - "выше", 0 < threshold}
// т.е. ось абсцисс (OX) должна быть направленна "вправо", а ось ординат (OY) должна быть направленна "вверх"
frame union_frames(frame& f1, frame& f2, float threshold)
{
    //  мой код
    frame fRet; // ваяем что отправим
    fRet.img = f1.img; // f1.img == f2.img => должно быть и == fRet.img
    fRet.boxes = std::move(f1.boxes); // берём объекты из первого кадра
    fRet.boxes.insert(fRet.boxes.cend(), f2.boxes.cbegin(), f2.boxes.cend()); // добавляем объекты из второго кадра
    frame_clean(fRet, threshold); // смешиваем (очищаем лишние)
    return fRet;
}

int main()
{
    return 0;
}
