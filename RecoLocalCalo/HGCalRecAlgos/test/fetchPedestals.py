import pymysql
import json
import sys

# MySQL connection
conn = pymysql.connect(
    host   = 'dbod-cms-hgcal-dpg-runtable.cern.ch',
    port   = 5501,
    user   = 'guest',
    db     = 'Calibrations'
)

typeCode  = sys.argv[1] if len(sys.argv) > 1 else 'MH_B1W_DNT0177'
reference = sys.argv[2] if len(sys.argv) > 2 else '115457'

cursor = conn.cursor()
cursor.execute("""
    SELECT payload, timestamp
    FROM calibrations_test
    WHERE typeCode  = %s
      AND runType   = 'pedestal'
      AND reference = %s
      AND payload IS NOT NULL
    ORDER BY timestamp DESC
    LIMIT 1
""", (typeCode, reference))

row = cursor.fetchone()
if not row:
    print(f"ERROR: No record found for typeCode={typeCode} reference={reference}")
    sys.exit(1)

payload_str, timestamp = row
payload = json.loads(payload_str)

print(f"Found: typeCode={typeCode} reference={reference} timestamp={timestamp}")
print(f"Channels: {len(payload['adc_ped'])}")

# Save to JSON file
outfile = f"pedestal_{typeCode}_{reference}.json"
with open(outfile, 'w') as f:
    json.dump({
        "typeCode"  : typeCode,
        "reference" : reference,
        "timestamp" : str(timestamp),
        "payload"   : payload
    }, f, indent=2)

print(f"Saved to: {outfile}")
conn.close()
