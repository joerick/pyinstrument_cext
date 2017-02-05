from unittest import TestCase
import time

from pyinstrument_cext import setstatprofile

def do_nothing():
    pass

def busy_wait(duration):
    end_time = time.time() + duration

    while time.time() < end_time:
        do_nothing()

class TestSetstatprofile(TestCase):
    def setUp(self):
        self.count = 0

    def profile_callback(self, frame, event, arg):
        self.count += 1

    def test100ms(self):
        setstatprofile(self.profile_callback, 0.1)
        busy_wait(1.0)
        setstatprofile(None)
        # check that the profiling calls were throttled appropriately
        self.assertTrue(8 < self.count < 12)

    def test10ms(self):
        setstatprofile(self.profile_callback, 0.01)
        busy_wait(1.0)
        setstatprofile(None)
        self.assertTrue(98 < self.count < 102)
