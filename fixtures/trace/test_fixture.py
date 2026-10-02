#!/usr/bin/env python3
"""Independent checks of fixture evidence and lexical comparison boundaries."""
import hashlib
import json
import subprocess
import unittest
from pathlib import Path
from build_fixture import functions

ROOT = Path(__file__).resolve().parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class TraceFixtureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads((ROOT / "trace-manifest.json").read_text())

    def test_real_diff_and_hashes(self):
        actual = subprocess.run(["git", "diff", "--no-index", "--", "base/demo.cpp", "head/demo.cpp"],
                                cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(actual.returncode, 1)
        self.assertEqual(actual.stdout, (ROOT / "base-head.diff").read_text())
        for revision, info in self.data["revisions"].items():
            self.assertEqual(info["sha256"], sha(ROOT / info["source"]))
        for build in self.data["builds"]:
            self.assertEqual(build["binarySha256"], sha(ROOT / build["binary"]))
            self.assertEqual(build["disassemblySha256"], sha(ROOT / build["disassembly"]))
            self.assertEqual(build["sourceSha256"], self.data["revisions"][build["revision"]]["sha256"])
            self.assertTrue((ROOT / build["disassembly"]).read_text().startswith("\n"))

    def test_repeated_appearances_and_many_to_many(self):
        codemap = json.loads((ROOT / self.data["codemap"]).read_text())
        helper = [location for trace in codemap["traces"] for location in trace["locations"]
                  if location["title"] == "shared_helper"]
        self.assertEqual(len(helper), 2)
        links = self.data["links"]
        self.assertEqual({link["originKind"] for link in links}, {"human"})
        helper_id = "demo.cpp::shared_helper(int)"
        self.assertEqual({link["requirementId"] for link in links
                          if link["kind"] == "requirement-symbol" and link["symbolId"] == helper_id},
                         {"REQ-ONE", "REQ-TWO"})
        self.assertGreaterEqual(sum(link.get("requirementId") == "REQ-CALC" for link in links), 2)

    def test_limited_normalization_keeps_important_differences(self):
        source = (ROOT / "head/demo.cpp").read_text()
        parsed = functions(source)
        self.assertEqual(parsed["calculate_alpha"]["exact"], parsed["calculate_clone"]["exact"])
        self.assertNotEqual(parsed["calculate_alpha"]["exact"], parsed["calculate_beta"]["exact"])
        self.assertEqual(parsed["calculate_alpha"]["normalized"], parsed["calculate_beta"]["normalized"])
        self.assertNotEqual(parsed["calculate_alpha"]["normalized"],
                            parsed["calculate_with_state"]["normalized"])
        self.assertEqual(parsed["calculate_with_state"]["globalWrites"], ["state"])
        changed_constant = functions(source.replace("doubled + 5", "doubled + 6", 1))
        self.assertNotEqual(parsed["calculate_alpha"]["normalized"],
                            changed_constant["calculate_alpha"]["normalized"])
        changed_call = functions(source.replace("calculate_beta(input) + shared_helper(input)",
                                                "calculate_alpha(input) + shared_helper(input)"))
        self.assertNotEqual(parsed["scenario_two"]["calls"], changed_call["scenario_two"]["calls"])
        changed_type = functions(source.replace("int doubled = input * 2", "long doubled = input * 2", 1))
        self.assertNotEqual(parsed["calculate_alpha"]["types"], changed_type["calculate_alpha"]["types"])
        changed_condition = functions(source.replace("return doubled + 5;", "return doubled > 0 ? doubled + 5 : 5;", 1))
        self.assertNotEqual(parsed["calculate_alpha"]["branches"],
                            changed_condition["calculate_alpha"]["branches"])

    def test_build_and_section_evidence(self):
        for build in self.data["builds"]:
            for link in build["sourceLinks"]:
                self.assertEqual(link["originKind"], "tool")
                self.assertEqual(link["buildId"], build["id"])
                self.assertEqual(link["binary"]["buildId"], build["id"])
                self.assertEqual(link["source"]["revision"], build["revision"])
                self.assertTrue(link["binary"]["section"])
                self.assertLess(int(link["binary"]["start"], 0), int(link["binary"]["end"], 0))
        o0 = next(b for b in self.data["builds"] if b["revision"] == "head" and b["optimization"] == "O0")
        o2 = next(b for b in self.data["builds"] if b["revision"] == "head" and b["optimization"] == "O2"
                  and b["hasDebugInfo"])
        no_debug = next(b for b in self.data["builds"] if not b["hasDebugInfo"])
        debug_id = "demo.cpp::debug_only(int)"
        self.assertEqual(next(r for r in o0["ranges"] if r["symbolId"] == debug_id)["status"], "mapped")
        self.assertEqual(next(r for r in o2["ranges"] if r["symbolId"] == debug_id)["status"],
                         "optimized-away-or-unavailable")
        self.assertEqual(no_debug["sourceLinks"], [])


if __name__ == "__main__":
    unittest.main()
