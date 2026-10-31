"""Reproduce the reviewed commissioning data selection; never modify raw runs."""
import csv
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT/'datasets'
RUNS = DATA/'sensor_runs'
FEATURES = ['temperature_c', 'vibration_rms_g', 'current_rms_a']
SOURCE = 'Wokwi commissioning captures; DS18B20, MPU6050 window RMS, ADC current proxy (not measured current RMS)'
PLAN = {
    'train': ['train/normal_train_01.csv'],
    'calibration': ['calibration/normal_calibration_02.csv'],
    'test': ['test/normal_test_01.csv', 'test/temperature_abnormal_test_01.csv'],
    'exclude_entire_run': ['calibration/normal_calibration_01.csv'],
}

def read_run(relative):
    path=RUNS/relative
    reviewed=json.loads((DATA/'reviewed_sensor_runs.json').read_text())
    for item in (relative, str(Path(relative).with_suffix('.json'))):
        if hashlib.sha256((RUNS/item).read_bytes()).hexdigest()!=reviewed.get(item):
            raise ValueError('Raw file differs from reviewed capture: '+item)
    meta=json.loads(path.with_suffix('.json').read_text())
    rows=list(csv.DictReader(path.open(newline='')))
    run=path.stem; split=relative.split('/')[0]
    label=1 if run=='temperature_abnormal_test_01' else 0
    if meta.get('status')!='complete' or meta.get('run_id')!=run or meta.get('split')!=split:
        raise ValueError('Run metadata mismatch: '+relative)
    if len(rows)!=60 or meta.get('collected_records')!=len(rows):
        raise ValueError('Unexpected row count: '+relative)
    seen=set(); previous=(-1,-1)
    for row in rows:
        if row['run_id']!=run or row['split']!=split or int(row['label'])!=label:
            raise ValueError('Run/label mismatch: '+relative)
        values=[float(row[k]) for k in FEATURES]
        if not all(math.isfinite(x) for x in values):raise ValueError('Nonfinite row')
        key=(int(row['sequence']),int(row['timestamp_ms']))
        if key in seen or key[0]<=previous[0] or key[1]<=previous[1]:raise ValueError('Duplicate/nonmonotonic row')
        seen.add(key);previous=key
    return rows,dict(csv_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                     metadata_sha256=hashlib.sha256(path.with_suffix('.json').read_bytes()).hexdigest(),rows=len(rows))

def reasons(row):
    issues=[]
    if not 41<=float(row['temperature_c'])<=44:issues.append('outside intended 41..44C normal range')
    if not 2<=float(row['current_rms_a'])<=3:issues.append('outside intended 2..3A proxy normal range')
    if not 0<=float(row['vibration_rms_g'])<=0.04:issues.append('outside small-motion demo range 0..0.04g')
    return issues

def main():
    audit=[]; manifest={}; prepared={}
    for split in ['train','calibration']:
        accepted=[]
        for relative in PLAN[split]:
            rows, info=read_run(relative);manifest[relative]=info
            for index,row in enumerate(rows,1):
                why=reasons(row)
                audit.append(dict(file=relative,data_row=index,sequence=row['sequence'],
                                  action='exclude' if why else 'include',reason='; '.join(why) or 'within predefined normal range'))
                if not why:accepted.append(row)
        prepared[split]=accepted
    for relative in PLAN['exclude_entire_run']:
        rows,info=read_run(relative);manifest[relative]=info
        audit.append(dict(file=relative,data_row='all',sequence='all',action='exclude',reason='entire capture quarantined: operator-confirmed accidental high-temperature control change'))
    if len(prepared['train'])!=60 or len(prepared['calibration'])!=17:
        raise ValueError('Selection differs from reviewed 60/17 rows; stop for review')
    # Explicit test-run whitelist; never use its values for the selection or fitting rules.
    prepared['test']=[]
    for relative in PLAN['test']:
        rows,info=read_run(relative);manifest[relative]=info
        prepared['test'].extend(rows)
    fields=['run_id','split','label','scenario','received_at_utc','sequence','timestamp_ms',*FEATURES,'state']
    for split,rows in prepared.items():
        with (DATA/(split+'.csv')).open('w',newline='') as f:
            w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
    with (DATA/'sensor_selection_audit.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=['file','data_row','sequence','action','reason']);w.writeheader();w.writerows(audit)
    metadata=dict(source=SOURCE,feature_order=FEATURES,selection_plan=PLAN,raw_run_manifest=manifest,
                  training_rows=60,calibration_rows=17,test_rows=120,
                  exclusions=dict(entire_calibration_01_rows=60,calibration_02_out_of_range_rows=43),
                  scope='Provisional commissioning demo; one training capture, 17 selected calibration rows, two constant held test scenarios. No physical hardware validation. Captures have separate IDs/times but no device boot-session ID.',
                  calibration_limit='99th nearest-rank percentile on 17 rows is their maximum; cannot establish a 1% false-positive rate.',
                  labels='Operator-assigned test conditions; not derived from AI scores or fault state.',
                  no_invented_noise=True)
    (DATA/'metadata.json').write_text(json.dumps(metadata,indent=2,allow_nan=False)+'\n')
    print('Prepared: train=60, calibration=17, test=120')
    print('Excluded: calibration_01 entire run (60); calibration_02 out-of-range rows (43)')
    print('Raw files unchanged. No training or model export performed by this script.')
if __name__=='__main__':main()
