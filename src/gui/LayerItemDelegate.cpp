/*
 * MppingItemDelegate.h
 *
 * (c) 2016 Dame Diongue -- baydamd(@)gmail(.)com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "LayerItemDelegate.h"
#include <QPainterPath>

namespace mmp {

LayerItemDelegate::LayerItemDelegate(QObject *parent) :
  QStyledItemDelegate(parent)
{

}

void LayerItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  painter->setRenderHint(QPainter::Antialiasing);

  if (index.isValid())
  {
    QRect rect = option.rect;
    int x = rect.x();
    int y = rect.y();

    if (option.state & QStyle::State_Selected)
    {
      painter->fillRect(rect, option.palette.color(QPalette::Highlight));
    }

    if (index.column() == MM::HideColumn)
    {
      bool isVisible = index.model()->data(index, Qt::CheckStateRole).toBool();
      if (isVisible)
      {
        QStyleOptionToolButton mappingMuteButton;
        mappingMuteButton.state |= QStyle::State_Enabled;
        mappingMuteButton.rect = QRect(x + 4, y + 12,
                MM::MAPPING_LIST_ICON_SIZE, MM::MAPPING_LIST_ICON_SIZE);
        mappingMuteButton.icon = MM::themedIcon(":/visible-mapping");
        mappingMuteButton.iconSize = QSize(MM::MAPPING_LIST_ICON_SIZE,
                MM::MAPPING_LIST_ICON_SIZE);

        QApplication::style()->drawControl(
              QStyle::CE_ToolButtonLabel, &mappingMuteButton, painter);
      }
    }

    if (index.column() == MM::IconAndNameColum)
    {
      // Draw Icon
      QIcon mappingIcon = qvariant_cast<QIcon>(index.model()->data(index,
                  Qt::DecorationRole));
      QPixmap iconPixmap = mappingIcon.pixmap(24, 24);
      QRect iconRect(x + 5, y, 24, rect.height());
      QApplication::style()->drawItemPixmap(painter, iconRect,
              Qt::AlignLeft | Qt::AlignVCenter, iconPixmap);

      // Draw Text
      QString mappingName = index.model()->data(index, Qt::EditRole).toString();
      QRect textRect(x + 40, y, rect.width() - 40, rect.height());
      QPalette::ColorRole textRole = (option.state & QStyle::State_Selected)
                                        ? QPalette::HighlightedText : QPalette::Text;

      QApplication::style()->drawItemText(painter, textRect,
              Qt::AlignLeft | Qt::AlignVCenter,
              option.palette, true, mappingName, textRole);
    }

    if (index.column() == MM::GroupButtonColum)
    {
      bool isSolo = index.model()->data(index, Qt::CheckStateRole + 1).toBool();
      bool isLocked = index.model()->data(index, Qt::CheckStateRole + 2).toBool();
      // Here is need to always align button on right side
      int buttonX = rect.width() + x;

      // Create all mappings buttons:
      // solo button
      QStyleOptionToolButton mappingSoloButton;
      mappingSoloButton.state |= QStyle::State_Enabled;
      mappingSoloButton.rect = QRect(buttonX - 118, y + 12,
              MM::MAPPING_LIST_ICON_SIZE, MM::MAPPING_LIST_ICON_SIZE);
      mappingSoloButton.icon = MM::themedIcon(":/solo-mapping");
      mappingSoloButton.iconSize = QSize(MM::MAPPING_LIST_ICON_SIZE,
              MM::MAPPING_LIST_ICON_SIZE);
      mappingSoloButton.text = tr("Solo mapping");
      //TODO: show tooltip
      //mappingSoloButton.toolButtonStyle = Qt::ToolButtonTextUnderIcon;

      // lock button
      QStyleOptionToolButton mappingLockButton;
      mappingLockButton.state |= QStyle::State_Enabled;
      mappingLockButton.rect = QRect(buttonX - 88, y + 12, MM::MAPPING_LIST_ICON_SIZE,
              MM::MAPPING_LIST_ICON_SIZE);
      mappingLockButton.icon = MM::themedIcon(":/lock-mapping");
      mappingLockButton.iconSize = QSize(MM::MAPPING_LIST_ICON_SIZE,
              MM::MAPPING_LIST_ICON_SIZE);
      mappingLockButton.text = tr("Lock mapping");
      //TODO: show tooltip
      //mappingLockButton.toolButtonStyle = Qt::ToolButtonTextUnderIcon;

      // duplicate button
      QStyleOptionToolButton mappingDuplicateButton;
      mappingDuplicateButton.state |= QStyle::State_Enabled;
      mappingDuplicateButton.rect = QRect(buttonX - 58, y + 12,
              MM::MAPPING_LIST_ICON_SIZE, MM::MAPPING_LIST_ICON_SIZE);
      mappingDuplicateButton.icon = MM::themedIcon(":/duplicate-mapping");
      mappingDuplicateButton.iconSize = QSize(MM::MAPPING_LIST_ICON_SIZE,
              MM::MAPPING_LIST_ICON_SIZE);
      mappingDuplicateButton.text = tr("Duplicate mapping");
      //TODO: show tooltip
      //mappingDuplicateButton.toolButtonStyle = Qt::ToolButtonTextUnderIcon;

      // delete button
      QStyleOptionToolButton mappingDeleteButton;
      mappingDeleteButton.state |= QStyle::State_Enabled;
      mappingDeleteButton.rect = QRect(buttonX - 28, y + 12,
              MM::MAPPING_LIST_ICON_SIZE, MM::MAPPING_LIST_ICON_SIZE);
      mappingDeleteButton.icon = MM::themedIcon(":/delete-mapping");
      mappingDeleteButton.iconSize = QSize(MM::MAPPING_LIST_ICON_SIZE,
              MM::MAPPING_LIST_ICON_SIZE);
      mappingDeleteButton.text = tr("Delete mapping");
      //TODO: show tooltip
      //mappingDeleteButton.toolButtonStyle = Qt::ToolButtonTextUnderIcon;

      if (isSolo)
      {
        QPainterPath mappingSoloPanel;
        mappingSoloPanel.addRoundedRect(mappingSoloButton.rect.adjusted(
              -2, -2, 2, 2), 4, 4);
        painter->setPen(QPen()); // No pen
        painter->fillPath(mappingSoloPanel, MM::DARK_BLUE);
        painter->drawPath(mappingSoloPanel);
      }
      QApplication::style()->drawControl( QStyle::CE_ToolButtonLabel,
              &mappingSoloButton, painter);

      if (isLocked)
      {
        QPainterPath mappingLockPanel;
        mappingLockPanel.addRoundedRect(QRectF(mappingLockButton.rect.adjusted(
              -2, -2, 2, 2)), 4, 4);
        painter->setPen(QPen()); // No pen
        painter->fillPath(mappingLockPanel, MM::DARK_BLUE);
        painter->drawPath(mappingLockPanel);
      }
      QApplication::style()->drawControl(
            QStyle::CE_ToolButtonLabel, &mappingLockButton, painter);

      QApplication::style()->drawControl(
            QStyle::CE_ToolButtonLabel, &mappingDuplicateButton, painter);
      QApplication::style()->drawControl(
            QStyle::CE_ToolButtonLabel, &mappingDeleteButton, painter);
    }
  }
  else
  {
    QStyledItemDelegate::paint(painter, option, index);
  }
}

QWidget *LayerItemDelegate::createEditor(QWidget *parent,
        const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  if (index.column() == MM::IconAndNameColum)
  {
    QLineEdit *editor = new QLineEdit(parent);
    editor->setFixedHeight(option.rect.height());
    editor->setContentsMargins(option.rect.x() + 4, 0, 0, 0);
    return editor;
  }
  else
  {
    return nullptr;
  }
}

void LayerItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
  QString value = index.model()->data(index, Qt::EditRole).toString();

  QLineEdit *nameEdit = static_cast<QLineEdit*>(editor); // TODO: use reinterpret_cast instead static_cast
  nameEdit->setText(value);
}

void LayerItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
  QLineEdit *nameEdit = static_cast<QLineEdit*>(editor); // TODO: use reinterpret_cast instead static_cast
  model->setData(index, nameEdit->text(), Qt::EditRole);
}

void LayerItemDelegate::updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  Q_UNUSED(index)
  editor->setGeometry(option.rect);
}

bool LayerItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
        const QStyleOptionViewItem &option, const QModelIndex &index)
{
  QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
  QRect rect = option.rect;
  int x = rect.x();
  int y = rect.y();
  int buttonX = rect.width() + x;

  QRect hideButtonRect = QRect(x + 4, y + 12, MM::MAPPING_LIST_ICON_SIZE,
          MM::MAPPING_LIST_ICON_SIZE);
  QRect soloButtonRect = QRect(buttonX - 118, y + 12, MM::MAPPING_LIST_ICON_SIZE,
          MM::MAPPING_LIST_ICON_SIZE);
  QRect lockButtonRect = QRect(buttonX - 88, y + 12, MM::MAPPING_LIST_ICON_SIZE,
          MM::MAPPING_LIST_ICON_SIZE);
  QRect duplicateButtonRect = QRect(buttonX - 58, y + 12, MM::MAPPING_LIST_ICON_SIZE,
          MM::MAPPING_LIST_ICON_SIZE);
  QRect deleteButtonRect = QRect(buttonX - 28, y + 12, MM::MAPPING_LIST_ICON_SIZE,
          MM::MAPPING_LIST_ICON_SIZE);

  if (event->type() == QEvent::MouseMove)
  {
    // TODO: handle tooltip
  }
  else if (event->type() == QEvent::MouseButtonPress)
  {
    if (mouseEvent->buttons() & Qt::LeftButton && index.isValid())
    {
      if (index.column() == MM::HideColumn)
      {
        if (hideButtonRect.contains(mouseEvent->pos()))
        {
          model->setData(index, !(index.data(Qt::CheckStateRole).toBool()),
                  Qt::CheckStateRole);
        }
      }
      if (index.column() == MM::GroupButtonColum)
      {
        if (soloButtonRect.contains(mouseEvent->pos()))
        {
          model->setData(index, !(index.data(Qt::CheckStateRole + 1).toBool()),
                  Qt::CheckStateRole + 1);
        }
        else if (lockButtonRect.contains(mouseEvent->pos()))
        {
          model->setData(index, !(index.data(Qt::CheckStateRole + 2).toBool()),
                  Qt::CheckStateRole + 2);
        }
        else if (duplicateButtonRect.contains(mouseEvent->pos()))
        {
          emit itemDuplicated(index.data(Qt::UserRole).toInt());
        }
        else if (deleteButtonRect.contains(mouseEvent->pos()))
        {
          emit itemRemoved(index.data(Qt::UserRole).toInt());
        }
      }
    }
    else if (mouseEvent->buttons() & Qt::RightButton && index.isValid())
    {
      emit itemContextMenuRequested(mouseEvent->pos());
    }
  }
  return QAbstractItemDelegate::editorEvent(event, model, option, index);
}

bool LayerItemDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view,
        const QStyleOptionViewItem &option, const QModelIndex &index)
{
  if (event->type() == QEvent::ToolTip && index.isValid())
  {
    QRect rect = option.rect;
    int x = rect.x();
    int y = rect.y();
    int buttonX = rect.width() + x;

    if (index.column() == MM::HideColumn)
    {
      QRect hideButtonRect = QRect(x + 4, y + 12, MM::MAPPING_LIST_ICON_SIZE,
              MM::MAPPING_LIST_ICON_SIZE);
      if (hideButtonRect.contains(event->pos()))
      {
        QToolTip::showText(event->globalPos(), tr("Show/hide layer"), view->viewport());
        return true;
      }
    }
    else if (index.column() == MM::GroupButtonColum)
    {
      QRect soloButtonRect = QRect(buttonX - 118, y + 12, MM::MAPPING_LIST_ICON_SIZE,
              MM::MAPPING_LIST_ICON_SIZE);
      QRect lockButtonRect = QRect(buttonX - 88, y + 12, MM::MAPPING_LIST_ICON_SIZE,
              MM::MAPPING_LIST_ICON_SIZE);
      QRect duplicateButtonRect = QRect(buttonX - 58, y + 12, MM::MAPPING_LIST_ICON_SIZE,
              MM::MAPPING_LIST_ICON_SIZE);
      QRect deleteButtonRect = QRect(buttonX - 28, y + 12, MM::MAPPING_LIST_ICON_SIZE,
              MM::MAPPING_LIST_ICON_SIZE);

      if (soloButtonRect.contains(event->pos()))
        QToolTip::showText(event->globalPos(), tr("Solo this layer"), view->viewport());
      else if (lockButtonRect.contains(event->pos()))
        QToolTip::showText(event->globalPos(), tr("Lock this layer"), view->viewport());
      else if (duplicateButtonRect.contains(event->pos()))
        QToolTip::showText(event->globalPos(), tr("Duplicate this layer"), view->viewport());
      else if (deleteButtonRect.contains(event->pos()))
        QToolTip::showText(event->globalPos(), tr("Delete this layer"), view->viewport());
      else
        return QStyledItemDelegate::helpEvent(event, view, option, index);
      return true;
    }
  }
  return QStyledItemDelegate::helpEvent(event, view, option, index);
}

}
