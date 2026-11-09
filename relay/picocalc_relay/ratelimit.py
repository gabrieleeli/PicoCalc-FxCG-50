"""Per-device sliding-window (per minute) and daily request limits."""
from __future__ import annotations

import threading
import time
from collections import defaultdict, deque


class RateLimiter:
    def __init__(self, per_minute: int, per_day: int, clock=time.time):
        self.per_minute = per_minute
        self.per_day = per_day
        self.clock = clock
        self._lock = threading.Lock()
        self._recent: dict[str, deque[float]] = defaultdict(deque)
        self._daily: dict[str, tuple[int, int]] = {}

    def check(self, device: str) -> str | None:
        """Records a request; returns None if allowed, otherwise the reason."""
        now = self.clock()
        day = int(now // 86400)
        with self._lock:
            recent = self._recent[device]
            while recent and now - recent[0] >= 60:
                recent.popleft()
            d, count = self._daily.get(device, (day, 0))
            if d != day:
                count = 0
            if count >= self.per_day:
                return "Limite giornaliero di richieste raggiunto"
            if len(recent) >= self.per_minute:
                return "Troppe richieste: attendi un minuto"
            recent.append(now)
            self._daily[device] = (day, count + 1)
            return None
