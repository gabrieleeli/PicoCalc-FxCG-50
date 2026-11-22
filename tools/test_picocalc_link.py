#!/usr/bin/env python3
"""Prove di andata e ritorno di tools/picocalc_link.py (v1.17.0) con il
protocollo C++ del firmware (tests/link_peer, stesso src/link_protocol.cpp).

Uso: python3 tools/test_picocalc_link.py PERCORSO/link_peer
"""
import os
import random
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import picocalc_link as pl  # noqa: E402

PEER = None


class Unit(unittest.TestCase):
    def test_frame_roundtrip_and_noise(self):
        f = pl.encode_frame(pl.DATA, 513, b"\x01\x02\x7eP")
        d = pl.Decoder()
        broken = bytearray(f)
        broken[8] ^= 0xFF
        for b in b"rumore\n\x7e\x7e" + f + bytes(broken) + f:
            d.push(bytes([b]))
        frames = d.frames()
        self.assertEqual(frames, [(pl.DATA, 513, b"\x01\x02\x7eP")] * 2)
        self.assertGreater(d.discarded, 0)

    def test_package_and_file(self):
        entries = [(0, "", b"\x01\x02"), (1, "PICOCALC/SHEET/CONTI.pcsheet", bytes(range(256)) * 9)]
        pkg = pl.encode_package(entries)
        self.assertEqual(pl.decode_package(pkg), entries)
        self.assertEqual(pl.decode_file(pl.encode_file(pkg)), pkg)
        for bad in ("../x", "PICOCALC/../x", "OTHER/A.txt", "PICOCALC/", "PICOCALC/A B.txt"):
            self.assertFalse(pl.valid_path(bad), bad)
        with self.assertRaises(pl.LinkError):
            pl.decode_package(pl.encode_package([(1, "PICOCALC/../evil", b"")]))
        damaged = bytearray(pl.encode_file(pkg))
        damaged[-1] ^= 1
        with self.assertRaises(pl.LinkError):
            pl.decode_file(bytes(damaged))

    def test_device_paths(self):
        self.assertEqual(pl.device_path("/tmp/HI.py", ""), "PICOCALC/PY/HI.py")
        self.assertEqual(pl.device_path("x/CONTI.pcsheet", ""), "PICOCALC/SHEET/CONTI.pcsheet")
        self.assertEqual(pl.device_path("a.bin", "PICOCALC/LIST/A.csv"), "PICOCALC/LIST/A.csv")
        with self.assertRaises(pl.LinkError):
            pl.device_path("a.bin", "")


class RoundTrip(unittest.TestCase):
    def setUp(self):
        if PEER is None:
            self.skipTest("serve il percorso di link_peer")
        self.tmp = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.tmp.cleanup()

    def path(self, name):
        return os.path.join(self.tmp.name, name)

    def test_pc_to_device(self):
        rnd = random.Random(7)
        big = bytes(rnd.randrange(256) for _ in range(5000))
        with open(self.path("HI.py"), "w") as f:
            f.write("print('hi')\n")
        with open(self.path("big.bin"), "wb") as f:
            f.write(big)
        with open(self.path("ws.bin"), "wb") as f:
            f.write(b"WAXF" + bytes(40))
        out = self.path("dev.bin")
        for noise in ([], ["--noise"]):
            rc = pl.main(["send", "--quiet", "--command", PEER + " receive " + out, "--workspace", self.path("ws.bin"),
                          self.path("HI.py"), self.path("big.bin") + ":PICOCALC/LIST/BIG.csv"] + noise)
            self.assertEqual(rc, 0)
            with open(out, "rb") as f:
                entries = pl.decode_package(f.read())
            self.assertEqual(entries, [(0, "", b"WAXF" + bytes(40)), (1, "PICOCALC/PY/HI.py", b"print('hi')\n"),
                                       (1, "PICOCALC/LIST/BIG.csv", big)])

    def test_device_to_pc(self):
        entries = [(1, "PICOCALC/GEOM/SEGM.pcgeo", bytes(range(200)) * 30)]
        with open(self.path("pkg.bin"), "wb") as f:
            f.write(pl.encode_package(entries))
        out = self.path("recv.pclink")
        rc = pl.main(["receive", "--quiet", "--command", PEER + " send " + self.path("pkg.bin") + " SEGM", "-o", out,
                      "--extract", self.path("x"), "--wait", "10"])
        self.assertEqual(rc, 0)
        with open(out, "rb") as f:
            self.assertEqual(pl.decode_package(pl.decode_file(f.read())), entries)
        with open(self.path("x/PICOCALC/GEOM/SEGM.pcgeo"), "rb") as f:
            self.assertEqual(f.read(), entries[0][2])
        # il pacchetto ricevuto si rimanda cosi' com'e' (.pclink)
        back = self.path("again.bin")
        self.assertEqual(pl.main(["send", "--quiet", "--command", PEER + " receive " + back, "--package", out]), 0)
        with open(back, "rb") as f:
            self.assertEqual(pl.decode_package(f.read()), entries)

    def test_pc_rejects_damaged_package(self):
        # il trasporto e' corretto (CRC giusti) ma il contenuto non e' un pacchetto: rifiutato, nessun file
        with open(self.path("bad.bin"), "wb") as f:
            f.write(b"\x09garbage")
        out = self.path("bad.pclink")
        rc = pl.main(["receive", "--quiet", "--command", PEER + " send " + self.path("bad.bin") + " BAD", "-o", out, "--wait", "10"])
        self.assertEqual(rc, 1)
        self.assertFalse(os.path.exists(out))


if __name__ == "__main__":
    if len(sys.argv) >= 2:
        PEER = os.path.abspath(sys.argv.pop(1))
    unittest.main(verbosity=1)
