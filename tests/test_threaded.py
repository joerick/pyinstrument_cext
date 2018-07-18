from unittest import TestCase
import time, threading

from pyinstrument_cext import setstatprofile

def do_nothing():
    pass

def busy_wait(duration):
    end_time = time.time() + duration

    while time.time() < end_time:
        do_nothing()


class TestThreaded(TestCase):
    def setUp(self):
        self.count = 0
        self.count_lock = threading.Lock()

    def profile_callback(self, frame, event, arg):
        self.count_lock.acquire()
        self.count += 1
        self.count_lock.release()

    def profile_a_busy_wait(self):
        setstatprofile(self.profile_callback, 0.1)
        busy_wait(1.0)
        setstatprofile(None)

    def test_threaded(self):
        threads = [threading.Thread(target=self.profile_a_busy_wait) for _ in range(10)]
        for thread in threads:
            thread.start()

        for thread in threads:
            thread.join()
        
        # threaded counts are way lower, presumably due to the GIL.
        # Really we only care that it didn't crash!
        self.assertTrue(50 < self.count < 105,
                'profile count should be approx. 100, was %i' % self.count)
