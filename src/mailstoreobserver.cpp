/*
 * Copyright (c) 2013 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
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

#include "mailstoreobserver.h"

// nemoemail-qt5
#include <emailagent.h>

// Qt
#include <QDBusConnection>
#include <QDebug>

namespace {

const auto dbusService QStringLiteral("com.jolla.email.ui");
const auto dbusPath = QStringLiteral("/com/jolla/email/ui");
const auto dbusInterface = QStringLiteral("com.jolla.email.ui");

const auto publishedMessageId = QStringLiteral("x-nemo.email.published-message-id");
const auto markAsReadAction = QStringLiteral("markAsRead");

QVariant remoteAction(const QString &name, const QString &displayName, const QString &method, const QVariantList &arguments = QVariantList())
{
    return Notification::remoteAction(name, displayName, dbusService, dbusPath, dbusInterface, method, arguments);
}

QVariantList singleMessageRemoteActionList(Notification *notification, const MessageInfo &messageInfo)
{
    const int messageId = static_cast<int>(messageInfo.id.toULongLong());
    QVariantList messageArg(QVariantList () << messageId);

    QVariantList actions;
    actions << ::remoteAction("default", QString(), "openMessage", messageArg);

    if (messageInfo.hasMultipleRecipients) {
        //: Reply to all recipients of this email
        //% "Reply all"
        actions << ::remoteAction(QString(), qtTrId("qmf-notification_reply_all"), "replyAllToMessage", messageArg);
    } else {
        //: Reply to this email
        //% "Reply"
        actions << ::remoteAction(QString(), qtTrId("qmf-notification_reply_one"), "replyToMessage", messageArg);
    }

    //: Mark an email as "read"
    //% "Mark as read"
    actions << Notification::remoteAction(markAsReadAction, qtTrId("qmf-notification_mark_as_read"));
    notification->setProperty("messageId", messageId);

    return actions;
}

void initNotification(Notification *notification)
{
    if (!notification) {
        return;
    }

    //% "Mail"
    notification->setAppName(qtTrId("qmf-notification_mail"));
    notification->setAppIcon("icon-lock-email");
    notification->setHintValue(QStringLiteral("x-nemo-display-on"), true);
    notification->setHintValue("x-nemo-priority", 100);
}

QPair<QString, QString> accountProperties(const QMailAccountId &accountId)
{
    static QHash<QMailAccountId, QPair<QString, QString> > properties;

    QHash<QMailAccountId, QPair<QString, QString> >::iterator it = properties.find(accountId);
    if (it == properties.end()) {
        // Add the properties for this account
        QMailAccount account(accountId);
        it = properties.insert(accountId, qMakePair(account.name(), account.iconPath()));
    }
    return *it;
}

struct MessageEarlierComparator {
    bool operator()(const MessageInfo *lhs, const MessageInfo *rhs)
    {
        return lhs->timeStamp < rhs->timeStamp;
    }
};

}

MailStoreObserver::MailStoreObserver(QObject *parent) :
    QObject(parent),
    _publicationChanges(false),
    _appOnScreen(false),
    _storage(0)
{
    _storage = QMailStore::instance();

    connect(_storage, SIGNAL(messagesAdded(const QMailMessageIdList&)),
            this, SLOT(addMessages(const QMailMessageIdList&)));
    connect(_storage, SIGNAL(messagesUpdated(const QMailMessageIdList&)),
            this, SLOT(updateMessages(const QMailMessageIdList&)));
    connect(_storage, SIGNAL(messagesRemoved(const QMailMessageIdList&)),
            this, SLOT(removeMessages(const QMailMessageIdList&)));

    reloadNotifications();

    QDBusConnection dbusSession(QDBusConnection::sessionBus());
    dbusSession.connect(QString(), dbusPath, dbusInterface, "displayEntered", this, SLOT(setNotifyOff()));
    dbusSession.connect(QString(), dbusPath, dbusInterface, "displayExit", this, SLOT(setNotifyOn()));
    dbusSession.connect(QString(), dbusPath, dbusInterface, "combinedInboxDisplayed", this, SLOT(combinedInboxDisplayed()));
    dbusSession.connect(QString(), dbusPath, dbusInterface, "accountInboxDisplayed", this, SLOT(accountInboxDisplayed(int)));
}

void MailStoreObserver::reloadNotifications()
{
    const QMailAccountIdList enabledAccounts(QMailStore::instance()->queryAccounts(QMailAccountKey::messageType(QMailMessage::Email)
                                                                                 & QMailAccountKey::status(QMailAccount::Enabled)));

    clearFoldersToSync();
    // Find the set of messages we've previously published notifications for
    QList<QObject *> existingNotifications(Notification::notifications());
    for (QObject *obj : existingNotifications) {
        if (Notification *notification = qobject_cast<Notification *>(obj)) {
            const QString publishedId(notification->hintValue(publishedMessageId).toString());
            const QMailMessageId messageId(QMailMessageId(publishedId.toULongLong()));

            bool published = false;
            if (messageId.isValid()) {
                const QMailMessageMetaData message(messageId);

                // Checks if parent account is still valid
                // accounts can be removed when messageServer is not running.
                if (enabledAccounts.contains(message.parentAccountId())) {
                    if (notifyMessage(message)) {
                        _publishedMessages.insert(messageId, constructMessageInfo(message));
                        published = true;
                    }
                }
            }

            if (!published) {
                notification->close();
            }
        }
    }
    qDeleteAll(existingNotifications);
}

// Close existent notification
void MailStoreObserver::closeNotifications()
{
    QList<QObject *> existingNotifications(Notification::notifications());
    for (QObject *obj : existingNotifications) {
        if (Notification *notification = qobject_cast<Notification *>(obj)) {
            notification->close();
        }
    }
    qDeleteAll(existingNotifications);

    _publishedMessages.clear();
}

void MailStoreObserver::closeAccountNotifications(const QMailAccountId &accountId)
{
    QList<QObject *> existingNotifications(Notification::notifications());
    for (QObject *obj : existingNotifications) {
        if (Notification *notification = qobject_cast<Notification *>(obj)) {
            const QString publishedId(notification->hintValue(publishedMessageId).toString());
            const QMailMessageId messageId(QMailMessageId(publishedId.toULongLong()));
            if (messageId.isValid()) {
                const QMailMessageMetaData message(messageId);
                if (message.parentAccountId() == accountId) {
                    notification->close();
                    _publishedMessages.remove(messageId);
                }
            }
        }
    }
    qDeleteAll(existingNotifications);
}

// Contructs messageInfo object from a email message
QSharedPointer<MessageInfo> MailStoreObserver::constructMessageInfo(const QMailMessageMetaData &message)
{
    MessageInfo* messageInfo = new MessageInfo();
    messageInfo->id = message.id();

    QMailAddress mailAdress = message.from();

    messageInfo->origin = mailAdress.address().toLower();
    messageInfo->sender = mailAdress.name();
    messageInfo->subject = message.subject();
    messageInfo->timeStamp = message.date().toUTC();
    messageInfo->accountId = message.parentAccountId();
    messageInfo->hasMultipleRecipients = message.recipients().count() > 1;

    return QSharedPointer<MessageInfo>(messageInfo);
}

// Check if this message should be notified, old messages are not
// notified since QMailMessage::NoNotification is used
bool MailStoreObserver::notifyMessage(const QMailMessageMetaData &message)
{
    if (message.messageType()==QMailMessage::Email &&
        !(message.status() & QMailMessage::Read) &&
        !(message.status() & QMailMessage::Temporary) &&
        !(message.status() & QMailMessage::NoNotification) &&
        !(message.status() & QMailMessage::Junk) &&
        !(message.status() & QMailMessage::Trash) &&
        messageInFolderToSync(message)) {
        return true;
    } else {
        return false;
    }
}

// App is on screen beep only
void MailStoreObserver::notifyOnly()
{
    Notification notification;
    initNotification(&notification);
    notification.setIsTransient(true);
    notification.setHintValue("x-nemo-feedback", QStringLiteral("email"));
    notification.publish();
}

void MailStoreObserver::updateNotifications()
{
    QHash<QMailMessageId, int> existingMessageNotificationIds;

    // Limit the maximum number of notifications published for any each account
    if (_publishedMessages.count() > MaxNotificationsPerAccount) {
        QHash<QMailAccountId, QList<const MessageInfo *> > accountMessages;

        MessageHash::const_iterator it = _publishedMessages.constBegin(), end = _publishedMessages.constEnd();
        for ( ; it != end; ++it) {
            const MessageInfo *message(it.value().data());
            accountMessages[message->accountId].append(message);
        }

        QList<QMailMessageId> messagesToRemove;

        QHash<QMailAccountId, QList<const MessageInfo *> >::iterator ait = accountMessages.begin(), aend = accountMessages.end();
        for ( ; ait != aend; ++ait) {
            QList<const MessageInfo *> &messages(ait.value());
            if (messages.count() > MaxNotificationsPerAccount) {
                // Remove the notifications for the earliest messages for this account
                std::sort(messages.begin(), messages.end(), MessageEarlierComparator());

                int removeCount = messages.count() - MaxNotificationsPerAccount;
                QList<const MessageInfo *>::const_iterator mit = messages.constBegin(), mend = mit + removeCount;
                for ( ; mit != mend; ++mit) {
                    messagesToRemove.append((*mit)->id);
                }
            }
        }

        for (const QMailMessageId &id : messagesToRemove) {
            _publishedMessages.remove(id);
        }
    }

    // Remove any existing notifications whose message should no longer be published
    QList<QObject *> existingNotifications(Notification::notifications());
    for (QObject *obj : existingNotifications) {
        if (Notification *notification = qobject_cast<Notification *>(obj)) {
            const QString publishedId(notification->hintValue(publishedMessageId).toString());
            const QMailMessageId messageId(QMailMessageId(publishedId.toULongLong()));
            if (!_publishedMessages.contains(messageId)) {
                notification->close();
            } else {
                existingMessageNotificationIds.insert(messageId, notification->replacesId());
            }
        }
    }
    qDeleteAll(existingNotifications);

    // Update the notification for each current message that has been modified
    MessageHash::const_iterator it = _publishedMessages.constBegin(), end = _publishedMessages.constEnd();
    for ( ; it != end; ++it) {
        const QMailMessageId messageId(it.key());
        const MessageInfo *message(it.value().data());

        // Only republish this message if there is something to show
        if (!_newMessages.contains(messageId))
            continue;

        Notification *notification = new Notification(this);
        connect(notification, &Notification::closed,
                this, &MailStoreObserver::notificationClosed);
        connect(notification, &Notification::actionInvoked,
                this, &MailStoreObserver::notificationActionInvoked);

        // Group emails by their source account name
        QPair<QString, QString> properties(accountProperties(message->accountId));

        initNotification(notification);
        notification->setAppName(properties.first);
        notification->setAppIcon(properties.second);
        notification->setHintValue("x-nemo-feedback", "email_exists");
        notification->setHintValue(publishedMessageId, QString::number(messageId.toULongLong()));
        notification->setSummary(message->sender.isEmpty() ? message->origin : message->sender);
        notification->setBody(message->subject);
        notification->clearPreviewSummary();
        notification->clearPreviewBody();
        notification->setTimestamp(message->timeStamp);
        notification->setRemoteActions(singleMessageRemoteActionList(notification, *message));

        QHash<QMailMessageId, int>::iterator existingNotif(existingMessageNotificationIds.find(messageId));
        if (existingNotif != existingMessageNotificationIds.end()) {
            // Replace the existing notification for this message
            notification->setReplacesId(existingNotif.value());
        }

        notification->publish();
    }
}

// ################ Slots #####################

void MailStoreObserver::actionsCompleted()
{
    if (_publicationChanges) {
        _publicationChanges = false;

        updateNotifications();

        if (!_newMessages.isEmpty()) {
            // Notify the user of new messages
            if (_appOnScreen) {
                notifyOnly();
            } else {
                Notification *summaryNotification = new Notification(this);
                connect(summaryNotification, &Notification::closed,
                        this, &MailStoreObserver::notificationClosed);
                connect(summaryNotification, &Notification::actionInvoked,
                        this, &MailStoreObserver::notificationActionInvoked);

                initNotification(summaryNotification);
                summaryNotification->setIsTransient(true);
                summaryNotification->setHintValue("x-nemo-feedback", QStringLiteral("email"));

                if (_newMessages.count() == 1) {
                    const QMailMessageId messageId(*_newMessages.begin());
                    const MessageInfo *message(_publishedMessages[messageId].data());

                    summaryNotification->setPreviewSummary(message->sender.isEmpty() ? message->origin : message->sender);
                    summaryNotification->setPreviewBody(message->subject);
                    summaryNotification->setRemoteActions(singleMessageRemoteActionList(summaryNotification, *message));

                    // Override the icon to be the icon associated with this account
                    QMailAccount account(message->accountId);
                    summaryNotification->setAppIcon(account.iconPath());
                } else {
                    //: Summary of new email(s) notification
                    //% "You have %n new email(s)"
                    summaryNotification->setPreviewSummary(qtTrId("qmf-notification_new_email_banner_notification", _newMessages.count()));

                    // Find if these messages are all for the same account
                    QMailAccountId firstAccountId;
                    for (const QMailMessageId &messageId : _newMessages) {
                        const MessageInfo *message(_publishedMessages[messageId].data());
                        if (!firstAccountId.isValid()) {
                            firstAccountId = message->accountId;
                        } else if (message->accountId != firstAccountId) {
                            firstAccountId = QMailAccountId();
                            break;
                        }
                    }

                    if (firstAccountId.isValid()) {
                        // Show the inbox for this account
                        const QVariant varId(static_cast<int>(firstAccountId.toULongLong()));
                        summaryNotification->setRemoteAction(::remoteAction("default", QString(), "openInbox", QVariantList() << varId));

                        // Also override the icon to be the icon associated with this account
                        QMailAccount account(firstAccountId);
                        summaryNotification->setAppIcon(account.iconPath());
                    } else {
                        // Multiple accounts - show the combined inbox
                        summaryNotification->setRemoteAction(::remoteAction("default", QString(), "openCombinedInbox"));
                    }
                }

                summaryNotification->publish();
            }

            _newMessages.clear();
        }
    }
}

void MailStoreObserver::notificationClosed(uint reason)
{
    Q_UNUSED(reason)
    Notification *notification = qobject_cast<Notification*>(sender());
    notification->deleteLater();
}

void MailStoreObserver::notificationActionInvoked(const QString &name)
{
    Notification *notification = qobject_cast<Notification*>(sender());
    const int messageId = notification->property("messageId").toInt();
    if (name == markAsReadAction) {
        EmailAgent *agent = EmailAgent::instance();
        agent->markMessageAsRead(messageId);
    }
}

void MailStoreObserver::addMessages(const QMailMessageIdList &ids)
{
    clearFoldersToSync();

    for (const QMailMessageId &id : ids) {
        const QMailMessageMetaData message(id);

        // Workaround for plugin that try to add same message twice
        if (notifyMessage(message) && !_publishedMessages.contains(id)) {
            _publishedMessages.insert(id, constructMessageInfo(message));
            _newMessages.insert(id);
            _publicationChanges = true;
        }
    }
}

void MailStoreObserver::removeMessages(const QMailMessageIdList &ids)
{
    for (const QMailMessageId &id : ids) {
        if (_publishedMessages.contains(id)) {
            _publishedMessages.remove(id);
            _newMessages.remove(id);
            _publicationChanges = true;
        }
    }
    // Local action not handled by action observer
    actionsCompleted();
}

void MailStoreObserver::updateMessages(const QMailMessageIdList &ids)
{
    // TODO: notify messages that we already have and change the status
    // from read to unread ???
    clearFoldersToSync();

    for (const QMailMessageId &id : ids) {
        if (_publishedMessages.contains(id)) {
            // Check if message was read
            const QMailMessageMetaData message(id);
            if (!notifyMessage(message)) {
                _publishedMessages.remove(id);
                _newMessages.remove(id);
                _publicationChanges = true;
            }
        }
    }
}

void MailStoreObserver::transmitCompleted(const QMailAccountId &accountId)
{
    const QVariant acctId(accountId.toULongLong());

    // If there is an existing failure for this notification, remove it
    QList<QObject *> existingNotifications(Notification::notifications());
    for (QObject *obj : existingNotifications) {
        if (Notification *notification = qobject_cast<Notification *>(obj)) {
            if (notification->hintValue("x-nemo.email.sendFailed-accountId") == accountId) {
                notification->close();
                break;
            }
        }
    }
    qDeleteAll(existingNotifications);
}

void MailStoreObserver::transmitFailed(const QMailAccountId &accountId)
{
    // Check if there are messages queued to send, transmition failed can be emitted for account testing or by other processes
    // working with the mail store.
    QMailMessageKey outboxFilter(QMailMessageKey::status(QMailMessage::Outbox) & ~QMailMessageKey::status(QMailMessage::Trash));
    QMailMessageKey accountKey(QMailMessageKey::parentAccountId(accountId));
    if (!QMailStore::instance()->countMessages(accountKey & outboxFilter)) {
        return;
    }

    QMailAccount account(accountId);
    QString accountName = account.name();
    QVariant acctId = static_cast<int>(accountId.toULongLong());

    //: Summary of email sending failed notification
    //% "Email sending failed"
    QString summary = qtTrId("qmf-notification_send_failed_summary");
    //: Body of email sending failed notification
    //% "Account %1"
    QString body = qtTrId("qmf-notification_send_failed_Body").arg(accountName);

    Notification sendFailure;
    initNotification(&sendFailure);
    sendFailure.setHintValue("x-nemo.email.sendFailed-accountId", accountId.toULongLong());
    sendFailure.setSummary(summary);
    sendFailure.setBody(body);
    sendFailure.setRemoteAction(::remoteAction("default", QString(), "openOutbox", QVariantList() << acctId));

    // If there is an existing failure for this notification, replace it
    QList<QObject *> existingNotifications(Notification::notifications());
    for (QObject *obj : existingNotifications) {
        if (Notification *notification = qobject_cast<Notification *>(obj)) {
            if (notification->hintValue("x-nemo.email.sendFailed-accountId") == accountId) {
                sendFailure.setReplacesId(notification->replacesId());
                break;
            }
        }
    }
    qDeleteAll(existingNotifications);

    sendFailure.publish();
}

void MailStoreObserver::setNotifyOn()
{
    _appOnScreen = false;
}

void MailStoreObserver::setNotifyOff()
{
    _appOnScreen = true;
}

void MailStoreObserver::combinedInboxDisplayed()
{
    closeNotifications();
}

void MailStoreObserver::accountInboxDisplayed(int accountId)
{
    QMailAccountId acctId(accountId);
    if (acctId.isValid()) {
        closeAccountNotifications(acctId);
    }
}

void MailStoreObserver::clearFoldersToSync()
{
    _tempFoldersToSync.clear();
}

bool MailStoreObserver::messageInFolderToSync(const QMailMessageMetaData &message)
{
    QMailAccountId accountId = message.parentAccountId();
    QMailFolderId folderId = message.parentFolderId();

    // Optimise by only getting the folder list once for each account
    if (_tempFoldersToSync.contains(accountId)) {
        QList<QMailFolderId> const &folders = _tempFoldersToSync.value(accountId);
        return folders.contains(folderId);
    } else {
        QList<QMailFolderId> const &folders = QMailAccount(accountId).foldersToSync();
        _tempFoldersToSync.insert(accountId, folders);
        return folders.contains(folderId);
    }
}

