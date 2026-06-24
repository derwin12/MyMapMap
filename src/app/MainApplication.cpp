/*
 * MainApplication.cpp
 *
 * (c) 2014 Sofian Audry -- info(@)sofianaudry(.)com
 * (c) 2014 Alexandre Quessy -- alexandre(@)quessy(.)net
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

#include "MainApplication.h"
#include <QStyleFactory>

namespace mmp {

MainApplication::MainApplication(int &argc, char *argv[])
  : QApplication(argc, argv)
{
#ifdef Q_OS_WIN32
  // Set settings default format
  QSettings::setDefaultFormat(QSettings::IniFormat);
#endif

  // Set application information.
  setApplicationName(MM::APPLICATION_NAME);
  setApplicationVersion(MM::VERSION);
  setOrganizationName(MM::ORGANIZATION_NAME);
  setOrganizationDomain(MM::ORGANIZATION_DOMAIN);
}

MainApplication::~MainApplication()
{
}

void MainApplication::applyTheme(const QString& theme)
{
  qApp->setStyle(QStyleFactory::create("Fusion"));

  if (theme == "light") {
    const QColor bg(0xf2, 0xf2, 0xf2);
    const QColor surface(0xff, 0xff, 0xff);
    const QColor text(0x1e, 0x1e, 0x2e);
    const QColor textDisabled(0xa0, 0xa0, 0xa0);
    const QColor accent(0x29, 0x80, 0xb9);
    const QColor border(0xc8, 0xc8, 0xc8);

    QPalette p;
    p.setColor(QPalette::Window,          bg);
    p.setColor(QPalette::WindowText,      text);
    p.setColor(QPalette::Base,            surface);
    p.setColor(QPalette::AlternateBase,   bg);
    p.setColor(QPalette::ToolTipBase,     surface);
    p.setColor(QPalette::ToolTipText,     text);
    p.setColor(QPalette::Text,            text);
    p.setColor(QPalette::Button,          bg);
    p.setColor(QPalette::ButtonText,      text);
    p.setColor(QPalette::BrightText,      Qt::black);
    p.setColor(QPalette::Link,            accent);
    p.setColor(QPalette::Highlight,       accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::Text,       textDisabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, textDisabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, textDisabled);
    p.setColor(QPalette::Mid,             border);
    p.setColor(QPalette::Dark,            QColor(0xb0, 0xb0, 0xb0));
    p.setColor(QPalette::Shadow,          QColor(0x80, 0x80, 0x80));
    qApp->setPalette(p);
    qApp->setStyleSheet(QString()); // clear dark stylesheet
  } else {
    // Dark theme
    const QColor bg(0x27, 0x2a, 0x36);
    const QColor surface(0x32, 0x35, 0x41);
    const QColor border(0x4c, 0x4f, 0x5b);
    const QColor text(0xf6, 0xf5, 0xf5);
    const QColor textDisabled(0x6a, 0x6d, 0x7c);
    const QColor accent(0x4a, 0x9e, 0xe0);

    QPalette p;
    p.setColor(QPalette::Window,          bg);
    p.setColor(QPalette::WindowText,      text);
    p.setColor(QPalette::Base,            surface);
    p.setColor(QPalette::AlternateBase,   bg);
    p.setColor(QPalette::ToolTipBase,     surface);
    p.setColor(QPalette::ToolTipText,     text);
    p.setColor(QPalette::Text,            text);
    p.setColor(QPalette::Button,          surface);
    p.setColor(QPalette::ButtonText,      text);
    p.setColor(QPalette::BrightText,      Qt::white);
    p.setColor(QPalette::Link,            accent);
    p.setColor(QPalette::Highlight,       accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::Text,       textDisabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, textDisabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, textDisabled);
    p.setColor(QPalette::Mid,             border);
    p.setColor(QPalette::Dark,            bg);
    p.setColor(QPalette::Shadow,          QColor(0x10, 0x12, 0x18));
    qApp->setPalette(p);

    QFile stylesheet(":/stylesheet");
    (void)stylesheet.open(QFile::ReadOnly);
    qApp->setStyleSheet(QLatin1String(stylesheet.readAll()));
  }
}

bool MainApplication::notify(QObject *receiver, QEvent *event)
{
  try
  {
    return QApplication::notify(receiver, event);
  }
  catch (std::exception &ex)
  {
    qDebug() << "std::exception was caught: " << ex.what() << Qt::endl;
    qDebug() << "event type: " << event->type() << Qt::endl;
  }

  return false;
}

}
