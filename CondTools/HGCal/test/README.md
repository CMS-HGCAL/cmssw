# HGCal Conditions tests

## Inspect the Run 118667 validation database

The SQLite file below contains pedestal conditions tags.
Run these commands in a CMSSW environment with the HGCal calibration
payload classes and their XML converters compiled and available.

```bash
db="sqlite_file:/eos/user/f/fayuce/hgcal_conditions_cmssw20_review/five_conditions_closure.D9ICPJ/level0_calib_params.db"

# List the available calibration tags.
conddb --db "$db" listTags

# Inspect the ADC pedestals IOV and payload hash.
conddb --db "$db" list HGCalCalibration_v1_ADCPedestals

# Export this validation payload to XML.
conddb --db "$db" dump 9e119ab6db97c7069e94efb32a88c7791f4bb2af --destfile ADCPedestals.xml

# Display the beginning of the XML.
head -n 35 ADCPedestals.xml
```

For another tag or database, obtain the corresponding payload hash
with `list`, then pass it to `dump`.
