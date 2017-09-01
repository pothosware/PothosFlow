// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/MainWindow.hpp"
#include "MainWindow/MainSettings.hpp"
#include "MainWindow/IconUtils.hpp"
#include <Pothos/System.hpp>
#include <Poco/Logger.h>
#include <Poco/Environment.h>
#include <QMessageBox>
#include <QApplication>
#include <QDir>
#include <stdexcept>
#include <cstdlib> //EXIT_FAILURE

static void myQLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Poco::Message::Priority prio(Poco::Message::PRIO_INFORMATION);
    switch (type) {
    case QtDebugMsg: prio = Poco::Message::PRIO_DEBUG; break;
    #if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    case QtInfoMsg: prio = Poco::Message::PRIO_INFORMATION; break;
    #endif
    case QtWarningMsg: prio = Poco::Message::PRIO_WARNING; break;
    case QtCriticalMsg: prio = Poco::Message::PRIO_CRITICAL; break;
    case QtFatalMsg: prio = Poco::Message::PRIO_FATAL; break;
    }
    const Poco::Message message(context.category, msg.toStdString(), prio, context.file, context.line);
    Poco::Logger::get(context.category).log(message);
}

struct MyScopedSyslogListener
{
    MyScopedSyslogListener(void)
    {
        const auto port = Pothos::System::Logger::startSyslogListener();
        //FIXME listener server supports IPv4 only; must forward to IPv4
        Poco::Environment::set("POTHOS_SYSLOG_ADDR", "127.0.0.1:"+port);
    }
    ~MyScopedSyslogListener(void)
    {
        Pothos::System::Logger::stopSyslogListener();
    }
};

int main(int argc, char **argv)
{
    qInstallMessageHandler(myQLogHandler);
    MyScopedSyslogListener syslogListener;

    //did the user specified files on the command line?
    //stash the files so they are loaded into the editor
    //this replaces the currently stored file list
    QStringList files;
    for (int i = 1; i < argc; i++)
    {
        QString file(argv[i]);
        if (file.isEmpty()) continue;
        files.push_back(QDir(file).absolutePath());
    }
    if (not files.isEmpty())
    {
        auto settings = new MainSettings(nullptr);
        settings->setValue("GraphEditorTabs/files", files);
        delete settings;
    }

    //create the entry point to the GUI
    QApplication app(argc, argv);
    app.setApplicationName("Pothos");
    app.setOrganizationName("PothosWare");
    app.setOrganizationDomain("www.pothosware.com");
    app.setApplicationVersion(QString::fromStdString(Pothos::System::getApiVersion()));

    //setup the application icon
    app.setWindowIcon(QIcon(makeIconPath("PothosFlow.png")));

    POTHOS_EXCEPTION_TRY
    {
        //create the main window for the GUI
        MainWindow mainWindow(nullptr);

        //begin application execution
        return app.exec();
    }
    POTHOS_EXCEPTION_CATCH (const Pothos::Exception &ex)
    {
        QMessageBox msgBox(QMessageBox::Critical, "PothosFlow Application Error", QString::fromStdString(ex.displayText()));
        msgBox.exec();
        return EXIT_FAILURE;
    }
}
