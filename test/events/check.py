# Reads the NDJSON that events_test prints and feeds it to the real collector (add_event); fails on any surprise.
import json, sqlite3, sys
sys.path.insert(0, sys.argv[1])
import collector

db = sqlite3.connect(":memory:")
db.executescript(collector.SCHEMA)
counts = {}
for line in sys.stdin:
    json.loads(line)                                    # must be valid JSON
    kind = collector.add_event(db, json.loads(line))
    counts[kind] = counts.get(kind, 0) + 1
print(counts)
assert counts == {"rx": 5, "tx": 1, "status": 2}, counts
rows = db.execute("select source, heard_from, direct, hops_used, class, air_bytes, plain_bytes, rssi_dbm from receptions order by 1,2").fetchall()
print(rows)
assert len(rows) == 5
by_src = {r[0]: r for r in rows}
assert by_src["K6ATV-10"][1] == "K6ATV-10" and by_src["K6ATV-10"][2] == 1
assert by_src["K6ATV-14"][1] == "N0RLY-1" and by_src["K6ATV-14"][3] == 1       # starred relay found
assert by_src["K6ATV-4"][1] == "K6ATV-4" or by_src["K6ATV-4"][2] == 1
bad = db.execute("select outcome, crc_valid, raw_hex, fo_hz, class from receptions where outcome='crc_error'").fetchall()
assert bad == [("crc_error", 0, "3cff022200ab", -3600.0, "hybrid")], bad
assert db.execute("select count(*) from receptions where outcome='dropped_no_reference' and class='ppm_primed'").fetchone()[0] == 1
nf = db.execute("select nf_avg_dbm, nf_min_dbm, nf_max_dbm, nf_samples from status where nf_samples is not null").fetchall()
assert nf == [(-107.3, -111, -98, 59)], nf
print("ok")
