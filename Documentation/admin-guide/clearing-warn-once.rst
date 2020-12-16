Clearing WARN_ONCE
------------------

WARN_ONCE / WARN_ON_ONCE / printk_once only emit a message once.

echo 1 > /sys/kernel/debug/clear_warn_once

clears the state and allows the warnings to print once again.
This can be useful after test suite runs to reproduce problems.

Values greater than one set a timer for a periodic state reset; e.g.

echo 60 > /sys/kernel/debug/clear_warn_once

will establish an hourly state reset, effectively turning WARN_ONCE
into a long period rate-limited warning.

Writing a value of zero (or one) will remove any previously set timer.
