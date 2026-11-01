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
assert counts == {"rx": 3, "tx": 1, "status": 1}, counts
rows = db.execute("select source, heard_from, direct, hops_used, class, air_bytes, plain_bytes, rssi_dbm from receptions order by 1,2").fetchall()
print(rows)
assert len(rows) == 3
by_src = {r[0]: r for r in rows}
assert by_src["K6ATV-10"][1] == "K6ATV-10" and by_src["K6ATV-10"][2] == 1
assert by_src["K6ATV-14"][1] == "N0RLY-1" and by_src["K6ATV-14"][3] == 1       # starred relay found
assert by_src["K6ATV-4"][1] == "K6ATV-4" or by_src["K6ATV-4"][2] == 1
print("ok")
