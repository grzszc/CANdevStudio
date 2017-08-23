#ifndef CANRAWVIEW_P_H
#define CANRAWVIEW_P_H

#include "log.hpp"
#include "ui_canrawview.h"
#include <QDebug>
//#include <QFile>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtCore/QElapsedTimer>
#include <QtGui/QStandardItemModel>
#include <QtSerialBus/QCanBusFrame>

namespace Ui {
class CanRawViewPrivate;
}

class QElapsedTimer;

class CanRawViewPrivate : public QWidget {
    Q_OBJECT
    Q_DECLARE_PUBLIC(CanRawView)

public:
    CanRawViewPrivate(CanRawView* q)
        : ui(std::make_unique<Ui::CanRawViewPrivate>())
        , timer(std::make_unique<QElapsedTimer>())
        , simStarted(false)
        , q_ptr(q)
        , columnsOrder({ "rowID", "timeDouble", "time", "idInt", "id", "dir", "dlc", "data" })
    {
        ui->setupUi(this);

        tvModel.setHorizontalHeaderLabels(columnsOrder);
        ui->tv->setModel(&tvModel);
        ui->tv->horizontalHeader()->setSectionsMovable(true);
        ui->tv->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
        ui->tv->setColumnHidden(0, true);
        ui->tv->setColumnHidden(1, true);
        ui->tv->setColumnHidden(3, true);

        connect(ui->pbClear, &QPushButton::pressed, this, &CanRawViewPrivate::clear);
        connect(ui->pbDockUndock, &QPushButton::pressed, this, &CanRawViewPrivate::dockUndock);

        connect(
            ui->tv->horizontalHeader(), &QHeaderView::sectionClicked, [=](int logicalIndex) { sort(logicalIndex); });
    }

    ~CanRawViewPrivate() {}

    void saveSettings(QJsonObject& json)
    {
        QJsonObject jObjects;
        QJsonArray viewModelsArray;
        /*
         * Temporary below code comments use for test during debug and do possibility to write settings to file
         *
        QString fileName("viewSave.cds");
        QFile saveFile(fileName);

        if (saveFile.open(QIODevice::WriteOnly) == false) {
            cds_debug("Problem with open the file {0} to write confiruration", fileName.toStdString());
            return;
        }
        */
        assert(ui != nullptr);
        assert(ui->freezeBox != nullptr);
        assert(q_ptr != nullptr);
        assert(q_ptr->windowTitle().toStdString().length() != 0);

        writeColumnsOrder(jObjects);
        jObjects["Sorting"] = prevIndex;
        jObjects["Scrolling"] = (ui->freezeBox->isChecked() == true) ? 1 : 0;
        writeViewModel(viewModelsArray);
        jObjects["Models"] = std::move(viewModelsArray);
        json[q_ptr->windowTitle().toStdString().c_str()] = std::move(jObjects);
        /*
        QJsonDocument saveDoc(json);
        saveFile.write(saveDoc.toJson());
        */
    }

    void frameView(const QCanBusFrame& frame, const QString& direction)
    {
        if (!simStarted) {
            cds_debug("send/received frame while simulation stopped");
            return;
        }

        auto payHex = frame.payload().toHex();
        // inster space between bytes, skip the end
        for (int ii = payHex.size() - 2; ii >= 2; ii -= 2) {
            payHex.insert(ii, ' ');
        }

        QList<QVariant> qvList;
        QList<QStandardItem*> list;

        qvList.append(rowID++);
        qvList.append(QString::number((double)timer->elapsed() / 1000, 'f', 2).toDouble());
        qvList.append(QString::number((double)timer->elapsed() / 1000, 'f', 2));
        qvList.append(frame.frameId());
        qvList.append(QString("0x" + QString::number(frame.frameId(), 16)));
        qvList.append(direction);
        qvList.append(QString::number(frame.payload().size()).toInt());
        qvList.append(QString::fromUtf8(payHex.data(), payHex.size()));

        for (QVariant qvitem : qvList) {
            QStandardItem* item = new QStandardItem();
            item->setData(qvitem, Qt::DisplayRole);
            list.append(item);
        }

        tvModel.appendRow(list);

        currentSortOrder = ui->tv->horizontalHeader()->sortIndicatorOrder();
        currentSortIndicator = ui->tv->horizontalHeader()->sortIndicatorSection();
        ui->tv->sortByColumn(sortIndex, currentSortOrder);
        ui->tv->horizontalHeader()->setSortIndicator(currentSortIndicator, currentSortOrder);

        if (ui->freezeBox->isChecked() == false) {
            ui->tv->scrollToBottom();
        }
    }

    std::unique_ptr<Ui::CanRawViewPrivate> ui;
    std::unique_ptr<QElapsedTimer> timer;
    QStandardItemModel tvModel;
    bool simStarted;

private:
    CanRawView* q_ptr;
    int rowID = 0;
    int prevIndex = 0;
    int sortIndex = 0;
    int currentSortIndicator = 0;
    Qt::SortOrder currentSortOrder = Qt::AscendingOrder;
    QStringList columnsOrder;

    void writeColumnsOrder(QJsonObject& json) const
    {
        assert(ui != nullptr);
        assert(ui->tv != nullptr);

        int ii = 0;
        QJsonArray columnList;
        for (const auto& column : columnsOrder) {
            if (ui->tv->isColumnHidden(ii) == false) {
                columnList.append(column);
            }
            ++ii;
        }
        json["Columns"] = std::move(columnList);
    }

    void writeViewModel(QJsonArray& jsonArray) const
    {
        assert(ui != nullptr);
        assert(ui->tv != nullptr);

        for (auto row = 0; row < tvModel.rowCount(); ++row) {
            QJsonArray lineIter;
            for (auto column = 0; column < tvModel.columnCount(); ++column) {
                if (ui->tv->isColumnHidden(column) == false) {
                    auto pp = tvModel.data(tvModel.index(row, column));
                    lineIter.append(std::move(pp.toString()));
                }
            }
            jsonArray.append(std::move(lineIter));
        }
    }

private slots:
    /**
     * @brief clear
     *
     * This function is used to clear whole table
     */
    void clear() { tvModel.removeRows(0, tvModel.rowCount()); }

    void dockUndock()
    {
        Q_Q(CanRawView);
        emit q->dockUndock();
    }

    void sort(const int clickedIndex)
    {
        currentSortOrder = ui->tv->horizontalHeader()->sortIndicatorOrder();
        sortIndex = clickedIndex;

        if ((ui->tv->model()->headerData(clickedIndex, Qt::Horizontal).toString() == "time")
            || (ui->tv->model()->headerData(clickedIndex, Qt::Horizontal).toString() == "id")) {
            sortIndex = sortIndex - 1;
        }

        if (prevIndex == clickedIndex) {
            if (currentSortOrder == Qt::DescendingOrder) {
                ui->tv->sortByColumn(sortIndex, Qt::DescendingOrder);
                ui->tv->horizontalHeader()->setSortIndicator(clickedIndex, Qt::DescendingOrder);
            } else {
                ui->tv->sortByColumn(0, Qt::AscendingOrder);
                ui->tv->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
                prevIndex = 0;
                sortIndex = 0;
            }
        } else {
            ui->tv->sortByColumn(sortIndex, Qt::AscendingOrder);
            ui->tv->horizontalHeader()->setSortIndicator(clickedIndex, Qt::AscendingOrder);
            prevIndex = clickedIndex;
        }
    }
};
#endif // CANRAWVIEW_P_H
