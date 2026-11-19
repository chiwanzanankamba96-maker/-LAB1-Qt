
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QSqlTableModel>
#include <QVector>
#include <QHash>
#include <QCloseEvent>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;
    QGraphicsScene *scene;
    QSqlTableModel *model;

    struct ShapeConnection {
        QGraphicsItem *first;
        QGraphicsItem *second;
        QGraphicsLineItem *line;
    };

    QVector<ShapeConnection> connections;

    QHash<int, QGraphicsItem *> shapeItems;

    void loadShapes();
    void loadConnections();
    void savePositions();
    void updateLines();
};

#endif // MAINWINDOW_H
