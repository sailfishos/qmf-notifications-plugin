/*
 * Copyright (C) 2013-2014 Jolla Ltd.
 * Contact: Valerio Valerio <valerio.valerio@jollamobile.com>
 *
 * This file is part of qmf-notifications-plugin
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "notificationsplugin.h"

// QMF
#include <qmaillog.h>

// Qt
#include <QCoreApplication>
#include <QTranslator>

NotificationsPlugin::NotificationsPlugin(QObject *parent)
    : QMailMessageServerPlugin(parent)
    , _actionObserver(new ActionObserver(this))
{
    QString translationPath("/usr/share/translations/");
    QTranslator *engineeringEnglish = new QTranslator(this);
    engineeringEnglish->load("qmf-notifications_eng_en", translationPath);
    QCoreApplication::instance()->installTranslator(engineeringEnglish);

    QTranslator *translator = new QTranslator(this);
    translator->load(QLocale(), "qmf-notifications", "-", translationPath);
    QCoreApplication::instance()->installTranslator(translator);

    // Initiate after the translator since it will use it
    _mailStoreObserver = new MailStoreObserver(this);
}

NotificationsPlugin::~NotificationsPlugin()
{
}

NotificationsPlugin* NotificationsPlugin::createService()
{
    return this;
}

void NotificationsPlugin::exec()
{
    // Connect actions observer to mail store observer
    // to report when all actions are completed and
    // only then emit notifications.
    connect(_actionObserver, &ActionObserver::actionsCompleted,
            _mailStoreObserver, &MailStoreObserver::publishChanges);
    connect(_actionObserver, &ActionObserver::transmitCompleted,
            _mailStoreObserver, &MailStoreObserver::transmitCompleted);
    connect(_actionObserver, &ActionObserver::transmitFailed,
            _mailStoreObserver, &MailStoreObserver::transmitFailed);
    connect(_mailStoreObserver, &MailStoreObserver::mailStoreChanges,
            [this] () {
                if (!_actionObserver->hasRunningAction()) {
                    _mailStoreObserver->publishChanges();
                }
            });
    qMailLog(Messaging) << "Initiating mail notifications plugin";
}

QString NotificationsPlugin::key() const
{
    return "notifications";
}
