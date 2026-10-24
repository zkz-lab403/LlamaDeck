#include "GgufParser.h"

#include <QByteArray>
#include <QFile>
#include <QtEndian>

#include <cstring>

namespace {

qint64 scalarSize(quint32 type)
{
    switch (type) {
    case 0:  // uint8
    case 1:  // int8
    case 7:  // bool
        return 1;
    case 2:  // uint16
    case 3:  // int16
        return 2;
    case 4:  // uint32
    case 5:  // int32
    case 6:  // float32
        return 4;
    case 10: // uint64
    case 11: // int64
    case 12: // float64
        return 8;
    default:
        return -1;
    }
}

bool isUnsignedInt(quint32 type)
{
    return type == 0 || type == 2 || type == 4 || type == 10;
}

class Reader {
public:
    Reader(const QByteArray &data, quint32 version, qint64 start)
        : m_data(data), m_version(version), pos(start)
    {
    }

    bool ok() const { return m_ok; }
    qint64 remaining() const { return m_ok ? m_data.size() - pos : 0; }

    bool need(qint64 n)
    {
        if (!m_ok || n < 0 || n > m_data.size() - pos) {
            m_ok = false;
            return false;
        }
        return true;
    }

    quint8 u8()
    {
        if (!need(1))
            return 0;
        return quint8(m_data.at(int(pos++)));
    }

    quint16 u16()
    {
        if (!need(2))
            return 0;
        const quint16 v = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(m_data.constData() + pos));
        pos += 2;
        return v;
    }

    quint32 u32()
    {
        if (!need(4))
            return 0;
        const quint32 v = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar *>(m_data.constData() + pos));
        pos += 4;
        return v;
    }

    quint64 u64()
    {
        if (!need(8))
            return 0;
        const quint64 v = qFromLittleEndian<quint64>(
            reinterpret_cast<const uchar *>(m_data.constData() + pos));
        pos += 8;
        return v;
    }

    float f32()
    {
        const quint32 bits = u32();
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    void skip(qint64 n)
    {
        if (need(n))
            pos += n;
    }

    bool skipString()
    {
        const quint64 len = (m_version == 1) ? u32() : u64();
        if (!m_ok)
            return false;
        return need(qint64(len)) && (pos += qint64(len), true);
    }

    QString string()
    {
        const quint64 len = (m_version == 1) ? u32() : u64();
        if (!m_ok || len > quint64(32 * 1024 * 1024) || !need(qint64(len)))
            return QString();
        const QString value = QString::fromUtf8(m_data.constData() + pos, int(len));
        pos += qint64(len);
        return value;
    }

    bool skipValue(quint32 type)
    {
        const qint64 fixed = scalarSize(type);
        if (fixed > 0) {
            skip(fixed);
            return m_ok;
        }
        if (type == 8)
            return skipString();
        if (type == 9) {
            const quint32 elemType = u32();
            const quint64 count = u64();
            if (!m_ok)
                return false;
            const qint64 elemSize = scalarSize(elemType);
            if (elemSize > 0) {
                if (count > quint64(remaining() / elemSize)) {
                    m_ok = false;
                    return false;
                }
                skip(qint64(count) * elemSize);
                return m_ok;
            }
            if (elemType == 8) {
                for (quint64 i = 0; i < count; ++i) {
                    if (!skipString())
                        return false;
                }
                return m_ok;
            }
            m_ok = false;
            return false;
        }
        m_ok = false;
        return false;
    }

private:
    const QByteArray &m_data;
    quint32 m_version = 3;
    qint64 pos = 0;
    bool m_ok = true;
};

quint64 readUInt(Reader &r, quint32 type)
{
    switch (type) {
    case 0:
        return r.u8();
    case 2:
        return r.u16();
    case 4:
        return r.u32();
    case 10:
        return r.u64();
    case 5:
        return quint64(qint32(r.u32()));
    case 11:
        return quint64(qint64(r.u64()));
    case 3:
        return quint64(qint16(r.u16()));
    default:
        r.skipValue(type);
        return 0;
    }
}

} // namespace

GgufInfo GgufParser::parse(const QString &filePath)
{
    GgufInfo info;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return info;

    const qint64 cap = 8LL * 1024 * 1024;
    const QByteArray data = file.read(qMin(cap, file.size()));
    if (data.size() < 24 || !data.startsWith("GGUF"))
        return info;

    const quint32 version = qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(data.constData() + 4));
    if (version < 1 || version > 4)
        return info;

    const quint64 kvCount = qFromLittleEndian<quint64>(
        reinterpret_cast<const uchar *>(data.constData() + 16));
    if (kvCount > 1000000)
        return info;

    info.valid = true;

    Reader reader(data, version, 24);
    QString architecture;

    for (quint64 i = 0; i < kvCount && reader.ok(); ++i) {
        const QString key = reader.string();
        const quint32 type = reader.u32();
        if (!reader.ok())
            break;

        if (key.isEmpty()) {
            reader.skipValue(type);
            continue;
        }

        if (type == 8 && (key == QLatin1String("general.name")
                          || key == QLatin1String("general.architecture"))) {
            const QString value = reader.string();
            if (!reader.ok())
                break;
            if (key == QLatin1String("general.name")) {
                info.name = value;
            } else {
                info.architecture = value;
                architecture = value;
            }
            continue;
        }

        if (isUnsignedInt(type)) {
            const quint64 value = readUInt(reader, type);
            if (key == QLatin1String("general.parameter_count"))
                info.parameterCount = value;
            else if (key.endsWith(QLatin1String(".context_length")))
                info.contextLength = value;
            else if (key.endsWith(QLatin1String(".block_count")))
                info.blockCount = int(value);
            continue;
        }

        if (type == 5 || type == 11 || type == 3 && key.endsWith(QLatin1String(".block_count"))) {
            // Signed integer metadata: still usable for our numeric fields.
            const quint64 value = readUInt(reader, type);
            if (key == QLatin1String("general.parameter_count"))
                info.parameterCount = value;
            else if (key.endsWith(QLatin1String(".context_length")))
                info.contextLength = value;
            else if (key.endsWith(QLatin1String(".block_count")))
                info.blockCount = int(value);
            continue;
        }

        reader.skipValue(type);
    }

    Q_UNUSED(architecture)
    return info;
}
