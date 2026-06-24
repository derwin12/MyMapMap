/****************************************************************************
**
** Copyright (C) 2006 Trolltech ASA. All rights reserved.
**
** This file is part of the documentation of Qt. It was originally
** published as part of Qt Quarterly.
**
** This file may be used under the terms of the GNU General Public License
** version 2.0 as published by the Free Software Foundation or under the
** terms of the Qt Commercial License Agreement. The respective license
** texts for these are provided with the open source and commercial
** editions of Qt.
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "fileedit.h"
#include <QHBoxLayout>
#include <QToolButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QFocusEvent>

FileEdit::FileEdit(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    theLineEdit = new QLineEdit(this);
    theLineEdit->setReadOnly(true);
    theLineEdit->setFrame(false);
    theLineEdit->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
    QToolButton *button = new QToolButton(this);
    button->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred));
    button->setFixedWidth(18);
    button->setText(QLatin1String("..."));
    layout->addWidget(theLineEdit);
    layout->addWidget(button);
    setFocusProxy(button);
    setFocusPolicy(Qt::StrongFocus);
    connect(button, SIGNAL(clicked()),
                this, SLOT(buttonClicked()));
}

QSize FileEdit::sizeHint() const
{
    return QSize(100, theLineEdit->sizeHint().height());
}

void FileEdit::setFilePath(const QString &filePath)
{
    if (theFilePath == filePath)
        return;
    theFilePath = filePath;
    theLineEdit->setText(QFileInfo(filePath).fileName());
}

void FileEdit::buttonClicked()
{
#ifdef Q_OS_LINUX
    QString filePath = QFileDialog::getOpenFileName(this, tr("Choose a file"), theFilePath, filter(), nullptr, QFileDialog::DontUseNativeDialog);
#else
    QString filePath = QFileDialog::getOpenFileName(this, tr("Choose a file"), theFilePath, filter());
#endif

    if (filePath.isNull())
        return;
    setFilePath(filePath);
    emit filePathChanged(filePath);
}

void FileEdit::focusInEvent(QFocusEvent *e)
{
    theLineEdit->event(e);
    if (e->reason() == Qt::TabFocusReason || e->reason() == Qt::BacktabFocusReason) {
        theLineEdit->selectAll();
    }
    QWidget::focusInEvent(e);
}

void FileEdit::focusOutEvent(QFocusEvent *e)
{
    theLineEdit->event(e);
    QWidget::focusOutEvent(e);
}

void FileEdit::keyPressEvent(QKeyEvent *e)
{
    theLineEdit->event(e);
}

void FileEdit::keyReleaseEvent(QKeyEvent *e)
{
    theLineEdit->event(e);
}
