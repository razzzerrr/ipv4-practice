#pragma once
#include <QString>
#include <QStringList>
#include <QHostAddress>
#include <vector>
#include <algorithm>

class IpParser {
public:
    // Главный метод: принимает любую строку и возвращает список индексов подсетей /24
    // Если формат совсем битый — вернет false в переменную success
    static std::vector<uint32_t> parseStringTo24Blocks(const QString& rawInput, bool& success) {
        success = true;
        std::vector<uint32_t> indices;

        QString input = rawInput.trimmed();
        if (input.isEmpty()) {
            success = false;
            return indices;
        }

        // 1. ВАРИАНТ: Диапазон (например, "192.168.1.0-192.168.5.255")
        if (input.contains('-')) {
            QStringList parts = input.split('-', Qt::SkipEmptyParts);
            if (parts.size() != 2) {
                success = false;
                return indices;
            }

            bool ok1, ok2;
            quint32 startIp = parsePartialIp(parts[0].trimmed(), ok1);
            quint32 endIp = parsePartialIp(parts[1].trimmed(), ok2);

            if (!ok1 || !ok2) {
                success = false;
                return indices;
            }

            // Переводим IP в индексы блоков /24 (отбрасываем последний байт сдвигом на 8)
            uint32_t startIdx = startIp >> 8;
            uint32_t endIdx = endIp >> 8;

            if (startIdx > endIdx) {
                std::swap(startIdx, endIdx);
            }

            for (uint32_t idx = startIdx; idx <= endIdx; ++idx) {
                indices.push_back(idx);
            }
            return indices;
        }

        // 2. ВАРИАНТ: С маской CIDR (например, "192.168.0.0/16" или сокращенный "192./16")
        if (input.contains('/')) {
            QStringList parts = input.split('/', Qt::SkipEmptyParts);
            if (parts.size() != 2) {
                success = false;
                return indices;
            }

            bool maskOk;
            int mask = parts[1].trimmed().toInt(&maskOk);
            if (!maskOk || mask < 0 || mask > 32) {
                success = false;
                return indices;
            }

            bool ipOk;
            quint32 baseIp = parsePartialIp(parts[0].trimmed(), ipOk);
            if (!ipOk) {
                success = false;
                return indices;
            }

            // Выравниваем базовый IP по маске подсети (зануляем лишние биты справа)
            quint32 netMask = (mask == 0) ? 0 : (0xFFFFFFFF << (32 - mask));
            quint32 startIp = baseIp & netMask;

            if (mask >= 24) {
                // Если маска /24, /25... /32 — это всё укладывается в один блок /24
                indices.push_back(startIp >> 8);
            }
            else {
                // Если маска шире (например /16 или /8), вычисляем сколько блоков /24 она занимает
                uint32_t numBlocks = 1 << (24 - mask);
                uint32_t startIdx = startIp >> 8;
                for (uint32_t i = 0; i < numBlocks; ++i) {
                    indices.push_back(startIdx + i);
                }
            }
            return indices;
        }

        // 3. ВАРИАНТ: Одиночный IP (например, "192.168.1.15" или "10.10.10.")
        bool ipOk;
        quint32 ip = parsePartialIp(input, ipOk);
        if (!ipOk) {
            success = false;
            return indices;
        }
        indices.push_back(ip >> 8);
        return indices;
    }

private:
    // Умное дополнение неполного IP нулями и валидация через QHostAddress
    static quint32 parsePartialIp(const QString& str, bool& ok) {
        ok = false;

        // Разделяем по точкам. Оставляем пустые части, чтобы "192." корректно обрабатывался
        QStringList parts = str.split('.', Qt::KeepEmptyParts);

        // Очищаем от мусора и пустых хвостов
        while (!parts.isEmpty() && parts.last().trimmed().isEmpty()) {
            parts.removeLast();
        }

        if (parts.isEmpty() || parts.size() > 4) {
            return 0;
        }

        // Дописываем недостающие октеты нулями ( превращаем "192" -> "192.0.0.0" )
        while (parts.size() < 4) {
            parts.append("0");
        }

        QString fullIpStr = parts.join(".");

        // Делегируем валидацию и парсинг встроенному классу Qt
        QHostAddress address;
        if (address.setAddress(fullIpStr)) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                ok = true;
                return address.toIPv4Address(); // Возвращает готовый чистый quint32
            }
        }

        return 0; // Ошибка валидации (например, если ввели "abc.def.1.1")
    }
};