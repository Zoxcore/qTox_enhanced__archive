/*
    Copyright © 2014-2019 by The qTox Project Contributors

    This file is part of qTox, a Qt-based graphical interface for Tox.

    qTox is libre software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    qTox is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with qTox.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "src/net/updatecheck.h"
#include "src/persistence/settings.h"

#include <QNetworkAccessManager>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QObject>
#include <QRegularExpression>
#include <QTimer>
#include <cassert>

namespace {
const QString versionUrl{QStringLiteral("https://api.github.com/repos/qTox/qTox/releases/latest")};
const QString versionRegexString{QStringLiteral("v([0-9]+)\\.([0-9]+)\\.([0-9]+)")};

struct Version {
    int major;
    int minor;
    int patch;
};

} // namespace

UpdateCheck::UpdateCheck(const Settings& settings_)
    : settings(settings_)
{
    qInfo() << "qTox is running version:" << GIT_DESCRIBE;
    qInfo() << "qTox UpdateCheck DISABLED";
}

void UpdateCheck::checkForUpdate()
{
    qInfo() << "qTox checkForUpdate DISABLED";
}
