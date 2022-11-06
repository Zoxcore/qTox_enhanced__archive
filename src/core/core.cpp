/*
    Copyright © 2013 by Maxim Biro <nurupo.contributions@gmail.com>
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

#include "core.h"
#include "coreav.h"
#include "corefile.h"

#include "src/core/coreext.h"
#include "src/core/dhtserver.h"
#include "src/core/icoresettings.h"
#include "src/core/toxlogger.h"
#include "src/core/toxoptions.h"
#include "src/core/toxstring.h"
#include "src/model/groupinvite.h"
#include "src/model/status.h"
#include "src/model/ibootstraplistgenerator.h"
#include "src/persistence/profile.h"
#include "src/persistence/settings.h"
#include "src/widget/widget.h"
#include "util/strongtype.h"
#include "util/compatiblerecursivemutex.h"
#include "util/toxcoreerrorparser.h"

#include <QCoreApplication>
#include <QDateTime>
// zoff
#include <QFile>
#include <QDir>
#if QT_VERSION >= QT_VERSION_CHECK( 5, 10, 0 )
#include <QRandomGenerator>
#endif
// zoff
#include <QRegularExpression>
#include <QString>
#include <QStringBuilder>
#include <QTimer>

#include <tox/tox.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <random>

const QString Core::TOX_EXT = ".tox";

#define ASSERT_CORE_THREAD assert(QThread::currentThread() == coreThread.get())

namespace {

QList<DhtServer> shuffleBootstrapNodes(QList<DhtServer> bootstrapNodes)
{
    std::mt19937 rng(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::shuffle(bootstrapNodes.begin(), bootstrapNodes.end(), rng);
    return bootstrapNodes;
}

} // namespace

Core::Core(QThread* coreThread_, IBootstrapListGenerator& bootstrapListGenerator_, ICoreSettings& settings_)
    : tox(nullptr)
    , toxTimer{new QTimer{this}}
    , coreThread(coreThread_)
    , bootstrapListGenerator(bootstrapListGenerator_)
    , settings(settings_)
{
    assert(toxTimer);
    // need to migrate Settings and History if this changes
    assert(ToxPk::size == tox_public_key_size());
    assert(GroupId::size == tox_conference_id_size());
    assert(ToxId::size == tox_address_size());
    toxTimer->setSingleShot(true);
    connect(toxTimer, &QTimer::timeout, this, &Core::process);
    connect(coreThread_, &QThread::finished, toxTimer, &QTimer::stop);
}

Core::~Core()
{
    /*
     * First stop the thread to stop the timer and avoid Core emitting callbacks
     * into an already destructed CoreAV.
     */
    coreThread->exit(0);
    coreThread->wait();

    tox.reset();
}

/**
 * @brief Registers all toxcore callbacks
 * @param tox Tox instance to register the callbacks on
 */
void Core::registerCallbacks(Tox* tox)
{
    tox_callback_friend_request(tox, onFriendRequest);
    tox_callback_friend_message(tox, onFriendMessage);
    tox_callback_friend_name(tox, onFriendNameChange);
    tox_callback_friend_typing(tox, onFriendTypingChange);
    tox_callback_friend_status_message(tox, onStatusMessageChanged);
    tox_callback_friend_status(tox, onUserStatusChanged);
    tox_callback_friend_connection_status(tox, onConnectionStatusChanged);
    tox_callback_friend_read_receipt(tox, onReadReceiptCallback);
    tox_callback_conference_invite(tox, onGroupInvite);
    tox_callback_conference_message(tox, onGroupMessage);
    tox_callback_conference_peer_list_changed(tox, onGroupPeerListChange);
    tox_callback_conference_peer_name(tox, onGroupPeerNameChange);
    tox_callback_conference_title(tox, onGroupTitleChange);
    tox_callback_friend_lossless_packet(tox, onLosslessPacket);
    tox_callback_group_invite(tox, onNgcInvite);
    tox_callback_group_self_join(tox, onNgcSelfJoin);
    tox_callback_group_peer_join(tox, onNgcPeerJoin);
    tox_callback_group_peer_exit(tox, onNgcPeerExit);
    tox_callback_group_peer_name(tox, onNgcPeerName);
    tox_callback_group_message(tox, onNgcGroupMessage);
    tox_callback_group_private_message(tox, onNgcGroupPrivateMessage);
}

/**
 * @brief Factory method for the Core object
 * @param savedata empty if new profile or saved data else
 * @param settings Settings specific to Core
 * @return nullptr or a Core object ready to start
 */
ToxCorePtr Core::makeToxCore(const QByteArray& savedata, ICoreSettings& settings,
                             IBootstrapListGenerator& bootstrapNodes, ToxCoreErrors* err)
{
    settings.setToxcore(nullptr);

    QThread* thread = new QThread();
    if (thread == nullptr) {
        qCritical() << "Could not allocate Core thread";
        return {};
    }
    thread->setObjectName("qTox Core");

    auto toxOptions = ToxOptions::makeToxOptions(savedata, settings);
    if (toxOptions == nullptr) {
        qCritical() << "Could not allocate ToxOptions data structure";
        if (err) {
            *err = ToxCoreErrors::ERROR_ALLOC;
        }
        return {};
    }

    ToxCorePtr core(new Core(thread, bootstrapNodes, settings));
    if (core == nullptr) {
        if (err) {
            *err = ToxCoreErrors::ERROR_ALLOC;
        }
        return {};
    }

    Tox_Err_New tox_err;
    core->tox = ToxPtr(tox_new(*toxOptions, &tox_err));

    switch (tox_err) {
    case TOX_ERR_NEW_OK:
        break;

    case TOX_ERR_NEW_LOAD_BAD_FORMAT:
        qCritical() << "Failed to parse Tox save data";
        if (err) {
            *err = ToxCoreErrors::BAD_PROXY;
        }
        return {};

    case TOX_ERR_NEW_PORT_ALLOC:
        if (toxOptions->getIPv6Enabled()) {
            toxOptions->setIPv6Enabled(false);
            core->tox = ToxPtr(tox_new(*toxOptions, &tox_err));
            if (tox_err == TOX_ERR_NEW_OK) {
                qWarning() << "Core failed to start with IPv6, falling back to IPv4. LAN discovery "
                              "may not work properly.";
                break;
            }
        }

        qCritical() << "Can't to bind the port";
        if (err) {
            *err = ToxCoreErrors::FAILED_TO_START;
        }
        return {};

    case TOX_ERR_NEW_PROXY_BAD_HOST:
    case TOX_ERR_NEW_PROXY_BAD_PORT:
    case TOX_ERR_NEW_PROXY_BAD_TYPE:
        qCritical() << "Bad proxy, error code:" << tox_err;
        if (err) {
            *err = ToxCoreErrors::BAD_PROXY;
        }
        return {};

    case TOX_ERR_NEW_PROXY_NOT_FOUND:
        qCritical() << "Proxy not found";
        if (err) {
            *err = ToxCoreErrors::BAD_PROXY;
        }
        return {};

    case TOX_ERR_NEW_LOAD_ENCRYPTED:
        qCritical() << "Attempted to load encrypted Tox save data";
        if (err) {
            *err = ToxCoreErrors::INVALID_SAVE;
        }
        return {};

    case TOX_ERR_NEW_MALLOC:
        qCritical() << "Memory allocation failed";
        if (err) {
            *err = ToxCoreErrors::ERROR_ALLOC;
        }
        return {};

    case TOX_ERR_NEW_NULL:
        qCritical() << "A parameter was null";
        if (err) {
            *err = ToxCoreErrors::FAILED_TO_START;
        }
        return {};

    default:
        qCritical() << "Toxcore failed to start, unknown error code:" << tox_err;
        if (err) {
            *err = ToxCoreErrors::FAILED_TO_START;
        }
        return {};
    }

    // tox should be valid by now
    assert(core->tox != nullptr);

    // create CoreFile
    core->file = CoreFile::makeCoreFile(core.get(), core->tox.get(), core->coreLoopLock);
    if (!core->file) {
        qCritical() << "CoreFile failed to start";
        if (err) {
            *err = ToxCoreErrors::FAILED_TO_START;
        }
        return {};
    }

    core->ext = CoreExt::makeCoreExt(core->tox.get());
    connect(core.get(), &Core::friendStatusChanged, core->ext.get(), &CoreExt::onFriendStatusChanged);

    registerCallbacks(core->tox.get());

    // connect the thread with the Core
    connect(thread, &QThread::started, core.get(), &Core::onStarted);
    core->moveToThread(thread);

    settings.setToxcore(core->tox.get());

    // when leaving this function 'core' should be ready for it's start() action or
    // a nullptr
    return core;
}

void Core::onStarted()
{
    ASSERT_CORE_THREAD;

    // One time initialization stuff
    QString name = getUsername();
    if (!name.isEmpty()) {
        emit usernameSet(name);
    }

    QString msg = getStatusMessage();
    if (!msg.isEmpty()) {
        emit statusMessageSet(msg);
    }

    ToxId id = getSelfId();
    // Id comes from toxcore, must be valid
    assert(id.isValid());
    emit idSet(id);

    loadFriends();
    loadGroups();

    process(); // starts its own timer
}

/**
 * @brief Starts toxcore and it's event loop, can be called from any thread
 */
void Core::start()
{
    coreThread->start();
}

const CoreAV* Core::getAv() const
{
    return av;
}

CoreAV* Core::getAv()
{
    return av;
}

void Core::setAv(CoreAV *coreAv)
{
    av = coreAv;
}

CoreFile* Core::getCoreFile() const
{
    return file.get();
}

Tox* Core::getTox() const
{
    return tox.get();
}

CompatibleRecursiveMutex &Core::getCoreLoopLock() const
{
    return coreLoopLock;
}

const CoreExt* Core::getExt() const
{
    return ext.get();
}

CoreExt* Core::getExt()
{
    return ext.get();
}

/**
 * @brief Processes toxcore events and ensure we stay connected, called by its own timer
 */
void Core::process()
{
    QMutexLocker ml{&coreLoopLock};

    ASSERT_CORE_THREAD;

    tox_iterate(tox.get(), this);
    ext->process();

#ifdef DEBUG
    // we want to see the debug messages immediately
    fflush(stdout);
#endif

    // TODO(sudden6): recheck if this is still necessary
    if (checkConnection()) {
        tolerance = CORE_DISCONNECT_TOLERANCE;
    } else if (!(--tolerance)) {
        bootstrapDht();
        tolerance = 3 * CORE_DISCONNECT_TOLERANCE;
    }

    unsigned sleeptime =
        qMin(tox_iteration_interval(tox.get()), getCoreFile()->corefileIterationInterval());
    toxTimer->start(sleeptime);
}

bool Core::checkConnection()
{
    ASSERT_CORE_THREAD;
    auto selfConnection = tox_self_get_connection_status(tox.get());
    QString connectionName;
    bool toxConnected = false;
    switch (selfConnection)
    {
        case TOX_CONNECTION_NONE:
            toxConnected = false;
            break;
        case TOX_CONNECTION_TCP:
            toxConnected = true;
            connectionName = "a TCP relay";
            break;
        case TOX_CONNECTION_UDP:
            toxConnected = true;
            connectionName = "the UDP DHT";
            break;
        qWarning() << "tox_self_get_connection_status returned unknown enum!";
    }

    if (toxConnected && !isConnected) {
        qDebug().noquote() << "Connected to" << connectionName;
        emit connected();
    } else if (!toxConnected && isConnected) {
        qDebug() << "Disconnected from the DHT";
        emit disconnected();
    }

    isConnected = toxConnected;
    return toxConnected;
}

/**
 * @brief Connects us to the Tox network
 */
void Core::bootstrapDht()
{
    ASSERT_CORE_THREAD;


    auto const shuffledBootstrapNodes = shuffleBootstrapNodes(bootstrapListGenerator.getBootstrapNodes());
    if (shuffledBootstrapNodes.empty()) {
        qWarning() << "No bootstrap node list";
        return;
    }

    // i think the more we bootstrap, the more we jitter because the more we overwrite nodes
    auto numNewNodes = 2;
    for (int i = 0; i < numNewNodes && i < shuffledBootstrapNodes.size(); ++i) {
        const auto& dhtServer = shuffledBootstrapNodes.at(i);
        QByteArray address;
        if (!dhtServer.ipv4.isEmpty()) {
            address = dhtServer.ipv4.toLatin1();
        } else if (!dhtServer.ipv6.isEmpty() && settings.getEnableIPv6()) {
            address = dhtServer.ipv6.toLatin1();
        } else {
            ++numNewNodes;
            continue;
        }

        ToxPk pk{dhtServer.publicKey};
        qDebug() << "Connecting to bootstrap node" << pk.toString();
        const uint8_t* pkPtr = pk.getData();

        Tox_Err_Bootstrap error;
        if (dhtServer.statusUdp) {
            tox_bootstrap(tox.get(), address.constData(), dhtServer.udpPort, pkPtr, &error);
            PARSE_ERR(error);
        }
        if (dhtServer.statusTcp) {
            const auto ports = dhtServer.tcpPorts.size();
            const auto tcpPort = dhtServer.tcpPorts[rand() % ports];
            tox_add_tcp_relay(tox.get(), address.constData(), tcpPort, pkPtr, &error);
            PARSE_ERR(error);
        }
    }
}

void Core::onFriendRequest(Tox* tox, const uint8_t* cFriendPk, const uint8_t* cMessage,
                           size_t cMessageSize, void* core)
{
    std::ignore = tox;
    ToxPk friendPk(cFriendPk);
    QString requestMessage = ToxString(cMessage, cMessageSize).getQString();
    emit static_cast<Core*>(core)->friendRequestReceived(friendPk, requestMessage);
}

static size_t xnet_unpack_u16(const uint8_t *bytes, uint16_t *v)
{
    uint8_t hi = bytes[0];
    uint8_t lo = bytes[1];
    *v = (static_cast<uint16_t>(hi) << 8) | lo;
    return sizeof(*v);
}

static size_t xnet_unpack_u32(const uint8_t *bytes, uint32_t *v)
{
    const uint8_t *p = bytes;
    uint16_t hi;
    uint16_t lo;
    p += xnet_unpack_u16(p, &hi);
    p += xnet_unpack_u16(p, &lo);
    *v = (static_cast<uint32_t>(hi) << 16) | lo;
    return p - bytes;
}

void Core::onFriendMessage(Tox* tox, uint32_t friendId, Tox_Message_Type type, const uint8_t* cMessage,
                           size_t cMessageSize, void* core)
{
    std::ignore = tox;
    uint32_t msgV3_timestamp = 0;
    bool isAction = (type == TOX_MESSAGE_TYPE_ACTION);
    QString msg = ToxString(cMessage, cMessageSize).getQString();

    // HINT: check for msgV3 --------------
    bool has_msgv3 = false;
    if ((cMessage) && (cMessageSize > (TOX_MSGV3_MSGID_LENGTH + TOX_MSGV3_TIMESTAMP_LENGTH + TOX_MSGV3_GUARD))) {
        int pos = cMessageSize - (TOX_MSGV3_MSGID_LENGTH + TOX_MSGV3_TIMESTAMP_LENGTH + TOX_MSGV3_GUARD);
        uint8_t g1 = *(cMessage + pos);
        uint8_t g2 = *(cMessage + pos + 1);
        // check for the msgv3 guard
        if ((g1 == 0) && (g2 == 0)) {
            // we have msgV3 meta data
            const char *msgV3_hash_buffer_bin = reinterpret_cast<const char*>(cMessage + pos + 2);
            const uint8_t *p = static_cast<const uint8_t *>(cMessage + pos + 2);
            p = p + 32;
            p += xnet_unpack_u32(p, &msgV3_timestamp);
            QByteArray msgv3hash = QByteArray(msgV3_hash_buffer_bin, 32);
            // qDebug() << "msgv3hash:" << QString::fromUtf8(msgv3hash.toHex()).toUpper();
            msg = QString::fromUtf8(msgv3hash.toHex()).toUpper().rightJustified(64, '0') + QString(":") + msg;
            has_msgv3 = true;
        }
    }

    // HINT: check for msgV3 --------------
    if (has_msgv3) {
        emit static_cast<Core*>(core)->friendMessageReceived(friendId, msg, isAction,
            static_cast<int>(Widget::MessageHasIdType::MSGV3_ID));
    } else {
        emit static_cast<Core*>(core)->friendMessageReceived(friendId, msg, isAction);
    }
}

void Core::onFriendNameChange(Tox* tox, uint32_t friendId, const uint8_t* cName, size_t cNameSize, void* core)
{
    std::ignore = tox;
    QString newName = ToxString(cName, cNameSize).getQString();
    // no saveRequest, this callback is called on every connection, not just on name change
    emit static_cast<Core*>(core)->friendUsernameChanged(friendId, newName);
}

void Core::onFriendTypingChange(Tox* tox, uint32_t friendId, bool isTyping, void* core)
{
    std::ignore = tox;
    emit static_cast<Core*>(core)->friendTypingChanged(friendId, isTyping);
}

void Core::onStatusMessageChanged(Tox* tox, uint32_t friendId, const uint8_t* cMessage,
                                  size_t cMessageSize, void* core)
{
    std::ignore = tox;
    QString message = ToxString(cMessage, cMessageSize).getQString();
    // no saveRequest, this callback is called on every connection, not just on name change
    emit static_cast<Core*>(core)->friendStatusMessageChanged(friendId, message);
}

void Core::onUserStatusChanged(Tox* tox, uint32_t friendId, Tox_User_Status userstatus, void* core)
{
    std::ignore = tox;
    Status::Status status;
    switch (userstatus) {
    case TOX_USER_STATUS_AWAY:
        status = Status::Status::Away;
        break;

    case TOX_USER_STATUS_BUSY:
        status = Status::Status::Busy;
        break;

    default:
        status = Status::Status::Online;
        break;
    }

    // no saveRequest, this callback is called on every connection, not just on name change
    emit static_cast<Core*>(core)->friendStatusChanged(friendId, status);
}

void Core::onConnectionStatusChanged(Tox* tox, uint32_t friendId, Tox_Connection status, void* vCore)
{
    std::ignore = tox;
    Core* core = static_cast<Core*>(vCore);
    Status::Status friendStatus = Status::Status::Offline;
    switch (status)
    {
        case TOX_CONNECTION_NONE:
            friendStatus = Status::Status::Offline;
            qDebug() << "Disconnected from friend" << friendId;
            break;
        case TOX_CONNECTION_TCP:
            friendStatus = Status::Status::Online;
            qDebug() << "Connected to friend" << friendId << "through a TCP relay";
            break;
        case TOX_CONNECTION_UDP:
            friendStatus = Status::Status::Online;
            qDebug() << "Connected to friend" << friendId << "directly with UDP";
            break;
        qWarning() << "tox_callback_friend_connection_status returned unknown enum!";
    }

    // Ignore Online because it will be emited from onUserStatusChanged
    bool isOffline = friendStatus == Status::Status::Offline;
    if (isOffline) {
        emit core->friendStatusChanged(friendId, friendStatus);
        core->checkLastOnline(friendId);
    }
}

void Core::onGroupInvite(Tox* tox, uint32_t friendId, Tox_Conference_Type type,
                         const uint8_t* cookie, size_t length, void* vCore)
{
    std::ignore = tox;
    Core* core = static_cast<Core*>(vCore);
    const QByteArray data(reinterpret_cast<const char*>(cookie), length);
    const GroupInvite inviteInfo(friendId, type, data);
    switch (type) {
    case TOX_CONFERENCE_TYPE_TEXT:
        qDebug() << QString("Text group invite by %1").arg(friendId);
        emit core->groupInviteReceived(inviteInfo);
        break;

    case TOX_CONFERENCE_TYPE_AV:
        qDebug() << QString("AV group invite by %1").arg(friendId);
        emit core->groupInviteReceived(inviteInfo);
        break;

    default:
        qWarning() << "Group invite with unknown type " << type;
    }
}

QString Core::GetRandomString(int randomStringLength) const
{
   const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

   QString randomString;
   for(int i=0; i<randomStringLength; ++i)
   {
#if QT_VERSION < QT_VERSION_CHECK( 5, 10, 0 )
       int index = qrand() % possibleCharacters.length();
#else
       int index = QRandomGenerator::global()->generate() % possibleCharacters.length();
#endif
       QChar nextChar = possibleCharacters.at(index);
       randomString.append(nextChar);
   }
   return randomString;
}

void Core::onNgcInvite(Tox* tox, uint32_t friendId, const uint8_t* invite_data, size_t length,
                       const uint8_t *group_name, size_t group_name_length, void* vCore)
{
    std::ignore = group_name;
    std::ignore = group_name_length;
    Core* core = static_cast<Core*>(vCore);
    qDebug() << QString("NGC group invite by %1").arg(friendId);

    auto rnd_letters = core->GetRandomString(5);
    auto user_name = QString("user ") + rnd_letters;
    auto user_name_len = user_name.toUtf8().size();

    Tox_Err_Group_Invite_Accept error;
    uint32_t groupId = tox_group_invite_accept(
                        tox,
                        friendId,
                        invite_data,
                        length,
                        reinterpret_cast<const uint8_t*>(user_name.toUtf8().constData()),
                        user_name_len,
                        NULL, 0,  &error);
    if (groupId == UINT32_MAX) {
        qDebug() << QString("NGC group invite by %1: FAILED").arg(friendId);
    } else {
        qDebug() << QString("NGC group invite by %1: OK").arg(friendId);
        emit core->saveRequest();
        emit core->groupJoined((Settings::NGC_GROUPNUM_OFFSET + groupId), core->getGroupPersistentId(groupId, 1));
    }
}

void Core::onNgcSelfJoin(Tox* tox, uint32_t group_number, void* vCore)
{
    std::ignore = tox;
    Core* core = static_cast<Core*>(vCore);
    qDebug() << QString("onNgcSelfJoin:gn #%1").arg(group_number);
    emit core->saveRequest();
}

void Core::onNgcPeerName(Tox *tox, uint32_t group_number, uint32_t peer_id, const uint8_t *name,
                                    size_t length, void *vCore)
{
    std::ignore = tox;
    std::ignore = name;
    std::ignore = length;
    Core* core = static_cast<Core*>(vCore);
    qDebug() << QString("onNgcPeerName:peer_id") << peer_id;
    emit core->groupPeerlistChanged(Settings::NGC_GROUPNUM_OFFSET + group_number);
    emit core->saveRequest();
}

void Core::onNgcPeerExit(Tox *tox, uint32_t group_number, uint32_t peer_id, Tox_Group_Exit_Type exit_type,
                                    const uint8_t *name, size_t name_length, const uint8_t *part_message, size_t length, void *vCore)
{
    std::ignore = tox;
    std::ignore = name;
    std::ignore = name_length;
    std::ignore = part_message;
    std::ignore = length;
    Core* core = static_cast<Core*>(vCore);
    qDebug() << QString("onNgcPeerExit:peer_id") << peer_id << "exit type" << exit_type;
    emit core->groupPeerlistChanged(Settings::NGC_GROUPNUM_OFFSET + group_number);
    emit core->saveRequest();
}

void Core::onNgcPeerJoin(Tox* tox, uint32_t group_number, uint32_t peer_id, void* vCore)
{
    std::ignore = tox;
    std::ignore = group_number;
    Core* core = static_cast<Core*>(vCore);

    auto peerPk = core->getGroupPeerPk((Settings::NGC_GROUPNUM_OFFSET + group_number), peer_id);

    Tox_Err_Group_Peer_Query error;
    size_t name_length = tox_group_peer_get_name_size(tox, group_number, peer_id, &error);
    if ((name_length > 0) && (name_length < 500)) {
        uint8_t *name = reinterpret_cast<uint8_t*>(calloc(1, name_length + 1));
        if (name) {
            bool res = tox_group_peer_get_name(tox, group_number, peer_id, name, &error);
            if (res) {
                const auto newName = ToxString(name, name_length).getQString();
                emit core->groupPeerlistChanged(Settings::NGC_GROUPNUM_OFFSET + group_number);
            }
            free(name);
        }
    }

    qDebug() << QString("onNgcPeerJoin:peer #%1").arg(peer_id);
    emit core->saveRequest();
}

void Core::onNgcGroupMessage(Tox* tox, uint32_t group_number, uint32_t peer_id, Tox_Message_Type type,
                             const uint8_t *message, size_t length, uint32_t message_id, void* vCore)
{
    std::ignore = tox;
    std::ignore = type;
    Core* core = static_cast<Core*>(vCore);
    QString msg = ToxString(message, length).getQString();
    uint32_t message_id_hostenc;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&message_id);
    xnet_unpack_u32(p, &message_id_hostenc);
    QByteArray msgIdhash = QByteArray(reinterpret_cast<const char*>(&message_id_hostenc), 4);
    // qDebug() << "msgIdhash:" << QString::fromUtf8(msgIdhash.toHex()).toUpper();
    msg = QString::fromUtf8(msgIdhash.toHex()).toUpper().rightJustified(8, '0') + QString(":") + msg;

    auto peerPk = core->getGroupPeerPk((Settings::NGC_GROUPNUM_OFFSET + group_number), peer_id);
    emit core->groupMessageReceived((Settings::NGC_GROUPNUM_OFFSET + group_number), peer_id, msg,
        false, static_cast<int>(Widget::MessageHasIdType::NGC_MSG_ID));
}

void Core::onNgcGroupPrivateMessage(Tox* tox, uint32_t group_number, uint32_t peer_id, Tox_Message_Type type,
        const uint8_t *message, size_t length, void* vCore)
{
    std::ignore = tox;
    Core* core = static_cast<Core*>(vCore);
    std::ignore = type;
    QString msg = ToxString(message, length).getQString();
    qDebug() << QString("onNgcGroupPrivateMessage:peer=") << peer_id;
    emit core->groupMessageReceived((Settings::NGC_GROUPNUM_OFFSET + group_number), peer_id, QString("Private Message:") + msg, false);
}

void Core::onGroupMessage(Tox* tox, uint32_t groupId, uint32_t peerId, Tox_Message_Type type,
                          const uint8_t* cMessage, size_t length, void* vCore)
{
    std::ignore = tox;
    Core* core = static_cast<Core*>(vCore);
    bool isAction = type == TOX_MESSAGE_TYPE_ACTION;
    if (length > 9) {
        QString conf_msgid = ToxString(cMessage, 8).getQString();
        QString message_text = ToxString((cMessage + 9), (length - 9)).getQString();
        QString wrapped_message = conf_msgid.toUpper().rightJustified(8, '0') + QString(":") + message_text;
        // qDebug() << QString("onGroupMessage:wrapped_message:") << wrapped_message;
        emit core->groupMessageReceived(groupId, peerId, wrapped_message, isAction,
            static_cast<int>(Widget::MessageHasIdType::CONF_MSG_ID));
    } else {
        qWarning() << QString("onGroupMessage:group message length from tox less than 10. length:") << length;
    }
}

void Core::onGroupPeerListChange(Tox* tox, uint32_t groupId, void* vCore)
{
    std::ignore = tox;
    const auto core = static_cast<Core*>(vCore);
    qDebug() << QString("Group %1 peerlist changed").arg(groupId);
    // no saveRequest, this callback is called on every connection to group peer, not just on brand new peers
    emit core->groupPeerlistChanged(groupId);
}

void Core::onGroupPeerNameChange(Tox* tox, uint32_t groupId, uint32_t peerId, const uint8_t* name,
                                 size_t length, void* vCore)
{
    std::ignore = tox;
    const auto newName = ToxString(name, length).getQString();
    qDebug() << QString("Group %1, peer %2, name changed to %3").arg(groupId).arg(peerId).arg(newName);
    auto* core = static_cast<Core*>(vCore);
    auto peerPk = core->getGroupPeerPk(groupId, peerId);
    emit core->groupPeerNameChanged(groupId, peerPk, newName);
}

void Core::onGroupTitleChange(Tox* tox, uint32_t groupId, uint32_t peerId, const uint8_t* cTitle,
                              size_t length, void* vCore)
{
    std::ignore = tox;
    Core* core = static_cast<Core*>(vCore);
    QString author;
    // from tox.h: "If peer_number == UINT32_MAX, then author is unknown (e.g. initial joining the conference)."
    if (peerId != std::numeric_limits<uint32_t>::max()) {
        author = core->getGroupPeerName(groupId, peerId);
    }
    emit core->saveRequest();
    emit core->groupTitleChanged(groupId, author, ToxString(cTitle, length).getQString());
}

/**
 * @brief Handling of custom lossless packets received by toxcore. Currently only used to forward toxext packets to CoreExt
 */
void Core::onLosslessPacket(Tox* tox, uint32_t friendId,
                            const uint8_t* data, size_t length, void* vCore)
{
    std::ignore = tox;
    Core* core = static_cast<Core*>(vCore);
    core->ext->onLosslessPacket(friendId, data, length);

    // zoff
    // qDebug() << "onLosslessPacket:fn=" << friendId << " data:" << (int)data[0] << (int)data[1] << (int)data[2];
    if (data[0] == 181) // HINT: pkt id for CONTROL_PROXY_MESSAGE_TYPE_PUSH_URL_FOR_FRIEND == 181
    {
        if (length > 5)
        {
            const uint8_t* data_str = data + 1;
            QString pushtoken = ToxString(data_str, (length - 1)).getQString();
            emit static_cast<Core*>(core)->friendPushtokenReceived(friendId, pushtoken);
        }
        else
        {
            qDebug() << "onLosslessPacket:DEL:Pushtoken";
            emit static_cast<Core*>(core)->friendPushtokenReceived(friendId, " ");
        }
    }
    // zoff
}

void Core::onReadReceiptCallback(Tox* tox, uint32_t friendId, uint32_t receipt, void* core)
{
    std::ignore = tox;
    emit static_cast<Core*>(core)->receiptRecieved(friendId, ReceiptNum{receipt});
}

void Core::acceptFriendRequest(const ToxPk& friendPk)
{
    QMutexLocker ml{&coreLoopLock};
    Tox_Err_Friend_Add error;
    uint32_t friendId = tox_friend_add_norequest(tox.get(), friendPk.getData(), &error);
    if (PARSE_ERR(error)) {
        emit saveRequest();
        emit friendAdded(friendId, friendPk);
    } else {
        emit failedToAddFriend(friendPk);
    }
}

/**
 * @brief Checks that sending friendship request is correct and returns error message accordingly
 * @param friendId Id of a friend which request is destined to
 * @param message Friendship request message
 * @return Returns empty string if sending request is correct, according error message otherwise
 */
QString Core::getFriendRequestErrorMessage(const ToxId& friendId, const QString& message) const
{
    QMutexLocker ml{&coreLoopLock};

    if (!friendId.isValid()) {
        return tr("Invalid Tox ID", "Error while sending friend request");
    }

    if (message.isEmpty()) {
        return tr("You need to write a message with your request",
                  "Error while sending friend request");
    }

    if (message.length() > static_cast<int>(tox_max_friend_request_length())) {
        return tr("Your message is too long!", "Error while sending friend request");
    }

    if (hasFriendWithPublicKey(friendId.getPublicKey())) {
        return tr("Friend is already added", "Error while sending friend request");
    }

    return QString{};
}

void Core::requestNgc(const QString& ngcId, const QString& message)
{
    QMutexLocker ml{&coreLoopLock};

    std::ignore = message;

    QByteArray ngcIdBytes = QByteArray::fromHex(ngcId.toLatin1());

    auto rnd_letters = GetRandomString(5);
    auto user_name = QString("user ") + rnd_letters;
    auto user_name_len = user_name.toUtf8().size();
    qDebug() << QString("requestNgc join:my peer name=") << user_name << QString("user_name_len=") << user_name_len;

    Tox_Err_Group_Join error;
    // TODO: add password if needed
    uint32_t groupId = tox_group_join(tox.get(),
        reinterpret_cast<const uint8_t*>(ngcIdBytes.constData()),
        reinterpret_cast<const uint8_t*>(user_name.toUtf8().constData()),
        user_name_len,
        NULL,
        0,
        &error);

    if (groupId == UINT32_MAX) {
        qDebug() << "requestNgc join failed, error: " << error;
    } else {
        qDebug() << "requestNgc join OK, group num: " << groupId;
        emit saveRequest();
        emit groupJoined((Settings::NGC_GROUPNUM_OFFSET + groupId), getGroupPersistentId(groupId, 1));
    }
}

void Core::requestFriendship(const ToxId& friendId, const QString& message)
{
    QMutexLocker ml{&coreLoopLock};

    ToxPk friendPk = friendId.getPublicKey();
    QString errorMessage = getFriendRequestErrorMessage(friendId, message);
    if (!errorMessage.isNull()) {
        emit failedToAddFriend(friendPk, errorMessage);
        emit saveRequest();
        return;
    }

    ToxString cMessage(message);
    Tox_Err_Friend_Add error;
    uint32_t friendNumber =
        tox_friend_add(tox.get(), friendId.getBytes(), cMessage.data(), cMessage.size(), &error);
    if (PARSE_ERR(error)) {
        qDebug() << "Requested friendship from " << friendNumber;
        emit saveRequest();
        emit friendAdded(friendNumber, friendPk);
        emit requestSent(friendPk, message);
    } else {
        qDebug() << "Failed to send friend request";
        emit failedToAddFriend(friendPk);
    }
}

bool Core::sendMessageWithType(uint32_t friendId, const QString& message, Tox_Message_Type type,
                               ReceiptNum& receipt)
{
    int size = message.toUtf8().size();
    auto maxSize = static_cast<int>(getMaxMessageSize());
    if (size > maxSize) {
        assert(false);
        qCritical() << "Core::sendMessageWithType called with message of size:" << size
                    << "when max is:" << maxSize << ". Ignoring.";
        return false;
    }

    ToxString cMessage(message);
    Tox_Err_Friend_Send_Message error;
    receipt = ReceiptNum{tox_friend_send_message(tox.get(), friendId, type, cMessage.data(),
                                                 cMessage.size(), &error)};
    if (PARSE_ERR(error)) {
        return true;
    }
    return false;
}

bool Core::sendMessage(uint32_t friendId, const QString& message, ReceiptNum& receipt)
{
    QMutexLocker ml(&coreLoopLock);
    return sendMessageWithType(friendId, message, TOX_MESSAGE_TYPE_NORMAL, receipt);
}

bool Core::sendAction(uint32_t friendId, const QString& action, ReceiptNum& receipt)
{
    QMutexLocker ml(&coreLoopLock);
    return sendMessageWithType(friendId, action, TOX_MESSAGE_TYPE_ACTION, receipt);
}

void Core::sendTyping(uint32_t friendId, bool typing)
{
    QMutexLocker ml{&coreLoopLock};

    Tox_Err_Set_Typing error;
    tox_self_set_typing(tox.get(), friendId, typing, &error);
    if (!PARSE_ERR(error)) {
        emit failedToSetTyping(typing);
    }
}

void Core::sendGroupMessageWithType(int groupId, const QString& message, Tox_Message_Type type)
{
    QMutexLocker ml{&coreLoopLock};

    if (groupId >= static_cast<int>(Settings::NGC_GROUPNUM_OFFSET)) {
        int size = message.toUtf8().size();
        auto maxSize = static_cast<int>(TOX_GROUP_MAX_MESSAGE_LENGTH);
        if (size > maxSize) {
            qCritical() << "Core::sendMessageWithType NGC called with message of size:" << size
                        << "when max is:" << maxSize << ". Ignoring.";
            return;
        }

        ToxString cMsg(message);
        Tox_Err_Group_Send_Message error;
        uint32_t message_id;
        tox_group_send_message(tox.get(), (groupId - Settings::NGC_GROUPNUM_OFFSET), type, cMsg.data(), cMsg.size(), &message_id, &error);
        if (!PARSE_ERR(error)) {
            emit groupSentFailed(groupId);
            return;
        }
    } else {
        int size = message.toUtf8().size();
        auto maxSize = static_cast<int>(getMaxMessageSize());
        if (size > maxSize) {
            qCritical() << "Core::sendMessageWithType called with message of size:" << size
                        << "when max is:" << maxSize << ". Ignoring.";
            return;
        }

        ToxString cMsg(message);
        Tox_Err_Conference_Send_Message error;
        tox_conference_send_message(tox.get(), groupId, type, cMsg.data(), cMsg.size(), &error);
        if (!PARSE_ERR(error)) {
            emit groupSentFailed(groupId);
            return;
        }
    }
}

void Core::sendGroupMessage(int groupId, const QString& message)
{
    QMutexLocker ml{&coreLoopLock};

    sendGroupMessageWithType(groupId, message, TOX_MESSAGE_TYPE_NORMAL);
}

void Core::sendGroupAction(int groupId, const QString& message)
{
    QMutexLocker ml{&coreLoopLock};

    sendGroupMessageWithType(groupId, message, TOX_MESSAGE_TYPE_ACTION);
}

void Core::changeGroupTitle(int groupId, const QString& title)
{
    QMutexLocker ml{&coreLoopLock};

    ToxString cTitle(title);
    Tox_Err_Conference_Title error;
    tox_conference_set_title(tox.get(), groupId, cTitle.data(), cTitle.size(), &error);
    if (PARSE_ERR(error)) {
        emit saveRequest();
        emit groupTitleChanged(groupId, getUsername(), title);
    }
}

void Core::removeFriend(uint32_t friendId)
{
    QMutexLocker ml{&coreLoopLock};

    Tox_Err_Friend_Delete error;
    tox_friend_delete(tox.get(), friendId, &error);
    if (!PARSE_ERR(error)) {
        emit failedToRemoveFriend(friendId);
        return;
    }

    emit saveRequest();
    emit friendRemoved(friendId);
}

void Core::removeGroup(int groupId)
{
    QMutexLocker ml{&coreLoopLock};

    if (groupId >= static_cast<int>(Settings::NGC_GROUPNUM_OFFSET)) {
        Tox_Err_Group_Leave error;
        tox_group_leave(tox.get(), (groupId - Settings::NGC_GROUPNUM_OFFSET), reinterpret_cast<const uint8_t*>("exit"), 4, &error);
        if (PARSE_ERR(error)) {
            emit saveRequest();
        }
    } else {
        Tox_Err_Conference_Delete error;
        tox_conference_delete(tox.get(), groupId, &error);
        if (PARSE_ERR(error)) {
            emit saveRequest();

            /*
             * TODO(sudden6): this is probably not (thread-)safe, but can be ignored for now since
             * we don't change av at runtime.
             */

            if (av) {
                av->leaveGroupCall(groupId);
            }
        }
    }
}

/**
 * @brief Returns our username, or an empty string on failure
 */
QString Core::getUsername() const
{
    QMutexLocker ml{&coreLoopLock};

    QString sname;
    if (!tox) {
        return sname;
    }

    int size = tox_self_get_name_size(tox.get());
    if (!size) {
        return {};
    }
    std::vector<uint8_t> nameBuf(size);
    tox_self_get_name(tox.get(), nameBuf.data());
    return ToxString(nameBuf.data(), size).getQString();
}

void Core::setUsername(const QString& username)
{
    QMutexLocker ml{&coreLoopLock};

    if (username == getUsername()) {
        return;
    }

    ToxString cUsername(username);
    Tox_Err_Set_Info error;
    tox_self_set_name(tox.get(), cUsername.data(), cUsername.size(), &error);
    if (!PARSE_ERR(error)) {
        emit failedToSetUsername(username);
        return;
    }

    emit usernameSet(username);
    emit saveRequest();
}

/**
 * @brief Returns our Tox ID
 */
ToxId Core::getSelfId() const
{
    QMutexLocker ml{&coreLoopLock};

    uint8_t friendId[TOX_ADDRESS_SIZE] = {0x00};
    tox_self_get_address(tox.get(), friendId);
    return ToxId(friendId, TOX_ADDRESS_SIZE);
}

/**
 * @brief Gets self public key
 * @return Self PK
 */
ToxPk Core::getSelfPublicKey() const
{
    QMutexLocker ml{&coreLoopLock};

    uint8_t selfPk[TOX_PUBLIC_KEY_SIZE] = {0x00};
    tox_self_get_public_key(tox.get(), selfPk);
    return ToxPk(selfPk);
}

QByteArray Core::getSelfDhtId() const
{
    QMutexLocker ml{&coreLoopLock};
    QByteArray dhtKey(TOX_PUBLIC_KEY_SIZE, 0x00);
    tox_self_get_dht_id(tox.get(), reinterpret_cast<uint8_t*>(dhtKey.data()));
    return dhtKey;
}

int Core::getSelfUdpPort() const
{
    QMutexLocker ml{&coreLoopLock};
    Tox_Err_Get_Port error;
    auto port = tox_self_get_udp_port(tox.get(), &error);
    if (!PARSE_ERR(error)) {
        return -1;
    }
    return port;
}

/**
 * @brief Returns our status message, or an empty string on failure
 */
QString Core::getStatusMessage() const
{
    QMutexLocker ml{&coreLoopLock};

    assert(tox != nullptr);

    size_t size = tox_self_get_status_message_size(tox.get());
    if (!size) {
        return {};
    }
    std::vector<uint8_t> nameBuf(size);
    tox_self_get_status_message(tox.get(), nameBuf.data());
    return ToxString(nameBuf.data(), size).getQString();
}

/**
 * @brief Returns our user status
 */
Status::Status Core::getStatus() const
{
    QMutexLocker ml{&coreLoopLock};

    return static_cast<Status::Status>(tox_self_get_status(tox.get()));
}

void Core::setStatusMessage(const QString& message)
{
    QMutexLocker ml{&coreLoopLock};

    if (message == getStatusMessage()) {
        return;
    }

    ToxString cMessage(message);
    Tox_Err_Set_Info error;
    tox_self_set_status_message(tox.get(), cMessage.data(), cMessage.size(), &error);
    if (!PARSE_ERR(error)) {
        emit failedToSetStatusMessage(message);
        return;
    }

    emit saveRequest();
    emit statusMessageSet(message);
}

void Core::setStatus(Status::Status status)
{
    QMutexLocker ml{&coreLoopLock};

    Tox_User_Status userstatus;
    switch (status) {
    case Status::Status::Online:
        userstatus = TOX_USER_STATUS_NONE;
        break;

    case Status::Status::Away:
        userstatus = TOX_USER_STATUS_AWAY;
        break;

    case Status::Status::Busy:
        userstatus = TOX_USER_STATUS_BUSY;
        break;

    default:
        return;
        break;
    }

    tox_self_set_status(tox.get(), userstatus);
    emit saveRequest();
    emit statusSet(status);
}

/**
 * @brief Returns the unencrypted tox save data
 */
QByteArray Core::getToxSaveData()
{
    QMutexLocker ml{&coreLoopLock};

    uint32_t fileSize = tox_get_savedata_size(tox.get());
    QByteArray data;
    data.resize(fileSize);
    tox_get_savedata(tox.get(), reinterpret_cast<uint8_t*>(data.data()));
    return data;
}

void Core::loadFriends()
{
    QMutexLocker ml{&coreLoopLock};

    const size_t friendCount = tox_self_get_friend_list_size(tox.get());
    if (friendCount == 0) {
        return;
    }

    std::vector<uint32_t> ids(friendCount);
    tox_self_get_friend_list(tox.get(), ids.data());
    uint8_t friendPk[TOX_PUBLIC_KEY_SIZE] = {0x00};
    for (size_t i = 0; i < friendCount; ++i) {
        Tox_Err_Friend_Get_Public_Key keyError;
        tox_friend_get_public_key(tox.get(), ids[i], friendPk, &keyError);
        if (!PARSE_ERR(keyError)) {
            continue;
        }
        emit friendAdded(ids[i], ToxPk(friendPk));
        emit friendUsernameChanged(ids[i], getFriendUsername(ids[i]));
        Tox_Err_Friend_Query queryError;
        size_t statusMessageSize = tox_friend_get_status_message_size(tox.get(), ids[i], &queryError);
        if (PARSE_ERR(queryError) && statusMessageSize) {
            std::vector<uint8_t> messageData(statusMessageSize);
            tox_friend_get_status_message(tox.get(), ids[i], messageData.data(), &queryError);
            QString friendStatusMessage = ToxString(messageData.data(), statusMessageSize).getQString();
            emit friendStatusMessageChanged(ids[i], friendStatusMessage);
        }
        checkLastOnline(ids[i]);

        // HINT: load pushtoken for friend from db, and put it in the Friend object
        emit friendLoaded(ids[i]);
        // HINT: update connection status so that the icon get drawn again, and takes pushtoken into account
        Tox_Err_Friend_Query error2;
        Tox_Connection connection_status = tox_friend_get_connection_status(tox.get(), ids[i], &error2);
        Status::Status friendStatus = Status::Status::Offline;
        switch (connection_status)
        {
            case TOX_CONNECTION_NONE:
                friendStatus = Status::Status::Offline;
                break;
            case TOX_CONNECTION_TCP:
                friendStatus = Status::Status::Online;
                break;
            case TOX_CONNECTION_UDP:
                friendStatus = Status::Status::Online;
                break;
            default:
                friendStatus = Status::Status::Offline;
                break;
        }
        // HINT: yes 3 times. we need to force a status change so the UI will update
        emit friendStatusChanged(ids[i], Status::Status::Online);
        emit friendStatusChanged(ids[i], Status::Status::Offline);
        emit friendStatusChanged(ids[i], friendStatus);
    }
}

void Core::loadGroups()
{
    QMutexLocker ml{&coreLoopLock};

    const size_t groupCount = tox_conference_get_chatlist_size(tox.get());
    if (groupCount > 0) {
        std::vector<uint32_t> groupNumbers(groupCount);
        tox_conference_get_chatlist(tox.get(), groupNumbers.data());

        for (size_t i = 0; i < groupCount; ++i) {
            Tox_Err_Conference_Title error;
            QString name;
            const auto groupNumber = groupNumbers[i];
            size_t titleSize = tox_conference_get_title_size(tox.get(), groupNumber, &error);
            const GroupId persistentId = getGroupPersistentId(groupNumber, 0);
            const QString defaultName = tr("Groupchat %1").arg(persistentId.toString().left(8));
            if (PARSE_ERR(error) || !titleSize) {
                std::vector<uint8_t> nameBuf(titleSize);
                tox_conference_get_title(tox.get(), groupNumber, nameBuf.data(), &error);
                if (PARSE_ERR(error)) {
                    name = ToxString(nameBuf.data(), titleSize).getQString();
                } else {
                    name = defaultName;
                }
            } else {
                name = defaultName;
            }
            if (getGroupAvEnabled(groupNumber)) {
                if (toxav_groupchat_enable_av(tox.get(), groupNumber, CoreAV::groupCallCallback, this)) {
                    qCritical() << "Failed to enable audio on loaded group" << groupNumber;
                }
            }
            emit emptyGroupCreated(groupNumber, persistentId, name);
        }
    }

    const uint32_t ngcCount = tox_group_get_number_groups(tox.get());
    if (ngcCount > 0) {
        std::vector<uint32_t> groupNumbers(ngcCount);
        tox_group_get_grouplist(tox.get(), groupNumbers.data());

        for (size_t i = 0; i < ngcCount; ++i) {
            Tox_Err_Group_State_Queries error;
            QString name;
            const auto groupNumber = groupNumbers[i];
            size_t titleSize = tox_group_get_name_size(tox.get(), groupNumber, &error);
            const GroupId persistentId = getGroupPersistentId(groupNumber, 1);
            const QString defaultName = persistentId.toString().left(8);
            if (PARSE_ERR(error) || !titleSize) {
                std::vector<uint8_t> nameBuf(titleSize);
                tox_group_get_name(tox.get(), groupNumber, nameBuf.data(), &error);
                if (PARSE_ERR(error)) {
                    name = ToxString(nameBuf.data(), titleSize).getQString();
                } else {
                    name = defaultName;
                }
            } else {
                name = defaultName;
            }
            emit emptyGroupCreated((Settings::NGC_GROUPNUM_OFFSET + groupNumber), persistentId, name);
        }
    }
}

void Core::checkLastOnline(uint32_t friendId)
{
    QMutexLocker ml{&coreLoopLock};

    Tox_Err_Friend_Get_Last_Online error;
    const uint64_t lastOnline = tox_friend_get_last_online(tox.get(), friendId, &error);
    if (PARSE_ERR(error)) {
        emit friendLastSeenChanged(friendId, QDateTime::fromTime_t(lastOnline));
    }
}

/**
 * @brief Returns the list of friendIds in our friendlist, an empty list on error
 */
QVector<uint32_t> Core::getFriendList() const
{
    QMutexLocker ml{&coreLoopLock};

    QVector<uint32_t> friends;
    friends.resize(tox_self_get_friend_list_size(tox.get()));
    tox_self_get_friend_list(tox.get(), friends.data());
    return friends;
}

GroupId Core::getGroupPersistentId(uint32_t groupNumber, int is_ngc) const
{
    QMutexLocker ml{&coreLoopLock};

    if (is_ngc == 1) {
        std::vector<uint8_t> idBuff(TOX_GROUP_CHAT_ID_SIZE);
        Tox_Err_Group_State_Queries error;
        if (tox_group_get_chat_id(tox.get(), groupNumber,
                                  idBuff.data(), &error)) {
            return GroupId{idBuff.data()};
        } else {
            qCritical() << "Failed to get conference ID of group" << groupNumber;
            return {};
        }
    } else {
        std::vector<uint8_t> idBuff(TOX_CONFERENCE_UID_SIZE);
        if (tox_conference_get_id(tox.get(), groupNumber,
                                  idBuff.data())) {
            return GroupId{idBuff.data()};
        } else {
            qCritical() << "Failed to get conference ID of group" << groupNumber;
            return {};
        }
    }
}

/**
 * @brief Get number of peers in the conference.
 * @return The number of peers in the conference. UINT32_MAX on failure.
 */
uint32_t Core::getGroupNumberPeers(int groupId) const
{
    QMutexLocker ml{&coreLoopLock};

    if (groupId >= static_cast<int>(Settings::NGC_GROUPNUM_OFFSET)) {
        Tox_Err_Group_Peer_Query error;
        uint32_t count = tox_group_peer_count(tox.get(), (groupId - Settings::NGC_GROUPNUM_OFFSET), &error);
        if (!PARSE_ERR(error)) {
            return std::numeric_limits<uint32_t>::max();
        }

        return count;
    } else {
        Tox_Err_Conference_Peer_Query error;
        uint32_t count = tox_conference_peer_count(tox.get(), groupId, &error);
        if (!PARSE_ERR(error)) {
            return std::numeric_limits<uint32_t>::max();
        }

        return count;
    }
}

/**
 * @brief Get the name of a peer of a group
 */
QString Core::getGroupPeerName(int groupId, int peerId) const
{
    QMutexLocker ml{&coreLoopLock};

    Tox_Err_Conference_Peer_Query error;
    size_t length = tox_conference_peer_get_name_size(tox.get(), groupId, peerId, &error);
    if (!PARSE_ERR(error) || !length) {
        qDebug() << "getGroupPeerName:error:1";
        return QString{};
    }

    std::vector<uint8_t> nameBuf(length);
    tox_conference_peer_get_name(tox.get(), groupId, peerId, nameBuf.data(), &error);
    if (!PARSE_ERR(error)) {
        qDebug() << "getGroupPeerName:error:2";
        return QString{};
    }

    return ToxString(nameBuf.data(), length).getQString();
}

/**
 * @brief Get the public key of a peer of a group
 */
ToxPk Core::getGroupPeerPk(int groupId, int peerId) const
{
    QMutexLocker ml{&coreLoopLock};

    uint8_t friendPk[TOX_PUBLIC_KEY_SIZE] = {0x00};

    if (groupId >= static_cast<int>(Settings::NGC_GROUPNUM_OFFSET)) {
        Tox_Err_Group_Peer_Query error;
        tox_group_peer_get_public_key(tox.get(), (groupId - Settings::NGC_GROUPNUM_OFFSET), peerId, friendPk, &error);
        if (!PARSE_ERR(error)) {
            qDebug() << "getGroupPeerPk:error:1";
            return ToxPk{};
        }

    } else {
        Tox_Err_Conference_Peer_Query error;
        tox_conference_peer_get_public_key(tox.get(), groupId, peerId, friendPk, &error);
        if (!PARSE_ERR(error)) {
            qDebug() << "getGroupPeerPk:error:2";
            return ToxPk{};
        }
    }

    return ToxPk(friendPk);
}

/**
 * @brief Get the names of the peers of a group
 */
QStringList Core::getGroupPeerNames(int groupId) const
{
    QMutexLocker ml{&coreLoopLock};

    assert(tox != nullptr);

    if (groupId >= static_cast<int>(Settings::NGC_GROUPNUM_OFFSET)) {
        uint32_t nPeers = getGroupNumberPeers(groupId);
        if (nPeers == std::numeric_limits<uint32_t>::max()) {
            qWarning() << "getGroupPeerNames NGC: Unable to get number of peers";
            return {};
        }

        Tox_Err_Group_Peer_Query error;
        uint32_t *peerlist = static_cast<uint32_t *>(calloc(nPeers, sizeof(uint32_t)));

        if (!peerlist) {
            qWarning() << "getGroupPeerNames NGC: calloc failed";
            return {};
        }

        tox_group_get_peerlist(tox.get(), (groupId - Settings::NGC_GROUPNUM_OFFSET), peerlist, &error);
        if (!PARSE_ERR(error)) {
            qWarning() << "getGroupPeerNames NGC: Unable to get peerlist";
            return {};
        }

        QStringList names;
        for (int i = 0; i < static_cast<int>(nPeers); ++i) {
            size_t length = tox_group_peer_get_name_size(tox.get(), (groupId - Settings::NGC_GROUPNUM_OFFSET), *(peerlist + i), &error);

            if (!PARSE_ERR(error) || !length) {
                names.append(QString());
                continue;
            }

            std::vector<uint8_t> nameBuf(length);
            tox_group_peer_get_name(tox.get(), (groupId - Settings::NGC_GROUPNUM_OFFSET), *(peerlist + i), nameBuf.data(), &error);
            if (PARSE_ERR(error)) {
                names.append(QString::number(*(peerlist + i)) + QString(":") + ToxString(nameBuf.data(), length).getQString());
            } else {
                qWarning() << "getGroupPeerNames NGC: tox_group_peer_get_name error";
                names.append(QString());
            }
        }

        free(peerlist);
        assert(names.size() == static_cast<int>(nPeers));

        return names;
    } else {
        uint32_t nPeers = getGroupNumberPeers(groupId);
        if (nPeers == std::numeric_limits<uint32_t>::max()) {
            qWarning() << "getGroupPeerNames: Unable to get number of peers";
            return {};
        }

        QStringList names;
        for (int i = 0; i < static_cast<int>(nPeers); ++i) {
            Tox_Err_Conference_Peer_Query error;
            size_t length = tox_conference_peer_get_name_size(tox.get(), groupId, i, &error);

            if (!PARSE_ERR(error) || !length) {
                names.append(QString());
                continue;
            }

            std::vector<uint8_t> nameBuf(length);
            tox_conference_peer_get_name(tox.get(), groupId, i, nameBuf.data(), &error);
            if (PARSE_ERR(error)) {
                names.append(ToxString(nameBuf.data(), length).getQString());
            } else {
                names.append(QString());
            }
        }

        assert(names.size() == static_cast<int>(nPeers));

        return names;
    }
}

/**
 * @brief Check, that group has audio or video stream
 * @param groupId Id of group to check
 * @return True for AV groups, false for text-only groups
 */
bool Core::getGroupAvEnabled(int groupId) const
{
    if (groupId >= static_cast<int>(Settings::NGC_GROUPNUM_OFFSET)) {
        return TOX_CONFERENCE_TYPE_TEXT;
    } else {
        QMutexLocker ml{&coreLoopLock};
        Tox_Err_Conference_Get_Type error;
        Tox_Conference_Type type = tox_conference_get_type(tox.get(), groupId, &error);
        PARSE_ERR(error);
        // would be nice to indicate to caller that we don't actually know..
        return type == TOX_CONFERENCE_TYPE_AV;
    }
}

/**
 * @brief Accept a groupchat invite.
 * @param inviteInfo Object which contains info about group invitation
 *
 * @return Conference number on success, UINT32_MAX on failure.
 */
uint32_t Core::joinGroupchat(const GroupInvite& inviteInfo)
{
    QMutexLocker ml{&coreLoopLock};

    const uint32_t friendId = inviteInfo.getFriendId();
    const uint8_t confType = inviteInfo.getType();
    const QByteArray invite = inviteInfo.getInvite();
    const uint8_t* const cookie = reinterpret_cast<const uint8_t*>(invite.data());
    const size_t cookieLength = invite.length();
    uint32_t groupNum{std::numeric_limits<uint32_t>::max()};
    switch (confType) {
    case TOX_CONFERENCE_TYPE_TEXT: {
        qDebug() << QString("Trying to accept invite for text group chat sent by friend %1").arg(friendId);
        Tox_Err_Conference_Join error;
        groupNum = tox_conference_join(tox.get(), friendId, cookie, cookieLength, &error);
        if (!PARSE_ERR(error)) {
            groupNum = std::numeric_limits<uint32_t>::max();
        }
        break;
    }
    case TOX_CONFERENCE_TYPE_AV: {
        qDebug() << QString("Trying to join AV groupchat invite sent by friend %1").arg(friendId);
        groupNum = toxav_join_av_groupchat(tox.get(), friendId, cookie, cookieLength,
                                           CoreAV::groupCallCallback, this);
        break;
    }
    default:
        qWarning() << "joinGroupchat: Unknown groupchat type " << confType;
    }
    if (groupNum != std::numeric_limits<uint32_t>::max()) {
        emit saveRequest();
        emit groupJoined(groupNum, getGroupPersistentId(groupNum, 0));
    }
    return groupNum;
}

void Core::groupInviteFriend(uint32_t friendId, int groupId)
{
    QMutexLocker ml{&coreLoopLock};

    if (groupId >= static_cast<int>(Settings::NGC_GROUPNUM_OFFSET)) {
        Tox_Err_Group_Invite_Friend error;
        bool result = tox_group_invite_friend(tox.get(),
            (groupId - Settings::NGC_GROUPNUM_OFFSET),
            friendId, &error);
        if (result) {
            qDebug() << "groupInviteFriend: inviting to NGC group OK, groupnum" << groupId << "error:" << error;
        } else {
            qWarning() << "groupInviteFriend: inviting to NGC group failed, groupnum" << groupId;
        }
    } else {
        Tox_Err_Conference_Invite error;
        tox_conference_invite(tox.get(), friendId, groupId, &error);
        qDebug() << "groupInviteFriend: inviting to group ... , groupnum" << groupId << "error:" << error;
        PARSE_ERR(error);
    }
}

void Core::changeOwnNgcName(uint32_t groupnumber, const QString& name)
{
    qDebug() << "changeOwnNgcName" << name << "gid" << groupnumber;
    if (groupnumber >= static_cast<int>(Settings::NGC_GROUPNUM_OFFSET)) {
        ToxString cName(name);
        bool res = tox_group_self_set_name(tox.get(), (groupnumber - Settings::NGC_GROUPNUM_OFFSET), cName.data(), cName.size(), NULL);
        if (res == false) {
            qWarning() << "changeOwnNgcName: setting new self name failed";
        } else {
            emit groupPeerlistChanged(groupnumber);
            emit saveRequest();
        }
    }
}

int Core::createGroup(uint8_t type)
{
    QMutexLocker ml{&coreLoopLock};

    if (type == TOX_CONFERENCE_TYPE_TEXT) {
        Tox_Err_Conference_New error;
        uint32_t groupId = tox_conference_new(tox.get(), &error);
        if (PARSE_ERR(error)) {
            emit saveRequest();
            emit emptyGroupCreated(groupId, getGroupPersistentId(groupId, 0));
            return groupId;
        } else {
            return std::numeric_limits<uint32_t>::max();
        }
    } else if (type == TOX_CONFERENCE_TYPE_AV) {
        // unlike tox_conference_new, toxav_add_av_groupchat does not have an error enum, so -1 group number is our
        // only indication of an error
        int groupId = toxav_add_av_groupchat(tox.get(), CoreAV::groupCallCallback, this);
        if (groupId != -1) {
            emit saveRequest();
            emit emptyGroupCreated(groupId, getGroupPersistentId(groupId, 0));
        } else {
            qCritical() << "Unknown error creating AV groupchat";
        }
        return groupId;
    } else {
        qWarning() << "createGroup: Unknown type " << type;
        return -1;
    }
}

/**
 * @brief Checks if a friend is online. Unknown friends are considered offline.
 */
bool Core::isFriendOnline(uint32_t friendId) const
{
    QMutexLocker ml{&coreLoopLock};

    Tox_Err_Friend_Query error;
    Tox_Connection connection = tox_friend_get_connection_status(tox.get(), friendId, &error);
    PARSE_ERR(error);
    return connection != TOX_CONNECTION_NONE;
}

/**
 * @brief Checks if we have a friend by public key
 */
bool Core::hasFriendWithPublicKey(const ToxPk& publicKey) const
{
    QMutexLocker ml{&coreLoopLock};

    if (publicKey.isEmpty()) {
        return false;
    }

    Tox_Err_Friend_By_Public_Key error;
    (void)tox_friend_by_public_key(tox.get(), publicKey.getData(), &error);
    return PARSE_ERR(error);
}

/**
 * @brief Get the public key part of the ToxID only
 */
ToxPk Core::getFriendPublicKey(uint32_t friendNumber) const
{
    QMutexLocker ml{&coreLoopLock};

    uint8_t rawid[TOX_PUBLIC_KEY_SIZE];
    Tox_Err_Friend_Get_Public_Key error;
    tox_friend_get_public_key(tox.get(), friendNumber, rawid, &error);
    if (!PARSE_ERR(error)) {
        qWarning() << "getFriendPublicKey: Getting public key failed";
        return ToxPk();
    }

    return ToxPk(rawid);
}

/**
 * @brief Get the username of a friend
 */
QString Core::getFriendUsername(uint32_t friendnumber) const
{
    QMutexLocker ml{&coreLoopLock};

    Tox_Err_Friend_Query error;
    size_t nameSize = tox_friend_get_name_size(tox.get(), friendnumber, &error);
    if (!PARSE_ERR(error) || !nameSize) {
        return QString();
    }

    std::vector<uint8_t> nameBuf(nameSize);
    tox_friend_get_name(tox.get(), friendnumber, nameBuf.data(), &error);
    if (!PARSE_ERR(error)) {
        return QString();
    }
    return ToxString(nameBuf.data(), nameSize).getQString();
}

uint64_t Core::getMaxMessageSize() const
{
    /*
     * TODO: Remove this hack; the reported max message length we receive from c-toxcore
     * as of 08-02-2019 is inaccurate, causing us to generate too large messages when splitting
     * them up.
     *
     * The inconsistency lies in c-toxcore group.c:2480 using MAX_GROUP_MESSAGE_DATA_LEN to verify
     * message size is within limit, but tox_max_message_length giving a different size limit to us.
     *
     * (uint32_t tox_max_message_length(void); declared in tox.h, unable to see explicit definition)
     */
    return tox_max_message_length() - 50;
}

QString Core::getPeerName(const ToxPk& id) const
{
    QMutexLocker ml{&coreLoopLock};

    Tox_Err_Friend_By_Public_Key keyError;
    uint32_t friendId = tox_friend_by_public_key(tox.get(), id.getData(), &keyError);
    if (!PARSE_ERR(keyError)) {
        qWarning() << "getPeerName: No such peer";
        return {};
    }

    Tox_Err_Friend_Query queryError;
    const size_t nameSize = tox_friend_get_name_size(tox.get(), friendId, &queryError);
    if (!PARSE_ERR(queryError) || !nameSize) {
        return {};
    }

    std::vector<uint8_t> nameBuf(nameSize);
    tox_friend_get_name(tox.get(), friendId, nameBuf.data(), &queryError);
    if (!PARSE_ERR(queryError)) {
        qWarning() << "getPeerName: Can't get name of friend " + QString().setNum(friendId);
        return {};
    }

    return ToxString(nameBuf.data(), nameSize).getQString();
}

/**
 * @brief Sets the NoSpam value to prevent friend request spam
 * @param nospam an arbitrary which becomes part of the Tox ID
 */
void Core::setNospam(uint32_t nospam)
{
    QMutexLocker ml{&coreLoopLock};

    tox_self_set_nospam(tox.get(), nospam);
    emit idSet(getSelfId());
}
