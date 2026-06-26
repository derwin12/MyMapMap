/*
 * FppMultiSyncListener.cpp
 *
 * (c) 2026 MyMapMap contributors
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

#include "FppMultiSyncListener.h"

#include <QUdpSocket>
#include <QtEndian>
#include <QDebug>

#include <cstring>

namespace mmp {

// FPP MultiSync wire format (all multi-byte fields little-endian, packed).
//
//   ControlPkt (7 bytes):
//     char     fppd[4];        // 'F','P','P','D'
//     uint8_t  pktType;        // CTRL_PKT_* (1 == SYNC)
//     uint16_t extraDataLen;
//
//   SyncPkt body (follows the header for CTRL_PKT_SYNC):
//     uint8_t  syncType;       // SYNC_PKT_* (start/stop/sync/open)
//     uint8_t  fileType;       // SYNC_FILE_* (sequence/media)
//     uint32_t frameNumber;
//     float    secondsElapsed;
//     char     filename[];     // null-terminated, fills the rest of the packet
namespace {
constexpr int   CTRL_HEADER_LEN = 7;
constexpr quint8 CTRL_PKT_SYNC  = 1;

constexpr quint8 SYNC_PKT_START = 0;
constexpr quint8 SYNC_PKT_STOP  = 1;
constexpr quint8 SYNC_PKT_SYNC  = 2;
constexpr quint8 SYNC_PKT_OPEN  = 3;

// Offsets of the SyncPkt fields within the datagram.
constexpr int OFF_SYNC_TYPE = CTRL_HEADER_LEN + 0; // 7
constexpr int OFF_FILE_TYPE = CTRL_HEADER_LEN + 1; // 8
constexpr int OFF_FRAME_NUM = CTRL_HEADER_LEN + 2; // 9
constexpr int OFF_SECONDS   = CTRL_HEADER_LEN + 6; // 13
constexpr int OFF_FILENAME  = CTRL_HEADER_LEN + 10; // 17

// Reads a little-endian IEEE-754 float from a (possibly unaligned) byte buffer.
float readLittleEndianFloat(const char* p)
{
  const quint32 bits = qFromLittleEndian<quint32>(
      reinterpret_cast<const uchar*>(p));
  float f;
  std::memcpy(&f, &bits, sizeof(f));
  return f;
}
} // namespace

FppMultiSyncListener::FppMultiSyncListener(quint16 port, QObject* parent)
  : QObject(parent),
    _socket(nullptr),
    _port(port),
    _verbose(false)
{
}

FppMultiSyncListener::~FppMultiSyncListener()
{
  stop();
}

bool FppMultiSyncListener::start()
{
  if (_socket)
    return true; // already listening

  _socket = new QUdpSocket(this);

  // ShareAddress + ReuseAddressHint let us co-exist with FPP itself or other
  // MultiSync listeners on the same machine (FPP broadcasts to all of them).
  const bool bound = _socket->bind(
      QHostAddress::AnyIPv4, _port,
      QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);

  if (!bound)
  {
    qWarning() << "FPP MultiSync: could not bind UDP port" << _port
               << "-" << _socket->errorString();
    delete _socket;
    _socket = nullptr;
    return false;
  }

  connect(_socket, &QUdpSocket::readyRead,
          this, &FppMultiSyncListener::onReadyRead);

  qInfo() << "FPP MultiSync: listening on UDP port" << _port;
  return true;
}

void FppMultiSyncListener::stop()
{
  if (_socket)
  {
    _socket->close();
    _socket->deleteLater();
    _socket = nullptr;
  }
}

bool FppMultiSyncListener::isListening() const
{
  return _socket != nullptr;
}

void FppMultiSyncListener::onReadyRead()
{
  while (_socket && _socket->hasPendingDatagrams())
  {
    QByteArray datagram;
    datagram.resize(int(_socket->pendingDatagramSize()));
    _socket->readDatagram(datagram.data(), datagram.size());
    parseDatagram(datagram.constData(), datagram.size());
  }
}

void FppMultiSyncListener::parseDatagram(const char* data, int len)
{
  // Must at least contain the control header.
  if (len < CTRL_HEADER_LEN)
    return;

  // Magic "FPPD".
  if (data[0] != 'F' || data[1] != 'P' || data[2] != 'P' || data[3] != 'D')
    return;

  const quint8 pktType = quint8(data[4]);
  if (pktType != CTRL_PKT_SYNC)
    return; // we only act on sync packets in this prototype

  // Need the full fixed SyncPkt body (filename may be empty).
  if (len < OFF_FILENAME)
    return;

  const quint8 syncType = quint8(data[OFF_SYNC_TYPE]);
  const quint32 frameNumber = qFromLittleEndian<quint32>(
      reinterpret_cast<const uchar*>(data + OFF_FRAME_NUM));
  const double secondsElapsed = readLittleEndianFloat(data + OFF_SECONDS);

  // Filename is null-terminated and fills the remainder of the datagram.
  QString filename;
  if (len > OFF_FILENAME)
  {
    const char* nameStart = data + OFF_FILENAME;
    const int maxNameLen = len - OFF_FILENAME;
    const int nameLen = int(qstrnlen(nameStart, maxNameLen));
    filename = QString::fromUtf8(nameStart, nameLen);
  }

  if (_verbose)
  {
    qInfo() << "FPP MultiSync: syncType" << syncType
            << "frame" << frameNumber
            << "seconds" << secondsElapsed
            << "file" << filename;
  }

  switch (syncType)
  {
  case SYNC_PKT_START:
    emit mediaStart(filename, secondsElapsed);
    break;
  case SYNC_PKT_STOP:
    emit mediaStop(filename);
    break;
  case SYNC_PKT_SYNC:
    emit mediaSync(filename, secondsElapsed, frameNumber);
    break;
  case SYNC_PKT_OPEN:
    emit mediaOpen(filename);
    break;
  default:
    break;
  }
}

}
