#ifndef SHAPEICONDELEGATE_H
#define SHAPEICONDELEGATE_H

#include <QApplication>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QModelIndex>
#include <QStyle>
#include <QStyleOptionViewItem>

class ShapeIconDelegate : public QStyledItemDelegate
{
public:
    explicit ShapeIconDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        // Draw the standard table cell background
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QStyle *style = opt.widget
                            ? opt.widget->style()
                            : QApplication::style();

        style->drawPrimitive(
            QStyle::PE_PanelItemViewItem,
            &opt,
            painter,
            opt.widget
            );

        // Get the total number of shapes
        int count = index.data(Qt::DisplayRole).toInt();

        // Get the shape type from column 1
        QString type = index.siblingAtColumn(1)
                           .data(Qt::DisplayRole).toString();

        // Determine how many icons to draw
        int iconCount = 0;

        if (count >= 1 && count <= 3) {
            iconCount = 1;
        }
        else if (count >= 4 && count <= 10) {
            iconCount = 2;
        }
        else if (count > 10) {
            iconCount = 3;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(Qt::black, 2));
        painter->setBrush(Qt::NoBrush);

        // Draw icons inside the table cell
        for (int i = 0; i < iconCount; ++i) {

            QRectF rect(
                option.rect.x() + 10 + i * 38,
                option.rect.center().y() - 10,
                28,
                20
                );

            if (type == "Rectangle") {
                painter->drawRect(rect);
            }
            else if (type == "Ellipse") {
                painter->drawEllipse(rect);
            }
            else if (type == "Polygon") {
                QPolygonF triangle;

                triangle << QPointF(rect.center().x(), rect.top())
                         << QPointF(rect.right(), rect.bottom())
                         << QPointF(rect.left(), rect.bottom());

                painter->drawPolygon(triangle);
            }
        }

        painter->restore();
    }
};

#endif // SHAPEICONDELEGATE_H
