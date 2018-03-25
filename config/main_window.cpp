/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXQt - a lightweight, Qt based, desktop toolset
 * http://razor-qt.org
 *
 * Copyright: 2013 Razor team
 * Authors:
 *   Kuzma Shapran <kuzma.shapran@gmail.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "main_window.h"
#include "actions.h"
#include "default_model.h"
#include "edit_action_dialog.h"

#include <QItemSelectionModel>
#include <QSortFilterProxyModel>


MainWindow::MainWindow(QWidget *parent)
    : QDialog(parent)
    , mEditActionDialog(0)
{
    setupUi(this);

    QColor grayedOutColour(actions_TV->palette().color(actions_TV->foregroundRole()));
    QColor backgroundColour(actions_TV->palette().color(actions_TV->backgroundRole()));
    grayedOutColour.toHsl();
    backgroundColour.toHsl();
    grayedOutColour.setHslF(grayedOutColour.hslHueF(), grayedOutColour.hslSaturationF(), (grayedOutColour.lightnessF() + backgroundColour.lightnessF() * 3) / 4.0);
    grayedOutColour.toRgb();

    QFont highlightedFont(actions_TV->font());
    QFont italicFont(actions_TV->font());
    QFont highlightedItalicFont(actions_TV->font());
    highlightedFont.setBold(!highlightedFont.bold());
    italicFont.setItalic(!italicFont.italic());
    highlightedItalicFont.setItalic(!highlightedItalicFont.italic());
    highlightedItalicFont.setBold(!highlightedItalicFont.bold());

    mActions = new Actions(this);
    mDefaultModel = new DefaultModel(mActions, grayedOutColour, highlightedFont, italicFont, highlightedItalicFont, this);
    mSortFilterProxyModel = new QSortFilterProxyModel(this);

    mSortFilterProxyModel->setSourceModel(mDefaultModel);
    actions_TV->setModel(mSortFilterProxyModel);

    mSelectionModel = new QItemSelectionModel(actions_TV->model());
    actions_TV->setSelectionModel(mSelectionModel);

    connect(mSelectionModel, SIGNAL(selectionChanged(QItemSelection, QItemSelection)), SLOT(selectionChanged(QItemSelection, QItemSelection)));

    connect(mActions, SIGNAL(daemonDisappeared()), SLOT(daemonDisappeared()));
    connect(mActions, SIGNAL(daemonAppeared()), SLOT(daemonAppeared()));
    connect(mActions, SIGNAL(multipleActionsBehaviourChanged(MultipleActionsBehaviour)), SLOT(multipleActionsBehaviourChanged(MultipleActionsBehaviour)));
}

void MainWindow::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type())
    {
    case QEvent::LanguageChange:
        retranslateUi(this);
        break;
    default:
        break;
    }
}

void MainWindow::daemonDisappeared()
{
    add_PB->setEnabled(false);
    actions_TV->setEnabled(false);
    multipleActionsBehaviour_CB->setEnabled(false);
}

void MainWindow::daemonAppeared()
{
    add_PB->setEnabled(true);
    actions_TV->setEnabled(true);
    multipleActionsBehaviour_CB->setEnabled(true);

    multipleActionsBehaviour_CB->setCurrentIndex(mActions->multipleActionsBehaviour());

    actions_TV->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);

    int m = actions_TV->model()->columnCount() - 1;
    for (int i = 0; i < m; ++i)
    {
        actions_TV->resizeColumnToContents(i);
        actions_TV->setColumnWidth(i, actions_TV->columnWidth(i) + 20);
    }
}

void MainWindow::multipleActionsBehaviourChanged(MultipleActionsBehaviour behaviour)
{
    multipleActionsBehaviour_CB->setCurrentIndex(behaviour);
}

void MainWindow::on_multipleActionsBehaviour_CB_currentIndexChanged(int index)
{
    mActions->setMultipleActionsBehaviour(static_cast<MultipleActionsBehaviour>(index));
}

void MainWindow::selectionChanged(const QItemSelection &/*selected*/, const QItemSelection &/*deselected*/)
{
    QModelIndexList rows = mSelectionModel->selectedRows();

    modify_PB->setEnabled(rows.length() == 1);
    remove_PB->setEnabled(rows.length() != 0);

    bool enableSwap = (rows.length() == 2);
    if (enableSwap)
    {
        QPair<bool, GeneralActionInfo> info0 = mActions->actionById(mDefaultModel->id(mSortFilterProxyModel->mapToSource(rows[0])));
        QPair<bool, GeneralActionInfo> info1 = mActions->actionById(mDefaultModel->id(mSortFilterProxyModel->mapToSource(rows[1])));
        enableSwap = (info0.first && info1.first && (info0.second.shortcut == info1.second.shortcut));
    }
    swap_PB->setEnabled(enableSwap);
}

void MainWindow::on_add_PB_clicked()
{
    editAction(QModelIndex());
}

void MainWindow::on_modify_PB_clicked()
{
    editAction(mSelectionModel->currentIndex());
}

void MainWindow::on_swap_PB_clicked()
{
    QModelIndexList rows = mSelectionModel->selectedRows();
    mActions->swapActions(mDefaultModel->id(mSortFilterProxyModel->mapToSource(rows[0])), mDefaultModel->id(mSortFilterProxyModel->mapToSource(rows[1])));
}

void MainWindow::on_remove_PB_clicked()
{
    const auto rows = mSelectionModel->selectedRows();
    for(const QModelIndex &rowIndex : rows)
        mActions->removeAction(mDefaultModel->id(mSortFilterProxyModel->mapToSource(rowIndex)));
}

void MainWindow::on_actions_TV_doubleClicked(const QModelIndex &index)
{
    switch (index.column())
    {
    case 0:
    {
        qulonglong id = mDefaultModel->id(mSortFilterProxyModel->mapToSource(index));
        mActions->enableAction(id, !mActions->isActionEnabled(id));
    }
        break;

    default:
        editAction(index);
    }
}

void MainWindow::editAction(const QModelIndex &index)
{
    qulonglong id = 0;

    if (index.isValid())
        id = mDefaultModel->id(mSortFilterProxyModel->mapToSource(index));

    if (!mEditActionDialog)
        mEditActionDialog = new EditActionDialog(mActions, this);

    if (mEditActionDialog->load(id))
        mEditActionDialog->exec();
}
