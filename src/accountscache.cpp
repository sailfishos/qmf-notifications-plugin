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

#include "accountscache.h"

// accounts-qt
#include <Accounts/Provider>

AccountsCache::AccountsCache(QObject *parent) :
    QObject(parent)
{
    connect(_manager, SIGNAL(accountCreated(Accounts::AccountId)), this, SLOT(accountCreated(Accounts::AccountId)));
    connect(_manager, SIGNAL(accountRemoved(Accounts::AccountId)), this, SLOT(accountRemoved(Accounts::AccountId)));
    connect(_manager, SIGNAL(enabledEvent(Accounts::AccountId)), this, SLOT(enabledEvent(Accounts::AccountId)));
    initCache();
}

void AccountsCache::initCache()
{
    Accounts::AccountIdList accountIDList = _manager->accountListEnabled("e-mail");

    foreach (Accounts::AccountId accountId, accountIDList) {
        Accounts::Account* account = Accounts::Account::fromId(_manager, accountId, this);
        if (account->enabled()) {
            _accountsList.insert(accountId, account);
        } else {
            delete account;
        }
    }
}

bool AccountsCache::isEnabledMailAccount(const Accounts::AccountId accountId)
{
    QScopedPointer<Accounts::Account> account(Accounts::Account::fromId(_manager, accountId, this));
    if (!account)
        return false;

    Accounts::ServiceList emailServices = account->enabledServices();
    // Only enabled email accounts
    if (1 != emailServices.count() || !account->enabled())
        return false;

    return true;
}

// ########## Slots ########################

void AccountsCache::accountCreated(Accounts::AccountId accountId)
{
    if (isEnabledMailAccount(accountId)) {
        Accounts::Account *account = Accounts::Account::fromId(_manager, accountId, this);
        _accountsList.insert(accountId, account);
    }
}

void AccountsCache::accountRemoved(Accounts::AccountId accountId)
{
    if (_accountsList.contains(accountId)) {
        delete _accountsList.take(accountId);
    }
}

void AccountsCache::enabledEvent(Accounts::AccountId accountId)
{
    if (isEnabledMailAccount(accountId)) {
        if (!_accountsList.contains(accountId)) {
            Accounts::Account* account = Accounts::Account::fromId(_manager, accountId, this);
            _accountsList.insert(accountId, account);
        }
    } else if (_accountsList.contains(accountId)) {
        accountRemoved(accountId);
    }

}

// ############### Public functions #####################

AccountInfo AccountsCache::accountInfo(const Accounts::AccountId accountId)
{
    AccountInfo accInfo;

    if (!_accountsList.contains(accountId)) {
        qWarning() << "AccountInfo called with unknown account";
        return accInfo;
    }

    accInfo.name = _accountsList.value(accountId)->displayName();
    Accounts::Provider provider = _manager->provider(_accountsList.value(accountId)->providerName());
    accInfo.providerIcon = QUrl(provider.iconName());
    return accInfo;
}
