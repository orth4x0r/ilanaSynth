#include "ModalPianoTable.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "WavetableData.h"

namespace
{
    struct Reader
    {
        const unsigned char* data;
        size_t size, position = 0;

        bool has (size_t bytes) const { return position + bytes <= size; }

        template <typename T>
        T read()
        {
            T value {};
            if (has (sizeof (T)))
                std::memcpy (&value, data + position, sizeof (T));
            position += sizeof (T);
            return value;
        }
    };

    ModalPianoTable parse()
    {
        ModalPianoTable table;
        Reader in { reinterpret_cast<const unsigned char*> (WavetableData::modalpiano_bin),
                    (size_t) WavetableData::modalpiano_binSize };
        if (! in.has (14) || std::memcmp (in.data, "IMPT", 4) != 0)
            return table;
        in.position = 4;
        const auto version = in.read<std::uint16_t>();
        const auto count = in.read<std::uint16_t>();
        const auto timeCount = in.read<std::uint16_t>();
        table.rate = in.read<float>();
        if (version < 1 || version > 3 || timeCount == 0)
            return {};
        for (int t = 0; t < timeCount; ++t)
            table.times.push_back (in.read<float>());

        std::vector<ModalPianoTable::Entry> read;
        for (int e = 0; e < count; ++e)
        {
            ModalPianoTable::Entry entry;
            entry.note = in.read<std::uint16_t>();
            entry.velocity = in.read<std::uint16_t>();
            entry.partials = in.read<std::uint16_t>();
            const auto attackLength = in.read<std::uint32_t>();
            const auto scale = in.read<float>();
            if (! in.has ((size_t) entry.partials * (4 + 2 * (size_t) timeCount) + (size_t) attackLength * 2))
                return {};
            for (int p = 0; p < entry.partials; ++p)
                entry.ratios.push_back (in.read<float>());
            for (int i = 0; i < entry.partials * timeCount; ++i)
                entry.levels.push_back ((float) in.read<std::int16_t>() / 64.0f);
            for (std::uint32_t i = 0; i < attackLength; ++i)
                entry.attack.push_back ((float) in.read<std::int16_t>() / 32767.0f * scale);
            if (version >= 2)
            {
                const auto modes = in.read<std::uint16_t>();
                if (! in.has ((size_t) modes * (4 + 2 * (size_t) timeCount)))
                    return {};
                for (int m = 0; m < modes; ++m)
                    entry.modes.push_back (in.read<float>());
                for (int i = 0; i < modes * timeCount; ++i)
                    entry.modeLevels.push_back ((float) in.read<std::int16_t>() / 64.0f);
            }
            if (version >= 3)
            {
                const auto bands = in.read<std::uint16_t>();
                if (bands != ModalPianoTable::noiseBandCount || ! in.has ((size_t) bands * 2 * (size_t) timeCount))
                    return {};
                for (int i = 0; i < bands * timeCount; ++i)
                    entry.noiseLevels.push_back ((float) in.read<std::int16_t>() / 64.0f);
            }
            read.push_back (std::move (entry));
        }

        for (const auto& entry : read)
        {
            if (std::find (table.notes.begin(), table.notes.end(), entry.note) == table.notes.end())
                table.notes.push_back (entry.note);
            if (std::find (table.velocities.begin(), table.velocities.end(), entry.velocity) == table.velocities.end())
                table.velocities.push_back (entry.velocity);
        }
        std::sort (table.notes.begin(), table.notes.end());
        std::sort (table.velocities.begin(), table.velocities.end());

        // Every note at every velocity, or nothing.
        table.entries.resize (table.notes.size() * table.velocities.size());
        std::vector<bool> filled (table.entries.size(), false);
        for (auto& entry : read)
        {
            const auto n = (size_t) (std::find (table.notes.begin(), table.notes.end(), entry.note) - table.notes.begin());
            const auto v = (size_t) (std::find (table.velocities.begin(), table.velocities.end(), entry.velocity) - table.velocities.begin());
            const auto index = v * table.notes.size() + n;
            filled[index] = true;
            table.entries[index] = std::move (entry);
        }
        if (std::find (filled.begin(), filled.end(), false) != filled.end())
            return {};
        return table;
    }
}

const ModalPianoTable& ModalPianoTable::get()
{
    static const ModalPianoTable table = parse();
    return table;
}
