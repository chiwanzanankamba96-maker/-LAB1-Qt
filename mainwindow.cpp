
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "shapeicondelegate.h"

#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsLineItem>
#include <QGraphicsView>

#include <QPushButton>
#include <QMessageBox>
#include <QPen>
#include <QPolygonF>
#include <QLineF>
#include <QTimer>
#include <QDebug>
#include <QSet>
#include <QStringList>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QHeaderView>

#include <QToolBar>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QAbstractItemView>
#include <QCloseEvent>

// =====================================
// MAIN WINDOW
// =====================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    scene(new QGraphicsScene(this)),
    model(nullptr)
{
    ui->setupUi(this);

    scene->setSceneRect(0, 0, 900, 650);
    ui->graphicsView->setScene(scene);

    // =====================================
    // 1. SQLITE DATABASE
    // =====================================

    QSqlDatabase db =
        QSqlDatabase::addDatabase("QSQLITE");

    db.setDatabaseName("shapes.db");

    if (!db.open()) {
        QMessageBox::critical(
            this, "Database Error",
            db.lastError().text()
            );
        return;
    }

    QSqlQuery query(db);

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS shapes ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "type TEXT NOT NULL,"
            "connected_to TEXT,"
            "total_count INTEGER DEFAULT 0,"
            "x REAL DEFAULT 0,"
            "y REAL DEFAULT 0,"
            "width REAL DEFAULT 120,"
            "height REAL DEFAULT 80)"
            )) {
        QMessageBox::critical(
            this, "Database Error",
            query.lastError().text()
            );
        return;
    }

    // Add missing columns to older databases
    QSqlRecord record = db.record("shapes");

    struct Column {
        const char *name;
        const char *definition;
    };

    const Column columns[] = {
        {"x", "REAL DEFAULT 0"},
        {"y", "REAL DEFAULT 0"},
        {"width", "REAL DEFAULT 120"},
        {"height", "REAL DEFAULT 80"}
    };

    for (const Column &column : columns) {
        if (record.indexOf(column.name) == -1) {
            QSqlQuery alter(db);

            QString sql =
                QString("ALTER TABLE shapes ADD COLUMN %1 %2")
                    .arg(column.name, column.definition);

            if (!alter.exec(sql)) {
                QMessageBox::critical(
                    this, "Database Upgrade Error",
                    alter.lastError().text()
                    );
                return;
            }
        }
    }

    qDebug() << "SQLite database ready!";

    // =====================================
    // 2. DATABASE TABLE
    // =====================================

    model = new QSqlTableModel(this, db);
    model->setTable("shapes");

    model->setEditStrategy(
        QSqlTableModel::OnManualSubmit
        );

    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Shape Type");
    model->setHeaderData(2, Qt::Horizontal, "Connected To");
    model->setHeaderData(3, Qt::Horizontal, "Total Count");

    model->select();

    ui->tableView->setModel(model);

    // Custom icons from Lesson 11
    ui->tableView->setItemDelegateForColumn(
        3, new ShapeIconDelegate(this)
        );

    // Hide technical columns
    for (int column = 4;
         column < model->columnCount(); ++column) {
        ui->tableView->hideColumn(column);
    }

    ui->tableView->setEditTriggers(
        QAbstractItemView::NoEditTriggers
        );

    ui->tableView->setSelectionBehavior(
        QAbstractItemView::SelectRows
        );

    ui->tableView->setSelectionMode(
        QAbstractItemView::SingleSelection
        );

    ui->tableView->setColumnWidth(0, 60);
    ui->tableView->setColumnWidth(1, 140);
    ui->tableView->setColumnWidth(2, 140);
    ui->tableView->setColumnWidth(3, 150);

    ui->tableView->horizontalHeader()
        ->setStretchLastSection(true);

    // =====================================
    // 3. UPDATE COUNTS
    // =====================================

    auto refreshCounts = [this]() {
        QSqlQuery update;

        if (!update.exec(
                "UPDATE shapes SET total_count = "
                "(SELECT COUNT(*) FROM shapes AS s "
                "WHERE s.type = shapes.type)"
                )) {
            qDebug() << "Count error:"
                     << update.lastError().text();
        }

        model->select();
    };

    refreshCounts();

    // =====================================
    // 4. RESTORE SAVED SHAPES
    // =====================================

    loadShapes();
    loadConnections();

    // Automatically show saved shapes when opening
    QTimer::singleShot(0, this, [this]() {

        if (shapeItems.isEmpty()) {
            ui->graphicsView->centerOn(150, 100);
            return;
        }

        QRectF bounds;
        bool first = true;

        for (QGraphicsItem *item : shapeItems) {
            if (first) {
                bounds = item->sceneBoundingRect();
                first = false;
            } else {
                bounds = bounds.united(
                    item->sceneBoundingRect()
                    );
            }
        }

        bounds.adjust(-40, -40, 40, 40);

        ui->graphicsView->fitInView(
            bounds,
            Qt::KeepAspectRatio
            );
    });

    // Update lines when shapes move
    connect(scene, &QGraphicsScene::changed,
            this, [this]() {
                updateLines();
            });

    // =====================================
    // 5. SAVE NEW SHAPE
    // =====================================

    auto saveShape =
        [this, refreshCounts](const QString &type) {

            QSqlQuery insert;

            insert.prepare(
                "INSERT INTO shapes "
                "(type, connected_to, total_count, "
                "x, y, width, height) "
                "VALUES (?, '', 0, 0, 0, 120, 80)"
                );

            insert.addBindValue(type);

            if (!insert.exec()) {
                QMessageBox::warning(
                    this, "Database Error",
                    insert.lastError().text()
                    );
                return;
            }

            int id = insert.lastInsertId().toInt();

            double x = 30 + ((id - 1) % 4) * 155;
            double y = 30 + (((id - 1) / 4) % 4) * 125;

            QGraphicsItem *item = nullptr;

            if (type == "Rectangle") {
                item = scene->addRect(0, 0, 120, 80);
            }
            else if (type == "Ellipse") {
                item = scene->addEllipse(0, 0, 120, 80);
            }
            else if (type == "Polygon") {
                QPolygonF triangle;

                triangle << QPointF(60, 0)
                         << QPointF(120, 80)
                         << QPointF(0, 80);

                item = scene->addPolygon(triangle);
            }

            if (item) {
                item->setData(0, id);
                item->setPos(x, y);

                item->setFlag(
                    QGraphicsItem::ItemIsMovable
                    );

                item->setFlag(
                    QGraphicsItem::ItemIsSelectable
                    );

                shapeItems.insert(id, item);
            }

            QSqlQuery position;
            position.prepare(
                "UPDATE shapes SET x = ?, y = ? WHERE id = ?"
                );
            position.addBindValue(x);
            position.addBindValue(y);
            position.addBindValue(id);

            if (!position.exec()) {
                qDebug() << "Position error:"
                         << position.lastError().text();
            }

            refreshCounts();

            if (item) {
                ui->graphicsView->ensureVisible(item);
            }

            qDebug() << type << "saved with ID:" << id;
        };

    // =====================================
    // 6. SHAPE BUTTONS
    // =====================================

    connect(ui->btnRectangle, &QPushButton::clicked,
            this, [saveShape]() {
                saveShape("Rectangle");
            });

    connect(ui->btnEllipse, &QPushButton::clicked,
            this, [saveShape]() {
                saveShape("Ellipse");
            });

    connect(ui->btnPolygon, &QPushButton::clicked,
            this, [saveShape]() {
                saveShape("Polygon");
            });

    // =====================================
    // 7. CONNECT SHAPES
    // =====================================

    connect(ui->btnConnect, &QPushButton::clicked,
            this, [this]() {

                QList<QGraphicsItem *> selected =
                    scene->selectedItems();

                if (selected.size() != 2) {
                    QMessageBox::warning(
                        this, "Selection Error",
                        "Please select exactly two shapes."
                        );
                    return;
                }

                int id1 = selected[0]->data(0).toInt();
                int id2 = selected[1]->data(0).toInt();

                if (id1 <= 0 || id2 <= 0 || id1 == id2)
                    return;

                QSqlQuery read;
                read.prepare(
                    "SELECT connected_to FROM shapes WHERE id = ?"
                    );
                read.addBindValue(id1);

                if (!read.exec() || !read.next()) {
                    QMessageBox::warning(
                        this, "Database Error",
                        "Could not read shape information."
                        );
                    return;
                }

                QStringList existing =
                    read.value(0).toString().split(
                        ',', Qt::SkipEmptyParts
                        );
                read.finish();

                if (existing.contains(QString::number(id2))) {
                    QMessageBox::information(
                        this, "Already Connected",
                        "These shapes are already connected."
                        );
                    return;
                }

                QSqlDatabase database =
                    QSqlDatabase::database();

                if (!database.transaction()) {
                    QMessageBox::warning(
                        this, "Database Error",
                        database.lastError().text()
                        );
                    return;
                }

                auto addConnectedId =
                    [](int fromId, int toId) -> bool {

                    QSqlQuery q;
                    q.prepare(
                        "SELECT connected_to FROM shapes WHERE id = ?"
                        );
                    q.addBindValue(fromId);

                    if (!q.exec() || !q.next())
                        return false;

                    QStringList ids =
                        q.value(0).toString().split(
                            ',', Qt::SkipEmptyParts
                            );
                    q.finish();

                    if (!ids.contains(QString::number(toId)))
                        ids.append(QString::number(toId));

                    QSqlQuery update;
                    update.prepare(
                        "UPDATE shapes SET connected_to = ? "
                        "WHERE id = ?"
                        );
                    update.addBindValue(ids.join(","));
                    update.addBindValue(fromId);

                    return update.exec();
                };

                bool success =
                    addConnectedId(id1, id2) &&
                    addConnectedId(id2, id1);

                if (!success) {
                    database.rollback();

                    QMessageBox::warning(
                        this, "Database Error",
                        "Could not save the connection."
                        );
                    return;
                }

                if (!database.commit()) {
                    database.rollback();

                    QMessageBox::warning(
                        this, "Database Error",
                        database.lastError().text()
                        );
                    return;
                }

                QPointF p1 =
                    selected[0]->sceneBoundingRect().center();

                QPointF p2 =
                    selected[1]->sceneBoundingRect().center();

                auto *line = scene->addLine(
                    QLineF(p1, p2),
                    QPen(Qt::black, 2)
                    );

                line->setZValue(-1);

                connections.append({
                    selected[0], selected[1], line
                });

                model->select();
            });

    // =====================================
    // 8. FILTER CONTROLS
    // =====================================

    auto *toolbar = addToolBar("Shape Filters");
    toolbar->setMovable(false);

    toolbar->addWidget(new QLabel(" ID: ", this));

    auto *idFilter = new QLineEdit(this);
    idFilter->setPlaceholderText("Any ID");
    idFilter->setMaximumWidth(80);
    toolbar->addWidget(idFilter);

    toolbar->addWidget(new QLabel(" Type: ", this));

    auto *typeFilter = new QComboBox(this);
    typeFilter->addItems({
        "All types", "Rectangle", "Ellipse", "Polygon"
    });
    toolbar->addWidget(typeFilter);

    toolbar->addWidget(new QLabel(" Count: ", this));

    auto *countFilter = new QSpinBox(this);
    countFilter->setRange(0, 100000);
    countFilter->setSpecialValueText("Any");
    toolbar->addWidget(countFilter);

    auto *filterButton =
        new QPushButton("Apply Filter", this);
    toolbar->addWidget(filterButton);

    auto *clearButton =
        new QPushButton("Clear", this);
    toolbar->addWidget(clearButton);

    connect(filterButton, &QPushButton::clicked,
            this, [this, idFilter,
             typeFilter, countFilter]() {

                QStringList conditions;
                QString text = idFilter->text().trimmed();

                if (!text.isEmpty()) {
                    bool ok = false;
                    int id = text.toInt(&ok);

                    if (!ok || id <= 0) {
                        QMessageBox::warning(
                            this, "Invalid ID",
                            "Enter a positive numeric ID."
                            );
                        return;
                    }

                    conditions << QString("id = %1").arg(id);
                }

                if (typeFilter->currentIndex() > 0) {
                    conditions << QString("type = '%1'")
                    .arg(typeFilter->currentText());
                }

                if (countFilter->value() > 0) {
                    conditions << QString("total_count = %1")
                    .arg(countFilter->value());
                }

                model->setFilter(conditions.join(" AND "));
                model->select();
            });

    connect(clearButton, &QPushButton::clicked,
            this, [this, idFilter,
             typeFilter, countFilter]() {

                idFilter->clear();
                typeFilter->setCurrentIndex(0);
                countFilter->setValue(0);

                model->setFilter("");
                model->select();
            });

    // =====================================
    // 9. DELETE SELECTED SHAPE
    // =====================================

    auto *deleteButton =
        new QPushButton("Delete Selected", this);
    toolbar->addWidget(deleteButton);

    connect(deleteButton, &QPushButton::clicked,
            this, [this, refreshCounts]() {

                QModelIndex index =
                    ui->tableView->currentIndex();

                if (!index.isValid()) {
                    QMessageBox::warning(
                        this, "Selection Error",
                        "Select a row in the table first."
                        );
                    return;
                }

                int id =
                    model->index(index.row(), 0).data().toInt();

                if (QMessageBox::question(
                        this, "Delete Shape",
                        "Permanently delete shape ID " +
                            QString::number(id) + "?"
                        ) != QMessageBox::Yes) {
                    return;
                }

                savePositions();

                QSqlDatabase database =
                    QSqlDatabase::database();

                if (!database.transaction()) {
                    QMessageBox::warning(
                        this, "Database Error",
                        database.lastError().text()
                        );
                    return;
                }

                QSqlQuery remove;
                remove.prepare(
                    "DELETE FROM shapes WHERE id = ?"
                    );
                remove.addBindValue(id);

                if (!remove.exec()) {
                    database.rollback();
                    QMessageBox::warning(
                        this, "Database Error",
                        remove.lastError().text()
                        );
                    return;
                }

                QSqlQuery read;
                if (!read.exec(
                        "SELECT id, connected_to FROM shapes"
                        )) {
                    database.rollback();
                    QMessageBox::warning(
                        this, "Database Error",
                        read.lastError().text()
                        );
                    return;
                }

                QVector<QPair<int, QString>> updates;

                while (read.next()) {
                    int otherId = read.value(0).toInt();

                    QStringList ids =
                        read.value(1).toString().split(
                            ',', Qt::SkipEmptyParts
                            );

                    if (ids.removeAll(QString::number(id)) > 0) {
                        updates.append({
                            otherId, ids.join(",")
                        });
                    }
                }

                read.finish();

                for (const auto &entry : updates) {
                    QSqlQuery update;

                    update.prepare(
                        "UPDATE shapes SET connected_to = ? "
                        "WHERE id = ?"
                        );
                    update.addBindValue(entry.second);
                    update.addBindValue(entry.first);

                    if (!update.exec()) {
                        database.rollback();
                        QMessageBox::warning(
                            this, "Database Error",
                            update.lastError().text()
                            );
                        return;
                    }
                }

                if (!database.commit()) {
                    database.rollback();
                    QMessageBox::warning(
                        this, "Database Error",
                        database.lastError().text()
                        );
                    return;
                }

                QGraphicsItem *target =
                    shapeItems.value(id, nullptr);

                if (target) {
                    for (int i = connections.size() - 1;
                         i >= 0; --i) {

                        auto c = connections[i];

                        if (c.first == target ||
                            c.second == target) {

                            connections.removeAt(i);
                            scene->removeItem(c.line);
                            delete c.line;
                        }
                    }

                    shapeItems.remove(id);
                    scene->removeItem(target);
                    delete target;
                }

                refreshCounts();
                qDebug() << "Deleted shape ID:" << id;
            });
}

// =====================================
// 10. LOAD SAVED SHAPES
// =====================================

void MainWindow::loadShapes()
{
    QSqlQuery query;

    if (!query.exec(
            "SELECT id, type, x, y, width, height "
            "FROM shapes ORDER BY id"
            )) {
        qDebug() << "Load error:"
                 << query.lastError().text();
        return;
    }

    while (query.next()) {
        int id = query.value(0).toInt();
        QString type = query.value(1).toString();

        double x = query.value(2).toDouble();
        double y = query.value(3).toDouble();
        double width = query.value(4).toDouble();
        double height = query.value(5).toDouble();

        if (width <= 0) width = 120;
        if (height <= 0) height = 80;

        QGraphicsItem *item = nullptr;

        if (type == "Rectangle") {
            item = scene->addRect(0, 0, width, height);
        }
        else if (type == "Ellipse") {
            item = scene->addEllipse(0, 0, width, height);
        }
        else if (type == "Polygon") {
            QPolygonF triangle;

            triangle << QPointF(width / 2, 0)
                     << QPointF(width, height)
                     << QPointF(0, height);

            item = scene->addPolygon(triangle);
        }

        if (!item)
            continue;

        item->setData(0, id);
        item->setPos(x, y);

        item->setFlag(QGraphicsItem::ItemIsMovable);
        item->setFlag(QGraphicsItem::ItemIsSelectable);

        shapeItems.insert(id, item);
    }

    qDebug() << "Loaded shapes:" << shapeItems.size();
}

// =====================================
// 11. LOAD CONNECTIONS
// =====================================

void MainWindow::loadConnections()
{
    QSqlQuery query;

    if (!query.exec(
            "SELECT id, connected_to FROM shapes"
            )) {
        qDebug() << "Connection load error:"
                 << query.lastError().text();
        return;
    }

    QSet<QString> loaded;

    while (query.next()) {
        int id1 = query.value(0).toInt();

        QStringList ids =
            query.value(1).toString().split(
                ',', Qt::SkipEmptyParts
                );

        for (const QString &text : ids) {
            bool ok = false;
            int id2 = text.toInt(&ok);

            if (!ok || id1 == id2)
                continue;

            if (!shapeItems.contains(id1) ||
                !shapeItems.contains(id2))
                continue;

            int a = qMin(id1, id2);
            int b = qMax(id1, id2);

            QString key =
                QString("%1-%2").arg(a).arg(b);

            if (loaded.contains(key))
                continue;

            loaded.insert(key);

            QGraphicsItem *first =
                shapeItems.value(a);

            QGraphicsItem *second =
                shapeItems.value(b);

            QPointF p1 =
                first->sceneBoundingRect().center();

            QPointF p2 =
                second->sceneBoundingRect().center();

            auto *line = scene->addLine(
                QLineF(p1, p2),
                QPen(Qt::black, 2)
                );

            line->setZValue(-1);

            connections.append({
                first, second, line
            });
        }
    }

    qDebug() << "Loaded connections:"
             << connections.size();
}

// =====================================
// 12. SAVE POSITIONS AND SIZES
// =====================================

void MainWindow::savePositions()
{
    for (auto it = shapeItems.cbegin();
         it != shapeItems.cend(); ++it) {

        int id = it.key();
        QGraphicsItem *item = it.value();

        QRectF rect;

        if (auto *r =
            dynamic_cast<QGraphicsRectItem *>(item)) {
            rect = r->rect();
        }
        else if (auto *e =
                 dynamic_cast<QGraphicsEllipseItem *>(item)) {
            rect = e->rect();
        }
        else if (auto *p =
                 dynamic_cast<QGraphicsPolygonItem *>(item)) {
            rect = p->polygon().boundingRect();
        }
        else {
            rect = item->boundingRect();
        }

        QPointF position = item->pos();

        QSqlQuery query;
        query.prepare(
            "UPDATE shapes "
            "SET x = ?, y = ?, width = ?, height = ? "
            "WHERE id = ?"
            );

        query.addBindValue(position.x());
        query.addBindValue(position.y());
        query.addBindValue(rect.width());
        query.addBindValue(rect.height());
        query.addBindValue(id);

        if (!query.exec()) {
            qDebug() << "Position save error:"
                     << query.lastError().text();
        }
    }
}

// =====================================
// 13. UPDATE CONNECTING LINES
// =====================================

void MainWindow::updateLines()
{
    for (const auto &connection : connections) {

        QPointF p1 =
            connection.first->sceneBoundingRect().center();

        QPointF p2 =
            connection.second->sceneBoundingRect().center();

        QLineF newLine(p1, p2);

        if (connection.line->line() != newLine) {
            connection.line->setLine(newLine);
        }
    }
}

// =====================================
// 14. SAVE WHEN CLOSING
// =====================================

void MainWindow::closeEvent(QCloseEvent *event)
{
    savePositions();
    QMainWindow::closeEvent(event);
}

// =====================================
// DESTRUCTOR
// =====================================

MainWindow::~MainWindow()
{
    delete ui;
}
