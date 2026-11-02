#pragma once
#include "Types.h"

namespace ms
{
/** Pattern index for step counter `n` (counts from 0 per track). Random modes keep a little state. */
class DirectionWalker
{
public:
    void reset() { pos = 0; }

    int index (Direction d, int64_t n, int len, Rng& rng)
    {
        if (len <= 1) return 0;
        switch (d)
        {
            case Direction::Forward:  return int (posMod (n, len));
            case Direction::Reverse:  return len - 1 - int (posMod (n, len));
            case Direction::PingPong: // 0 1 2 3 2 1 0 1 ... (end points not repeated)
            {
                const int period = 2 * len - 2, p = int (posMod (n, period));
                return p < len ? p : period - p;
            }
            case Direction::Pendulum: // 0 1 2 3 3 2 1 0 0 1 ... (end points repeated)
            {
                const int period = 2 * len, p = int (posMod (n, period));
                return p < len ? p : period - 1 - p;
            }
            case Direction::Random:
                return rng.nextInt (len);
            case Direction::RandomWalk:
            {
                pos = std::min (pos, len - 1);
                pos += rng.nextInt (2) == 0 ? -1 : 1;
                if (pos < 0) pos = 1;
                if (pos > len - 1) pos = len - 2;
                return pos;
            }
        }
        return 0;
    }

private:
    int pos = 0;
};
} // namespace ms
