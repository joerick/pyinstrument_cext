pyinstrument_cext
=================

[Pyinstrument][1]'s C extensions - reducing the overhead of statistical profilers.

API
---

### `pyinstrument_cext.setstatprofile(profilefunc, interval)`

Sets the statistal profiler callback. The function in the same manner as [setprofile][2], but
instead of being called every on every call and return, the function is throttled - called no more
than every `interval` seconds with the current stack.

[1]: https://github.com/joerick/pyinstrument
[2]: https://docs.python.org/2/library/sys.html#sys.setprofile
